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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBCombatMovementSpreadTest, "Glyphbound.Combat.MovementSpread",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGBCombatMovementSpreadTest::RunTest(const FString& Parameters)
{
	// Base 1.2, +2.5 at full speed 600.
	TestEqual(TEXT("Standing still: base only"), UGBCombatLibrary::ComputeMovementSpread(1.2f, 2.5f, 0.f, 600.f), 1.2f, 0.001f);
	TestEqual(TEXT("Half speed: half the bonus"), UGBCombatLibrary::ComputeMovementSpread(1.2f, 2.5f, 300.f, 600.f), 2.45f, 0.001f);
	TestEqual(TEXT("Full speed: whole bonus"), UGBCombatLibrary::ComputeMovementSpread(1.2f, 2.5f, 600.f, 600.f), 3.7f, 0.001f);
	TestEqual(TEXT("Faster than full (falling, knockback): capped"), UGBCombatLibrary::ComputeMovementSpread(1.2f, 2.5f, 1500.f, 600.f), 3.7f, 0.001f);
	TestEqual(TEXT("No full speed (rooted): base only"), UGBCombatLibrary::ComputeMovementSpread(1.2f, 2.5f, 300.f, 0.f), 1.2f, 0.001f);
	TestEqual(TEXT("No move bonus: base only"), UGBCombatLibrary::ComputeMovementSpread(1.2f, 0.f, 600.f, 600.f), 1.2f, 0.001f);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBCombatMeleeAimPointTest, "Glyphbound.Combat.MeleeAimPoint",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGBCombatMeleeAimPointTest::RunTest(const FString& Parameters)
{
	const FVector BodyBottom(0.f, 0.f, 0.f);
	const FVector BodyTop(0.f, 0.f, 180.f);

	auto CheckNear = [&](const TCHAR* What, double Actual, double Expected, double Tolerance)
	{
		TestTrue(What, FMath::Abs(Actual - Expected) <= Tolerance);
	};

	// Aiming level at head height returns a point at head height.
	{
		const FVector Result = UGBCombatLibrary::ClosestBodyPointToAim(
			FVector(200.f, 0.f, 150.f), FVector(-1.f, 0.f, 0.f), 1000.f, BodyBottom, BodyTop);
		CheckNear(TEXT("Level aim at the head: X on the body"), Result.X, 0.0, 1.e-2);
		CheckNear(TEXT("Level aim at the head: Z at aim height"), Result.Z, 150.0, 1.e-2);
	}

	// Aiming down at the feet returns a point near the bottom.
	{
		const FVector AimOrigin(200.f, 0.f, 150.f);
		const FVector AimDir = (FVector(0.f, 0.f, 0.f) - AimOrigin).GetSafeNormal();
		const FVector Result = UGBCombatLibrary::ClosestBodyPointToAim(AimOrigin, AimDir, 1000.f, BodyBottom, BodyTop);
		TestTrue(TEXT("Downward aim reaches the feet"), Result.Z < 20.0);
		CheckNear(TEXT("Downward aim stays on the body axis X"), Result.X, 0.0, 1.e-2);
	}

	// Aiming far above the head returns the top (clamped), not the aim height.
	{
		const FVector Result = UGBCombatLibrary::ClosestBodyPointToAim(
			FVector(200.f, 0.f, 400.f), FVector(-1.f, 0.f, 0.f), 1000.f, BodyBottom, BodyTop);
		CheckNear(TEXT("High aim clamps to the top"), Result.Z, 180.0, 1.e-2);
		// A wrong implementation returning the aim height (400) or the center (90) fails here.
		TestTrue(TEXT("High aim is above the center"), Result.Z > 100.0);
	}

	// An aim that passes beside the body still returns the height where it passes closest.
	{
		const FVector Result = UGBCombatLibrary::ClosestBodyPointToAim(
			FVector(200.f, 50.f, 100.f), FVector(-1.f, 0.f, 0.f), 1000.f, BodyBottom, BodyTop);
		CheckNear(TEXT("Side pass keeps the pass height"), Result.Z, 100.0, 1.e-2);
		CheckNear(TEXT("Side pass is on the body axis"), Result.X, 0.0, 1.e-2);
		CheckNear(TEXT("Side pass is on the body axis Y"), Result.Y, 0.0, 1.e-2);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBCombatPrecisionRecoveryTest, "Glyphbound.Combat.PrecisionRecovery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGBCombatPrecisionRecoveryTest::RunTest(const FString& Parameters)
{
	// Cooldown = max(1, 7 - 1 * rank): 6 / 5 / 4 for ranks 1 / 2 / 3.
	TestEqual(TEXT("Rank 1 cooldown is 6 s"), UGBCombatLibrary::ComputePrecisionRecoveryCooldown(1, 7.f, 1.f, 1.f), 6.f, 1.e-4f);
	TestEqual(TEXT("Rank 2 cooldown is 5 s"), UGBCombatLibrary::ComputePrecisionRecoveryCooldown(2, 7.f, 1.f, 1.f), 5.f, 1.e-4f);
	TestEqual(TEXT("Rank 3 cooldown is 4 s"), UGBCombatLibrary::ComputePrecisionRecoveryCooldown(3, 7.f, 1.f, 1.f), 4.f, 1.e-4f);
	TestEqual(TEXT("Rank 0 is off"), UGBCombatLibrary::ComputePrecisionRecoveryCooldown(0, 7.f, 1.f, 1.f), 0.f);
	TestEqual(TEXT("Negative rank is off"), UGBCombatLibrary::ComputePrecisionRecoveryCooldown(-2, 7.f, 1.f, 1.f), 0.f);
	TestEqual(TEXT("Cooldown never goes below the minimum"), UGBCombatLibrary::ComputePrecisionRecoveryCooldown(9, 7.f, 1.f, 1.f), 1.f);

	// A weak point killing blow by the player with the perk and the cooldown over heals.
	TestTrue(TEXT("Weak point kill by the player after the cooldown"), UGBCombatLibrary::ShouldPrecisionRecover(true, true, 1, 6.f, 6.f));
	TestTrue(TEXT("Weak point kill by the player past the cooldown"), UGBCombatLibrary::ShouldPrecisionRecover(true, true, 2, 12.f, 5.f));

	// Each wrong condition blocks it on its own.
	TestFalse(TEXT("Rank 0 never heals"), UGBCombatLibrary::ShouldPrecisionRecover(true, true, 0, 1000.f, 0.f));
	TestFalse(TEXT("Body-shot kill never heals"), UGBCombatLibrary::ShouldPrecisionRecover(false, true, 3, 1000.f, 4.f));
	TestFalse(TEXT("Another instigator never heals"), UGBCombatLibrary::ShouldPrecisionRecover(true, false, 3, 1000.f, 4.f));
	TestFalse(TEXT("Inside the cooldown no heal"), UGBCombatLibrary::ShouldPrecisionRecover(true, true, 1, 5.99f, 6.f));
	TestTrue(TEXT("Exactly at the cooldown heals"), UGBCombatLibrary::ShouldPrecisionRecover(true, true, 3, 4.f, 4.f));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
