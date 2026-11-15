#include "Items/GBInventoryComponent.h"

#include "Combat/GBCombatLibrary.h"
#include "Core/GBStatNames.h"
#include "Core/GBPlaceholder.h"
#include "Data/GBDataSubsystem.h"
#include "Stats/GBStatsComponent.h"
#include "Spells/GBSpell_Projectile.h"
#include "Combat/GBStatusEffectComponent.h"
#include "Combat/GBProjectile.h"
#include "Weapons/GBEquipmentComponent.h"
#include "Weapons/GBHeldItem.h"
#include "Player/GBPlayerCharacter.h"
#include "World/GBProgressSubsystem.h"
#include "UI/GBHUD.h"
#include "Stats/GBAttributeComponent.h"

namespace GBInventoryLocal
{
	// Skill-perk stats (Skills.json, base 0) and the potions they drink.
	const FName AutoHealthPotionStat(TEXT("AutoHealthPotion"));
	const FName AutoManaPotionStat(TEXT("AutoManaPotion"));
	const FName HealthPotionId(TEXT("HealthPotion"));
	const FName ManaPotionId(TEXT("ManaPotion"));
	constexpr float AutoPotionFraction = 0.33f;
}

namespace
{
	const FName ArmorModifierSource(TEXT("Armor"));

	float ReadItemStat(const FGBItemDefinition& Def, FName Key, float DefaultValue)
	{
		if (const float* Value = Def.Stats.Find(Key))
		{
			return *Value;
		}
		UE_LOG(LogGB, Warning, TEXT("Item '%s' has no stat '%s'; using %.2f."), *Def.Id.ToString(), *Key.ToString(), DefaultValue);
		return DefaultValue;
	}

	FString ItemDisplayName(const FGBItemDefinition* Def, FName ItemId)
	{
		return (Def && !Def->DisplayName.IsEmpty()) ? Def->DisplayName : ItemId.ToString();
	}

	FString ElementName(EGBElement Element)
	{
		return StaticEnum<EGBElement>()->GetNameStringByValue(static_cast<int64>(Element));
	}
}

UGBInventoryComponent::UGBInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UGBInventoryComponent::BeginPlay()
{
	Super::BeginPlay();

	if (UGBDataSubsystem* Data = UGBDataSubsystem::Get(this))
	{
		DataReloadedHandle = Data->OnDataReloaded.AddUObject(this, &UGBInventoryComponent::HandleDataReloaded);
	}

	// Skill perks "Emergency Draught" / "Emergency Focus": below a third of the resource, drink a potion by
	// itself. The perk sets the stat AutoHealthPotion / AutoManaPotion above 0. One potion at a time: none is
	// drunk while the same potion is still working.
	if (UGBAttributeComponent* Attributes = UGBAttributeComponent::Find(GetOwner()))
	{
		auto TryAutoPotion = [this](FName PerkStat, FName PotionId, float Value, float MaxValue)
		{
			const UGBStatsComponent* Stats = UGBStatsComponent::Find(GetOwner());
			if (!Stats || Stats->GetStat(PerkStat, 0.f) <= 0.f || MaxValue <= 0.f || Value <= 0.f
				|| Value >= MaxValue * GBInventoryLocal::AutoPotionFraction || !HasItem(PotionId))
			{
				return;
			}
			const UGBDataSubsystem* Data = UGBDataSubsystem::Get(this);
			const FGBItemDefinition* Def = Data ? Data->FindItem(PotionId) : nullptr;
			const UGBStatusEffectComponent* Effects = UGBStatusEffectComponent::Find(GetOwner());
			if (!Def || !Effects || Effects->HasEffect(Def->UseEffect))
			{
				return;
			}
			UseItem(PotionId);
		};
		Attributes->OnHealthChanged.AddWeakLambda(this, [TryAutoPotion](float Health, float MaxHealth)
		{
			TryAutoPotion(GBInventoryLocal::AutoHealthPotionStat, GBInventoryLocal::HealthPotionId, Health, MaxHealth);
		});
		Attributes->OnManaChanged.AddWeakLambda(this, [TryAutoPotion](float Mana, float MaxMana)
		{
			TryAutoPotion(GBInventoryLocal::AutoManaPotionStat, GBInventoryLocal::ManaPotionId, Mana, MaxMana);
		});
	}
}

void UGBInventoryComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UGBDataSubsystem* Data = UGBDataSubsystem::Get(this))
	{
		Data->OnDataReloaded.Remove(DataReloadedHandle);
	}
	DataReloadedHandle.Reset();

	Super::EndPlay(EndPlayReason);
}

void UGBInventoryComponent::InitializeFromData()
{
	if (!EquippedArmor.IsNone())
	{
		EquippedArmor = NAME_None;
		ApplyArmorModifiers();
	}
	Items.Reset();
	QuickUseItem = NAME_None;

	const UGBDataSubsystem* Data = UGBDataSubsystem::Get(this);
	if (!Data)
	{
		UE_LOG(LogGB, Warning, TEXT("Inventory: no data subsystem, no starting items."));
		OnInventoryChanged.Broadcast();
		return;
	}

	for (const TPair<FName, int32>& Pair : Data->GetPlayer().StartingItems)
	{
		AddItem(Pair.Key, Pair.Value);
	}
	OnInventoryChanged.Broadcast();
}

int32 UGBInventoryComponent::AddItem(FName ItemId, int32 Count)
{
	if (Count <= 0)
	{
		return 0;
	}

	const UGBDataSubsystem* Data = UGBDataSubsystem::Get(this);
	const FGBItemDefinition* Def = Data ? Data->FindItem(ItemId) : nullptr;
	if (!Def)
	{
		UE_LOG(LogGB, Warning, TEXT("AddItem: unknown item '%s'."), *ItemId.ToString());
		return 0;
	}

	if (Def->Type == EGBItemType::Ammo)
	{
		UGBEquipmentComponent* Equipment = GetOwner() ? GetOwner()->FindComponentByClass<UGBEquipmentComponent>() : nullptr;
		if (!Equipment)
		{
			return 0;
		}
		const float* PerItem = Def->Stats.Find(GBStat::SpareMagazines);
		const int32 MagazinesPerItem = FMath::Max(1, PerItem ? FMath::RoundToInt(*PerItem) : 1);
		const int32 Magazines = Count * MagazinesPerItem;
		// A full gun takes nothing, so the pickup stays on the floor; a nearly-full one keeps the remainder.
		const int32 Added = (Magazines > 0) ? Equipment->AddSpareMagazines(Magazines) : 0;
		return UsedAmmoItems(Added, MagazinesPerItem);
	}

	if (Def->MaxStack <= 0)
	{
		UE_LOG(LogGB, Warning, TEXT("AddItem: item '%s' has MaxStack %d; treating it as 1."), *ItemId.ToString(), Def->MaxStack);
	}
	const int32 MaxStack = FMath::Max(1, Def->MaxStack);
	const int32 Current = GetCount(ItemId);
	const int32 Taken = FMath::Clamp(MaxStack - Current, 0, Count);
	if (Taken <= 0)
	{
		return 0;
	}

	Items.Add(ItemId, Current + Taken);
	if (QuickUseItem.IsNone() && Def->bQuickUse)
	{
		QuickUseItem = ItemId;
	}

	OnItemCountChanged.Broadcast(ItemId, Current + Taken);
	OnInventoryChanged.Broadcast();
	return Taken;
}

bool UGBInventoryComponent::RemoveItem(FName ItemId, int32 Count)
{
	if (Count <= 0)
	{
		return true;
	}

	const int32 Current = GetCount(ItemId);
	if (Current < Count)
	{
		return false;
	}

	const int32 NewCount = Current - Count;
	if (NewCount > 0)
	{
		Items.Add(ItemId, NewCount);
	}
	else
	{
		Items.Remove(ItemId);
		HandleStackEmptied(ItemId);
	}

	OnItemCountChanged.Broadcast(ItemId, NewCount);
	OnInventoryChanged.Broadcast();
	return true;
}

