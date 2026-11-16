// The game's logo (ARCANE OVERRIDE), drawn on the title screen and in the end credits. Free functions, no reflection.
#pragma once

#include "CoreMinimal.h"

class UCanvas;

namespace GBLogo
{
	/** Draws the logo centered on CX with its top at TopY, fitted inside MaxW x MaxH, aspect kept.
	 *  Returns the drawn height, or 0 if the texture is missing (the caller then draws the title text). */
	float Draw(UCanvas* Canvas, float CX, float TopY, float MaxW, float MaxH, float Alpha = 1.f);
}
