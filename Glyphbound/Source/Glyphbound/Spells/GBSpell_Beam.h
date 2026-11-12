// Spell behavior: a sustained cone from the caster's aim that damages what is in front (flamethrower).
// Runs until Duration ends or the cast input is pressed again (the spellbook calls OnCastReleased).
//
// Params (Spells.json "Params"):
//   Range             cm; length of the cone
//   ConeAngle         degrees; half-angle of the cone
//   DamagePerSecond   damage per second to each hostile actor in the cone (dealt every TickInterval)
//   TickInterval      seconds between damage ticks
//   Duration          seconds the beam can last
//   ManaPerSecond     optional; > 0 = mana drained while it runs; it stops when mana runs out (default 0)
//   EffectDuration / EffectStrength   optional multipliers on the on-hit effect (default 1)
//   SpriteSize        optional; height of the sprite nearest to the hand (default 40); the farther ones grow with the cone
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
}

UCLASS()
class GLYPHBOUND_API AGBSpell_Beam : public AGBSpellActor
{
	GENERATED_BODY()

public:
	AGBSpell_Beam();

	virtual void Tick(float DeltaSeconds) override;
	virtual bool IsSustaining() const override { return bSpellStarted && !bEnded; }

protected:
	virtual void StartSpell() override;

	void DamageTick();
	void FollowAim();

	UPROPERTY() TArray<TObjectPtr<UBillboardComponent>> Sprites;

	FTimerHandle TickTimer;
	float BeamRange = 0.f;
	float ConeHalfAngleDeg = 0.f;
	float DamagePerTick = 0.f;
	float ManaPerTick = 0.f;
};
