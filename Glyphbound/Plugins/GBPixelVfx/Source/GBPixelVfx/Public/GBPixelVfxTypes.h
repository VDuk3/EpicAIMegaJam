// The baked pixel VFX clip: what the baker writes (Content/Data/Vfx/<Id>.json + <Id>.png)
// and what the runtime subsystem, component and actor read back. Plain struct, no UObject.
#pragma once

#include "CoreMinimal.h"
#include "GBPixelVfxLog.h"
#include "GBPixelVfxTypes.generated.h"

UENUM(BlueprintType)
enum class EGBPixelVfxFacing : uint8
{
	Billboard UMETA(DisplayName = "Billboard"),
	Upright UMETA(DisplayName = "Upright"),
	Ground UMETA(DisplayName = "Ground"),
	Fixed UMETA(DisplayName = "Fixed")
};

USTRUCT(BlueprintType)
struct GBPIXELVFX_API FGBPixelVfxLight
{
	GENERATED_BODY()

	UPROPERTY()
	bool bEnabled = true;

	// sRGB hex string ("#RRGGBB") as stored in the clip JSON.
	UPROPERTY()
	FString ColorHex = TEXT("#FFFFFF");

	UPROPERTY()
	FLinearColor Color = FLinearColor::White;

	// Candelas at track value 1.
	UPROPERTY()
	float Intensity = 0.f;

	UPROPERTY()
	float RadiusCm = 0.f;

	UPROPERTY()
	bool bCastShadows = true;

	// Average emissive centroid of the clip, in frame UV (V down).
	UPROPERTY()
	float AnchorU = 0.5f;

	UPROPERTY()
	float AnchorV = 0.5f;

	// One value 0..1 per frame. "Baked" = the measured envelope, "Constant" = always 1.
	UPROPERTY()
	FString TrackMode = TEXT("Baked");

	UPROPERTY()
	TArray<float> Track;
};

USTRUCT(BlueprintType)
struct GBPIXELVFX_API FGBPixelVfxClip
{
	GENERATED_BODY()

	UPROPERTY()
	FName Id = NAME_None;

	// PNG file name inside Content/Data/Vfx (for example "FluidFire.png").
	UPROPERTY()
	FString AtlasFile;

	UPROPERTY()
	int32 FrameWidth = 0;

	UPROPERTY()
	int32 FrameHeight = 0;

	UPROPERTY()
	int32 Columns = 0;

	UPROPERTY()
	int32 FrameCount = 0;

	UPROPERTY()
	int32 Fps = 12;

	UPROPERTY()
	bool bLoop = true;

	// Real size of the captured area: the baked sprite fills the same space as the source effect.
	UPROPERTY()
	float WorldWidthCm = 0.f;

	UPROPERTY()
	float WorldHeightCm = 0.f;

	// Where the source system origin sits inside the frame (0..1, V down).
	UPROPERTY()
	float PivotU = 0.5f;

	UPROPERTY()
	float PivotV = 0.5f;

	UPROPERTY()
	EGBPixelVfxFacing Facing = EGBPixelVfxFacing::Upright;

	UPROPERTY()
	float Glow = 0.f;

	// True when the sprite needs scene lighting (smoke): uses the Lit material variant.
	UPROPERTY()
	bool bLit = false;

	UPROPERTY()
	FGBPixelVfxLight Light;

	UPROPERTY()
	FString SourceSystem;

	UPROPERTY()
	float SourceDurationSeconds = 0.f;

	UPROPERTY()
	int32 SourceSimFps = 60;

	UPROPERTY()
	FString EngineVersion;

	UPROPERTY()
	FString BakedUtc;

	int32 Rows() const { return (Columns > 0 && FrameCount > 0) ? (FrameCount + Columns - 1) / Columns : 0; }

	// UV offset (x, y) and scale (z, w) of one frame inside the atlas.
	static FVector4f FrameRect(const FGBPixelVfxClip& Clip, int32 FrameIndex);

	// Stepped frame index at the given playback time. Loops wrap; one-shots clamp to the
	// last frame and set bOutFinished once it has been shown for one frame duration.
	static int32 FrameAtTime(const FGBPixelVfxClip& Clip, float Seconds, bool& bOutFinished);

	bool LoadFromJsonString(const FString& Text);
	bool SaveToJsonString(FString& OutText) const;
};
