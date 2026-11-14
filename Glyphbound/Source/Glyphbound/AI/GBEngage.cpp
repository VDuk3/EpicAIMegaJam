#include "AI/GBEngage.h"

EGBEngageAction GBEngage::Decide(float Distance, bool bHasSight, float RangeMin, float RangeMax, bool bCanRetreat)
{
	if (!bHasSight || Distance > RangeMax)
	{
		return EGBEngageAction::Advance;
	}
	if (Distance < RangeMin && bCanRetreat)
	{
		return EGBEngageAction::Retreat;
	}
	return EGBEngageAction::Hold;
}

bool GBEngage::Notices(float Distance, float AngleToPlayerDegrees, bool bHasSight, float AggroRange, float SightAngleDegrees, bool bProvokedOrAlerted)
{
	if (AggroRange <= 0.f)
	{
		return false;
	}
	if (bProvokedOrAlerted)
	{
		return true;
	}
	return Distance <= AggroRange && bHasSight && FMath::Abs(AngleToPlayerDegrees) <= SightAngleDegrees * 0.5f;
}
