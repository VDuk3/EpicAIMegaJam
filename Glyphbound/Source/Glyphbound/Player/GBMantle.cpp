#include "Player/GBMantle.h"

namespace GBMantle
{
	namespace
	{
		float Smooth01(float Edge0, float Edge1, float X)
		{
			const float T = FMath::Clamp((X - Edge0) / (Edge1 - Edge0), 0.f, 1.f);
			return T * T * (3.f - 2.f * T);
		}
	}

	float JumpRise(float JumpZVelocity, float Gravity)
	{
		if (Gravity <= 0.f)
		{
			return 0.f;
		}
		return JumpZVelocity * JumpZVelocity / (2.f * Gravity);
	}

	bool CanMantleFromGround(float LedgeHeight, float JumpRiseCm, const FMantleParams& Params)
	{
		return LedgeHeight >= Params.MinHeight && LedgeHeight <= JumpRiseCm + Params.MaxReach;
	}

	bool CanMantleFromAir(float LedgeHeight, const FMantleParams& Params)
	{
		return LedgeHeight >= Params.AirMinHeight && LedgeHeight <= Params.MaxReach;
	}

	float Duration(float RiseCm, const FMantleParams& Params)
	{
		return FMath::Max(0.05f, Params.BaseDuration + Params.DurationPerMeter * RiseCm / 100.f);
	}

	FVector EvaluatePath(const FVector& Start, const FVector& End, float Alpha, float RiseShare)
	{
		const float A = FMath::Clamp(Alpha, 0.f, 1.f);
		if (A <= 0.f)
		{
			return Start;
		}
		if (A >= 1.f)
		{
			return End;
		}
		const float Share = FMath::Clamp(RiseShare, 0.f, 1.f);
		const FVector Top(Start.X, Start.Y, End.Z);
		if (Share <= KINDA_SMALL_NUMBER)
		{
			return FMath::Lerp(Start, End, Smooth01(0.f, 1.f, A));
		}
		if (Share >= 1.f - KINDA_SMALL_NUMBER)
		{
			const float E = 1.f - (1.f - A) * (1.f - A);
			return FMath::Lerp(Start, Top, E);
		}
		if (A <= Share)
		{
			const float T = A / Share;
			const float E = 1.f - (1.f - T) * (1.f - T);
			return FMath::Lerp(Start, Top, E);
		}
		return FMath::Lerp(Top, End, Smooth01(Share, 1.f, A));
	}

	FMantlePose EvaluatePose(float Alpha, float RiseShare)
	{
		const float A = FMath::Clamp(Alpha, 0.f, 1.f);
		FMantlePose Pose;
		Pose.ItemLower01 = Smooth01(0.f, 0.2f, A) * (1.f - Smooth01(0.75f, 1.f, A));
		Pose.Grip01 = Smooth01(0.f, 0.15f, A) * (1.f - Smooth01(0.7f, 0.9f, A));
		const float Share = FMath::Clamp(RiseShare, 0.f, 1.f);
		if (Share <= KINDA_SMALL_NUMBER || Share >= 1.f - KINDA_SMALL_NUMBER)
		{
			Pose.CameraDip01 = FMath::Sin(UE_PI * A);
		}
		else if (A <= Share)
		{
			Pose.CameraDip01 = Smooth01(0.f, Share, A);
		}
		else
		{
			Pose.CameraDip01 = 1.f - Smooth01(Share, 1.f, A);
		}
		return Pose;
	}

	void GetProbeDirections(const FVector2D& ViewForward, const FVector2D& IntentDir, TArray<FVector2D, TInlineAllocator<MaxProbeDirections>>& Out)
	{
		Out.Reset();
		if (!IntentDir.IsNearlyZero())
		{
			Out.Add(IntentDir.GetSafeNormal());
			return;
		}
		FVector2D Forward = ViewForward.GetSafeNormal();
		if (Forward.IsNearlyZero())
		{
			Forward = FVector2D(1.f, 0.f);
		}
		// 0, +45, -45, +90, -90, +135, -135, 180 degrees (positive = counterclockwise).
		static constexpr float OffsetsDeg[MaxProbeDirections] = { 0.f, 45.f, -45.f, 90.f, -90.f, 135.f, -135.f, 180.f };
		for (float OffsetDeg : OffsetsDeg)
		{
			const float Radians = FMath::DegreesToRadians(OffsetDeg);
			const float C = FMath::Cos(Radians);
			const float S = FMath::Sin(Radians);
			Out.Add(FVector2D(Forward.X * C - Forward.Y * S, Forward.X * S + Forward.Y * C).GetSafeNormal());
		}
	}

