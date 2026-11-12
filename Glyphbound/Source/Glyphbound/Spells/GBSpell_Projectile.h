// Spell behavior: launches one AGBProjectile (fireball, lightning bolt).
//
// Params (Spells.json "Params"):
//   Damage             hit damage (also the blast damage when ExplosionRadius > 0)
//   Speed              cm/s
//   Range              cm; the projectile disappears after this distance; also how far the aim trace looks
//   Radius             collision radius, cm
//   ExplosionRadius    optional; > 0 = deals Damage in this radius on impact instead of a single hit
//   ExplosionFalloff   optional; > 0.5 = blast damage fades from the center to the edge (default 1)
//   ExplosionKnockback optional; cm/s push on hostile characters in the blast, full at the center (default 0)
//   ExplosionEffectTime optional; seconds the shockwave is shown (default FGBProjectileConfig's, 0.35)
//   ProjectileGravity  optional; 0 = straight line, 1 = normal gravity (default 0)
//   Pierce             optional; > 0.5 = flies through the actors it damages
//   HeadshotMult       optional; weak point multiplier, 1 = no bonus (default 1)
//   VisualScale        optional; sprite size relative to the collision radius (default 1)
//   EffectDuration / EffectStrength   optional multipliers on the on-hit effect (default 1)
//   BuildUp           raw elemental build-up per hit
// Strings: Sprite (sprite id), Color ("#RRGGBB", default = element color).
#pragma once

#include "CoreMinimal.h"
#include "Spells/GBSpellActor.h"
#include "GBSpell_Projectile.generated.h"

namespace GBSpellParam
{
	inline const FName ExplosionRadius(TEXT("ExplosionRadius"));
	inline const FName ExplosionFalloff(TEXT("ExplosionFalloff"));
	inline const FName ExplosionKnockback(TEXT("ExplosionKnockback"));
	inline const FName ExplosionEffectTime(TEXT("ExplosionEffectTime"));
	inline const FName Pierce(TEXT("Pierce"));
	inline const FName VisualScale(TEXT("VisualScale"));
}

UCLASS()
class GLYPHBOUND_API AGBSpell_Projectile : public AGBSpellActor
{
	GENERATED_BODY()

protected:
	virtual void StartSpell() override;
};
