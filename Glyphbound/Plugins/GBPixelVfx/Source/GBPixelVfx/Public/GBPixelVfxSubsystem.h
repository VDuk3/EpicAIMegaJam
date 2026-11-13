// Loads baked pixel VFX clips (Content/Data/Vfx/<Id>.json + <Id>.png) and caches their atlases.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GBPixelVfxTypes.h"
#include "GBPixelVfxSubsystem.generated.h"

UCLASS()
class GBPIXELVFX_API UGBPixelVfxSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// Null when the clip is missing or broken (one warning, never a crash).
	const FGBPixelVfxClip* FindClip(FName Id) const;
	UTexture2D* GetAtlas(FName Id) const;

	// Ids of every clip in the folder (file names without extension).
	TArray<FName> ListClipIds() const;
	void ClearCache();

	static FString VfxDataDir();

private:
	bool LoadClip(FName Id) const;

	UPROPERTY(Transient)
	mutable TMap<FName, TObjectPtr<UTexture2D>> AtlasCache;

	mutable TMap<FName, FGBPixelVfxClip> ClipCache;
	mutable TSet<FName> MissingIds;
};
