// Painting layer of AGBHUD shared by both skins: primitives (rects, polygons, glows), the pixel font of the RPG
// skin, the condensed vector type of the cyberpunk skin, and the skin-neutral text roles the menus use.
#include "UI/GBHUD.h"

#include "UI/GBHUDShared.h"
#include "UI/GBPixelFont.h"
#include "UI/GBPixelSpriteLibrary.h"

#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineFontServices.h"
#include "Fonts/FontMeasure.h"
#include "Fonts/SlateFontInfo.h"
#include "Misc/Paths.h"

namespace GBPaintLocal
{
	/** Deterministic 0..1 noise from integer coordinates (art generation). */
	float Hash01(int32 X, int32 Y, int32 Seed)
	{
		uint32 H = (uint32)X * 374761393u + (uint32)Y * 668265263u + (uint32)Seed * 2246822519u;
		H = (H ^ (H >> 13)) * 1274126177u;
		H ^= H >> 16;
		return (H & 0xFFFF) / 65535.f;
	}

	FColor Shade(const FColor& Base, float Amount)
	{
		auto Channel = [Amount](uint8 C) { return (uint8)FMath::Clamp(FMath::RoundToInt(C * (1.f + Amount)), 0, 255); };
		return FColor(Channel(Base.R), Channel(Base.G), Channel(Base.B), Base.A);
	}

	/**
	 * Dark obsidian blocks, 16 x 8 art pixels, two courses offset by half a block (tile 32 x 16), 1-pixel mortar,
	 * light from the top left, a little wear noise (VisualStyleGuide 5: noise only on rough materials).
	 */
	void BuildStone(TArray<FColor>& Pixels, int32& OutW, int32& OutH)
	{
		const int32 W = 32;
		const int32 H = 16;
		OutW = W;
		OutH = H;
		Pixels.SetNum(W * H);
		const FColor Mortar = FColor::FromHex(TEXT("101017"));
		const FColor Bases[3] = { FColor::FromHex(TEXT("252734")), FColor::FromHex(TEXT("2A2B37")), FColor::FromHex(TEXT("22232E")) };
		const FColor Light = FColor::FromHex(TEXT("3B3C4C"));
		const FColor Dark = FColor::FromHex(TEXT("1A1A22"));
		const FColor Violet = FColor::FromHex(TEXT("3A3346"));
		for (int32 Y = 0; Y < H; ++Y)
		{
			const int32 Course = Y / 8;
			const int32 LY = Y % 8;
			for (int32 X = 0; X < W; ++X)
			{
				const int32 Shift = Course == 0 ? 0 : 8;
				const int32 BX = (X + Shift) % W;
				const int32 Brick = BX / 16 + Course * 2;
				const int32 LX = BX % 16;
				FColor C;
				if (LY == 7 || LX == 15)
				{
					C = Mortar;
				}
				else if (LY == 0 || LX == 0)
				{
					C = Light;
				}
				else if (LY == 6 || LX == 14)
				{
					C = Dark;
				}
				else
				{
					C = Bases[Brick % 3];
					const float N = Hash01(X, Y, 7);
					if (N > 0.93f)
					{
						C = Shade(C, 0.22f);
					}
					else if (N < 0.07f)
					{
						C = Shade(C, -0.25f);
					}
					else if (N > 0.86f && Hash01(Brick, 3, 11) > 0.5f)
					{
						C = Violet;
					}
				}
				Pixels[Y * W + X] = C;
			}
		}
	}

	UTexture2D* MakeSmoothTexture(const TArray<FColor>& Pixels, int32 W, int32 H, const TCHAR* Name)
	{
		UTexture2D* Texture = UGBPixelSpriteLibrary::CreatePixelTexture(Pixels, W, H, Name, false);
		if (Texture)
		{
			Texture->Filter = TF_Bilinear;
			Texture->SRGB = false;
			Texture->UpdateResource();
		}
		return Texture;
	}

