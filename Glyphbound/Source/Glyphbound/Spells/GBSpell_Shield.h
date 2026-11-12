// Spell behavior: a sustained frontal block held by the caster (ice shield).
// Runs until Duration ends, its Health is used up, or the cast input is pressed again.
//
// Params (Spells.json "Params"):
//   Duration          seconds the shield can be held
//   BlockFraction     0..1; part of each hit coming from the front (relative to the caster's aim) that is removed
//   Health            total damage the shield can absorb; every blocked point is subtracted and it breaks at 0
//   Distance          optional; cm in front of the eyes where the shield sprite is drawn (default 80)
//   SpriteSize        optional; height of the shield sprite (default 45)
// Strings: Sprite.
#pragma once

#include "CoreMinimal.h"
#include "Spells/GBSpellActor.h"
#include "GBSpell_Shield.generated.h"

class UBillboardComponent;
class UGBAttributeComponent;

UCLASS()
class GLYPHBOUND_API AGBSpell_Shield : public AGBSpellActor
{
	GENERATED_BODY()

public:
	AGBSpell_Shield();

	virtual void Tick(float DeltaSeconds) override;
	virtual bool IsSustaining() const override { return bSpellStarted && !bEnded; }

	float GetShieldHealth() const { return ShieldHealth; }

	/** Pure rule, also usable by tests: how much of a hit the shield removes. HitDirection travels from the source to the caster. */
	static float ComputeBlockedDamage(float Amount, const FVector& HitDirection, const FVector& AimDirection, float BlockFraction, float ShieldHealthLeft);

protected:
	virtual void StartSpell() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void HandleIncomingDamage(FGBDamageInfo& Info);
	void FollowAim();

	UPROPERTY(VisibleAnywhere) TObjectPtr<UBillboardComponent> Sprite;

	TWeakObjectPtr<UGBAttributeComponent> BoundAttributes;
	FDelegateHandle DamageHandle;
	float ShieldHealth = 0.f;
	float BlockFraction = 0.f;
	float SpriteDistance = 0.f;
};
