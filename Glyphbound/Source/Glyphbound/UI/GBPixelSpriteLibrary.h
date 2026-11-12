// Placeholder pixel art generated from text (Sprites.json): textures for HUD sprites and world billboards.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Data/GBDataTypes.h"
#include "GBPixelSpriteLibrary.generated.h"

class UTexture2D;
class UBillboardComponent;

/**
 * Art rule (GDD 3): everything medieval / magic is pixel art. Until the artists deliver, sprites are drawn as
 * rows of characters in Sprites.json and turned into small textures here (nearest filtering, no mips, no
 * compression), so placeholders are text in git and need no asset import. Real art later replaces a sprite id
 * with an imported texture without touching the callers.
 */
UCLASS()
class GLYPHBOUND_API UGBPixelSpriteLibrary : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UGBPixelSpriteLibrary* Get(const UObject* WorldContextObject);

	/**
	 * The texture of a sprite id, built on first use and cached. Unknown ids return a small magenta checker
	 * (never null), so a missing sprite is visible instead of crashing.
	 */
	UTexture2D* GetSprite(FName SpriteId);

	/** Size in pixels of a sprite (0,0 if unknown). */
	FIntPoint GetSpriteSize(FName SpriteId) const;

	/**
	 * Sets up a billboard as a camera-facing world sprite: texture, visible in game, world size so that the
	 * sprite is WorldHeight cm tall. Tint is not supported by billboards; use different sprites for colors.
	 */
	void SetupBillboard(UBillboardComponent* Billboard, FName SpriteId, float WorldHeight);

	/** Pure conversion, also used by the tests: rows + palette -> BGRA pixels. Returns false if the rows are empty or ragged. */
	static bool BuildPixels(const FGBSpriteDefinition& Definition, TArray<FColor>& OutPixels, int32& OutWidth, int32& OutHeight);

	/** Drops the cache (after a data reload). */
	void ClearCache();

protected:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UTexture2D* CreateTexture(const TArray<FColor>& Pixels, int32 Width, int32 Height, FName DebugName);

	UPROPERTY() TMap<FName, TObjectPtr<UTexture2D>> Cache;
	UPROPERTY() TObjectPtr<UTexture2D> MissingTexture;
};
