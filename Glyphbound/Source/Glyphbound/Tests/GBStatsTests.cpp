// Pure-logic tests of the stat math: modifier order, source scaling, regen steps and QTE-scaled params.
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Core/GBStatNames.h"
#include "Core/GBTypes.h"
#include "Data/GBDataTypes.h"
#include "Stats/GBAttributeComponent.h"
#include "Stats/GBStatsComponent.h"
#include "UObject/Package.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GBStatsTestsPrivate
{
	FGBModifierDef Mod(FName Stat, EGBModOp Op, float Value)
	{
		FGBModifierDef Result;
		Result.Stat = Stat;
		Result.Op = Op;
		Result.Value = Value;
		return Result;
	}

	FGBScaledParam Param(float Base, float Min, float Max, EGBScaleSource ScaledBy)
	{
		FGBScaledParam Result;
		Result.Base = Base;
		Result.Min = Min;
		Result.Max = Max;
		Result.ScaledBy = ScaledBy;
		return Result;
	}

	const FName TestStat(TEXT("Test.Stat"));
	const FName OtherStat(TEXT("Test.Other"));
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBStatsModifierOrderTest, "Glyphbound.Stats.ModifierOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGBStatsModifierOrderTest::RunTest(const FString& Parameters)
{
	using namespace GBStatsTestsPrivate;
	// Adds go before multipliers even when a multiplier comes first: (10 + 5) * 2.
	const FGBModifierDef MulThenAdd[] = { Mod(TestStat, EGBModOp::Multiply, 2.f), Mod(TestStat, EGBModOp::Add, 5.f) };
	TestNearlyEqual(TEXT("(Base + Add) * Mult"), UGBStatsComponent::ApplyModifiers(10.f, MulThenAdd), 30.f);

	// Multipliers multiply each other (x2 and x1.5 = x3), they are not summed as percentages (x2.5).
	const FGBModifierDef TwoMuls[] = { Mod(TestStat, EGBModOp::Multiply, 2.f), Mod(TestStat, EGBModOp::Multiply, 1.5f) };
	TestNearlyEqual(TEXT("Multipliers stack as a product"), UGBStatsComponent::ApplyModifiers(10.f, TwoMuls), 30.f);

	// The last override replaces everything, including adds and multipliers listed after it.
	const FGBModifierDef Overrides[] =
	{
		Mod(TestStat, EGBModOp::Override, 7.f),
		Mod(TestStat, EGBModOp::Add, 5.f),
		Mod(TestStat, EGBModOp::Override, 3.f),
		Mod(TestStat, EGBModOp::Multiply, 2.f)
	};
	TestNearlyEqual(TEXT("Last override wins"), UGBStatsComponent::ApplyModifiers(10.f, Overrides), 3.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBStatsModifierSourcesTest, "Glyphbound.Stats.ModifierSources",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGBStatsModifierSourcesTest::RunTest(const FString& Parameters)
{
	using namespace GBStatsTestsPrivate;
	UGBStatsComponent* Stats = NewObject<UGBStatsComponent>(GetTransientPackage());

	// Half strength: x1.5 becomes x1.25 (scaled around 1), +4 becomes +2.
	const FGBModifierDef Buff[] = { Mod(TestStat, EGBModOp::Multiply, 1.5f), Mod(OtherStat, EGBModOp::Add, 4.f) };
	Stats->AddModifiers(TEXT("Effect.A"), Buff, 0.5f);
	TestNearlyEqual(TEXT("Multiply scaled around 1"), Stats->Evaluate(TestStat, 10.f), 12.5f);
	TestNearlyEqual(TEXT("Add scaled linearly"), Stats->Evaluate(OtherStat, 10.f), 12.f);

	// Same source again replaces, it does not stack.
	Stats->AddModifiers(TEXT("Effect.A"), Buff, 1.f);
	TestNearlyEqual(TEXT("Same source replaces"), Stats->Evaluate(TestStat, 10.f), 15.f);

	// Keys merge in EvaluateKeys; removal by source.
	const FName Keys[] = { TestStat, OtherStat };
	TestNearlyEqual(TEXT("EvaluateKeys merges keys"), Stats->EvaluateKeys(Keys, 10.f), 21.f);
	Stats->RemoveModifiers(TEXT("Effect.A"));
	TestNearlyEqual(TEXT("Removed source no longer applies"), Stats->Evaluate(TestStat, 10.f), 10.f);
	TestFalse(TEXT("Source gone"), Stats->HasModifierSource(TEXT("Effect.A")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBStatsRegenTest, "Glyphbound.Stats.Regen",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGBStatsRegenTest::RunTest(const FString& Parameters)
{
	using namespace GBStatsTestsPrivate;
	// Args: Current, Max, RegenPerSecond, RegenDelay, TimeSinceLastLoss, DeltaTime.
	TestNearlyEqual(TEXT("No regen before the delay"), UGBAttributeComponent::StepRegen(50.f, 100.f, 10.f, 2.f, 1.f, 0.5f), 50.f);
	TestNearlyEqual(TEXT("Only the time past the delay counts"), UGBAttributeComponent::StepRegen(50.f, 100.f, 10.f, 2.f, 1.9f, 0.2f), 51.f);
	TestNearlyEqual(TEXT("Regen after the delay"), UGBAttributeComponent::StepRegen(50.f, 100.f, 10.f, 2.f, 5.f, 0.5f), 55.f);
	TestNearlyEqual(TEXT("Clamped at max"), UGBAttributeComponent::StepRegen(98.f, 100.f, 10.f, 0.f, 5.f, 1.f), 100.f);
	TestNearlyEqual(TEXT("No regen at rate 0"), UGBAttributeComponent::StepRegen(50.f, 100.f, 0.f, 0.f, 5.f, 1.f), 50.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBStatsScaledParamTest, "Glyphbound.Stats.ScaledParam",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGBStatsScaledParamTest::RunTest(const FString& Parameters)
{
	using namespace GBStatsTestsPrivate;
	const FGBScaledParam ByPower = Param(10.f, 4.f, 20.f, EGBScaleSource::Power);
	TestNearlyEqual(TEXT("QTE off returns Base"), ByPower.Resolve(1.f, 1.f, false), 10.f);
	TestNearlyEqual(TEXT("Power-tagged uses Power, ignores Reach"), ByPower.Resolve(0.5f, 0.f, true), 12.f);
	TestNearlyEqual(TEXT("Power above 1 clamps to Max"), ByPower.Resolve(2.f, 0.f, true), 20.f);
	TestNearlyEqual(TEXT("Power below 0 clamps to Min"), ByPower.Resolve(-1.f, 1.f, true), 4.f);

	const FGBScaledParam ByReach = Param(10.f, 4.f, 20.f, EGBScaleSource::Reach);
	TestNearlyEqual(TEXT("Reach-tagged uses Reach"), ByReach.Resolve(1.f, 0.25f, true), 8.f);

	const FGBScaledParam Fixed = Param(10.f, 4.f, 20.f, EGBScaleSource::None);
	TestNearlyEqual(TEXT("Untagged stays at Base with the QTE on"), Fixed.Resolve(1.f, 1.f, true), 10.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBStatsWeaponSlotKeyTest, "Glyphbound.Stats.WeaponSlotKeys",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGBStatsWeaponSlotKeyTest::RunTest(const FString& Parameters)
{
	// The slot-wide modifier key names one slot only: a gun enchant must not buff staffs or swords.
	TestEqual(TEXT("Staff slot key"), GBStat::WeaponSlotKey(EGBHeldSlot::Staff, GBStat::Damage).ToString(), TEXT("Weapon.Staff.Damage"));
	TestEqual(TEXT("Gun slot key"), GBStat::WeaponSlotKey(EGBHeldSlot::Gun, GBStat::Damage).ToString(), TEXT("Weapon.Gun.Damage"));
	TestEqual(TEXT("Sword slot key"), GBStat::WeaponSlotKey(EGBHeldSlot::Sword, GBStat::Damage).ToString(), TEXT("Weapon.Sword.Damage"));
	TestEqual(TEXT("Slot key keeps the stat name"), GBStat::WeaponSlotKey(EGBHeldSlot::Gun, GBStat::FireInterval).ToString(), TEXT("Weapon.Gun.FireInterval"));
	TestFalse(TEXT("Gun key differs from the all-weapons key"),
		GBStat::WeaponSlotKey(EGBHeldSlot::Gun, GBStat::Damage) == GBStat::WeaponAllKey(GBStat::Damage));
	return true;
}

#endif
