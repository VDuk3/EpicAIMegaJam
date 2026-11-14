// Ledge climb (mantle) math: pure logic with no UObject, so the automation tests can cover it (GDD: climb).
#pragma once

#include "CoreMinimal.h"

namespace GBMantle
{
	/** Tunables from Player.json (all cm / seconds; RiseShare is a 0..1 fraction). */
	struct FMantleParams
	{
		float MaxReach = 160.f;
		float MinHeight = 50.f;
		float AirMinHeight = 15.f;
		float BaseDuration = 0.22f;
		float DurationPerMeter = 0.18f;
		float RiseShare = 0.6f;
	};

	/** View pose weights at one point of the climb, all 0..1. */
	struct FMantlePose
	{
		float ItemLower01 = 0.f;
		float Grip01 = 0.f;
		float CameraDip01 = 0.f;
	};

	/** How many probe directions the directionless search tries (45 degrees apart, covering the full circle). */
	constexpr int32 MaxProbeDirections = 8;

	/** One jump's rise: v^2 / (2 g). Guards a non-positive gravity (returns 0). Units follow the inputs (cm). */
	float JumpRise(float JumpZVelocity, float Gravity);
	/** From the ground, on a jump press: MinHeight <= LedgeHeight <= JumpRise + MaxReach. */
	bool CanMantleFromGround(float LedgeHeight, float JumpRiseCm, const FMantleParams& Params);
	/** In the air: AirMinHeight <= LedgeHeight <= MaxReach. */
	bool CanMantleFromAir(float LedgeHeight, const FMantleParams& Params);
	/** Climb duration from the rise: base + per-metre * rise / 100, at least 0.05 s. */
	float Duration(float RiseCm, const FMantleParams& Params);
	/**
	 * Actor location along the mantle at Alpha in [0,1] (clamped). The first RiseShare of Alpha goes straight
	 * up from Start to (Start.XY, End.Z) with an ease-out; the rest goes horizontally to End with a smoothstep.
	 * Exactly Start at 0 and End at 1.
	 */
	FVector EvaluatePath(const FVector& Start, const FVector& End, float Alpha, float RiseShare);
	/** View pose weights at Alpha (clamped). All exactly 0 at Alpha 0 and at Alpha 1. */
	FMantlePose EvaluatePose(float Alpha, float RiseShare);
	/**
	 * Directions to probe for a ledge, in priority order (2D unit vectors).
	 * With an intent (non-zero IntentDir) only that direction: the player chose where to go. Without one,
	 * MaxProbeDirections directions 45 degrees apart, starting with ViewForward, then +45, -45, +90, -90,
	 * +135, -135 and 180 degrees (positive = counterclockwise first), so the nearest to the view wins.
	 * Uses an inline allocator: no heap allocation, safe to call every tick.
	 */
	void GetProbeDirections(const FVector2D& ViewForward, const FVector2D& IntentDir, TArray<FVector2D, TInlineAllocator<MaxProbeDirections>>& Out);
	/**
	 * Share of the view's yaw turn to the wall applied by Alpha in [0,1] (clamped): smoothstep from 0 at
	 * Alpha 0 to 1 at Alpha = RiseShare, 1 after. The climb adds deltas of this times the total turn.
	 */
	float TurnFraction(float Alpha, float RiseShare);
	/**
	 * Hand over hand on a ladder, from the height climbed so far: X = lean to one side (-1..1, the other way on the
	 * next rung), Y = dip between two rungs (0..1). Both exactly 0 on every rung, and with RungCm <= 0.
	 */
	FVector2D LadderSway(float ClimbedCm, float RungCm);
}
