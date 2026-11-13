// The 1990s first-person RPG skin of AGBHUD (GDD 13: staff and sword in hand): a carved status bar of dark stone,
// worn iron and gilt along the bottom, glass tubes for life and mana, gold-rimmed spell sockets with pixel icons,
// all laid out on the art-pixel grid (ArtPixel) so every edge is a whole sprite pixel, and the game's pixel font.
#include "UI/GBHUD.h"

#include "UI/GBHUDShared.h"
#include "UI/GBPixelFont.h"
#include "UI/GBPixelSpriteLibrary.h"
#include "Player/GBPlayerCharacter.h"
#include "Weapons/GBEquipmentComponent.h"
#include "Weapons/GBHeldItem.h"
#include "Spells/GBSpellbookComponent.h"
#include "Items/GBInventoryComponent.h"
#include "Progression/GBSkillTreeComponent.h"
#include "Combat/GBStatusEffectComponent.h"
#include "Stats/GBStatsComponent.h"
#include "Stats/GBAttributeComponent.h"
#include "Data/GBDataSubsystem.h"
#include "Core/GBStatNames.h"

#include "Engine/Canvas.h"
#include "Engine/Texture2D.h"

namespace GBRpgLocal
{
	FLinearColor Darker(const FLinearColor& C, float K) { return FLinearColor(C.R * K, C.G * K, C.B * K, C.A); }
	FLinearColor Lighter(const FLinearColor& C, float K) { return FMath::Lerp(C, FLinearColor(1.f, 1.f, 1.f, C.A), K); }
}

// ======================================================================== pieces

void AGBHUD::RpgStone(float X, float Y, float W, float H, const FLinearColor& Tint)
{
	EnsureArt();
	if (!RpgStoneTexture || W <= 0.f || H <= 0.f)
	{
		return;
	}
	const float P = ArtPixel();
	const float TileW = RpgStoneTexture->GetSizeX() * P;
	const float TileH = RpgStoneTexture->GetSizeY() * P;
	// UVs from the screen position: panels next to each other share one masonry.
	const float X0 = FMath::RoundToFloat(X);
	const float Y0 = FMath::RoundToFloat(Y);
	DrawTexture(RpgStoneTexture, X0, Y0, FMath::RoundToFloat(W), FMath::RoundToFloat(H), X0 / TileW, Y0 / TileH, W / TileW, H / TileH, Tint, BLEND_Translucent);
}

namespace GBRpgLocal
{
	/** One bevel ring T pixels thick: top and left in A, bottom and right in B. */
	void Ring(AGBHUD* Hud, TFunctionRef<void(float, float, float, float, const FLinearColor&)> Fill, float X, float Y, float W, float H, float T, const FLinearColor& A, const FLinearColor& B)
	{
		Fill(X, Y, W, T, A);
		Fill(X, Y + T, T, H - T, A);
		Fill(X + T, Y + H - T, W - T, T, B);
		Fill(X + W - T, Y + T, T, H - 2.f * T, B);
	}
}

void AGBHUD::RpgFrame(float X, float Y, float W, float H, ERpgFrame Kind)
{
	using namespace GBRpgLocal;
	const float P = ArtPixel();
	auto Fill = [this](float FX, float FY, float FW, float FH, const FLinearColor& C) { FillRect(FX, FY, FW, FH, C); };
	switch (Kind)
	{
	case ERpgFrame::Iron:
		Ring(this, Fill, X, Y, W, H, P, GBRpg::Outline, GBRpg::Outline);
		Ring(this, Fill, X + P, Y + P, W - 2 * P, H - 2 * P, P, GBRpg::Iron4, GBRpg::Iron1);
		Ring(this, Fill, X + 2 * P, Y + 2 * P, W - 4 * P, H - 4 * P, P, GBRpg::Iron2, GBRpg::Iron0);
		Ring(this, Fill, X + 3 * P, Y + 3 * P, W - 6 * P, H - 6 * P, P, GBRpg::Outline, GBRpg::Outline);
		break;
	case ERpgFrame::Gold:
		Ring(this, Fill, X, Y, W, H, P, GBRpg::Outline, GBRpg::Outline);
		Ring(this, Fill, X + P, Y + P, W - 2 * P, H - 2 * P, P, GBRpg::Gold3, GBRpg::Gold1);
		Ring(this, Fill, X + 2 * P, Y + 2 * P, W - 4 * P, H - 4 * P, P, GBRpg::Gold1, GBRpg::Gold3);
		break;
	case ERpgFrame::Inset:
		Ring(this, Fill, X, Y, W, H, P, GBRpg::Outline, GBRpg::Stone3);
		Ring(this, Fill, X + P, Y + P, W - 2 * P, H - 2 * P, P, GBRpg::StoneDeep, GBRpg::Stone2);
		break;
	case ERpgFrame::Bone:
		Ring(this, Fill, X, Y, W, H, P, GBRpg::Outline, GBRpg::Outline);
		Ring(this, Fill, X + P, Y + P, W - 2 * P, H - 2 * P, P, GBRpg::Bone, GBRpg::BoneDim);
		break;
	}
}

