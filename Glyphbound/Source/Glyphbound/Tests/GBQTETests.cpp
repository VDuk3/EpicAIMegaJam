// Pure-logic tests of the spell QTE (GDD 7): gesture scoring and the session state machine.
#include "QTE/GBQTELogic.h"
#include "Misc/AutomationTest.h"
#include "Algo/Reverse.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GBQTETest
{
	FGBQTEConfig MakeConfig()
	{
		FGBQTEConfig Config;
		Config.SequenceLength = 4;
		Config.GestureCount = 1;
		Config.TimeLimit = 3.f;
		Config.GestureMinScore = 0.35f;
		Config.GestureMinLength = 120.f;
		Config.AllowedGestures = { EGBGesture::LineRight };
		return Config;
	}

	EGBQTEKey WrongKey(EGBQTEKey Key)
	{
		return static_cast<EGBQTEKey>((static_cast<uint8>(Key) + 1) % 4);
	}

	/** Circle of radius R sampled in Segments steps; with +Y down, increasing angle is clockwise on screen. */
	TArray<FVector2D> ClockwiseCircle(double Radius, int32 Segments)
	{
		TArray<FVector2D> Points;
		for (int32 i = 0; i <= Segments; ++i)
		{
			const double Angle = 2.0 * UE_DOUBLE_PI * i / Segments;
			Points.Add(FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius);
		}
		return Points;
	}

	void DrawRight(FGBQTESession& Session, double Sign)
	{
		Session.AddGestureDelta(FVector2D(100.0 * Sign, 0.0));
		Session.AddGestureDelta(FVector2D(100.0 * Sign, 0.0));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBQTEGestureLineTest, "Glyphbound.QTE.GestureLine", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGBQTEGestureLineTest::RunTest(const FString& Parameters)
{
	const TArray<FVector2D> Straight = { {0, 0}, {50, 0}, {100, 0}, {200, 0} };
	TestTrue(TEXT("Straight right stroke scores high for LineRight"), FGBGestureRecognizer::Score(Straight, EGBGesture::LineRight, 120.f) > 0.9f);
	TestEqual(TEXT("Opposite direction scores 0"), FGBGestureRecognizer::Score(Straight, EGBGesture::LineLeft, 120.f), 0.f);
	TestEqual(TEXT("Perpendicular direction scores 0"), FGBGestureRecognizer::Score(Straight, EGBGesture::LineDown, 120.f), 0.f);

	const TArray<FVector2D> Wobbly = { {0, 0}, {50, 40}, {100, -40}, {150, 40}, {200, 0} };
	const float WobblyScore = FGBGestureRecognizer::Score(Wobbly, EGBGesture::LineRight, 120.f);
	TestTrue(TEXT("Wobbly right stroke scores between 0 and a straight one"), WobblyScore > 0.f && WobblyScore < 0.9f);

	const TArray<FVector2D> Up = { {0, 0}, {0, -200} };
	TestTrue(TEXT("Stroke towards -Y is LineUp"), FGBGestureRecognizer::Score(Up, EGBGesture::LineUp, 120.f) > 0.9f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBQTEGestureCircleTest, "Glyphbound.QTE.GestureCircle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGBQTEGestureCircleTest::RunTest(const FString& Parameters)
{
	const TArray<FVector2D> Clockwise = GBQTETest::ClockwiseCircle(100.0, 32);
	TestTrue(TEXT("Turn of a clockwise circle is positive"), FGBGestureRecognizer::TotalTurnDegrees(Clockwise) > 300.f);
	TestTrue(TEXT("Clockwise circle scores high for CircleClockwise"), FGBGestureRecognizer::Score(Clockwise, EGBGesture::CircleClockwise, 120.f) > 0.8f);
	TestEqual(TEXT("Clockwise circle scores 0 for CircleCounterClockwise"), FGBGestureRecognizer::Score(Clockwise, EGBGesture::CircleCounterClockwise, 120.f), 0.f);

	TArray<FVector2D> CounterClockwise = Clockwise;
	Algo::Reverse(CounterClockwise);
	TestTrue(TEXT("Reversed circle scores high for CircleCounterClockwise"), FGBGestureRecognizer::Score(CounterClockwise, EGBGesture::CircleCounterClockwise, 120.f) > 0.8f);

	const TArray<FVector2D> Line = { {0, 0}, {300, 0} };
	TestEqual(TEXT("A straight line is not a circle"), FGBGestureRecognizer::Score(Line, EGBGesture::CircleClockwise, 120.f), 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBQTEGestureShortTest, "Glyphbound.QTE.GestureShort", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGBQTEGestureShortTest::RunTest(const FString& Parameters)
{
	const TArray<FVector2D> Short = { {0, 0}, {50, 0} };
	TestEqual(TEXT("A stroke shorter than MinLength scores 0"), FGBGestureRecognizer::Score(Short, EGBGesture::LineRight, 120.f), 0.f);
	TestTrue(TEXT("The same stroke counts once MinLength allows it"), FGBGestureRecognizer::Score(Short, EGBGesture::LineRight, 40.f) > 0.9f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBQTESessionScoreTest, "Glyphbound.QTE.SessionScore", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGBQTESessionScoreTest::RunTest(const FString& Parameters)
{
	FRandomStream Random(1234);

	{
		FGBQTESession Session;
		Session.Start(GBQTETest::MakeConfig(), Random);
		TestEqual(TEXT("Sequence has SequenceLength keys"), Session.Sequence.Num(), 4);
		TestTrue(TEXT("Starts in the Sequence phase"), Session.Phase == EGBQTEPhase::Sequence);
		for (EGBQTEKey Key : TArray<EGBQTEKey>(Session.Sequence))
		{
			TestTrue(TEXT("Right key is reported right"), Session.PressKey(Key));
		}
		TestTrue(TEXT("Gesture phase after the last key"), Session.Phase == EGBQTEPhase::Gesture);
		GBQTETest::DrawRight(Session, 1.0);
		TestTrue(TEXT("Stroke is ready to judge"), Session.IsGestureReadyToJudge());
		Session.SubmitGesture();
		TestTrue(TEXT("Finished after the last gesture"), Session.IsFinished());
		const FGBQTEResult Result = Session.GetResult();
		TestEqual(TEXT("All right keys: Power 1"), Result.Power, 1.f);
		TestTrue(TEXT("Good gesture: Reach high"), Result.Reach > 0.9f);
		TestTrue(TEXT("Success"), Result.bSuccess && Result.bUsedQTE && !Result.bTimedOut);
	}

	{
		FGBQTESession Session;
		Session.Start(GBQTETest::MakeConfig(), Random);
		const TArray<EGBQTEKey> Sequence = Session.Sequence;
		TestFalse(TEXT("Wrong key is reported wrong"), Session.PressKey(GBQTETest::WrongKey(Sequence[0])));
		for (int32 i = 1; i < Sequence.Num(); ++i)
		{
			Session.PressKey(Sequence[i]);
		}
		GBQTETest::DrawRight(Session, 1.0);
		Session.SubmitGesture();
		TestEqual(TEXT("One wrong key of four: Power 0.75"), Session.GetResult().Power, 0.75f, 1e-4f);
	}

	{
		FGBQTEConfig Config = GBQTETest::MakeConfig();
		Config.SequenceLength = 0;
		FGBQTESession Session;
		Session.Start(Config, Random);
		TestTrue(TEXT("No sequence: starts in the Gesture phase"), Session.Phase == EGBQTEPhase::Gesture);
		GBQTETest::DrawRight(Session, 1.0);
		Session.SubmitGesture();
		TestEqual(TEXT("No sequence: Power counts as 1"), Session.GetResult().Power, 1.f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBQTESessionAllMissedTest, "Glyphbound.QTE.SessionAllMissed", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGBQTESessionAllMissedTest::RunTest(const FString& Parameters)
{
	FRandomStream Random(99);
	FGBQTESession Session;
	Session.Start(GBQTETest::MakeConfig(), Random);
	for (EGBQTEKey Key : TArray<EGBQTEKey>(Session.Sequence))
	{
		Session.PressKey(GBQTETest::WrongKey(Key));
	}
	GBQTETest::DrawRight(Session, -1.0);
	TestEqual(TEXT("Opposite stroke scores 0"), Session.SubmitGesture(), 0.f);
	TestTrue(TEXT("Finished"), Session.IsFinished());
	const FGBQTEResult Result = Session.GetResult();
	TestEqual(TEXT("Power 0"), Result.Power, 0.f);
	TestEqual(TEXT("Reach 0"), Result.Reach, 0.f);
	TestFalse(TEXT("Every step missed: the spell fails"), Result.bSuccess);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBQTESessionTimeoutTest, "Glyphbound.QTE.SessionTimeout", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGBQTESessionTimeoutTest::RunTest(const FString& Parameters)
{
	FRandomStream Random(7);
	FGBQTESession Session;
	Session.Start(GBQTETest::MakeConfig(), Random);
	Session.PressKey(Session.Sequence[0]);

	Session.Advance(1.f);
	TestFalse(TEXT("Not finished before the limit"), Session.IsFinished());
	Session.Advance(2.5f);
	TestTrue(TEXT("Finished at the limit"), Session.IsFinished());
	TestTrue(TEXT("Timed out"), Session.bTimedOut);
	TestFalse(TEXT("Keys are ignored after the end"), Session.PressKey(Session.Sequence[1]));

	const FGBQTEResult Result = Session.GetResult();
	TestTrue(TEXT("Result says timed out"), Result.bTimedOut);
	TestEqual(TEXT("Unpressed keys count as missed: Power 0.25"), Result.Power, 0.25f, 1e-4f);
	TestEqual(TEXT("Undrawn gesture counts as missed: Reach 0"), Result.Reach, 0.f);
	TestTrue(TEXT("One right key is enough to cast"), Result.bSuccess);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
