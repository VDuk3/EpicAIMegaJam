// Pure elemental build-up logic (GDD 6): one bar per element, no UObjects, unit-tested.
#pragma once

#include "CoreMinimal.h"
#include "GBBuildUpMeter.generated.h"

UENUM(BlueprintType)
enum class EGBBuildUpPhase : uint8
{
	Building,
	Active,
	Immune
};

/** Bit mask returned by FGBBuildUpMeter::Advance. */
enum class EGBBuildUpEvent : uint8
{
	ValueChanged = 1 << 0,
	StatusEnded = 1 << 1,
	ImmunityEnded = 1 << 2
};

USTRUCT(BlueprintType)
struct GLYPHBOUND_API FGBBuildUpConfig
{
	GENERATED_BODY()

	/** <= 0: this bar never fills (element disabled for this enemy). */
	UPROPERTY() float Threshold = 0.f;
	/** Multiplier on incoming build-up (resistance < 1, vulnerability > 1, 0 = immune). */
	UPROPERTY() float BuildUpTaken = 1.f;
	/** Seconds without build-up before the bar drains. */
	UPROPERTY() float DecayDelay = 0.f;
	/** Points per second. */
	UPROPERTY() float DecayRate = 0.f;
	UPROPERTY() float StatusDuration = 0.f;
	UPROPERTY() float ImmunityDuration = 0.f;
};

USTRUCT(BlueprintType)
struct GLYPHBOUND_API FGBBuildUpMeter
{
	GENERATED_BODY()

	UPROPERTY() float Value = 0.f;
	UPROPERTY() EGBBuildUpPhase Phase = EGBBuildUpPhase::Building;
	UPROPERTY() float PhaseTimeRemaining = 0.f;
	UPROPERTY() float TimeSinceBuildUp = 0.f;

	/**
	 * Adds Amount * BuildUpTaken while Building. Returns true when this add filled the bar:
	 * Value goes to 0 and the phase to Active for StatusDuration. Ignored (false) when not Building,
	 * Amount <= 0, BuildUpTaken <= 0 or Threshold <= 0. Resets the decay delay.
	 * Full means Value >= Threshold - 1e-4 (float tolerance, so exactly the threshold counts).
	 */
	bool Add(float Amount, const FGBBuildUpConfig& Config);

	/** Advances time; returns a bit mask of EGBBuildUpEvent (ValueChanged, StatusEnded, ImmunityEnded). */
	uint8 Advance(float DeltaTime, const FGBBuildUpConfig& Config);

	/** Building with an empty bar: nothing to tick. */
	bool IsIdle() const;
};

/**
 * Remembers the last 8 build-up events of one target, so one event (a shotgun shot) counts once:
 * the biggest amount of that event wins.
 */
struct GLYPHBOUND_API FGBBuildUpEventLog
{

	/**
	 * Returns how much of Amount to really add: all of it for a new id (or id 0);
	 * for a known id only the part above what that id already added (0 if smaller).
	 */
	float Accept(int32 EventId, float Amount);

private:
	struct FEntry
	{
		int32 Id = 0;
		float Amount = 0.f;
	};
	FEntry Entries[8] = {};
	int32 NextSlot = 0;
};
