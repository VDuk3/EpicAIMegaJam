// FGBPixelVfxBakeJob: JSON loader. Reads exactly the keys of the Bake/Jobs files.
#include "GBPixelVfxBakeJob.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "GBPixelVfxConvert.h"

namespace GBPixelVfxJobLocal
{
	bool ParseHex(const FString& Text, FLinearColor& OutColor)
	{
		FColor Srgb;
		if (!GBPixelVfxConvert::ParseHexColor(Text, Srgb))
		{
			return false;
		}
		OutColor = FLinearColor(Srgb);
		return true;
	}

	void ReadFloats(const TSharedPtr<FJsonObject>& Obj, TMap<FName, float>& Out)
	{
		if (!Obj.IsValid())
		{
			return;
		}
		for (const auto& Pair : Obj->Values)
		{
			Out.Add(FName(*Pair.Key), static_cast<float>(Pair.Value->AsNumber()));
		}
	}

	void ReadVectors(const TSharedPtr<FJsonObject>& Obj, TMap<FName, FVector>& Out)
	{
		if (!Obj.IsValid())
		{
			return;
		}
		for (const auto& Pair : Obj->Values)
		{
			const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
			if (Pair.Value->TryGetArray(Arr) && Arr && Arr->Num() == 3)
			{
				Out.Add(FName(*Pair.Key), FVector(
					(*Arr)[0]->AsNumber(), (*Arr)[1]->AsNumber(), (*Arr)[2]->AsNumber()));
			}
		}
	}

	void ReadColors(const TSharedPtr<FJsonObject>& Obj, TMap<FName, FLinearColor>& Out)
	{
		if (!Obj.IsValid())
		{
			return;
		}
		for (const auto& Pair : Obj->Values)
		{
			FString Text;
			if (Pair.Value->TryGetString(Text))
			{
				FLinearColor Color;
				if (ParseHex(Text, Color))
				{
					Out.Add(FName(*Pair.Key), Color);
				}
			}
		}
	}

	FVector ReadVector3(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Key, const FVector& Default)
	{
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (!Obj.IsValid() || !Obj->TryGetArrayField(Key, Arr) || !Arr || Arr->Num() != 3)
		{
			return Default;
		}
		return FVector((*Arr)[0]->AsNumber(), (*Arr)[1]->AsNumber(), (*Arr)[2]->AsNumber());
	}

	double ReadNumber(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Key, double Default)
	{
		double Value = Default;
		if (Obj.IsValid())
		{
			Obj->TryGetNumberField(Key, Value);
		}
		return Value;
	}

	bool ReadBool(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Key, bool Default)
	{
		bool Value = Default;
		if (Obj.IsValid())
		{
			Obj->TryGetBoolField(Key, Value);
		}
		return Value;
	}

	FString ReadString(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Key, const FString& Default)
	{
		FString Value = Default;
		if (Obj.IsValid())
		{
			Obj->TryGetStringField(Key, Value);
		}
		return Value;
	}

	// The FluidFire fire ramp: used when a job has no Ramps array at all.
	TArray<FColor> DefaultFireRamp()
	{
		TArray<FColor> Ramp;
		const TCHAR* Hex[] = { TEXT("#4B0F0F"), TEXT("#BC2A12"), TEXT("#F03416"),
			TEXT("#F3610B"), TEXT("#E2DE9C"), TEXT("#F5F3CF") };
		for (const TCHAR* H : Hex)
		{
			FColor Srgb;
			if (GBPixelVfxConvert::ParseHexColor(FString(H), Srgb))
			{
				Ramp.Add(Srgb);
			}
		}
		return Ramp;
	}

	void ReadRamps(const TSharedPtr<FJsonObject>& Pixel, TArray<TArray<FColor>>& Out)
	{
		const TArray<TSharedPtr<FJsonValue>>* Ramps = nullptr;
		if (!Pixel.IsValid() || !Pixel->TryGetArrayField(TEXT("Ramps"), Ramps) || !Ramps)
		{
			// Absent: keep the caller's defaults.
			return;
		}
		Out.Reset();
		for (const TSharedPtr<FJsonValue>& Ramp : *Ramps)
		{
			const TArray<TSharedPtr<FJsonValue>>* Colors = nullptr;
			if (!Ramp->TryGetArray(Colors) || !Colors)
			{
				continue;
			}
			TArray<FColor> RampColors;
			for (const TSharedPtr<FJsonValue>& Entry : *Colors)
			{
				FString Text;
				if (Entry->TryGetString(Text))
				{
					FColor Srgb;
					if (GBPixelVfxConvert::ParseHexColor(Text, Srgb))
					{
						RampColors.Add(Srgb);
					}
				}
			}
			if (RampColors.Num() > 0)
			{
				Out.Add(MoveTemp(RampColors));
			}
		}
	}
}

