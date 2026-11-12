#include "Weapons/GBHeldItem.h"

#include "Combat/GBCombatLibrary.h"
#include "Combat/GBStatusEffectComponent.h"
#include "Core/GBStatNames.h"
#include "Data/GBDataSubsystem.h"
#include "Engine/World.h"
#include "Player/GBPlayerCharacter.h"
#include "Spells/GBSpellbookComponent.h"
#include "Stats/GBStatsComponent.h"
#include "Weapons/GBEquipmentComponent.h"

AGBHeldItem::AGBHeldItem()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	SetActorEnableCollision(false);
}

void AGBHeldItem::InitFromDefinition(const FGBWeaponDefinition& Definition, AGBPlayerCharacter* InOwner)
{
	WeaponId = Definition.Id;
	Slot = Definition.Slot;
	Element = Definition.Element;
	PlayerOwner = InOwner;
	SetOwner(InOwner);
	SetInstigator(InOwner);
	Random.GenerateNewSeed();
}

const FGBWeaponDefinition* AGBHeldItem::GetDefinition() const
{
	const UGBDataSubsystem* Data = UGBDataSubsystem::Get(this);
	return Data ? Data->FindWeapon(WeaponId) : nullptr;
}

FString AGBHeldItem::GetDisplayName() const
{
	const FGBWeaponDefinition* Def = GetDefinition();
	return (Def && !Def->DisplayName.IsEmpty()) ? Def->DisplayName : WeaponId.ToString();
}

float AGBHeldItem::GetStat(FName Stat, float DefaultValue) const
{
	float Base = DefaultValue;
	if (const FGBWeaponDefinition* Def = GetDefinition())
	{
		if (const float* Value = Def->Stats.Find(Stat))
		{
			Base = *Value;
		}
	}

	if (const UGBStatsComponent* Stats = GetOwnerStats())
	{
		TArray<FName, TInlineAllocator<3>> Keys;
		Keys.Add(GBStat::WeaponKey(WeaponId, Stat));
		Keys.Add(GBStat::WeaponSlotKey(Slot, Stat));
		Keys.Add(GBStat::WeaponAllKey(Stat));
		return Stats->EvaluateKeys(Keys, Base);
	}
	return Base;
}

void AGBHeldItem::OnEquipped()
{
	bEquipped = true;
	SetActorHiddenInGame(false);
}

void AGBHeldItem::OnUnequipped()
{
	bEquipped = false;
	bTriggerHeld = false;
	bAttackQueued = false;
	SetActorHiddenInGame(true);
}

EGBHudSkin AGBHeldItem::GetHudSkin() const
{
	return Slot == EGBHeldSlot::Gun ? EGBHudSkin::Cyberpunk : EGBHudSkin::Rpg90s;
}

float AGBHeldItem::GetTimeSinceLastAttack() const
{
	const UWorld* World = GetWorld();
	return World ? static_cast<float>(World->GetTimeSeconds() - LastAttackTime) : 1000.f;
}

bool AGBHeldItem::IsAutomatic() const
{
	const FGBWeaponDefinition* Def = GetDefinition();
	return Def && Def->bAutomatic;
}

UGBStatsComponent* AGBHeldItem::GetOwnerStats() const
{
	const AGBPlayerCharacter* Player = GetPlayerOwner();
	return Player ? Player->GetStats() : nullptr;
}

bool AGBHeldItem::GetActiveEnchant(EGBElement& OutElement, FName& OutOnHitEffect) const
{
	const AGBPlayerCharacter* Player = GetPlayerOwner();
	const UGBStatusEffectComponent* Effects = Player ? Player->GetStatusEffects() : nullptr;
	const UGBDataSubsystem* Data = UGBDataSubsystem::Get(this);
	if (!Effects || !Data)
	{
		return false;
	}

	for (const FGBActiveStatusEffect& Active : Effects->GetActiveEffects())
	{
		const FGBStatusEffectDefinition* Def = Data->FindStatusEffect(Active.EffectId);
		if (!Def)
		{
			continue;
		}
		const FString* ElementName = Def->Strings.Find(GBWeaponString::WeaponElement);
		if (!ElementName || ElementName->IsEmpty())
		{
			continue;
		}
		const int64 Value = StaticEnum<EGBElement>()->GetValueByNameString(*ElementName);
		if (Value == INDEX_NONE)
		{
			UE_LOG(LogGB, Warning, TEXT("Status effect '%s': unknown WeaponElement '%s'"), *Active.EffectId.ToString(), **ElementName);
			continue;
		}
		OutElement = static_cast<EGBElement>(Value);
		const FString* Effect = Def->Strings.Find(GBWeaponString::WeaponOnHitEffect);
		OutOnHitEffect = (Effect && !Effect->IsEmpty()) ? FName(**Effect) : NAME_None;
		return true;
	}
	return false;
}

