#include "Combat/GBBuildUpMeter.h"

namespace
{
	constexpr float BuildUpFullTolerance = 1.e-4f;
}

bool FGBBuildUpMeter::Add(float Amount, const FGBBuildUpConfig& Config)
{
	if (Phase != EGBBuildUpPhase::Building)
	{
		return false;
	}
	if (Amount <= 0.f || Config.BuildUpTaken <= 0.f || Config.Threshold <= 0.f)
	{
		return false;
	}
	Value += Amount * Config.BuildUpTaken;
	TimeSinceBuildUp = 0.f;
	if (Value >= Config.Threshold - BuildUpFullTolerance)
	{
		Value = 0.f;
		Phase = EGBBuildUpPhase::Active;
		PhaseTimeRemaining = Config.StatusDuration;
		return true;
	}
	return false;
}

uint8 FGBBuildUpMeter::Advance(float DeltaTime, const FGBBuildUpConfig& Config)
{
	uint8 Events = 0;
	if (DeltaTime <= 0.f)
	{
		return Events;
	}
	if (Phase == EGBBuildUpPhase::Building)
	{
		const float Before = TimeSinceBuildUp;
		TimeSinceBuildUp += DeltaTime;
		if (Value > 0.f)
		{
			const float DecayStart = FMath::Max(Before, Config.DecayDelay);
			const float DecayTime = FMath::Max(0.f, TimeSinceBuildUp - DecayStart);
			if (DecayTime > 0.f && Config.DecayRate > 0.f)
			{
				const float Before21 = Value;
				Value = FMath::Max(0.f, Value - Config.DecayRate * DecayTime);
				if (Value != Before21)
				{
					Events |= static_cast<uint8>(EGBBuildUpEvent::ValueChanged);
				}
			}
		}
	}
	else if (Phase == EGBBuildUpPhase::Active)
	{
		PhaseTimeRemaining -= DeltaTime;
		if (PhaseTimeRemaining <= BuildUpFullTolerance)
		{
			Events |= static_cast<uint8>(EGBBuildUpEvent::StatusEnded);
			if (Config.ImmunityDuration > 0.f)
			{
				Phase = EGBBuildUpPhase::Immune;
				PhaseTimeRemaining = Config.ImmunityDuration + PhaseTimeRemaining;
			}
			else
			{
				Phase = EGBBuildUpPhase::Building;
				PhaseTimeRemaining = 0.f;
			}
		}
	}
	else
	{
		PhaseTimeRemaining -= DeltaTime;
		if (PhaseTimeRemaining <= BuildUpFullTolerance)
		{
			Phase = EGBBuildUpPhase::Building;
			PhaseTimeRemaining = 0.f;
			Events |= static_cast<uint8>(EGBBuildUpEvent::ImmunityEnded);
		}
	}
	return Events;
}

bool FGBBuildUpMeter::IsIdle() const
{
	return Phase == EGBBuildUpPhase::Building && Value <= 0.f;
}

float FGBBuildUpEventLog::Accept(int32 EventId, float Amount)
{
	if (EventId == 0)
	{
		return Amount;
	}
	for (FEntry& Entry : Entries)
	{
		if (Entry.Id == EventId)
		{
			if (Amount > Entry.Amount)
			{
				const float Diff = Amount - Entry.Amount;
				Entry.Amount = Amount;
				return Diff;
			}
			return 0.f;
		}
	}
	// New id: remember the biggest amount of this event; overwrite the oldest slot.
	Entries[NextSlot].Id = EventId;
	Entries[NextSlot].Amount = Amount;
	NextSlot = (NextSlot + 1) % 8;
	return Amount;
}
