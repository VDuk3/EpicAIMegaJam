// Small helpers and the two skins' palettes, shared by the GBHUD*.cpp files (private to the UI folder).
#pragma once

#include "CoreMinimal.h"
#include "Core/GBTypes.h"
#include "UI/GBPixelFont.h"

/** Game.json "Values" keys of the floating damage numbers (code defaults next to each read in GBHUD_DamageNumbers.cpp). */
namespace GBHudKey
{
	inline const FName DamageNumberLifetime(TEXT("DamageNumberLifetime"));                 // seconds (world time)
	inline const FName DamageNumberRiseSpeed(TEXT("DamageNumberRiseSpeed"));               // pixels per second at 1080p
	inline const FName DamageNumberDriftSpeed(TEXT("DamageNumberDriftSpeed"));             // sideways pixels per second at 1080p
	inline const FName DamageNumberTextSize(TEXT("DamageNumberTextSize"));                 // line height at 1080p
	inline const FName DamageNumberBigScale(TEXT("DamageNumberBigScale"));                 // size multiplier of an effective hit
	inline const FName DamageNumberBigHealthFraction(TEXT("DamageNumberBigHealthFraction")); // hit >= this fraction of max health = big
	inline const FName DamageNumberFadeStart(TEXT("DamageNumberFadeStart"));               // 0..1 of the life when the fade begins
	inline const FName DamageNumberMaxAlive(TEXT("DamageNumberMaxAlive"));
	inline const FName DamageNumberMergeWindow(TEXT("DamageNumberMergeWindow"));           // seconds: hits on one target merge
	inline const FName TestSkillPointAmount(TEXT("TestSkillPointAmount"));                 // skill points given by the test button in the skill menu
}

namespace GBHudText
{
	FString ElementName(EGBElement Element);
	FLinearColor ElementColor(EGBElement Element);
	/** Shortens a name to MaxLen characters: "Lightning bolt" -> "L.bolt", "Flamethrower" -> "Flameth.". */
	FString Abbreviate(const FString& Name, int32 MaxLen);
	/** Display name of a spell, or its id if it has none. */
	FString SpellName(const UObject* Context, FName SpellId);
	/** Sprite id of a spell's HUD icon ("icon_" + lower-case spell id). */
	FName SpellIconId(FName SpellId);
}

/** sRGB hex -> linear color. */
inline FLinearColor GBHex(const TCHAR* Hex, float Alpha = 1.f)
{
	FLinearColor Color = FLinearColor::FromSRGBColor(FColor::FromHex(Hex));
	Color.A = Alpha;
	return Color;
}

inline FLinearColor GBAlpha(const FLinearColor& Color, float Alpha)
{
	return FLinearColor(Color.R, Color.G, Color.B, Color.A * Alpha);
}

/**
 * 90s RPG skin: dark stone, worn iron and gilt, bone-colored text (Design/VisualStyleGuide.md ramps: obsidian,
 * iron, bone, blood, arcane, ice; the gilt ramp is the UI's own, from the wood and bone ramps).
 */
namespace GBRpg
{
	inline const FLinearColor Outline = GBHex(TEXT("#07060A"));
	inline const FLinearColor StoneDeep = GBHex(TEXT("#121219"));
	inline const FLinearColor Stone0 = GBHex(TEXT("#1D1D25"));
	inline const FLinearColor Stone1 = GBHex(TEXT("#252734"));
	inline const FLinearColor Stone2 = GBHex(TEXT("#32343C"));
	inline const FLinearColor Stone3 = GBHex(TEXT("#46475A"));
	inline const FLinearColor StoneViolet = GBHex(TEXT("#655478"));
	inline const FLinearColor Iron0 = GBHex(TEXT("#262827"));
	inline const FLinearColor Iron1 = GBHex(TEXT("#373837"));
	inline const FLinearColor Iron2 = GBHex(TEXT("#555957"));
	inline const FLinearColor Iron3 = GBHex(TEXT("#758685"));
	inline const FLinearColor Iron4 = GBHex(TEXT("#9DAAA9"));
	inline const FLinearColor Gold0 = GBHex(TEXT("#41290D"));
	inline const FLinearColor Gold1 = GBHex(TEXT("#7A5410"));
	inline const FLinearColor Gold2 = GBHex(TEXT("#B4862A"));
	inline const FLinearColor Gold3 = GBHex(TEXT("#E2C26A"));
	inline const FLinearColor Gold4 = GBHex(TEXT("#F5F3CF"));
	inline const FLinearColor Bone = GBHex(TEXT("#E6E4B3"));
	inline const FLinearColor BoneDim = GBHex(TEXT("#A19D7A"));
	inline const FLinearColor BoneDark = GBHex(TEXT("#6E6A55"));
	inline const FLinearColor Ember = GBHex(TEXT("#F3610B"));
	inline const FLinearColor EmberLight = GBHex(TEXT("#FFB45A"));
	inline const FLinearColor Blood = GBHex(TEXT("#D21A12"));
	inline const FLinearColor Arcane = GBHex(TEXT("#8548C0"));
	inline const FLinearColor ArcaneLight = GBHex(TEXT("#C0A2E1"));
	inline const FLinearColor Ice = GBHex(TEXT("#00F6FF"));
	inline const FLinearColor Good = GBHex(TEXT("#7CCB4A"));

