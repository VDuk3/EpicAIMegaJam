// GBPixelVfxConvert: Niagara frame -> palette mapped pixel art. See the header for the step order.
#include "GBPixelVfxConvert.h"

namespace GBPixelVfxConvertLocal
{
	int32 HexDigit(TCHAR C)
	{
		if (C >= TEXT('0') && C <= TEXT('9')) return C - TEXT('0');
		if (C >= TEXT('a') && C <= TEXT('f')) return 10 + C - TEXT('a');
		if (C >= TEXT('A') && C <= TEXT('F')) return 10 + C - TEXT('A');
		return -1;
	}

	float Saturate(float X) { return FMath::Clamp(X, 0.f, 1.f); }

	bool IsSplitMode(const GBPixelVfxConvert::FSettings& Settings)
	{
		return Settings.ColorMode.Equals(TEXT("Split"), ESearchCase::IgnoreCase) && Settings.Ramps.Num() >= 2;
	}

	int32 RampIndexForValue(float V, int32 Count)
	{
		if (Count <= 0)
		{
			return 0;
		}
		return FMath::Clamp(FMath::FloorToInt(V * static_cast<float>(Count)), 0, Count - 1);
	}

	// Nearest-rank percentile of a sorted ascending array. Falls back to 1 when empty.
	float PercentileOfSorted(const TArray<float>& Sorted, float P)
	{
		if (Sorted.Num() == 0)
		{
			return 1.f;
		}
		const int32 Rank = FMath::Clamp(FMath::CeilToInt(P * Sorted.Num()) - 1, 0, Sorted.Num() - 1);
		return Sorted[Rank];
	}

	FLinearColor ToneMapForNearest(const FLinearColor& C, float Peak)
	{
		const float Inv = 1.f / FMath::Max(Peak, 1.e-4f);
		const FLinearColor S(C.R * Inv, C.G * Inv, C.B * Inv);
		return FLinearColor(S.R / (1.f + S.R), S.G / (1.f + S.G), S.B / (1.f + S.B));
	}

	void NearestRampColor(const FLinearColor& C, const GBPixelVfxConvert::FSettings& Settings,
		float Peak, uint8& OutRamp, uint8& OutIndex)
	{
		OutRamp = 0;
		OutIndex = 0;
		if (Settings.Ramps.Num() == 0 || Settings.Ramps[0].Num() == 0)
		{
			return;
		}
		const FVector Target = GBPixelVfxConvert::LabFromLinear(ToneMapForNearest(C, Peak));
		float Best = FLT_MAX;
		for (int32 Ramp = 0; Ramp < Settings.Ramps.Num(); ++Ramp)
		{
			const TArray<FColor>& Colors = Settings.Ramps[Ramp];
			for (int32 i = 0; i < Colors.Num(); ++i)
			{
				// Palette colors are used directly (sRGB -> linear -> OKLab):
				// tone-mapping them would move the palette positions in OKLab.
				const FVector Candidate = GBPixelVfxConvert::LabFromLinear(FLinearColor(Colors[i]));
				const float Dist = static_cast<float>(FVector::DistSquared(Target, Candidate));
				if (Dist < Best)
				{
					Best = Dist;
					OutRamp = static_cast<uint8>(FMath::Min(Ramp, 255));
					OutIndex = static_cast<uint8>(FMath::Min(i, 255));
				}
			}
		}
	}

	bool Inside(int32 X, int32 Y, int32 W, int32 H)
	{
		return X >= 0 && Y >= 0 && X < W && Y < H;
	}
}

