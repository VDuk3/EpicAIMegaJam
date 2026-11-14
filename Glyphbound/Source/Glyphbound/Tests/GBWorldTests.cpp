// Pure-logic tests of the level rules built on progress flags.
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "AI/GBEncounter.h"
#include "World/GBBreakable.h"
#include "World/GBDoor.h"
#include "World/GBEnergyGate.h"
#include "World/GBLadder.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBRequiredFlagsTest, "Glyphbound.World.RequiredFlags",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGBRequiredFlagsTest::RunTest(const FString& Parameters)
{
	const FName A(TEXT("Encounter.Zone_Fire.Cleared"));
	const FName B(TEXT("Encounter.Zone_Ice.Cleared"));
	const FName C(TEXT("Encounter.Zone_Air.Cleared"));
	const TArray<FName> Required = { A, B, C };

	TSet<FName> Flags;
	// An empty list needs nothing (an implementation that returns "any flag set" would fail the empty-flags case).
	TestTrue(TEXT("Empty requirement is met with no flags"), AGBEncounter::AreRequiredFlagsMet(TArray<FName>(), Flags));
	TestFalse(TEXT("Nothing set: not met"), AGBEncounter::AreRequiredFlagsMet(Required, Flags));

	// "Any" instead of "all" would pass these partial sets.
	Flags.Add(A);
	TestFalse(TEXT("One of three set: not met"), AGBEncounter::AreRequiredFlagsMet(Required, Flags));
	Flags.Add(B);
	TestFalse(TEXT("Two of three set: not met"), AGBEncounter::AreRequiredFlagsMet(Required, Flags));

	// Unrelated flags must not count (a check by flag count would pass this).
	Flags.Add(FName(TEXT("Unrelated")));
	TestFalse(TEXT("Three flags set but one of them unrelated: not met"), AGBEncounter::AreRequiredFlagsMet(Required, Flags));

	Flags.Add(C);
	TestTrue(TEXT("All three set: met"), AGBEncounter::AreRequiredFlagsMet(Required, Flags));

	// A flag cleared again must close it (a check that remembers "was met once" would fail here).
	Flags.Remove(B);
	TestFalse(TEXT("A flag cleared again: not met"), AGBEncounter::AreRequiredFlagsMet(Required, Flags));

	TestTrue(TEXT("Empty requirement is met with flags set"), AGBEncounter::AreRequiredFlagsMet(TArray<FName>(), Flags));

	// Each required flag missing on its own (an implementation that ignores the first, middle or last flag fails one of these).
	for (int32 Missing = 0; Missing < Required.Num(); ++Missing)
	{
		TSet<FName> AllButOne(Required);
		AllButOne.Remove(Required[Missing]);
		TestFalse(*FString::Printf(TEXT("Only required flag %d missing: not met"), Missing), AGBEncounter::AreRequiredFlagsMet(Required, AllButOne));
	}
	TestTrue(TEXT("All required flags and nothing else: met"), AGBEncounter::AreRequiredFlagsMet(Required, TSet<FName>(Required)));

	// A single-flag requirement.
	const TArray<FName> Single = { A };
	TestFalse(TEXT("Single requirement, nothing set"), AGBEncounter::AreRequiredFlagsMet(Single, TSet<FName>()));
	TestFalse(TEXT("Single requirement, another flag set"), AGBEncounter::AreRequiredFlagsMet(Single, TSet<FName>({ B })));
	TestTrue(TEXT("Single requirement, set"), AGBEncounter::AreRequiredFlagsMet(Single, TSet<FName>({ A })));
	return true;
}

