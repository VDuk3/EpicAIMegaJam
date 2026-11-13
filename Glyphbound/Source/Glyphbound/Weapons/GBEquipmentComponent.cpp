#include "Weapons/GBEquipmentComponent.h"

#include "Core/GBStatNames.h"
#include "Data/GBDataSubsystem.h"
#include "Engine/World.h"
#include "Player/GBPlayerCharacter.h"
#include "Spells/GBSpellbookComponent.h"
#include "Stats/GBStatsComponent.h"
#include "Weapons/GBGun.h"
#include "Weapons/GBStaff.h"
#include "Weapons/GBSword.h"
#include "World/GBProgressSubsystem.h"

namespace
{
	/** Cycle order (GDD 9): Staff -> Gun -> Sword. */
	const EGBHeldSlot EquipSlotOrder[] = { EGBHeldSlot::Staff, EGBHeldSlot::Gun, EGBHeldSlot::Sword };

	int32 EquipSlotIndex(EGBHeldSlot Slot)
	{
		for (int32 Index = 0; Index < static_cast<int32>(UE_ARRAY_COUNT(EquipSlotOrder)); ++Index)
		{
			if (EquipSlotOrder[Index] == Slot)
			{
				return Index;
			}
		}
		return 0;
	}
}

UGBEquipmentComponent::UGBEquipmentComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UGBEquipmentComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Quiet teardown: no unequip callbacks (they touch the camera and broadcast to a HUD that may be gone).
	for (const TPair<EGBHeldSlot, TObjectPtr<AGBHeldItem>>& Pair : Items)
	{
		if (AGBHeldItem* Item = Pair.Value.Get(); IsValid(Item))
		{
			Item->Destroy();
		}
	}
	Items.Reset();
	Super::EndPlay(EndPlayReason);
}

// ---- Lookups ----

bool UGBEquipmentComponent::FindWeaponSlot(FName WeaponId, EGBHeldSlot& OutSlot) const
{
	const UGBDataSubsystem* Data = UGBDataSubsystem::Get(this);
	const FGBWeaponDefinition* Def = Data ? Data->FindWeapon(WeaponId) : nullptr;
	if (!Def)
	{
		return false;
	}
	OutSlot = Def->Slot;
	return true;
}

AGBHeldItem* UGBEquipmentComponent::FindEquippedItem(FName WeaponId) const
{
	if (WeaponId.IsNone())
	{
		return nullptr;
	}
	for (const TPair<EGBHeldSlot, TObjectPtr<AGBHeldItem>>& Pair : Items)
	{
		AGBHeldItem* Item = Pair.Value.Get();
		if (IsValid(Item) && Item->GetWeaponId() == WeaponId)
		{
			return Item;
		}
	}
	return nullptr;
}

bool UGBEquipmentComponent::OwnsWeapon(FName WeaponId) const
{
	return !WeaponId.IsNone() && OwnedWeapons.Contains(WeaponId);
}

bool UGBEquipmentComponent::IsWeaponEquipped(FName WeaponId) const
{
	return FindEquippedItem(WeaponId) != nullptr;
}

