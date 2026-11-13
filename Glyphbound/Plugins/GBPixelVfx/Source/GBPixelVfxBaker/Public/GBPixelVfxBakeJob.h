// Bake job description: mirrors Bake/Jobs/<Id>.json. Every key is optional; unknown keys are ignored.
#pragma once

#include "CoreMinimal.h"

struct FGBPixelVfxBakeJob
{
	FName Id = NAME_None;
	FString SystemPath;
	TMap<FName, float> UserFloats;
	TMap<FName, FVector> UserVectors;
	TMap<FName, FLinearColor> UserColors;

	// Capture
	int32 Resolution = 512;
	bool bAutoFrame = true;
	float AutoFramePadding = 0.06f;
	float WidthCm = 600.f;
	FVector CenterCm = FVector(0.f, 0.f, 250.f);
	float ViewYawDeg = 0.f;
	float ViewPitchDeg = 0.f;
	FString Projection = TEXT("Perspective");
	float FovDeg = 10.f;
	float WarmupSeconds = 4.f;
	float DurationSeconds = 2.f;
	int32 SimFps = 60;
	float Exposure = 1.f;
	bool bKeyLight = true;
	float KeyLightIntensity = 8.f;

	// Pixel
	int32 FrameWidth = 48;
	int32 FrameHeight = 64;
	int32 Fps = 12;
	bool bLoop = true;
	int32 LoopBlendFrames = 6;
	FString AlphaMode = TEXT("Max");
	float AlphaThreshold = 0.3f;
	FString ColorMode = TEXT("Split");
	float HotLuminance = 0.55f;
	float HotSaturation = 0.35f;
	TArray<TArray<FColor>> Ramps;
	float LevelsPercentile = 0.99f;
	float Gamma = 1.f;
	float Hysteresis = 0.03f;
	int32 MinIslandPixels = 2;
	bool bFillHoles = false;
	FString Outline = TEXT("None");

	// Light
	bool bLightEnabled = true;
	FLinearColor LightColor = FLinearColor(1.f, 0.38f, 0.04f);
	FString LightColorHex = TEXT("#F3610B");
	float LightIntensity = 12.f;
	float LightRadiusCm = 900.f;
	bool bLightCastShadows = true;
	FString LightTrackMode = TEXT("Baked");

	// World
	FString Facing = TEXT("Upright");
	float Glow = 1.5f;
	bool bLit = false;

	int32 FrameCount() const { return FMath::Max(1, FMath::RoundToInt(DurationSeconds * static_cast<float>(Fps))); }

	bool LoadFromFile(const FString& Path);
	static TArray<FString> ListJobFiles();
};