int32 UGBInventoryComponent::UsedAmmoItems(int32 AddedMagazines, int32 MagazinesPerItem)
{
	if (AddedMagazines <= 0 || MagazinesPerItem <= 0)
	{
		return 0;
	}
	return (AddedMagazines + MagazinesPerItem - 1) / MagazinesPerItem;
}

int32 UGBInventoryComponent::GetCount(FName ItemId) const
{
	return Items.FindRef(ItemId);
}
void UGBInventoryComponent::HandleStackEmptied(FName ItemId)
{
	if (QuickUseItem == ItemId)
	{
		QuickUseItem = FindQuickUseFrom(ItemId, 1);
	}
	if (EquippedArmor == ItemId)
	{
		EquippedArmor = NAME_None;
		ApplyArmorModifiers();
	}
}

bool UGBInventoryComponent::UseItem(FName ItemId)
{
	if (!HasItem(ItemId))
	{
		return false;
	}

	const UGBDataSubsystem* Data = UGBDataSubsystem::Get(this);
	const FGBItemDefinition* Def = Data ? Data->FindItem(ItemId) : nullptr;
	if (!Def)
	{
		UE_LOG(LogGB, Warning, TEXT("UseItem: unknown item '%s'."), *ItemId.ToString());
		return false;
	}

	AGBPlayerCharacter* Player = Cast<AGBPlayerCharacter>(GetOwner());
	if (Player && Player->IsDead())
	{
		return false;
	}

	if (Def->Type == EGBItemType::Consumable)
	{
		UGBStatusEffectComponent* Effects = UGBStatusEffectComponent::Find(GetOwner());
		if (!Effects)
		{
			UE_LOG(LogGB, Warning, TEXT("UseItem: '%s' has no status effect component to apply '%s'."), *GetNameSafe(GetOwner()), *Def->UseEffect.ToString());
			return false;
		}
		if (!Effects->ApplyEffect(Def->UseEffect, GetOwner()))
		{
			UE_LOG(LogGB, Warning, TEXT("UseItem: item '%s' has an unknown UseEffect '%s'."), *ItemId.ToString(), *Def->UseEffect.ToString());
			return false;
		}
		return RemoveItem(ItemId, 1);
	}

	if (Def->Type == EGBItemType::Throwable)
	{
		// Copy: removing the item broadcasts, and a listener could reload the data.
		const FGBItemDefinition DefCopy = *Def;
		if (!UseThrowable(DefCopy))
		{
			return false;
		}
		return RemoveItem(ItemId, 1);
	}

	return false;
}

