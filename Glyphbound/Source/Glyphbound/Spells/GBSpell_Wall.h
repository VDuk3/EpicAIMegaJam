// Spell behavior: a wall raised on the ground in front of the caster (fire wall, ice wall).
//
// Params (Spells.json "Params"):
//   Distance          cm in front of the caster where the wall stands
//   Width / Height / Thickness   cm (Width across the caster's view, Thickness along it)
//   Duration          seconds the wall lasts
//   Blocking          > 0.5 = blocks pawns and projectiles and can be broken (needs Health)
//   Health            hit points of a blocking wall; it ends at 0 (enemies and shots break it)
//   DamageTaken       optional; damage multiplier of a blocking wall (default 1)
//   DamagePerSecond   optional; > 0 = hostile actors inside / touching the wall take damage every TickInterval
//   TickInterval      seconds between damage ticks (when DamagePerSecond > 0)
//   SpriteSize        optional; height of each sprite (default = Height)
//   EffectDuration / EffectStrength   optional multipliers on the on-hit effect (default 1)
// Strings: Sprite (drawn in a row along the wall), Color (glow tint, default = element color), OnHitEffect
// (status effect given with each damage tick, e.g. Burn for the fire wall).
#pragma once

#include "CoreMinimal.h"
#include "Spells/GBSpellActor.h"
#include "GBSpell_Wall.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UBillboardComponent;
struct FGBDamageInfo;

namespace GBSpellParam
{
	inline const FName Blocking(TEXT("Blocking"));
}

UCLASS()
class GLYPHBOUND_API AGBSpell_Wall : public AGBSpellActor
{
	GENERATED_BODY()

public:
	AGBSpell_Wall();

protected:
	virtual void StartSpell() override;

	void SetupHealth(float Health);
	void DamageTick();
	void HandleWallDeath(const FGBDamageInfo& KillingBlow);

	UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> Box;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Visual;
	UPROPERTY() TArray<TObjectPtr<UBillboardComponent>> Sprites;

	FTimerHandle DurationTimer;
	FTimerHandle TickTimer;
	float DamagePerTick = 0.f;
};