// A door leaf: shut at 0, fully open at 1, never past either end, and eased (a leaf that moved in a straight line
// would be at a quarter of the way after a quarter of the time; the eased one is still behind).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBDoorLeafTest, "Glyphbound.World.DoorLeaf",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGBDoorLeafTest::RunTest(const FString& Parameters)
{
	const FVector Slide(0.f, 110.f, 300.f);
	TestTrue(TEXT("Shut at 0"), AGBDoor::ComputeLeafOffset(Slide, 0.f).IsNearlyZero());
	TestTrue(TEXT("Fully open at 1"), AGBDoor::ComputeLeafOffset(Slide, 1.f).Equals(Slide, 0.01f));
	TestTrue(TEXT("Never past open"), AGBDoor::ComputeLeafOffset(Slide, 3.f).Equals(Slide, 0.01f));
	TestTrue(TEXT("Never past shut"), AGBDoor::ComputeLeafOffset(Slide, -2.f).IsNearlyZero());
	TestTrue(TEXT("Half way at half time"), AGBDoor::ComputeLeafOffset(Slide, 0.5f).Equals(Slide * 0.5f, 0.01f));
	const float Quarter = static_cast<float>(AGBDoor::ComputeLeafOffset(Slide, 0.25f).Z);
	TestTrue(TEXT("Starts gently: behind a straight line at a quarter"), Quarter > 0.f && Quarter < Slide.Z * 0.25f - 1.f);
	const float ThreeQuarters = static_cast<float>(AGBDoor::ComputeLeafOffset(Slide, 0.75f).Z);
	TestTrue(TEXT("Stops gently: ahead of a straight line at three quarters"), ThreeQuarters > Slide.Z * 0.75f + 1.f && ThreeQuarters < Slide.Z);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBBreakableHitsTest, "Glyphbound.World.BreakableHits",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGBBreakableHitsTest::RunTest(const FString& Parameters)
{
	// A hit counts by being a hit, not by how hard it is; a blast of pellets is one hit.
	TestTrue(TEXT("A weak hit counts"), AGBBreakable::CountsAsHit(1.f, 1.f, 10.f, 0.25f));
	TestTrue(TEXT("A strong hit counts the same"), AGBBreakable::CountsAsHit(500.f, 1.f, 10.f, 0.25f));
	TestFalse(TEXT("A graze under the minimum does not"), AGBBreakable::CountsAsHit(0.5f, 1.f, 10.f, 0.25f));
	TestFalse(TEXT("A second pellet of the same blast does not"), AGBBreakable::CountsAsHit(20.f, 1.f, 0.02f, 0.25f));
	TestTrue(TEXT("The next shot does"), AGBBreakable::CountsAsHit(20.f, 1.f, 0.3f, 0.25f));
	// Cracks spread evenly and stop at full.
	TestEqual(TEXT("Whole before any hit"), AGBBreakable::ComputeCrackStage(0, 4), 0.f);
	TestEqual(TEXT("Half way after two of four"), AGBBreakable::ComputeCrackStage(2, 4), 0.5f);
	TestTrue(TEXT("Each hit shows more"), AGBBreakable::ComputeCrackStage(3, 4) > AGBBreakable::ComputeCrackStage(2, 4));
	TestEqual(TEXT("Never past full"), AGBBreakable::ComputeCrackStage(9, 4), 1.f);
	TestEqual(TEXT("A bad setup shows as broken, not as whole"), AGBBreakable::ComputeCrackStage(0, 0), 1.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBLadderSecondsTest, "Glyphbound.World.LadderSeconds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGBLadderSecondsTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("6 m at 3 m/s plus half a second to step off"), AGBLadder::ComputeClimbSeconds(600.f, 300.f, 0.5f), 2.5f);
	TestEqual(TEXT("Down takes as long as up"), AGBLadder::ComputeClimbSeconds(-600.f, 300.f, 0.5f), AGBLadder::ComputeClimbSeconds(600.f, 300.f, 0.5f));
	TestTrue(TEXT("A longer ladder takes longer"), AGBLadder::ComputeClimbSeconds(900.f, 300.f, 0.5f) > AGBLadder::ComputeClimbSeconds(600.f, 300.f, 0.5f));
	TestTrue(TEXT("A speed of zero does not hang the player on the ladder for ever"), FMath::IsFinite(AGBLadder::ComputeClimbSeconds(600.f, 0.f, 0.5f)));
	TestTrue(TEXT("Usable from its own standing place"), AGBLadder::IsWithinUse(40.f, 3.f, 160.f));
	TestFalse(TEXT("Not from across the room"), AGBLadder::IsWithinUse(240.f, 0.f, 160.f));
	TestFalse(TEXT("The head is not usable from the floor under it"), AGBLadder::IsWithinUse(30.f, -405.f, 160.f));
	TestFalse(TEXT("Nor the foot from the landing above it"), AGBLadder::IsWithinUse(30.f, 405.f, 160.f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBEnergyGateFlagsTest, "Glyphbound.World.EnergyGateFlags",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGBEnergyGateFlagsTest::RunTest(const FString& Parameters)
{
	const TArray<FName> Two = { TEXT("SocketA"), TEXT("SocketB") };
	TestFalse(TEXT("Shut with no socket filled"), AGBEnergyGate::AllFlagsPresent(Two, {}));
	TestFalse(TEXT("Shut with one of two"), AGBEnergyGate::AllFlagsPresent(Two, { TEXT("SocketA") }));
	TestFalse(TEXT("Another flag does not stand in for a missing one"), AGBEnergyGate::AllFlagsPresent(Two, { TEXT("SocketA"), TEXT("Other") }));
	TestTrue(TEXT("Out with both"), AGBEnergyGate::AllFlagsPresent(Two, { TEXT("SocketB"), TEXT("SocketA"), TEXT("Other") }));
	TestFalse(TEXT("A gate with no flags never goes out"), AGBEnergyGate::AllFlagsPresent({}, { TEXT("SocketA") }));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