void AGBHUD::RpgRivet(float X, float Y)
{
	const float P = ArtPixel();
	FillRect(X + P, Y, 2 * P, P, GBRpg::Outline);
	FillRect(X, Y + P, P, 2 * P, GBRpg::Outline);
	FillRect(X + 3 * P, Y + P, P, 2 * P, GBRpg::Outline);
	FillRect(X + P, Y + 3 * P, 2 * P, P, GBRpg::Outline);
	FillRect(X + P, Y + P, P, P, GBRpg::Iron4);
	FillRect(X + 2 * P, Y + P, P, P, GBRpg::Iron3);
	FillRect(X + P, Y + 2 * P, P, P, GBRpg::Iron2);
	FillRect(X + 2 * P, Y + 2 * P, P, P, GBRpg::Iron1);
}

void AGBHUD::RpgGem(float CX, float CY, const FLinearColor& Color, bool bLit)
{
	using namespace GBRpgLocal;
	const float P = ArtPixel();
	// A 7 x 7 cut gem: outline, dark lower right, light upper left, one white specular pixel.
	static const TCHAR* Rows[] = { TEXT("..ooo.."), TEXT(".oLLBo."), TEXT("oLSBBDo"), TEXT("oLBBBDo"), TEXT("oBBBDDo"), TEXT(".oDDDo."), TEXT("..ooo..") };
	const FLinearColor Base = bLit ? Color : Darker(Color, 0.35f);
	const FLinearColor Light = bLit ? Lighter(Color, 0.45f) : Darker(Color, 0.5f);
	const FLinearColor Dark = Darker(Color, bLit ? 0.45f : 0.2f);
	const FLinearColor Spec = bLit ? GBRpg::Gold4 : Darker(Color, 0.6f);
	const float X0 = FMath::RoundToFloat(CX - 3.5f * P);
	const float Y0 = FMath::RoundToFloat(CY - 3.5f * P);
	if (bLit)
	{
		DrawGlow(CX, CY, 9.f * P, 9.f * P, GBAlpha(Color, 0.35f));
	}
	for (int32 Row = 0; Row < 7; ++Row)
	{
		for (int32 Col = 0; Col < 7; ++Col)
		{
			const TCHAR C = Rows[Row][Col];
			const FLinearColor* Pick = nullptr;
			switch (C)
			{
			case TEXT('o'): Pick = &GBRpg::Outline; break;
			case TEXT('L'): Pick = &Light; break;
			case TEXT('B'): Pick = &Base; break;
			case TEXT('D'): Pick = &Dark; break;
			case TEXT('S'): Pick = &Spec; break;
			default: break;
			}
			if (Pick)
			{
				FillRect(X0 + Col * P, Y0 + Row * P, P, P, *Pick);
			}
		}
	}
}

void AGBHUD::RpgWindow(float X, float Y, float W, float H)
{
	const float P = ArtPixel();
	// Drop shadow, stone, then the frame on top.
	FillRect(X + 2 * P, Y + 2 * P, W, H, FLinearColor(0.f, 0.f, 0.f, 0.5f));
	RpgStone(X, Y, W, H);
	// A warm light pool in the upper part of the window, darker corners.
	DrawGlow(X + W * 0.5f, Y + H * 0.3f, W * 0.6f, H * 0.6f, FLinearColor(0.35f, 0.22f, 0.12f, 0.12f));
	RpgFrame(X, Y, W, H, ERpgFrame::Iron);
	// Gold trim just inside the iron.
	const float In = 4 * P;
	DrawOutline(X + In, Y + In, W - 2 * In, H - 2 * In, GBRpg::Gold1, P);
	FillRect(X + In + P, Y + In + P, W - 2 * In - 2 * P, P, GBAlpha(GBRpg::Outline, 0.6f));
	// Corner bosses.
	const float B = 4 * P;
	RpgRivet(X + P * 0.f, Y + P * 0.f);
	RpgRivet(X + W - B, Y);
	RpgRivet(X, Y + H - B);
	RpgRivet(X + W - B, Y + H - B);
}

void AGBHUD::RpgWell(float X, float Y, float W, float H)
{
	FillRect(X, Y, W, H, GBAlpha(GBRpg::StoneDeep, 0.88f));
	RpgFrame(X, Y, W, H, ERpgFrame::Inset);
}

FBox2D AGBHUD::RpgPlaque(const FString& Title, float CX, float Y, int32 Scale)
{
	const float P = ArtPixel();
	const FVector2D Size = MeasurePixelText(Title, Scale);
	const float W = FMath::RoundToFloat((Size.X + 20.f * P) / P) * P;
	const float H = FMath::RoundToFloat((Size.Y + 8.f * P) / P) * P;
	const float X = FMath::RoundToFloat(CX - W * 0.5f);
	// Pointed ends: three steps on each side, then the body.
	for (int32 Step = 1; Step <= 3; ++Step)
	{
		const float Inset = Step * P;
		const float SH = H - 2.f * Step * P;
		FillRect(X - Step * P, Y + Inset, P, SH, Step == 3 ? GBRpg::Outline : GBRpg::Gold1);
		FillRect(X + W + (Step - 1) * P, Y + Inset, P, SH, Step == 3 ? GBRpg::Outline : GBRpg::Gold1);
	}
	FillRect(X, Y, W, H, GBRpg::Stone1);
	RpgStone(X + 2 * P, Y + 2 * P, W - 4 * P, H - 4 * P, FLinearColor(0.85f, 0.8f, 0.8f, 1.f));
	RpgFrame(X, Y, W, H, ERpgFrame::Gold);
	FPixelTextStyle Style;
	Style.Scale = Scale;
	Style.Ink = &GBRpg::InkGold;
	Style.Outline = GBRpg::Outline;
	Style.Shadow = FLinearColor(0.f, 0.f, 0.f, 0.7f);
	DrawPixelText(Title, CX, Y + (H - Size.Y) * 0.5f, Style, 0.5f);
	return FBox2D(FVector2D(X, Y), FVector2D(X + W, Y + H));
}

