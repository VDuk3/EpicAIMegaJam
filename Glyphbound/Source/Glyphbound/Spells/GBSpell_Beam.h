// Spell behavior: a sustained cone from the caster's aim that damages what is in front (flamethrower).
// Runs until Duration ends (Duration <= 0 = no time limit) or the sustain rule ends it; with HoldToSustain
// the beam only keeps going while the cast key is held (the spellbook's press-again-to-stop also still applies).
//
// Params (Spells.json "Params"):
//   Range             cm; length of the cone
//   ConeAngle         degrees; half-angle of the cone
//   DamagePerSecond   damage per second to each hostile actor in the cone (dealt every TickInterval)
//   TickInterval      seconds between damage ticks
//   Duration          seconds the beam can last at most; <= 0 = no time limit
//   MinDuration       seconds the beam always runs at least (so a cast released during the QTE is not wasted)
//   HoldToSustain     1 = the beam only keeps going while the cast key is held; 0 = old behavior (key ignored)
//   ManaPerSecond     optional; > 0 = mana drained while it runs; it stops when mana runs out (default 0)
//   ManaCost          paid once at the cast (handled by the spellbook, as for every spell)
//   EffectDuration / EffectStrength   optional multipliers on the on-hit effect (default 1)
//   SpriteSize        optional; height of the sprite nearest to the hand (default 40); the farther ones grow with the cone
// Strings: Sprite (drawn several times along the cone), OnHitEffect (e.g. Burn).
// Targets behind world geometry (visibility check against world static) are not hit.
// Strings: Sprite (drawn several times along the cone), OnHitEffect (e.g. Burn).
// Targets behind world geometry (visibility check against world static) are not hit.
#pragma once

#include "CoreMinimal.h"
#include "Spells/GBSpellActor.h"
#include "GBSpell_Beam.generated.h"

class UBillboardComponent;

namespace GBSpellParam
{
	inline const FName ConeAngle(TEXT("ConeAngle"));
	inline const FName ManaPerSecond(TEXT("ManaPerSecond"));
	inline const FName HoldToSustain(TEXT("HoldToSustain"));
	inline const FName MinDuration(TEXT("MinDuration"));
}

UCLASS()
class GLYPHBOUND_API AGBSpell_Beam : public AGBSpellActor
{
	GENERATED_BODY()

public:
	AGBSpell_Beam();

	virtual void Tick(float DeltaSeconds) override;
	virtual bool IsSustaining() const override { return bSpellStarted && !bEnded; }

	/**
	 * Pure sustain rule: MaxDuration <= 0 = no time limit. False once Elapsed reaches MaxDuration;
	 * true while Elapsed is still below MinDuration; otherwise the beam keeps going only when
	 * hold mode is off or the cast key is still held.
	 */
	static bool ShouldBeamContinue(bool bHoldToSustain, bool bKeyHeld, float Elapsed, float MinDuration, float MaxDuration);

protected:
	virtual void StartSpell() override;

	void DamageTick();
	void FollowAim();
	/** Ends the spell when the sustain rule says it is over. */
	void CheckSustain();

	UPROPERTY() TArray<TObjectPtr<UBillboardComponent>> Sprites;

	FTimerHandle TickTimer;
	float BeamRange = 0.f;
	float ConeHalfAngleDeg = 0.f;
	float DamagePerTick = 0.f;
	float ManaPerTick = 0.f;
	bool bHoldToSustain = false;
	float BeamMinDuration = 0.f;
	float BeamMaxDuration = 0.f;
	/** World time when the beam started (for Elapsed in the sustain rule). */
	double BeamStartTime = 0.0;
};