	inline const FLinearColor HealthRamp[4] = { GBHex(TEXT("#4B0A0A")), GBHex(TEXT("#9A0A06")), GBHex(TEXT("#DA2418")), GBHex(TEXT("#FF9A7A")) };
	inline const FLinearColor ManaRamp[4] = { GBHex(TEXT("#0E1458")), GBHex(TEXT("#2234B0")), GBHex(TEXT("#4C78EE")), GBHex(TEXT("#B8D8FF")) };
	inline const FLinearColor IceRamp[4] = { GBHex(TEXT("#0E4A55")), GBHex(TEXT("#1C8B9B")), GBHex(TEXT("#14AFB4")), GBHex(TEXT("#B8FFFF")) };
	inline const FLinearColor StaminaRamp[4] = { GBHex(TEXT("#41290D")), GBHex(TEXT("#926806")), GBHex(TEXT("#E2C26A")), GBHex(TEXT("#F5F3CF")) };

	// Baked text inks (top, middle, bottom of the capitals).
	inline const FGBPixelInk InkGold(FColor::FromHex(TEXT("FFF6C8")), FColor::FromHex(TEXT("F0C658")), FColor::FromHex(TEXT("B07418")));
	inline const FGBPixelInk InkBone(FColor::FromHex(TEXT("F5F3DA")), FColor::FromHex(TEXT("E6E2B8")), FColor::FromHex(TEXT("C2BB8C")));
	inline const FGBPixelInk InkBlood(FColor::FromHex(TEXT("FF9070")), FColor::FromHex(TEXT("F02A18")), FColor::FromHex(TEXT("8A0A08")));
	inline const FGBPixelInk InkMana(FColor::FromHex(TEXT("D8ECFF")), FColor::FromHex(TEXT("7AA8FF")), FColor::FromHex(TEXT("3A58D8")));
	inline const FGBPixelInk InkIce(FColor::FromHex(TEXT("E8FFFF")), FColor::FromHex(TEXT("60F0F8")), FColor::FromHex(TEXT("1C8B9B")));
	inline const FGBPixelInk InkEmber(FColor::FromHex(TEXT("FFF0A0")), FColor::FromHex(TEXT("FFA030")), FColor::FromHex(TEXT("D04008")));
}

/** Cyberpunk skin: coral red lines and type on near-black, cyan for values and interaction, yellow for alerts. */
namespace GBCy
{
	inline const FLinearColor Red = GBHex(TEXT("#FF5A4E"));
	inline const FLinearColor RedBright = GBHex(TEXT("#FF9B90"));
	inline const FLinearColor RedDim = GBHex(TEXT("#A3352E"));
	inline const FLinearColor RedDeep = GBHex(TEXT("#4A1416"));
	inline const FLinearColor RedBlack = GBHex(TEXT("#160707"));
	inline const FLinearColor Pale = GBHex(TEXT("#FFE2DC"));
	inline const FLinearColor Cyan = GBHex(TEXT("#55F4F0"));
	inline const FLinearColor CyanDim = GBHex(TEXT("#1F7C80"));
	inline const FLinearColor CyanDeep = GBHex(TEXT("#0A2C30"));
	inline const FLinearColor Yellow = GBHex(TEXT("#F4E44A"));
	inline const FLinearColor Green = GBHex(TEXT("#5CF29A"));
	inline const FLinearColor Ink = GBHex(TEXT("#07080C"));
	inline const FLinearColor Grey = GBHex(TEXT("#6A6E78"));
}
