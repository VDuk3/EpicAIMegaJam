#include "Weapons/GBEquipmentComponent.h"

#include "Data/GBDataSubsystem.h"
#include "Engine/World.h"
#include "Player/GBPlayerCharacter.h"
#include "Spells/GBSpellbookComponent.h"
#include "Weapons/GBGun.h"
#include "Weapons/GBStaff.h"
#include "Weapons/GBSword.h"
#include "World/GBPickup.h"
#include "World/GBProgressSubsystem.h"

namespace
{
	/** Where a replaced weapon is dropped: in front of the player (placement offset, not a gameplay number). */
	constexpr float EquipDropDistanceCm = 120.f;

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

AGBHeldItem* UGBEquipmentComponent::GiveWeapon(FName WeaponId, bool bAutoSwitch, bool bDropOld)
{
	AGBHeldItem* NewItem = SpawnItem(WeaponId);
	if (!NewItem)
	{
		return nullptr;
	}

	const EGBHeldSlot NewSlot = NewItem->GetSlot();
	const bool bHandsEmpty = GetActiveItem() == nullptr;
	bool bReplacedActive = false;

	if (AGBHeldItem* Old = GetItemInSlot(NewSlot))
	{
		bReplacedActive = Old->IsEquipped();
		Old->StopPrimary();
		Old->StopSecondary();
		Old->OnUnequipped();
		if (bDropOld)
		{
			if (const AActor* Owner = GetOwner())
			{
				FVector Forward = Owner->GetActorForwardVector();
				Forward.Z = 0.f;
				Forward = Forward.GetSafeNormal();
				FGBDropDefinition Drop;
				Drop.WeaponId = Old->GetWeaponId();
				AGBPickup::SpawnPickup(GetWorld(), Drop, Owner->GetActorLocation() + Forward * EquipDropDistanceCm);
			}
		}
		Old->Destroy();
	}
	Items.Add(NewSlot, NewItem);

	if (bHandsEmpty || bReplacedActive)
	{
		EquipSlot(NewSlot);
	}
	else if (!bAutoSwitch || !SwitchTo(NewSlot))
	{
		OnEquipmentChanged.Broadcast();
	}
	return NewItem;
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

void UGBEquipmentComponent::RefillAllAmmo()
{
	for (const TPair<EGBHeldSlot, TObjectPtr<AGBHeldItem>>& Pair : Items)
	{
		if (AGBHeldItem* Item = Pair.Value.Get(); IsValid(Item))
		{
			Item->RefillAmmo();
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

void UGBEquipmentComponent::SaveToSnapshot(FGBPlayerSnapshot& Snapshot) const
{
	Snapshot.Weapons.Reset();
	Snapshot.Ammo.Reset();
	Snapshot.WeaponElements.Reset();
	for (const TPair<EGBHeldSlot, TObjectPtr<AGBHeldItem>>& Pair : Items)
	{
		const AGBHeldItem* Item = Pair.Value.Get();
		if (!IsValid(Item))
		{
			continue;
		}
		Snapshot.Weapons.Add(Item->GetWeaponId());
		Snapshot.WeaponElements.Add(Item->GetWeaponId(), Item->GetElement());
		if (Item->UsesAmmo())
		{
			Snapshot.Ammo.Add(Item->GetWeaponId(), Item->GetAmmo());
		}
	}
	Snapshot.ActiveSlot = ActiveSlot;
}

void UGBEquipmentComponent::LoadFromSnapshot(const FGBPlayerSnapshot& Snapshot)
{
	DestroyAllItems();

	for (const FName& WeaponId : Snapshot.Weapons)
	{
		AGBHeldItem* Item = SpawnItem(WeaponId);
		if (!Item)
		{
			continue;
		}
		if (AGBHeldItem* Previous = GetItemInSlot(Item->GetSlot()))
		{
			Previous->Destroy(); // two weapons of one slot in a snapshot: the last one wins
		}
		Items.Add(Item->GetSlot(), Item);

		if (const EGBElement* SavedElement = Snapshot.WeaponElements.Find(WeaponId))
		{
			Item->SetElement(*SavedElement);
		}
		if (const FGBAmmoState* SavedAmmo = Snapshot.Ammo.Find(WeaponId))
		{
			Item->SetAmmo(*SavedAmmo);
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