	const FSlateFontInfo& CyFont(uint8 Face)
	{
		static TMap<uint8, FSlateFontInfo> Cache;
		if (const FSlateFontInfo* Found = Cache.Find(Face))
		{
			return *Found;
		}
		const TCHAR* File = TEXT("Roboto-Regular.ttf");
		switch (Face)
		{
		case 0: File = TEXT("Roboto-BoldCondensed.ttf"); break;
		case 1: File = TEXT("Roboto-Regular.ttf"); break;
		case 2: File = TEXT("Roboto-Medium.ttf"); break;
		case 3: File = TEXT("Roboto-Light.ttf"); break;
		case 4: File = TEXT("Roboto-Black.ttf"); break;
		case 5: File = TEXT("DroidSansMono.ttf"); break;
		default: break;
		}
		const FString Path = FPaths::EngineContentDir() / TEXT("Slate/Fonts") / File;
		return Cache.Add(Face, FSlateFontInfo(Path, 16.f));
	}
}

// ======================================================================== basics

double AGBHUD::GetRealTime() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetRealTimeSeconds() : 0.0;
}

float AGBHUD::UIScale() const
{
	return Canvas ? FMath::Max(0.1f, Canvas->ClipY / 1080.f) : 1.f;
}

float AGBHUD::ArtPixel() const
{
	return Canvas ? FMath::Max(2.f, (float)FMath::RoundToInt(Canvas->ClipY / 270.f)) : 4.f;
}

void AGBHUD::EnsureArt()
{
	using namespace GBPaintLocal;
	if (!PixelFont)
	{
		PixelFont = NewObject<UGBPixelFont>(this);
		PixelFont->Init();
	}
	if (!RpgStoneTexture)
	{
		TArray<FColor> Pixels;
		int32 W = 0;
		int32 H = 0;
		BuildStone(Pixels, W, H);
		RpgStoneTexture = UGBPixelSpriteLibrary::CreatePixelTexture(Pixels, W, H, TEXT("RpgStone"), true);
	}
	if (!GlowTexture)
	{
		const int32 N = 64;
		TArray<FColor> Pixels;
		Pixels.SetNum(N * N);
		for (int32 Y = 0; Y < N; ++Y)
		{
			for (int32 X = 0; X < N; ++X)
			{
				const float DX = (X + 0.5f) / N * 2.f - 1.f;
				const float DY = (Y + 0.5f) / N * 2.f - 1.f;
				const float R = FMath::Clamp(FMath::Sqrt(DX * DX + DY * DY), 0.f, 1.f);
				const float A = FMath::Square(1.f - R) * (1.f - R * 0.3f);
				Pixels[Y * N + X] = FColor(255, 255, 255, (uint8)FMath::RoundToInt(A * 255.f));
			}
		}
		GlowTexture = MakeSmoothTexture(Pixels, N, N, TEXT("HudGlow"));
	}
	if (!RampTexture)
	{
		const int32 N = 64;
		TArray<FColor> Pixels;
		Pixels.SetNum(N);
		for (int32 X = 0; X < N; ++X)
		{
			Pixels[X] = FColor(255, 255, 255, (uint8)FMath::RoundToInt((1.f - X / (float)(N - 1)) * 255.f));
		}
		RampTexture = MakeSmoothTexture(Pixels, N, 1, TEXT("HudRamp"));
	}
}

// ======================================================================== primitives

void AGBHUD::FillRect(float X, float Y, float W, float H, const FLinearColor& Color)
{
	if (W <= 0.f || H <= 0.f || Color.A <= 0.f)
	{
		return;
	}
	DrawRect(Color, FMath::RoundToFloat(X), FMath::RoundToFloat(Y), FMath::RoundToFloat(X + W) - FMath::RoundToFloat(X), FMath::RoundToFloat(Y + H) - FMath::RoundToFloat(Y));
}

void AGBHUD::DrawOutline(float X, float Y, float W, float H, const FLinearColor& Color, float Thickness)
{
	const float T = FMath::Max(1.f, FMath::RoundToFloat(Thickness));
	FillRect(X, Y, W, T, Color);
	FillRect(X, Y + H - T, W, T, Color);
	FillRect(X, Y + T, T, H - 2.f * T, Color);
	FillRect(X + W - T, Y + T, T, H - 2.f * T, Color);
}

