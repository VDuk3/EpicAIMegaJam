// The modern cyberpunk skin of AGBHUD (GDD 13: a gun in hand). Thin coral-red lines and condensed capitals on
// near-black, cyan for values and anything you can act on, yellow for alerts; boxes with cut corners, slanted
// segmented bars, corner brackets, a chromatic glitch when the HUD boots or the player is hit.
#include "UI/GBHUD.h"

#include "UI/GBHUDShared.h"
#include "Player/GBPlayerCharacter.h"
#include "Weapons/GBEquipmentComponent.h"
#include "Weapons/GBHeldItem.h"
#include "Weapons/GBGun.h"
#include "Spells/GBSpellbookComponent.h"
#include "Items/GBInventoryComponent.h"
#include "Progression/GBSkillTreeComponent.h"
#include "Combat/GBStatusEffectComponent.h"
#include "Stats/GBStatsComponent.h"
#include "Stats/GBAttributeComponent.h"
#include "Data/GBDataSubsystem.h"
#include "Core/GBStatNames.h"

#include "Engine/Canvas.h"

// ======================================================================== pieces

TArray<FVector2D> AGBHUD::CyChamfer(float X, float Y, float W, float H, float Cut, uint8 Corners) const
{
	TArray<FVector2D> Points;
	Points.Reserve(8);
	const float C = FMath::Min(Cut, FMath::Min(W, H) * 0.5f);
	if (Corners & 1) { Points.Add(FVector2D(X, Y + C)); Points.Add(FVector2D(X + C, Y)); }
	else { Points.Add(FVector2D(X, Y)); }
	if (Corners & 2) { Points.Add(FVector2D(X + W - C, Y)); Points.Add(FVector2D(X + W, Y + C)); }
	else { Points.Add(FVector2D(X + W, Y)); }
	if (Corners & 4) { Points.Add(FVector2D(X + W, Y + H - C)); Points.Add(FVector2D(X + W - C, Y + H)); }
	else { Points.Add(FVector2D(X + W, Y + H)); }
	if (Corners & 8) { Points.Add(FVector2D(X + C, Y + H)); Points.Add(FVector2D(X, Y + H - C)); }
	else { Points.Add(FVector2D(X, Y + H)); }
	return Points;
}

void AGBHUD::CyBox(float X, float Y, float W, float H, float Cut, const FLinearColor& Fill, const FLinearColor& Line, uint8 Corners, float Thick)
{
	X = FMath::RoundToFloat(X);
	Y = FMath::RoundToFloat(Y);
	W = FMath::RoundToFloat(W);
	H = FMath::RoundToFloat(H);
	if (Fill.A > 0.f)
	{
		FillPoly(CyChamfer(X, Y, W, H, Cut, Corners), Fill);
	}
	if (Line.A > 0.f)
	{
		// Lines on pixel centers so 1-pixel edges stay crisp.
		StrokePoly(CyChamfer(X + 0.5f, Y + 0.5f, W - 1.f, H - 1.f, Cut, Corners), Line, Thick);
	}
}

void AGBHUD::CyBrackets(float X, float Y, float W, float H, float Len, const FLinearColor& Color, float Thick)
{
	const float T = FMath::Max(1.f, FMath::RoundToFloat(Thick));
	FillRect(X, Y, Len, T, Color);
	FillRect(X, Y, T, Len, Color);
	FillRect(X + W - Len, Y, Len, T, Color);
	FillRect(X + W - T, Y, T, Len, Color);
	FillRect(X, Y + H - T, Len, T, Color);
	FillRect(X, Y + H - Len, T, Len, Color);
	FillRect(X + W - Len, Y + H - T, Len, T, Color);
	FillRect(X + W - T, Y + H - Len, T, Len, Color);
}

