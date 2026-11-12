#include "Combat/GBCombatLibrary.h"

#include "AI/GBEnemyCharacter.h"
#include "Combat/GBStatusEffectComponent.h"
#include "Core/GBStatNames.h"
#include "Stats/GBAttributeComponent.h"
#include "Stats/GBStatsComponent.h"
#include "Engine/Engine.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"

namespace GBCombatLocal
{
	/** The obstruction trace starts this far above the explosion origin, so the floor under it does not block it. */
	constexpr float RadialTraceLiftCm = 5.f;
}

FGBOnDamageDealt& UGBCombatLibrary::OnDamageDealt()
{
	static FGBOnDamageDealt Event;
	return Event;
}

float UGBCombatLibrary::DealDamage(AActor* Target, const FGBDamageInfo& Info, bool bAllowFriendlyFire)
{
	if (!IsValid(Target))
	{
		return 0.f;
	}

	UGBAttributeComponent* Attributes = UGBAttributeComponent::Find(Target);
	if (!Attributes)
	{
		return 0.f;
	}

	AActor* DamageInstigator = Info.Instigator.Get();
	if (!IsValid(DamageInstigator))
	{
		DamageInstigator = nullptr;
	}

	if (!CanDamage(DamageInstigator, Target))
	{
		// Friendly fire explicitly allows same-team hits; anything without attributes or dead stays immune.
		if (!bAllowFriendlyFire || !UGBAttributeComponent::Find(Target) || !IsAlive(Target))
		{
			return 0.f;
		}
	}

	FGBDamageInfo FinalInfo = Info;
	FinalInfo.Instigator = DamageInstigator;
	if (DamageInstigator)
	{
		if (const UGBStatsComponent* InstigatorStats = UGBStatsComponent::Find(DamageInstigator))
		{
			FinalInfo.Amount *= InstigatorStats->GetStat(GBStat::DamageDealt, 1.f);
		}
	}

	if (FinalInfo.Amount <= 0.f)
	{
		return 0.f;
	}

	const float Dealt = Attributes->ApplyDamage(FinalInfo);
	if (Dealt > 0.f)
	{
		OnDamageDealt().Broadcast(Target, FinalInfo, Dealt);
	}

	if (Dealt > 0.f && !FinalInfo.OnHitEffect.IsNone())
	{
		if (UGBStatusEffectComponent* Effects = UGBStatusEffectComponent::Find(Target))
		{
			Effects->ApplyEffect(FinalInfo.OnHitEffect, DamageInstigator, FinalInfo.EffectDurationScale, FinalInfo.EffectStrengthScale);
		}
	}

	return Dealt;
}

int32 UGBCombatLibrary::DealRadialDamage(const UObject* WorldContextObject, const FVector& Origin, float Radius, const FGBDamageInfo& Info,
	TConstArrayView<AActor*> IgnoreActors, bool bLinearFalloff)
{
	if (Radius <= 0.f || !GEngine)
	{
		return 0;
	}

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!World)
	{
		return 0;
	}

	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
	ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(GBRadialDamage), false);
	for (AActor* Ignored : IgnoreActors)
	{
		if (Ignored)
		{
			QueryParams.AddIgnoredActor(Ignored);
		}
	}

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps, Origin, FQuat::Identity, ObjectParams, FCollisionShape::MakeSphere(Radius), QueryParams);

	AActor* DamageInstigator = Info.Instigator.Get();
	if (!IsValid(DamageInstigator))
	{
		DamageInstigator = nullptr;
	}

	TSet<AActor*> Visited;
	int32 DamagedCount = 0;

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Target = Overlap.GetActor();
		if (!IsValid(Target) || Visited.Contains(Target))
		{
			continue;
		}
		Visited.Add(Target);

		if (IgnoreActors.Contains(Target) || !UGBAttributeComponent::Find(Target))
		{
			continue;
		}

		if (!CanDamage(DamageInstigator, Target))
		{
			continue;
		}

		const FVector TargetLocation = Target->GetActorLocation();
		const float Distance = FVector::Dist(TargetLocation, Origin);
		if (Distance > Radius)
		{
			continue;
		}

		// Solid geometry between the blast and the target blocks the damage.
		FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(GBRadialObstruction), true);
		TraceParams.AddIgnoredActor(Target);
		for (AActor* Ignored : IgnoreActors)
		{
			if (Ignored)
			{
				TraceParams.AddIgnoredActor(Ignored);
			}
		}
		FHitResult Obstruction;
		const FVector TraceStart = Origin + FVector(0.f, 0.f, GBCombatLocal::RadialTraceLiftCm);
		if (World->LineTraceSingleByChannel(Obstruction, TraceStart, TargetLocation, ECC_Visibility, TraceParams)
			&& Obstruction.bBlockingHit && Obstruction.GetActor() && Obstruction.GetActor() != Target)
		{
			continue;
		}

		FGBDamageInfo TargetInfo = Info;
		TargetInfo.HitLocation = TargetLocation;
		TargetInfo.HitDirection = (TargetLocation - Origin).GetSafeNormal();
		if (bLinearFalloff)
		{
			TargetInfo.Amount *= FMath::Clamp(1.f - Distance / Radius, 0.f, 1.f);
		}

		if (DealDamage(Target, TargetInfo) > 0.f)
		{
			++DamagedCount;
		}
	}

	return DamagedCount;
}