namespace GBPixelVfxConvert
{
bool ParseHexColor(const FString& Hex, FColor& OutColor)
{
	FString Text = Hex;
	Text.TrimStartAndEndInline();
	if (Text.StartsWith(TEXT("#")))
	{
		Text.RightChopInline(1);
	}
	if (Text.Len() != 6)
	{
		return false;
	}
	uint8 Bytes[4] = { 0, 0, 0, 255 };
	for (int32 i = 0; i < 3; ++i)
	{
		const int32 Hi = GBPixelVfxConvertLocal::HexDigit(Text[i * 2]);
		const int32 Lo = GBPixelVfxConvertLocal::HexDigit(Text[i * 2 + 1]);
		if (Hi < 0 || Lo < 0)
		{
			return false;
		}
		Bytes[i] = static_cast<uint8>(Hi * 16 + Lo);
	}
	OutColor = FColor(Bytes[0], Bytes[1], Bytes[2], Bytes[3]);
	return true;
}

float PixelLuminance(const FLinearColor& Color)
{
	return Color.R * 0.2126f + Color.G * 0.7152f + Color.B * 0.0722f;
}

float PixelSaturation(const FLinearColor& Color)
{
	const float MaxC = FMath::Max3(Color.R, Color.G, Color.B);
	if (MaxC <= 1.e-6f)
	{
		return 0.f;
	}
	const float MinC = FMath::Min3(Color.R, Color.G, Color.B);
	return (MaxC - MinC) / MaxC;
}

FVector LabFromLinear(const FLinearColor& Color)
{
	const float R = FMath::Max(0.f, Color.R);
	const float G = FMath::Max(0.f, Color.G);
	const float B = FMath::Max(0.f, Color.B);
	const float L = 0.4122214708f * R + 0.5363325363f * G + 0.0514459929f * B;
	const float M = 0.2119034982f * R + 0.6806995451f * G + 0.1073969566f * B;
	const float S = 0.0883024619f * R + 0.2817188376f * G + 0.6299787005f * B;
	const float Lc = FMath::Pow(FMath::Max(0.f, L), 1.f / 3.f);
	const float Mc = FMath::Pow(FMath::Max(0.f, M), 1.f / 3.f);
	const float Sc = FMath::Pow(FMath::Max(0.f, S), 1.f / 3.f);
	return FVector(
		0.2104542553 * Lc + 0.7936177850 * Mc - 0.0040720468 * Sc,
		1.9779984951 * Lc - 2.4285922050 * Mc + 0.4505937099 * Sc,
		0.0259040371 * Lc + 0.7827717662 * Mc - 0.8086757660 * Sc);
}

bool IsHotPixel(const FLinearColor& Unpremultiplied, const FSettings& Settings)
{
	return PixelLuminance(Unpremultiplied) >= Settings.HotLuminance
		|| PixelSaturation(Unpremultiplied) >= Settings.HotSaturation;
}

void ApplyExposure(TArray<FWorkFrame>& Frames, float Exposure)
{
	if (FMath::IsNearlyEqual(Exposure, 1.f))
	{
		return;
	}
	for (FWorkFrame& Frame : Frames)
	{
		for (FLinearColor& Pixel : Frame.Color)
		{
			Pixel.R *= Exposure;
			Pixel.G *= Exposure;
			Pixel.B *= Exposure;
		}
	}
}

void ApplyLoopBlend(TArray<FWorkFrame>& Frames, int32 KeepCount, int32 BlendFrames, bool bLoop)
{
	if (!bLoop || BlendFrames <= 0 || KeepCount <= 0 || Frames.Num() < KeepCount + BlendFrames)
	{
		while (Frames.Num() > KeepCount)
		{
			Frames.Pop();
		}
		return;
	}
	const int32 B = FMath::Min(BlendFrames, KeepCount);
	for (int32 i = 0; i < B; ++i)
	{
		FWorkFrame& Dst = Frames[i];
		const FWorkFrame& Extra = Frames[KeepCount + i];
		if (Dst.Color.Num() != Extra.Color.Num() || Dst.Opacity.Num() != Extra.Opacity.Num())
		{
			continue;
		}
		const float T = (static_cast<float>(i) + 0.5f) / static_cast<float>(B);
		for (int32 p = 0; p < Dst.Color.Num(); ++p)
		{
			Dst.Color[p] = Extra.Color[p] * (1.f - T) + Dst.Color[p] * T;
		}
		for (int32 p = 0; p < Dst.Opacity.Num(); ++p)
		{
			Dst.Opacity[p] = Extra.Opacity[p] * (1.f - T) + Dst.Opacity[p] * T;
		}
	}
	while (Frames.Num() > KeepCount)
	{
		Frames.Pop();
	}
}

FWorkFrame DownsampleFrame(const FWorkFrame& In, int32 OutWidth, int32 OutHeight)
{
	FWorkFrame Out;
	Out.Width = OutWidth;
	Out.Height = OutHeight;
	if (In.Width <= 0 || In.Height <= 0 || OutWidth <= 0 || OutHeight <= 0
		|| In.Color.Num() != In.Width * In.Height || In.Opacity.Num() != In.Width * In.Height)
	{
		return Out;
	}
	Out.Color.SetNumZeroed(OutWidth * OutHeight);
	Out.Opacity.SetNumZeroed(OutWidth * OutHeight);
	for (int32 y = 0; y < OutHeight; ++y)
	{
		const int32 Y0 = y * In.Height / OutHeight;
		const int32 Y1 = (y + 1) * In.Height / OutHeight;
		for (int32 x = 0; x < OutWidth; ++x)
		{
			const int32 X0 = x * In.Width / OutWidth;
			const int32 X1 = (x + 1) * In.Width / OutWidth;
			FLinearColor Sum(0.f, 0.f, 0.f, 0.f);
			float OpSum = 0.f;
			int32 Count = 0;
			for (int32 sy = Y0; sy < Y1; ++sy)
			{
				for (int32 sx = X0; sx < X1; ++sx)
				{
					const int32 Src = sy * In.Width + sx;
					Sum += In.Color[Src];
					OpSum += In.Opacity[Src];
					++Count;
				}
			}
			const int32 Dst = y * OutWidth + x;
			if (Count > 0)
			{
				Out.Color[Dst] = Sum / static_cast<float>(Count);
				Out.Opacity[Dst] = OpSum / static_cast<float>(Count);
			}
		}
	}
	return Out;
}

void ComputePeaks(const TArray<FWorkFrame>& Frames, const FSettings& Settings,
	float& OutPeak, float& OutHotPeak, float& OutColdPeak)
{
	using namespace GBPixelVfxConvertLocal;
	TArray<float> All;
	TArray<float> Hot;
	TArray<float> Cold;
	const bool bSplit = IsSplitMode(Settings);
	for (const FWorkFrame& Frame : Frames)
	{
		const int32 Count = FMath::Min(Frame.Color.Num(), Frame.Opacity.Num());
		for (int32 i = 0; i < Count; ++i)
		{
			const float Op = Frame.Opacity[i];
			const FLinearColor& RGB = Frame.Color[i];
			const float Lum = PixelLuminance(RGB);
			float A = Op;
			if (Settings.AlphaMode.Equals(TEXT("Luminance"), ESearchCase::IgnoreCase))
			{
				A = Saturate(Lum);
			}
			else if (Settings.AlphaMode.Equals(TEXT("Max"), ESearchCase::IgnoreCase))
			{
				A = FMath::Max(Op, Saturate(Lum));
			}
			if (A < Settings.AlphaThreshold)
			{
				continue;
			}
			const FLinearColor C = RGB / FMath::Max(A, 1.e-4f);
			const float LumC = PixelLuminance(C);
			All.Add(LumC);
			if (bSplit)
			{
				if (IsHotPixel(C, Settings))
				{
					Hot.Add(LumC);
				}
				else
				{
					Cold.Add(LumC);
				}
			}
		}
	}
	All.Sort();
	Hot.Sort();
	Cold.Sort();
	OutPeak = PercentileOfSorted(All, Settings.LevelsPercentile);
	OutHotPeak = Hot.Num() > 0 ? PercentileOfSorted(Hot, Settings.LevelsPercentile) : OutPeak;
	OutColdPeak = Cold.Num() > 0 ? PercentileOfSorted(Cold, Settings.LevelsPercentile) : OutPeak;
	OutPeak = FMath::Max(OutPeak, 1.e-4f);
	OutHotPeak = FMath::Max(OutHotPeak, 1.e-4f);
	OutColdPeak = FMath::Max(OutColdPeak, 1.e-4f);
}

FFrameMap MapFrame(const FWorkFrame& Frame, const FSettings& Settings,
	float Peak, float HotPeak, float ColdPeak)
{
	using namespace GBPixelVfxConvertLocal;
	FFrameMap Map;
	Map.Width = Frame.Width;
	Map.Height = Frame.Height;
	const int32 Count = FMath::Min(Frame.Color.Num(), Frame.Opacity.Num());
	Map.Pixels.SetNum(Count);
	const bool bSplit = IsSplitMode(Settings);
	const bool bNearest = Settings.ColorMode.Equals(TEXT("Nearest"), ESearchCase::IgnoreCase);
	const int32 Ramp0Count = Settings.Ramps.Num() > 0 ? Settings.Ramps[0].Num() : 0;
	const int32 Ramp1Count = Settings.Ramps.Num() > 1 ? Settings.Ramps[1].Num() : 0;
	for (int32 i = 0; i < Count; ++i)
	{
		FMappedPixel& Dst = Map.Pixels[i];
		const float Op = Frame.Opacity[i];
		const FLinearColor& RGB = Frame.Color[i];
		const float Lum = PixelLuminance(RGB);
		float A = Op;
		if (Settings.AlphaMode.Equals(TEXT("Luminance"), ESearchCase::IgnoreCase))
		{
			A = Saturate(Lum);
		}
		else if (Settings.AlphaMode.Equals(TEXT("Max"), ESearchCase::IgnoreCase))
		{
			A = FMath::Max(Op, Saturate(Lum));
		}
		if (A < Settings.AlphaThreshold)
		{
			continue;
		}
		const FLinearColor C = RGB / FMath::Max(A, 1.e-4f);
		const float LumC = PixelLuminance(C);
		Dst.bSolid = true;
		if (bNearest)
		{
			Dst.Value = Saturate(FMath::Pow(FMath::Clamp(LumC / Peak, 0.f, 1.f), Settings.Gamma));
			Dst.bHot = true;
			NearestRampColor(C, Settings, Peak, Dst.Ramp, Dst.Index);
		}
		else if (bSplit)
		{
			if (IsHotPixel(C, Settings))
			{
				Dst.bHot = true;
				Dst.Value = Saturate(FMath::Pow(FMath::Clamp(LumC / HotPeak, 0.f, 1.f), Settings.Gamma));
				Dst.Ramp = 0;
				Dst.Index = static_cast<uint8>(RampIndexForValue(Dst.Value, Ramp0Count));
			}
			else
			{
				Dst.bHot = false;
				Dst.Value = Saturate(FMath::Pow(FMath::Clamp(LumC / ColdPeak, 0.f, 1.f), Settings.Gamma));
				Dst.Ramp = 1;
				Dst.Index = static_cast<uint8>(RampIndexForValue(Dst.Value, Ramp1Count));
			}
		}
		else
		{
			Dst.bHot = true;
			Dst.Value = Saturate(FMath::Pow(FMath::Clamp(LumC / Peak, 0.f, 1.f), Settings.Gamma));
			Dst.Ramp = 0;
			Dst.Index = static_cast<uint8>(RampIndexForValue(Dst.Value, Ramp0Count));
		}
	}
	return Map;
}

void ApplyHysteresis(FFrameMap& Current, const FFrameMap& Prev, const FSettings& Settings)
{
	using namespace GBPixelVfxConvertLocal;
	if (Current.Pixels.Num() != Prev.Pixels.Num() || Settings.Hysteresis <= 0.f)
	{
		return;
	}
	const int32 PrevSolid = Prev.SolidCount();
	if (PrevSolid > 0)
	{
		const int32 CurSolid = Current.SolidCount();
		// Fast changing frames (explosions): damping would smear real change, so skip it.
		if (FMath::Abs(static_cast<float>(CurSolid - PrevSolid)) / static_cast<float>(PrevSolid) > 0.3f)
		{
			return;
		}
	}
	for (int32 i = 0; i < Current.Pixels.Num(); ++i)
	{
		FMappedPixel& Dst = Current.Pixels[i];
		const FMappedPixel& Src = Prev.Pixels[i];
		if (!Dst.bSolid || !Src.bSolid || Dst.Ramp != Src.Ramp)
		{
			continue;
		}
		if (Dst.Index == Src.Index)
		{
			continue;
		}
		int32 RampLen = 0;
		if (Src.Ramp < static_cast<uint8>(Settings.Ramps.Num()))
		{
			RampLen = Settings.Ramps[Src.Ramp].Num();
		}
		if (RampLen <= 0)
		{
			continue;
		}
		// The previous band widened by Hysteresis on both sides keeps the old index.
		const float BandLo = static_cast<float>(Src.Index) / static_cast<float>(RampLen) - Settings.Hysteresis;
		const float BandHi = (static_cast<float>(Src.Index) + 1.f) / static_cast<float>(RampLen) + Settings.Hysteresis;
		if (Dst.Value >= BandLo && Dst.Value <= BandHi)
		{
			Dst.Index = Src.Index;
		}
	}
}

void RemoveSmallIslands(FFrameMap& Map, int32 MinIslandPixels)
{
	using namespace GBPixelVfxConvertLocal;
	if (MinIslandPixels <= 1 || Map.Width <= 0 || Map.Height <= 0
		|| Map.Pixels.Num() != Map.Width * Map.Height)
	{
		return;
	}
	const int32 W = Map.Width;
	const int32 H = Map.Height;
	TArray<int32> Island;
	Island.Reserve(64);
	TArray<int32> Stack;
	Stack.Reserve(64);
	TArray<uint8> Seen;
	Seen.SetNumZeroed(W * H);
	const int32 DX[4] = { 1, -1, 0, 0 };
	const int32 DY[4] = { 0, 0, 1, -1 };
	for (int32 Start = 0; Start < W * H; ++Start)
	{
		if (!Map.Pixels[Start].bSolid || Seen[Start])
		{
			continue;
		}
		Island.Reset();
		Stack.Reset();
		Stack.Add(Start);
		Seen[Start] = 1;
		while (Stack.Num() > 0)
		{
			const int32 At = Stack.Pop();
			Island.Add(At);
			const int32 X = At % W;
			const int32 Y = At / W;
			for (int32 d = 0; d < 4; ++d)
			{
				const int32 NX = X + DX[d];
				const int32 NY = Y + DY[d];
				if (!Inside(NX, NY, W, H))
				{
					continue;
				}
				const int32 NAt = NY * W + NX;
				if (!Seen[NAt] && Map.Pixels[NAt].bSolid)
				{
					Seen[NAt] = 1;
					Stack.Add(NAt);
				}
			}
		}
		if (Island.Num() < MinIslandPixels)
		{
			for (int32 At : Island)
			{
				Map.Pixels[At].bSolid = false;
			}
		}
	}
}

void FillOnePixelHoles(FFrameMap& Map)
{
	using namespace GBPixelVfxConvertLocal;
	if (Map.Width <= 0 || Map.Height <= 0 || Map.Pixels.Num() != Map.Width * Map.Height)
	{
		return;
	}
	const int32 W = Map.Width;
	const int32 H = Map.Height;
	const int32 DX[4] = { 1, -1, 0, 0 };
	const int32 DY[4] = { 0, 0, 1, -1 };
	for (int32 y = 0; y < H; ++y)
	{
		for (int32 x = 0; x < W; ++x)
		{
			const int32 At = y * W + x;
			if (Map.Pixels[At].bSolid)
			{
				continue;
			}
			FMappedPixel Votes[4];
			int32 VoteCount = 0;
			bool bAllSolid = true;
			for (int32 d = 0; d < 4; ++d)
			{
				const int32 NX = x + DX[d];
				const int32 NY = y + DY[d];
				if (!Inside(NX, NY, W, H) || !Map.Pixels[NY * W + NX].bSolid)
				{
					bAllSolid = false;
					break;
				}
				Votes[VoteCount++] = Map.Pixels[NY * W + NX];
			}
			if (!bAllSolid)
			{
				continue;
			}
			// Most common neighbor color wins; ties go to the first neighbor.
			int32 Best = 0;
			int32 BestVotes = -1;
			for (int32 a = 0; a < VoteCount; ++a)
			{
				int32 VotesForA = 0;
				for (int32 b = 0; b < VoteCount; ++b)
				{
					if (Votes[b].Ramp == Votes[a].Ramp && Votes[b].Index == Votes[a].Index)
					{
						++VotesForA;
					}
				}
				if (VotesForA > BestVotes)
				{
					BestVotes = VotesForA;
					Best = a;
				}
			}
			Map.Pixels[At] = Votes[Best];
			Map.Pixels[At].bSolid = true;
		}
	}
}

void RecolorSmallColorClusters(FFrameMap& Map, const FSettings& Settings, int32 MinPixels)
{
	using namespace GBPixelVfxConvertLocal;
	if (MinPixels <= 1 || Map.Width <= 0 || Map.Height <= 0
		|| Map.Pixels.Num() != Map.Width * Map.Height)
	{
		return;
	}
	const int32 W = Map.Width;
	const int32 H = Map.Height;
	const TArray<FMappedPixel> Orig = Map.Pixels;
	TArray<uint8> Seen;
	Seen.SetNumZeroed(W * H);
	const int32 DX[4] = { 1, -1, 0, 0 };
	const int32 DY[4] = { 0, 0, 1, -1 };
	TArray<int32> Island;
	TArray<int32> Stack;
	for (int32 Start = 0; Start < W * H; ++Start)
	{
		if (!Orig[Start].bSolid || Seen[Start])
		{
			continue;
		}
		Island.Reset();
		Stack.Reset();
		Stack.Add(Start);
		Seen[Start] = 1;
		while (Stack.Num() > 0)
		{
			const int32 At = Stack.Pop();
			Island.Add(At);
			const int32 X = At % W;
			const int32 Y = At / W;
			for (int32 d = 0; d < 4; ++d)
			{
				const int32 NX = X + DX[d];
				const int32 NY = Y + DY[d];
				if (!Inside(NX, NY, W, H))
				{
					continue;
				}
				const int32 NAt = NY * W + NX;
				if (!Seen[NAt] && Orig[NAt].bSolid
					&& Orig[NAt].Ramp == Orig[Start].Ramp && Orig[NAt].Index == Orig[Start].Index)
				{
					Seen[NAt] = 1;
					Stack.Add(NAt);
				}
			}
		}
		if (Island.Num() >= MinPixels)
		{
			continue;
		}
		// Border: solid 4-neighbors of the region with a different color (found on the pre-pass state).
		TArray<int32> Border;
		for (int32 At : Island)
		{
			const int32 X = At % W;
			const int32 Y = At / W;
			for (int32 d = 0; d < 4; ++d)
			{
				const int32 NX = X + DX[d];
				const int32 NY = Y + DY[d];
				if (!Inside(NX, NY, W, H) || !Orig[NY * W + NX].bSolid)
				{
					continue;
				}
				const int32 NAt = NY * W + NX;
				if (Orig[NAt].Ramp == Orig[Start].Ramp && Orig[NAt].Index == Orig[Start].Index)
				{
					continue;
				}
				Border.Add(NAt);
			}
		}
		if (Border.Num() == 0)
		{
			continue;
		}
		uint8 BestRamp = Orig[Border[0]].Ramp;
		uint8 BestIndex = Orig[Border[0]].Index;
		int32 BestVotes = -1;
		float BestLum = FLT_MAX;
		for (int32 a = 0; a < Border.Num(); ++a)
		{
			const uint8 AR = Orig[Border[a]].Ramp;
			const uint8 AI = Orig[Border[a]].Index;
			bool bCounted = false;
			for (int32 b = 0; b < a; ++b)
			{
				if (Orig[Border[b]].Ramp == AR && Orig[Border[b]].Index == AI)
				{
					bCounted = true;
					break;
				}
			}
			if (bCounted)
			{
				continue;
			}
			int32 VotesForA = 0;
			for (int32 b = 0; b < Border.Num(); ++b)
			{
				if (Orig[Border[b]].Ramp == AR && Orig[Border[b]].Index == AI)
				{
					++VotesForA;
				}
			}
			float Lum = FLT_MAX;
			if (AR < Settings.Ramps.Num() && AI < Settings.Ramps[AR].Num())
			{
				Lum = PixelLuminance(FLinearColor(Settings.Ramps[AR][AI]));
			}
			if (VotesForA > BestVotes || (VotesForA == BestVotes && Lum < BestLum))
			{
				BestVotes = VotesForA;
				BestLum = Lum;
				BestRamp = AR;
				BestIndex = AI;
			}
		}
		for (int32 At : Island)
		{
			Map.Pixels[At].Ramp = BestRamp;
			Map.Pixels[At].Index = BestIndex;
		}
	}
}

void ApplyFadeOut(TArray<FFrameMap>& Maps, const FSettings& Settings)
{
	using namespace GBPixelVfxConvertLocal;
	if (Settings.bLoop || Settings.FadeOutFrames <= 0 || Maps.Num() == 0)
	{
		return;
	}
	const int32 N = FMath::Min(Settings.FadeOutFrames, Maps.Num());
	const bool bErode = Settings.FadeOutMode.Equals(TEXT("Erode"), ESearchCase::IgnoreCase);
	static const uint8 Bayer[4][4] = { { 0, 8, 2, 10 }, { 12, 4, 14, 6 }, { 3, 11, 1, 9 }, { 15, 7, 13, 5 } };
	for (int32 k = 0; k < N; ++k)
	{
		const int32 f = Maps.Num() - N + k;
		FFrameMap& Map = Maps[f];
		if (Map.Width <= 0 || Map.Height <= 0 || Map.Pixels.Num() != Map.Width * Map.Height)
		{
			continue;
		}
		const float T = (static_cast<float>(k) + 1.f) / (static_cast<float>(N) + 1.f);
		if (!bErode)
		{
			for (int32 y = 0; y < Map.Height; ++y)
			{
				for (int32 x = 0; x < Map.Width; ++x)
				{
					const int32 At = y * Map.Width + x;
					if (!Map.Pixels[At].bSolid)
					{
						continue;
					}
					const float Gate = static_cast<float>(Bayer[y % 4][x % 4]) / 16.f;
					if (Gate < T)
					{
						Map.Pixels[At].bSolid = false;
					}
				}
			}
		}
		else
		{
			const int32 W = Map.Width;
			const int32 H = Map.Height;
			TArray<int32> Dist;
			Dist.SetNum(W * H);
			for (int32 i = 0; i < W * H; ++i)
			{
				Dist[i] = -1;
			}
			TArray<int32> Queue;
			Queue.Reserve(W * H);
			int32 QHead = 0;
			for (int32 i = 0; i < W * H; ++i)
			{
				if (!Map.Pixels[i].bSolid)
				{
					Dist[i] = 0;
					Queue.Add(i);
				}
			}
			// The frame edge counts as transparent: edge pixels are one step away.
			for (int32 y = 0; y < H; ++y)
			{
				for (int32 x = 0; x < W; ++x)
				{
					if (x != 0 && y != 0 && x != W - 1 && y != H - 1)
					{
						continue;
					}
					const int32 At = y * W + x;
					if (Map.Pixels[At].bSolid && Dist[At] < 0)
					{
						Dist[At] = 1;
						Queue.Add(At);
					}
				}
			}
			const int32 DXE[4] = { 1, -1, 0, 0 };
			const int32 DYE[4] = { 0, 0, 1, -1 };
			while (QHead < Queue.Num())
			{
				const int32 At = Queue[QHead++];
				const int32 X = At % W;
				const int32 Y = At / W;
				for (int32 d = 0; d < 4; ++d)
				{
					const int32 NX = X + DXE[d];
					const int32 NY = Y + DYE[d];
					if (!Inside(NX, NY, W, H))
					{
						continue;
					}
					const int32 NAt = NY * W + NX;
					if (Dist[NAt] < 0 && Map.Pixels[NAt].bSolid)
					{
						Dist[NAt] = Dist[At] + 1;
						Queue.Add(NAt);
					}
				}
			}
			int32 MaxDepth = 0;
			for (int32 i = 0; i < W * H; ++i)
			{
				if (Map.Pixels[i].bSolid)
				{
					MaxDepth = FMath::Max(MaxDepth, Dist[i] < 0 ? 1 : Dist[i]);
				}
			}
			if (MaxDepth <= 0)
			{
				continue;
			}
			const int32 KillDist = FMath::CeilToInt(T * static_cast<float>(MaxDepth));
			for (int32 i = 0; i < W * H; ++i)
			{
				if (Map.Pixels[i].bSolid && Dist[i] >= 0 && Dist[i] <= KillDist)
				{
					Map.Pixels[i].bSolid = false;
				}
			}
		}
	}
}

bool ResolveOutlineColor(const FSettings& Settings, FColor& OutColor)
{
	if (Settings.Outline.Equals(TEXT("Black"), ESearchCase::IgnoreCase))
	{
		OutColor = FColor(0, 0, 0, 255);
		return true;
	}
	if (Settings.Outline.Equals(TEXT("Darkest"), ESearchCase::IgnoreCase)
		&& Settings.Ramps.Num() > 0 && Settings.Ramps[0].Num() > 0)
	{
		OutColor = Settings.Ramps[0][0];
		OutColor.A = 255;
		return true;
	}
	return false;
}

void ResolveFrames(const TArray<FFrameMap>& Maps, const FSettings& Settings,
	TArray<TArray<FColor>>& OutPixels)
{
	using namespace GBPixelVfxConvertLocal;
	OutPixels.Reset(Maps.Num());
	FColor OutlineColor(0, 0, 0, 255);
	const bool bOutline = ResolveOutlineColor(Settings, OutlineColor);
	for (const FFrameMap& Map : Maps)
	{
		TArray<FColor> Pixels;
		Pixels.SetNum(Map.Pixels.Num());
		for (int32 i = 0; i < Map.Pixels.Num(); ++i)
		{
			const FMappedPixel& Pixel = Map.Pixels[i];
			if (Pixel.bSolid && Pixel.Ramp < Settings.Ramps.Num()
				&& Pixel.Index < Settings.Ramps[Pixel.Ramp].Num())
			{
				Pixels[i] = Settings.Ramps[Pixel.Ramp][Pixel.Index];
				Pixels[i].A = 255;
			}
			else
			{
				Pixels[i] = FColor(0, 0, 0, 0);
			}
		}
		if (bOutline && Map.Width > 0 && Map.Height > 0 && Map.Pixels.Num() == Map.Width * Map.Height)
		{
			const int32 W = Map.Width;
			const int32 H = Map.Height;
			const int32 DX[4] = { 1, -1, 0, 0 };
			const int32 DY[4] = { 0, 0, 1, -1 };
			for (int32 y = 0; y < H; ++y)
			{
				for (int32 x = 0; x < W; ++x)
				{
					const int32 At = y * W + x;
					if (Map.Pixels[At].bSolid)
					{
						continue;
					}
					bool bAdjacent = false;
					for (int32 d = 0; d < 4; ++d)
					{
						const int32 NX = x + DX[d];
						const int32 NY = y + DY[d];
						if (Inside(NX, NY, W, H) && Map.Pixels[NY * W + NX].bSolid)
						{
							bAdjacent = true;
							break;
						}
					}
					if (bAdjacent)
					{
						Pixels[At] = OutlineColor;
					}
				}
			}
		}
		OutPixels.Add(MoveTemp(Pixels));
	}
}

void ComputeLightTrack(const TArray<FFrameMap>& Maps, const FSettings& Settings,
	TArray<float>& OutTrack, float& OutAnchorU, float& OutAnchorV)
{
	OutTrack.Reset();
	OutAnchorU = 0.5f;
	OutAnchorV = 0.5f;
	if (!Settings.bLightEnabled || Maps.Num() == 0)
	{
		return;
	}
	OutTrack.SetNum(Maps.Num());
	float MaxE = 0.f;
	double AnchorUSum = 0.0;
	double AnchorVSum = 0.0;
	double AnchorWeight = 0.0;
	for (int32 f = 0; f < Maps.Num(); ++f)
	{
		const FFrameMap& Map = Maps[f];
		float E = 0.f;
		double FrameUSum = 0.0;
		double FrameVSum = 0.0;
		double FrameWeight = 0.0;
		for (int32 i = 0; i < Map.Pixels.Num(); ++i)
		{
			const FMappedPixel& Pixel = Map.Pixels[i];
			if (!Pixel.bSolid || !Pixel.bHot)
			{
				continue;
			}
			E += Pixel.Value;
			if (Map.Width > 0 && Map.Height > 0)
			{
				const float U = (static_cast<float>(i % Map.Width) + 0.5f) / static_cast<float>(Map.Width);
				const float V = (static_cast<float>(i / Map.Width) + 0.5f) / static_cast<float>(Map.Height);
				FrameUSum += U * Pixel.Value;
				FrameVSum += V * Pixel.Value;
				FrameWeight += Pixel.Value;
			}
		}
		OutTrack[f] = E;
		MaxE = FMath::Max(MaxE, E);
		if (FrameWeight > 0.0)
		{
			AnchorUSum += FrameUSum / FrameWeight;
			AnchorVSum += FrameVSum / FrameWeight;
			AnchorWeight += 1.0;
		}
	}
	if (MaxE > 0.f)
	{
		for (float& Entry : OutTrack)
		{
			Entry /= MaxE;
		}
	}
	else
	{
		for (float& Entry : OutTrack)
		{
			Entry = 0.f;
		}
	}
	if (AnchorWeight > 0.0)
	{
		OutAnchorU = static_cast<float>(AnchorUSum / AnchorWeight);
		OutAnchorV = static_cast<float>(AnchorVSum / AnchorWeight);
	}
}

int32 CountDistinctColors(const TArray<TArray<FColor>>& Frames)
{
	TSet<uint32> Seen;
	for (const TArray<FColor>& Pixels : Frames)
	{
		for (const FColor& Pixel : Pixels)
		{
			if (Pixel.A == 0)
			{
				continue;
			}
			Seen.Add((static_cast<uint32>(Pixel.R) << 16) | (static_cast<uint32>(Pixel.G) << 8) | Pixel.B);
		}
	}
	return Seen.Num();
}
}
