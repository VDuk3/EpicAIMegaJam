#include "UI/GBPixelSpriteLibrary.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBSpritePixelsTest, "Glyphbound.UI.SpritePixels",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGBSpritePixelsTest::RunTest(const FString& Parameters)
{
	// 2x2, row-major from the top: red, transparent / half-alpha green, unknown key (magenta).
	FGBSpriteDefinition Def;
	Def.Id = TEXT("test");
	Def.Palette.Add(TEXT("r"), TEXT("#FF0000"));
	Def.Palette.Add(TEXT("g"), TEXT("#00FF0080"));
	Def.Rows = { FString(TEXT("r.")), FString(TEXT("gz")) };

	TArray<FColor> Pixels;
	int32 Width = 0;
	int32 Height = 0;
	TestTrue(TEXT("2x2 builds"), UGBPixelSpriteLibrary::BuildPixels(Def, Pixels, Width, Height));
	TestEqual(TEXT("Width"), Width, 2);
	TestEqual(TEXT("Height"), Height, 2);
	if (Pixels.Num() != 4)
	{
		AddError(FString::Printf(TEXT("Expected 4 pixels, got %d"), Pixels.Num()));
		return false;
	}
	TestEqual(TEXT("Top-left is opaque red"), Pixels[0], FColor(255, 0, 0, 255));
	TestEqual(TEXT("'.' is transparent"), (int32)Pixels[1].A, 0);
	TestEqual(TEXT("Bottom-left is green with the given alpha"), Pixels[2], FColor(0, 255, 0, 128));
	TestEqual(TEXT("Unknown key is magenta"), Pixels[3], FColor(255, 0, 255, 255));

	// A non-square sprite: width from the row length, height from the row count.
	FGBSpriteDefinition Wide = Def;
	Wide.Rows = { FString(TEXT("r.r")) };
	TestTrue(TEXT("3x1 builds"), UGBPixelSpriteLibrary::BuildPixels(Wide, Pixels, Width, Height));
	TestEqual(TEXT("3x1 width"), Width, 3);
	TestEqual(TEXT("3x1 height"), Height, 1);

	// Ragged rows and empty sprites are rejected.
	FGBSpriteDefinition Ragged = Def;
	Ragged.Rows = { FString(TEXT("rr")), FString(TEXT("r")) };
	TestFalse(TEXT("Ragged rows fail"), UGBPixelSpriteLibrary::BuildPixels(Ragged, Pixels, Width, Height));
	FGBSpriteDefinition Empty = Def;
	Empty.Rows.Reset();
	TestFalse(TEXT("No rows fails"), UGBPixelSpriteLibrary::BuildPixels(Empty, Pixels, Width, Height));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
