// The game's own pixel font for the 90s RPG skin (GDD 13): 5x7 capitals, lowercase with descenders, drawn as text
// rows in GBPixelFont.cpp during the jam and baked here into small nearest-filtered atlases (no font asset import).
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GBPixelFont.generated.h"

class UTexture2D;

/** A vertical color ramp baked into the glyph pixels: Top for the first rows, Mid in the middle, Bottom for the last. */
struct FGBPixelInk
{
	FColor Top = FColor::White;
	FColor Mid = FColor::White;
	FColor Bottom = FColor::White;

	FGBPixelInk() = default;
	FGBPixelInk(FColor InTop, FColor InMid, FColor InBottom) : Top(InTop), Mid(InMid), Bottom(InBottom) {}
	/** One flat color (tinted at draw time). */
	static FGBPixelInk Flat() { return FGBPixelInk(); }
	uint32 Key() const { return HashCombine(HashCombine(GetTypeHash(Top), GetTypeHash(Mid)), GetTypeHash(Bottom)); }
};

/**
 * Glyph metrics, in font pixels: capitals are 7 rows tall from the top of the cell, two more rows hold descenders,
 * one empty column separates glyphs. Every glyph has a 1-pixel outline around it in the atlas.
 */
UCLASS(Transient)
class GLYPHBOUND_API UGBPixelFont : public UObject
{
	GENERATED_BODY()

public:
	static constexpr int32 CapHeight = 7;
	static constexpr int32 CellRows = 9;
	static constexpr int32 Spacing = 1;

	struct FGlyph
	{
		/** Top-left of the glyph's padded cell in the atlas (the padding is the 1-pixel outline ring). */
		int32 AtlasX = 0;
		int32 AtlasY = 0;
		int32 Width = 0;
	};

	/** Builds the glyph table and the outline atlas. Safe to call more than once. */
	void Init();

	const FGlyph* FindGlyph(TCHAR Char) const;
	/** Width in font pixels of a single line (no trailing spacing). */
	int32 MeasureWidth(const FString& Text) const;

	/** Atlas with the glyph pixels colored by Ink (built on first use, cached). */
	UTexture2D* GetInkAtlas(const FGBPixelInk& Ink);
	/** Atlas with only the outline ring of every glyph, white (tinted at draw time). */
	UTexture2D* GetOutlineAtlas() const { return OutlineAtlas; }
	FIntPoint GetAtlasSize() const { return AtlasSize; }

	/** Pure: glyph rows of a character ('#' = ink), or null. Used by the tests. */
	static const TCHAR* GetGlyphRows(TCHAR Char);

private:
	TMap<TCHAR, FGlyph> Glyphs;
	/** Glyph masks in atlas space (1 = ink), row-major. */
	TArray<uint8> Mask;
	FIntPoint AtlasSize = FIntPoint::ZeroValue;

	UPROPERTY() TObjectPtr<UTexture2D> OutlineAtlas;
	UPROPERTY() TMap<uint32, TObjectPtr<UTexture2D>> InkAtlases;
};
