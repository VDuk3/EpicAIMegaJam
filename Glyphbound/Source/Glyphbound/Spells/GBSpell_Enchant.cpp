#include "Spells/GBSpell_Enchant.h"
#include "Combat/GBStatusEffectComponent.h"
#include "Core/GBStatNames.h"
#include "Data/GBDataSubsystem.h"

namespace
{
	const TCHAR* EnchantDefaultEffectPrefix = TEXT("Enchant_");
}

FName AGBSpell_Enchant::MakeEffectId(const FString& Prefix, EGBElement Element)
{
	const FString ElementName = StaticEnum<EGBElement>()->GetNameStringByValue(static_cast<int64>(Element));
	return FName(*(Prefix + ElementName));
}

float AGBSpell_Enchant::ComputeDurationScale(float WantedDuration, float EffectDuration)
{
	if (WantedDuration <= 0.f || EffectDuration <= 0.f)
	{
		return 1.f;
	}
	return WantedDuration / EffectDuration;
}

void AGBSpell_Enchant::StartSpell()
{
	AActor* Caster = GetCaster();
	UGBStatusEffectComponent* Effects = UGBStatusEffectComponent::Find(Caster);
	if (!Effects)
	{
		UE_LOG(LogGB, Warning, TEXT("Spell '%s': the caster has no status effect component, nothing to enchant."), *SpellCast.SpellId.ToString());
		EndSpell();
		return;
	}

	FString Prefix = SpellCast.GetString(GBSpellString::EffectPrefix, EnchantDefaultEffectPrefix);
	if (Prefix.IsEmpty())
	{
		// An empty prefix would match (and remove) every active effect.
		Prefix = EnchantDefaultEffectPrefix;
	}
	const FName EffectId = MakeEffectId(Prefix, SpellCast.Element);

	UGBDataSubsystem* Data = UGBDataSubsystem::Get(this);
	const FGBStatusEffectDefinition* EffectDefinition = Data ? Data->FindStatusEffect(EffectId) : nullptr;
	if (!EffectDefinition)
	{
		UE_LOG(LogGB, Warning, TEXT("Spell '%s': status effect '%s' is not in StatusEffects.json."), *SpellCast.SpellId.ToString(), *EffectId.ToString());
		EndSpell();
		return;
	}

	// Only one enchant at a time.
	TArray<FName> ToRemove;
	for (const FGBActiveStatusEffect& Active : Effects->GetActiveEffects())
	{
		if (Active.EffectId != EffectId && Active.EffectId.ToString().StartsWith(Prefix))
		{
			ToRemove.AddUnique(Active.EffectId);
		}
	}
	for (const FName OldId : ToRemove)
	{
		Effects->RemoveEffect(OldId);
	}

	const float DurationScale = ComputeDurationScale(ReadParam(GBStat::Duration, 0.f), EffectDefinition->Duration);
	const float StrengthScale = SpellCast.Get(GBStat::EffectStrength, 1.f);
	Effects->ApplyEffect(EffectId, Caster, DurationScale, StrengthScale);
	EndSpell();
}