void AGBHUD::RpgDivider(float X, float Y, float W)
{
	const float P = ArtPixel();
	FillRect(X, Y, W, P, GBRpg::Gold1);
	FillRect(X, Y + P, W, P, GBAlpha(GBRpg::Outline, 0.8f));
	FillRect(X + 2 * P, Y - P, W - 4 * P, P, GBAlpha(GBRpg::Gold2, 0.35f));
	RpgGem(X + W * 0.5f, Y + 0.5f * P, GBRpg::Gold2, true);
}

void AGBHUD::RpgTube(float X, float Y, float W, float H, float Fraction, const FLinearColor Ramp[4], float Trail)
{
	const float P = ArtPixel();
	X = FMath::RoundToFloat(X);
	Y = FMath::RoundToFloat(Y);
	const float Cap = 3 * P;
	// Glass body.
	FillRect(X, Y, W, H, GBRpg::Outline);
	const float GX = X + Cap;
	const float GY = Y + P;
	const float GW = W - 2 * Cap;
	const float GH = H - 2 * P;
	FillRect(GX, GY, GW, GH, FMath::Lerp(GBRpg::StoneDeep, Ramp[0], 0.25f));
	// Liquid, snapped to whole art pixels.
	const float Frac = FMath::Clamp(Fraction, 0.f, 1.f);
	const float LW = FMath::RoundToFloat(GW * Frac / P) * P;
	if (Trail > Frac)
	{
		const float TW = FMath::RoundToFloat(GW * FMath::Clamp(Trail, 0.f, 1.f) / P) * P;
		FillRect(GX + LW, GY, TW - LW, GH, GBAlpha(Ramp[3], 0.55f));
	}
	if (LW > 0.f)
	{
		const int32 Rows = FMath::Max(1, FMath::RoundToInt(GH / P));
		for (int32 Row = 0; Row < Rows; ++Row)
		{
			const FLinearColor& C = Row == 0 ? Ramp[2] : (Row == Rows - 1 ? Ramp[0] : Ramp[1]);
			FillRect(GX, GY + Row * P, LW, P, C);
		}
		// Shine streak along the second row and a lit meniscus at the front.
		if (Rows > 3)
		{
			FillRect(GX + 2 * P, GY + P, FMath::Max(0.f, LW - 6 * P), P, GBAlpha(Ramp[3], 0.55f));
		}
		FillRect(GX + LW - P, GY, P, GH, Ramp[2]);
		// Two drifting motes inside the liquid.
		const float Time = (float)GetRealTime();
		for (int32 i = 0; i < 2; ++i)
		{
			const float Phase = FMath::Frac(Time * (0.11f + 0.07f * i) + i * 0.47f);
			const float MX = GX + FMath::RoundToFloat(Phase * (LW - 2 * P) / P) * P;
			const float MY = GY + (Rows > 3 ? (2 + i) * P : 0.f);
			if (MX + P < GX + LW - P)
			{
				FillRect(MX, MY, P, P, GBAlpha(Ramp[3], 0.8f));
			}
		}
	}
	// Glass glare across the whole tube.
	FillRect(GX, GY, GW, P, GBAlpha(FLinearColor::White, 0.10f));
	// Iron end caps.
	for (int32 Side = 0; Side < 2; ++Side)
	{
		const float CX = Side == 0 ? X : X + W - Cap;
		FillRect(CX, Y, Cap, H, GBRpg::Outline);
		FillRect(CX + P, Y + P, Cap - 2 * P, H - 2 * P, GBRpg::Iron2);
		FillRect(CX + P, Y + P, Cap - 2 * P, P, GBRpg::Iron4);
		FillRect(CX + P, Y + H - 2 * P, Cap - 2 * P, P, GBRpg::Iron0);
	}
}

void AGBHUD::RpgKeyCap(float X, float Y, const FString& Key, float* OutWidth)
{
	const float P = ArtPixel();
	const int32 Scale = FMath::Max(2, FMath::RoundToInt(P * 0.5f));
	const FVector2D Size = MeasurePixelText(Key, Scale);
	const float W = FMath::Max(FMath::CeilToFloat((Size.X + 6 * P) / P) * P, 9 * P);
	const float H = FMath::CeilToFloat((Size.Y + 5 * P) / P) * P;
	FillRect(X, Y, W, H, GBRpg::Outline);
	FillRect(X + P, Y + P, W - 2 * P, H - 2 * P, GBRpg::Iron1);
	FillRect(X + P, Y + P, W - 2 * P, P, GBRpg::Iron3);
	FillRect(X + P, Y + H - 2 * P, W - 2 * P, P, GBRpg::Iron0);
	FPixelTextStyle Style;
	Style.Scale = Scale;
	Style.Color = GBRpg::Bone;
	Style.Shadow = FLinearColor(0.f, 0.f, 0.f, 0.9f);
	DrawPixelText(Key, X + W * 0.5f, Y + FMath::RoundToFloat((H - Size.Y) * 0.5f / Scale) * Scale - P * 0.f, Style, 0.5f);
	if (OutWidth)
	{
		*OutWidth = W;
	}
}