void AGBHUD::CySlantBar(float X, float Y, float W, float H, float Slant, float Fraction, int32 Segments, const FLinearColor& Fill, const FLinearColor& Empty, float Trail)
{
	const int32 N = FMath::Max(1, Segments);
	const float Gap = FMath::Max(1.f, FMath::RoundToFloat(2.f * UIScale()));
	const float SW = (W - Slant - (N - 1) * Gap) / N;
	const float Frac = FMath::Clamp(Fraction, 0.f, 1.f);
	const float TrailFrac = FMath::Clamp(Trail, 0.f, 1.f);
	auto Seg = [&](float X0, float Width, const FLinearColor& Color)
	{
		if (Width <= 0.f || Color.A <= 0.f)
		{
			return;
		}
		TArray<FVector2D> Points;
		Points.Add(FVector2D(X0 + Slant, Y));
		Points.Add(FVector2D(X0 + Slant + Width, Y));
		Points.Add(FVector2D(X0 + Width, Y + H));
		Points.Add(FVector2D(X0, Y + H));
		FillPoly(Points, Color);
	};
	for (int32 i = 0; i < N; ++i)
	{
		const float X0 = X + i * (SW + Gap);
		const float Start = (float)i / N;
		const float End = (float)(i + 1) / N;
		const float Filled = FMath::Clamp((Frac - Start) / (End - Start), 0.f, 1.f);
		const float Trailed = FMath::Clamp((TrailFrac - Start) / (End - Start), 0.f, 1.f);
		Seg(X0, SW, Empty);
		if (Trailed > Filled)
		{
			Seg(X0, SW * Trailed, GBAlpha(GBCy::Pale, 0.65f));
		}
		Seg(X0, SW * Filled, Fill);
	}
}

float AGBHUD::CyKey(float X, float Y, const FString& Key, const FLinearColor& Color, float Height)
{
	const float S = UIScale();
	FCyTextStyle Style;
	Style.Face = ECyFace::Condensed;
	Style.Px = Height * 0.72f;
	Style.Color = Color;
	Style.bUpper = true;
	const FVector2D Size = MeasureCyText(Key, Style);
	const float H = FMath::RoundToFloat(Height * S);
	const float W = FMath::Max(H, FMath::RoundToFloat(Size.X + 10.f * S));
	CyBox(X, Y, W, H, 0.f, GBAlpha(GBCy::Ink, 0.6f), Color, 0);
	DrawCyText(Key, X + W * 0.5f, Y + (H - Size.Y) * 0.5f, Style, 0.5f);
	return W;
}

void AGBHUD::CyScanlines(float X, float Y, float W, float H, float Alpha)
{
	const float Step = FMath::Max(3.f, FMath::RoundToFloat(4.f * UIScale()));
	const FLinearColor Line(0.f, 0.f, 0.f, Alpha);
	for (float LY = FMath::RoundToFloat(Y); LY < Y + H; LY += Step)
	{
		FillRect(X, LY, W, 1.f, Line);
	}
}

void AGBHUD::CyRuler(float X, float Y, float W, const FLinearColor& Color)
{
	const float S = UIScale();
	FillRect(X, Y, W, 1.f, Color);
	const float Step = FMath::Max(4.f, FMath::RoundToFloat(10.f * S));
	int32 i = 0;
	for (float TX = X; TX <= X + W; TX += Step, ++i)
	{
		const float L = (i % 5 == 0) ? 7.f * S : 3.f * S;
		FillRect(TX, Y - L, 1.f, L, Color);
	}
}

// ======================================================================== HUD

namespace GBCyLocal
{
	/** Boot glitch after a switch to the gun: strong chromatic split and flicker that settles in 0.35 s. */
	float BootSplit(double Since, float S)
	{
		if (Since < 0.0 || Since > 0.35)
		{
			return 0.f;
		}
		return (float)(1.0 - Since / 0.35) * 6.f * S;
	}
}

