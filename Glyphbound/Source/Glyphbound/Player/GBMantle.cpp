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
}