void AGBHUD::RpgCursor(float X, float CY)
{
	const float P = ArtPixel();
	static const TCHAR* Rows[] = { TEXT("o...."), TEXT("oGo.."), TEXT("oLGo."), TEXT("oLGGo"), TEXT("oDGo."), TEXT("oDo.."), TEXT("o....") };
	const float Bob = FMath::RoundToFloat(FMath::Sin((float)GetRealTime() * 7.f) * 0.8f) * P;
	const float X0 = FMath::RoundToFloat(X + Bob);
	const float Y0 = FMath::RoundToFloat(CY - 3.5f * P);
	for (int32 Row = 0; Row < 7; ++Row)
	{
		for (int32 Col = 0; Col < 5; ++Col)
		{
			const TCHAR C = Rows[Row][Col];
			const FLinearColor* Pick = C == TEXT('o') ? &GBRpg::Outline : C == TEXT('G') ? &GBRpg::Gold3 : C == TEXT('L') ? &GBRpg::Gold4 : C == TEXT('D') ? &GBRpg::Gold1 : nullptr;
			if (Pick)
			{
				FillRect(X0 + Col * P, Y0 + Row * P, P, P, *Pick);
			}
		}
	}
}

// ======================================================================== status bar

void AGBHUD::DrawRpgHud(AGBPlayerCharacter* Player)
{
	const float P = ArtPixel();
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	const float BarH = 30 * P;
	const float Top = H - BarH;
	const float CX = FMath::RoundToFloat(W * 0.5f / P) * P;

	UGBEquipmentComponent* Equipment = Player->GetEquipment();
	AGBHeldItem* Item = Equipment ? Equipment->GetActiveItem() : nullptr;
	const bool bStaff = Item && Item->GetSlot() == EGBHeldSlot::Staff;

	// Intro after a weapon switch: the bar rises from below.
	const float Intro = FMath::Clamp((float)(GetRealTime() - SkinChangeTime) / 0.22f, 0.f, 1.f);
	const float Drop = FMath::RoundToFloat((1.f - FMath::InterpEaseOut(0.f, 1.f, Intro, 3.f)) * (BarH + 12 * P) / P) * P;
	const float T = Top + Drop;

	// ---- bar body
	FillRect(0.f, T - 2 * P, W, 2 * P, FLinearColor(0.f, 0.f, 0.f, 0.35f));
	RpgStone(0.f, T, W, BarH + 2 * P);
	FillRect(0.f, T, W, P, GBRpg::Outline);
	FillRect(0.f, T + P, W, P, GBRpg::Iron4);
	FillRect(0.f, T + 2 * P, W, P, GBRpg::Iron2);
	FillRect(0.f, T + 3 * P, W, P, GBRpg::Outline);
	FillRect(0.f, T + 4 * P, W, P, GBRpg::Gold1);
	FillRect(0.f, T + 5 * P, W, P, GBAlpha(GBRpg::Outline, 0.55f));
	for (float RX = 6 * P; RX < W - 4 * P; RX += 40 * P)
	{
		RpgRivet(RX, T);
	}

	// ---- center crest (spell sockets / held weapon)
	const float Socket = 22 * P;
	const float SocketGap = 3 * P;
	const int32 NumSlots = UGBSpellbookComponent::NumSlots;
	const float CrestW = NumSlots * Socket + (NumSlots - 1) * SocketGap + 16 * P;
	const float CrestX = CX - FMath::RoundToFloat(CrestW * 0.5f / P) * P;
	const float CrestTop = T - 10 * P;
	// Stepped shoulders, then the plate.
	for (int32 Step = 0; Step < 4; ++Step)
	{
		const float SY = CrestTop + (Step + 1) * 2 * P;
		const float SW = (4 - Step) * 2 * P;
		RpgStone(CrestX - SW, SY, SW, T - SY + P);
		RpgStone(CrestX + CrestW, SY, SW, T - SY + P);
		FillRect(CrestX - SW, SY, 2 * P, P, GBRpg::Outline);
		FillRect(CrestX + CrestW + SW - 2 * P, SY, 2 * P, P, GBRpg::Outline);
		FillRect(CrestX - SW, SY + P, 2 * P, P, GBRpg::Iron3);
		FillRect(CrestX + CrestW + SW - 2 * P, SY + P, 2 * P, P, GBRpg::Iron3);
	}
	RpgStone(CrestX, CrestTop, CrestW, H - CrestTop);
	FillRect(CrestX, CrestTop, CrestW, P, GBRpg::Outline);
	FillRect(CrestX, CrestTop + P, CrestW, P, GBRpg::Iron4);
	FillRect(CrestX, CrestTop + 2 * P, CrestW, P, GBRpg::Iron2);
	FillRect(CrestX, CrestTop + 3 * P, CrestW, P, GBRpg::Outline);
	FillRect(CrestX + 2 * P, CrestTop + 4 * P, CrestW - 4 * P, P, GBRpg::Gold1);
	RpgGem(CX, CrestTop + 1.5f * P, GBRpg::Arcane, true);

	if (bStaff)
	{
		DrawRpgSpellSockets(Player, CX, CrestTop + 7 * P);
	}
	else if (Item)
	{
		// Sword (or any non-staff RPG item): its name on the crest and its element as a lit gem.
		const EGBElement Element = Item->GetElement();
		FPixelTextStyle Name;
		Name.Scale = FMath::Max(2, FMath::RoundToInt(P));
		Name.Ink = &GBRpg::InkGold;
		Name.Outline = GBRpg::Outline;
		Name.Shadow = FLinearColor(0.f, 0.f, 0.f, 0.7f);
		DrawPixelText(Item->GetDisplayName().ToUpper(), CX, CrestTop + 11 * P, Name, 0.5f);
		const FString ElementText = GBHudText::ElementName(Element);
		const int32 Small = FMath::Max(2, FMath::RoundToInt(P * 0.5f));
		const FVector2D ES = MeasurePixelText(ElementText, Small);
		RpgGem(CX - ES.X * 0.5f - 6 * P, CrestTop + 24 * P, GBHudText::ElementColor(Element), true);
		FPixelTextStyle ElementStyle;
		ElementStyle.Scale = Small;
		ElementStyle.Color = GBHudText::ElementColor(Element);
		ElementStyle.Shadow = FLinearColor(0.f, 0.f, 0.f, 0.9f);
		DrawPixelText(ElementText, CX, CrestTop + 22 * P, ElementStyle, 0.5f);
	}

	// ---- left: life, mana, dash
	DrawRpgVitals(Player, 6 * P, T + 7 * P, FMath::Min(150 * P, CrestX - 26 * P));

	// ---- right: quick-use socket, skill points
	const float QS = 20 * P;
	const float QX = W - 8 * P - QS;
	const float QY = T + 7 * P;
	RpgWell(QX, QY, QS, QS);
	RpgFrame(QX - P, QY - P, QS + 2 * P, QS + 2 * P, ERpgFrame::Gold);
	FillRect(QX + 2 * P, QY + 2 * P, QS - 4 * P, QS - 4 * P, GBRpg::StoneDeep);
	FString QuickName;
	int32 QuickCount = 0;
	if (const UGBInventoryComponent* Inventory = Player->GetInventory())
	{
		const FName QuickId = Inventory->GetQuickUseItem();
		if (QuickId != NAME_None)
		{
			const UGBDataSubsystem* Data = UGBDataSubsystem::Get(this);
			const FGBItemDefinition* ItemDef = Data ? Data->FindItem(QuickId) : nullptr;
			QuickName = (ItemDef && !ItemDef->DisplayName.IsEmpty()) ? ItemDef->DisplayName : QuickId.ToString();
			QuickCount = Inventory->GetCount(QuickId);
			const FString* Sprite = ItemDef ? ItemDef->Strings.Find(GBString::Sprite) : nullptr;
			DrawSpriteFit(Sprite ? FName(**Sprite) : NAME_None, QX + 3 * P, QY + 3 * P, QS - 6 * P, QS - 6 * P);
		}
	}
	float KeyW = 0.f;
	RpgKeyCap(QX - 4 * P, QY - 5 * P, TEXT("F"), &KeyW);
	const int32 Small = FMath::Max(2, FMath::RoundToInt(P * 0.5f));
	if (QuickCount > 0)
	{
		FPixelTextStyle Count;
		Count.Scale = Small;
		Count.Ink = &GBRpg::InkBone;
		Count.Outline = GBRpg::Outline;
		DrawPixelText(FString::Printf(TEXT("x%d"), QuickCount), QX + QS - P, QY + QS - 4 * P, Count, 1.f);
	}
	const float InfoRight = QX - 8 * P;
	FPixelTextStyle Info;
	Info.Scale = Small;
	Info.Shadow = FLinearColor(0.f, 0.f, 0.f, 0.9f);
	Info.Ink = &GBRpg::InkBone;
	DrawPixelText(QuickName.IsEmpty() ? FString(TEXT("Nothing to use")) : QuickName, InfoRight, QY + P, Info, 1.f);
	if (const UGBSkillTreeComponent* SkillTree = Player->GetSkillTree())
	{
		if (SkillTree->GetPoints() > 0)
		{
			const FString Text = FString::Printf(TEXT("%d skill point%s"), SkillTree->GetPoints(), SkillTree->GetPoints() == 1 ? TEXT("") : TEXT("s"));
			float KW = 0.f;
			RpgKeyCap(InfoRight - 9 * P, QY + 10 * P, TEXT("K"), &KW);
			FPixelTextStyle Points = Info;
			Points.Ink = &GBRpg::InkGold;
			const FVector2D PS = DrawPixelText(Text, InfoRight - 12 * P, QY + 12 * P, Points, 1.f);
			const float Pulse = 0.6f + 0.4f * FMath::Sin((float)GetRealTime() * 4.f);
			RpgGem(InfoRight - 16 * P - PS.X, QY + 13.5f * P, GBAlpha(GBRpg::Gold3, 1.f) * FLinearColor(1.f, 1.f, 1.f, Pulse), true);
		}
	}

	// ---- above the bar: active effects, summoned shield
	DrawRpgEffects(Player, 6 * P, T - 4 * P);
}

