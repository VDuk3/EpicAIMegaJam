// Pure logic tests for the pixel-art conversion. Every test must be able to fail:
// each one checks behavior that a plausible wrong implementation would break.
#include "GBPixelVfxConvert.h"

#include "GBPixelVfxTypes.h"
#include "Misc/AutomationTest.h"

namespace GBPixelVfxTestLocal
{
	using namespace GBPixelVfxConvert;

	FSettings MakeRampSettings()
	{
		FSettings Settings;
		Settings.FrameWidth = 8;
		Settings.FrameHeight = 1;
		Settings.ColorMode = TEXT("Ramp");
		Settings.AlphaMode = TEXT("Opacity");
		Settings.AlphaThreshold = 0.3f;
		Settings.LevelsPercentile = 1.f;
		Settings.Gamma = 1.f;
		TArray<FColor> Ramp;
		Ramp.Add(FColor(0, 0, 0));
		Ramp.Add(FColor(85, 85, 85));
		Ramp.Add(FColor(170, 170, 170));
		Ramp.Add(FColor(255, 255, 255));
		Settings.Ramps.Add(Ramp);
		return Settings;
	}

	// One row of grey pixels with full opacity, linear value X across the row.
	FWorkFrame MakeGreyRow(int32 Width, float Step)
	{
		FWorkFrame Frame;
		Frame.Width = Width;
		Frame.Height = 1;
		Frame.Color.SetNum(Width);
		Frame.Opacity.SetNum(Width);
		for (int32 i = 0; i < Width; ++i)
		{
			const float X = FMath::Min(1.f, static_cast<float>(i) * Step);
			Frame.Color[i] = FLinearColor(X, X, X);
			Frame.Opacity[i] = 1.f;
		}
		return Frame;
	}

