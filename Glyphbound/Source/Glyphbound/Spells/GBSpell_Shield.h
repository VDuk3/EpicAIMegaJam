// Spell behavior: a summoned ice shield the caster raises like a first-person RPG shield (GDD 6).
// Casting summons it for Duration (or until its Health is used up). It does nothing by itself: it registers in the
// caster's UGBSpellbookComponent, which raises / lowers it with the Block input. Only while raised it removes part
// of the hits from the front and slows the caster. No world visual: the HUD draws the raised shield in first person.
//
// Params (Spells.json "Params"):
//   Duration              seconds the shield stays summoned
//   BlockFraction         0..1; part of each hit coming from the front (relative to the caster's aim) that is removed
//   Health                total damage the shield can absorb; every blocked point is subtracted and it breaks at 0
//   RaisedMoveSpeedMult   multiplier on the caster's MoveSpeed while the shield is raised
// Strings: Sprite, Color (read by the HUD for the first-person shield), HudTint (optional "#RRGGBB" tint of the
// raised shield sprite in the HUD; absent = white, so Color never changes the HUD look).
#pragma once

#include "CoreMinimal.h"
#include "Spells/GBSpellActor.h"
#include "GBSpell_Shield.generated.h"

class UGBAttributeComponent;

/** Shield params that are not in GBStat. */
namespace GBShieldParam
{
	inline const FName RaisedMoveSpeedMult(TEXT("RaisedMoveSpeedMult"));
	/** Param "BlockArcDegrees": frontal arc (degrees) the raised shield blocks; 180 = the front hemisphere, 360 = every side. */
	inline const FName BlockArcDegrees(TEXT("BlockArcDegrees"));
}

UCLASS()
class GLYPHBOUND_API AGBSpell_Shield : public AGBSpellActor
{
	GENERATED_BODY()

public:
	AGBSpell_Shield();

	/** A summoned shield never makes the spellbook busy. */
	virtual bool IsSustaining() const override { return false; }

	/** Raised by the spellbook while Block is held (and allowed). Lowering also removes the move-speed penalty. */
	void SetRaised(bool bInRaised);
	bool IsRaised() const { return bRaised; }
	/** Started, not ended, not broken. */
	bool IsShieldActive() const { return bSpellStarted && !bEnded && ShieldHealth > 0.f; }

	float GetShieldHealth() const { return ShieldHealth; }
	float GetShieldMaxHealth() const { return MaxShieldHealth; }
	/** Seconds until the shield expires (0 once ended). */
	float GetTimeRemaining() const;

	/** Modifier source of the raised-shield slow on the caster's stats. */
	static const FName RaisedSource;

	/** Pure rule, also usable by tests: how much of a hit the shield removes. HitDirection travels from the source to the caster. */
	static float ComputeBlockedDamage(float Amount, const FVector& HitDirection, const FVector& AimDirection, float BlockFraction, float ShieldHealthLeft, float BlockArcDegrees = 180.f);

protected:
	virtual void StartSpell() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void HandleIncomingDamage(FGBDamageInfo& Info);

	TWeakObjectPtr<UGBAttributeComponent> BoundAttributes;
	FDelegateHandle DamageHandle;
	float ShieldHealth = 0.f;
	float MaxShieldHealth = 0.f;
	float BlockFraction = 0.f;
	float BlockArcDegrees = 180.f;
	float RaisedMoveSpeedMult = 1.f;
	/** World time (dilated, like the end timer) when the shield expires. */
	double ExpireTime = 0.0;
	bool bRaised = false;
};