bool UGBInventoryComponent::UseThrowable(const FGBItemDefinition& Def)
{
	AGBPlayerCharacter* Player = Cast<AGBPlayerCharacter>(GetOwner());
	UWorld* World = GetWorld();
	if (!Player || !World)
	{
		return false;
	}

	FGBProjectileConfig Config;
	Config.Damage.Amount = ReadItemStat(Def, GBStat::Damage, 0.f);
	Config.Damage.Element = Def.Element;
	Config.Damage.Instigator = Player;
	Config.Damage.SourceId = Def.Id;
	Config.Damage.BuildUp = ReadItemStat(Def, GBStat::BuildUp, 0.f);
	Config.Damage.BuildUpEventId = UGBCombatLibrary::NewBuildUpEventId();
	if (const FString* OnHit = Def.Strings.Find(GBString::OnHitEffect))
	{
		Config.Damage.OnHitEffect = FName(**OnHit);
	}

	Config.ExplosionRadius = ReadItemStat(Def, GBStat::Radius, 0.f);
	// Full damage in the whole radius when 0, linear falloff when 1 (same meaning as the spell param).
	Config.bExplosionFalloff = ReadItemStat(Def, GBSpellParam::ExplosionFalloff, 1.f) > 0.5f;
	Config.ExplosionKnockback = FMath::Max(0.f, ReadItemStat(Def, GBSpellParam::ExplosionKnockback, 0.f));
	Config.Speed = ReadItemStat(Def, GBStat::Speed, Config.Speed);
	Config.FuseTime = ReadItemStat(Def, GBStat::FuseTime, 0.f);
	Config.GravityScale = ReadItemStat(Def, GBStat::ProjectileGravity, 1.f);
	Config.Radius = ReadItemStat(Def, GBStat::ProjectileRadius, Config.Radius);
	Config.Bounciness = ReadItemStat(Def, GBItemStat::Bounciness, Config.Bounciness);
	Config.Friction = ReadItemStat(Def, GBItemStat::Friction, Config.Friction);
	if (const float* Range = Def.Stats.Find(GBStat::Range))
	{
		Config.MaxRange = *Range;
	}
	if (const UGBDataSubsystem* Data = UGBDataSubsystem::Get(World))
	{
		Config.MaxLifetime = Data->GetGameValue(GBProjectileKey::ProjectileMaxLifetime, Config.MaxLifetime);
	}

	// Art rule (GDD 3): the grenade is cyberpunk, so it is 3D.
	Config.bPixelArt = false;
	const FLinearColor ElementTint = GBPlaceholder::ElementColor(static_cast<uint8>(Def.Element));
	const FString* ColorText = Def.Strings.Find(GBString::Color);
	Config.Color = ColorText ? UGBDataSubsystem::ParseColor(*ColorText, ElementTint) : ElementTint;

	FRotator ThrowRotation = Player->GetAimDirection().Rotation();
	const float Tilt = ReadItemStat(Def, GBItemStat::ThrowPitch, 8.f);
	ThrowRotation.Pitch = FMath::Clamp(ThrowRotation.Pitch + static_cast<double>(Tilt), -89.0, 89.0);

	return AGBProjectile::SpawnProjectile(World, Config, Player->GetMuzzleLocation(), ThrowRotation.Vector(), Player) != nullptr;
}

FName UGBInventoryComponent::FindQuickUseFrom(FName From, int32 Direction) const
{
	const UGBDataSubsystem* Data = UGBDataSubsystem::Get(this);
	if (!Data)
	{
		return NAME_None;
	}

	const TArray<FGBItemDefinition>& Defs = Data->GetItems();
	const int32 Num = Defs.Num();
	if (Num == 0)
	{
		return NAME_None;
	}

	const int32 Step = Direction >= 0 ? 1 : -1;
	int32 Start = Defs.IndexOfByPredicate([From](const FGBItemDefinition& Def) { return Def.Id == From; });
	if (Start == INDEX_NONE)
	{
		// So that the first step lands on the first (or last) item.
		Start = Step > 0 ? Num - 1 : 0;
	}

	for (int32 Offset = 1; Offset <= Num; ++Offset)
	{
		const int32 Index = ((Start + Step * Offset) % Num + Num) % Num;
		const FGBItemDefinition& Def = Defs[Index];
		if (Def.bQuickUse && GetCount(Def.Id) > 0)
		{
			return Def.Id;
		}
	}
	return NAME_None;
}

void UGBInventoryComponent::CycleQuickUse(int32 Direction)
{
	const FName Next = FindQuickUseFrom(QuickUseItem, Direction);
	if (Next != QuickUseItem)
	{
		QuickUseItem = Next;
		OnInventoryChanged.Broadcast();
	}
}

bool UGBInventoryComponent::UseQuickItem()
{
	if (QuickUseItem.IsNone())
	{
		return false;
	}
	return UseItem(QuickUseItem);
}

bool UGBInventoryComponent::EquipArmor(FName ItemId)
{
	if (!ItemId.IsNone())
	{
		const UGBDataSubsystem* Data = UGBDataSubsystem::Get(this);
		const FGBItemDefinition* Def = Data ? Data->FindItem(ItemId) : nullptr;
		if (!Def || Def->Type != EGBItemType::Armor)
		{
			UE_LOG(LogGB, Warning, TEXT("EquipArmor: '%s' is not an armor."), *ItemId.ToString());
			return false;
		}
		if (!HasItem(ItemId))
		{
			return false;
		}
	}

	EquippedArmor = ItemId;
	ApplyArmorModifiers();
	OnInventoryChanged.Broadcast();
	return true;
}