void AGBHUD::DrawCyberHud(AGBPlayerCharacter* Player)
{
	const float S = UIScale();
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	const double SinceBoot = GetRealTime() - SkinChangeTime;
	// Flicker in: the HUD blinks twice before it settles.
	if (SinceBoot >= 0.0 && SinceBoot < 0.2 && FMath::Frac((float)SinceBoot * 20.f) < 0.35f)
	{
		return;
	}

	UGBEquipmentComponent* Equipment = Player->GetEquipment();
	AGBHeldItem* Item = Equipment ? Equipment->GetActiveItem() : nullptr;
	TGuardValue<bool> ShadowGuard(bForceTextShadow, true);

	// Low health: the screen edge pulses red.
	if (const UGBAttributeComponent* Attributes = Player->GetAttributes())
	{
		const float Frac = Attributes->GetMaxHealth() > 0.f ? Attributes->GetHealth() / Attributes->GetMaxHealth() : 1.f;
		if (Frac <= 0.3f && Frac > 0.f)
		{
			const float Pulse = 0.5f + 0.5f * FMath::Sin((float)GetRealTime() * 6.f);
			const FLinearColor Edge = GBAlpha(GBCy::Red, 0.18f + 0.12f * Pulse);
			DrawFade(0.f, 0.f, W * 0.16f, H, Edge, true);
			DrawFade(W * 0.84f, 0.f, W * 0.16f, H, Edge, false);
		}
	}

	DrawCyberVitals(Player, 44.f * S, 38.f * S);
	float EffectsY = 38.f * S + 104.f * S;
	EffectsY = DrawCyberEffects(Player, 44.f * S, EffectsY);
	DrawCyberShield(Player, 44.f * S, EffectsY);

	if (Item)
	{
		DrawCyberWeapon(Player, Item, W - 52.f * S, H - 44.f * S);
	}

	// Quick-use item, bottom left.
	if (const UGBInventoryComponent* Inventory = Player->GetInventory())
	{
		const float Box = 76.f * S;
		const float BX = 52.f * S;
		const float BY = H - 44.f * S - Box;
		CyBox(BX, BY, Box, Box, 12.f * S, GBAlpha(GBCy::Ink, 0.62f), GBAlpha(GBCy::Red, 0.75f), 0x5);
		CyBrackets(BX - 4.f * S, BY - 4.f * S, Box + 8.f * S, Box + 8.f * S, 8.f * S, GBAlpha(GBCy::RedDim, 0.8f));
		const FName QuickId = Inventory->GetQuickUseItem();
		const UGBDataSubsystem* Data = UGBDataSubsystem::Get(this);
		const FGBItemDefinition* ItemDef = (Data && QuickId != NAME_None) ? Data->FindItem(QuickId) : nullptr;
		FCyTextStyle Name;
		Name.Face = ECyFace::Condensed;
		Name.Px = 17.f;
		Name.Spacing = 80;
		Name.bUpper = true;
		Name.Color = GBCy::RedBright;
		if (QuickId != NAME_None)
		{
			const FString* Sprite = ItemDef ? ItemDef->Strings.Find(GBString::Sprite) : nullptr;
			DrawSpriteFit(Sprite ? FName(**Sprite) : NAME_None, BX + 12.f * S, BY + 12.f * S, Box - 24.f * S, Box - 24.f * S, FLinearColor(0.85f, 1.f, 1.f, 1.f), true);
			FCyTextStyle Count;
			Count.Face = ECyFace::Condensed;
			Count.Px = 24.f;
			Count.Color = GBCy::Pale;
			Count.Shadow = FLinearColor(0.f, 0.f, 0.f, 0.8f);
			DrawCyText(FString::Printf(TEXT("%d"), Inventory->GetCount(QuickId)), BX + Box - 8.f * S, BY + Box - 32.f * S, Count, 1.f);
			const FString ItemName = (ItemDef && !ItemDef->DisplayName.IsEmpty()) ? ItemDef->DisplayName : QuickId.ToString();
			DrawCyText(ItemName, BX + Box + 14.f * S, BY + Box - 24.f * S, Name, 0.f);
		}
		else
		{
			Name.Color = GBCy::Grey;
			DrawCyText(TEXT("Empty"), BX + Box + 14.f * S, BY + Box - 24.f * S, Name, 0.f);
		}
		CyKey(BX + Box + 14.f * S, BY + 4.f * S, TEXT("F"), GBCy::Cyan, 24.f);
	}

	// Skill points: a yellow notice, top right.
	if (const UGBSkillTreeComponent* SkillTree = Player->GetSkillTree(); SkillTree && SkillTree->GetPoints() > 0)
	{
		FCyTextStyle Notice;
		Notice.Face = ECyFace::Condensed;
		Notice.Px = 19.f;
		Notice.Spacing = 90;
		Notice.bUpper = true;
		Notice.Color = GBCy::Ink;
		const FString Text = FString::Printf(TEXT("Skill point%s available  %d"), SkillTree->GetPoints() == 1 ? TEXT("") : TEXT("s"), SkillTree->GetPoints());
		const FVector2D Size = MeasureCyText(Text, Notice);
		const float NH = 32.f * S;
		const float NW = Size.X + 34.f * S;
		const float NX = W - 52.f * S - NW;
		const float NY = 40.f * S;
		CyBox(NX, NY, NW, NH, 10.f * S, GBCy::Yellow, FLinearColor::Transparent, 0x2);
		FillRect(NX - 8.f * S, NY, 4.f * S, NH, GBCy::Yellow);
		DrawCyText(Text, NX + 14.f * S, NY + (NH - Size.Y) * 0.5f, Notice, 0.f);
		CyKey(NX - 8.f * S - 40.f * S, NY + 3.f * S, TEXT("K"), GBCy::Yellow, 26.f);
	}
}

