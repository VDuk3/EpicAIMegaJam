// Staffs (GDD 8): medieval, pixel art drawn by the HUD (sprite id in Strings["Sprite"]); no 3D mesh.
#pragma once

#include "CoreMinimal.h"
#include "Weapons/GBHeldItem.h"
#include "GBStaff.generated.h"

/**
 * Primary = basic magic attack: an AGBProjectile from the muzzle towards the point the camera aims at.
 * Stats: Damage, FireInterval, ProjectileSpeed, ProjectileRadius, ProjectileGravity, Range, ManaCost (0 = free).
 * Looks: pixel-art projectile, sprite from Strings["ProjectileSprite"]. Blocked while the spellbook is busy.
 */
UCLASS()
class GLYPHBOUND_API AGBStaff : public AGBHeldItem
{
	GENERATED_BODY()

public:
	virtual void StartPrimary() override { PressTrigger(); }
	virtual void StopPrimary() override { ReleaseTrigger(); }

protected:
	virtual bool PerformAttack() override;
};
