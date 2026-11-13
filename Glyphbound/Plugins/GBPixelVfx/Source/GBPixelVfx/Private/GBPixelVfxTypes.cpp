// FGBPixelVfxClip: JSON read/write plus the pure playback helpers (unit-testable, no engine objects).
#include "GBPixelVfxTypes.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace GBPixelVfxClipLocal
{
	EGBPixelVfxFacing FacingFromString(const FString& Text)
	{
		if (Text.Equals(TEXT("Billboard"), ESearchCase::IgnoreCase))
		{
			return EGBPixelVfxFacing::Billboard;
		}
		if (Text.Equals(TEXT("Ground"), ESearchCase::IgnoreCase))
		{
			return EGBPixelVfxFacing::Ground;
		}
		if (Text.Equals(TEXT("Fixed"), ESearchCase::IgnoreCase))
		{
			return EGBPixelVfxFacing::Fixed;
		}
		return EGBPixelVfxFacing::Upright;
	}

	FString FacingToString(EGBPixelVfxFacing Facing)
	{
		switch (Facing)
		{
		case EGBPixelVfxFacing::Billboard: return TEXT("Billboard");
		case EGBPixelVfxFacing::Ground: return TEXT("Ground");
		case EGBPixelVfxFacing::Fixed: return TEXT("Fixed");
		default: return TEXT("Upright");
		}
	}

	int32 HexDigit(TCHAR C)
	{
		if (C >= TEXT('0') && C <= TEXT('9')) return C - TEXT('0');
		if (C >= TEXT('a') && C <= TEXT('f')) return 10 + C - TEXT('a');
		if (C >= TEXT('A') && C <= TEXT('F')) return 10 + C - TEXT('A');
		return -1;
	}

	// "#RRGGBB" (the '#' is optional) -> linear color. Returns white when the text is broken.
	FLinearColor ColorFromHex(const FString& Hex)
	{
		FString Text = Hex;
		Text.TrimStartAndEndInline();
		if (Text.StartsWith(TEXT("#")))
		{
			Text.RightChopInline(1);
		}
		if (Text.Len() != 6)
		{
			return FLinearColor::White;
		}
		uint8 Bytes[3] = { 255, 255, 255 };
		for (int32 i = 0; i < 3; ++i)
		{
			const int32 Hi = HexDigit(Text[i * 2]);
			const int32 Lo = HexDigit(Text[i * 2 + 1]);
			if (Hi < 0 || Lo < 0)
			{
				return FLinearColor::White;
			}
			Bytes[i] = static_cast<uint8>(Hi * 16 + Lo);
		}
		return FLinearColor(FColor(Bytes[0], Bytes[1], Bytes[2]));
	}

	FString ColorToHex(const FLinearColor& Color)
	{
		const FColor Srgb = Color.ToFColor(true);
		return FString::Printf(TEXT("#%02X%02X%02X"), Srgb.R, Srgb.G, Srgb.B);
	}
}

FVector4f FGBPixelVfxClip::FrameRect(const FGBPixelVfxClip& Clip, int32 FrameIndex)
{
	if (Clip.Columns <= 0 || Clip.FrameWidth <= 0 || Clip.FrameHeight <= 0 || Clip.Rows() <= 0)
	{
		return FVector4f(0.f, 0.f, 1.f, 1.f);
	}
	const int32 Clamped = FMath::Clamp(FrameIndex, 0, Clip.FrameCount - 1);
	const float AtlasW = static_cast<float>(Clip.Columns * Clip.FrameWidth);
	const float AtlasH = static_cast<float>(Clip.Rows() * Clip.FrameHeight);
	const int32 Col = Clamped % Clip.Columns;
	const int32 Row = Clamped / Clip.Columns;
	return FVector4f(
		Col * Clip.FrameWidth / AtlasW,
		Row * Clip.FrameHeight / AtlasH,
		Clip.FrameWidth / AtlasW,
		Clip.FrameHeight / AtlasH);
}

int32 FGBPixelVfxClip::FrameAtTime(const FGBPixelVfxClip& Clip, float Seconds, bool& bOutFinished)
{
	bOutFinished = false;
	if (Clip.FrameCount <= 0 || Clip.Fps <= 0)
	{
		bOutFinished = true;
		return 0;
	}
	const int32 Stepped = FMath::FloorToInt(Seconds * static_cast<float>(Clip.Fps));
	if (Clip.bLoop)
	{
		const int32 Wrapped = Stepped % Clip.FrameCount;
		return Wrapped >= 0 ? Wrapped : Wrapped + Clip.FrameCount;
	}
	// One-shots show every frame for exactly one frame duration: the last frame is
	// not finished until FrameCount / Fps seconds have elapsed.
	if (Stepped >= Clip.FrameCount)
	{
		bOutFinished = true;
		return Clip.FrameCount - 1;
	}
	return FMath::Max(0, Stepped);
}

