// Pure-logic tests of spell cast resolution (no world, no data files).
#include "Misc/AutomationTest.h"
#include "Core/GBStatNames.h"
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

#endif