void AGBHUD::DrawRpgVitals(AGBPlayerCharacter* Player, float X, float Y, float W)
{
	const float P = ArtPixel();
	const UGBAttributeComponent* Attributes = Player->GetAttributes();
	if (!Attributes)
	{
		return;
	}
	const int32 NumScale = FMath::Max(2, FMath::RoundToInt(P));
	const FVector2D NumSize = MeasurePixelText(TEXT("000"), NumScale);
	const float LabelW = 12 * P;
	const float TubeX = X + LabelW;
	const float TubeW = FMath::RoundToFloat((W - LabelW - NumSize.X - 6 * P) / P) * P;
	const float TubeH = 8 * P;

	RpgWell(X - 2 * P, Y - 3 * P, W + 4 * P, 27 * P);

	auto Row = [&](float RY, float Frac, float Trail, const FLinearColor Ramp[4], const FGBPixelInk& Ink, int32 Value, FName Icon)
	{
		// Small emblem at the left (heart / mana flask), a tube, then the number.
		if (Icon == TEXT("heart"))
		{
			static const TCHAR* Heart[] = { TEXT(".oo.oo."), TEXT("oLRoRRo"), TEXT("oRRRRDo"), TEXT("oRRRRDo"), TEXT(".oRRDo."), TEXT("..oDo.."), TEXT("...o...") };
			for (int32 R = 0; R < 7; ++R)
			{
				for (int32 C = 0; C < 7; ++C)
				{
					const TCHAR Ch = Heart[R][C];
					const FLinearColor* Pick = Ch == TEXT('o') ? &GBRpg::Outline : Ch == TEXT('L') ? &Ramp[3] : Ch == TEXT('R') ? &Ramp[2] : Ch == TEXT('D') ? &Ramp[1] : nullptr;
					if (Pick)
					{
						FillRect(X + (1 + C) * P, RY + (0.5f + R) * P, P, P, *Pick);
					}
				}
			}
		}
		else
		{
			static const TCHAR* Flask[] = { TEXT("..ooo.."), TEXT("..oLo.."), TEXT(".oLBBo."), TEXT("oLBBBBo"), TEXT("oBBBBDo"), TEXT("oBBBDDo"), TEXT(".ooooo.") };
			for (int32 R = 0; R < 7; ++R)
			{
				for (int32 C = 0; C < 7; ++C)
				{
					const TCHAR Ch = Flask[R][C];
					const FLinearColor* Pick = Ch == TEXT('o') ? &GBRpg::Outline : Ch == TEXT('L') ? &Ramp[3] : Ch == TEXT('B') ? &Ramp[2] : Ch == TEXT('D') ? &Ramp[1] : nullptr;
					if (Pick)
					{
						FillRect(X + (1 + C) * P, RY + (0.5f + R) * P, P, P, *Pick);
					}
				}
			}
		}
		RpgTube(TubeX, RY, TubeW, TubeH, Frac, Ramp, Trail);
		FPixelTextStyle Num;
		Num.Scale = NumScale;
		Num.Ink = &Ink;
		Num.Outline = GBRpg::Outline;
		DrawPixelText(FString::Printf(TEXT("%d"), Value), TubeX + TubeW + 4 * P, RY + FMath::RoundToFloat((TubeH - NumSize.Y) * 0.5f / P) * P, Num, 0.f);
	};

	const float MaxHealth = Attributes->GetMaxHealth();
	const float MaxMana = Attributes->GetMaxMana();
	const float HealthFrac = MaxHealth > 0.f ? Attributes->GetHealth() / MaxHealth : 0.f;
	const float ManaFrac = MaxMana > 0.f ? Attributes->GetMana() / MaxMana : 0.f;
	Row(Y, HealthFrac, HealthTrail01, GBRpg::HealthRamp, HealthFrac <= 0.3f ? GBRpg::InkBlood : GBRpg::InkBone, FMath::CeilToInt(Attributes->GetHealth()), TEXT("heart"));
	Row(Y + 9 * P, ManaFrac, -1.f, GBRpg::ManaRamp, GBRpg::InkMana, FMath::FloorToInt(Attributes->GetMana()), TEXT("mana"));
	if (HealthFrac <= 0.3f)
	{
		// Low life: the heart beats a red glow.
		const float Beat = FMath::Pow(FMath::Abs(FMath::Sin((float)GetRealTime() * 4.f)), 6.f);
		DrawGlow(X + 4.5f * P, Y + 4 * P, 10 * P, 10 * P, FLinearColor(1.f, 0.1f, 0.05f, 0.5f * Beat));
	}

	// Dash: a thin gilt tube, "DASH" when ready.
	const float DashY = Y + 18 * P;
	const float Remaining = Player->GetDashCooldownRemaining();
	const UGBStatsComponent* Stats = Player->GetStats();
	const float Total = Stats ? Stats->GetStat(GBStat::DashCooldown, 0.f) : 0.f;
	const float DashFrac = (Remaining <= 0.f || Total <= 0.f) ? 1.f : 1.f - Remaining / Total;
	FPixelTextStyle Label;
	Label.Scale = FMath::Max(2, FMath::RoundToInt(P * 0.5f));
	Label.Ink = DashFrac >= 1.f ? &GBRpg::InkGold : nullptr;
	Label.Color = DashFrac >= 1.f ? FLinearColor::White : GBRpg::BoneDark;
	Label.Shadow = FLinearColor(0.f, 0.f, 0.f, 0.9f);
	// A thin gilt tube under the mana, its label at its right end (bottom-aligned, never below the screen edge).
	const float DashW = FMath::RoundToFloat(TubeW * 0.5f / P) * P;
	RpgTube(TubeX, DashY, DashW, 4 * P, DashFrac, GBRpg::StaminaRamp);
	DrawPixelText(TEXT("DASH"), TubeX + DashW + 3 * P, DashY + 4 * P - 7 * Label.Scale, Label, 0.f);
}

