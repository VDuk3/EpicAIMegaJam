// Pure-logic tests of the era rules: which actor exists in which era, and when flags hide or reveal it.
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "World/GBEraComponent.h"
#include "World/GBEraSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBEraPresenceRuleTest, "Glyphbound.Era.PresenceRule",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGBEraPresenceRuleTest::RunTest(const FString& Parameters)
{
	// Each case breaks exactly one condition, so dropping any single check from the rule makes one case fail.
	TestFalse(TEXT("Past-only thing is absent in the future"),
		UGBEraComponent::ComputePresent(EGBEraPresence::PastOnly, EGBEra::Future, true, false));
	TestFalse(TEXT("Future-only thing is absent in the past"),
		UGBEraComponent::ComputePresent(EGBEraPresence::FutureOnly, EGBEra::Past, true, false));
	TestFalse(TEXT("Missing required flag hides it even in its era"),
		UGBEraComponent::ComputePresent(EGBEraPresence::FutureOnly, EGBEra::Future, false, false));
	TestFalse(TEXT("Removed-by flag set hides it even in its era"),
		UGBEraComponent::ComputePresent(EGBEraPresence::PastOnly, EGBEra::Past, true, true));
	TestFalse(TEXT("Both-era thing hidden by its removed-by flag"),
		UGBEraComponent::ComputePresent(EGBEraPresence::Both, EGBEra::Future, true, true));
	TestTrue(TEXT("Future-only, flag ok, not removed: present in the future"),
		UGBEraComponent::ComputePresent(EGBEraPresence::FutureOnly, EGBEra::Future, true, false));
	TestTrue(TEXT("Past-only, flag ok, not removed: present in the past"),
		UGBEraComponent::ComputePresent(EGBEraPresence::PastOnly, EGBEra::Past, true, false));
	TestTrue(TEXT("Both-era thing present in the past"),
		UGBEraComponent::ComputePresent(EGBEraPresence::Both, EGBEra::Past, true, false));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBEraIsPresentInTest, "Glyphbound.Era.IsPresentIn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGBEraIsPresentInTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Both in past"), UGBEraSubsystem::IsPresentIn(EGBEraPresence::Both, EGBEra::Past));
	TestTrue(TEXT("Both in future"), UGBEraSubsystem::IsPresentIn(EGBEraPresence::Both, EGBEra::Future));
	TestTrue(TEXT("PastOnly in past"), UGBEraSubsystem::IsPresentIn(EGBEraPresence::PastOnly, EGBEra::Past));
	TestFalse(TEXT("PastOnly in future"), UGBEraSubsystem::IsPresentIn(EGBEraPresence::PastOnly, EGBEra::Future));
	TestFalse(TEXT("FutureOnly in past"), UGBEraSubsystem::IsPresentIn(EGBEraPresence::FutureOnly, EGBEra::Past));
	TestTrue(TEXT("FutureOnly in future"), UGBEraSubsystem::IsPresentIn(EGBEraPresence::FutureOnly, EGBEra::Future));
	TestTrue(TEXT("OtherEra of past"), UGBEraSubsystem::OtherEra(EGBEra::Past) == EGBEra::Future);
	TestTrue(TEXT("OtherEra of future"), UGBEraSubsystem::OtherEra(EGBEra::Future) == EGBEra::Past);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