void UGBInventoryComponent::ApplyArmorModifiers()
{
	UGBStatsComponent* Stats = UGBStatsComponent::Find(GetOwner());
	if (!Stats)
	{
		return;
	}

	Stats->RemoveModifiers(ArmorModifierSource);
	if (EquippedArmor.IsNone())
	{
		return;
	}

	const UGBDataSubsystem* Data = UGBDataSubsystem::Get(this);
	const FGBItemDefinition* Def = Data ? Data->FindItem(EquippedArmor) : nullptr;
	if (!Def || Def->Type != EGBItemType::Armor)
	{
		UE_LOG(LogGB, Warning, TEXT("Equipped armor '%s' is no longer an armor in the data; unequipping it."), *EquippedArmor.ToString());
		EquippedArmor = NAME_None;
		return;
	}
	Stats->AddModifiers(ArmorModifierSource, Def->Modifiers);
}

const FGBRecipeDefinition* UGBInventoryComponent::FindRecipe(FName ItemA, FName ItemB) const
{
	const UGBDataSubsystem* Data = UGBDataSubsystem::Get(this);
	if (!Data)
	{
		return nullptr;
	}

	for (const FGBRecipeDefinition& Recipe : Data->GetRecipes())
	{
		if ((Recipe.A == ItemA && Recipe.B == ItemB) || (Recipe.A == ItemB && Recipe.B == ItemA))
		{
			return &Recipe;
		}
	}
	return nullptr;
}

bool UGBInventoryComponent::TryCombine(FName ItemA, FName ItemB)
{
	const FGBRecipeDefinition* Found = FindRecipe(ItemA, ItemB);
	if (!Found)
	{
		ShowHudMessage(TEXT("These items cannot be combined"));
		return false;
	}
	// Copy: the steps below broadcast, and the pointer must not outlive this frame's data anyway.
	const FGBRecipeDefinition Recipe = *Found;

	const bool bSameItem = Recipe.A == Recipe.B;
	if (GetCount(Recipe.A) < (bSameItem ? 2 : 1) || !HasItem(Recipe.B))
	{
		return false;
	}

	const UGBDataSubsystem* Data = UGBDataSubsystem::Get(this);
	if (!Recipe.ResultItem.IsNone() && !(Data && Data->FindItem(Recipe.ResultItem)))
	{
		UE_LOG(LogGB, Warning, TEXT("Recipe '%s': unknown ResultItem '%s'."), *Recipe.Id.ToString(), *Recipe.ResultItem.ToString());
		return false;
	}
	if (!Recipe.ResultWeapon.IsNone() && !(Data && Data->FindWeapon(Recipe.ResultWeapon)))
	{
		UE_LOG(LogGB, Warning, TEXT("Recipe '%s': unknown ResultWeapon '%s'."), *Recipe.Id.ToString(), *Recipe.ResultWeapon.ToString());
		return false;
	}
	UGBEquipmentComponent* Equipment = GetOwner() ? GetOwner()->FindComponentByClass<UGBEquipmentComponent>() : nullptr;
	if (!Recipe.ResultWeapon.IsNone() && !Equipment)
	{
		UE_LOG(LogGB, Warning, TEXT("Recipe '%s': the owner has no equipment to receive '%s'."), *Recipe.Id.ToString(), *Recipe.ResultWeapon.ToString());
		return false;
	}

	const FString NameA = ItemDisplayName(Data ? Data->FindItem(Recipe.A) : nullptr, Recipe.A);
	const FString NameB = ItemDisplayName(Data ? Data->FindItem(Recipe.B) : nullptr, Recipe.B);

	if (Recipe.bConsumeA)
	{
		RemoveItem(Recipe.A, 1);
	}
	if (Recipe.bConsumeB)
	{
		RemoveItem(Recipe.B, 1);
	}

	FString Message;
	if (!Recipe.ResultWeapon.IsNone())
	{
		AGBHeldItem* Weapon = Equipment->GiveWeapon(Recipe.ResultWeapon, true, false);
		// GiveWeapon returns null when the weapon is stored (slot taken) or already owned and stored:
		// the element still applies to the owned weapon either way.
		Equipment->SetWeaponElement(Recipe.ResultWeapon, Recipe.ResultElement);
		if (Equipment->OwnsWeapon(Recipe.ResultWeapon))
		{
			const UGBDataSubsystem* DataNow2 = UGBDataSubsystem::Get(this);
			const FGBWeaponDefinition* WeaponDef = DataNow2 ? DataNow2->FindWeapon(Recipe.ResultWeapon) : nullptr;
			const FString WeaponName = (WeaponDef && !WeaponDef->DisplayName.IsEmpty()) ? WeaponDef->DisplayName : Recipe.ResultWeapon.ToString();
			(void)Weapon;
			Message = FString::Printf(TEXT("Forged: %s (%s)"), *WeaponName, *ElementName(Recipe.ResultElement));
		}
	}
	if (!Recipe.ResultItem.IsNone())
	{
		UGBDataSubsystem* DataNow = UGBDataSubsystem::Get(this);
		const FString ResultName = ItemDisplayName(DataNow ? DataNow->FindItem(Recipe.ResultItem) : nullptr, Recipe.ResultItem);
		if (AddItem(Recipe.ResultItem, 1) > 0 && Message.IsEmpty())
		{
			Message = FString::Printf(TEXT("Crafted: %s"), *ResultName);
		}
	}
	if (!Recipe.SetsFlag.IsNone())
	{
		if (UGBProgressSubsystem* Progress = UGBProgressSubsystem::Get(this))
		{
			Progress->SetFlag(Recipe.SetsFlag);
		}
		else
		{
			UE_LOG(LogGB, Warning, TEXT("Recipe '%s': no progress subsystem to set '%s'."), *Recipe.Id.ToString(), *Recipe.SetsFlag.ToString());
		}
	}
	if (Message.IsEmpty())
	{
		Message = FString::Printf(TEXT("Combined: %s + %s"), *NameA, *NameB);
	}

	ShowHudMessage(Message);
	OnInventoryChanged.Broadcast();
	return true;
}