void AGBHUD::DrawRpgSpellSockets(AGBPlayerCharacter* Player, float CX, float Y)
{
	UGBSpellbookComponent* Spellbook = Player->GetSpellbook();
	if (!Spellbook)
	{
		return;
	}
	const UGBDataSubsystem* Data = UGBDataSubsystem::Get(this);
	const float P = ArtPixel();
	const float Socket = 22 * P;
	const float Gap = 3 * P;
	const int32 Num = UGBSpellbookComponent::NumSlots;
	const float X0 = CX - FMath::RoundToFloat((Num * Socket + (Num - 1) * Gap) * 0.5f / P) * P;
	static const TCHAR* Keys[] = { TEXT("RMB"), TEXT("1"), TEXT("2"), TEXT("3"), TEXT("4") };
	const int32 Small = FMath::Max(2, FMath::RoundToInt(P * 0.5f));

	for (int32 Slot = 0; Slot < Num; ++Slot)
	{
		const float X = X0 + Slot * (Socket + Gap);
		RpgFrame(X, Y, Socket, Socket, ERpgFrame::Gold);
		const float IX = X + 3 * P;
		const float IY = Y + 3 * P;
		const float IS = Socket - 6 * P;
		FillRect(IX, IY, IS, IS, GBRpg::StoneDeep);
		FillRect(IX, IY, IS, P, GBRpg::Outline);
		FillRect(IX, IY, P, IS, GBRpg::Outline);

		const FName SpellId = Spellbook->GetSlotSpell(Slot);
		const FGBSpellDefinition* Def = (Data && SpellId != NAME_None) ? Data->FindSpell(SpellId) : nullptr;
		if (Def)
		{
			const FLinearColor ElementColor = GBHudText::ElementColor(Def->Element);
			const EGBCastFailReason Reason = Spellbook->CanCast(SpellId);
			const float Remaining = Spellbook->GetCooldownRemaining(SpellId);
			const bool bReady = Reason == EGBCastFailReason::None;
			if (bReady)
			{
				DrawGlow(IX + IS * 0.5f, IY + IS * 0.5f, IS * 0.55f, IS * 0.55f, GBAlpha(ElementColor, 0.28f));
			}
			if (!DrawSpriteFit(GBHudText::SpellIconId(SpellId), IX + P, IY + P, IS - 2 * P, IS - 2 * P))
			{
				FPixelTextStyle Fallback;
				Fallback.Scale = Small;
				Fallback.Color = ElementColor;
				DrawPixelText(GBHudText::Abbreviate(Def->DisplayName, 5), IX + IS * 0.5f, IY + IS * 0.4f, Fallback, 0.5f);
			}
			if (Remaining > 0.f)
			{
				const float Duration = Spellbook->GetCooldownDuration(SpellId);
				const float Frac = Duration > 0.f ? FMath::Clamp(Remaining / Duration, 0.f, 1.f) : 1.f;
				const float CH = FMath::CeilToFloat(IS * Frac / P) * P;
				FillRect(IX, IY + IS - CH, IS, CH, FLinearColor(0.02f, 0.02f, 0.05f, 0.72f));
				FillRect(IX, IY + IS - CH, IS, P, GBAlpha(GBRpg::Gold3, 0.8f));
				FPixelTextStyle Cool;
				Cool.Scale = FMath::Max(2, FMath::RoundToInt(P * 0.75f));
				Cool.Ink = &GBRpg::InkBone;
				Cool.Outline = GBRpg::Outline;
				DrawPixelText(Remaining >= 10.f ? FString::Printf(TEXT("%d"), FMath::CeilToInt(Remaining)) : FString::Printf(TEXT("%.1f"), Remaining),
					IX + IS * 0.5f, IY + IS * 0.5f - 3.5f * Cool.Scale, Cool, 0.5f);
			}
			else if (!bReady && Reason != EGBCastFailReason::OnCooldown)
			{
				// Not enough mana (or not castable now): the socket sinks into a cold blue shade.
				FillRect(IX, IY, IS, IS, FLinearColor(0.02f, 0.04f, 0.15f, 0.62f));
			}
			// Mana cost on a tiny blue tab at the bottom right.
			FPixelTextStyle Cost;
			Cost.Scale = Small;
			Cost.Ink = Reason == EGBCastFailReason::NotEnoughMana ? &GBRpg::InkBlood : &GBRpg::InkMana;
			Cost.Outline = GBRpg::Outline;
			DrawPixelText(FString::Printf(TEXT("%d"), FMath::RoundToInt(Spellbook->GetManaCost(SpellId))), X + Socket - 2 * P, Y + Socket - 3 * P - 7 * Small, Cost, 1.f);
		}
		else
		{
			// Empty socket: a faint carved rune.
			const float MX = IX + IS * 0.5f;
			const float MY = IY + IS * 0.5f;
			FillRect(MX - P, MY - 4 * P, 2 * P, 8 * P, GBAlpha(GBRpg::Stone3, 0.6f));
			FillRect(MX - 4 * P, MY - P, 8 * P, 2 * P, GBAlpha(GBRpg::Stone3, 0.6f));
		}
		// Key label under the socket.
		FPixelTextStyle KeyStyle;
		KeyStyle.Scale = Small;
		KeyStyle.Ink = &GBRpg::InkGold;
		KeyStyle.Shadow = FLinearColor(0.f, 0.f, 0.f, 0.9f);
		DrawPixelText(Keys[Slot], X + Socket * 0.5f, Y + Socket + 2 * P, KeyStyle, 0.5f);
	}
}

