#include "AI/GBSteering.h"

TOptional<int32> GBSteering::ChooseOffset(TConstArrayView<bool> Blocked, EGBSteerMode Mode, int32 PreferredSide)
{
	if (Blocked.Num() == 0 || Blocked.Num() % 2 == 0)
	{
		return {};
	}
	const int32 N = (Blocked.Num() - 1) / 2;
	const int32 Side = PreferredSide < 0 ? -1 : 1;
	auto IsFree = [&Blocked, N](int32 Offset) { return !Blocked[Offset + N]; };

	if (Mode != EGBSteerMode::GoAround && IsFree(0))
	{
		return 0;
	}

	if (Mode == EGBSteerMode::Free)
	{
		// Closest angle to the target wins; the preferred side only breaks ties.
		for (int32 Step = 1; Step <= N; ++Step)
		{
			if (IsFree(Side * Step))
			{
				return Side * Step;
			}
			if (IsFree(-Side * Step))
			{
				return -Side * Step;
			}
		}
	}
	else
	{
		// The whole preferred side first, even at a wider angle than a free direction on the other side.
		for (int32 Step = 1; Step <= N; ++Step)
		{
			if (IsFree(Side * Step))
			{
				return Side * Step;
			}
		}
		for (int32 Step = 1; Step <= N; ++Step)
		{
			if (IsFree(-Side * Step))
			{
				return -Side * Step;
			}
		}
	}

	if (Mode == EGBSteerMode::GoAround && IsFree(0))
	{
		return 0;
	}
	return {};
}