void AGBHUD::DrawCyberVitals(AGBPlayerCharacter* Player, float X, float Y)
{
	const float S = UIScale();
	const UGBAttributeComponent* Attributes = Player->GetAttributes();
	if (!Attributes)
	{
		return;
	}
	const float Split = GBCyLocal::BootSplit(GetRealTime() - SkinChangeTime, S) + GBCyLocal::BootSplit(GetRealTime() - LastDamageTime, S) * 0.6f;
	const float MaxHealth = Attributes->GetMaxHealth();
	const float MaxMana = Attributes->GetMaxMana();
	const float HealthFrac = MaxHealth > 0.f ? Attributes->GetHealth() / MaxHealth : 0.f;
	const float ManaFrac = MaxMana > 0.f ? Attributes->GetMana() / MaxMana : 0.f;
	const bool bLow = HealthFrac <= 0.3f;

	// Health readout box.
	const float BoxW = 104.f * S;
	const float BoxH = 62.f * S;
	DrawGlow(X + 200.f * S, Y + 30.f * S, 330.f * S, 90.f * S, FLinearColor(0.f, 0.f, 0.f, 0.32f));
	CyBox(X, Y, BoxW, BoxH, 12.f * S, GBAlpha(GBCy::RedBlack, 0.8f), GBAlpha(bLow ? GBCy::Yellow : GBCy::Red, 0.9f), 0x5);
	FillRect(X - 6.f * S, Y + 10.f * S, 2.f * S, BoxH - 20.f * S, GBCy::Red);
	FCyTextStyle Tag;
	Tag.Face = ECyFace::Mono;
	Tag.Px = 12.f;
	Tag.Color = GBCy::RedDim;
	DrawCyText(TEXT("HP"), X + 14.f * S, Y + 6.f * S, Tag, 0.f);
	FCyTextStyle Big;
	Big.Face = ECyFace::Condensed;
	Big.Px = 44.f;
	Big.Color = bLow ? GBCy::Yellow : GBCy::Pale;
	Big.Split = Split;
	DrawCyText(FString::Printf(TEXT("%d"), FMath::CeilToInt(Attributes->GetHealth())), X + BoxW - 12.f * S, Y + 8.f * S, Big, 1.f);

	// Bars, with a faint red bloom under the health bar.
	const float BX = X + BoxW + 14.f * S;
	DrawGlow(BX + 165.f * S, Y + 14.f * S, 190.f * S, 22.f * S, GBAlpha(bLow ? GBCy::Yellow : GBCy::Red, 0.18f));
	CySlantBar(BX, Y + 6.f * S, 330.f * S, 16.f * S, 7.f * S, HealthFrac, 22, bLow ? GBCy::Yellow : GBCy::Red, GBAlpha(GBCy::RedDeep, 0.85f), HealthTrail01);
	CySlantBar(BX, Y + 30.f * S, 250.f * S, 8.f * S, 4.f * S, ManaFrac, 18, GBCy::Cyan, GBAlpha(GBCy::CyanDeep, 0.9f));
	FCyTextStyle Small;
	Small.Face = ECyFace::Condensed;
	Small.Px = 18.f;
	Small.Color = GBCy::Cyan;
	DrawCyText(FString::Printf(TEXT("%d"), FMath::FloorToInt(Attributes->GetMana())), BX + 262.f * S, Y + 24.f * S, Small, 0.f);
	Tag.Color = GBAlpha(GBCy::CyanDim, 1.f);
	DrawCyText(TEXT("MANA"), BX + 292.f * S, Y + 28.f * S, Tag, 0.f);

	// Max health label at the end of the health bar.
	Tag.Color = GBCy::RedDim;
	DrawCyText(FString::Printf(TEXT("/%d"), FMath::RoundToInt(MaxHealth)), BX + 338.f * S, Y + 6.f * S, Tag, 0.f);

	// Dash.
	const float Remaining = Player->GetDashCooldownRemaining();
	const UGBStatsComponent* Stats = Player->GetStats();
	const float Total = Stats ? Stats->GetStat(GBStat::DashCooldown, 0.f) : 0.f;
	const float DashFrac = (Remaining <= 0.f || Total <= 0.f) ? 1.f : 1.f - Remaining / Total;
	const bool bDashReady = DashFrac >= 1.f;
	Tag.Color = bDashReady ? GBCy::Cyan : GBCy::RedDim;
	DrawCyText(TEXT("DASH"), BX, Y + 44.f * S, Tag, 0.f);
	CySlantBar(BX + 40.f * S, Y + 47.f * S, 110.f * S, 6.f * S, 3.f * S, DashFrac, 4, bDashReady ? GBCy::Cyan : GBCy::RedDim, GBAlpha(GBCy::RedDeep, 0.8f));
	if (bLow)
	{
		FCyTextStyle Warn;
		Warn.Face = ECyFace::Condensed;
		Warn.Px = 16.f;
		Warn.Spacing = 150;
		Warn.bUpper = true;
		Warn.Color = GBAlpha(GBCy::Yellow, 0.6f + 0.4f * FMath::Sin((float)GetRealTime() * 8.f));
		DrawCyText(TEXT("Critical"), BX + 170.f * S, Y + 41.f * S, Warn, 0.f);
	}
}

