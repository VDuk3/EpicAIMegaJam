#include "UI/GBPixelFont.h"
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"

#if WITH_DEV_AUTOMATION_TESTS

// The RPG skin's pixel font is hand-typed glyph rows: a mistyped row (one character more or less) shifts every
// pixel after it in the atlas, and a missing glyph turns into '?'. Neither is easy to spot while playing.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBPixelFontTest, "Glyphbound.UI.PixelFont",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGBPixelFontTest::RunTest(const FString& Parameters)
{
	// Every character the game's UI text can use has a glyph: letters, digits and the punctuation in our strings.
	const FString Needed = TEXT("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 .,:;!?-+=/%()[]'\"*_<>");
	for (const TCHAR Char : Needed)
	{
		const TCHAR* Rows = UGBPixelFont::GetGlyphRows(Char);
		if (!TestNotNull(FString::Printf(TEXT("glyph '%c' exists"), Char), Rows))
		{
			continue;
		}
		// Rectangular, 1-5 columns, at most 9 rows, only ink and blank.
		TArray<FString> Lines;
		FString(Rows).ParseIntoArray(Lines, TEXT("|"), false);
		TestTrue(FString::Printf(TEXT("glyph '%c' has 1-9 rows"), Char), Lines.Num() >= 1 && Lines.Num() <= UGBPixelFont::CellRows);
		const int32 Width = Lines.Num() > 0 ? Lines[0].Len() : 0;
		TestTrue(FString::Printf(TEXT("glyph '%c' is 1-5 wide"), Char), Width >= 1 && Width <= 5);
		for (const FString& Line : Lines)
		{
			TestEqual(FString::Printf(TEXT("glyph '%c' rows have one width"), Char), Line.Len(), Width);
			for (const TCHAR Pixel : Line)
			{
				TestTrue(FString::Printf(TEXT("glyph '%c' uses only '#' and '.'"), Char), Pixel == TEXT('#') || Pixel == TEXT('.'));
			}
		}
		// Capitals and digits use the full 7-row cap height (a short one would sit too high on the line).
		if (FChar::IsUpper(Char) || FChar::IsDigit(Char))
		{
			TestEqual(FString::Printf(TEXT("'%c' is 7 rows tall"), Char), Lines.Num(), UGBPixelFont::CapHeight);
		}
	}

	// Width = glyph widths + one spacing column between glyphs; unknown characters fall back to '?' (5 wide).
	UGBPixelFont* Font = NewObject<UGBPixelFont>(GetTransientPackage());
	Font->Init();
	TestEqual(TEXT("\"I\" is 3 wide"), Font->MeasureWidth(TEXT("I")), 3);
	TestEqual(TEXT("\"Il\" = 3 + 1 + 2"), Font->MeasureWidth(TEXT("Il")), 6);
	TestEqual(TEXT("\"10\" = 5 + 1 + 5"), Font->MeasureWidth(TEXT("10")), 11);
	TestEqual(TEXT("an unknown character measures like '?'"), Font->MeasureWidth(FString(1, TEXT("é"))), 5);
	TestEqual(TEXT("empty text is 0 wide"), Font->MeasureWidth(FString()), 0);
	return true;
}

#endif
