// Local obstacle steering for enemies when there is no usable navmesh path: the pure decision (unit-tested).
#pragma once

#include "CoreMinimal.h"

/** How the candidate directions are searched. */
enum class EGBSteerMode : uint8
{
	/** Straight at the target first, then both sides by increasing angle (PreferredSide first at each angle). */
	Free,
	/** Straight first, then every angle on PreferredSide, then the other side: no left-right flipping at a corner. */
	HoldSide,
	/** Committed to going around after being stuck: straight is the last resort; PreferredSide first, then the other side. */
	GoAround
};

namespace GBSteering
{
	/**
	 * Candidate directions are Offset * AngleStep around the direction to the target, Offset in [-N, N], where
	 * Blocked.Num() == 2N + 1 and Blocked[Offset + N] says whether a short capsule sweep along that direction hit
	 * something. The sign of an offset is a side (+ = clockwise seen from above). PreferredSide is +1 or -1
	 * (0 counts as +1). Returns the offset to take, or an unset value when every candidate is blocked (or the
	 * array is empty / even-sized).
	 */
	GLYPHBOUND_API TOptional<int32> ChooseOffset(TConstArrayView<bool> Blocked, EGBSteerMode Mode, int32 PreferredSide);
}
