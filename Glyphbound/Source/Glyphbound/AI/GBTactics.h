// Pure rules of the enemy tactics (unit-tested, no world): the leash zone, squad roles, attack tokens and the score of a
// tactical position. The squad subsystem (GBSquad) and the enemies call these; every weight comes from Game.json.
#pragma once

#include "CoreMinimal.h"

enum class EGBSquadRole : uint8
{
	None,
	/** Base of fire: holds a position with a view and fires the most. */
	Suppress,
	FlankLeft,
	FlankRight,
	/** Bounds forward from cover to cover. */
	Assault,
	/** The shield chief: walks at the player, he is the cover; never uses cover. */
	Anchor
};

namespace GBTactics
{
	GLYPHBOUND_API const TCHAR* RoleName(EGBSquadRole Role);

	/** One blocked hit of the shield: when and how much. */
	struct FBlockedHit
	{
		float Time = 0.f;
		float Amount = 0.f;
	};
	/**
	 * Adds a blocked hit and returns the damage blocked inside the last WindowSeconds (older hits are dropped, so each hit does not
	 * restart the window: three hits 2 s apart are never one sum).
	 */
	GLYPHBOUND_API float AddBlockedHit(TArray<FBlockedHit>& Hits, float Now, float Amount, float WindowSeconds);

	// ---- Zone (leash) ----
	/** Nearest point of the box to P (all three axes). An invalid box returns P. */
	GLYPHBOUND_API FVector ClampToBox(const FBox& Box, const FVector& P);
	GLYPHBOUND_API bool IsInsideBox(const FBox& Box, const FVector& P);
	/** The zone: bounds of the points and of the trigger box, grown by Padding on X and Y and by ZMargin on Z. */
	GLYPHBOUND_API FBox MakeZone(const FBox& TriggerBounds, TConstArrayView<FVector> Points, float Padding, float ZMargin);

	// ---- Roles ----
	struct FRoleInput
	{
		float DistanceToPlayer = 0.f;
		/** 0..1: pushes towards Assault / close flanks and away from holding. */
		float Aggression = 0.5f;
		bool bAnchor = false;
	};
	/**
	 * One role per input, in input order. An Anchor is always Anchor and, while one lives, takes the base-of-fire job: the
	 * rest prefer flanks. Without one the member nearest to the player (distance scaled up by aggression) is Suppress, the
	 * next two are FlankLeft and FlankRight, the extra ones alternate Suppress and Assault (an aggressive extra pushes).
	 */
	GLYPHBOUND_API TArray<EGBSquadRole> AssignRoles(TConstArrayView<FRoleInput> Members);

	// ---- Attack tokens ----
	struct FTokenRequest
	{
		int32 Id = 0;
		/** Time it started waiting. */
		float WaitingSince = 0.f;
		/** Time its last burst ended (a large negative number when it never fired). */
		float LastBurstEnd = -1000.f;
		/** How much of the pool it uses: 1 for a shooter, 0.5 for a flyer. */
		float Weight = 1.f;
	};
	/**
	 * Ids granted a token now, in order. Capacity is MaxShooters (plus 1 when TotalMembers >= BigSquadSize); the holders'
	 * weights are counted first. The longest waiting goes first; a member that fired less than MinRest ago waits behind
	 * every rested one (it still gets a token when nobody else wants it).
	 */
	GLYPHBOUND_API TArray<int32> GrantTokens(TConstArrayView<FTokenRequest> Waiting, float HeldWeight, float MaxShooters, int32 TotalMembers, int32 BigSquadSize, float Now, float MinRest);

	// ---- Position score ----
	struct FPositionWeights
	{
		float CoverWithFiringSpot = 100.f;
		float OpenWithLineOfFire = 55.f;
		float CoverWithoutFiringSpot = 25.f;
		float InBand = 30.f;
		float Spread = 20.f;
		float Flank = 30.f;
		float Path = 10.f;
		float CrossFire = 25.f;
		float ViewCone = 20.f;
		/** Angle (degrees) at which two members stop counting as bunched up. */
		float SpreadFullAngle = 60.f;
		/** Reward for standing on a side where the player has room to escape (pushes him to the walls). 0 = off. */
		float Corner = 0.f;
	};
	struct FPositionInput
	{
		bool bCovered = false;
		bool bFiringSpot = false;
		bool bOpenLineOfFire = false;
		float DistanceToPlayer = 0.f;
		float RangeMin = 600.f;
		float RangeMax = 1400.f;
		/** Smallest angle (degrees) between this point and any other member / reserved position, seen from the player. 180 when alone. */
		float MinAngleToOthers = 180.f;
		EGBSquadRole Role = EGBSquadRole::Suppress;
		/** Angle (degrees, 0..180) round the player from the base of fire's line. */
		float AngleFromBase = 0.f;
		float PathLength = 0.f;
		bool bCrossesFireLine = false;
		bool bInViewCone = false;
		/** 0..1: how open the player's escape is in the direction of this point (1 = nothing blocks him that way). */
		float EscapeOpenness = 0.f;
	};
	/** Higher is better. A point nearer than RangeMin * 0.5 to the player is never acceptable (very negative). */
	GLYPHBOUND_API float ScorePosition(const FPositionInput& In, const FPositionWeights& W);
	/** True when P is closer than Spacing to any of Others (2D). */
	GLYPHBOUND_API bool IsTooClose(const FVector& P, TConstArrayView<FVector> Others, float Spacing);
}
