#include "Data/GBDataTypes.h"

float FGBScaledParam::Resolve(float Power, float Reach, bool bUseQTE) const
{
	if (!bUseQTE)
	{
		return Base;
	}
	switch (ScaledBy)
	{
	case EGBScaleSource::Power:
		return FMath::Lerp(Min, Max, FMath::Clamp(Power, 0.f, 1.f));
	case EGBScaleSource::Reach:
		return FMath::Lerp(Min, Max, FMath::Clamp(Reach, 0.f, 1.f));
	default:
		return Base;
	}
}
