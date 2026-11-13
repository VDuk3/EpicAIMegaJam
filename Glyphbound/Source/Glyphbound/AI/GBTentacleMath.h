// Pure procedural tentacle math: no UObjects, no world, unit-tested in Tests/GBEntityTests.cpp.
// Points are FVector in the tentacle mesh's component space.
#pragma once

#include "CoreMinimal.h"

namespace GBTentacle
{
	/**
	 * Two-bone-style FABRIK on a chain of joints (GDD 11: the Entity's tentacles).
	 * Joints[0] is the fixed root and every segment keeps its length.
	 * Runs forward/backward passes until the tip is within Tolerance of Target or Iterations run out.
	 * When Target is out of reach the chain ends up straight, pointing from the root at Target.
	 */
	GLYPHBOUND_API void SolveFabrik(TArray<FVector>& Joints, const FVector& Target, int32 Iterations, float Tolerance);

	/**
	 * Semi-implicit damped spring step (the tentacles' inertia lag).
	 * Sub-steps internally so Dt up to 0.1 s stays stable. Returns the new value.
	 */
	GLYPHBOUND_API float SpringStep(float& Value, float& Velocity, float Target, float Stiffness, float Damping, float Dt);

	/**
	 * Travelling wave bend of one bone (degrees): Amplitude * ((BoneIndex + 1) / BoneCount)
	 * * sin(2*pi*(Frequency*Time + Phase) - BoneIndex / WaveLength). Phase is in cycles.
	 * The angle grows towards the tip. Returns 0 for degenerate counts.
	 */
	GLYPHBOUND_API float WaveAngle(float Time, float Frequency, float Phase, float BoneIndex, float WaveLength, float Amplitude, int32 BoneCount);
}
