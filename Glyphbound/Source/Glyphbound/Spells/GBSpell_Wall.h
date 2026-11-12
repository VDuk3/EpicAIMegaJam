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
//   TileSize          with Texture: cm covered by one tile of the texture (default 100)
//   Glow              with Texture: > 0.5 = the surface also emits its own color (magic look; default 0)
//   EffectDuration / EffectStrength   optional multipliers on the on-hit effect (default 1)
//   BuildUp           raw elemental build-up per damage tick
// Strings: Texture (tile sprite id: the wall is a STRUCTURE, one solid box with a pixel texture, GDD 3; the ice
// wall), Sprite (without Texture: an effect, a glowing box plus a row of camera-facing sprites; the fire wall),
// Color (glow tint, default = element color), Tint (optional "#RRGGBB" tint of a textured wall; absent = white;
// Color is the plain-shape color and is never used there so it cannot darken the tile).
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
	/** Param "TileSize" (named apart so it cannot clash with another behavior's constant). */
	inline const FName WallTileSize(TEXT("TileSize"));
	/** Param "Glow". */
	inline const FName WallGlow(TEXT("Glow"));
	/** Param "TouchMargin": cm the damage box extends past the wall, so pawns pressed against it still count as touching. */
	inline const FName TouchMargin(TEXT("TouchMargin"));
}

namespace GBSpellString
{
	/** Strings "Texture": tile sprite id of a textured (structure) wall. */
	inline const FName WallTexture(TEXT("Texture"));
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