void AGBHUD::DrawRpgEffects(AGBPlayerCharacter* Player, float X, float Bottom)
{
	const float P = ArtPixel();
	const int32 Small = FMath::Max(2, FMath::RoundToInt(P * 0.5f));
	float Y = Bottom;
	// Summoned shield first (lowest), then the effects stacked upward.
	if (const UGBSpellbookComponent* Spellbook = Player->GetSpellbook(); Spellbook && Spellbook->HasShield())
	{
		Y -= 14 * P;
		DrawRpgShield(Player, X, Y);
		Y -= 2 * P;
	}
	const UGBStatusEffectComponent* Effects = Player->GetStatusEffects();
	if (!Effects)
	{
		return;
	}
	const UGBDataSubsystem* Data = UGBDataSubsystem::Get(this);
	for (const FGBActiveStatusEffect& Effect : Effects->GetActiveEffects())
	{
		const FGBStatusEffectDefinition* Def = Data ? Data->FindStatusEffect(Effect.EffectId) : nullptr;
		FString Name = (Def && !Def->DisplayName.IsEmpty()) ? Def->DisplayName : Effect.EffectId.ToString();
		if (Effect.Stacks > 1)
		{
			Name += FString::Printf(TEXT(" x%d"), Effect.Stacks);
		}
		const FString Time = Effect.TotalDuration > 0.f ? FString::Printf(TEXT("%.1fs"), Effect.TimeRemaining) : FString();
		const float PW = 70 * P;
		const float PH = 11 * P;
		Y -= PH + P;
		FillRect(X, Y, PW, PH, GBRpg::Outline);
		RpgStone(X + P, Y + P, PW - 2 * P, PH - 2 * P, FLinearColor(0.8f, 0.8f, 0.85f, 1.f));
		FillRect(X + P, Y + P, PW - 2 * P, P, GBRpg::Iron3);
		FillRect(X + P, Y + PH - 2 * P, PW - 2 * P, P, GBRpg::Iron0);
		FPixelTextStyle Text;
		Text.Scale = Small;
		Text.Ink = &GBRpg::InkBone;
		Text.Shadow = FLinearColor(0.f, 0.f, 0.f, 0.9f);
		DrawPixelText(GBHudText::Abbreviate(Name, 16), X + 4 * P, Y + 2.f * P, Text, 0.f);
		FPixelTextStyle TimeStyle = Text;
		TimeStyle.Ink = &GBRpg::InkGold;
		DrawPixelText(Time, X + PW - 3 * P, Y + 2.f * P, TimeStyle, 1.f);
		if (Effect.TotalDuration > 0.f)
		{
			const float Frac = FMath::Clamp(Effect.TimeRemaining / Effect.TotalDuration, 0.f, 1.f);
			FillRect(X + 2 * P, Y + PH - 3 * P, FMath::RoundToFloat((PW - 4 * P) * Frac / P) * P, P, GBRpg::Ember);
		}
	}
}

