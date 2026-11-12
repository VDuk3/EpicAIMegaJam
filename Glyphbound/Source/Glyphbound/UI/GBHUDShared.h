// Small text helpers shared by GBHUD.cpp and GBHUD_Menus.cpp (private to the UI folder).
#pragma once

#include "CoreMinimal.h"
#include "Core/GBTypes.h"

namespace GBHudText
{
	FString ElementName(EGBElement Element);
	FLinearColor ElementColor(EGBElement Element);
	/** Shortens a name to MaxLen characters: "Lightning bolt" -> "L.bolt", "Flamethrower" -> "Flameth.". */
	FString Abbreviate(const FString& Name, int32 MaxLen);
	/** Display name of a spell, or its id if it has none. */
	FString SpellName(const UObject* Context, FName SpellId);
}
