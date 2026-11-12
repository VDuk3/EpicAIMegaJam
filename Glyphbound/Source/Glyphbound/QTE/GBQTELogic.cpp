#include "QTE/GBQTELogic.h"

namespace GBQTELogicPrivate
{
	// Segments shorter than this carry no direction (repeated points, sub-pixel jitter).
	constexpr double MinSegmentLength = 1e-3;

	constexpr int32 NumKeys = 4;
	const EGBGesture AllGestures[] = {
		EGBGesture::LineRight, EGBGesture::LineLeft, EGBGesture::LineDown, EGBGesture::LineUp,
		EGBGesture::CircleClockwise, EGBGesture::CircleCounterClockwise };

	bool IsCircle(EGBGesture Gesture)
	{
		return Gesture == EGBGesture::CircleClockwise || Gesture == EGBGesture::CircleCounterClockwise;
	}

	FVector2D LineDirection(EGBGesture Gesture)
	{
		switch (Gesture)
		{
		case EGBGesture::LineRight: return FVector2D(1.0, 0.0);
		case EGBGesture::LineLeft:  return FVector2D(-1.0, 0.0);
		case EGBGesture::LineDown:  return FVector2D(0.0, 1.0);
		case EGBGesture::LineUp:    return FVector2D(0.0, -1.0);
		default:                    return FVector2D::ZeroVector;
		}
	}
}

// ---------------------------------------------------------------------------------------------
// FGBGestureRecognizer

float FGBGestureRecognizer::PathLength(TConstArrayView<FVector2D> Points)
{
	double Length = 0.0;
	for (int32 i = 1; i < Points.Num(); ++i)
	{
		Length += FVector2D::Distance(Points[i - 1], Points[i]);
	}
	return static_cast<float>(Length);
}

float FGBGestureRecognizer::TotalTurnDegrees(TConstArrayView<FVector2D> Points)
{
	double TotalRadians = 0.0;
	bool bHasPrevious = false;
	FVector2D Previous = FVector2D::ZeroVector;
	for (int32 i = 1; i < Points.Num(); ++i)
	{
		const FVector2D Segment = Points[i] - Points[i - 1];
		if (Segment.Size() < GBQTELogicPrivate::MinSegmentLength)
		{
			continue;
		}
		if (bHasPrevious)
		{
			// With +Y down, a positive cross product is a clockwise turn as seen on screen.
			const double Cross = Previous.X * Segment.Y - Previous.Y * Segment.X;
			const double Dot = Previous.X * Segment.X + Previous.Y * Segment.Y;
			TotalRadians += FMath::Atan2(Cross, Dot);
		}
		Previous = Segment;
		bHasPrevious = true;
	}
	return static_cast<float>(FMath::RadiansToDegrees(TotalRadians));
}

float FGBGestureRecognizer::Score(TConstArrayView<FVector2D> Points, EGBGesture Gesture, float MinLength)
{
	if (Points.Num() < 2)
	{
		return 0.f;
	}
	const float Length = PathLength(Points);
	if (Length < MinLength || Length < GBQTELogicPrivate::MinSegmentLength)
	{
		return 0.f;
	}

	const FVector2D Start = Points[0];
	const FVector2D End = Points.Last();

	if (!GBQTELogicPrivate::IsCircle(Gesture))
	{
		const FVector2D Chord = End - Start;
		const double ChordLength = Chord.Size();
		if (ChordLength < GBQTELogicPrivate::MinSegmentLength)
		{
			return 0.f;
		}
		const double Straightness = FMath::Clamp(ChordLength / Length, 0.0, 1.0);
		const double DirectionMatch = FMath::Clamp(FVector2D::DotProduct(Chord / ChordLength, GBQTELogicPrivate::LineDirection(Gesture)), 0.0, 1.0);
		return static_cast<float>(Straightness * DirectionMatch);
	}

	const float Sign = Gesture == EGBGesture::CircleClockwise ? 1.f : -1.f;
	const float Turn = TotalTurnDegrees(Points) * Sign;
	const float TurnScore = FMath::Clamp(Turn / 360.f, 0.f, 1.f);
	if (TurnScore <= 0.f)
	{
		return 0.f;
	}

	// Closure: 1 when the stroke ends where it began, 0 when the gap is as large as the stroke's bounding box diagonal.
	FVector2D Min = Start;
	FVector2D Max = Start;
	for (const FVector2D& P : Points)
	{
		Min = FVector2D::Min(Min, P);
		Max = FVector2D::Max(Max, P);
	}
	const double Size = (Max - Min).Size();
	if (Size < GBQTELogicPrivate::MinSegmentLength)
	{
		return 0.f;
	}
	const double Gap = FVector2D::Distance(Start, End);
	const double Closure = FMath::Clamp(1.0 - Gap / Size, 0.0, 1.0);
	return static_cast<float>(TurnScore * Closure);
}

// ---------------------------------------------------------------------------------------------
// FGBQTESession

