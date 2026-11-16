#include "UI/GBPixelFont.h"

#include "UI/GBPixelSpriteLibrary.h"
#include "Engine/Texture2D.h"

namespace GBPixelFontData
{
	struct FEntry
	{
		TCHAR Char;
		const TCHAR* Rows;
	};

	// Rows from the top of the cell, separated by '|'. Rows 0-6 sit above the baseline, rows 7-8 are descenders;
	// shorter glyphs are padded with empty rows at the bottom. Every row of a glyph has the same width.
	static const FEntry Entries[] = {
		{ TEXT('A'), TEXT(".###.|#...#|#...#|#####|#...#|#...#|#...#") },
		{ TEXT('B'), TEXT("####.|#...#|#...#|####.|#...#|#...#|####.") },
		{ TEXT('C'), TEXT(".###.|#...#|#....|#....|#....|#...#|.###.") },
		{ TEXT('D'), TEXT("####.|#...#|#...#|#...#|#...#|#...#|####.") },
		{ TEXT('E'), TEXT("#####|#....|#....|####.|#....|#....|#####") },
		{ TEXT('F'), TEXT("#####|#....|#....|####.|#....|#....|#....") },
		{ TEXT('G'), TEXT(".###.|#...#|#....|#.###|#...#|#...#|.####") },
		{ TEXT('H'), TEXT("#...#|#...#|#...#|#####|#...#|#...#|#...#") },
		{ TEXT('I'), TEXT("###|.#.|.#.|.#.|.#.|.#.|###") },
		{ TEXT('J'), TEXT("..###|...#.|...#.|...#.|...#.|#..#.|.##..") },
		{ TEXT('K'), TEXT("#...#|#..#.|#.#..|##...|#.#..|#..#.|#...#") },
		{ TEXT('L'), TEXT("#....|#....|#....|#....|#....|#....|#####") },
		{ TEXT('M'), TEXT("#...#|##.##|#.#.#|#.#.#|#...#|#...#|#...#") },
		{ TEXT('N'), TEXT("#...#|##..#|##..#|#.#.#|#..##|#..##|#...#") },
		{ TEXT('O'), TEXT(".###.|#...#|#...#|#...#|#...#|#...#|.###.") },
		{ TEXT('P'), TEXT("####.|#...#|#...#|####.|#....|#....|#....") },
		{ TEXT('Q'), TEXT(".###.|#...#|#...#|#...#|#.#.#|#..#.|.##.#") },
		{ TEXT('R'), TEXT("####.|#...#|#...#|####.|#.#..|#..#.|#...#") },
		{ TEXT('S'), TEXT(".####|#....|#....|.###.|....#|....#|####.") },
		{ TEXT('T'), TEXT("#####|..#..|..#..|..#..|..#..|..#..|..#..") },
		{ TEXT('U'), TEXT("#...#|#...#|#...#|#...#|#...#|#...#|.###.") },
		{ TEXT('V'), TEXT("#...#|#...#|#...#|#...#|.#.#.|.#.#.|..#..") },
		{ TEXT('W'), TEXT("#...#|#...#|#...#|#.#.#|#.#.#|##.##|#...#") },
		{ TEXT('X'), TEXT("#...#|#...#|.#.#.|..#..|.#.#.|#...#|#...#") },
		{ TEXT('Y'), TEXT("#...#|#...#|.#.#.|..#..|..#..|..#..|..#..") },
		{ TEXT('Z'), TEXT("#####|....#|...#.|..#..|.#...|#....|#####") },

		{ TEXT('a'), TEXT(".....|.....|.###.|....#|.####|#...#|.####") },
		{ TEXT('b'), TEXT("#....|#....|####.|#...#|#...#|#...#|####.") },
		{ TEXT('c'), TEXT("....|....|.###|#...|#...|#...|.###") },
		{ TEXT('d'), TEXT("....#|....#|.####|#...#|#...#|#...#|.####") },
		{ TEXT('e'), TEXT(".....|.....|.###.|#...#|#####|#....|.###.") },
		{ TEXT('f'), TEXT("..##|.#..|####|.#..|.#..|.#..|.#..") },
		{ TEXT('g'), TEXT(".....|.....|.####|#...#|#...#|#...#|.####|....#|.###.") },
		{ TEXT('h'), TEXT("#....|#....|####.|#...#|#...#|#...#|#...#") },
		{ TEXT('i'), TEXT("#|.|#|#|#|#|#") },
		{ TEXT('j'), TEXT("..#|...|..#|..#|..#|..#|..#|#.#|.#.") },
		{ TEXT('k'), TEXT("#...|#...|#..#|#.#.|##..|#.#.|#..#") },
		{ TEXT('l'), TEXT("#.|#.|#.|#.|#.|#.|.#") },
		{ TEXT('m'), TEXT(".....|.....|##.#.|#.#.#|#.#.#|#.#.#|#.#.#") },
		{ TEXT('n'), TEXT(".....|.....|####.|#...#|#...#|#...#|#...#") },
		{ TEXT('o'), TEXT(".....|.....|.###.|#...#|#...#|#...#|.###.") },
		{ TEXT('p'), TEXT(".....|.....|####.|#...#|#...#|#...#|####.|#....|#....") },
		{ TEXT('q'), TEXT(".....|.....|.####|#...#|#...#|#...#|.####|....#|....#") },
		{ TEXT('r'), TEXT("....|....|#.##|##..|#...|#...|#...") },
		{ TEXT('s'), TEXT(".....|.....|.####|#....|.###.|....#|####.") },
		{ TEXT('t'), TEXT(".#..|.#..|####|.#..|.#..|.#..|..##") },
		{ TEXT('u'), TEXT(".....|.....|#...#|#...#|#...#|#...#|.####") },
		{ TEXT('v'), TEXT(".....|.....|#...#|#...#|#...#|.#.#.|..#..") },
		{ TEXT('w'), TEXT(".....|.....|#...#|#...#|#.#.#|#.#.#|.#.#.") },
		{ TEXT('x'), TEXT(".....|.....|#...#|.#.#.|..#..|.#.#.|#...#") },
		{ TEXT('y'), TEXT(".....|.....|#...#|#...#|#...#|#...#|.####|....#|.###.") },
		{ TEXT('z'), TEXT(".....|.....|#####|...#.|..#..|.#...|#####") },

		{ TEXT('0'), TEXT(".###.|#...#|#..##|#.#.#|##..#|#...#|.###.") },
		{ TEXT('1'), TEXT("..#..|.##..|..#..|..#..|..#..|..#..|.###.") },
		{ TEXT('2'), TEXT(".###.|#...#|....#|...#.|..#..|.#...|#####") },
		{ TEXT('3'), TEXT("####.|....#|....#|.###.|....#|....#|####.") },
		{ TEXT('4'), TEXT("...#.|..##.|.#.#.|#..#.|#####|...#.|...#.") },
		{ TEXT('5'), TEXT("#####|#....|####.|....#|....#|#...#|.###.") },
		{ TEXT('6'), TEXT(".###.|#....|#....|####.|#...#|#...#|.###.") },
		{ TEXT('7'), TEXT("#####|....#|...#.|..#..|..#..|..#..|..#..") },
		{ TEXT('8'), TEXT(".###.|#...#|#...#|.###.|#...#|#...#|.###.") },
		{ TEXT('9'), TEXT(".###.|#...#|#...#|.####|....#|....#|.###.") },

		{ TEXT(' '), TEXT("...") },
		{ TEXT('.'), TEXT(".|.|.|.|.|.|#") },
		{ TEXT(','), TEXT("..|..|..|..|..|.#|.#|#.") },
		{ TEXT(':'), TEXT(".|.|#|.|.|.|#") },
		{ TEXT(';'), TEXT("..|..|.#|..|..|..|.#|#.") },
		{ TEXT('!'), TEXT("#|#|#|#|#|.|#") },
		{ TEXT('?'), TEXT(".###.|#...#|....#|...#.|..#..|.....|..#..") },
		{ TEXT('-'), TEXT("....|....|....|####") },
		{ TEXT('+'), TEXT(".....|..#..|..#..|#####|..#..|..#..") },
		{ TEXT('='), TEXT("....|....|####|....|####") },
		{ TEXT('/'), TEXT("....#|....#|...#.|..#..|.#...|#....|#....") },
		{ TEXT('\\'), TEXT("#....|#....|.#...|..#..|...#.|....#|....#") },
		{ TEXT('%'), TEXT("##..#|##..#|...#.|..#..|.#...|#..##|#..##") },
		{ TEXT('('), TEXT("..#|.#.|#..|#..|#..|.#.|..#") },
		{ TEXT(')'), TEXT("#..|.#.|..#|..#|..#|.#.|#..") },
		{ TEXT('['), TEXT("###|#..|#..|#..|#..|#..|###") },
		{ TEXT(']'), TEXT("###|..#|..#|..#|..#|..#|###") },
		{ TEXT('<'), TEXT("....|...#|..#.|.#..|..#.|...#") },
		{ TEXT('>'), TEXT("....|#...|.#..|..#.|.#..|#...") },
		{ TEXT('\''), TEXT("#|#") },
		{ TEXT('"'), TEXT("#.#|#.#") },
		{ TEXT('*'), TEXT(".....|#.#.#|.###.|#####|.###.|#.#.#") },
		{ TEXT('_'), TEXT(".....|.....|.....|.....|.....|.....|#####") },
		{ TEXT('|'), TEXT("#|#|#|#|#|#|#|#|#") },
		{ TEXT('^'), TEXT("..#..|.#.#.|#...#") },
		{ TEXT('#'), TEXT(".#.#.|#####|.#.#.|.#.#.|.#.#.|#####|.#.#.") },
		{ TEXT('&'), TEXT(".##..|#..#.|#.#..|.#...|#.#.#|#..#.|.##.#") },
		{ TEXT('~'), TEXT(".....|.....|.#...|#.#.#|...#.") },
		{ (TCHAR)0x00E1, TEXT("...#.|..#..|.###.|....#|.####|#...#|.####") },
		{ (TCHAR)0x00ED, TEXT("..#|.#.|.#.|.#.|.#.|.#.|.#.") },
		{ (TCHAR)0x00F3, TEXT("...#.|..#..|.###.|#...#|#...#|#...#|.###.") },
		{ TEXT('@'), TEXT(".###.|#...#|#.###|#.#.#|#.###|#....|.###.") },
	};