TArray<FString> FGBPixelVfxBakeJob::ListJobFiles()
{
	TArray<FString> Files;
	const FString Dir = FPaths::ProjectPluginsDir() / TEXT("GBPixelVfx/Bake/Jobs");
	IFileManager::Get().FindFiles(Files, *(Dir / TEXT("*.json")), true, false);
	Files.Sort();
	for (FString& File : Files)
	{
		File = Dir / File;
	}
	return Files;
}

bool FGBPixelVfxBakeJob::LoadFromFile(const FString& Path)
{
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *Path))
	{
		return false;
	}
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return false;
	}

	FGBPixelVfxBakeJob Fresh;
	FString IdText;
	if (!Root->TryGetStringField(TEXT("Id"), IdText) || IdText.IsEmpty())
	{
		return false;
	}
	Fresh.Id = FName(*IdText);
	Fresh.Ramps.Add(GBPixelVfxJobLocal::DefaultFireRamp());
	Root->TryGetStringField(TEXT("System"), Fresh.SystemPath);

	const TSharedPtr<FJsonObject>* Sub = nullptr;
	if (Root->TryGetObjectField(TEXT("UserFloats"), Sub) && Sub)
	{
		GBPixelVfxJobLocal::ReadFloats(*Sub, Fresh.UserFloats);
	}
	if (Root->TryGetObjectField(TEXT("UserVectors"), Sub) && Sub)
	{
		GBPixelVfxJobLocal::ReadVectors(*Sub, Fresh.UserVectors);
	}
	if (Root->TryGetObjectField(TEXT("UserColors"), Sub) && Sub)
	{
		GBPixelVfxJobLocal::ReadColors(*Sub, Fresh.UserColors);
	}

	const TSharedPtr<FJsonObject> Capture = Root->GetObjectField(TEXT("Capture"));
	Fresh.Resolution = static_cast<int32>(GBPixelVfxJobLocal::ReadNumber(Capture, TEXT("Resolution"), Fresh.Resolution));
	Fresh.bAutoFrame = GBPixelVfxJobLocal::ReadBool(Capture, TEXT("AutoFrame"), Fresh.bAutoFrame);
	Fresh.AutoFramePadding = static_cast<float>(GBPixelVfxJobLocal::ReadNumber(Capture, TEXT("AutoFramePadding"), Fresh.AutoFramePadding));
	Fresh.WidthCm = static_cast<float>(GBPixelVfxJobLocal::ReadNumber(Capture, TEXT("WidthCm"), Fresh.WidthCm));
	Fresh.CenterCm = GBPixelVfxJobLocal::ReadVector3(Capture, TEXT("CenterCm"), Fresh.CenterCm);
	Fresh.ViewYawDeg = static_cast<float>(GBPixelVfxJobLocal::ReadNumber(Capture, TEXT("ViewYawDeg"), Fresh.ViewYawDeg));
	Fresh.ViewPitchDeg = static_cast<float>(GBPixelVfxJobLocal::ReadNumber(Capture, TEXT("ViewPitchDeg"), Fresh.ViewPitchDeg));
	Fresh.Projection = GBPixelVfxJobLocal::ReadString(Capture, TEXT("Projection"), Fresh.Projection);
	Fresh.FovDeg = static_cast<float>(GBPixelVfxJobLocal::ReadNumber(Capture, TEXT("FovDeg"), Fresh.FovDeg));
	Fresh.WarmupSeconds = static_cast<float>(GBPixelVfxJobLocal::ReadNumber(Capture, TEXT("WarmupSeconds"), Fresh.WarmupSeconds));
	Fresh.DurationSeconds = static_cast<float>(GBPixelVfxJobLocal::ReadNumber(Capture, TEXT("DurationSeconds"), Fresh.DurationSeconds));
	Fresh.SimFps = static_cast<int32>(GBPixelVfxJobLocal::ReadNumber(Capture, TEXT("SimFps"), Fresh.SimFps));
	Fresh.Exposure = static_cast<float>(GBPixelVfxJobLocal::ReadNumber(Capture, TEXT("Exposure"), Fresh.Exposure));
	Fresh.bKeyLight = GBPixelVfxJobLocal::ReadBool(Capture, TEXT("KeyLight"), Fresh.bKeyLight);
	Fresh.KeyLightIntensity = static_cast<float>(GBPixelVfxJobLocal::ReadNumber(Capture, TEXT("KeyLightIntensity"), Fresh.KeyLightIntensity));

	const TSharedPtr<FJsonObject> Pixel = Root->GetObjectField(TEXT("Pixel"));
	Fresh.FrameWidth = static_cast<int32>(GBPixelVfxJobLocal::ReadNumber(Pixel, TEXT("FrameWidth"), Fresh.FrameWidth));
	Fresh.FrameHeight = static_cast<int32>(GBPixelVfxJobLocal::ReadNumber(Pixel, TEXT("FrameHeight"), Fresh.FrameHeight));
	Fresh.Fps = static_cast<int32>(GBPixelVfxJobLocal::ReadNumber(Pixel, TEXT("Fps"), Fresh.Fps));
	Fresh.bLoop = GBPixelVfxJobLocal::ReadBool(Pixel, TEXT("Loop"), Fresh.bLoop);
	Fresh.LoopBlendFrames = static_cast<int32>(GBPixelVfxJobLocal::ReadNumber(Pixel, TEXT("LoopBlendFrames"), Fresh.LoopBlendFrames));
	Fresh.AlphaMode = GBPixelVfxJobLocal::ReadString(Pixel, TEXT("AlphaMode"), Fresh.AlphaMode);
	Fresh.AlphaThreshold = static_cast<float>(GBPixelVfxJobLocal::ReadNumber(Pixel, TEXT("AlphaThreshold"), Fresh.AlphaThreshold));
	Fresh.ColorMode = GBPixelVfxJobLocal::ReadString(Pixel, TEXT("ColorMode"), Fresh.ColorMode);
	Fresh.HotLuminance = static_cast<float>(GBPixelVfxJobLocal::ReadNumber(Pixel, TEXT("HotLuminance"), Fresh.HotLuminance));
	Fresh.HotSaturation = static_cast<float>(GBPixelVfxJobLocal::ReadNumber(Pixel, TEXT("HotSaturation"), Fresh.HotSaturation));
	GBPixelVfxJobLocal::ReadRamps(Pixel, Fresh.Ramps);
	Fresh.LevelsPercentile = static_cast<float>(GBPixelVfxJobLocal::ReadNumber(Pixel, TEXT("LevelsPercentile"), Fresh.LevelsPercentile));
	Fresh.Gamma = static_cast<float>(GBPixelVfxJobLocal::ReadNumber(Pixel, TEXT("Gamma"), Fresh.Gamma));
	Fresh.Hysteresis = static_cast<float>(GBPixelVfxJobLocal::ReadNumber(Pixel, TEXT("Hysteresis"), Fresh.Hysteresis));
	Fresh.MinIslandPixels = static_cast<int32>(GBPixelVfxJobLocal::ReadNumber(Pixel, TEXT("MinIslandPixels"), Fresh.MinIslandPixels));
	Fresh.bFillHoles = GBPixelVfxJobLocal::ReadBool(Pixel, TEXT("FillHoles"), Fresh.bFillHoles);
	Fresh.MinColorClusterPixels = static_cast<int32>(GBPixelVfxJobLocal::ReadNumber(Pixel, TEXT("MinColorClusterPixels"), Fresh.MinColorClusterPixels));
	Fresh.FadeOutFrames = static_cast<int32>(GBPixelVfxJobLocal::ReadNumber(Pixel, TEXT("FadeOutFrames"), Fresh.FadeOutFrames));
	Fresh.FadeOutMode = GBPixelVfxJobLocal::ReadString(Pixel, TEXT("FadeOutMode"), Fresh.FadeOutMode);
	Fresh.Outline = GBPixelVfxJobLocal::ReadString(Pixel, TEXT("Outline"), Fresh.Outline);

	const TSharedPtr<FJsonObject> Light = Root->GetObjectField(TEXT("Light"));
	Fresh.bLightEnabled = GBPixelVfxJobLocal::ReadBool(Light, TEXT("Enabled"), Fresh.bLightEnabled);
	const FString LightHex = GBPixelVfxJobLocal::ReadString(Light, TEXT("Color"), Fresh.LightColorHex);
	FLinearColor ParsedColor;
	if (GBPixelVfxJobLocal::ParseHex(LightHex, ParsedColor))
	{
		Fresh.LightColorHex = LightHex;
		Fresh.LightColor = ParsedColor;
	}
	Fresh.LightIntensity = static_cast<float>(GBPixelVfxJobLocal::ReadNumber(Light, TEXT("Intensity"), Fresh.LightIntensity));
	Fresh.LightRadiusCm = static_cast<float>(GBPixelVfxJobLocal::ReadNumber(Light, TEXT("RadiusCm"), Fresh.LightRadiusCm));
	Fresh.bLightCastShadows = GBPixelVfxJobLocal::ReadBool(Light, TEXT("CastShadows"), Fresh.bLightCastShadows);
	Fresh.LightTrackMode = GBPixelVfxJobLocal::ReadString(Light, TEXT("TrackMode"), Fresh.LightTrackMode);

	const TSharedPtr<FJsonObject> World = Root->GetObjectField(TEXT("World"));
	Fresh.Facing = GBPixelVfxJobLocal::ReadString(World, TEXT("Facing"), Fresh.Facing);
	Fresh.Glow = static_cast<float>(GBPixelVfxJobLocal::ReadNumber(World, TEXT("Glow"), Fresh.Glow));
	Fresh.bLit = GBPixelVfxJobLocal::ReadBool(World, TEXT("Lit"), Fresh.bLit);

	*this = Fresh;
	return true;
}
