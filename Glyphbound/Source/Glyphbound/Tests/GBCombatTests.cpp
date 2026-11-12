// Pure combat math: distance falloff, headshot line, team hostility.
#include "Combat/GBCombatLibrary.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBCombatDistanceFalloffTest, "Glyphbound.Combat.DistanceFalloff",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGBCombatDistanceFalloffTest::RunTest(const FString& Parameters)
{
	// Linear ramp: 1.5 up to 1000 cm, 0.5 from 3000 cm on.
	TestEqual(TEXT("Point blank uses NearMult"), UGBCombatLibrary::DistanceFalloffMultiplier(0.f, 1000.f, 3000.f, 1.5f, 0.5f), 1.5f);
	TestEqual(TEXT("At FalloffStart uses NearMult"), UGBCombatLibrary::DistanceFalloffMultiplier(1000.f, 1000.f, 3000.f, 1.5f, 0.5f), 1.5f);
	TestEqual(TEXT("Midpoint is halfway"), UGBCombatLibrary::DistanceFalloffMultiplier(2000.f, 1000.f, 3000.f, 1.5f, 0.5f), 1.0f, 1.e-4f);
	TestEqual(TEXT("Quarter way is linear"), UGBCombatLibrary::DistanceFalloffMultiplier(1500.f, 1000.f, 3000.f, 1.5f, 0.5f), 1.25f, 1.e-4f);
	TestEqual(TEXT("At FalloffEnd uses FarMult"), UGBCombatLibrary::DistanceFalloffMultiplier(3000.f, 1000.f, 3000.f, 1.5f, 0.5f), 0.5f, 1.e-4f);
	TestEqual(TEXT("Far away uses FarMult"), UGBCombatLibrary::DistanceFalloffMultiplier(9000.f, 1000.f, 3000.f, 1.5f, 0.5f), 0.5f);

	// Rising curve (rifle): the ramp must also work when FarMult > NearMult.
	TestEqual(TEXT("Rising ramp midpoint"), UGBCombatLibrary::DistanceFalloffMultiplier(2000.f, 1000.f, 3000.f, 0.5f, 1.5f), 1.0f, 1.e-4f);

	// FalloffEnd == FalloffStart: hard step at FalloffStart.
	TestEqual(TEXT("Equal ends: just before the step"), UGBCombatLibrary::DistanceFalloffMultiplier(999.f, 1000.f, 1000.f, 2.f, 1.f), 2.f);
	TestEqual(TEXT("Equal ends: just after the step"), UGBCombatLibrary::DistanceFalloffMultiplier(1001.f, 1000.f, 1000.f, 2.f, 1.f), 1.f);

	// FalloffEnd < FalloffStart: still a hard step at FalloffStart (not at FalloffEnd).
	TestEqual(TEXT("Inverted ends: between End and Start is near"), UGBCombatLibrary::DistanceFalloffMultiplier(750.f, 1000.f, 500.f, 2.f, 1.f), 2.f);
	TestEqual(TEXT("Inverted ends: past Start is far"), UGBCombatLibrary::DistanceFalloffMultiplier(1500.f, 1000.f, 500.f, 2.f, 1.f), 1.f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBCombatHeadshotTest, "Glyphbound.Combat.Headshot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGBCombatHeadshotTest::RunTest(const FString& Parameters)
{
	// Body from Z=100 to Z=300, head = top 25% -> head line at Z=250.
	TestTrue(TEXT("Just above the head line"), UGBCombatLibrary::IsHeadshot(251.f, 100.f, 200.f, 0.25f));
	TestFalse(TEXT("Just below the head line"), UGBCombatLibrary::IsHeadshot(249.f, 100.f, 200.f, 0.25f));
	TestFalse(TEXT("Chest is not the head"), UGBCombatLibrary::IsHeadshot(180.f, 100.f, 200.f, 0.25f));

	// Bigger head fraction moves the line down: top 50% -> line at Z=200.
	TestTrue(TEXT("HeadFraction 0.5: above 200"), UGBCombatLibrary::IsHeadshot(201.f, 100.f, 200.f, 0.5f));
	TestFalse(TEXT("HeadFraction 0.5: below 200"), UGBCombatLibrary::IsHeadshot(199.f, 100.f, 200.f, 0.5f));

	// HeadFraction 0: never a headshot, not even at the very top.
	TestFalse(TEXT("HeadFraction 0 at the top"), UGBCombatLibrary::IsHeadshot(300.f, 100.f, 200.f, 0.f));
	TestFalse(TEXT("HeadFraction 0 above the top"), UGBCombatLibrary::IsHeadshot(310.f, 100.f, 200.f, 0.f));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBCombatTeamHostilityTest, "Glyphbound.Combat.TeamHostility",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGBCombatTeamHostilityTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Player vs Enemy"), UGBCombatLibrary::AreTeamsHostile(EGBTeam::Player, EGBTeam::Enemy));
	TestTrue(TEXT("Enemy vs Player"), UGBCombatLibrary::AreTeamsHostile(EGBTeam::Enemy, EGBTeam::Player));

	TestFalse(TEXT("Neutral vs Player"), UGBCombatLibrary::AreTeamsHostile(EGBTeam::Neutral, EGBTeam::Player));
	TestFalse(TEXT("Player vs Neutral"), UGBCombatLibrary::AreTeamsHostile(EGBTeam::Player, EGBTeam::Neutral));
	TestFalse(TEXT("Neutral vs Enemy"), UGBCombatLibrary::AreTeamsHostile(EGBTeam::Neutral, EGBTeam::Enemy));
	TestFalse(TEXT("Enemy vs Neutral"), UGBCombatLibrary::AreTeamsHostile(EGBTeam::Enemy, EGBTeam::Neutral));
	TestFalse(TEXT("Neutral vs Neutral"), UGBCombatLibrary::AreTeamsHostile(EGBTeam::Neutral, EGBTeam::Neutral));

	TestFalse(TEXT("Player vs Player"), UGBCombatLibrary::AreTeamsHostile(EGBTeam::Player, EGBTeam::Player));
	TestFalse(TEXT("Enemy vs Enemy"), UGBCombatLibrary::AreTeamsHostile(EGBTeam::Enemy, EGBTeam::Enemy));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