	static constexpr int32 AtlasColumns = 16;
	// Padded cell: 5 columns of glyph at most + the outline ring on each side; 9 rows + the ring.
	static constexpr int32 CellW = 5 + 2;
	static constexpr int32 CellH = UGBPixelFont::CellRows + 2;

	static int32 RowWidth(const TCHAR* Rows)
	{
		int32 Width = 0;
		while (Rows[Width] != 0 && Rows[Width] != TEXT('|'))
		{
			++Width;
		}
		return Width;
	}

	static FColor LerpColor(const FColor& A, const FColor& B, float T)
	{
		return FColor(
			(uint8)FMath::RoundToInt(FMath::Lerp((float)A.R, (float)B.R, T)),
			(uint8)FMath::RoundToInt(FMath::Lerp((float)A.G, (float)B.G, T)),
			(uint8)FMath::RoundToInt(FMath::Lerp((float)A.B, (float)B.B, T)),
			255);
	}
}

const TCHAR* UGBPixelFont::GetGlyphRows(TCHAR Char)
{
	for (const GBPixelFontData::FEntry& Entry : GBPixelFontData::Entries)
	{
		if (Entry.Char == Char)
		{
			return Entry.Rows;
		}
	}
	return nullptr;
}

void UGBPixelFont::Init()
{
	using namespace GBPixelFontData;
	if (OutlineAtlas)
	{
		return;
	}
	const int32 Count = UE_ARRAY_COUNT(Entries);
	const int32 RowsOfCells = (Count + AtlasColumns - 1) / AtlasColumns;
	AtlasSize = FIntPoint(AtlasColumns * CellW, RowsOfCells * CellH);
	Mask.SetNumZeroed(AtlasSize.X * AtlasSize.Y);

	for (int32 Index = 0; Index < Count; ++Index)
	{
		const FEntry& Entry = Entries[Index];
		FGlyph Glyph;
		Glyph.AtlasX = (Index % AtlasColumns) * CellW;
		Glyph.AtlasY = (Index / AtlasColumns) * CellH;
		Glyph.Width = RowWidth(Entry.Rows);
		int32 Row = 0;
		int32 Col = 0;
		for (const TCHAR* C = Entry.Rows; *C; ++C)
		{
			if (*C == TEXT('|'))
			{
				++Row;
				Col = 0;
				continue;
			}
			if (*C == TEXT('#') && Row < CellRows && Col < CellW - 2)
			{
				Mask[(Glyph.AtlasY + 1 + Row) * AtlasSize.X + Glyph.AtlasX + 1 + Col] = 1;
			}
			++Col;
		}
		Glyphs.Add(Entry.Char, Glyph);
	}

	// Outline ring: every empty pixel next to ink (8 neighbours), inside the same cell.
	TArray<FColor> Pixels;
	Pixels.Init(FColor(0, 0, 0, 0), AtlasSize.X * AtlasSize.Y);
	for (int32 Y = 0; Y < AtlasSize.Y; ++Y)
	{
		for (int32 X = 0; X < AtlasSize.X; ++X)
		{
			if (Mask[Y * AtlasSize.X + X])
			{
				continue;
			}
			const int32 CellX0 = (X / CellW) * CellW;
			const int32 CellY0 = (Y / CellH) * CellH;
			bool bNear = false;
			for (int32 DY = -1; DY <= 1 && !bNear; ++DY)
			{
				for (int32 DX = -1; DX <= 1 && !bNear; ++DX)
				{
					const int32 NX = X + DX;
					const int32 NY = Y + DY;
					if (NX < CellX0 || NY < CellY0 || NX >= CellX0 + CellW || NY >= CellY0 + CellH)
					{
						continue;
					}
					bNear = Mask[NY * AtlasSize.X + NX] != 0;
				}
			}
			if (bNear)
			{
				Pixels[Y * AtlasSize.X + X] = FColor::White;
			}
		}
	}
	OutlineAtlas = UGBPixelSpriteLibrary::CreatePixelTexture(Pixels, AtlasSize.X, AtlasSize.Y, TEXT("PixelFontOutline"), false);
}

