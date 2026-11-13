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
	// Exact band boundaries for N = 4: k/N edges must map to the upper band, values
	// just below to the lower band (a floor(v*(N-1)) implementation would differ).
	auto IndexAtValue = [&Settings](float V) -> int32
	{
		FWorkFrame Frame;
		Frame.Width = 1;
		Frame.Height = 1;
		Frame.Color.SetNum(1);
		Frame.Opacity.SetNum(1);
		Frame.Color[0] = FLinearColor(V, V, V);
		Frame.Opacity[0] = 1.f;
		const FFrameMap Map = MapFrame(Frame, Settings, 1.f, 1.f, 1.f);
		FColor Resolved(0, 0, 0, 0);
		TArray<TArray<FColor>> Pixels;
		ResolveFrames({ Map }, Settings, Pixels);
		return RampIndexOf(Settings, Pixels[0][0]);
	};
	TestEqual(TEXT("just below 0.25 maps to band 0"), IndexAtValue(0.24f), 0);
	TestEqual(TEXT("exactly 0.25 maps to band 1"), IndexAtValue(0.25f), 1);
	TestEqual(TEXT("just below 0.5 maps to band 1"), IndexAtValue(0.49f), 1);
	TestEqual(TEXT("exactly 0.5 maps to band 2"), IndexAtValue(0.5f), 2);
	TestEqual(TEXT("just below 0.75 maps to band 2"), IndexAtValue(0.74f), 2);
	TestEqual(TEXT("exactly 0.75 maps to band 3"), IndexAtValue(0.75f), 3);
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
	Frame.Width = 4;
	Frame.Height = 1;
	Frame.Color.SetNum(4);
	Frame.Opacity.SetNum(4);
	// Luminance-only hot: bright neutral grey (high luminance, zero saturation).
	Frame.Color[0] = FLinearColor(0.8f, 0.8f, 0.8f);
	// Saturation-only hot: dim but vivid (low luminance, high saturation).
	Frame.Color[1] = FLinearColor(0.4f, 0.02f, 0.02f);
	// Hot by both (bright saturated orange). Cold: dim neutral grey.
	Frame.Color[2] = FLinearColor(3.f, 0.9f, 0.1f);
	Frame.Color[3] = FLinearColor(0.2f, 0.2f, 0.2f);
	for (int32 i = 0; i < 4; ++i)
	{
		Frame.Opacity[i] = 1.f;
	}
	float Peak = 1.f;
	float Hot = 1.f;
	float Cold = 1.f;
	ComputePeaks({ Frame }, Settings, Peak, Hot, Cold);
	const FFrameMap Map = MapFrame(Frame, Settings, Peak, Hot, Cold);
	TestTrue(TEXT("luminance-only pixel is hot"), Map.Pixels[0].bHot);
	TestTrue(TEXT("luminance-only hot pixel uses ramp 0"), Map.Pixels[0].Ramp == 0);
	TestTrue(TEXT("saturation-only pixel is hot"), Map.Pixels[1].bHot);
	TestTrue(TEXT("saturation-only hot pixel uses ramp 0"), Map.Pixels[1].Ramp == 0);
	TestTrue(TEXT("bright saturated pixel is hot"), Map.Pixels[2].bHot);
	TestTrue(TEXT("hot pixel uses ramp 0"), Map.Pixels[2].Ramp == 0);
	TestFalse(TEXT("dim grey pixel is not hot"), Map.Pixels[3].bHot);
	TestTrue(TEXT("cold pixel uses ramp 1"), Map.Pixels[3].Ramp == 1);
	// Peaks calculated by hand (percentile 1 = the maximum of each group). Hot group: luminances
	// 0.8, 0.100788 (0.4*0.2126 + 0.02*0.7152 + 0.02*0.0722) and 1.2887 (3*0.2126 + 0.9*0.7152 + 0.1*0.0722).
	// Cold group: 0.2. A peak routine that returned any other pair would be caught here.
	TestTrue(TEXT("the hot peak is the brightest hot luminance (1.2887)"), FMath::IsNearlyEqual(Hot, 1.2887f, 0.001f));
	TestTrue(TEXT("the cold peak is the brightest cold luminance (0.2)"), FMath::IsNearlyEqual(Cold, 0.2f, 0.001f));
	TestTrue(TEXT("the overall peak is the brightest luminance"), FMath::IsNearlyEqual(Peak, 1.2887f, 0.001f));
	// Levels: each pixel is normalized by its own group's peak.
	TestTrue(TEXT("luminance-only hot value is 0.8 / 1.2887"), FMath::IsNearlyEqual(Map.Pixels[0].Value, 0.6208f, 0.002f));
	TestTrue(TEXT("saturation-only hot value is 0.100788 / 1.2887"), FMath::IsNearlyEqual(Map.Pixels[1].Value, 0.0782f, 0.002f));
	TestTrue(TEXT("the brightest hot pixel is at full value"), FMath::IsNearlyEqual(Map.Pixels[2].Value, 1.f, 0.002f));
	TestTrue(TEXT("the cold pixel is at full value of the cold peak"), FMath::IsNearlyEqual(Map.Pixels[3].Value, 1.f, 0.002f));
	// Resulting ramp indices (4 hot bands, 2 cold bands): 0.62 -> 2, 0.078 -> 0, 1 -> 3, cold 1 -> 1.
	// A cold pixel normalized by the hot peak (0.155) would land in cold band 0.
	TestEqual(TEXT("hot pixel 0 lands in hot band 2"), static_cast<int32>(Map.Pixels[0].Index), 2);
	TestEqual(TEXT("hot pixel 1 lands in hot band 0"), static_cast<int32>(Map.Pixels[1].Index), 0);
	TestEqual(TEXT("hot pixel 2 lands in hot band 3"), static_cast<int32>(Map.Pixels[2].Index), 3);
	TestEqual(TEXT("cold pixel lands in cold band 1"), static_cast<int32>(Map.Pixels[3].Index), 1);
	TArray<TArray<FColor>> Pixels;
	ResolveFrames({ Map }, Settings, Pixels);
	TestEqual(TEXT("hot pixel resolves to its ramp 0 color"), Pixels[0][2], Settings.Ramps[0][3]);
	TestEqual(TEXT("cold pixel resolves to its ramp 1 color"), Pixels[0][3], Settings.Ramps[1][1]);
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
	// Pixels that disappear stay gone, and pixels that stay keep the held color only inside the
	// widened band. Previous frame: 10 solid pixels holding band 2 (band [0.5, 0.75) +/- 0.05).
	auto MakeStrip = [](int32 SolidCount, int32 Index, float Value) -> FFrameMap
	{
		FFrameMap Map;
		Map.Width = 10;
		Map.Height = 1;
		Map.Pixels.SetNum(10);
		for (int32 i = 0; i < SolidCount; ++i)
		{
			Map.Pixels[i].bSolid = true;
			Map.Pixels[i].Ramp = 0;
			Map.Pixels[i].Index = static_cast<uint8>(Index);
			Map.Pixels[i].Value = Value;
		}
		return Map;
	};
	const FFrameMap PrevStrip = MakeStrip(10, 2, 0.6f);
	// 8 of 10 remain (a 20% change: damping applies). Pixel 7 sits far below the band.
	FFrameMap Shrunk = MakeStrip(8, 1, 0.49f);
	Shrunk.Pixels[7].Value = 0.3f;
	ApplyHysteresis(Shrunk, PrevStrip, Settings);
	TestEqual(TEXT("pixels that disappeared stay transparent"), Shrunk.SolidCount(), 8);
	TestFalse(TEXT("a vanished pixel is not restored (8)"), Shrunk.Pixels[8].bSolid);
	TestFalse(TEXT("a vanished pixel is not restored (9)"), Shrunk.Pixels[9].bSolid);
	TestEqual(TEXT("a value just under the lower edge is held inside the margin"), static_cast<int32>(Shrunk.Pixels[0].Index), 2);
	TestEqual(TEXT("a value far under the band changes color"), static_cast<int32>(Shrunk.Pixels[7].Index), 1);
	// 5 of 10 remain (a 50% change): damping is skipped, so the near-boundary color changes.
	FFrameMap Burst = MakeStrip(5, 1, 0.49f);
	ApplyHysteresis(Burst, PrevStrip, Settings);
	TestEqual(TEXT("a rapid coverage change bypasses the hold"), static_cast<int32>(Burst.Pixels[0].Index), 1);
	TestEqual(TEXT("a rapid coverage change leaves the solid count alone"), Burst.SolidCount(), 5);
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

	// Majority beats first neighbor: right neighbor (read first) is color 0, the other
	// three are color 5, so the hole must take color 5, not the first neighbor's color.
	FFrameMap Majority;
	Majority.Width = 3;
	Majority.Height = 3;
	Majority.Pixels.SetNum(9);
	for (int32 i = 0; i < 9; ++i)
	{
		Majority.Pixels[i].bSolid = true;
		Majority.Pixels[i].Ramp = 0;
		Majority.Pixels[i].Index = 5;
	}
	Majority.Pixels[5].Ramp = 0; // right of center (x+1): read first
	Majority.Pixels[5].Index = 0;
	Majority.Pixels[4].bSolid = false;
	FillOnePixelHoles(Majority);
	TestTrue(TEXT("a majority hole is filled"), Majority.Pixels[4].bSolid);
	TestEqual(TEXT("the filled hole takes the majority color, not the first neighbor"), Majority.Pixels[4].Index, 5);

	// An unfillable gap (a transparent pixel with a transparent neighbor) stays transparent,
	// so a filler that fills every transparent pixel fails here.
	FFrameMap Gap;
	Gap.Width = 3;
	Gap.Height = 3;
	Gap.Pixels.SetNum(9);
	for (int32 i = 0; i < 9; ++i)
	{
		Gap.Pixels[i].bSolid = true;
		Gap.Pixels[i].Ramp = 0;
		Gap.Pixels[i].Index = 2;
	}
	Gap.Pixels[4].bSolid = false;
	Gap.Pixels[7].bSolid = false; // below the hole: the hole has a transparent neighbor
	FillOnePixelHoles(Gap);
	TestFalse(TEXT("a gap with a transparent neighbor is not filled"), Gap.Pixels[4].bSolid);
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
	auto SolidFrame = [](float Bright, float Opacity) -> FWorkFrame
	{
		FWorkFrame Frame;
		Frame.Width = 2;
		Frame.Height = 2;
		Frame.Color.SetNum(4);
		Frame.Opacity.SetNum(4);
		for (int32 i = 0; i < 4; ++i)
		{
			Frame.Color[i] = FLinearColor(Bright, Bright, Bright);
			Frame.Opacity[i] = Opacity;
		}
		return Frame;
	};
	TArray<FWorkFrame> Frames;
	Frames.Add(SolidFrame(1.f, 1.f)); // frame 0: dim, opaque
	Frames.Add(SolidFrame(2.f, 0.5f)); // frame 1: mid, half coverage
	Frames.Add(SolidFrame(1.f, 1.f)); // frame 2: dim
	Frames.Add(SolidFrame(1.f, 1.f)); // frame 3: dim, outside the blend window
	Frames.Add(SolidFrame(9.f, 0.2f)); // extra 0: bright, thin
	Frames.Add(SolidFrame(5.f, 0.8f)); // extra 1: distinct from extra 0
	ApplyLoopBlend(Frames, 4, 2, true);
	TestEqual(TEXT("the extra frames are dropped"), Frames.Num(), 4);
	// Frame 0 blends 75% extra0 / 25% self on RGB and opacity.
	const float Frame0 = Frames[0].Color[0].R;
	TestTrue(TEXT("frame 0 mostly equals extra frame 0"), FMath::Abs(Frame0 - 7.f) < 0.01f);
	TestTrue(TEXT("frame 0 opacity blends too"), FMath::Abs(Frames[0].Opacity[0] - 0.4f) < 0.01f);
	// Frame 1 blends 25% extra1 / 75% self: an implementation that only blends frame 0 fails here.
	const float Frame1 = Frames[1].Color[0].R;
	TestTrue(TEXT("frame 1 blends extra frame 1"), FMath::Abs(Frame1 - 2.75f) < 0.01f);
	TestTrue(TEXT("frame 1 opacity blends too"), FMath::Abs(Frames[1].Opacity[0] - 0.575f) < 0.01f);
	// Frame 3 is past the blend window: the last frame of the clip is unchanged.
	const float Frame3 = Frames[3].Color[0].R;
	TestTrue(TEXT("the last frame is unchanged"), FMath::Abs(Frame3 - 1.f) < 0.01f);
	// Non-loop clips never blend: with enough frames for a blend (KeepCount + BlendFrames = 4)
	// the extras are dropped and every kept RGB and opacity value is untouched.
	TArray<FWorkFrame> Flat;
	Flat.Add(SolidFrame(1.f, 1.f));
	Flat.Add(SolidFrame(2.f, 0.5f));
	Flat.Add(SolidFrame(9.f, 0.2f));
	Flat.Add(SolidFrame(5.f, 0.8f));
	ApplyLoopBlend(Flat, 2, 2, false);
	TestEqual(TEXT("non-loop drops the extras"), Flat.Num(), 2);
	if (Flat.Num() == 2)
	{
		const float Bright[2] = { 1.f, 2.f };
		const float Opacity[2] = { 1.f, 0.5f };
		for (int32 f = 0; f < 2; ++f)
		{
			for (int32 p = 0; p < 4; ++p)
			{
				TestTrue(TEXT("non-loop keeps every RGB channel"),
					Flat[f].Color[p].Equals(FLinearColor(Bright[f], Bright[f], Bright[f], 1.f), 0.001f));
				TestTrue(TEXT("non-loop keeps every opacity"), FMath::IsNearlyEqual(Flat[f].Opacity[p], Opacity[f], 0.001f));
			}
		}
	}
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
	// The last frame is shown for a full frame duration: mid-last-frame is not finished.
	const float LastStart = (Clip.FrameCount - 1) / static_cast<float>(Clip.Fps);
	const float ClipEnd = Clip.FrameCount / static_cast<float>(Clip.Fps);
	bFinished = true;
	TestEqual(TEXT("a one-shot shows the last frame mid-frame"), FGBPixelVfxClip::FrameAtTime(Clip, LastStart + 0.5f / Clip.Fps, bFinished), 23);
	TestFalse(TEXT("a one-shot is not finished mid-last-frame"), bFinished);
	bFinished = false;
	TestEqual(TEXT("a one-shot clamps at the exact end"), FGBPixelVfxClip::FrameAtTime(Clip, ClipEnd, bFinished), 23);
	TestTrue(TEXT("a one-shot finishes at the exact end"), bFinished);

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
		// Every entry must survive, not just the count and the peak.
		const float Expected[3] = { 0.2f, 0.9f, 1.f };
		for (int32 i = 0; i < 3; ++i)
		{
			TestTrue(TEXT("every track entry survives the round trip"),
				FMath::IsNearlyEqual(Back.Light.Track[i], Expected[i], 0.001f));
		}
	}
	TestTrue(TEXT("the id survives the round trip"), Back.Id == FName(TEXT("RoundTrip")));
	return true;
}

