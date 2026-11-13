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
	// Hot and cold levels are normalized independently: the hot peak (brightest hot
	// pixel) differs from the cold peak, and each pixel's value uses its own peak.
	TestTrue(TEXT("hot and cold peaks differ"), !FMath::IsNearlyEqual(Hot, Cold, 0.01f));
	const float HotLum0 = PixelLuminance(FLinearColor(0.8f, 0.8f, 0.8f));
	TestTrue(TEXT("luminance-only value uses the hot peak"),
		FMath::IsNearlyEqual(Map.Pixels[0].Value, FMath::Clamp(HotLum0 / Hot, 0.f, 1.f), 0.01f));
	const float ColdLum = PixelLuminance(FLinearColor(0.2f, 0.2f, 0.2f));
	TestTrue(TEXT("cold value uses the cold peak"),
		FMath::IsNearlyEqual(Map.Pixels[3].Value, FMath::Clamp(ColdLum / Cold, 0.f, 1.f), 0.01f));
	TArray<TArray<FColor>> Pixels;
	ResolveFrames({ Map }, Settings, Pixels);
	TestEqual(TEXT("hot pixel resolves to a ramp 0 color"), Pixels[0][2], Settings.Ramps[0][Map.Pixels[2].Index]);
	TestEqual(TEXT("cold pixel resolves to a ramp 1 color"), Pixels[0][3], Settings.Ramps[1][Map.Pixels[3].Index]);
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
	// Non-loop clips never blend: extras are dropped and kept frames are untouched.
	TArray<FWorkFrame> Flat;
	Flat.Add(SolidFrame(1.f, 1.f));
	Flat.Add(SolidFrame(2.f, 0.5f));
	Flat.Add(SolidFrame(9.f, 0.2f));
	ApplyLoopBlend(Flat, 2, 2, false);
	TestEqual(TEXT("non-loop drops the extras"), Flat.Num(), 2);
	TestTrue(TEXT("non-loop leaves frame 0 alone"), FMath::Abs(Flat[0].Color[0].R - 1.f) < 0.01f);
	TestTrue(TEXT("non-loop leaves frame 1 opacity alone"), FMath::Abs(Flat[1].Opacity[0] - 0.5f) < 0.01f);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBPixelVfxQuadPivotTest, "GBPixelVfx.Clip.QuadPivotPlacement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGBPixelVfxQuadPivotTest::RunTest(const FString& Parameters)
{
	FGBPixelVfxClip Clip;
	Clip.WorldWidthCm = 200.f;
	Clip.WorldHeightCm = 400.f;
	Clip.PivotU = 0.5f;
	Clip.PivotV = 1.f;
	Clip.Light.AnchorU = 0.5f;
	Clip.Light.AnchorV = 0.5f;
	// Upright quad: local X = world X, local Y = world Z (up), normal faces -X.
	const FQuat R(FMatrix(FVector(0.f, -1.f, 0.f), FVector(0.f, 0.f, 1.f), FVector(-1.f, 0.f, 0.f),
		FVector::ZeroVector).Rotator());
	const FVector Comp(100.f, 200.f, 0.f);
	FVector QuadLoc;
	FVector LightLoc;
	FGBPixelVfxClip::ComputeQuadAndLightPlacement(Clip, 1.f, Comp, R, QuadLoc, LightLoc);
	// PivotV = 1 (bottom of the frame): the quad center is H/2 above the component.
	TestTrue(TEXT("pivot bottom puts the quad center H/2 up"),
		FMath::IsNearlyEqual(QuadLoc.Z, 200.f, 0.01f));
	TestTrue(TEXT("pivot bottom does not shift sideways"),
		FMath::IsNearlyEqual(QuadLoc.X, Comp.X, 0.01f)
		&& FMath::IsNearlyEqual(QuadLoc.Y, Comp.Y, 0.01f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBPixelVfxLightAnchorTest, "GBPixelVfx.Clip.LightAnchorPlacement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGBPixelVfxLightAnchorTest::RunTest(const FString& Parameters)
{
	FGBPixelVfxClip Clip;
	Clip.WorldWidthCm = 200.f;
	Clip.WorldHeightCm = 400.f;
	Clip.PivotU = 0.5f;
	Clip.PivotV = 1.f;
	Clip.Light.AnchorU = 0.5f;
	Clip.Light.AnchorV = 0.5f;
	// Same upright frame as the pivot test.
	const FQuat R(FMatrix(FVector(0.f, -1.f, 0.f), FVector(0.f, 0.f, 1.f), FVector(-1.f, 0.f, 0.f),
		FVector::ZeroVector).Rotator());
	const FVector Comp(100.f, 200.f, 0.f);
	FVector QuadLoc;
	FVector LightLoc;
	FGBPixelVfxClip::ComputeQuadAndLightPlacement(Clip, 1.f, Comp, R, QuadLoc, LightLoc);
	// AnchorV = 0.5 is H/2 above the pivot: the light sits H/2 above the component,
	// pushed 10 cm toward the camera (quad normal = -X here).
	TestTrue(TEXT("mid-frame anchor puts the light H/2 above the pivot"),
		FMath::IsNearlyEqual(LightLoc.Z, 200.f, 0.01f));
	TestTrue(TEXT("the light keeps the component Y"), FMath::IsNearlyEqual(LightLoc.Y, Comp.Y, 0.01f));
	TestTrue(TEXT("the light is pushed 10 cm toward the camera"),
		FMath::IsNearlyEqual(LightLoc.X, Comp.X - 10.f, 0.01f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBPixelVfxColorClusterTest, "GBPixelVfx.Cleanup.ColorClusters",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGBPixelVfxColorClusterTest::RunTest(const FString& Parameters)
{
	using namespace GBPixelVfxConvert;
	FSettings Settings = GBPixelVfxTestLocal::MakeRampSettings();
	// Two colors in ramp 0; the frame is all color 1 with a 2-pixel color-2 speck.
	FFrameMap Map;
	Map.Width = 6;
	Map.Height = 4;
	Map.Pixels.SetNum(24);
	for (int32 i = 0; i < 24; ++i)
	{
		Map.Pixels[i].bSolid = true;
		Map.Pixels[i].Ramp = 0;
		Map.Pixels[i].Index = 1;
		Map.Pixels[i].Value = 0.5f;
	}
	Map.Pixels[7].Index = 2;
	Map.Pixels[8].Index = 2;
	FFrameMap Cleaned = Map;
	RecolorSmallColorClusters(Cleaned, Settings, 3);
	TestEqual(TEXT("a 2-pixel speck is recolored when the minimum is 3"), Cleaned.SolidCount(), 24);
	TestEqual(TEXT("the speck takes the surrounding color"), Cleaned.Pixels[7].Index, 1);
	TestEqual(TEXT("the speck keeps its ramp"), Cleaned.Pixels[7].Ramp, 0);
	FFrameMap Kept = Map;
	RecolorSmallColorClusters(Kept, Settings, 2);
	TestEqual(TEXT("a 2-pixel region is kept when the minimum is 2"), Kept.Pixels[7].Index, 2);
	// Off: nothing changes.
	FFrameMap Off = Map;
	RecolorSmallColorClusters(Off, Settings, 0);
	TestEqual(TEXT("clusters off leaves the speck alone"), Off.Pixels[7].Index, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBPixelVfxFadeOutTest, "GBPixelVfx.Temporal.FadeOut",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGBPixelVfxFadeOutTest::RunTest(const FString& Parameters)
{
	using namespace GBPixelVfxConvert;
	auto SolidMap = []() -> FFrameMap
	{
		FFrameMap Map;
		Map.Width = 8;
		Map.Height = 8;
		Map.Pixels.SetNum(64);
		for (int32 i = 0; i < 64; ++i)
		{
			Map.Pixels[i].bSolid = true;
			Map.Pixels[i].Ramp = 0;
			Map.Pixels[i].Index = 1;
			Map.Pixels[i].Value = 1.f;
		}
		return Map;
	};
	// Dissolve: removals are monotonic (frame-space pattern) and grow with t.
	FSettings Dissolve;
	Dissolve.bLoop = false;
	Dissolve.FadeOutFrames = 2;
	Dissolve.FadeOutMode = TEXT("Dissolve");
	TArray<FFrameMap> DissolveMaps;
	DissolveMaps.Add(SolidMap());
	DissolveMaps.Add(SolidMap());
	DissolveMaps.Add(SolidMap());
	ApplyFadeOut(DissolveMaps, Dissolve);
	const int32 First = DissolveMaps[1].SolidCount();
	const int32 Last = DissolveMaps[2].SolidCount();
	TestTrue(TEXT("dissolve removes pixels over the fade"), First < 64 && Last < First);
	TestEqual(TEXT("dissolve leaves pre-fade frames alone"), DissolveMaps[0].SolidCount(), 64);
	// Removed pixels never come back: every solid pixel of the last frame is solid in the first faded frame.
	for (int32 i = 0; i < 64; ++i)
	{
		if (DissolveMaps[2].Pixels[i].bSolid)
		{
			TestTrue(TEXT("dissolve removals never return"), DissolveMaps[1].Pixels[i].bSolid);
		}
	}
	// Erode: a solid block shrinks from the transparent border inward.
	FSettings Erode;
	Erode.bLoop = false;
	Erode.FadeOutFrames = 1;
	Erode.FadeOutMode = TEXT("Erode");
	TArray<FFrameMap> ErodeMaps;
	FFrameMap Block = SolidMap();
	Block.Pixels[0].bSolid = false; // one transparent corner seeds the erosion
	ErodeMaps.Add(Block);
	ApplyFadeOut(ErodeMaps, Erode);
	TestTrue(TEXT("erode removes the border first"), !ErodeMaps[0].Pixels[1].bSolid || !ErodeMaps[0].Pixels[8].bSolid);
	TestTrue(TEXT("erode keeps the deep interior"), ErodeMaps[0].Pixels[27].bSolid);
	// Looping clips never fade.
	FSettings Looped;
	Looped.bLoop = true;
	Looped.FadeOutFrames = 2;
	Looped.FadeOutMode = TEXT("Dissolve");
	TArray<FFrameMap> LoopedMaps;
	LoopedMaps.Add(SolidMap());
	LoopedMaps.Add(SolidMap());
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
	// Two 2x1 frames: frame 0 has one hot pixel (v = 1) at U = 0.25, frame 1 has two
	// hot pixels (v = 1 each) at U = 0.25 and 0.75. Energies are 1 and 2, so the
	// normalized track is [0.5, 1]; the anchor averages the per-frame centroids.
	FSettings Settings = GBPixelVfxTestLocal::MakeRampSettings();
	Settings.bLightEnabled = true;
	FFrameMap F0;
	F0.Width = 2;
	F0.Height = 1;
	F0.Pixels.SetNum(2);
	F0.Pixels[0].bSolid = true;
	F0.Pixels[0].bHot = true;
	F0.Pixels[0].Ramp = 0;
	F0.Pixels[0].Index = 1;
	F0.Pixels[0].Value = 1.f;
	FFrameMap F1;
	F1.Width = 2;
	F1.Height = 1;
	F1.Pixels.SetNum(2);
	F1.Pixels[0].bSolid = true;
	F1.Pixels[0].bHot = true;
	F1.Pixels[0].Ramp = 0;
	F1.Pixels[0].Index = 1;
	F1.Pixels[0].Value = 1.f;
	F1.Pixels[1].bSolid = true;
	F1.Pixels[1].bHot = true;
	F1.Pixels[1].Ramp = 0;
	F1.Pixels[1].Index = 1;
	F1.Pixels[1].Value = 1.f;
	TArray<float> Track;
	float AnchorU = 0.f;
	float AnchorV = 0.f;
	ComputeLightTrack({ F0, F1 }, Settings, Track, AnchorU, AnchorV);
	TestEqual(TEXT("two frames produce two track values"), Track.Num(), 2);
	if (Track.Num() == 2)
	{
		// A constant-lighting implementation returning 1 for both fails here.
		TestTrue(TEXT("dimmer frame has lower energy"), FMath::IsNearlyEqual(Track[0], 0.5f, 0.01f));
		TestTrue(TEXT("brightest frame normalizes to 1"), FMath::IsNearlyEqual(Track[1], 1.f, 0.01f));
	}
	// Frame centroids: 0.25 and 0.5; the anchor averages them to 0.375.
	TestTrue(TEXT("the anchor U averages the frame centroids"), FMath::IsNearlyEqual(AnchorU, 0.375f, 0.01f));
	TestTrue(TEXT("the anchor V is the row middle"), FMath::IsNearlyEqual(AnchorV, 0.5f, 0.01f));
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