const UGBPixelFont::FGlyph* UGBPixelFont::FindGlyph(TCHAR Char) const
{
	if (const FGlyph* Glyph = Glyphs.Find(Char))
	{
		return Glyph;
	}
	// Letters missing in one case fall back to the other; anything else shows as '?'.
	if (const FGlyph* Upper = Glyphs.Find(FChar::ToUpper(Char)))
	{
		return Upper;
	}
	return Glyphs.Find(TEXT('?'));
}

int32 UGBPixelFont::MeasureWidth(const FString& Text) const
{
	int32 Width = 0;
	for (const TCHAR Char : Text)
	{
		if (const FGlyph* Glyph = FindGlyph(Char))
		{
			Width += Glyph->Width + Spacing;
		}
	}
	return FMath::Max(0, Width - Spacing);
}

UTexture2D* UGBPixelFont::GetInkAtlas(const FGBPixelInk& Ink)
{
	using namespace GBPixelFontData;
	Init();
	const uint32 Key = Ink.Key();
	if (TObjectPtr<UTexture2D>* Found = InkAtlases.Find(Key))
	{
		return *Found;
	}
	TArray<FColor> Pixels;
	Pixels.Init(FColor(0, 0, 0, 0), AtlasSize.X * AtlasSize.Y);
	for (int32 Y = 0; Y < AtlasSize.Y; ++Y)
	{
		// Ramp over the capital height: Top on row 0, Mid around row 3, Bottom from row 6 (descenders keep Bottom).
		const int32 Row = FMath::Clamp((Y % CellH) - 1, 0, CellRows - 1);
		const float T = FMath::Clamp(Row / (float)(CapHeight - 1), 0.f, 1.f);
		const FColor Color = T < 0.5f ? LerpColor(Ink.Top, Ink.Mid, T * 2.f) : LerpColor(Ink.Mid, Ink.Bottom, (T - 0.5f) * 2.f);
		for (int32 X = 0; X < AtlasSize.X; ++X)
		{
			if (Mask[Y * AtlasSize.X + X])
			{
				Pixels[Y * AtlasSize.X + X] = Color;
			}
		}
	}
	UTexture2D* Texture = UGBPixelSpriteLibrary::CreatePixelTexture(Pixels, AtlasSize.X, AtlasSize.Y, TEXT("PixelFontInk"), false);
	InkAtlases.Add(Key, Texture);
	return Texture;
}