void AGBHUD::DrawRpgShield(AGBPlayerCharacter* Player, float X, float Y)
{
	const UGBSpellbookComponent* Spellbook = Player->GetSpellbook();
	if (!Spellbook || !Spellbook->HasShield())
	{
		return;
	}
	const float P = ArtPixel();
	const int32 Small = FMath::Max(2, FMath::RoundToInt(P * 0.5f));
	const bool bRaised = Spellbook->IsBlockRaised();
	const float PW = 70 * P;
	const float PH = 14 * P;
	FillRect(X, Y, PW, PH, GBRpg::Outline);
	RpgStone(X + P, Y + P, PW - 2 * P, PH - 2 * P, FLinearColor(0.8f, 0.8f, 0.85f, 1.f));
	if (bRaised)
	{
		RpgFrame(X, Y, PW, PH, ERpgFrame::Gold);
	}
	else
	{
		FillRect(X + P, Y + P, PW - 2 * P, P, GBRpg::Iron3);
		FillRect(X + P, Y + PH - 2 * P, PW - 2 * P, P, GBRpg::Iron0);
	}
	FPixelTextStyle Text;
	Text.Scale = Small;
	Text.Ink = bRaised ? &GBRpg::InkIce : &GBRpg::InkBone;
	Text.Shadow = FLinearColor(0.f, 0.f, 0.f, 0.9f);
	DrawPixelText(bRaised ? TEXT("BLOCKING") : TEXT("ICE SHIELD"), X + 4 * P, Y + 3 * P, Text, 0.f);
	FPixelTextStyle TimeStyle = Text;
	TimeStyle.Ink = &GBRpg::InkGold;
	DrawPixelText(FString::Printf(TEXT("%.1fs"), FMath::Max(0.f, Spellbook->GetShieldTimeRemaining())), X + PW - 3 * P, Y + 3 * P, TimeStyle, 1.f);
	RpgTube(X + 3 * P, Y + PH - 6 * P, PW - 6 * P, 4 * P, Spellbook->GetShieldHealthFraction(), GBRpg::IceRamp);
	if (!bRaised)
	{
		RpgKeyCap(X + PW + 2 * P, Y + 2 * P, TEXT("Q"));
	}
}