void AGBHUD::FillPoly(const TArray<FVector2D>& Points, const FLinearColor& Color)
{
	if (!Canvas || Points.Num() < 3 || Color.A <= 0.f)
	{
		return;
	}
	TArray<FCanvasUVTri> Tris;
	Tris.Reserve(Points.Num() - 2);
	for (int32 i = 1; i + 1 < Points.Num(); ++i)
	{
		FCanvasUVTri Tri;
		Tri.V0_Pos = Points[0];
		Tri.V1_Pos = Points[i];
		Tri.V2_Pos = Points[i + 1];
		Tri.V0_UV = Tri.V1_UV = Tri.V2_UV = FVector2D::ZeroVector;
		Tri.V0_Color = Tri.V1_Color = Tri.V2_Color = Color;
		Tris.Add(Tri);
	}
	FCanvasTriangleItem Item(Tris, GWhiteTexture);
	Item.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Item);
}

void AGBHUD::StrokePoly(const TArray<FVector2D>& Points, const FLinearColor& Color, float Thickness)
{
	if (Points.Num() < 2 || Color.A <= 0.f)
	{
		return;
	}
	for (int32 i = 0; i < Points.Num(); ++i)
	{
		const FVector2D& A = Points[i];
		const FVector2D& B = Points[(i + 1) % Points.Num()];
		DrawLine(A.X, A.Y, B.X, B.Y, Color, Thickness);
	}
}

void AGBHUD::DrawGlow(float CX, float CY, float RX, float RY, const FLinearColor& Color)
{
	EnsureArt();
	if (!GlowTexture || Color.A <= 0.f)
	{
		return;
	}
	DrawTexture(GlowTexture, CX - RX, CY - RY, 2.f * RX, 2.f * RY, 0.f, 0.f, 1.f, 1.f, Color, BLEND_Translucent);
}

void AGBHUD::DrawFade(float X, float Y, float W, float H, const FLinearColor& Color, bool bFadeRight)
{
	EnsureArt();
	if (!RampTexture || W <= 0.f || H <= 0.f)
	{
		return;
	}
	if (bFadeRight)
	{
		DrawTexture(RampTexture, X, Y, W, H, 0.f, 0.f, 1.f, 1.f, Color, BLEND_Translucent);
	}
	else
	{
		DrawTexture(RampTexture, X, Y, W, H, 1.f, 0.f, -1.f, 1.f, Color, BLEND_Translucent);
	}
}

void AGBHUD::DrawSprite(UTexture2D* Texture, float X, float Y, float PixelScale, const FLinearColor& Tint)
{
	if (!Texture)
	{
		return;
	}
	const float W = Texture->GetSizeX() * PixelScale;
	const float H = Texture->GetSizeY() * PixelScale;
	DrawTexture(Texture, FMath::RoundToFloat(X), FMath::RoundToFloat(Y), W, H, 0.f, 0.f, 1.f, 1.f, Tint, BLEND_Translucent);
}

void AGBHUD::DrawSpriteTile(UTexture2D* Texture, float X, float Y, float W, float H, float U, float V, float UL, float VL, const FLinearColor& Tint)
{
	if (!Texture || W <= 0.f || H <= 0.f)
	{
		return;
	}
	DrawTexture(Texture, FMath::RoundToFloat(X), FMath::RoundToFloat(Y), W, H, U, V, UL, VL, Tint, BLEND_Translucent);
}

