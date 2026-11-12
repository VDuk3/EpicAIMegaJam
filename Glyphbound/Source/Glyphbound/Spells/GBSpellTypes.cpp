#include "Spells/GBSpellTypes.h"
#include "Core/GBStatNames.h"
#include "Stats/GBStatsComponent.h"

float FGBSpellCast::Get(FName Param, float DefaultValue) const
{
	const float* Value = Values.Find(Param);
	return Value ? *Value : DefaultValue;
}

bool FGBSpellCast::Has(FName Param) const
{
	return Values.Contains(Param);
}

FString FGBSpellCast::GetString(FName Key, const FString& DefaultValue) const
{
	const FString* Value = Strings.Find(Key);
	return Value ? *Value : DefaultValue;
}

FGBDamageInfo FGBSpellCast::MakeDamageInfo(FName AmountParam, AActor* Causer) const
{
	FGBDamageInfo Info;
	Info.Amount = Get(AmountParam);
	Info.Element = Element;
	Info.Instigator = Caster;
	Info.Causer = Causer;
	Info.SourceId = SpellId;
	if (const FString* Effect = Strings.Find(GBString::OnHitEffect))
	{
		if (!Effect->IsEmpty())
		{
			Info.OnHitEffect = FName(**Effect);
		}
	}
	Info.EffectDurationScale = Get(GBStat::EffectDuration, 1.f);
	Info.EffectStrengthScale = Get(GBStat::EffectStrength, 1.f);
	return Info;
}

namespace GBSpell
{
	static float ApplySpellModifiers(FName SpellId, FName Param, float Value, const UGBStatsComponent* Stats)
	{
		if (!Stats)
		{
			return Value;
		}
		const FName Keys[] = { GBStat::SpellKey(SpellId, Param), GBStat::SpellAllKey(Param) };
		return Stats->EvaluateKeys(Keys, Value);
	}

	/** Upper limit applied after QTE scaling AND modifiers. Cap <= 0 = no cap. */
	static float ApplyParamCap(const FGBScaledParam& Scaled, float Value)
	{
		return Scaled.Cap > 0.f ? FMath::Min(Value, Scaled.Cap) : Value;
	}

	FGBSpellCast ResolveCast(const FGBSpellDefinition& Definition, float Power, float Reach, bool bUseQTE, const UGBStatsComponent* Stats)
	{
		FGBSpellCast Result;
		Result.SpellId = Definition.Id;
		Result.Element = Definition.Element;
		Result.Power = Power;
		Result.Reach = Reach;
		Result.bUsedQTE = bUseQTE;
		Result.Strings = Definition.Strings;
		Result.Values.Reserve(Definition.Params.Num());
		for (const TPair<FName, FGBScaledParam>& Pair : Definition.Params)
		{
			const float Value = Pair.Value.Resolve(Power, Reach, bUseQTE);
			Result.Values.Add(Pair.Key, ApplyParamCap(Pair.Value, ApplySpellModifiers(Definition.Id, Pair.Key, Value, Stats)));
		}
		return Result;
	}

	float ResolveParam(const FGBSpellDefinition& Definition, FName Param, float Power, float Reach, bool bUseQTE, const UGBStatsComponent* Stats, float DefaultValue)
	{
		const FGBScaledParam* Scaled = Definition.Params.Find(Param);
		if (!Scaled)
		{
			return DefaultValue;
		}
		return ApplyParamCap(*Scaled, ApplySpellModifiers(Definition.Id, Param, Scaled->Resolve(Power, Reach, bUseQTE), Stats));
	}
}
