// Spell behavior: a flat area on the ground that affects hostile actors standing on it (ice floor).
//
// Params (Spells.json "Params"):
//   Range             optional; cm, the zone is centered on the aimed ground point within this range
//                     (0 or missing = under the caster)
//   Radius            cm
//   Height            optional; cm above the ground that still counts as standing on it (default 200)
//   Duration          seconds
//   TickInterval      seconds between applications
//   DamagePerSecond   optional; > 0 = damage each tick (DamagePerSecond * TickInterval) with the on-hit effect
//   BuildUp           raw elemental build-up per tick (sent directly when DamagePerSecond is 0)
//   FigureGlow        optional; emissive amount of the ground figure (default 1)
//   EffectDuration / EffectStrength   optional multipliers on the status effect (default 1)
// Strings: OnHitEffect (status effect applied every tick, e.g. Slow), Color (figure tint, default = white,
// the sprite's own colors are the look), Sprite (the ground figure; without it a flat tinted disc shows).
#pragma once

#include "CoreMinimal.h"
#include "Spells/GBSpellActor.h"
#include "GBSpell_Zone.generated.h"

class UStaticMeshComponent;

UCLASS()
class GLYPHBOUND_API AGBSpell_Zone : public AGBSpellActor
{
	GENERATED_BODY()

public:
	AGBSpell_Zone();

protected:
	virtual void StartSpell() override;

	void ZoneTick();

	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Figure;

	FTimerHandle TickTimer;
	float ZoneRadius = 0.f;
	float ZoneHeight = 0.f;
	float DamagePerTick = 0.f;
};
