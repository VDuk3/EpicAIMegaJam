// Pure-logic tests of the level rules built on progress flags.
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "AI/GBEncounter.h"

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

#endif // WITH_DEV_AUTOMATION_TESTS