FGBAmmoState UGBEquipmentComponent::ComputeFullAmmo(FName WeaponId) const
{
	FGBAmmoState Full;
	const UGBDataSubsystem* Data = UGBDataSubsystem::Get(this);
	const FGBWeaponDefinition* Def = Data ? Data->FindWeapon(WeaponId) : nullptr;
	if (!Def || Def->Slot != EGBHeldSlot::Gun)
	{
		return Full;
	}
	const UGBStatsComponent* Stats = UGBStatsComponent::Find(GetOwner());
	// Same keys and defaults as AGBHeldItem::GetStat / AGBGun, so a stored gun refills to what it would hold in hand.
	auto ReadStat = [Def, Stats, WeaponId](FName Stat, float DefaultValue)
	{
		const float* Found = Def->Stats.Find(Stat);
		const float Base = Found ? *Found : DefaultValue;
		if (!Stats)
		{
			return Base;
		}
		TArray<FName, TInlineAllocator<3>> Keys;
		Keys.Add(GBStat::WeaponKey(WeaponId, Stat));
		Keys.Add(GBStat::WeaponSlotKey(EGBHeldSlot::Gun, Stat));
		Keys.Add(GBStat::WeaponAllKey(Stat));
		return Stats->EvaluateKeys(Keys, Base);
	};
	Full.InMagazine = FMath::Max(0, FMath::RoundToInt(ReadStat(GBStat::MagazineSize, 1.f)));
	Full.SpareMagazines = FMath::Max(0, FMath::RoundToInt(ReadStat(GBStat::SpareMagazines, 0.f)));
	if (ReadStat(GBStat::ReloadPerRound, 0.f) > 0.5f)
	{
		// A round-by-round gun counts its reserve in rounds: the same total as the magazines would have held.
		Full.SpareMagazines *= Full.InMagazine;
	}
	return Full;
}

bool UGBEquipmentComponent::GetWeaponAmmo(FName WeaponId, FGBAmmoState& OutAmmo) const
{
	if (!OwnsWeapon(WeaponId))
	{
		return false;
	}
	if (const AGBHeldItem* Item = FindEquippedItem(WeaponId))
	{
		if (!Item->UsesAmmo())
		{
			return false;
		}
		OutAmmo = Item->GetAmmo();
		return true;
	}
	EGBHeldSlot Slot = EGBHeldSlot::Staff;
	if (!FindWeaponSlot(WeaponId, Slot) || Slot != EGBHeldSlot::Gun)
	{
		return false;
	}
	const FGBAmmoState* Stored = StoredAmmo.Find(WeaponId);
	OutAmmo = Stored ? *Stored : ComputeFullAmmo(WeaponId);
	return true;
}

EGBElement UGBEquipmentComponent::GetWeaponElement(FName WeaponId) const
{
	if (const AGBHeldItem* Item = FindEquippedItem(WeaponId))
	{
		return Item->GetElement();
	}
	if (const EGBElement* Stored = StoredElements.Find(WeaponId))
	{
		return *Stored;
	}
	const UGBDataSubsystem* Data = UGBDataSubsystem::Get(this);
	const FGBWeaponDefinition* Def = Data ? Data->FindWeapon(WeaponId) : nullptr;
	return Def ? Def->Element : EGBElement::Physical;
}

void UGBEquipmentComponent::SetWeaponElement(FName WeaponId, EGBElement InElement)
{
	if (!OwnsWeapon(WeaponId))
	{
		return;
	}
	if (AGBHeldItem* Item = FindEquippedItem(WeaponId))
	{
		Item->SetElement(InElement);
	}
	else
	{
		StoredElements.Add(WeaponId, InElement);
	}
	OnEquipmentChanged.Broadcast();
}

// ---- Spawning and storing ----

