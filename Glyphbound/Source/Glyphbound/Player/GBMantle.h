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

	// ---- Vault: over a thin obstacle (a railing) that is too high to jump and has no top to stand on ----

	/** Tunables from Player.json (cm / seconds; shares are 0..1 fractions of the move's time). */
	struct FVaultParams
	{
		float MinHeight = 50.f;
		float MaxHeight = 150.f;
		float MaxDepth = 60.f;
		float Reach = 220.f;
		float MaxDrop = 450.f;
		float Duration = 0.55f;
		float ExitBelowTop = 130.f;
		/** The rise ends at RiseShare, the drop begins at DropStart; between them the body crosses over the top. */
		float RiseShare = 0.3f;
		float DropStart = 0.62f;
		/** The landing floor must be at least this far below the obstacle's top (else it is a ledge, not an obstacle). */
		float MinTopAboveLanding = 10.f;
	};

	/** View pose weights of the vault, all 0..1 (the roll side and the camera amounts are applied by the player). */
	struct FVaultPose
	{
		float ItemLower01 = 0.f;
		/** The supporting hand on the obstacle's top. */
		float Grip01 = 0.f;
		/** The view tips forward (peaks over the top). */
		float PitchDown01 = 0.f;
		/** The view rolls to one side (peaks over the top). */
		float Roll01 = 0.f;
		/** The camera drops a little as the body goes over. */
		float Dip01 = 0.f;
	};

	/**
	 * Does this obstacle qualify for a vault? TopHeight: its top above the feet. DepthCm: its thickness along the
	 * crossing. ReachCm: horizontal distance from the player to the landing spot. DropCm: landing floor below the
	 * feet (negative = above). TopAboveLandingCm: the obstacle's top above the landing floor.
	 * All limits are inclusive.
	 */
	bool CanVault(float TopHeight, float DepthCm, float ReachCm, float DropCm, float TopAboveLandingCm, const FVaultParams& Params);
	/** Feet height at the end of the move: the far floor, but never lower than ExitBelowTop under the top (then he falls the rest). */
	float VaultEndFeetZ(float FarFloorZ, float TopZ, float ExitBelowTop);
	/**
	 * Actor location along the vault at Alpha (clamped). Z rises from Start to ApexZ by RiseShare (ease-out), holds
	 * over the top, then drops to End by 1 from DropStart (ease-in); XY stays at Start until RiseShare, crosses to End
	 * between RiseShare and DropStart (smoothstep) and stays at End after. Exactly Start at 0 and End at 1.
	 */
	FVector EvaluateVaultPath(const FVector& Start, const FVector& End, float ApexZ, float Alpha, float RiseShare, float DropStart);
	/** View pose weights at Alpha (clamped). All exactly 0 at Alpha 0 and at Alpha 1. */
	FVaultPose EvaluateVaultPose(float Alpha, float RiseShare, float DropStart);
	/** The largest step of Alpha the vault takes between two collision sweeps (a long tick is cut into steps of at most this). */
	constexpr float VaultMaxAlphaStep = 0.03f;
	/** How many equal steps (at least 1) cover AlphaDelta with none larger than MaxStep. A non-positive MaxStep gives 1. */
	int32 VaultSubsteps(float AlphaDelta, float MaxStep);
}

/** Vault stat names (Player.json "Stats"). */
namespace GBVaultStat
{
	inline const FName MinHeight(TEXT("VaultMinHeight"));               // cm: lowest obstacle top above the feet
	inline const FName MaxHeight(TEXT("VaultMaxHeight"));               // cm: highest obstacle top above the feet
	inline const FName MaxDepth(TEXT("VaultMaxDepth"));                 // cm: thickest obstacle along the crossing
	inline const FName Reach(TEXT("VaultReach"));                       // cm: horizontal distance to the landing spot
	inline const FName MaxDrop(TEXT("VaultMaxDrop"));                   // cm: the landing floor may be this far below the feet
	inline const FName PushTime(TEXT("VaultPushTime"));                 // s: holding forward into an obstacle this long vaults it
	inline const FName PushMaxSpeed(TEXT("VaultPushMaxSpeed"));         // cm/s: the push counts while he is slower than this (blocked)
	inline const FName Duration(TEXT("VaultTime"));                     // s
	inline const FName ExitSpeed(TEXT("VaultExitSpeed"));               // cm/s kept forward when control returns
	inline const FName ExitBelowTop(TEXT("VaultExitBelowTop"));         // cm: with a far floor lower than this under the top, he ends here and falls
	inline const FName ApexClearance(TEXT("VaultApexClearance"));       // cm: the feet pass this far over the top
	inline const FName RiseShare(TEXT("VaultRiseShare"));               // share of the time rising beside the obstacle
	inline const FName DropStart(TEXT("VaultDropStart"));               // share of the time when the drop on the far side begins
	inline const FName CameraPitchDeg(TEXT("VaultCameraPitchDeg"));     // degrees the view tips forward over the top
	inline const FName CameraRollDeg(TEXT("VaultCameraRollDeg"));       // degrees the view rolls to the supporting hand's side
	inline const FName CameraDipCm(TEXT("VaultCameraDipCm"));           // cm the camera drops over the top
}