	int32 RampIndexOf(const FSettings& Settings, const FColor& Color)
	{
		for (int32 i = 0; i < Settings.Ramps[0].Num(); ++i)
		{
			if (Settings.Ramps[0][i] == Color)
			{
				return i;
			}
		}
		return -1;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBPixelVfxHardAlphaTest, "GBPixelVfx.Alpha.HardCutoff",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGBPixelVfxHardAlphaTest::RunTest(const FString& Parameters)
{
	using namespace GBPixelVfxConvert;
	using namespace GBPixelVfxTestLocal;
	FSettings Settings = MakeRampSettings();
	FWorkFrame Frame;
	Frame.Width = 4;
	Frame.Height = 1;
	Frame.Color.SetNum(4);
	Frame.Opacity.SetNum(4);
	// Bright white everywhere; only the opacity crosses the threshold between pixels 1 and 2.
	for (int32 i = 0; i < 4; ++i)
	{
		Frame.Color[i] = FLinearColor(4.f, 4.f, 4.f);
	}
	Frame.Opacity[0] = 0.f;
	Frame.Opacity[1] = 0.29f;
	Frame.Opacity[2] = 0.31f;
	Frame.Opacity[3] = 1.f;
	float Peak = 1.f;
	float Hot = 1.f;
	float Cold = 1.f;
	ComputePeaks({ Frame }, Settings, Peak, Hot, Cold);
	const FFrameMap Map = MapFrame(Frame, Settings, Peak, Hot, Cold);
	TArray<TArray<FColor>> Pixels;
	ResolveFrames({ Map }, Settings, Pixels);
	TestTrue(TEXT("pixel below the threshold is transparent"), Pixels[0][0].A == 0);
	TestTrue(TEXT("pixel just below the threshold is transparent"), Pixels[0][1].A == 0);
	TestTrue(TEXT("pixel just above the threshold is solid"), Pixels[0][2].A == 255);
	TestTrue(TEXT("pixel at full opacity is solid"), Pixels[0][3].A == 255);
	for (const TArray<FColor>& Row : Pixels)
	{
		for (const FColor& Pixel : Row)
		{
			TestTrue(TEXT("no partial alpha in the output"), Pixel.A == 0 || Pixel.A == 255);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBPixelVfxRampOrderTest, "GBPixelVfx.Mapping.RampGradient",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGBPixelVfxRampOrderTest::RunTest(const FString& Parameters)
{
	using namespace GBPixelVfxConvert;
	using namespace GBPixelVfxTestLocal;
	FSettings Settings = MakeRampSettings();
	const FWorkFrame Frame = MakeGreyRow(8, 0.25f);
	float Peak = 1.f;
	float Hot = 1.f;
	float Cold = 1.f;
	ComputePeaks({ Frame }, Settings, Peak, Hot, Cold);
	const FFrameMap Map = MapFrame(Frame, Settings, Peak, Hot, Cold);
	TArray<TArray<FColor>> Pixels;
	ResolveFrames({ Map }, Settings, Pixels);
	int32 PrevIndex = -1;
	bool bSawFirst = false;
	bool bSawLast = false;
	for (int32 i = 0; i < 8; ++i)
	{
		const int32 Index = RampIndexOf(Settings, Pixels[0][i]);
		TestTrue(TEXT("every output color is one of the ramp colors"), Index >= 0);
		if (Index >= 0)
		{
			TestTrue(TEXT("ramp indices never decrease along a rising gradient"), Index >= PrevIndex);
			PrevIndex = Index;
			bSawFirst = bSawFirst || Index == 0;
			bSawLast = bSawLast || Index == Settings.Ramps[0].Num() - 1;
		}
	}
	TestTrue(TEXT("a full range gradient uses the first ramp color"), bSawFirst);
	TestTrue(TEXT("a full range gradient uses the last ramp color"), bSawLast);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBPixelVfxSplitSortTest, "GBPixelVfx.Mapping.SplitRouting",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGBPixelVfxSplitSortTest::RunTest(const FString& Parameters)
{
	using namespace GBPixelVfxConvert;
	using namespace GBPixelVfxTestLocal;
	FSettings Settings = MakeRampSettings();
	Settings.ColorMode = TEXT("Split");
	Settings.AlphaMode = TEXT("Opacity");
	Settings.HotLuminance = 0.55f;
	Settings.HotSaturation = 0.9f; // only luminance decides in this test
	TArray<FColor> ColdRamp;
	ColdRamp.Add(FColor(16, 16, 16));
	ColdRamp.Add(FColor(64, 64, 64));
	Settings.Ramps.Add(ColdRamp);
	FWorkFrame Frame;
	Frame.Width = 2;
	Frame.Height = 1;
	Frame.Color.SetNum(2);
	Frame.Opacity.SetNum(2);
	// Hot: bright saturated orange. Cold: dim neutral grey.
	Frame.Color[0] = FLinearColor(3.f, 0.9f, 0.1f);
	Frame.Color[1] = FLinearColor(0.2f, 0.2f, 0.2f);
	Frame.Opacity[0] = 1.f;
	Frame.Opacity[1] = 1.f;
	float Peak = 1.f;
	float Hot = 1.f;
	float Cold = 1.f;
	ComputePeaks({ Frame }, Settings, Peak, Hot, Cold);
	const FFrameMap Map = MapFrame(Frame, Settings, Peak, Hot, Cold);
	TestTrue(TEXT("bright saturated pixel is hot"), Map.Pixels[0].bHot);
	TestTrue(TEXT("hot pixel uses ramp 0"), Map.Pixels[0].Ramp == 0);
	TestFalse(TEXT("dim grey pixel is not hot"), Map.Pixels[1].bHot);
	TestTrue(TEXT("cold pixel uses ramp 1"), Map.Pixels[1].Ramp == 1);
	TArray<TArray<FColor>> Pixels;
	ResolveFrames({ Map }, Settings, Pixels);
	TestEqual(TEXT("hot pixel resolves to a ramp 0 color"), Pixels[0][0], Settings.Ramps[0][Map.Pixels[0].Index]);
	TestEqual(TEXT("cold pixel resolves to a ramp 1 color"), Pixels[0][1], Settings.Ramps[1][Map.Pixels[1].Index]);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBPixelVfxHysteresisHoldTest, "GBPixelVfx.Temporal.HysteresisBand",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGBPixelVfxHysteresisHoldTest::RunTest(const FString& Parameters)
{
	using namespace GBPixelVfxConvert;
	using namespace GBPixelVfxTestLocal;
	FSettings Settings = MakeRampSettings();
	Settings.Hysteresis = 0.05f;
	Settings.LevelsPercentile = 1.f;
	// A value that sits in band 2 of 4 ([0.5, 0.75)) but wobbles across the lower edge by
	// less than the margin must keep one color; a second run wobbles far past it and must change.
	auto RunWobble = [&Settings](float Base, float Wobble) -> TArray<int32>
	{
		TArray<int32> Indices;
		FFrameMap Prev;
		bool bFirst = true;
		for (int32 f = 0; f < 6; ++f)
		{
			const float V = (f % 2 == 0) ? Base - Wobble : Base + Wobble;
			FWorkFrame Frame;
			Frame.Width = 1;
			Frame.Height = 1;
			Frame.Color.SetNum(1);
			Frame.Opacity.SetNum(1);
			Frame.Color[0] = FLinearColor(V, V, V);
			Frame.Opacity[0] = 1.f;
			// Fixed peak of 1 so V is the levels value directly.
			FFrameMap Map = MapFrame(Frame, Settings, 1.f, 1.f, 1.f);
			if (!bFirst)
			{
				ApplyHysteresis(Map, Prev, Settings);
			}
			bFirst = false;
			Prev = Map;
			Indices.Add(Map.Pixels[0].Index);
		}
		return Indices;
	};
	// Band 2 of 4 spans [0.5, 0.75); wobble 0.02 around 0.51 stays inside the widened band.
	const TArray<int32> Held = RunWobble(0.51f, 0.02f);
	bool bAllSame = true;
	for (int32 Index : Held)
	{
		bAllSame = bAllSame && Index == Held[0];
	}
	TestTrue(TEXT("a wobble inside the margin keeps one color over 6 frames"), bAllSame);
	// Wobble 0.3 around 0.6 crosses into far bands: the color must change at least once.
	const TArray<int32> Moved = RunWobble(0.6f, 0.3f);
	bool bChanged = false;
	for (int32 Index : Moved)
	{
		bChanged = bChanged || Index != Moved[0];
	}
	TestTrue(TEXT("a wobble far past the margin changes the color"), bChanged);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBPixelVfxCleanupIslandsTest, "GBPixelVfx.Cleanup.IslandsAndHoles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGBPixelVfxCleanupIslandsTest::RunTest(const FString& Parameters)
{
	using namespace GBPixelVfxConvert;
	// A 2-pixel island in a 6x6 frame.
	auto MakeIsland = []() -> FFrameMap
	{
		FFrameMap Map;
		Map.Width = 6;
		Map.Height = 6;
		Map.Pixels.SetNum(36);
		Map.Pixels[0].bSolid = true;
		Map.Pixels[1].bSolid = true;
		return Map;
	};
	FFrameMap Removed = MakeIsland();
	RemoveSmallIslands(Removed, 3);
	TestEqual(TEXT("a 2-pixel island is removed when the minimum is 3"), Removed.SolidCount(), 0);
	FFrameMap Kept = MakeIsland();
	RemoveSmallIslands(Kept, 2);
	TestEqual(TEXT("a 2-pixel island is kept when the minimum is 2"), Kept.SolidCount(), 2);

	// A single transparent hole fully surrounded by solid pixels is filled.
	FFrameMap Hole;
	Hole.Width = 3;
	Hole.Height = 3;
	Hole.Pixels.SetNum(9);
	for (int32 i = 0; i < 9; ++i)
	{
		Hole.Pixels[i].bSolid = true;
		Hole.Pixels[i].Ramp = 0;
		Hole.Pixels[i].Index = 2;
	}
	Hole.Pixels[4].bSolid = false;
	FillOnePixelHoles(Hole);
	TestTrue(TEXT("a one-pixel hole is filled"), Hole.Pixels[4].bSolid);
	TestEqual(TEXT("the filled hole takes the neighbor color"), Hole.Pixels[4].Index, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBPixelVfxOutlineShapeTest, "GBPixelVfx.Cleanup.OutlineRing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGBPixelVfxOutlineShapeTest::RunTest(const FString& Parameters)
{
	using namespace GBPixelVfxConvert;
	using namespace GBPixelVfxTestLocal;
	FSettings Settings = MakeRampSettings();
	Settings.Outline = TEXT("Black");
	FFrameMap Map;
	Map.Width = 6;
	Map.Height = 6;
	Map.Pixels.SetNum(36);
	for (int32 y = 2; y < 4; ++y)
	{
		for (int32 x = 2; x < 4; ++x)
		{
			FMappedPixel& Pixel = Map.Pixels[y * 6 + x];
			Pixel.bSolid = true;
			Pixel.Ramp = 0;
			Pixel.Index = 1;
		}
	}
	TArray<TArray<FColor>> Pixels;
	ResolveFrames({ Map }, Settings, Pixels);
	const FColor Outline(0, 0, 0, 255);
	// The 8 4-adjacent pixels around the 2x2 block.
	const int32 Expected[8][2] = { {2,1},{3,1},{1,2},{4,2},{1,3},{4,3},{2,4},{3,4} };
	for (const auto& Cell : Expected)
	{
		TestEqual(TEXT("4-adjacent pixel is outlined"), Pixels[0][Cell[1] * 6 + Cell[0]], Outline);
	}
	TestEqual(TEXT("distant pixel stays transparent"), Pixels[0][0], FColor(0, 0, 0, 0));
	TestEqual(TEXT("solid pixel keeps its color"), Pixels[0][2 * 6 + 2], Settings.Ramps[0][1]);

	// A block in the corner must not draw outside the frame.
	FFrameMap Corner;
	Corner.Width = 6;
	Corner.Height = 6;
	Corner.Pixels.SetNum(36);
	Corner.Pixels[0].bSolid = true;
	Corner.Pixels[0].Ramp = 0;
	Corner.Pixels[0].Index = 1;
	Corner.Pixels[1].bSolid = true;
	Corner.Pixels[1].Ramp = 0;
	Corner.Pixels[1].Index = 1;
	Corner.Pixels[6].bSolid = true;
	Corner.Pixels[6].Ramp = 0;
	Corner.Pixels[6].Index = 1;
	Corner.Pixels[7].bSolid = true;
	Corner.Pixels[7].Ramp = 0;
	Corner.Pixels[7].Index = 1;
	TArray<TArray<FColor>> CornerPixels;
	ResolveFrames({ Corner }, Settings, CornerPixels);
	TestTrue(TEXT("corner block resolves without crashing"), CornerPixels[0].Num() == 36);
	int32 Outlined = 0;
	for (const FColor& Pixel : CornerPixels[0])
	{
		if (Pixel == Outline)
		{
			++Outlined;
		}
	}
	TestEqual(TEXT("a 2x2 block in the corner gets 4 in-frame outline pixels"), Outlined, 4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBPixelVfxLoopBlendFlowTest, "GBPixelVfx.Temporal.LoopBlend",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGBPixelVfxLoopBlendFlowTest::RunTest(const FString& Parameters)
{
	using namespace GBPixelVfxConvert;
	auto SolidFrame = [](float Bright) -> FWorkFrame
	{
		FWorkFrame Frame;
		Frame.Width = 2;
		Frame.Height = 2;
		Frame.Color.SetNum(4);
		Frame.Opacity.SetNum(4);
		for (int32 i = 0; i < 4; ++i)
		{
			Frame.Color[i] = FLinearColor(Bright, Bright, Bright);
			Frame.Opacity[i] = 1.f;
		}
		return Frame;
	};
	TArray<FWorkFrame> Frames;
	Frames.Add(SolidFrame(1.f)); // frame 0: dim
	Frames.Add(SolidFrame(1.f)); // frame 1: dim
	Frames.Add(SolidFrame(1.f)); // frame 2: dim
	Frames.Add(SolidFrame(1.f)); // frame 3: dim, outside the blend window
	Frames.Add(SolidFrame(9.f)); // extra 0: bright
	Frames.Add(SolidFrame(9.f)); // extra 1: bright
	ApplyLoopBlend(Frames, 4, 2, true);
	TestEqual(TEXT("the extra frames are dropped"), Frames.Num(), 4);
	// Frame 0 blends 75% extra / 25% self: it must be much closer to the extra frame than to itself.
	const float Frame0 = Frames[0].Color[0].R;
	TestTrue(TEXT("frame 0 mostly equals the extra frame"), FMath::Abs(Frame0 - 7.f) < 0.01f);
	// Frame 3 is past the blend window: the last frame of the clip is unchanged.
	const float Frame3 = Frames[3].Color[0].R;
	TestTrue(TEXT("the last frame is unchanged"), FMath::Abs(Frame3 - 1.f) < 0.01f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBPixelVfxClipPlaybackTest, "GBPixelVfx.Clip.PlaybackHelpers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGBPixelVfxClipPlaybackTest::RunTest(const FString& Parameters)
{
	FGBPixelVfxClip Clip;
	Clip.Columns = 8;
	Clip.FrameWidth = 48;
	Clip.FrameHeight = 64;
	Clip.FrameCount = 24;
	Clip.Fps = 12;
	Clip.bLoop = true;
	// Frame 9 of an 8-column atlas: column 1, row 1 of a 384x192 atlas.
	const FVector4f Rect = FGBPixelVfxClip::FrameRect(Clip, 9);
	TestTrue(TEXT("frame 9 offset U is 1/8"), FMath::IsNearlyEqual(Rect.X, 0.125f));
	TestTrue(TEXT("frame 9 offset V is 1/3"), FMath::IsNearlyEqual(Rect.Y, 1.f / 3.f, 0.001f));
	TestTrue(TEXT("frame 9 scale U is 1/8"), FMath::IsNearlyEqual(Rect.Z, 0.125f));
	TestTrue(TEXT("frame 9 scale V is 1/3"), FMath::IsNearlyEqual(Rect.W, 1.f / 3.f, 0.001f));

	bool bFinished = false;
	TestEqual(TEXT("a looping clip wraps"), FGBPixelVfxClip::FrameAtTime(Clip, 2.5f, bFinished), 6);
	TestFalse(TEXT("a looping clip never finishes"), bFinished);
	Clip.bLoop = false;
	TestEqual(TEXT("a one-shot clamps to the last frame"), FGBPixelVfxClip::FrameAtTime(Clip, 99.f, bFinished), 23);
	TestTrue(TEXT("a one-shot finishes past its end"), bFinished);
	bFinished = true;
	TestEqual(TEXT("a one-shot shows frame 0 at time 0"), FGBPixelVfxClip::FrameAtTime(Clip, 0.f, bFinished), 0);
	TestFalse(TEXT("a one-shot is not finished at time 0"), bFinished);

	// JSON round trip keeps the light track.
	Clip.Id = FName(TEXT("RoundTrip"));
	Clip.AtlasFile = TEXT("RoundTrip.png");
	Clip.Light.bEnabled = true;
	Clip.Light.Track = { 0.2f, 0.9f, 1.f };
	FString Json;
	TestTrue(TEXT("clip serializes to JSON"), Clip.SaveToJsonString(Json));
	FGBPixelVfxClip Back;
	TestTrue(TEXT("clip parses back from JSON"), Back.LoadFromJsonString(Json));
	TestEqual(TEXT("the light track survives the round trip"), Back.Light.Track.Num(), 3);
	if (Back.Light.Track.Num() == 3)
	{
		TestTrue(TEXT("the brightest track value survives"), FMath::IsNearlyEqual(Back.Light.Track[2], 1.f));
	}
	TestTrue(TEXT("the id survives the round trip"), Back.Id == FName(TEXT("RoundTrip")));
	return true;
}
