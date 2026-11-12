// The one place where damage is dealt, plus the pure combat math (unit-tested).
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Core/GBTypes.h"
#include "GBCombatLibrary.generated.h"

UCLASS()
class GLYPHBOUND_API UGBCombatLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Deals one hit to Target (needs a UGBAttributeComponent; otherwise returns 0).
	 * Applies the instigator's DamageDealt stat, skips friendly fire (same team, unless bAllowFriendlyFire),
	 * forwards to UGBAttributeComponent::ApplyDamage, and applies Info.OnHitEffect if the target has a
	 * UGBStatusEffectComponent and the hit was not cancelled. Returns the damage really dealt.
	 */
	static float DealDamage(AActor* Target, const FGBDamageInfo& Info, bool bAllowFriendlyFire = false);

	/**
	 * Damages every hostile actor with attributes whose origin is within Radius of Origin (sphere overlap on pawns
	 * and world dynamic). With bLinearFalloff the damage goes from full at the center to 0 at the edge.
	 * Info.HitLocation / HitDirection are filled per target. Returns how many actors were damaged.
	 */
	static int32 DealRadialDamage(const UObject* WorldContextObject, const FVector& Origin, float Radius, const FGBDamageInfo& Info,
		TConstArrayView<AActor*> IgnoreActors, bool bLinearFalloff = false);

	static EGBTeam GetTeam(const AActor* Actor);

	/** Player vs Enemy are hostile; Neutral is hostile to nobody. */
	static bool AreHostile(const AActor* A, const AActor* B);
	static bool AreTeamsHostile(EGBTeam A, EGBTeam B);

	static bool IsAlive(const AActor* Actor);

	// ---- Pure math (tests in Tests/) ----

	/**
	 * Damage multiplier by distance (GDD 5 "damage by distance"): NearMult up to FalloffStart, FarMult from
	 * FalloffEnd on, linear in between. FalloffEnd <= FalloffStart means a hard step at FalloffStart.
	 */
	static float DistanceFalloffMultiplier(float Distance, float FalloffStart, float FalloffEnd, float NearMult, float FarMult);

	/**
	 * Headshot rule for sprites and capsules (GDD 3): the head is the top HeadFraction of the body.
	 * FeetZ = bottom of the body, BodyHeight = full height, HitZ = world Z of the hit.
	 */
	static bool IsHeadshot(float HitZ, float FeetZ, float BodyHeight, float HeadFraction);

	/** A random direction inside a cone of SpreadDegrees (full angle = 2 * half-angle... SpreadDegrees is the half-angle). */
	static FVector ApplySpread(const FVector& Direction, float SpreadHalfAngleDegrees, FRandomStream& Random);
};