void UGBInventoryComponent::SaveToSnapshot(FGBPlayerSnapshot& Snapshot) const
{
	Snapshot.Items = Items;
	Snapshot.QuickUseItem = QuickUseItem;
	Snapshot.EquippedArmor = EquippedArmor;
}

void UGBInventoryComponent::LoadFromSnapshot(const FGBPlayerSnapshot& Snapshot)
{
	TSet<FName> ChangedIds;
	for (const TPair<FName, int32>& Pair : Items)
	{
		ChangedIds.Add(Pair.Key);
	}

	Items.Reset();
	for (const TPair<FName, int32>& Pair : Snapshot.Items)
	{
		if (Pair.Value > 0)
		{
			Items.Add(Pair.Key, Pair.Value);
			ChangedIds.Add(Pair.Key);
		}
	}

	QuickUseItem = HasItem(Snapshot.QuickUseItem) ? Snapshot.QuickUseItem : FindQuickUseFrom(Snapshot.QuickUseItem, 1);
	EquippedArmor = HasItem(Snapshot.EquippedArmor) ? Snapshot.EquippedArmor : NAME_None;
	ApplyArmorModifiers();

	for (const FName& Id : ChangedIds)
	{
		OnItemCountChanged.Broadcast(Id, GetCount(Id));
	}
	OnInventoryChanged.Broadcast();
}

void UGBInventoryComponent::HandleDataReloaded()
{
	ApplyArmorModifiers();
	OnInventoryChanged.Broadcast();
}

void UGBInventoryComponent::ShowHudMessage(const FString& Text) const
{
	if (AGBHUD* Hud = AGBHUD::Get(this))
	{
		Hud->ShowMessage(Text);
	}
}
