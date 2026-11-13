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

		// Lengths and the authored segment directions. A direction of a zero-length or coincident pair falls
		// back to the previous valid one (or +X), so coincident joints never collapse a segment.
		TArray<float> Lengths;
		TArray<FVector> Directions;
		Lengths.SetNumUninitialized(Count - 1);
		Directions.SetNumUninitialized(Count - 1);
		float TotalLength = 0.f;
		FVector LastDirection = FVector::XAxisVector;
		for (int32 Index = 0; Index < Count - 1; ++Index)
		{
			const FVector Delta = Joints[Index + 1] - Joints[Index];
			const float Segment = Delta.Size();
			Lengths[Index] = Segment;
			TotalLength += Segment;
			if (Segment > KINDA_SMALL_NUMBER)
			{
				LastDirection = Delta / Segment;
			}
			Directions[Index] = LastDirection;
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

		// Collinear chain with the target on (or next to) its line: FABRIK can only fold it onto itself
		// and never bends. Nudge the middle joints sideways so the passes can find the fold.
		if (Count > 2 && TotalLength > KINDA_SMALL_NUMBER)
		{
			const FVector Axis = (Joints[Count - 1] - Root).GetSafeNormal();
			float MaxOff = 0.f;
			for (int32 Index = 1; Index < Count - 1; ++Index)
			{
				MaxOff = FMath::Max(MaxOff, FVector::CrossProduct(Joints[Index] - Root, Axis).Size());
			}
			const float TargetOff = FVector::CrossProduct(ToTarget, Axis).Size();
			if (!Axis.IsNearlyZero() && MaxOff < TotalLength * 0.01f && TargetOff < TotalLength * 0.01f)
			{
				FVector Perp = FVector::CrossProduct(Axis, FVector::ZAxisVector);
				if (Perp.SizeSquared() < 0.01f)
				{
					Perp = FVector::CrossProduct(Axis, FVector::YAxisVector);
				}
				Perp.Normalize();
				for (int32 Index = 1; Index < Count - 1; ++Index)
				{
					const float Bulge = FMath::Sin(PI * static_cast<float>(Index) / static_cast<float>(Count - 1));
					Joints[Index] += Perp * (Bulge * TotalLength * 0.05f);
				}
			}
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
				const FVector Delta = Joints[Index] - Joints[Index + 1];
				const FVector Direction = Delta.SizeSquared() > KINDA_SMALL_NUMBER ? Delta.GetSafeNormal() : -Directions[Index];
				Joints[Index] = Joints[Index + 1] + Direction * Lengths[Index];
			}
			// Backward: pin the root, walk out to the tip.
			Joints[0] = Root;
			for (int32 Index = 0; Index < Count - 1; ++Index)
			{
				const FVector Delta = Joints[Index + 1] - Joints[Index];
				const FVector Direction = Delta.SizeSquared() > KINDA_SMALL_NUMBER ? Delta.GetSafeNormal() : Directions[Index];
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
