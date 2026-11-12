// Pure-logic tests of spell cast resolution (no world, no data files).
#include "Misc/AutomationTest.h"
#include "Core/GBStatNames.h"
#include "Spells/GBSpell_Beam.h"
#include "Spells/GBSpellActor.h"
#include "Spells/GBSpellTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GBSpellTestUtil
{
	FGBScaledParam MakeParam(float Base, float Min, float Max, EGBScaleSource ScaledBy)
	{
		FGBScaledParam Param;
		Param.Base = Base;
		Param.Min = Min;
		Param.Max = Max;
		Param.ScaledBy = ScaledBy;
		return Param;
	}

	/** Base values are deliberately away from both Min and Max, so "used Base" and "used a lerp" cannot be confused. */
	FGBSpellDefinition MakeDefinition()
	{
		FGBSpellDefinition Definition;
		Definition.Id = TEXT("TestBolt");
		Definition.Element = EGBElement::Lightning;
		Definition.Params.Add(GBStat::Damage, MakeParam(37.f, 0.f, 100.f, EGBScaleSource::Power));
		Definition.Params.Add(GBStat::Radius, MakeParam(55.f, 20.f, 220.f, EGBScaleSource::Reach));
		Definition.Params.Add(GBStat::Cooldown, MakeParam(3.f, 1.f, 9.f, EGBScaleSource::None));
		Definition.Params.Add(GBStat::EffectDuration, MakeParam(1.5f, 0.5f, 2.5f, EGBScaleSource::Power));
		Definition.Strings.Add(GBString::OnHitEffect, TEXT("Shock"));
		return Definition;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBSpellResolveScalingTest, "Glyphbound.Spells.ResolveScaling",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGBSpellResolveScalingTest::RunTest(const FString& Parameters)
{
	const FGBSpellDefinition Definition = GBSpellTestUtil::MakeDefinition();

	const FGBSpellCast A = GBSpell::ResolveCast(Definition, 0.25f, 0.75f, true, nullptr);
	TestEqual(TEXT("Power-tagged param follows Power"), A.Get(GBStat::Damage), 25.f, 0.001f);
	TestEqual(TEXT("Reach-tagged param follows Reach"), A.Get(GBStat::Radius), 170.f, 0.001f);
	TestEqual(TEXT("Untagged param stays at Base with the QTE on"), A.Get(GBStat::Cooldown), 3.f, 0.001f);

	// Same Power, different Reach: the Power param must not move, the Reach param must.
	const FGBSpellCast B = GBSpell::ResolveCast(Definition, 0.25f, 0.f, true, nullptr);
	TestEqual(TEXT("Power-tagged param ignores Reach"), B.Get(GBStat::Damage), 25.f, 0.001f);
	TestEqual(TEXT("Reach-tagged param at Reach 0 is Min"), B.Get(GBStat::Radius), 20.f, 0.001f);

	// Same Reach, different Power.
	const FGBSpellCast C = GBSpell::ResolveCast(Definition, 1.f, 0.75f, true, nullptr);
	TestEqual(TEXT("Power-tagged param at Power 1 is Max"), C.Get(GBStat::Damage), 100.f, 0.001f);
	TestEqual(TEXT("Reach-tagged param ignores Power"), C.Get(GBStat::Radius), 170.f, 0.001f);

	TestEqual(TEXT("ResolveParam agrees with ResolveCast"),
		GBSpell::ResolveParam(Definition, GBStat::Radius, 0.25f, 0.75f, true, nullptr), 170.f, 0.001f);
	TestEqual(TEXT("ResolveParam returns the default for a missing param"),
		GBSpell::ResolveParam(Definition, GBStat::Speed, 0.5f, 0.5f, true, nullptr, 123.f), 123.f, 0.001f);

	TestEqual(TEXT("Cast keeps the spell id"), A.SpellId, FName(TEXT("TestBolt")));
	TestTrue(TEXT("Cast records that the QTE was used"), A.bUsedQTE);
	TestEqual(TEXT("Cast keeps the strings"), A.GetString(GBString::OnHitEffect), FString(TEXT("Shock")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBSpellResolveNoQTETest, "Glyphbound.Spells.ResolveNoQTE",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGBSpellResolveNoQTETest::RunTest(const FString& Parameters)
{
	const FGBSpellDefinition Definition = GBSpellTestUtil::MakeDefinition();

	// Perfect scores must not matter when the QTE is off: every param is Base.
	const FGBSpellCast SpellCast = GBSpell::ResolveCast(Definition, 1.f, 1.f, false, nullptr);
	TestEqual(TEXT("Every param is resolved"), SpellCast.Values.Num(), Definition.Params.Num());
	for (const TPair<FName, FGBScaledParam>& Pair : Definition.Params)
	{
		TestEqual(*FString::Printf(TEXT("%s equals Base"), *Pair.Key.ToString()), SpellCast.Get(Pair.Key, -1.f), Pair.Value.Base, 0.001f);
	}
	TestFalse(TEXT("Cast records that the QTE was not used"), SpellCast.bUsedQTE);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBSpellDamageInfoTest, "Glyphbound.Spells.DamageInfo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGBSpellDamageInfoTest::RunTest(const FString& Parameters)
{
	const FGBSpellDefinition Definition = GBSpellTestUtil::MakeDefinition();
	const FGBSpellCast SpellCast = GBSpell::ResolveCast(Definition, 0.25f, 0.75f, true, nullptr);

	const FGBDamageInfo Info = SpellCast.MakeDamageInfo(GBStat::Damage);
	TestEqual(TEXT("Amount is the resolved param"), Info.Amount, 25.f, 0.001f);
	TestTrue(TEXT("Element is the spell's"), Info.Element == EGBElement::Lightning);
	TestEqual(TEXT("SourceId is the spell id"), Info.SourceId, FName(TEXT("TestBolt")));
	TestEqual(TEXT("On-hit effect comes from Strings"), Info.OnHitEffect, FName(TEXT("Shock")));
	TestEqual(TEXT("Effect duration scale is the resolved EffectDuration"), Info.EffectDurationScale, 1.f, 0.001f);
	TestEqual(TEXT("Missing EffectStrength defaults to 1"), Info.EffectStrengthScale, 1.f, 0.001f);

	const FGBDamageInfo Other = SpellCast.MakeDamageInfo(GBStat::Radius);
	TestEqual(TEXT("Amount follows the asked param"), Other.Amount, 170.f, 0.001f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBSpellGroundFigureLayeringTest, "Glyphbound.Spells.GroundFigureLayering",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGBSpellGroundFigureLayeringTest::RunTest(const FString& Parameters)
{
	const float Lift = 3.f;
	const float Step = 1.5f;
	const float MaxRise = 60.f;
	const FVector Ground(100.f, 200.f, 50.f);
	const float Radius = 150.f;

	// No existing figures: ground + lift.
	TestEqual(TEXT("Empty world puts the figure at ground plus lift"),
		AGBSpellActor::ComputeGroundFigureZ(Ground, Radius, {}, Lift, Step, MaxRise), 53.f, 0.001f);

	// One overlapping figure at ground + lift: exactly one step above it.
	const TArray<FVector4> One = { FVector4(100.f, 200.f, 53.f, 150.f) };
	TestEqual(TEXT("One overlapping figure lifts the new one by a single step"),
		AGBSpellActor::ComputeGroundFigureZ(Ground, Radius, One, Lift, Step, MaxRise), 54.5f, 0.001f);

	// Two overlapping figures stacked: above the higher one.
	const TArray<FVector4> Two = { FVector4(100.f, 200.f, 53.f, 150.f), FVector4(100.f, 200.f, 54.5f, 300.f) };
	TestEqual(TEXT("Two stacked figures put the new one above the higher figure"),
		AGBSpellActor::ComputeGroundFigureZ(Ground, Radius, Two, Lift, Step, MaxRise), 56.f, 0.001f);

	// A figure far away (beyond the summed radii) is ignored.
	const TArray<FVector4> Far = { FVector4(1000.f, 200.f, 53.f, 150.f) };
	TestEqual(TEXT("A distant figure does not lift the new one"),
		AGBSpellActor::ComputeGroundFigureZ(Ground, Radius, Far, Lift, Step, MaxRise), 53.f, 0.001f);

	// A figure on a floor far above (more than MaxRise) is ignored.
	const TArray<FVector4> HighFloor = { FVector4(100.f, 200.f, 53.f + MaxRise + 10.f, 150.f) };
	TestEqual(TEXT("A figure more than MaxRise above the ground point is ignored"),
		AGBSpellActor::ComputeGroundFigureZ(Ground, Radius, HighFloor, Lift, Step, MaxRise), 53.f, 0.001f);

	// The newest is always strictly above everything it overlaps.
	const TArray<FVector4> Mixed = {
		FVector4(120.f, 210.f, 53.f, 100.f),
		FVector4(90.f, 190.f, 58.f, 200.f),
		FVector4(5000.f, 5000.f, 200.f, 500.f),
		FVector4(110.f, 205.f, 53.f + MaxRise + 5.f, 400.f)
	};
	const float Layered = AGBSpellActor::ComputeGroundFigureZ(Ground, Radius, Mixed, Lift, Step, MaxRise);
	TestTrue(TEXT("The layered figure ends strictly above each overlapping figure"), Layered > 58.f);
	TestEqual(TEXT("The layered figure sits one step above the highest overlap"), Layered, 59.5f, 0.001f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBSpellParamCapTest, "Glyphbound.Spells.ParamCap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGBSpellParamCapTest::RunTest(const FString& Parameters)
{
	FGBSpellDefinition Definition = GBSpellTestUtil::MakeDefinition();
	FGBScaledParam Capped = GBSpellTestUtil::MakeParam(10.f, 0.f, 100.f, EGBScaleSource::Power);
	Capped.Cap = 5.f;
	Definition.Params.Add(TEXT("CappedParam"), Capped);

	// Power 1 lerps to Max (100), which is above the cap: the resolved value is capped.
	const FGBSpellCast CappedCast = GBSpell::ResolveCast(Definition, 1.f, 0.f, true, nullptr);
	TestEqual(TEXT("A value above the cap is clamped"), CappedCast.Get(TEXT("CappedParam")), 5.f, 0.001f);
	TestEqual(TEXT("ResolveParam agrees with ResolveCast on the cap"),
		GBSpell::ResolveParam(Definition, TEXT("CappedParam"), 1.f, 0.f, true, nullptr), 5.f, 0.001f);

	// Power 0 lerps to Min (0), below the cap: unchanged.
	const FGBSpellCast BelowCast = GBSpell::ResolveCast(Definition, 0.f, 0.f, true, nullptr);
	TestEqual(TEXT("A value below the cap is unchanged"), BelowCast.Get(TEXT("CappedParam")), 0.f, 0.001f);

	// Cap 0 means no cap: a large value passes through.
	FGBScaledParam Uncapped = GBSpellTestUtil::MakeParam(10.f, 0.f, 100.f, EGBScaleSource::Power);
	Uncapped.Cap = 0.f;
	Definition.Params.Add(TEXT("UncappedParam"), Uncapped);
	const FGBSpellCast UncappedCast = GBSpell::ResolveCast(Definition, 1.f, 0.f, true, nullptr);
	TestEqual(TEXT("Cap 0 means no cap"), UncappedCast.Get(TEXT("UncappedParam")), 100.f, 0.001f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBSpellBeamHoldTest, "Glyphbound.Spells.BeamHold",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGBSpellBeamHoldTest::RunTest(const FString& Parameters)
{
	// Hold mode, key held, no time limit: keeps going.
	TestTrue(TEXT("Held keeps going"),
		AGBSpell_Beam::ShouldBeamContinue(true, true, 5.f, 0.75f, 0.f));
	// Hold mode, key released after the minimum: stops.
	TestFalse(TEXT("Released after the minimum stops"),
		AGBSpell_Beam::ShouldBeamContinue(true, false, 1.f, 0.75f, 0.f));
	// Hold mode, key released before the minimum: keeps going.
	TestTrue(TEXT("Released before the minimum keeps going"),
		AGBSpell_Beam::ShouldBeamContinue(true, false, 0.5f, 0.75f, 0.f));
	// The time limit stops the beam even while the key is held.
	TestFalse(TEXT("Max duration stops the beam even if held"),
		AGBSpell_Beam::ShouldBeamContinue(true, true, 10.f, 0.75f, 4.f));
	// Hold mode off: the key is ignored (only the time limit ends it).
	TestTrue(TEXT("Hold mode off ignores the key"),
		AGBSpell_Beam::ShouldBeamContinue(false, false, 5.f, 0.f, 0.f));
	TestFalse(TEXT("Hold mode off still ends at the time limit"),
		AGBSpell_Beam::ShouldBeamContinue(false, false, 5.f, 0.f, 4.f));
	return true;
}

#endif
