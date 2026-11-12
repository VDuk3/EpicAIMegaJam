// Small text helpers shared by GBHUD.cpp and GBHUD_Menus.cpp (private to the UI folder).
#pragma once

#include "CoreMinimal.h"
#include "Core/GBTypes.h"

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
}

namespace GBHudText
{
	FString ElementName(EGBElement Element);
	FLinearColor ElementColor(EGBElement Element);
	/** Shortens a name to MaxLen characters: "Lightning bolt" -> "L.bolt", "Flamethrower" -> "Flameth.". */
	FString Abbreviate(const FString& Name, int32 MaxLen);
	/** Display name of a spell, or its id if it has none. */
	FString SpellName(const UObject* Context, FName SpellId);
}