EGBTeam UGBCombatLibrary::GetTeam(const AActor* Actor)
{
	const UGBAttributeComponent* Attributes = UGBAttributeComponent::Find(Actor);
	return Attributes ? Attributes->Team : EGBTeam::Neutral;
}

bool UGBCombatLibrary::AreHostile(const AActor* A, const AActor* B)
{
	if (!A || !B)
	{
		return false;
	}
	return AreTeamsHostile(GetTeam(A), GetTeam(B));
}

bool UGBCombatLibrary::AreTeamsHostile(EGBTeam A, EGBTeam B)
{
	return (A == EGBTeam::Player && B == EGBTeam::Enemy) || (A == EGBTeam::Enemy && B == EGBTeam::Player);
}

bool UGBCombatLibrary::IsAlive(const AActor* Actor)
{
	if (!IsValid(Actor))
	{
		return false;
	}
	const UGBAttributeComponent* Attributes = UGBAttributeComponent::Find(Actor);
	return !Attributes || !Attributes->IsDead();
}

bool UGBCombatLibrary::CanDamage(const AActor* Instigator, const AActor* Target)
{
	if (!IsValid(Target))
	{
		return false;
	}
	const UGBAttributeComponent* TargetAttributes = UGBAttributeComponent::Find(Target);
	if (!TargetAttributes || TargetAttributes->IsDead())
	{
		return false;
	}
	if (!IsValid(Instigator))
	{
		return true;
	}
	const EGBTeam InstigatorTeam = GetTeam(Instigator);
	const EGBTeam TargetTeam = TargetAttributes->Team;
	if (InstigatorTeam == EGBTeam::Neutral || TargetTeam == EGBTeam::Neutral)
	{
		return true;
	}
	return AreTeamsHostile(InstigatorTeam, TargetTeam);
}

float UGBCombatLibrary::DistanceFalloffMultiplier(float Distance, float FalloffStart, float FalloffEnd, float NearMult, float FarMult)
{
	if (Distance <= FalloffStart)
	{
		return NearMult;
	}
	if (FalloffEnd <= FalloffStart || Distance >= FalloffEnd)
	{
		return FarMult;
	}
	const float Alpha = (Distance - FalloffStart) / (FalloffEnd - FalloffStart);
	return FMath::Lerp(NearMult, FarMult, Alpha);
}

bool UGBCombatLibrary::IsHeadshot(float HitZ, float FeetZ, float BodyHeight, float HeadFraction)
{
	if (HeadFraction <= 0.f || BodyHeight <= 0.f)
	{
		return false;
	}
	const float HeadLineZ = FeetZ + BodyHeight * (1.f - FMath::Min(HeadFraction, 1.f));
	return HitZ >= HeadLineZ;
}

FVector UGBCombatLibrary::ApplySpread(const FVector& Direction, float SpreadHalfAngleDegrees, FRandomStream& Random)
{
	const FVector Dir = Direction.GetSafeNormal();
	if (Dir.IsNearlyZero() || SpreadHalfAngleDegrees <= 0.f)
	{
		return Dir;
	}
	return Random.VRandCone(Dir, FMath::DegreesToRadians(SpreadHalfAngleDegrees));
}

bool UGBCombatLibrary::ApplyWeakPointBonus(const AActor* Target, FGBDamageInfo& Info, float WeakPointMult)
{
	Info.bHeadshot = false;
	if (WeakPointMult <= 1.f)
	{
		return false;
	}
	const AGBEnemyCharacter* Enemy = Cast<AGBEnemyCharacter>(Target);
	if (!Enemy || !Enemy->IsHeadshotLocation(Info.HitLocation))
	{
		return false;
	}
	Info.Amount *= WeakPointMult;
	Info.bHeadshot = true;
	return true;
}

FVector UGBCombatLibrary::ClosestBodyPointToAim(const FVector& AimOrigin, const FVector& AimDirection, float AimLength, const FVector& BodyBottom, const FVector& BodyTop)
{
	const FVector AimEnd = AimOrigin + AimDirection * AimLength;
	FVector OutAimPoint = AimOrigin;
	FVector OutBodyPoint = BodyBottom;
	FMath::SegmentDistToSegmentSafe(AimOrigin, AimEnd, BodyBottom, BodyTop, OutAimPoint, OutBodyPoint);
	return OutBodyPoint;
}
