// Spell behavior: a rune placed on the ground that explodes when a hostile actor steps on it (lightning rune).
//
// Params (Spells.json "Params"):
//   Range             cm; the rune is placed on the aimed ground point within this range
//   ArmTime           seconds before it can trigger
//   TriggerRadius     cm; a hostile actor this close (horizontally) sets it off
//   Height            optional; cm above the rune that still counts for the trigger (default 200)
//   Radius            cm; blast radius
//   BlastHeight       optional; cm above the ground where the blast is centered (default 50)
//   Falloff           optional; > 0.5 = blast damage fades from the center to the edge (default 0)
//   Damage            blast damage
//   Duration          seconds it waits before disappearing unused
//   EffectDuration / EffectStrength   optional multipliers on the on-hit effect (default 1)
//   SpriteSize        optional; height of the rune sprite (default 50)
// Strings: Sprite, Color (trigger-area tint, default = element color), OnHitEffect (e.g. Shock).
#pragma once

#include "CoreMinimal.h"
#include "Spells/GBSpellActor.h"
#include "GBSpell_Trap.generated.h"

class UStaticMeshComponent;
class UBillboardComponent;

namespace GBSpellParam
{
	inline const FName ArmTime(TEXT("ArmTime"));
	inline const FName TriggerRadius(TEXT("TriggerRadius"));
	inline const FName BlastHeight(TEXT("BlastHeight"));
	inline const FName Falloff(TEXT("Falloff"));
}

UCLASS()
class GLYPHBOUND_API AGBSpell_Trap : public AGBSpellActor
{
	GENERATED_BODY()

public:
	AGBSpell_Trap();

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void StartSpell() override;

	void Arm();
	void Trigger();

	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Disc;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UBillboardComponent> Sprite;

	FTimerHandle ArmTimer;
	bool bArmed = false;
	float TriggerRadius = 0.f;
	float TriggerHeight = 0.f;
};
