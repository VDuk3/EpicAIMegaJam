// Spell behavior: a slow ball that moves forward and damages every hostile actor around it (static-electricity ball).
//
// Params (Spells.json "Params"):
//   Speed             cm/s along the caster's aim; it stops (and keeps working) when it reaches a wall
//   Range             cm; the orb ends after travelling this far
//   Duration          seconds; the orb ends after this time
//   Radius            cm; damage area around the orb
//   DamagePerSecond   damage per second to each hostile actor in Radius (dealt every TickInterval)
//   TickInterval      seconds between damage ticks
//   ShowArea          optional; > 0.5 = draws a faint glow sphere of Radius (default 1)
//   EffectDuration / EffectStrength   optional multipliers on the on-hit effect (default 1)
//   BuildUp           raw elemental build-up per damage tick
//   SpriteSize        optional; height of the orb sprite (default 60)
// Strings: Sprite, Color (area tint, default = element color).
#pragma once

#include "CoreMinimal.h"
#include "Spells/GBSpellActor.h"
#include "GBSpell_Orb.generated.h"

class UStaticMeshComponent;
class UBillboardComponent;

namespace GBSpellParam
{
	inline const FName ShowArea(TEXT("ShowArea"));
}

UCLASS()
class GLYPHBOUND_API AGBSpell_Orb : public AGBSpellActor
{
	GENERATED_BODY()

public:
	AGBSpell_Orb();

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void StartSpell() override;

	void DamageTick();

	UPROPERTY(VisibleAnywhere) TObjectPtr<UBillboardComponent> Sprite;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Area;

	FTimerHandle TickTimer;
	FVector MoveDirection = FVector::ForwardVector;
	float MoveSpeed = 0.f;
	float RangeLeft = 0.f;
	float OrbRadius = 0.f;
	float DamagePerTick = 0.f;
	bool bMoving = false;
};