AGBHeldItem* UGBEquipmentComponent::SpawnItem(FName WeaponId)
{
	AGBPlayerCharacter* Player = Cast<AGBPlayerCharacter>(GetOwner());
	UWorld* World = GetWorld();
	const UGBDataSubsystem* Data = UGBDataSubsystem::Get(this);
	const FGBWeaponDefinition* Def = Data ? Data->FindWeapon(WeaponId) : nullptr;
	if (!Player || !World || !Def)
	{
		UE_LOG(LogGB, Warning, TEXT("GiveWeapon: unknown weapon '%s' (or no player / world)"), *WeaponId.ToString());
		return nullptr;
	}

	UClass* ItemClass = nullptr;
	if (!Def->ActorClass.IsEmpty())
	{
		ItemClass = LoadClass<AGBHeldItem>(nullptr, *Def->ActorClass);
		if (!ItemClass || ItemClass->HasAnyClassFlags(CLASS_Abstract))
		{
			UE_LOG(LogGB, Warning, TEXT("Weapon '%s': ActorClass '%s' not found or abstract, using the slot default"), *WeaponId.ToString(), *Def->ActorClass);
			ItemClass = nullptr;
		}
	}
	if (!ItemClass)
	{
		switch (Def->Slot)
		{
		case EGBHeldSlot::Staff: ItemClass = AGBStaff::StaticClass(); break;
		case EGBHeldSlot::Sword: ItemClass = AGBSword::StaticClass(); break;
		default: ItemClass = AGBGun::StaticClass(); break;
		}
	}

	// Copy the definition: spawning must not rely on a pointer into the data tables.
	const FGBWeaponDefinition Definition = *Def;

	FActorSpawnParameters Params;
	Params.Owner = Player;
	Params.Instigator = Player;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AGBHeldItem* Item = World->SpawnActor<AGBHeldItem>(ItemClass, Player->GetActorTransform(), Params);
	if (!Item)
	{
		UE_LOG(LogGB, Warning, TEXT("GiveWeapon: could not spawn '%s'"), *WeaponId.ToString());
		return nullptr;
	}

	USceneComponent* Anchor = Player->GetHeldItemAnchor() ? Player->GetHeldItemAnchor() : Player->GetRootComponent();
	Item->AttachToComponent(Anchor, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	Item->InitFromDefinition(Definition, Player);
	Item->OnUnequipped();
	Item->RefillAmmo();
	return Item;
}

void UGBEquipmentComponent::StoreItemState(AGBHeldItem* Item)
{
	if (!IsValid(Item))
	{
		return;
	}
	Item->StopPrimary();
	Item->StopSecondary();
	Item->OnUnequipped(); // a gun cancels its reload here

	const FName WeaponId = Item->GetWeaponId();
	StoredElements.Add(WeaponId, Item->GetElement());
	if (Item->UsesAmmo())
	{
		StoredAmmo.Add(WeaponId, Item->GetAmmo());
	}
}

void UGBEquipmentComponent::RestoreItemState(AGBHeldItem* Item)
{
	if (!IsValid(Item))
	{
		return;
	}
	const FName WeaponId = Item->GetWeaponId();
	if (const EGBElement* Element = StoredElements.Find(WeaponId))
	{
		Item->SetElement(*Element);
	}
	if (const FGBAmmoState* Ammo = StoredAmmo.Find(WeaponId))
	{
		Item->SetAmmo(*Ammo); // a gun never held has no entry and stays full from SpawnItem
	}
	// While in a slot, the actor is the only source of truth.
	StoredElements.Remove(WeaponId);
	StoredAmmo.Remove(WeaponId);
}

// ---- Giving and equipping ----

AGBHeldItem* UGBEquipmentComponent::GiveWeapon(FName WeaponId, bool bAutoSwitch, bool /*bDropOld*/)
{
	EGBHeldSlot NewSlot = EGBHeldSlot::Staff;
	if (!FindWeaponSlot(WeaponId, NewSlot))
	{
		UE_LOG(LogGB, Warning, TEXT("GiveWeapon: unknown weapon '%s'"), *WeaponId.ToString());
		return nullptr;
	}

	if (OwnsWeapon(WeaponId))
	{
		return FindEquippedItem(WeaponId);
	}

	// Owner's rule (2026-11-12): nothing is dropped and the weapon in hand is never replaced.
	if (HasWeaponInSlot(NewSlot))
	{
		OwnedWeapons.Add(WeaponId);
		if (NewSlot == EGBHeldSlot::Gun)
		{
			StoredAmmo.Add(WeaponId, ComputeFullAmmo(WeaponId));
		}
		OnEquipmentChanged.Broadcast();
		return nullptr;
	}

	const bool bHandsEmpty = GetActiveItem() == nullptr;
	AGBHeldItem* NewItem = SpawnItem(WeaponId);
	if (!NewItem)
	{
		return nullptr;
	}
	OwnedWeapons.Add(WeaponId);
	Items.Add(NewSlot, NewItem);

	if (bHandsEmpty)
	{
		EquipSlot(NewSlot);
	}
	else if (!bAutoSwitch || !SwitchTo(NewSlot))
	{
		OnEquipmentChanged.Broadcast();
	}
	return NewItem;
}

bool UGBEquipmentComponent::EquipOwnedWeapon(FName WeaponId)
{
	if (bSwitchLocked || !OwnsWeapon(WeaponId) || IsWeaponEquipped(WeaponId))
	{
		return false;
	}
	EGBHeldSlot Slot = EGBHeldSlot::Staff;
	if (!FindWeaponSlot(WeaponId, Slot))
	{
		UE_LOG(LogGB, Warning, TEXT("EquipOwnedWeapon: '%s' is no longer in Weapons.json"), *WeaponId.ToString());
		return false;
	}

	AGBHeldItem* Old = GetItemInSlot(Slot);
	// The new item comes up if it replaces the one in the hands (or the hands are empty).
	const bool bBringUp = ActiveSlot == Slot || GetActiveItem() == nullptr;

	AGBHeldItem* NewItem = SpawnItem(WeaponId);
	if (!NewItem)
	{
		return false;
	}
	if (Old)
	{
		StoreItemState(Old);
		Old->Destroy();
	}
	Items.Add(Slot, NewItem);
	RestoreItemState(NewItem);

	if (bBringUp)
	{
		EquipSlot(Slot); // cancels a sustained spell, broadcasts both delegates
		return true;
	}

	if (const AGBPlayerCharacter* Player = Cast<AGBPlayerCharacter>(GetOwner()))
	{
		if (UGBSpellbookComponent* Spellbook = Player->GetSpellbook())
		{
			Spellbook->CancelSustained();
		}
	}
	OnEquipmentChanged.Broadcast();
	return true;
}

bool UGBEquipmentComponent::HasWeaponInSlot(EGBHeldSlot Slot) const
{
	return GetItemInSlot(Slot) != nullptr;
}

AGBHeldItem* UGBEquipmentComponent::GetItemInSlot(EGBHeldSlot Slot) const
{
	AGBHeldItem* Item = Items.FindRef(Slot).Get();
	return IsValid(Item) ? Item : nullptr;
}

AGBHeldItem* UGBEquipmentComponent::GetActiveItem() const
{
	AGBHeldItem* Item = GetItemInSlot(ActiveSlot);
	return (Item && Item->IsEquipped()) ? Item : nullptr;
}

bool UGBEquipmentComponent::IsHoldingStaff() const
{
	return ActiveSlot == EGBHeldSlot::Staff && GetActiveItem() != nullptr;
}

bool UGBEquipmentComponent::IsHoldingGun() const
{
	return ActiveSlot == EGBHeldSlot::Gun && GetActiveItem() != nullptr;
}

void UGBEquipmentComponent::EquipSlot(EGBHeldSlot Slot)
{
	AGBHeldItem* Target = GetItemInSlot(Slot);
	AGBHeldItem* Current = GetItemInSlot(ActiveSlot);
	if (Current && Current != Target && Current->IsEquipped())
	{
		Current->StopPrimary();
		Current->StopSecondary();
		Current->OnUnequipped();
	}

	ActiveSlot = Slot;

	if (const AGBPlayerCharacter* Player = Cast<AGBPlayerCharacter>(GetOwner()))
	{
		if (UGBSpellbookComponent* Spellbook = Player->GetSpellbook())
		{
			Spellbook->CancelSustained();
		}
	}

	if (Target && !Target->IsEquipped())
	{
		Target->OnEquipped();
	}

	OnActiveItemChanged.Broadcast(Target);
	OnEquipmentChanged.Broadcast();
}

bool UGBEquipmentComponent::SwitchTo(EGBHeldSlot Slot)
{
	if (bSwitchLocked || !HasWeaponInSlot(Slot))
	{
		return false;
	}
	if (Slot == ActiveSlot && GetActiveItem() != nullptr)
	{
		return true;
	}
	EquipSlot(Slot);
	return true;
}

void UGBEquipmentComponent::CycleSlot(int32 Direction)
{
	if (bSwitchLocked)
	{
		return;
	}
	const int32 Count = static_cast<int32>(UE_ARRAY_COUNT(EquipSlotOrder));
	const int32 Step = Direction >= 0 ? 1 : -1;
	const int32 Start = EquipSlotIndex(ActiveSlot);
	for (int32 Offset = 1; Offset < Count; ++Offset)
	{
		const int32 Index = ((Start + Step * Offset) % Count + Count) % Count;
		if (HasWeaponInSlot(EquipSlotOrder[Index]))
		{
			SwitchTo(EquipSlotOrder[Index]);
			return;
		}
	}
}

// ---- Ammo ----

void UGBEquipmentComponent::RefillAllAmmo()
{
	for (const TPair<EGBHeldSlot, TObjectPtr<AGBHeldItem>>& Pair : Items)
	{
		if (AGBHeldItem* Item = Pair.Value.Get(); IsValid(Item))
		{
			Item->RefillAmmo();
		}
	}
	// Era travel restores everything (GDD 8), stored guns included.
	for (const FName& WeaponId : OwnedWeapons)
	{
		EGBHeldSlot Slot = EGBHeldSlot::Staff;
		if (!IsWeaponEquipped(WeaponId) && FindWeaponSlot(WeaponId, Slot) && Slot == EGBHeldSlot::Gun)
		{
			StoredAmmo.Add(WeaponId, ComputeFullAmmo(WeaponId));
		}
	}
	OnEquipmentChanged.Broadcast();
}

int32 UGBEquipmentComponent::AddSpareMagazines(int32 Count)
{
	AGBHeldItem* Gun = GetItemInSlot(EGBHeldSlot::Gun);
	return Gun ? Gun->AddSpareMagazines(Count) : 0;
}

EGBHudSkin UGBEquipmentComponent::GetHudSkin() const
{
	const AGBHeldItem* Active = GetActiveItem();
	return Active ? Active->GetHudSkin() : EGBHudSkin::Rpg90s;
}

void UGBEquipmentComponent::DestroyAllItems()
{
	for (const TPair<EGBHeldSlot, TObjectPtr<AGBHeldItem>>& Pair : Items)
	{
		if (AGBHeldItem* Item = Pair.Value.Get(); IsValid(Item))
		{
			Item->StopPrimary();
			Item->StopSecondary();
			Item->OnUnequipped();
			Item->Destroy();
		}
	}
	Items.Reset();
}

// ---- Snapshot ----

TMap<EGBHeldSlot, FName> UGBEquipmentComponent::ChooseEquippedWeapons(const TArray<FName>& Owned, const TArray<FName>& Equipped,
	TFunctionRef<bool(FName, EGBHeldSlot&)> SlotOf)
{
	TMap<EGBHeldSlot, FName> Result;
	auto TryPlace = [&Result, &SlotOf](FName WeaponId)
	{
		EGBHeldSlot Slot = EGBHeldSlot::Staff;
		if (!WeaponId.IsNone() && SlotOf(WeaponId, Slot) && !Result.Contains(Slot))
		{
			Result.Add(Slot, WeaponId);
		}
	};
	for (const FName& WeaponId : Equipped)
	{
		if (Owned.Contains(WeaponId))
		{
			TryPlace(WeaponId);
		}
	}
	for (const FName& WeaponId : Owned)
	{
		TryPlace(WeaponId);
	}
	return Result;
}

void UGBEquipmentComponent::SaveToSnapshot(FGBPlayerSnapshot& Snapshot) const
{
	Snapshot.Weapons = OwnedWeapons;
	Snapshot.EquippedWeapons.Reset();
	Snapshot.Ammo.Reset();
	Snapshot.WeaponElements.Reset();

	for (const TPair<EGBHeldSlot, TObjectPtr<AGBHeldItem>>& Pair : Items)
	{
		const AGBHeldItem* Item = Pair.Value.Get();
		if (!IsValid(Item))
		{
			continue;
		}
		const FName WeaponId = Item->GetWeaponId();
		Snapshot.Weapons.AddUnique(WeaponId);
		Snapshot.EquippedWeapons.Add(WeaponId);
		Snapshot.WeaponElements.Add(WeaponId, Item->GetElement());
		if (Item->UsesAmmo())
		{
			Snapshot.Ammo.Add(WeaponId, Item->GetAmmo());
		}
	}
	for (const TPair<FName, FGBAmmoState>& Pair : StoredAmmo)
	{
		if (!Snapshot.Ammo.Contains(Pair.Key))
		{
			Snapshot.Ammo.Add(Pair.Key, Pair.Value);
		}
	}
	for (const TPair<FName, EGBElement>& Pair : StoredElements)
	{
		if (!Snapshot.WeaponElements.Contains(Pair.Key))
		{
			Snapshot.WeaponElements.Add(Pair.Key, Pair.Value);
		}
	}
	Snapshot.ActiveSlot = ActiveSlot;
}

void UGBEquipmentComponent::LoadFromSnapshot(const FGBPlayerSnapshot& Snapshot)
{
	DestroyAllItems();
	OwnedWeapons.Reset();
	StoredAmmo.Reset();
	StoredElements.Reset();

	for (const FName& WeaponId : Snapshot.Weapons)
	{
		EGBHeldSlot Slot = EGBHeldSlot::Staff;
		if (FindWeaponSlot(WeaponId, Slot))
		{
			OwnedWeapons.AddUnique(WeaponId);
		}
		else
		{
			UE_LOG(LogGB, Warning, TEXT("Equipment snapshot: weapon '%s' is not in Weapons.json; dropped."), *WeaponId.ToString());
		}
	}

	const TMap<EGBHeldSlot, FName> ToEquip = ChooseEquippedWeapons(OwnedWeapons, Snapshot.EquippedWeapons,
		[this](FName WeaponId, EGBHeldSlot& OutSlot) { return FindWeaponSlot(WeaponId, OutSlot); });

	for (const FName& WeaponId : OwnedWeapons)
	{
		EGBHeldSlot Slot = EGBHeldSlot::Staff;
		FindWeaponSlot(WeaponId, Slot);
		const bool bEquipped = ToEquip.FindRef(Slot) == WeaponId;

		if (bEquipped)
		{
			if (AGBHeldItem* Item = SpawnItem(WeaponId))
			{
				Items.Add(Slot, Item);
				if (const EGBElement* SavedElement = Snapshot.WeaponElements.Find(WeaponId))
				{
					Item->SetElement(*SavedElement);
				}
				if (const FGBAmmoState* SavedAmmo = Snapshot.Ammo.Find(WeaponId))
				{
					Item->SetAmmo(*SavedAmmo);
				}
			}
			continue;
		}

		if (const EGBElement* SavedElement = Snapshot.WeaponElements.Find(WeaponId))
		{
			StoredElements.Add(WeaponId, *SavedElement);
		}
		if (Slot == EGBHeldSlot::Gun)
		{
			const FGBAmmoState* SavedAmmo = Snapshot.Ammo.Find(WeaponId);
			StoredAmmo.Add(WeaponId, SavedAmmo ? *SavedAmmo : ComputeFullAmmo(WeaponId));
		}
	}

	EGBHeldSlot Slot = Snapshot.ActiveSlot;
	if (!HasWeaponInSlot(Slot))
	{
		for (const EGBHeldSlot Candidate : EquipSlotOrder)
		{
			if (HasWeaponInSlot(Candidate))
			{
				Slot = Candidate;
				break;
			}
		}
	}

	if (HasWeaponInSlot(Slot))
	{
		EquipSlot(Slot);
	}
	else
	{
		ActiveSlot = Snapshot.ActiveSlot;
		OnActiveItemChanged.Broadcast(nullptr);
		OnEquipmentChanged.Broadcast();
	}
}
