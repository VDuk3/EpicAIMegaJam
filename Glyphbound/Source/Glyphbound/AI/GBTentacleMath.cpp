#include "AI/GBTentacleMath.h"

namespace GBTentacle
{
	void SolveFabrik(TArray<FVector>& Joints, const FVector& Target, int32 Iterations, float Tolerance)
	{
		const int32 Count = Joints.Num();
		if (Count < 2)
		{
			return;
		}
		if (Tolerance < 0.f)
		{
			Tolerance = 0.f;
		}

		TArray<float> Lengths;
		Lengths.SetNumUninitialized(Count - 1);
		float TotalLength = 0.f;
		for (int32 Index = 0; Index < Count - 1; ++Index)
		{
			const float Segment = (Joints[Index + 1] - Joints[Index]).Size();
			Lengths[Index] = Segment;
			TotalLength += Segment;
		}

		const FVector Root = Joints[0];
		const FVector ToTarget = Target - Root;
		if (ToTarget.Size() >= TotalLength && TotalLength > KINDA_SMALL_NUMBER)
		{
			// Out of reach: point every segment straight at the target.
			const FVector Direction = ToTarget.GetSafeNormal();
			for (int32 Index = 1; Index < Count; ++Index)
			{
				Joints[Index] = Joints[Index - 1] + Direction * Lengths[Index - 1];
			}
			return;
		}

		const int32 Passes = FMath::Max(Iterations, 0);
		for (int32 Pass = 0; Pass <= Passes; ++Pass)
		{
			if ((Joints[Count - 1] - Target).Size() <= Tolerance)
			{
				break;
			}
			if (Pass >= Passes)
			{
				break;
			}
			// Forward: pin the tip to the target, walk back to the root.
			Joints[Count - 1] = Target;
			for (int32 Index = Count - 2; Index >= 0; --Index)
			{
				const FVector Direction = (Joints[Index] - Joints[Index + 1]).GetSafeNormal();
				Joints[Index] = Joints[Index + 1] + Direction * Lengths[Index];
			}
			// Backward: pin the root, walk out to the tip.
			Joints[0] = Root;
			for (int32 Index = 0; Index < Count - 1; ++Index)
			{
				const FVector Direction = (Joints[Index + 1] - Joints[Index]).GetSafeNormal();
				Joints[Index + 1] = Joints[Index] + Direction * Lengths[Index];
			}
		}

		// The root never moves, even with zero iterations.
		Joints[0] = Root;
	}

	float SpringStep(float& Value, float& Velocity, float Target, float Stiffness, float Damping, float Dt)
	{
		if (Dt <= 0.f)
		{
			return Value;
		}
		// Semi-implicit Euler at a small fixed sub-step: stable for stiff springs at large frame times.
		const float MaxStep = 1.f / 120.f;
		float Remaining = Dt;
		while (Remaining > KINDA_SMALL_NUMBER)
		{
			const float Step = FMath::Min(Remaining, MaxStep);
			Velocity += (Stiffness * (Target - Value) - Damping * Velocity) * Step;
			Value += Velocity * Step;
			Remaining -= Step;
		}
		return Value;
	}

	float WaveAngle(float Time, float Frequency, float Phase, float BoneIndex, float WaveLength, float Amplitude, int32 BoneCount)
	{
		if (BoneCount <= 0 || Amplitude == 0.f)
		{
			return 0.f;
		}
		const float SafeLength = FMath::Max(WaveLength, KINDA_SMALL_NUMBER);
		const float Growth = (BoneIndex + 1.f) / static_cast<float>(BoneCount);
		return Amplitude * Growth * FMath::Sin(2.f * PI * (Frequency * Time + Phase) - BoneIndex / SafeLength);
	}
}
