// Pure decisions of the shooter AI (unit-tested): what to do about the range band, and whether the player is noticed.
#pragma once

#include "CoreMinimal.h"

enum class EGBEngageAction : uint8
{
	/** Close in on the player (too far, or no line of sight). */
	Advance,
	/** Stay in the band: strafe, shoot. */
	Hold,
	/** Back off (too close and there is room to go back). */
	Retreat
};

namespace GBEngage
{
	/**
	 * Range band decision of a ranged enemy. Without line of sight it always advances (it has nothing to shoot at); farther
	 * than RangeMax it advances; closer than RangeMin it retreats when the navmesh / the floor allows (bCanRetreat), otherwise
	 * it holds; inside the band it holds. RangeMin == RangeMax is a single line: hold exactly on it.
	 */
	GLYPHBOUND_API EGBEngageAction Decide(float Distance, bool bHasSight, float RangeMin, float RangeMax, bool bCanRetreat);

	/**
	 * Notice rule of the first-person-shooter AI: a provoked or alerted enemy always notices; otherwise it needs the player
	 * within AggroRange, inside the sight cone (full angle SightAngleDegrees, AngleToPlayerDegrees = angle between its
	 * facing and the player) and with line of sight. AggroRange <= 0 never notices (a dummy).
	 */
	GLYPHBOUND_API bool Notices(float Distance, float AngleToPlayerDegrees, bool bHasSight, float AggroRange, float SightAngleDegrees, bool bProvokedOrAlerted);
}