FGBDamageInfo AGBHeldItem::MakeDamageInfo() const
{
	FGBDamageInfo Info;
	Info.Amount = GetStat(GBStat::Damage);
	Info.Element = Element;
	Info.Instigator = GetPlayerOwner();
	Info.Causer = const_cast<AGBHeldItem*>(this);
	Info.SourceId = WeaponId;

	if (const FGBWeaponDefinition* Def = GetDefinition())
	{
		if (const FString* Effect = Def->Strings.Find(GBString::OnHitEffect))
		{
			if (!Effect->IsEmpty())
			{
				Info.OnHitEffect = FName(**Effect);
			}
		}
	}

	// The enchant's damage multiplier arrives by itself through its "Weapon.Gun.Damage" modifier.
	EGBElement EnchantElement = EGBElement::Physical;
	FName EnchantEffect = NAME_None;
	if (AcceptsEnchant() && GetActiveEnchant(EnchantElement, EnchantEffect))
	{
		Info.Element = EnchantElement;
		if (!EnchantEffect.IsNone())
		{
			Info.OnHitEffect = EnchantEffect;
		}
	}

	const int32 ElementIndex = GBElemental::ElementIndex(Info.Element);
	if (ElementIndex >= 0)
	{
		Info.BuildUp = GetStat(GBElemental::WeaponBuildUpKey(ElementIndex), 0.f);
		const UGBDataSubsystem* Data = UGBDataSubsystem::Get(this);
		const float DefaultMult = Data ? Data->GetGameValue(GBStat::WeakPointBuildUpMult, 1.f) : 1.f;
		Info.WeakPointBuildUpMult = GetStat(GBStat::WeakPointBuildUpMult, DefaultMult);
	}
	Info.BuildUpEventId = UGBCombatLibrary::NewBuildUpEventId();
	return Info;
}

void AGBHeldItem::PressTrigger()
{
	bTriggerHeld = true;
	bAttackQueued = true;
	UpdateTrigger();
}

void AGBHeldItem::ReleaseTrigger()
{
	bTriggerHeld = false;
	bAttackQueued = false;
}

bool AGBHeldItem::IsAttackReady() const
{
	return GetTimeSinceLastAttack() >= GetStat(GBStat::FireInterval, 0.f);
}

void AGBHeldItem::UpdateTrigger()
{
	if (!bEquipped || !(bAttackQueued || (bTriggerHeld && IsAutomatic())))
	{
		return;
	}
	if (!IsAttackReady())
	{
		return; // a pressed shot stays queued until the fire interval has passed
	}

	const AGBPlayerCharacter* Player = GetPlayerOwner();
	const UGBSpellbookComponent* Spellbook = Player ? Player->GetSpellbook() : nullptr;
	if (!Player || Player->IsDead() || (Spellbook && Spellbook->IsBusy()))
	{
		bAttackQueued = false;
		return;
	}

	if (PerformAttack())
	{
		if (const UWorld* World = GetWorld())
		{
			LastAttackTime = World->GetTimeSeconds();
		}
	}
	bAttackQueued = false;
}

void AGBHeldItem::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateTrigger();
}

FVector AGBHeldItem::FindAimPoint(float Range) const
{
	const AGBPlayerCharacter* Player = GetPlayerOwner();
	if (!Player)
	{
		return GetActorLocation();
	}
	const FVector Start = Player->GetAimOrigin();
	const FVector End = Start + Player->GetAimDirection() * Range;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(GBAimPoint), false, Player);
	Params.AddIgnoredActor(this);
	FHitResult Hit;
	if (GetWorld() && GetWorld()->LineTraceSingleByChannel(Hit, Start, End, GBWeapon::AimTraceChannel, Params))
	{
		return Hit.ImpactPoint;
	}
	return End;
}

void AGBHeldItem::NotifyEquipmentChanged() const
{
	const AGBPlayerCharacter* Player = GetPlayerOwner();
	if (UGBEquipmentComponent* Equipment = Player ? Player->GetEquipment() : nullptr)
	{
		Equipment->OnEquipmentChanged.Broadcast();
	}
}