bool AGBHUD::DrawSpriteFit(FName SpriteId, float X, float Y, float W, float H, const FLinearColor& Tint, bool bSilhouette)
{
	UGBPixelSpriteLibrary* Sprites = UGBPixelSpriteLibrary::Get(this);
	if (!Sprites || SpriteId == NAME_None || !Sprites->HasSprite(SpriteId))
	{
		return false;
	}
	UTexture2D* Texture = Sprites->GetSprite(SpriteId);
	if (!Texture)
	{
		return false;
	}
	const float SW = Texture->GetSizeX();
	const float SH = Texture->GetSizeY();
	float Scale = FMath::Min(W / SW, H / SH);
	// Whole pixels when there is room; a smaller fractional scale only for sprites larger than the box.
	if (Scale >= 1.f)
	{
		Scale = FMath::FloorToFloat(Scale);
	}
	const float DW = SW * Scale;
	const float DH = SH * Scale;
	const float DX = FMath::RoundToFloat(X + (W - DW) * 0.5f);
	const float DY = FMath::RoundToFloat(Y + (H - DH) * 0.5f);
	if (bSilhouette)
	{
		// A soft glow under the icon (holographic look of the cyberpunk skin).
		DrawGlow(DX + DW * 0.5f, DY + DH * 0.5f, DW * 0.7f, DH * 0.7f, GBAlpha(Tint, 0.25f));
	}
	DrawTexture(Texture, DX, DY, DW, DH, 0.f, 0.f, 1.f, 1.f, Tint, BLEND_Translucent);
	return true;
}

// ======================================================================== engine font (debug overlays)

void AGBHUD::DrawLabel(const FString& Text, const FLinearColor& Color, float X, float Y, float SizePx, float AlignX, bool bShadow, bool bLarge)
{
	if (!Canvas || !GEngine || Text.IsEmpty())
	{
		return;
	}
	UFont* Font = bLarge ? GEngine->GetLargeFont() : GEngine->GetMediumFont();
	if (!Font)
	{
		return;
	}
	const float Scale = SizePx * UIScale() / FMath::Max(1.f, Font->GetMaxCharHeight());
	float W = 0.f;
	float H = 0.f;
	GetTextSize(Text, W, H, Font, Scale);
	const float DrawX = FMath::RoundToFloat(X - W * AlignX);
	const float DrawY = FMath::RoundToFloat(Y);
	if (bShadow)
	{
		const float Off = FMath::Max(1.f, 2.f * UIScale());
		DrawText(Text, FLinearColor(0.f, 0.f, 0.f, 0.8f * Color.A), DrawX + Off, DrawY + Off, Font, Scale);
	}
	DrawText(Text, Color, DrawX, DrawY, Font, Scale);
}

FVector2D AGBHUD::MeasureLabel(const FString& Text, float SizePx, bool bLarge) const
{
	if (!Canvas || !GEngine || Text.IsEmpty())
	{
		return FVector2D::ZeroVector;
	}
	UFont* Font = bLarge ? GEngine->GetLargeFont() : GEngine->GetMediumFont();
	if (!Font)
	{
		return FVector2D::ZeroVector;
	}
	const float Scale = SizePx * UIScale() / FMath::Max(1.f, Font->GetMaxCharHeight());
	float W = 0.f;
	float H = 0.f;
	GetTextSize(Text, W, H, Font, Scale);
	return FVector2D(W, H);
}

// ======================================================================== pixel font

FVector2D AGBHUD::MeasurePixelText(const FString& Text, int32 Scale) const
{
	if (!PixelFont || Text.IsEmpty())
	{
		return FVector2D::ZeroVector;
	}
	return FVector2D(PixelFont->MeasureWidth(Text) * Scale, UGBPixelFont::CapHeight * Scale);
}