void AGBHUD::DrawCyberWeapon(AGBPlayerCharacter* Player, AGBHeldItem* Item, float Right, float Bottom)
{
	// Layout, right-aligned on Right:
	//   [element tag]  WEAPON NAME
	//   -------------------------------------- rule
	//   [enchant]          AMMO | RESERVE
	//   [enchanted / MP]   ////////////// pips
	const float S = UIScale();
	const float Split = GBCyLocal::BootSplit(GetRealTime() - SkinChangeTime, S);
	const float BlockW = 300.f * S;
	const float Left = Right - BlockW;
	DrawGlow(Right - BlockW * 0.45f, Bottom - 70.f * S, BlockW * 0.75f, 130.f * S, FLinearColor(0.f, 0.f, 0.f, 0.34f));

	FCyTextStyle NameStyle;
	NameStyle.Face = ECyFace::Condensed;
	NameStyle.Px = 26.f;
	NameStyle.Spacing = 140;
	NameStyle.bUpper = true;
	NameStyle.Color = GBCy::Red;
	NameStyle.Split = Split;

	const float NameY = Bottom - 158.f * S;
	const float RuleY = Bottom - 118.f * S;
	const float NumTop = Bottom - 110.f * S;
	const float PipsH = 12.f * S;
	const float PipsY = Bottom - PipsH;

	const FVector2D NameSize = DrawCyText(Item->GetDisplayName(), Right, NameY, NameStyle, 1.f);
	FillRect(Left + 8.f * S, RuleY, BlockW - 8.f * S, 1.f, GBAlpha(GBCy::Red, 0.65f));
	FillRect(Right - 60.f * S, RuleY - 1.f * S, 60.f * S, 3.f * S, GBCy::Red);
	TArray<FVector2D> Tick = { FVector2D(Left, RuleY + 6.f * S), FVector2D(Left + 6.f * S, RuleY), FVector2D(Left + 10.f * S, RuleY), FVector2D(Left + 4.f * S, RuleY + 6.f * S) };
	FillPoly(Tick, GBCy::Red);

	// Weapon enchant: the active element as a tag left of the name.
	UGBSpellbookComponent* Spellbook = Player->GetSpellbook();
	const EGBElement Element = Spellbook ? Spellbook->GetEnchantElement() : EGBElement::Fire;
	const FLinearColor ElementColor = GBHudText::ElementColor(Element);
	const FGBActiveStatusEffect* Enchant = nullptr;
	if (const UGBStatusEffectComponent* Effects = Player->GetStatusEffects())
	{
		for (const FGBActiveStatusEffect& Effect : Effects->GetActiveEffects())
		{
			if (Effect.EffectId.ToString().StartsWith(TEXT("Enchant_")))
			{
				Enchant = &Effect;
				break;
			}
		}
	}
	if (Enchant)
	{
		FCyTextStyle Tag;
		Tag.Face = ECyFace::Condensed;
		Tag.Px = 15.f;
		Tag.Spacing = 120;
		Tag.bUpper = true;
		Tag.Color = GBCy::Ink;
		const FString TagText = GBHudText::ElementName(Element);
		const FVector2D TS = MeasureCyText(TagText, Tag);
		const float TW = TS.X + 18.f * S;
		const float TH = 22.f * S;
		const float TX = Right - NameSize.X - 14.f * S - TW;
		const float TY = NameY + (NameSize.Y - TH) * 0.5f;
		CyBox(TX, TY, TW, TH, 6.f * S, ElementColor, FLinearColor::Transparent, 0x4);
		DrawCyText(TagText, TX + TW * 0.5f, TY + (TH - TS.Y) * 0.5f, Tag, 0.5f);
	}

	if (Item->UsesAmmo())
	{
		const FGBAmmoState Ammo = Item->GetAmmo();
		const int32 MagSize = FMath::Max(0, FMath::RoundToInt(Item->GetStat(GBStat::MagazineSize, 0.f)));
		const bool bPerRound = Item->ReloadsPerRound();

		FCyTextStyle Reserve;
		Reserve.Face = ECyFace::Condensed;
		Reserve.Px = 36.f;
		Reserve.Color = Ammo.SpareMagazines > 0 ? GBCy::Cyan : GBCy::Grey;
		const FString ReserveText = FString::Printf(TEXT("%d"), Ammo.SpareMagazines);
		const FVector2D ReserveSize = MeasureCyText(ReserveText, Reserve);
		FCyTextStyle Unit;
		Unit.Face = ECyFace::Mono;
		Unit.Px = 12.f;
		Unit.Color = GBCy::CyanDim;
		const FString UnitText = bPerRound ? TEXT("SHELLS") : TEXT("MAGS");
		const float ReserveW = FMath::Max(ReserveSize.X, MeasureCyText(UnitText, Unit).X);
		const float ReserveX = Right - ReserveW;
		DrawCyText(ReserveText, ReserveX, NumTop + 8.f * S, Reserve, 0.f);
		DrawCyText(UnitText, ReserveX, NumTop + 8.f * S + ReserveSize.Y * 0.92f, Unit, 0.f);

		FCyTextStyle BigAmmo;
		BigAmmo.Face = ECyFace::Condensed;
		BigAmmo.Px = 80.f;
		BigAmmo.Color = Ammo.InMagazine > 0 ? GBCy::Pale : GBCy::Red;
		BigAmmo.Split = Split;
		const FString AmmoText = FString::Printf(TEXT("%d"), Ammo.InMagazine);
		const float DivX = ReserveX - 16.f * S;
		FillRect(DivX, NumTop + 14.f * S, 1.f, 62.f * S, GBCy::RedDim);
		DrawCyText(AmmoText, DivX - 16.f * S, NumTop - 6.f * S, BigAmmo, 1.f);

		// Magazine pips, or the reload bar.
		const float PipsLeft = Right - 260.f * S;
		if (Item->IsReloading())
		{
			CySlantBar(PipsLeft, PipsY + 2.f * S, Right - PipsLeft, 8.f * S, 4.f * S, Item->GetReloadProgress(), 14, GBCy::Yellow, GBAlpha(GBCy::RedDeep, 0.85f));
			FCyTextStyle Reload;
			Reload.Face = ECyFace::Condensed;
			Reload.Px = 15.f;
			Reload.Spacing = 220;
			Reload.bUpper = true;
			Reload.Color = GBAlpha(GBCy::Yellow, 0.65f + 0.35f * FMath::Sin((float)GetRealTime() * 12.f));
			DrawCyText(TEXT("Reloading"), PipsLeft, PipsY - 20.f * S, Reload, 0.f);
		}
		else if (MagSize > 0)
		{
			const float Gap = 3.f * S;
			const float PipW = FMath::Clamp(((Right - PipsLeft) - (MagSize - 1) * Gap) / MagSize, 3.f * S, 10.f * S);
			const float TotalW = MagSize * PipW + (MagSize - 1) * Gap;
			const float PX0 = Right - TotalW;
			const float Sl = 4.f * S;
			for (int32 i = 0; i < MagSize; ++i)
			{
				const bool bLoaded = i >= MagSize - Ammo.InMagazine;
				const float PX = PX0 + i * (PipW + Gap);
				TArray<FVector2D> Pip = { FVector2D(PX + Sl, PipsY), FVector2D(PX + PipW + Sl, PipsY), FVector2D(PX + PipW, PipsY + PipsH), FVector2D(PX, PipsY + PipsH) };
				FillPoly(Pip, bLoaded ? GBCy::Red : GBAlpha(GBCy::RedDeep, 0.95f));
			}
			if (Ammo.InMagazine <= 0)
			{
				FCyTextStyle Warn;
				Warn.Face = ECyFace::Condensed;
				Warn.Px = 15.f;
				Warn.Spacing = 220;
				Warn.bUpper = true;
				Warn.Color = Ammo.SpareMagazines > 0 ? GBCy::Yellow : GBCy::Red;
				if (Ammo.SpareMagazines > 0)
				{
					const float KW = CyKey(PX0, PipsY - 28.f * S, TEXT("R"), GBCy::Yellow, 22.f);
					DrawCyText(TEXT("Reload"), PX0 + KW + 8.f * S, PipsY - 26.f * S, Warn, 0.f);
				}
				else
				{
					DrawCyText(TEXT("No ammo"), PX0, PipsY - 26.f * S, Warn, 0.f);
				}
			}
		}
	}

	// Enchant module at the left of the block: icon, key, cost or the enchant's time left.
	if (!Spellbook)
	{
		return;
	}
	const FName EnchantId = Spellbook->GetEnchantSpell();
	const float Box = 56.f * S;
	const float EX = Right - Box;
	const float EY = NameY - Box - 22.f * S;
	CyBox(EX, EY, Box, Box, 10.f * S, GBAlpha(GBCy::Ink, 0.72f), GBAlpha(ElementColor, 0.95f), 0x5);
	DrawGlow(EX + Box * 0.5f, EY + Box * 0.5f, Box * 0.6f, Box * 0.6f, GBAlpha(ElementColor, Enchant ? 0.35f : 0.16f));
	DrawSpriteFit(GBHudText::SpellIconId(EnchantId), EX + 10.f * S, EY + 10.f * S, Box - 20.f * S, Box - 20.f * S, FLinearColor::White);
	CyKey(EX - 10.f * S, EY - 10.f * S, TEXT("T"), GBCy::Cyan, 20.f);
	const float InfoRight = EX - 14.f * S;
	if (EnchantId != NAME_None)
	{
		const float Remaining = Spellbook->GetCooldownRemaining(EnchantId);
		const EGBCastFailReason Reason = Spellbook->CanCast(EnchantId);
		if (Remaining > 0.f)
		{
			const float Duration = Spellbook->GetCooldownDuration(EnchantId);
			const float Frac = Duration > 0.f ? FMath::Clamp(Remaining / Duration, 0.f, 1.f) : 1.f;
			FillRect(EX + 2.f * S, EY + Box * (1.f - Frac), Box - 4.f * S, Box * Frac - 2.f * S, FLinearColor(0.f, 0.f, 0.f, 0.6f));
		}
		else if (Reason != EGBCastFailReason::None && Reason != EGBCastFailReason::OnCooldown)
		{
			FillRect(EX + 2.f * S, EY + 2.f * S, Box - 4.f * S, Box - 4.f * S, FLinearColor(0.f, 0.f, 0.f, 0.55f));
		}
		FCyTextStyle Label;
		Label.Face = ECyFace::Mono;
		Label.Px = 12.f;
		Label.Color = GBCy::RedBright;
		FCyTextStyle Value;
		Value.Face = ECyFace::Condensed;
		Value.Px = 22.f;
		FString LabelText;
		FString ValueText;
		if (Enchant)
		{
			LabelText = TEXT("ENCHANTED");
			ValueText = FString::Printf(TEXT("%.1fs"), Enchant->TimeRemaining);
			Value.Color = ElementColor;
		}
		else if (Remaining > 0.f)
		{
			LabelText = TEXT("ENCHANT COOLDOWN");
			ValueText = FString::Printf(TEXT("%.1fs"), Remaining);
			Value.Color = GBCy::Pale;
		}
		else
		{
			LabelText = FString::Printf(TEXT("%s ENCHANT"), *GBHudText::ElementName(Element).ToUpper());
			ValueText = FString::Printf(TEXT("%d MP"), FMath::RoundToInt(Spellbook->GetManaCost(EnchantId)));
			Value.Color = Reason == EGBCastFailReason::NotEnoughMana ? GBCy::Red : GBCy::Cyan;
		}
		DrawCyText(LabelText, InfoRight, EY + 4.f * S, Label, 1.f);
		DrawCyText(ValueText, InfoRight, EY + 18.f * S, Value, 1.f);
		if (Enchant)
		{
			const float Frac = Enchant->TotalDuration > 0.f ? FMath::Clamp(Enchant->TimeRemaining / Enchant->TotalDuration, 0.f, 1.f) : 1.f;
			CySlantBar(InfoRight - 110.f * S, EY + Box - 8.f * S, 110.f * S, 5.f * S, 3.f * S, Frac, 10, ElementColor, GBAlpha(GBCy::RedDeep, 0.85f));
		}
	}
}