namespace GBPixelVfxTestLocal
{
	// Upright quad: local X = world -Y, local Y = world +Z (up), normal (local Z) = world -X.
	FQuat UprightRotation()
	{
		return FQuat(FMatrix(FVector(0.f, -1.f, 0.f), FVector(0.f, 0.f, 1.f), FVector(-1.f, 0.f, 0.f),
			FVector::ZeroVector).Rotator());
	}

	bool NearlyEqualVector(const FVector& A, const FVector& B)
	{
		return A.Equals(B, 0.01);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBPixelVfxQuadPivotTest, "GBPixelVfx.Clip.QuadPivotPlacement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGBPixelVfxQuadPivotTest::RunTest(const FString& Parameters)
{
	FGBPixelVfxClip Clip;
	Clip.WorldWidthCm = 200.f;
	Clip.WorldHeightCm = 400.f;
	Clip.PivotU = 0.25f; // asymmetric: the pivot is a quarter of the width from the left edge
	Clip.PivotV = 1.f;
	const FQuat R = GBPixelVfxTestLocal::UprightRotation();
	TestTrue(TEXT("the test rotation maps local X to world -Y"),
		GBPixelVfxTestLocal::NearlyEqualVector(R.RotateVector(FVector(1.f, 0.f, 0.f)), FVector(0.f, -1.f, 0.f)));
	const FVector Comp(100.f, 200.f, 0.f);
	FVector QuadLoc;
	FVector LightLoc;
	// SizeScale 1.5 makes the sprite 300 x 600 cm. The quad center is (0.5 - 0.25) * 300 = 75 cm to the
	// right of the pivot in quad space (world -Y) and (1 - 0.5) * 600 = 300 cm above it.
	FGBPixelVfxClip::ComputeQuadAndLightPlacement(Clip, 1.5f, Comp, R, QuadLoc, LightLoc);
	TestTrue(TEXT("the quad center follows pivot U/V and SizeScale"),
		GBPixelVfxTestLocal::NearlyEqualVector(QuadLoc, FVector(100.f, 125.f, 300.f)));
	// At scale 1 the same pivot gives 50 cm and 200 cm: the offsets scale with SizeScale.
	FGBPixelVfxClip::ComputeQuadAndLightPlacement(Clip, 1.f, Comp, R, QuadLoc, LightLoc);
	TestTrue(TEXT("at scale 1 the quad center is (100, 150, 200)"),
		GBPixelVfxTestLocal::NearlyEqualVector(QuadLoc, FVector(100.f, 150.f, 200.f)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBPixelVfxLightAnchorTest, "GBPixelVfx.Clip.LightAnchorPlacement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGBPixelVfxLightAnchorTest::RunTest(const FString& Parameters)
{
	FGBPixelVfxClip Clip;
	Clip.WorldWidthCm = 200.f;
	Clip.WorldHeightCm = 400.f;
	Clip.PivotU = 0.25f;
	Clip.PivotV = 1.f;
	Clip.Light.AnchorU = 0.75f;
	Clip.Light.AnchorV = 0.25f;
	const FQuat R = GBPixelVfxTestLocal::UprightRotation();
	const FVector Comp(100.f, 200.f, 0.f);
	FVector QuadLoc;
	FVector LightLoc;
	// SizeScale 1.5: sprite 300 x 600 cm. The anchor is (0.75 - 0.25) * 300 = 150 cm right of the pivot
	// (world -Y) and (1 - 0.25) * 600 = 450 cm above it, then 10 cm toward the camera (world -X).
	FGBPixelVfxClip::ComputeQuadAndLightPlacement(Clip, 1.5f, Comp, R, QuadLoc, LightLoc);
	TestTrue(TEXT("the light follows anchor U/V relative to the pivot, SizeScale and the 10 cm lift"),
		GBPixelVfxTestLocal::NearlyEqualVector(LightLoc, FVector(90.f, 50.f, 450.f)));
	FGBPixelVfxClip::ComputeQuadAndLightPlacement(Clip, 1.f, Comp, R, QuadLoc, LightLoc);
	TestTrue(TEXT("at scale 1 the light is at (90, 100, 300)"),
		GBPixelVfxTestLocal::NearlyEqualVector(LightLoc, FVector(90.f, 100.f, 300.f)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBPixelVfxColorClusterTest, "GBPixelVfx.Cleanup.ColorClusters",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGBPixelVfxColorClusterTest::RunTest(const FString& Parameters)
{
	using namespace GBPixelVfxConvert;
	FSettings Settings = GBPixelVfxTestLocal::MakeRampSettings();
	// Ramp 0 has four colors, index 0 the darkest and index 3 the brightest.
	auto MakeField = [](int32 W, int32 H, int32 Index) -> FFrameMap
	{
		FFrameMap Map;
		Map.Width = W;
		Map.Height = H;
		Map.Pixels.SetNum(W * H);
		for (FMappedPixel& Pixel : Map.Pixels)
		{
			Pixel.bSolid = true;
			Pixel.Ramp = 0;
			Pixel.Index = static_cast<uint8>(Index);
			Pixel.Value = 0.5f;
		}
		return Map;
	};
	auto SetIndex = [](FFrameMap& Map, int32 X, int32 Y, int32 Index)
	{
		Map.Pixels[Y * Map.Width + X].Index = static_cast<uint8>(Index);
	};
	auto IndexAt = [](const FFrameMap& Map, int32 X, int32 Y) -> int32
	{
		return Map.Pixels[Y * Map.Width + X].Index;
	};

	// A 2-pixel color-2 speck in a color-1 field: both members take the surrounding color.
	FFrameMap Map = MakeField(6, 4, 1);
	SetIndex(Map, 1, 1, 2);
	SetIndex(Map, 2, 1, 2);
	FFrameMap Cleaned = Map;
	RecolorSmallColorClusters(Cleaned, Settings, 3);
	TestEqual(TEXT("a 2-pixel speck is recolored when the minimum is 3"), Cleaned.SolidCount(), 24);
	TestEqual(TEXT("the first speck pixel takes the surrounding color"), IndexAt(Cleaned, 1, 1), 1);
	TestEqual(TEXT("the second speck pixel takes the surrounding color"), IndexAt(Cleaned, 2, 1), 1);
	TestEqual(TEXT("the speck keeps its ramp"), static_cast<int32>(Cleaned.Pixels[7].Ramp), 0);
	FFrameMap Kept = Map;
	RecolorSmallColorClusters(Kept, Settings, 2);
	TestEqual(TEXT("a 2-pixel region is kept when the minimum is 2"), IndexAt(Kept, 1, 1), 2);
	FFrameMap Off = Map;
	RecolorSmallColorClusters(Off, Settings, 0);
	TestEqual(TEXT("clusters off leaves the speck alone"), IndexAt(Off, 1, 1), 2);

	// Competing neighbors: a 3-pixel color-2 bar in a color-3 field. The start pixel's neighbors on
	// the left, above and below are color 0 (3 votes); the other five neighbors are color 3 (5 votes).
	// The majority (3) wins; "first neighbor" and "darkest neighbor" would pick 0.
	FFrameMap Bar = MakeField(9, 5, 3);
	for (int32 X = 3; X <= 5; ++X)
	{
		SetIndex(Bar, X, 2, 2);
	}
	SetIndex(Bar, 2, 2, 0);
	SetIndex(Bar, 3, 1, 0);
	SetIndex(Bar, 3, 3, 0);
	RecolorSmallColorClusters(Bar, Settings, 4);
	for (int32 X = 3; X <= 5; ++X)
	{
		TestEqual(TEXT("every bar pixel takes the majority neighbor color"), IndexAt(Bar, X, 2), 3);
	}

	// Vote tie: the start pixel's neighbors are bright (3), the other member's neighbors are dark (1),
	// three votes each. The darker color wins; "first neighbor" or "brighter" would pick 3.
	FFrameMap Tie = MakeField(9, 5, 3);
	SetIndex(Tie, 3, 2, 2);
	SetIndex(Tie, 4, 2, 2);
	SetIndex(Tie, 5, 2, 1);
	SetIndex(Tie, 4, 1, 1);
	SetIndex(Tie, 4, 3, 1);
	RecolorSmallColorClusters(Tie, Settings, 3);
	TestEqual(TEXT("a tie goes to the darker color (first member)"), IndexAt(Tie, 3, 2), 1);
	TestEqual(TEXT("a tie goes to the darker color (second member)"), IndexAt(Tie, 4, 2), 1);

	// Adjacent small regions are decided on the state before the pass: a 2x2 frame of two 2-pixel
	// rows swaps their colors. A pass that reads already-recolored pixels would leave the second row alone.
	FFrameMap Pair = MakeField(2, 2, 1);
	SetIndex(Pair, 0, 1, 2);
	SetIndex(Pair, 1, 1, 2);
	RecolorSmallColorClusters(Pair, Settings, 3);
	TestEqual(TEXT("the top row takes the bottom row's original color"), IndexAt(Pair, 0, 0), 2);
	TestEqual(TEXT("the top row's second pixel too"), IndexAt(Pair, 1, 0), 2);
	TestEqual(TEXT("the bottom row takes the top row's original color"), IndexAt(Pair, 0, 1), 1);
	TestEqual(TEXT("the bottom row's second pixel too"), IndexAt(Pair, 1, 1), 1);

	// L-shaped region: a boundary pixel touching two members is one voter, not two. The corner pixel
	// (1,0) is dark (0) and touches (0,0) and (1,1); (0,2) and (2,1) are two distinct bright (3)
	// pixels; (1,2) is a third color (1). Counting the corner twice ties 0 against 3 and the darker 0
	// wins; counting each pixel once gives 3 two votes against 1 and the bright color wins.
	FFrameMap Ell = MakeField(3, 3, 3);
	SetIndex(Ell, 0, 0, 2);
	SetIndex(Ell, 0, 1, 2);
	SetIndex(Ell, 1, 1, 2);
	SetIndex(Ell, 1, 0, 0);
	SetIndex(Ell, 1, 2, 1);
	RecolorSmallColorClusters(Ell, Settings, 4);
	TestEqual(TEXT("an L region counts a shared boundary pixel once (corner member)"), IndexAt(Ell, 0, 0), 3);
	TestEqual(TEXT("an L region counts a shared boundary pixel once (middle member)"), IndexAt(Ell, 0, 1), 3);
	TestEqual(TEXT("an L region counts a shared boundary pixel once (last member)"), IndexAt(Ell, 1, 1), 3);
	return true;
}

namespace GBPixelVfxTestLocal
{
	FFrameMap SolidMap(int32 Size)
	{
		FFrameMap Map;
		Map.Width = Size;
		Map.Height = Size;
		Map.Pixels.SetNum(Size * Size);
		for (FMappedPixel& Pixel : Map.Pixels)
		{
			Pixel.bSolid = true;
			Pixel.Ramp = 0;
			Pixel.Index = 1;
			Pixel.Value = 1.f;
		}
		return Map;
	}

	// One text row per frame row: 'X' solid, '.' transparent. Rows are joined with '/'.
	FString MaskOf(const FFrameMap& Map)
	{
		FString Text;
		for (int32 y = 0; y < Map.Height; ++y)
		{
			if (y > 0)
			{
				Text += TEXT("/");
			}
			for (int32 x = 0; x < Map.Width; ++x)
			{
				Text += Map.Pixels[y * Map.Width + x].bSolid ? TEXT("X") : TEXT(".");
			}
		}
		return Text;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBPixelVfxFadeOutTest, "GBPixelVfx.Temporal.FadeOut",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGBPixelVfxFadeOutTest::RunTest(const FString& Parameters)
{
	using namespace GBPixelVfxConvert;
	using GBPixelVfxTestLocal::SolidMap;
	using GBPixelVfxTestLocal::MaskOf;

	// Dissolve over 3 faded frames of a 4x4 frame. Progress is 1/3, 2/3, 1. A pixel goes when its
	// Bayer 4x4 value / 16 is below the progress. Bayer rows are (0 8 2 10) (12 4 14 6) (3 11 1 9) (15 7 13 5),
	// so the first faded frame loses values 0..5, the second 0..10 and the last everything.
	// Pixel (1,0) is transparent from the start and must stay so.
	FSettings Dissolve;
	Dissolve.bLoop = false;
	Dissolve.FadeOutFrames = 3;
	Dissolve.FadeOutMode = TEXT("Dissolve");
	TArray<FFrameMap> DissolveMaps;
	for (int32 i = 0; i < 4; ++i)
	{
		FFrameMap Map = SolidMap(4);
		Map.Pixels[1].bSolid = false;
		DissolveMaps.Add(Map);
	}
	ApplyFadeOut(DissolveMaps, Dissolve);
	TestEqual(TEXT("dissolve leaves the pre-fade frame alone"), MaskOf(DissolveMaps[0]), FString(TEXT("X.XX/XXXX/XXXX/XXXX")));
	TestEqual(TEXT("dissolve first faded frame"), MaskOf(DissolveMaps[1]), FString(TEXT("...X/X.XX/.X.X/XXX.")));
	TestEqual(TEXT("dissolve second faded frame"), MaskOf(DissolveMaps[2]), FString(TEXT("..../X.X./.X../X.X.")));
	TestEqual(TEXT("dissolve last frame is completely empty"), DissolveMaps[3].SolidCount(), 0);
	TestEqual(TEXT("a surviving pixel keeps its color"), static_cast<int32>(DissolveMaps[1].Pixels[4].Index), 1);

	// A one-frame fade of a one-frame clip is empty too (progress is already 1).
	FSettings Single = Dissolve;
	Single.FadeOutFrames = 1;
	TArray<FFrameMap> SingleMaps;
	SingleMaps.Add(SolidMap(8));
	ApplyFadeOut(SingleMaps, Single);
	TestEqual(TEXT("a single faded frame is empty"), SingleMaps[0].SolidCount(), 0);

	// Erode over 2 faded frames of a 7x7 frame: the frame edge counts as transparent, so a pixel's
	// depth is its distance to the edge plus one (1 at the border, 4 in the center). Progress 0.5 of the
	// maximum depth 4 removes depth <= 2: the two outer rings. The last frame is empty.
	FSettings Erode;
	Erode.bLoop = false;
	Erode.FadeOutFrames = 2;
	Erode.FadeOutMode = TEXT("Erode");
	TArray<FFrameMap> ErodeMaps;
	for (int32 i = 0; i < 3; ++i)
	{
		ErodeMaps.Add(SolidMap(7));
	}
	ApplyFadeOut(ErodeMaps, Erode);
	TestEqual(TEXT("erode leaves the pre-fade frame alone"), ErodeMaps[0].SolidCount(), 49);
	TestEqual(TEXT("erode first faded frame keeps the 3x3 center"),
		MaskOf(ErodeMaps[1]), FString(TEXT("......./......./..XXX../..XXX../..XXX../......./.......")));
	TestEqual(TEXT("erode last frame is completely empty"), ErodeMaps[2].SolidCount(), 0);

	// The same frame with a transparent hole in the center: depth is the distance to the hole or the
	// edge, whichever is closer. The deepest pixel is at depth 2, so progress 0.5 removes depth <= 1:
	// the outer ring and the four pixels beside the hole.
	TArray<FFrameMap> HoleMaps;
	for (int32 i = 0; i < 3; ++i)
	{
		FFrameMap Map = SolidMap(7);
		Map.Pixels[3 * 7 + 3].bSolid = false;
		HoleMaps.Add(Map);
	}
	ApplyFadeOut(HoleMaps, Erode);
	TestEqual(TEXT("erode pre-fade frame keeps its hole"), HoleMaps[0].SolidCount(), 48);
	TestEqual(TEXT("erode first faded frame erodes from the hole too"),
		MaskOf(HoleMaps[1]), FString(TEXT("......./.XXXXX./.XX.XX./.X...X./.XX.XX./.XXXXX./.......")));
	TestEqual(TEXT("erode with a hole ends empty"), HoleMaps[2].SolidCount(), 0);

	// Looping clips never fade.
	FSettings Looped;
	Looped.bLoop = true;
	Looped.FadeOutFrames = 2;
	Looped.FadeOutMode = TEXT("Dissolve");
	TArray<FFrameMap> LoopedMaps;
	LoopedMaps.Add(SolidMap(8));
	LoopedMaps.Add(SolidMap(8));
	ApplyFadeOut(LoopedMaps, Looped);
	TestEqual(TEXT("looping clips do not fade"), LoopedMaps[0].SolidCount(), 64);
	TestEqual(TEXT("looping clips do not fade (last)"), LoopedMaps[1].SolidCount(), 64);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBPixelVfxPercentilePeakTest, "GBPixelVfx.Levels.PercentilePeak",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGBPixelVfxPercentilePeakTest::RunTest(const FString& Parameters)
{
	using namespace GBPixelVfxConvert;
	// Ten visible pixels with luminance 0..9: the 50th percentile (nearest rank) is 4,
	// so an implementation using the maximum (9) fails the value check below.
	FSettings Settings = GBPixelVfxTestLocal::MakeRampSettings();
	Settings.LevelsPercentile = 0.5f;
	Settings.AlphaMode = TEXT("Opacity");
	Settings.AlphaThreshold = 0.0f;
	FWorkFrame Frame;
	Frame.Width = 10;
	Frame.Height = 1;
	Frame.Color.SetNum(10);
	Frame.Opacity.SetNum(10);
	for (int32 i = 0; i < 10; ++i)
	{
		const float X = static_cast<float>(i);
		Frame.Color[i] = FLinearColor(X, X, X);
		Frame.Opacity[i] = 1.f;
	}
	float Peak = 1.f;
	float Hot = 1.f;
	float Cold = 1.f;
	ComputePeaks({ Frame }, Settings, Peak, Hot, Cold);
	TestTrue(TEXT("the percentile peak is 4, not the max"),
		FMath::IsNearlyEqual(Peak, 4.f, 0.01f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBPixelVfxLightTrackTest, "GBPixelVfx.Light.TrackEnergyAnchor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGBPixelVfxLightTrackTest::RunTest(const FString& Parameters)
{
	using namespace GBPixelVfxConvert;
	// Three 2x2 frames (pixel order: (0,0) (1,0) (0,1) (1,1); U = 0.25 / 0.75, V = 0.25 / 0.75).
	// Energy is the sum of v over solid HOT pixels, so unequal values, a solid cold pixel and a
	// transparent hot pixel all matter.
	FSettings Settings = GBPixelVfxTestLocal::MakeRampSettings();
	Settings.bLightEnabled = true;
	auto Put = [](FFrameMap& Map, int32 At, bool bSolid, bool bHot, float Value)
	{
		Map.Pixels[At].bSolid = bSolid;
		Map.Pixels[At].bHot = bHot;
		Map.Pixels[At].Value = Value;
	};
	auto Blank = []() -> FFrameMap
	{
		FFrameMap Map;
		Map.Width = 2;
		Map.Height = 2;
		Map.Pixels.SetNum(4);
		return Map;
	};
	// Frame 0: hot 0.5 at (0,0), hot 0.25 at (1,0), a solid COLD pixel of 1.0 at (0,1) and a
	// TRANSPARENT hot pixel of 1.0 at (1,1). E = 0.75, centroid U = (0.25*0.5 + 0.75*0.25) / 0.75 = 0.41667,
	// V = 0.25.
	FFrameMap F0 = Blank();
	Put(F0, 0, true, true, 0.5f);
	Put(F0, 1, true, true, 0.25f);
	Put(F0, 2, true, false, 1.f);
	Put(F0, 3, false, true, 1.f);
	// Frame 1: hot 0.25 at (0,0), 1.0 at (0,1), 0.75 at (1,1). E = 2.0,
	// U = (0.25*0.25 + 0.25*1.0 + 0.75*0.75) / 2 = 0.4375, V = (0.25*0.25 + 0.75*1.0 + 0.75*0.75) / 2 = 0.6875.
	FFrameMap F1 = Blank();
	Put(F1, 0, true, true, 0.25f);
	Put(F1, 2, true, true, 1.f);
	Put(F1, 3, true, true, 0.75f);
	// Frame 2: only a solid cold pixel: no energy and no centroid, so it must not pull the anchor.
	FFrameMap F2 = Blank();
	Put(F2, 0, true, false, 1.f);
	TArray<float> Track;
	float AnchorU = 0.f;
	float AnchorV = 0.f;
	ComputeLightTrack({ F0, F1, F2 }, Settings, Track, AnchorU, AnchorV);
	TestEqual(TEXT("three frames produce three track values"), Track.Num(), 3);
	if (Track.Num() == 3)
	{
		// Counting hot pixels instead of summing their values would give 2/3 and 1.
		TestTrue(TEXT("frame 0 energy is 0.75 / 2.0"), FMath::IsNearlyEqual(Track[0], 0.375f, 0.002f));
		TestTrue(TEXT("the brightest frame normalizes to 1"), FMath::IsNearlyEqual(Track[1], 1.f, 0.002f));
		TestTrue(TEXT("a frame without hot pixels has no energy"), FMath::IsNearlyEqual(Track[2], 0.f, 0.002f));
	}
	// Average of the two frame centroids (the empty frame is skipped): U = (0.41667 + 0.4375) / 2 = 0.42708,
	// V = (0.25 + 0.6875) / 2 = 0.46875. Weighting frames by energy would give U = 0.4318.
	TestTrue(TEXT("the anchor U averages the value-weighted frame centroids"), FMath::IsNearlyEqual(AnchorU, 0.42708f, 0.001f));
	TestTrue(TEXT("the anchor V averages the value-weighted frame centroids"), FMath::IsNearlyEqual(AnchorV, 0.46875f, 0.001f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBPixelVfxFractionalAlphaTest, "GBPixelVfx.Alpha.FractionalUnpremultiply",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGBPixelVfxFractionalAlphaTest::RunTest(const FString& Parameters)
{
	using namespace GBPixelVfxConvert;
	// Coverage 0.5 over HDR red (2, 0, 0): premultiplied RGB is (1, 0, 0), so the
	// un-premultiplied color is (2, 0, 0) with luminance ~0.43. An implementation that
	// multiplies by alpha again would see (0.5, 0, 0) and map a much darker band.
	FSettings Settings = GBPixelVfxTestLocal::MakeRampSettings();
	Settings.AlphaMode = TEXT("Opacity");
	FWorkFrame Frame;
	Frame.Width = 1;
	Frame.Height = 1;
	Frame.Color.SetNum(1);
	Frame.Opacity.SetNum(1);
	Frame.Color[0] = FLinearColor(1.f, 0.f, 0.f);
	Frame.Opacity[0] = 0.5f;
	float Peak = 1.f;
	float Hot = 1.f;
	float Cold = 1.f;
	ComputePeaks({ Frame }, Settings, Peak, Hot, Cold);
	TestTrue(TEXT("the peak sees the un-premultiplied color"),
		FMath::IsNearlyEqual(Peak, 0.4252f, 0.01f));
	const FFrameMap Map = MapFrame(Frame, Settings, Peak, Hot, Cold);
	TestTrue(TEXT("fractional coverage is solid"), Map.Pixels[0].bSolid);
	TestTrue(TEXT("fractional coverage maps to the top band"),
		Map.Pixels[0].Value > 0.99f && Map.Pixels[0].Index == Settings.Ramps[0].Num() - 1);
	return true;
}