FVector2D AGBHUD::DrawPixelText(const FString& Text, float X, float Y, const FPixelTextStyle& Style, float AlignX)
{
	EnsureArt();
	if (!Canvas || !PixelFont || Text.IsEmpty() || Style.Color.A <= 0.f)
	{
		return FVector2D::ZeroVector;
	}
	const int32 Scale = FMath::Max(1, Style.Scale);
	const FVector2D Size = MeasurePixelText(Text, Scale);
	const float X0 = FMath::RoundToFloat(X - Size.X * AlignX);
	const float Y0 = FMath::RoundToFloat(Y);
	const FIntPoint Atlas = PixelFont->GetAtlasSize();
	if (Atlas.X <= 0 || Atlas.Y <= 0)
	{
		return Size;
	}
	UTexture2D* InkAtlas = PixelFont->GetInkAtlas(Style.Ink ? *Style.Ink : FGBPixelInk::Flat());
	UTexture2D* OutlineAtlas = PixelFont->GetOutlineAtlas();
	const FLinearColor InkTint = Style.Ink ? FLinearColor(1.f, 1.f, 1.f, Style.Color.A) : Style.Color;
	const float CellH = (float)(UGBPixelFont::CellRows + 2);

	auto Pass = [&](UTexture2D* Texture, const FLinearColor& Tint, float OffX, float OffY)
	{
		if (!Texture || Tint.A <= 0.f)
		{
			return;
		}
		float Pen = X0;
		for (const TCHAR Char : Text)
		{
			const UGBPixelFont::FGlyph* Glyph = PixelFont->FindGlyph(Char);
			if (!Glyph)
			{
				continue;
			}
			const float CellW = (float)(Glyph->Width + 2);
			DrawTexture(Texture, Pen - Scale + OffX, Y0 - Scale + OffY, CellW * Scale, CellH * Scale,
				Glyph->AtlasX / (float)Atlas.X, Glyph->AtlasY / (float)Atlas.Y, CellW / Atlas.X, CellH / Atlas.Y, Tint, BLEND_Translucent);
			Pen += (Glyph->Width + UGBPixelFont::Spacing) * Scale;
		}
	};

	const bool bOutline = Style.Outline.A > 0.f;
	if (Style.Shadow.A > 0.f)
	{
		const FLinearColor Shadow = GBAlpha(Style.Shadow, Style.Color.A);
		if (bOutline)
		{
			Pass(OutlineAtlas, Shadow, Scale, Scale);
		}
		Pass(InkAtlas, Shadow, Scale, Scale);
	}
	if (bOutline)
	{
		Pass(OutlineAtlas, GBAlpha(Style.Outline, Style.Color.A), 0.f, 0.f);
	}
	Pass(InkAtlas, InkTint, 0.f, 0.f);
	return Size;
}

// ======================================================================== cyberpunk type

FVector2D AGBHUD::MeasureCyText(const FString& Text, const FCyTextStyle& Style) const
{
	if (Text.IsEmpty())
	{
		return FVector2D::ZeroVector;
	}
	FSlateFontInfo Font = GBPaintLocal::CyFont((uint8)Style.Face);
	Font.Size = FMath::Max(1.f, Style.Px * UIScale() * 0.75f);
	Font.LetterSpacing = Style.Spacing;
	const TSharedPtr<FSlateFontMeasure> Measure = FEngineFontServices::IsInitialized() ? FEngineFontServices::Get().GetFontMeasure() : nullptr;
	if (!Measure.IsValid())
	{
		return FVector2D(Text.Len() * Style.Px * UIScale() * 0.5f, Style.Px * UIScale());
	}
	const FString Shown = Style.bUpper ? Text.ToUpper() : Text;
	return FVector2D(Measure->Measure(Shown, Font));
}

FVector2D AGBHUD::DrawCyText(const FString& Text, float X, float Y, const FCyTextStyle& Style, float AlignX)
{
	if (!Canvas || !RuntimeFont || Text.IsEmpty() || Style.Color.A <= 0.f)
	{
		return FVector2D::ZeroVector;
	}
	FSlateFontInfo Font = GBPaintLocal::CyFont((uint8)Style.Face);
	Font.Size = FMath::Max(1.f, Style.Px * UIScale() * 0.75f);
	Font.LetterSpacing = Style.Spacing;
	const FString Shown = Style.bUpper ? Text.ToUpper() : Text;
	const FVector2D Size = MeasureCyText(Text, Style);
	const FVector2D Pos(FMath::RoundToFloat(X - Size.X * AlignX), FMath::RoundToFloat(Y));
	const FText AsText = FText::FromString(Shown);

	auto Draw = [this, &Font, &AsText](const FVector2D& At, const FLinearColor& Color)
	{
		FCanvasTextItem Item(At, AsText, Font, Color);
		Item.Font = RuntimeFont;
		Item.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(Item);
	};
	const FLinearColor Shadow = Style.Shadow.A > 0.f ? Style.Shadow : (bForceTextShadow ? FLinearColor(0.f, 0.f, 0.f, 0.55f) : FLinearColor::Transparent);
	if (Shadow.A > 0.f)
	{
		const float Off = FMath::Max(1.f, FMath::RoundToFloat(2.f * UIScale()));
		Draw(Pos + FVector2D(Off, Off), GBAlpha(Shadow, Style.Color.A));
	}
	if (Style.Split > 0.f)
	{
		Draw(Pos + FVector2D(-Style.Split, 0.f), GBAlpha(GBCy::Cyan, 0.7f * Style.Color.A));
		Draw(Pos + FVector2D(Style.Split, 0.f), GBAlpha(GBCy::Red, 0.7f * Style.Color.A));
	}
	Draw(Pos, Style.Color);
	return Size;
}

