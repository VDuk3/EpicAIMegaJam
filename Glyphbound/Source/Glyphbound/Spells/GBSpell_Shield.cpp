#include "Spells/GBSpell_Shield.h"
#include "Core/GBStatNames.h"
#include "Engine/World.h"
#include "Spells/GBSpellbookComponent.h"
#include "Stats/GBAttributeComponent.h"
#include "Stats/GBStatsComponent.h"

const FName AGBSpell_Shield::RaisedSource(TEXT("Shield.Raised"));

AGBSpell_Shield::AGBSpell_Shield()
{
	PrimaryActorTick.bCanEverTick = false;
}

float AGBSpell_Shield::ComputeBlockedDamage(float Amount, const FVector& HitDirection, const FVector& AimDirection, float InBlockFraction, float ShieldHealthLeft, float BlockArcDegrees)
{
	if (Amount <= 0.f || ShieldHealthLeft <= 0.f || HitDirection.IsNearlyZero() || AimDirection.IsNearlyZero())
	{
		return 0.f;
	}
	const float ClampedArc = FMath::Clamp(BlockArcDegrees, 0.f, 360.f);
	if (ClampedArc <= 0.f)
	{
		return 0.f;
	}
	if (ClampedArc < 360.f)
	{
		// A hit blocks when the direction to the attacker is within half the arc of the aim: the dot of the
		// hit travel direction (source to caster) with the aim is then below minus the cosine of half the arc.
		// cos(half of 180) is snapped to 0, so 180 behaves exactly like the old "Dot >= 0" front rule.
		const float CosHalf = FMath::IsNearlyEqual(ClampedArc, 180.f) ? 0.f : FMath::Cos(FMath::DegreesToRadians(ClampedArc * 0.5f));
		if (FVector::DotProduct(HitDirection.GetSafeNormal(), AimDirection.GetSafeNormal()) >= -CosHalf)
		{
			return 0.f;
		}
	}
	return FMath::Min(Amount * FMath::Clamp(InBlockFraction, 0.f, 1.f), ShieldHealthLeft);
}

void AGBSpell_Shield::StartSpell()
{
	const float Duration = FMath::Max(0.f, ReadParam(GBStat::Duration, 20.f));
	BlockFraction = FMath::Clamp(ReadParam(GBStat::BlockFraction, 0.75f), 0.f, 1.f);
	BlockArcDegrees = FMath::Clamp(ReadParam(GBShieldParam::BlockArcDegrees, 180.f), 0.f, 360.f);
	MaxShieldHealth = FMath::Max(1.f, ReadParam(GBStat::Health, 150.f));
	ShieldHealth = MaxShieldHealth;
	RaisedMoveSpeedMult = FMath::Max(0.f, ReadParam(GBShieldParam::RaisedMoveSpeedMult, 0.6f));

	AActor* Caster = GetCaster();
	if (UGBAttributeComponent* Attributes = UGBAttributeComponent::Find(Caster))
	{
		BoundAttributes = Attributes;
		DamageHandle = Attributes->OnModifyIncomingDamage.AddUObject(this, &AGBSpell_Shield::HandleIncomingDamage);
	}
	else
	{
		UE_LOG(LogGB, Warning, TEXT("Spell '%s': the caster has no attributes, the shield cannot block."), *SpellCast.SpellId.ToString());
	}

	// Follow the caster (nothing is drawn, but the actor should not stay behind at the cast point).
	if (Caster && Caster->GetRootComponent())
	{
		AttachToComponent(Caster->GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
	}

	const UWorld* World = GetWorld();
	ExpireTime = (World ? World->GetTimeSeconds() : 0.0) + Duration;
	EndSpellAfter(Duration);

	UGBSpellbookComponent* Spellbook = Caster ? Caster->FindComponentByClass<UGBSpellbookComponent>() : nullptr;
	if (Spellbook)
	{
		Spellbook->RegisterShield(this);
	}
	else
	{
		UE_LOG(LogGB, Warning, TEXT("Spell '%s': the caster has no spellbook, the shield can never be raised."), *SpellCast.SpellId.ToString());
	}
}

void AGBSpell_Shield::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SetRaised(false);
	bEnded = true;

	AActor* Caster = GetCaster();
	if (IsValid(Caster))
	{
		if (UGBSpellbookComponent* Spellbook = Caster->FindComponentByClass<UGBSpellbookComponent>())
		{
			Spellbook->UnregisterShield(this);
		}
	}
	if (UGBAttributeComponent* Attributes = BoundAttributes.Get())
	{
		Attributes->OnModifyIncomingDamage.Remove(DamageHandle);
	}
	BoundAttributes.Reset();
	Super::EndPlay(EndPlayReason);
}

void AGBSpell_Shield::SetRaised(bool bInRaised)
{
	const bool bNewRaised = bInRaised && IsShieldActive();
	if (bNewRaised == bRaised)
	{
		return;
	}
	bRaised = bNewRaised;

	AActor* Caster = GetCaster();
	UGBStatsComponent* Stats = IsValid(Caster) ? UGBStatsComponent::Find(Caster) : nullptr;
	if (!Stats)
	{
		return;
	}
	if (bRaised)
	{
		FGBModifierDef Slow;
		Slow.Stat = GBStat::MoveSpeed;
		Slow.Op = EGBModOp::Multiply;
		Slow.Value = RaisedMoveSpeedMult;
		TArray<FGBModifierDef> Modifiers;
		Modifiers.Add(Slow);
		Stats->AddModifiers(RaisedSource, Modifiers);
	}
	else
	{
		Stats->RemoveModifiers(RaisedSource);
	}
}

float AGBSpell_Shield::GetTimeRemaining() const
{
	const UWorld* World = GetWorld();
	if (bEnded || !bSpellStarted || !World)
	{
		return 0.f;
	}
	return static_cast<float>(FMath::Max(0.0, ExpireTime - World->GetTimeSeconds()));
}

void AGBSpell_Shield::HandleIncomingDamage(FGBDamageInfo& Info)
{
	if (!bRaised || bEnded || ShieldHealth <= 0.f)
	{
		return;
	}
	AActor* Caster = GetCaster();
	FVector HitDirection = Info.HitDirection;
	if (HitDirection.IsNearlyZero() && Caster)
	{
		const AActor* Source = Info.Causer ? Info.Causer.Get() : Info.Instigator.Get();
		if (Source)
		{
			HitDirection = Caster->GetActorLocation() - Source->GetActorLocation();
		}
	}
	FVector AimOrigin;
	FVector AimDir;
	GetCasterAim(AimOrigin, AimDir);

	const float Blocked = ComputeBlockedDamage(Info.Amount, HitDirection, AimDir, BlockFraction, ShieldHealth, BlockArcDegrees);
	if (Blocked <= 0.f)
	{
		return;
	}
	Info.Amount -= Blocked;
	ShieldHealth -= Blocked;
	if (ShieldHealth <= KINDA_SMALL_NUMBER)
	{
		ShieldHealth = 0.f;
		SetRaised(false);
		// Broken: end after this damage call has finished.
		EndSpellAfter(0.f);
	}
}
