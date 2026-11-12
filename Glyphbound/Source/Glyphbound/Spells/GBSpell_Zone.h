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
//   EffectDuration / EffectStrength   optional multipliers on the status effect (default 1)
//   SpriteSize        optional; height of the center sprite (default 60)
// Strings: OnHitEffect (status effect applied every tick, e.g. Slow), Color (disc tint, default = element
// color), Sprite (optional center sprite).
#pragma once

#include "CoreMinimal.h"
#include "Spells/GBSpellActor.h"
#include "GBSpell_Zone.generated.h"

class UStaticMeshComponent;
class UBillboardComponent;

UCLASS()
class GLYPHBOUND_API AGBSpell_Zone : public AGBSpellActor
{
	GENERATED_BODY()

public:
	AGBSpell_Zone();

protected:
	virtual void StartSpell() override;

	void ZoneTick();

	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Disc;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UBillboardComponent> Sprite;

	FTimerHandle TickTimer;
	float ZoneRadius = 0.f;
	float ZoneHeight = 0.f;
	float DamagePerTick = 0.f;
};