// ======================================================================== text roles

FLinearColor AGBHUD::ToneColor(EGBHudSkin Skin, EGBTextTone Tone) const
{
	if (Skin == EGBHudSkin::Rpg90s)
	{
		switch (Tone)
		{
		case EGBTextTone::Normal: return GBRpg::Bone;
		case EGBTextTone::Accent: return GBRpg::Gold3;
		case EGBTextTone::Muted: return GBRpg::BoneDim;
		case EGBTextTone::Value: return GBRpg::Gold4;
		case EGBTextTone::Good: return GBRpg::Good;
		case EGBTextTone::Bad: return GBHex(TEXT("#FF5A40"));
		case EGBTextTone::Warn: return GBRpg::EmberLight;
		case EGBTextTone::Selected: return GBRpg::Gold4;
		case EGBTextTone::Disabled: return GBRpg::BoneDark;
		}
		return GBRpg::Bone;
	}
	switch (Tone)
	{
	case EGBTextTone::Normal: return GBCy::RedBright;
	case EGBTextTone::Accent: return GBCy::Red;
	case EGBTextTone::Muted: return GBCy::RedDim;
	case EGBTextTone::Value: return GBCy::Cyan;
	case EGBTextTone::Good: return GBCy::Green;
	case EGBTextTone::Bad: return GBCy::Red;
	case EGBTextTone::Warn: return GBCy::Yellow;
	case EGBTextTone::Selected: return GBCy::Pale;
	case EGBTextTone::Disabled: return GBCy::Grey;
	}
	return GBCy::RedBright;
}

namespace GBPaintLocal
{
	int32 PixScale(float ClipY, float AtTenEighty)
	{
		return FMath::Max(1, FMath::RoundToInt(AtTenEighty * ClipY / 1080.f));
	}

	int32 RpgScale(float ClipY, EGBTextSize Size)
	{
		switch (Size)
		{
		case EGBTextSize::Title: return PixScale(ClipY, 6.f);
		case EGBTextSize::Heading: return PixScale(ClipY, 4.f);
		case EGBTextSize::Body: return PixScale(ClipY, 3.f);
		case EGBTextSize::Small: return FMath::Max(2, PixScale(ClipY, 2.f));
		case EGBTextSize::Number: return PixScale(ClipY, 5.f);
		}
		return 2;
	}

}

namespace
{
	struct FCyRole
	{
		uint8 Face;
		float Px;
		int32 Spacing;
		bool bUpper;
	};
	FCyRole CyRole(EGBTextSize Size)
	{
		switch (Size)
		{
		case EGBTextSize::Title: return { 0, 54.f, 60, true };
		case EGBTextSize::Heading: return { 0, 30.f, 50, true };
		case EGBTextSize::Body: return { 1, 21.f, 0, false };
		case EGBTextSize::Small: return { 2, 16.f, 0, false };
		case EGBTextSize::Number: return { 0, 64.f, 0, false };
		}
		return { 1, 20.f, 0, false };
	}
}

