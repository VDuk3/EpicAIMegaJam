// Pure-logic tests of the elemental build-up meters and the event dedupe log.
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Combat/GBBuildUpMeter.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBElementalMeterThresholdTest, "Glyphbound.Elemental.MeterThreshold",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGBElementalMeterThresholdTest::RunTest(const FString& Parameters)
{
	FGBBuildUpConfig Config;
	Config.Threshold = 100.f;
	Config.BuildUpTaken = 1.f;
	Config.DecayDelay = 1000.f;
	Config.DecayRate = 0.f;
	Config.StatusDuration = 2.f;
	Config.ImmunityDuration = 5.f;

	FGBBuildUpMeter Meter;
	TestFalse(TEXT("Below threshold does not trigger"), Meter.Add(30.f, Config));
	TestNearlyEqual(TEXT("Partial value kept"), Meter.Value, 30.f);
	TestFalse(TEXT("Still below threshold"), Meter.Add(69.f, Config));
	TestTrue(TEXT("Phase still Building"), Meter.Phase == EGBBuildUpPhase::Building);
	// 30 + 69 = 99, one more point reaches exactly 100 and must trigger.
	TestTrue(TEXT("Reaching exactly the threshold triggers"), Meter.Add(1.f, Config));
	TestNearlyEqual(TEXT("Value resets on trigger"), Meter.Value, 0.f);
	TestTrue(TEXT("Phase Active after trigger"), Meter.Phase == EGBBuildUpPhase::Active);

	TestFalse(TEXT("Adds during Active are ignored"), Meter.Add(50.f, Config));
	TestNearlyEqual(TEXT("Value stays 0 during Active"), Meter.Value, 0.f);

	uint8 Events = Meter.Advance(2.f, Config);
	TestTrue(TEXT("StatusEnded after StatusDuration"), (Events & static_cast<uint8>(EGBBuildUpEvent::StatusEnded)) != 0);
	TestTrue(TEXT("Phase Immune after status"), Meter.Phase == EGBBuildUpPhase::Immune);

	TestFalse(TEXT("Adds during Immune are ignored"), Meter.Add(200.f, Config));
	Events = Meter.Advance(5.f, Config);
	TestTrue(TEXT("ImmunityEnded after ImmunityDuration"), (Events & static_cast<uint8>(EGBBuildUpEvent::ImmunityEnded)) != 0);
	TestTrue(TEXT("Phase Building again"), Meter.Phase == EGBBuildUpPhase::Building);
	TestTrue(TEXT("Adds count again after immunity"), Meter.Add(100.f, Config));

	// ImmunityDuration 0 goes straight back to Building.
	FGBBuildUpConfig NoImmunity = Config;
	NoImmunity.ImmunityDuration = 0.f;
	FGBBuildUpMeter Meter2;
	TestTrue(TEXT("Triggers"), Meter2.Add(100.f, NoImmunity));
	TestTrue(TEXT("Active"), Meter2.Phase == EGBBuildUpPhase::Active);
	Events = Meter2.Advance(2.f, NoImmunity);
	TestTrue(TEXT("StatusEnded"), (Events & static_cast<uint8>(EGBBuildUpEvent::StatusEnded)) != 0);
	TestTrue(TEXT("Straight back to Building"), Meter2.Phase == EGBBuildUpPhase::Building);

	// One long step (a hitch) past status + immunity ends both at once: the next hit counts.
	FGBBuildUpMeter Meter3;
	TestTrue(TEXT("Triggers before the hitch"), Meter3.Add(100.f, Config));
	Events = Meter3.Advance(7.5f, Config);
	TestTrue(TEXT("Hitch ends the status"), (Events & static_cast<uint8>(EGBBuildUpEvent::StatusEnded)) != 0);
	TestTrue(TEXT("Hitch ends the immunity too"), (Events & static_cast<uint8>(EGBBuildUpEvent::ImmunityEnded)) != 0);
	TestTrue(TEXT("Building right after the hitch"), Meter3.Phase == EGBBuildUpPhase::Building);
	Meter3.Add(10.f, Config);
	TestNearlyEqual(TEXT("Hit after the hitch counts"), Meter3.Value, 10.f);

	// Two configs need different numbers of the same add to trigger.
	FGBBuildUpConfig ConfigA;
	ConfigA.Threshold = 100.f;
	ConfigA.BuildUpTaken = 1.f;
	ConfigA.StatusDuration = 1.f;
	FGBBuildUpConfig ConfigB;
	ConfigB.Threshold = 160.f;
	ConfigB.BuildUpTaken = 0.75f;
	ConfigB.StatusDuration = 1.f;
	FGBBuildUpMeter MeterA;
	FGBBuildUpMeter MeterB;
	int32 AddsA = 0;
	while (!MeterA.Add(40.f, ConfigA))
	{
		++AddsA;
		if (AddsA > 10)
		{
			break;
		}
	}
	++AddsA; // the triggering add
	int32 AddsB = 0;
	while (!MeterB.Add(40.f, ConfigB))
	{
		++AddsB;
		if (AddsB > 10)
		{
			break;
		}
	}
	++AddsB;
	TestTrue(TEXT("Thin bar needs fewer adds"), AddsA < AddsB);

	// BuildUpTaken 0 never fills.
	FGBBuildUpConfig Immune;
	Immune.Threshold = 100.f;
	Immune.BuildUpTaken = 0.f;
	Immune.StatusDuration = 1.f;
	FGBBuildUpMeter MeterImmune;
	TestFalse(TEXT("BuildUpTaken 0 never triggers"), MeterImmune.Add(1000.f, Immune));
	TestNearlyEqual(TEXT("Value stays 0 when immune"), MeterImmune.Value, 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBElementalMeterDecayTest, "Glyphbound.Elemental.MeterDecay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGBElementalMeterDecayTest::RunTest(const FString& Parameters)
{
	FGBBuildUpConfig Config;
	Config.Threshold = 1000.f;
	Config.BuildUpTaken = 1.f;
	Config.DecayDelay = 2.f;
	Config.DecayRate = 20.f;
	Config.StatusDuration = 1.f;

	FGBBuildUpMeter Meter;
	Meter.Add(100.f, Config);
	Meter.Advance(1.f, Config);
	TestNearlyEqual(TEXT("Nothing drains during the delay"), Meter.Value, 100.f, 1.e-3f);
	Meter.Advance(1.5f, Config);
	// TimeSince is 2.5, 0.5 s past the delay: 100 - 20*0.5 = 90.
	TestNearlyEqual(TEXT("Drains at DecayRate after the delay"), Meter.Value, 90.f, 1.e-3f);

	// One Advance of 1.0 s equals ten steps of 0.1 s, crossing the delay boundary.
	FGBBuildUpMeter One;
	FGBBuildUpMeter Ten;
	One.Add(100.f, Config);
	Ten.Add(100.f, Config);
	One.Advance(1.5f, Config);
	Ten.Advance(1.5f, Config);
	One.Advance(1.f, Config);
	for (int32 i = 0; i < 10; ++i)
	{
		Ten.Advance(0.1f, Config);
	}
	TestNearlyEqual(TEXT("1.0 s equals ten 0.1 s steps across the delay"), One.Value, Ten.Value, 1.e-3f);

	// Fully after the delay.
	FGBBuildUpMeter One2;
	FGBBuildUpMeter Ten2;
	One2.Add(100.f, Config);
	Ten2.Add(100.f, Config);
	One2.Advance(3.f, Config);
	Ten2.Advance(3.f, Config);
	One2.Advance(1.f, Config);
	for (int32 i = 0; i < 10; ++i)
	{
		Ten2.Advance(0.1f, Config);
	}
	TestNearlyEqual(TEXT("1.0 s equals ten 0.1 s steps after the delay"), One2.Value, Ten2.Value, 1.e-3f);

	// Never below 0.
	FGBBuildUpMeter Floor;
	Floor.Add(10.f, Config);
	Floor.Advance(100.f, Config);
	TestNearlyEqual(TEXT("Never below 0"), Floor.Value, 0.f, 1.e-3f);

	// An add resets the delay.
	FGBBuildUpMeter Reset;
	Reset.Add(100.f, Config);
	Reset.Advance(1.9f, Config);
	Reset.Add(10.f, Config);
	Reset.Advance(1.f, Config);
	// 1.0 s after the second add is still inside the 2 s delay: 110, no drain.
	TestNearlyEqual(TEXT("An add resets the delay"), Reset.Value, 110.f, 1.e-3f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBElementalEventDedupeTest, "Glyphbound.Elemental.EventDedupe",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGBElementalEventDedupeTest::RunTest(const FString& Parameters)
{
	FGBBuildUpEventLog Log;
	TestNearlyEqual(TEXT("New id adds all"), Log.Accept(5, 20.f), 20.f, 1.e-4f);
	TestNearlyEqual(TEXT("Same id twice adds once"), Log.Accept(5, 20.f), 0.f, 1.e-4f);
	TestNearlyEqual(TEXT("Bigger amount adds only the difference"), Log.Accept(5, 30.f), 10.f, 1.e-4f);
	TestNearlyEqual(TEXT("Smaller amount adds nothing"), Log.Accept(5, 25.f), 0.f, 1.e-4f);
	TestNearlyEqual(TEXT("Id 0 is never deduped"), Log.Accept(0, 10.f), 10.f, 1.e-4f);
	TestNearlyEqual(TEXT("Id 0 adds again"), Log.Accept(0, 10.f), 10.f, 1.e-4f);

	// After 8 other ids the first one is forgotten.
	FGBBuildUpEventLog Ring;
	Ring.Accept(1, 10.f);
	for (int32 Id = 2; Id <= 8; ++Id)
	{
		Ring.Accept(Id, 10.f);
	}
	TestNearlyEqual(TEXT("Still remembered after 7 others"), Ring.Accept(1, 10.f), 0.f, 1.e-4f);
	Ring.Accept(9, 10.f);
	TestNearlyEqual(TEXT("Forgotten after 8 others adds again"), Ring.Accept(1, 10.f), 10.f, 1.e-4f);
	return true;
}

#endif
