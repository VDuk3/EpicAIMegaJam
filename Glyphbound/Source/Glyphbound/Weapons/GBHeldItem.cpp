#include "Weapons/GBHeldItem.h"

#include "Combat/GBCombatLibrary.h"
#include "Combat/GBStatusEffectComponent.h"
#include "Core/GBStatNames.h"
#include "Data/GBDataSubsystem.h"
#include "Engine/World.h"
#include "Player/GBPlayerCharacter.h"
#include "Spells/GBSpellbookComponent.h"
#include "Stats/GBStatsComponent.h"
#include "UI/GBStaffView.h"
#include "Weapons/GBEquipmentComponent.h"
#include "Components/PointLightComponent.h"

namespace GBHeldLight
{
	// Weapons.json: a faint torch-like light of the gem's colour while the staff is held (owner, 2026-11-15).
	const FName LightColor(TEXT("LightColor"));          // Strings: "#RRGGBB"; missing = no light
	const FName LightIntensity(TEXT("LightIntensity"));  // Stats: candelas at the pulse's middle
	const FName LightRadius(TEXT("LightRadius"));        // Stats: attenuation radius, cm
	const FName GlowPulsePeriod(TEXT("GlowPulsePeriod")); // Stats: seconds of one slow breath (the HUD gem glow uses the same)
	/** The light sits this far from the camera, in the direction of the drawn staff's head. */
	constexpr float HeadDistanceCm = 60.f;

	/** 0..1 slow, slightly irregular breath; the HUD repeats this formula for the gem glow so both move together. */
	float Pulse01(float Time, float Period)
	{
		const float Angle = 2.f * PI * Time / FMath::Max(0.5f, Period);
		return FMath::Clamp(0.5f + 0.35f * FMath::Sin(Angle) + 0.15f * FMath::Sin(Angle * 2.7f + 1.3f), 0.f, 1.f);
	}

	/** Movable point light (dynamic: lit by Lumen / ray traced shadows), created the first time the staff is held. */
	void Update(AGBHeldItem* Item)
	{
		const FGBWeaponDefinition* Def = Item->GetDefinition();
		const FString* ColorHex = Def ? Def->Strings.Find(LightColor) : nullptr;
		if (!ColorHex || ColorHex->IsEmpty())
		{
			return;
		}
		UPointLightComponent* Light = Item->FindComponentByClass<UPointLightComponent>();
		if (!Light)
		{
			Light = NewObject<UPointLightComponent>(Item, TEXT("StaffLight"));
			Light->SetMobility(EComponentMobility::Movable);
			Light->SetUsingAbsoluteLocation(true);
			Light->SetUsingAbsoluteRotation(true);
			Light->SetupAttachment(Item->GetRootComponent());
			Light->SetIntensityUnits(ELightUnits::Candelas);
			Light->SetLightColor(FColor::FromHex(*ColorHex));
			Light->SetAttenuationRadius(Item->GetStat(LightRadius, 800.f));
			Light->SetSourceRadius(4.f);
			Light->SetCastShadows(true);
			Light->RegisterComponent();
			Item->AddInstanceComponent(Light);
		}

		const AGBPlayerCharacter* Player = Cast<AGBPlayerCharacter>(Item->GetOwner());
		const bool bShow = Player && Item->IsEquipped();
		if (Light->IsVisible() != bShow)
		{
			Light->SetVisibility(bShow);
		}
		if (!bShow)
		{
			return;
		}

		const FRotator AimRot = Player->GetAimDirection().Rotation();
		FVector HeadDir;
		if (!GBStaffView::GetHeadAimDir(HeadDir))
		{
			HeadDir = FVector(1.f, 0.35f, -0.2f); // lower right of the view, where the staff stands
		}
		Light->SetWorldLocation(Player->GetAimOrigin() + AimRot.RotateVector(HeadDir.GetSafeNormal()) * HeadDistanceCm);

		const float Time = Item->GetWorld() ? (float)Item->GetWorld()->GetTimeSeconds() : 0.f;
		const float Pulse = Pulse01(Time, Item->GetStat(GlowPulsePeriod, 2.4f));
		Light->SetIntensity(Item->GetStat(LightIntensity, 6.f) * (0.8f + 0.4f * Pulse));
	}
}

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

	// The enchant's damage multiplier arrives by itself through its "Weapon.Gun.Damage" / "Weapon.Sword.Damage" modifiers.
	// Swords take the enchant too (owner, 2026-11-15); it replaces the sword's own element while it lasts.
	bool bElementFromEnchant = false;
	EGBElement EnchantElement = EGBElement::Physical;
	FName EnchantEffect = NAME_None;
	if ((AcceptsEnchant() || Slot == EGBHeldSlot::Sword) && GetActiveEnchant(EnchantElement, EnchantEffect))
	{
		Info.Element = EnchantElement;
		if (!EnchantEffect.IsNone())
		{
			Info.OnHitEffect = EnchantEffect;
		}
		bElementFromEnchant = true;
	}

	const int32 ElementIndex = GBElemental::ElementIndex(Info.Element);
	if (ElementIndex >= 0)
	{
		Info.BuildUp = GetStat(GBElemental::WeaponBuildUpKey(ElementIndex), 0.f);
		if (bElementFromEnchant)
		{
			Info.BuildUp *= GetStat(GBStat::EnchantBuildUpMult, 1.f);
		}
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
	GBHeldLight::Update(this);
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