FVector2D AGBHUD::UiMeasure(EGBHudSkin Skin, EGBTextSize Size, const FString& Text) const
{
	if (Skin == EGBHudSkin::Rpg90s)
	{
		return MeasurePixelText(Text, GBPaintLocal::RpgScale(Canvas ? Canvas->ClipY : 1080.f, Size));
	}
	const FCyRole CyR = CyRole(Size);
	FCyTextStyle Style;
	Style.Face = (ECyFace)CyR.Face;
	Style.Px = CyR.Px;
	Style.Spacing = CyR.Spacing;
	Style.bUpper = CyR.bUpper;
	return MeasureCyText(Text, Style);
}

float AGBHUD::UiLineHeight(EGBHudSkin Skin, EGBTextSize Size) const
{
	if (Skin == EGBHudSkin::Rpg90s)
	{
		const int32 Scale = GBPaintLocal::RpgScale(Canvas ? Canvas->ClipY : 1080.f, Size);
		return (UGBPixelFont::CellRows + 2) * Scale;
	}
	return CyRole(Size).Px * UIScale() * 1.32f;
}

FVector2D AGBHUD::UiText(EGBHudSkin Skin, EGBTextSize Size, EGBTextTone Tone, const FString& Text, float X, float Y, float AlignX, const FLinearColor* OverrideColor)
{
	if (Text.IsEmpty())
	{
		return FVector2D::ZeroVector;
	}
	const FLinearColor Color = OverrideColor ? *OverrideColor : ToneColor(Skin, Tone);
	if (Skin == EGBHudSkin::Rpg90s)
	{
		FPixelTextStyle Style;
		Style.Scale = GBPaintLocal::RpgScale(Canvas ? Canvas->ClipY : 1080.f, Size);
		Style.Color = Color;
		const bool bBig = Size == EGBTextSize::Title || Size == EGBTextSize::Heading || Size == EGBTextSize::Number;
		if (!OverrideColor)
		{
			switch (Tone)
			{
			case EGBTextTone::Normal: Style.Ink = bBig ? &GBRpg::InkGold : &GBRpg::InkBone; break;
			case EGBTextTone::Accent:
			case EGBTextTone::Selected: Style.Ink = &GBRpg::InkGold; break;
			case EGBTextTone::Warn: Style.Ink = &GBRpg::InkEmber; break;
			case EGBTextTone::Bad: Style.Ink = &GBRpg::InkBlood; break;
			case EGBTextTone::Value: Style.Ink = bBig ? &GBRpg::InkBone : nullptr; break;
			default: break;
			}
		}
		Style.Shadow = FLinearColor(0.f, 0.f, 0.f, 0.85f);
		if (bBig)
		{
			Style.Outline = GBRpg::Outline;
		}
		return DrawPixelText(Text, X, Y, Style, AlignX);
	}
	const FCyRole CyR = CyRole(Size);
	FCyTextStyle Style;
	Style.Face = (ECyFace)CyR.Face;
	Style.Px = CyR.Px;
	Style.Spacing = CyR.Spacing;
	Style.bUpper = CyR.bUpper;
	Style.Color = Color;
	return DrawCyText(Text, X, Y, Style, AlignX);
}

float AGBHUD::UiWrapped(EGBHudSkin Skin, EGBTextSize Size, EGBTextTone Tone, const FString& Text, float X, float Y, float MaxWidth, const FLinearColor* OverrideColor)
{
	TArray<FString> Words;
	Text.ParseIntoArrayWS(Words);
	const float LineH = UiLineHeight(Skin, Size);
	float CurY = Y;
	FString Line;
	for (const FString& Word : Words)
	{
		const FString Candidate = Line.IsEmpty() ? Word : Line + TEXT(" ") + Word;
		if (!Line.IsEmpty() && UiMeasure(Skin, Size, Candidate).X > MaxWidth)
		{
			UiText(Skin, Size, Tone, Line, X, CurY, 0.f, OverrideColor);
			CurY += LineH;
			Line = Word;
		}
		else
		{
			Line = Candidate;
		}
	}
	if (!Line.IsEmpty())
	{
		UiText(Skin, Size, Tone, Line, X, CurY, 0.f, OverrideColor);
		CurY += LineH;
	}
	return CurY - Y;
}