void FGBPixelVfxClip::ComputeQuadAndLightPlacement(const FGBPixelVfxClip& Clip, float SizeScale,
	const FVector& ComponentLocation, const FQuat& R,
	FVector& OutQuadLocation, FVector& OutLightLocation)
{
	const float W = Clip.WorldWidthCm * SizeScale;
	const float H = Clip.WorldHeightCm * SizeScale;
	// Quad-local offset (cm) of the pivot point from the quad center.
	const FVector PivotLocal((Clip.PivotU - 0.5f) * W, (0.5f - Clip.PivotV) * H, 0.f);
	// Quad-local offset (cm) from the pivot point to the light anchor.
	const FVector AnchorOffset((Clip.Light.AnchorU - Clip.PivotU) * W,
		(Clip.PivotV - Clip.Light.AnchorV) * H, 0.f);
	OutQuadLocation = ComponentLocation - R.RotateVector(PivotLocal);
	// 10 cm toward the camera (along the quad normal) so the light is not inside the quad plane.
	OutLightLocation = ComponentLocation + R.RotateVector(AnchorOffset)
		+ R.RotateVector(FVector(0.f, 0.f, 10.f));
}

bool FGBPixelVfxClip::LoadFromJsonString(const FString& Text)
{
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return false;
	}

	FGBPixelVfxClip Fresh;
	FString IdText;
	if (Root->TryGetStringField(TEXT("Id"), IdText))
	{
		Fresh.Id = FName(*IdText);
	}
	Root->TryGetStringField(TEXT("Atlas"), Fresh.AtlasFile);
	Root->TryGetNumberField(TEXT("FrameWidth"), Fresh.FrameWidth);
	Root->TryGetNumberField(TEXT("FrameHeight"), Fresh.FrameHeight);
	Root->TryGetNumberField(TEXT("Columns"), Fresh.Columns);
	Root->TryGetNumberField(TEXT("FrameCount"), Fresh.FrameCount);
	Root->TryGetNumberField(TEXT("Fps"), Fresh.Fps);
	Fresh.FrameWidth = FMath::Max(0, Fresh.FrameWidth);
	Fresh.FrameHeight = FMath::Max(0, Fresh.FrameHeight);
	Fresh.Columns = FMath::Max(0, Fresh.Columns);
	Fresh.FrameCount = FMath::Max(0, Fresh.FrameCount);
	Fresh.Fps = FMath::Max(0, Fresh.Fps);
	Root->TryGetBoolField(TEXT("Loop"), Fresh.bLoop);

	double WorldW = 0.0;
	double WorldH = 0.0;
	if (Root->TryGetNumberField(TEXT("WorldWidthCm"), WorldW))
	{
		Fresh.WorldWidthCm = static_cast<float>(WorldW);
	}
	if (Root->TryGetNumberField(TEXT("WorldHeightCm"), WorldH))
	{
		Fresh.WorldHeightCm = static_cast<float>(WorldH);
	}
	double PivotUVal = 0.5;
	double PivotVVal = 0.5;
	if (Root->TryGetNumberField(TEXT("PivotU"), PivotUVal))
	{
		Fresh.PivotU = static_cast<float>(PivotUVal);
	}
	if (Root->TryGetNumberField(TEXT("PivotV"), PivotVVal))
	{
		Fresh.PivotV = static_cast<float>(PivotVVal);
	}

	FString FacingText;
	if (Root->TryGetStringField(TEXT("Facing"), FacingText))
	{
		Fresh.Facing = GBPixelVfxClipLocal::FacingFromString(FacingText);
	}
	double GlowVal = 0.0;
	if (Root->TryGetNumberField(TEXT("Glow"), GlowVal))
	{
		Fresh.Glow = static_cast<float>(GlowVal);
	}
	Root->TryGetBoolField(TEXT("Lit"), Fresh.bLit);

	const TSharedPtr<FJsonObject>* LightField = nullptr;
	if (Root->TryGetObjectField(TEXT("Light"), LightField) && LightField && LightField->IsValid())
	{
		const TSharedPtr<FJsonObject>& LightJson = *LightField;
		LightJson->TryGetBoolField(TEXT("Enabled"), Fresh.Light.bEnabled);
		if (LightJson->TryGetStringField(TEXT("Color"), Fresh.Light.ColorHex))
		{
			Fresh.Light.Color = GBPixelVfxClipLocal::ColorFromHex(Fresh.Light.ColorHex);
		}
		double Intensity = 0.0;
		if (LightJson->TryGetNumberField(TEXT("Intensity"), Intensity))
		{
			Fresh.Light.Intensity = static_cast<float>(Intensity);
		}
		double Radius = 0.0;
		if (LightJson->TryGetNumberField(TEXT("RadiusCm"), Radius))
		{
			Fresh.Light.RadiusCm = static_cast<float>(Radius);
		}
		LightJson->TryGetBoolField(TEXT("CastShadows"), Fresh.Light.bCastShadows);
		double AnchorU = 0.5;
		double AnchorV = 0.5;
		if (LightJson->TryGetNumberField(TEXT("AnchorU"), AnchorU))
		{
			Fresh.Light.AnchorU = static_cast<float>(AnchorU);
		}
		if (LightJson->TryGetNumberField(TEXT("AnchorV"), AnchorV))
		{
			Fresh.Light.AnchorV = static_cast<float>(AnchorV);
		}
		LightJson->TryGetStringField(TEXT("TrackMode"), Fresh.Light.TrackMode);
		const TArray<TSharedPtr<FJsonValue>>* Track = nullptr;
		if (LightJson->TryGetArrayField(TEXT("Track"), Track) && Track)
		{
			Fresh.Light.Track.Reset(Track->Num());
			for (const TSharedPtr<FJsonValue>& Entry : *Track)
			{
				Fresh.Light.Track.Add(static_cast<float>(Entry->AsNumber()));
			}
		}
	}

	const TSharedPtr<FJsonObject>* SourceObj = nullptr;
	if (Root->TryGetObjectField(TEXT("Source"), SourceObj) && SourceObj && SourceObj->IsValid())
	{
		const TSharedPtr<FJsonObject>& Source = *SourceObj;
		Source->TryGetStringField(TEXT("System"), Fresh.SourceSystem);
		double Duration = 0.0;
		if (Source->TryGetNumberField(TEXT("DurationSeconds"), Duration))
		{
			Fresh.SourceDurationSeconds = static_cast<float>(Duration);
		}
		Source->TryGetNumberField(TEXT("SimFps"), Fresh.SourceSimFps);
		Source->TryGetStringField(TEXT("EngineVersion"), Fresh.EngineVersion);
		Source->TryGetStringField(TEXT("BakedUtc"), Fresh.BakedUtc);
	}

	if (Fresh.Id.IsNone() || Fresh.FrameCount <= 0 || Fresh.FrameWidth <= 0 || Fresh.FrameHeight <= 0
		|| Fresh.Columns <= 0 || Fresh.AtlasFile.IsEmpty())
	{
		return false;
	}
	*this = Fresh;
	return true;
}