float AGBHUD::DrawCyberEffects(AGBPlayerCharacter* Player, float X, float Y)
{
	const UGBStatusEffectComponent* Effects = Player->GetStatusEffects();
	if (!Effects)
	{
		return Y;
	}
	const float S = UIScale();
	const UGBDataSubsystem* Data = UGBDataSubsystem::Get(this);
	for (const FGBActiveStatusEffect& Effect : Effects->GetActiveEffects())
	{
		const FGBStatusEffectDefinition* Def = Data ? Data->FindStatusEffect(Effect.EffectId) : nullptr;
		FString Name = (Def && !Def->DisplayName.IsEmpty()) ? Def->DisplayName : Effect.EffectId.ToString();
		if (Effect.Stacks > 1)
		{
			Name += FString::Printf(TEXT(" x%d"), Effect.Stacks);
		}
		const float EW = 236.f * S;
		const float EH = 26.f * S;
		CyBox(X, Y, EW, EH, 7.f * S, GBAlpha(GBCy::Ink, 0.6f), GBAlpha(GBCy::RedDim, 0.9f), 0x4);
		FillRect(X, Y, 3.f * S, EH, GBCy::Red);
		FCyTextStyle Text;
		Text.Face = ECyFace::Condensed;
		Text.Px = 16.f;
		Text.Spacing = 80;
		Text.bUpper = true;
		Text.Color = GBCy::RedBright;
		DrawCyText(GBHudText::Abbreviate(Name, 22), X + 12.f * S, Y + 3.f * S, Text, 0.f);
		if (Effect.TotalDuration > 0.f)
		{
			FCyTextStyle Time;
			Time.Face = ECyFace::Mono;
			Time.Px = 13.f;
			Time.Color = GBCy::Cyan;
			DrawCyText(FString::Printf(TEXT("%04.1f"), Effect.TimeRemaining), X + EW - 10.f * S, Y + 5.f * S, Time, 1.f);
			FillRect(X + 3.f * S, Y + EH - 2.f * S, (EW - 10.f * S) * FMath::Clamp(Effect.TimeRemaining / Effect.TotalDuration, 0.f, 1.f), 2.f * S, GBAlpha(GBCy::Red, 0.9f));
		}
		Y += EH + 6.f * S;
	}
	return Y;
}