void FGBQTESession::Start(const FGBQTEConfig& InConfig, FRandomStream& Random)
{
	Config = InConfig;
	Config.SequenceLength = FMath::Max(0, Config.SequenceLength);
	Config.GestureCount = FMath::Max(0, Config.GestureCount);

	Sequence.Reset();
	SequenceResults.Reset();
	Gestures.Reset();
	GestureScores.Reset();
	CurrentStroke.Reset();
	TimeElapsed = 0.f;
	bTimedOut = false;

	for (int32 i = 0; i < Config.SequenceLength; ++i)
	{
		Sequence.Add(static_cast<EGBQTEKey>(Random.RandRange(0, GBQTELogicPrivate::NumKeys - 1)));
	}

	TArray<EGBGesture> Pool = Config.AllowedGestures;
	if (Pool.Num() == 0)
	{
		Pool.Append(GBQTELogicPrivate::AllGestures, UE_ARRAY_COUNT(GBQTELogicPrivate::AllGestures));
	}
	for (int32 i = 0; i < Config.GestureCount; ++i)
	{
		Gestures.Add(Pool[Random.RandRange(0, Pool.Num() - 1)]);
	}

	if (Sequence.Num() > 0)
	{
		Phase = EGBQTEPhase::Sequence;
	}
	else
	{
		Phase = Gestures.Num() > 0 ? EGBQTEPhase::Gesture : EGBQTEPhase::Finished;
	}
}

bool FGBQTESession::PressKey(EGBQTEKey Key)
{
	if (Phase != EGBQTEPhase::Sequence || !Sequence.IsValidIndex(SequenceResults.Num()))
	{
		return false;
	}
	const bool bRight = Sequence[SequenceResults.Num()] == Key;
	SequenceResults.Add(bRight);
	if (SequenceResults.Num() >= Sequence.Num())
	{
		CurrentStroke.Reset();
		Phase = Gestures.Num() > 0 ? EGBQTEPhase::Gesture : EGBQTEPhase::Finished;
	}
	return bRight;
}

void FGBQTESession::AddGestureDelta(const FVector2D& Delta)
{
	if (Phase != EGBQTEPhase::Gesture || Delta.IsNearlyZero())
	{
		return;
	}
	if (CurrentStroke.Num() == 0)
	{
		CurrentStroke.Add(FVector2D::ZeroVector);
	}
	CurrentStroke.Add(CurrentStroke.Last() + Delta);
}

float FGBQTESession::SubmitGesture()
{
	if (Phase != EGBQTEPhase::Gesture || !Gestures.IsValidIndex(GestureScores.Num()))
	{
		return 0.f;
	}
	float GestureScore = FGBGestureRecognizer::Score(CurrentStroke, Gestures[GestureScores.Num()], Config.GestureMinLength);
	if (GestureScore < Config.GestureMinScore)
	{
		GestureScore = 0.f;
	}
	GestureScores.Add(GestureScore);
	CurrentStroke.Reset();
	if (GestureScores.Num() >= Gestures.Num())
	{
		Phase = EGBQTEPhase::Finished;
	}
	return GestureScore;
}

bool FGBQTESession::IsGestureReadyToJudge() const
{
	return Phase == EGBQTEPhase::Gesture && CurrentStroke.Num() >= 2
		&& FGBGestureRecognizer::PathLength(CurrentStroke) >= Config.GestureMinLength;
}

void FGBQTESession::Advance(float RealDeltaSeconds)
{
	if (Phase != EGBQTEPhase::Sequence && Phase != EGBQTEPhase::Gesture)
	{
		return;
	}
	TimeElapsed += FMath::Max(0.f, RealDeltaSeconds);
	if (TimeElapsed >= Config.TimeLimit)
	{
		bTimedOut = true;
		Phase = EGBQTEPhase::Finished;
	}
}

FGBQTEResult FGBQTESession::GetResult() const
{
	FGBQTEResult Result;
	Result.bUsedQTE = true;
	Result.bTimedOut = bTimedOut;

	int32 RightKeys = 0;
	for (bool bRight : SequenceResults)
	{
		RightKeys += bRight ? 1 : 0;
	}

	float ScoreSum = 0.f;
	bool bAnyGesture = false;
	for (float GestureScore : GestureScores)
	{
		ScoreSum += GestureScore;
		bAnyGesture |= GestureScore > 0.f;
	}

	// A part with no steps does not weaken the spell (gesture-only or sequence-only setups).
	Result.Power = Sequence.Num() > 0 ? static_cast<float>(RightKeys) / Sequence.Num() : 1.f;
	Result.Reach = Gestures.Num() > 0 ? ScoreSum / Gestures.Num() : 1.f;

	// GDD 7: the spell fails only if every step is missed (no steps = nothing missed).
	const bool bNoSteps = Sequence.Num() == 0 && Gestures.Num() == 0;
	Result.bSuccess = bNoSteps || RightKeys > 0 || bAnyGesture;
	return Result;
}
