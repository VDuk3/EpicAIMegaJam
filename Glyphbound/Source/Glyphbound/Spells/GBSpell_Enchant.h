// Spell behavior: the weapon enchant. Puts a timed status effect on the caster that buffs the gun
// ("<EffectPrefix><Element>", e.g. "Enchant_Fire" in StatusEffects.json; the buff itself is the effect's modifiers).
// The element is the one the player picked for the enchant slot (the spellbook sets SpellCast.Element).
// Any other active effect with the same prefix is removed first: only one enchant at a time.
//
// Params (Spells.json "Params"):
//   Duration          seconds the buff lasts (the effect's own Duration is scaled to match; 0 = keep the effect's)
//   EffectStrength    optional; multiplier on the effect's strength (default 1)
// Strings: EffectPrefix (optional, default "Enchant_").
#pragma once

#include "CoreMinimal.h"
#include "Spells/GBSpellActor.h"
#include "GBSpell_Enchant.generated.h"

namespace GBSpellString
{
	inline const FName EffectPrefix(TEXT("EffectPrefix"));
}

UCLASS()
class GLYPHBOUND_API AGBSpell_Enchant : public AGBSpellActor
{
	GENERATED_BODY()

public:
	/** "Enchant_" + element name ("Enchant_Fire"). */
	static FName MakeEffectId(const FString& Prefix, EGBElement Element);

	/** Duration scale that makes an effect of EffectDuration seconds last WantedDuration seconds; 1 if either is 0. */
	static float ComputeDurationScale(float WantedDuration, float EffectDuration);

protected:
	virtual void StartSpell() override;
};