void AGBHUD::DrawCyberShield(AGBPlayerCharacter* Player, float X, float Y)
{
	const UGBSpellbookComponent* Spellbook = Player->GetSpellbook();
	if (!Spellbook || !Spellbook->HasShield())
	{
		return;
	}
	const float S = UIScale();
	const bool bRaised = Spellbook->IsBlockRaised();
	const FLinearColor Ice = GBHudText::ElementColor(EGBElement::Ice);
	const float EW = 236.f * S;
	const float EH = 34.f * S;
	CyBox(X, Y, EW, EH, 8.f * S, GBAlpha(GBCy::Ink, 0.6f), GBAlpha(Ice, 0.9f), 0x5);
	FCyTextStyle Text;
	Text.Face = ECyFace::Condensed;
	Text.Px = 16.f;
	Text.Spacing = 100;
	Text.bUpper = true;
	Text.Color = Ice;
	DrawCyText(bRaised ? TEXT("Blocking") : TEXT("Ice shield"), X + 10.f * S, Y + 3.f * S, Text, 0.f);
	FCyTextStyle Time;
	Time.Face = ECyFace::Mono;
	Time.Px = 13.f;
	Time.Color = Ice;
	DrawCyText(FString::Printf(TEXT("%.1fs"), FMath::Max(0.f, Spellbook->GetShieldTimeRemaining())), X + EW - 10.f * S, Y + 5.f * S, Time, 1.f);
	CySlantBar(X + 10.f * S, Y + EH - 10.f * S, EW - 20.f * S, 5.f * S, 3.f * S, Spellbook->GetShieldHealthFraction(), 12, Ice, GBAlpha(GBCy::CyanDeep, 0.9f));
	if (!bRaised)
	{
		CyKey(X + EW + 8.f * S, Y + 5.f * S, TEXT("Q"), GBCy::Cyan, 24.f);
	}
}