bool FGBPixelVfxClip::SaveToJsonString(FString& OutText) const
{
	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("Id"), Id.ToString());
	Root->SetStringField(TEXT("Atlas"), AtlasFile);
	Root->SetNumberField(TEXT("FrameWidth"), FrameWidth);
	Root->SetNumberField(TEXT("FrameHeight"), FrameHeight);
	Root->SetNumberField(TEXT("Columns"), Columns);
	Root->SetNumberField(TEXT("FrameCount"), FrameCount);
	Root->SetNumberField(TEXT("Fps"), Fps);
	Root->SetBoolField(TEXT("Loop"), bLoop);
	Root->SetNumberField(TEXT("WorldWidthCm"), WorldWidthCm);
	Root->SetNumberField(TEXT("WorldHeightCm"), WorldHeightCm);
	Root->SetNumberField(TEXT("PivotU"), PivotU);
	Root->SetNumberField(TEXT("PivotV"), PivotV);
	Root->SetStringField(TEXT("Facing"), GBPixelVfxClipLocal::FacingToString(Facing));
	Root->SetNumberField(TEXT("Glow"), Glow);
	Root->SetBoolField(TEXT("Lit"), bLit);

	const TSharedRef<FJsonObject> LightJson = MakeShared<FJsonObject>();
	LightJson->SetBoolField(TEXT("Enabled"), Light.bEnabled);
	LightJson->SetStringField(TEXT("Color"), Light.ColorHex);
	LightJson->SetNumberField(TEXT("Intensity"), Light.Intensity);
	LightJson->SetNumberField(TEXT("RadiusCm"), Light.RadiusCm);
	LightJson->SetBoolField(TEXT("CastShadows"), Light.bCastShadows);
	LightJson->SetNumberField(TEXT("AnchorU"), Light.AnchorU);
	LightJson->SetNumberField(TEXT("AnchorV"), Light.AnchorV);
	LightJson->SetStringField(TEXT("TrackMode"), Light.TrackMode);
	TArray<TSharedPtr<FJsonValue>> TrackValues;
	for (float Entry : Light.Track)
	{
		TrackValues.Add(MakeShared<FJsonValueNumber>(Entry));
	}
	LightJson->SetArrayField(TEXT("Track"), TrackValues);
	Root->SetObjectField(TEXT("Light"), LightJson);

	const TSharedRef<FJsonObject> Source = MakeShared<FJsonObject>();
	Source->SetStringField(TEXT("System"), SourceSystem);
	Source->SetNumberField(TEXT("DurationSeconds"), SourceDurationSeconds);
	Source->SetNumberField(TEXT("SimFps"), SourceSimFps);
	Source->SetStringField(TEXT("EngineVersion"), EngineVersion);
	Source->SetStringField(TEXT("BakedUtc"), BakedUtc);
	Root->SetObjectField(TEXT("Source"), Source);

	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutText);
	return FJsonSerializer::Serialize(Root, Writer);
}