	FVector2D LadderSway(float ClimbedCm, float RungCm)
	{
		if (RungCm <= KINDA_SMALL_NUMBER)
		{
			return FVector2D::ZeroVector;
		}
		const float Lean = FMath::Sin(UE_PI * ClimbedCm / RungCm);
		return FVector2D(Lean, FMath::Abs(Lean));
	}

	bool CanVault(float TopHeight, float DepthCm, float ReachCm, float DropCm, float TopAboveLandingCm, const FVaultParams& Params)
	{
		return TopHeight >= Params.MinHeight && TopHeight <= Params.MaxHeight
			&& DepthCm >= 0.f && DepthCm <= Params.MaxDepth
			&& ReachCm <= Params.Reach
			&& DropCm <= Params.MaxDrop
			&& TopAboveLandingCm >= Params.MinTopAboveLanding;
	}

	float VaultEndFeetZ(float FarFloorZ, float TopZ, float ExitBelowTop)
	{
		return FMath::Max(FarFloorZ, TopZ - ExitBelowTop);
	}

	namespace
	{
		void VaultShares(float RiseShare, float DropStart, float& OutRise, float& OutDrop)
		{
			OutRise = FMath::Clamp(RiseShare, 0.05f, 0.9f);
			OutDrop = FMath::Clamp(DropStart, OutRise + 0.05f, 0.95f);
		}
	}

	FVector EvaluateVaultPath(const FVector& Start, const FVector& End, float ApexZ, float Alpha, float RiseShare, float DropStart)
	{
		const float A = FMath::Clamp(Alpha, 0.f, 1.f);
		if (A <= 0.f)
		{
			return Start;
		}
		if (A >= 1.f)
		{
			return End;
		}
		float Rise = 0.f;
		float Drop = 0.f;
		VaultShares(RiseShare, DropStart, Rise, Drop);
		const float Apex = FMath::Max(ApexZ, FMath::Max(Start.Z, End.Z));
		float Z = Apex;
		if (A <= Rise)
		{
			const float T = A / Rise;
			Z = FMath::Lerp(Start.Z, Apex, 1.f - (1.f - T) * (1.f - T));
		}
		else if (A >= Drop)
		{
			const float T = (A - Drop) / (1.f - Drop);
			Z = FMath::Lerp(Apex, End.Z, T * T);
		}
		const float H = Smooth01(Rise, Drop, A);
		return FVector(FMath::Lerp(Start.X, End.X, H), FMath::Lerp(Start.Y, End.Y, H), Z);
	}

	FVaultPose EvaluateVaultPose(float Alpha, float RiseShare, float DropStart)
	{
		const float A = FMath::Clamp(Alpha, 0.f, 1.f);
		float Rise = 0.f;
		float Drop = 0.f;
		VaultShares(RiseShare, DropStart, Rise, Drop);
		const float Mid = 0.5f * (Rise + Drop);
		FVaultPose Pose;
		Pose.ItemLower01 = Smooth01(0.f, 0.15f, A) * (1.f - Smooth01(0.8f, 1.f, A));
		Pose.Grip01 = Smooth01(0.f, 0.2f, A) * (1.f - Smooth01(Mid, Drop + 0.1f, A));
		Pose.PitchDown01 = Smooth01(0.f, Rise, A) * (1.f - Smooth01(Drop, 1.f, A));
		Pose.Roll01 = Smooth01(Rise * 0.5f, Mid, A) * (1.f - Smooth01(Mid, Drop + (1.f - Drop) * 0.7f, A));
		Pose.Dip01 = Smooth01(Rise * 0.5f, Mid, A) * (1.f - Smooth01(Mid, Drop + (1.f - Drop) * 0.5f, A));
		return Pose;
	}

	int32 VaultSubsteps(float AlphaDelta, float MaxStep)
	{
		if (MaxStep <= KINDA_SMALL_NUMBER)
		{
			return 1;
		}
		return FMath::Max(1, FMath::CeilToInt(FMath::Abs(AlphaDelta) / MaxStep - 1.e-4f));
	}

	float TurnFraction(float Alpha, float RiseShare)
	{
		const float A = FMath::Clamp(Alpha, 0.f, 1.f);
		const float Share = FMath::Clamp(RiseShare, 0.f, 1.f);
		if (A <= 0.f)
		{
			return 0.f;
		}
		if (Share <= KINDA_SMALL_NUMBER)
		{
			return 1.f;
		}
		if (A >= Share)
		{
			return 1.f;
		}
		const float T = A / Share;
		return T * T * (3.f - 2.f * T);
	}
}
