// Pure pixel-art conversion (namespace GBPixelVfxConvert): no UObject, no engine objects.
// Operates on arrays of full resolution linear frames; every step is unit-testable.
#pragma once

#include "CoreMinimal.h"

namespace GBPixelVfxConvert
{
// Bake settings that drive the conversion. Code defaults are conservative (cleanup passes off);
// the bake jobs pick the real values.
struct FSettings
{
	int32 FrameWidth = 48;
	int32 FrameHeight = 64;
	int32 Fps = 12;
	bool bLoop = true;
	int32 LoopBlendFrames = 6;
	FString AlphaMode = TEXT("Max"); // Opacity, Luminance, Max
	float AlphaThreshold = 0.3f;
	FString ColorMode = TEXT("Split"); // Ramp, Split, Nearest
	float HotLuminance = 0.55f;
	float HotSaturation = 0.35f;
	// Palette ramps, dark to light, as sRGB bytes. Output colors are always one of these.
	TArray<TArray<FColor>> Ramps;
	float LevelsPercentile = 0.99f;
	float Gamma = 1.f;
	float Hysteresis = 0.03f;
	int32 MinIslandPixels = 0;
	bool bFillHoles = false;
	int32 MinColorClusterPixels = 0;
	int32 FadeOutFrames = 0;
	FString FadeOutMode = TEXT("Dissolve"); // Dissolve, Erode
	FString Outline = TEXT("None"); // None, Black, Darkest
	bool bLightEnabled = true;
};

// One working frame: RGB is premultiplied by coverage against black (linear HDR),
// Opacity is the coverage (0..1) before any threshold.
struct FWorkFrame
{
	int32 Width = 0;
	int32 Height = 0;
	TArray<FLinearColor> Color;
	TArray<float> Opacity;
};

struct FMappedPixel
{
	bool bSolid = false;
	uint8 Ramp = 0;
	uint8 Index = 0;
	float Value = 0.f; // levels value v (0..1)
	bool bHot = false;
};

struct FFrameMap
{
	int32 Width = 0;
	int32 Height = 0;
	TArray<FMappedPixel> Pixels;

	int32 SolidCount() const
	{
		int32 Count = 0;
		for (const FMappedPixel& Pixel : Pixels)
		{
			if (Pixel.bSolid)
			{
				++Count;
			}
		}
		return Count;
	}
};

GBPIXELVFXBAKER_API bool ParseHexColor(const FString& Hex, FColor& OutColor);
GBPIXELVFXBAKER_API float PixelLuminance(const FLinearColor& Color);
GBPIXELVFXBAKER_API float PixelSaturation(const FLinearColor& Color);
GBPIXELVFXBAKER_API FVector LabFromLinear(const FLinearColor& Color);
GBPIXELVFXBAKER_API bool IsHotPixel(const FLinearColor& Unpremultiplied, const FSettings& Settings);

// Exposure: RGB *= Exposure (in place).
GBPIXELVFXBAKER_API void ApplyExposure(TArray<FWorkFrame>& Frames, float Exposure);

// Loop cross-blend on the full resolution frames (Frames holds KeepCount + BlendFrames entries).
// After blending the extra entries are dropped. No-op unless bLoop and BlendFrames > 0.
GBPIXELVFXBAKER_API void ApplyLoopBlend(TArray<FWorkFrame>& Frames, int32 KeepCount, int32 BlendFrames, bool bLoop);

// Box average downsample of RGB and Opacity.
GBPIXELVFXBAKER_API FWorkFrame DownsampleFrame(const FWorkFrame& In, int32 OutWidth, int32 OutHeight);

// One levels peak for the whole clip plus the hot/cold re-normalization peaks (Split mode).
GBPIXELVFXBAKER_API void ComputePeaks(const TArray<FWorkFrame>& Frames, const FSettings& Settings,
	float& OutPeak, float& OutHotPeak, float& OutColdPeak);

// Per pixel alpha, levels, hard threshold and palette mapping.
GBPIXELVFXBAKER_API FFrameMap MapFrame(const FWorkFrame& Frame, const FSettings& Settings,
	float Peak, float HotPeak, float ColdPeak);

// Temporal hysteresis of the current frame against the previous mapped frame.
// Skipped for a frame whose solid count differs from the previous frame's by more than 30%.
GBPIXELVFXBAKER_API void ApplyHysteresis(FFrameMap& Current, const FFrameMap& Prev, const FSettings& Settings);

// 4-connected solid islands smaller than MinIslandPixels become transparent. No-op when <= 1.
GBPIXELVFXBAKER_API void RemoveSmallIslands(FFrameMap& Map, int32 MinIslandPixels);

// A transparent pixel whose 4 neighbors are all solid becomes solid with the most common neighbor color.
GBPIXELVFXBAKER_API void FillOnePixelHoles(FFrameMap& Map);

// Every 4-connected region of solid pixels with the same ramp and index that is
// smaller than MinPixels is recolored with the most common color among the solid
// pixels 4-adjacent to the region (ties: the darker one). No-op when MinPixels <= 1.
// Regions are found on the state before the pass (one pass).
GBPIXELVFXBAKER_API void RecolorSmallColorClusters(FFrameMap& Map, const FSettings& Settings, int32 MinPixels);

// Fade-out for one-shot clips (no-op when bLoop or FadeOutFrames <= 0). Over the last
// FadeOutFrames frames progress t goes from 1/N to 1; the last frame is always completely empty. Dissolve removes a solid
// pixel when Bayer4x4(x,y) < t; Erode removes solid pixels within ceil(t*MaxDepth)
// 4-connected steps of a transparent pixel or the frame edge.
GBPIXELVFXBAKER_API void ApplyFadeOut(TArray<FFrameMap>& Maps, const FSettings& Settings);

// Transparent pixels 4-adjacent to a solid pixel become the outline color. Returns false for "None".
GBPIXELVFXBAKER_API bool ResolveOutlineColor(const FSettings& Settings, FColor& OutColor);

// Resolve every frame to final sRGB bytes (palette colors exactly, alpha 0 or 255), with outline.
GBPIXELVFXBAKER_API void ResolveFrames(const TArray<FFrameMap>& Maps, const FSettings& Settings,
	TArray<TArray<FColor>>& OutPixels);

// Per frame energy E = sum of v over solid hot pixels (all solid pixels when not Split);
// track = E / max(E). Anchor = average over frames of the v-weighted centroid (frame UV, V down).
GBPIXELVFXBAKER_API void ComputeLightTrack(const TArray<FFrameMap>& Maps, const FSettings& Settings,
	TArray<float>& OutTrack, float& OutAnchorU, float& OutAnchorV);

GBPIXELVFXBAKER_API int32 CountDistinctColors(const TArray<TArray<FColor>>& Frames);
}
