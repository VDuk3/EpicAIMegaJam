// Committed jump takeoff: horizontal velocity from the input held at takeoff (GDD 18).
// Ledge climb (mantle): reach limits, path and view pose weights.
#include "Player/GBPlayerCharacter.h"
#include "Player/GBMantle.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBPlayerJumpDirectionTest, "Glyphbound.Player.JumpDirection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGBPlayerJumpDirectionTest::RunTest(const FString& Parameters)
{
	auto CheckNear = [&](const TCHAR* What, double Actual, double Expected, double Tolerance)
	{
		TestTrue(What, FMath::Abs(Actual - Expected) <= Tolerance);
	};

	// No input: the jump keeps its drift (standing takeoff keeps zero, moving takeoff keeps the velocity).
	{
		const FVector2D Still = AGBPlayerCharacter::ComputeJumpHorizontalVelocity(FVector2D(300.0, -100.0), FVector2D::ZeroVector, 750.0, 0.6);
		CheckNear(TEXT("No input keeps X"), Still.X, 300.0, 1.e-3);
		CheckNear(TEXT("No input keeps Y"), Still.Y, -100.0, 1.e-3);
		const FVector2D Zero = AGBPlayerCharacter::ComputeJumpHorizontalVelocity(FVector2D::ZeroVector, FVector2D::ZeroVector, 750.0, 1.0);
		TestTrue(TEXT("No input from standstill stays still"), Zero.IsNearlyZero(1.e-3));
	}

	// Input with a slow current velocity: leaves in the input direction at MaxWalkSpeed * Mult.
	{
		const FVector2D Result = AGBPlayerCharacter::ComputeJumpHorizontalVelocity(FVector2D(100.0, 0.0), FVector2D(0.0, 2.0), 750.0, 0.6);
		CheckNear(TEXT("Slow takeoff reaches the minimum speed"), Result.Size(), 450.0, 1.e-2);
		CheckNear(TEXT("Slow takeoff takes the input X"), Result.X, 0.0, 1.e-2);
		TestTrue(TEXT("Slow takeoff takes the input Y"), Result.Y > 449.0);
	}

	// Input with a fast current velocity: keeps the speed, takes the input direction (also opposite to motion).
	{
		const FVector2D Fast = AGBPlayerCharacter::ComputeJumpHorizontalVelocity(FVector2D(800.0, 0.0), FVector2D(0.0, 1.0), 750.0, 0.6);
		CheckNear(TEXT("Fast takeoff keeps its speed"), Fast.Size(), 800.0, 1.e-2);
		CheckNear(TEXT("Fast takeoff takes the input X"), Fast.X, 0.0, 1.e-2);
		TestTrue(TEXT("Fast takeoff takes the input Y"), Fast.Y > 799.0);

		const FVector2D Opposite = AGBPlayerCharacter::ComputeJumpHorizontalVelocity(FVector2D(500.0, 0.0), FVector2D(-3.0, 0.0), 750.0, 0.6);
		CheckNear(TEXT("Opposite input keeps the speed"), Opposite.Size(), 500.0, 1.e-2);
		TestTrue(TEXT("Opposite input reverses X"), Opposite.X < -499.0);
		CheckNear(TEXT("Opposite input zeroes Y"), Opposite.Y, 0.0, 1.e-2);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBPlayerMantleReachTest, "Glyphbound.Player.MantleReach",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGBPlayerMantleReachTest::RunTest(const FString& Parameters)
{
	GBMantle::FMantleParams Params;
	TestTrue(TEXT("JumpRise(800, 1568) is one jump"), FMath::Abs(GBMantle::JumpRise(800.f, 1568.f) - 204.08f) < 0.05f);
	TestTrue(TEXT("JumpRise guards zero gravity"), GBMantle::JumpRise(800.f, 0.f) == 0.f);
	TestTrue(TEXT("JumpRise guards negative gravity"), GBMantle::JumpRise(800.f, -9.8f) == 0.f);

	const float JumpRiseCm = GBMantle::JumpRise(800.f, 1568.f);
	const float GroundMax = JumpRiseCm + Params.MaxReach;
	TestTrue(TEXT("Ground: MinHeight climbs"), GBMantle::CanMantleFromGround(Params.MinHeight, JumpRiseCm, Params));
	TestTrue(TEXT("Ground: JumpRise + MaxReach climbs"), GBMantle::CanMantleFromGround(GroundMax, JumpRiseCm, Params));
	TestTrue(TEXT("Ground: below MinHeight does not climb"), !GBMantle::CanMantleFromGround(Params.MinHeight - 0.1f, JumpRiseCm, Params));
	TestTrue(TEXT("Ground: above JumpRise + MaxReach does not climb"), !GBMantle::CanMantleFromGround(GroundMax + 0.1f, JumpRiseCm, Params));

	TestTrue(TEXT("Air: AirMinHeight climbs"), GBMantle::CanMantleFromAir(Params.AirMinHeight, Params));
	TestTrue(TEXT("Air: MaxReach climbs"), GBMantle::CanMantleFromAir(Params.MaxReach, Params));
	TestTrue(TEXT("Air: below AirMinHeight does not climb"), !GBMantle::CanMantleFromAir(Params.AirMinHeight - 0.1f, Params));
	TestTrue(TEXT("Air: above MaxReach does not climb"), !GBMantle::CanMantleFromAir(Params.MaxReach + 0.1f, Params));

	// Other numbers: the limits follow the parameters and the jump, not fixed values.
	TestTrue(TEXT("JumpRise(1000, 980) follows v^2/2g"), FMath::Abs(GBMantle::JumpRise(1000.f, 980.f) - 510.20f) < 0.05f);
	GBMantle::FMantleParams Other;
	Other.MaxReach = 90.f;
	Other.MinHeight = 120.f;
	Other.AirMinHeight = 40.f;
	TestTrue(TEXT("Other: ground 119 below MinHeight"), !GBMantle::CanMantleFromGround(119.f, 300.f, Other));
	TestTrue(TEXT("Other: ground 390 = rise 300 + reach 90 climbs"), GBMantle::CanMantleFromGround(390.f, 300.f, Other));
	TestTrue(TEXT("Other: ground 391 too high"), !GBMantle::CanMantleFromGround(391.f, 300.f, Other));
	TestTrue(TEXT("Other: air 39 below AirMinHeight"), !GBMantle::CanMantleFromAir(39.f, Other));
	TestTrue(TEXT("Other: air 90 climbs"), GBMantle::CanMantleFromAir(90.f, Other));
	TestTrue(TEXT("Other: air 91 too high"), !GBMantle::CanMantleFromAir(91.f, Other));
	TestTrue(TEXT("Duration grows with the rise"), GBMantle::Duration(200.f, Params) > GBMantle::Duration(50.f, Params) + 0.1f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBPlayerMantlePathTest, "Glyphbound.Player.MantlePath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGBPlayerMantlePathTest::RunTest(const FString& Parameters)
{
	auto CheckNear = [&](const TCHAR* What, double Actual, double Expected, double Tolerance)
	{
		TestTrue(What, FMath::Abs(Actual - Expected) <= Tolerance);
	};

	const FVector Start(10.f, 20.f, 30.f);
	const FVector End(110.f, 220.f, 180.f);
	constexpr float Share = 0.6f;

	const FVector AtZero = GBMantle::EvaluatePath(Start, End, 0.f, Share);
	const FVector AtOne = GBMantle::EvaluatePath(Start, End, 1.f, Share);
	CheckNear(TEXT("Path starts exactly at Start X"), AtZero.X, Start.X, 1.e-4);
	CheckNear(TEXT("Path starts exactly at Start Y"), AtZero.Y, Start.Y, 1.e-4);
	CheckNear(TEXT("Path starts exactly at Start Z"), AtZero.Z, Start.Z, 1.e-4);
	CheckNear(TEXT("Path ends exactly at End X"), AtOne.X, End.X, 1.e-4);
	CheckNear(TEXT("Path ends exactly at End Y"), AtOne.Y, End.Y, 1.e-4);
	CheckNear(TEXT("Path ends exactly at End Z"), AtOne.Z, End.Z, 1.e-4);

	const FVector AtShare = GBMantle::EvaluatePath(Start, End, Share, Share);
	CheckNear(TEXT("RiseShare: XY still at Start X"), AtShare.X, Start.X, 1.e-3);
	CheckNear(TEXT("RiseShare: XY still at Start Y"), AtShare.Y, Start.Y, 1.e-3);
	CheckNear(TEXT("RiseShare: Z already at End Z"), AtShare.Z, End.Z, 1.e-3);

	float PrevZ = Start.Z;
	bool bXYMovedEarly = false;
	for (int32 i = 0; i <= 50; ++i)
	{
		const float Alpha = i / 50.f;
		const FVector At = GBMantle::EvaluatePath(Start, End, Alpha, Share);
		TestTrue(TEXT("Path Z never decreases"), At.Z >= PrevZ - 1.e-3f);
		PrevZ = At.Z;
		if (Alpha < Share - 1.e-4f
			&& (FMath::Abs(At.X - Start.X) > 1.e-3f || FMath::Abs(At.Y - Start.Y) > 1.e-3f))
		{
			bXYMovedEarly = true;
		}
	}
	TestTrue(TEXT("Path XY does not move before RiseShare"), !bXYMovedEarly);

	// Progress is continuous: part way up the rise is strictly between Start and End, part way across is strictly between in XY.
	const FVector Rising = GBMantle::EvaluatePath(Start, End, Share * 0.5f, Share);
	TestTrue(TEXT("Half the rise is above Start"), Rising.Z > Start.Z + 1.f);
	TestTrue(TEXT("Half the rise is below End"), Rising.Z < End.Z - 1.f);
	const FVector Crossing = GBMantle::EvaluatePath(Start, End, (Share + 1.f) * 0.5f, Share);
	TestTrue(TEXT("Half way across has left Start X"), Crossing.X > Start.X + 1.f);
	TestTrue(TEXT("Half way across is short of End X"), Crossing.X < End.X - 1.f);
	CheckNear(TEXT("Half way across stays at End Z"), Crossing.Z, End.Z, 1.e-3);
	// Another rise share moves the corner.
	const FVector OtherCorner = GBMantle::EvaluatePath(Start, End, 0.4f, 0.4f);
	CheckNear(TEXT("RiseShare 0.4: corner X"), OtherCorner.X, Start.X, 1.e-3);
	CheckNear(TEXT("RiseShare 0.4: corner Z"), OtherCorner.Z, End.Z, 1.e-3);

	const GBMantle::FMantlePose PoseZero = GBMantle::EvaluatePose(0.f, Share);
	CheckNear(TEXT("Pose ItemLower01 is 0 at 0"), PoseZero.ItemLower01, 0.0, 1.e-5);
	CheckNear(TEXT("Pose Grip01 is 0 at 0"), PoseZero.Grip01, 0.0, 1.e-5);
	CheckNear(TEXT("Pose CameraDip01 is 0 at 0"), PoseZero.CameraDip01, 0.0, 1.e-5);
	const GBMantle::FMantlePose PoseOne = GBMantle::EvaluatePose(1.f, Share);
	CheckNear(TEXT("Pose ItemLower01 is 0 at 1"), PoseOne.ItemLower01, 0.0, 1.e-5);
	CheckNear(TEXT("Pose Grip01 is 0 at 1"), PoseOne.Grip01, 0.0, 1.e-5);
	CheckNear(TEXT("Pose CameraDip01 is 0 at 1"), PoseOne.CameraDip01, 0.0, 1.e-5);

	const GBMantle::FMantlePose PoseMid = GBMantle::EvaluatePose(0.5f, Share);
	CheckNear(TEXT("Pose ItemLower01 is 1 at 0.5"), PoseMid.ItemLower01, 1.0, 1.e-4);
	CheckNear(TEXT("Pose Grip01 is 1 at 0.4 (hands on the ledge)"), GBMantle::EvaluatePose(0.4f, Share).Grip01, 1.0, 1.e-4);
	TestTrue(TEXT("Pose Grip01 rises over the first 15%"), GBMantle::EvaluatePose(0.07f, Share).Grip01 > 0.2f && GBMantle::EvaluatePose(0.07f, Share).Grip01 < 0.8f);
	CheckNear(TEXT("Pose Grip01 is 0 after 90%"), GBMantle::EvaluatePose(0.95f, Share).Grip01, 0.0, 1.e-4);
	CheckNear(TEXT("Pose ItemLower01 still 1 at 0.7"), GBMantle::EvaluatePose(0.7f, Share).ItemLower01, 1.0, 1.e-4);

	float BestAlpha = 0.f;
	float BestDip = -1.f;
	for (int32 i = 0; i <= 100; ++i)
	{
		const float Alpha = i / 100.f;
		const float Dip = GBMantle::EvaluatePose(Alpha, Share).CameraDip01;
		if (Dip > BestDip)
		{
			BestDip = Dip;
			BestAlpha = Alpha;
		}
	}
	TestTrue(TEXT("Pose CameraDip01 peaks at RiseShare"), FMath::Abs(BestAlpha - Share) <= 0.05f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBPlayerMantleProbeTest, "Glyphbound.Player.MantleProbe",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGBPlayerMantleProbeTest::RunTest(const FString& Parameters)
{
	auto CheckNear = [&](const TCHAR* What, double Actual, double Expected, double Tolerance)
	{
		TestTrue(What, FMath::Abs(Actual - Expected) <= Tolerance);
	};
	auto AngleDeg = [](const FVector2D& A, const FVector2D& B)
	{
		return FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector2D::DotProduct(A.GetSafeNormal(), B.GetSafeNormal()), -1.f, 1.f)));
	};

	using FProbeList = TArray<FVector2D, TInlineAllocator<GBMantle::MaxProbeDirections>>;

	// Intent: exactly one direction, the normalized intent (also when it is not normalized).
	{
		FProbeList WithIntent;
		GBMantle::GetProbeDirections(FVector2D(1.f, 0.f), FVector2D(0.f, 1.f), WithIntent);
		TestTrue(TEXT("Intent gives exactly one direction"), WithIntent.Num() == 1);
		CheckNear(TEXT("Intent direction is the intent X"), WithIntent[0].X, 0.0, 1.e-4);
		CheckNear(TEXT("Intent direction is the intent Y"), WithIntent[0].Y, 1.0, 1.e-4);

		FProbeList LongIntent;
		GBMantle::GetProbeDirections(FVector2D(1.f, 0.f), FVector2D(0.f, 5.f), LongIntent);
		TestTrue(TEXT("Non-normalized intent gives exactly one direction"), LongIntent.Num() == 1);
		CheckNear(TEXT("Non-normalized intent is normalized X"), LongIntent[0].X, 0.0, 1.e-4);
		CheckNear(TEXT("Non-normalized intent is normalized Y"), LongIntent[0].Y, 1.0, 1.e-4);

		FProbeList AwayIntent;
		GBMantle::GetProbeDirections(FVector2D(0.f, -1.f), FVector2D(-2.f, 0.f), AwayIntent);
		TestTrue(TEXT("Off-view intent gives exactly one direction"), AwayIntent.Num() == 1);
		CheckNear(TEXT("Off-view intent is normalized X"), AwayIntent[0].X, -1.0, 1.e-4);
		CheckNear(TEXT("Off-view intent is normalized Y"), AwayIntent[0].Y, 0.0, 1.e-4);
	}

	// No intent: 8 unit directions starting with the view, ending behind it, 45 degrees apart.
	{
		FProbeList All;
		GBMantle::GetProbeDirections(FVector2D(1.f, 0.f), FVector2D::ZeroVector, All);
		TestTrue(TEXT("No intent gives 8 directions"), All.Num() == 8);
		for (int32 i = 0; i < All.Num(); ++i)
		{
			CheckNear(TEXT("Each probe direction is unit length"), All[i].Size(), 1.0, 1.e-4);
		}
		CheckNear(TEXT("First probe is the view forward X"), All[0].X, 1.0, 1.e-4);
		CheckNear(TEXT("First probe is the view forward Y"), All[0].Y, 0.0, 1.e-4);
		CheckNear(TEXT("Last probe is behind the view X"), All[7].X, -1.0, 1.e-4);
		CheckNear(TEXT("Last probe is behind the view Y"), All[7].Y, 0.0, 1.e-4);
		const float Cos45 = FMath::Cos(FMath::DegreesToRadians(45.f));
		const float Sin45 = FMath::Sin(FMath::DegreesToRadians(45.f));
		CheckNear(TEXT("Second probe is +45 X"), All[1].X, Cos45, 1.e-4);
		CheckNear(TEXT("Second probe is +45 Y"), All[1].Y, Sin45, 1.e-4);
		CheckNear(TEXT("Third probe is -45 X"), All[2].X, Cos45, 1.e-4);
		CheckNear(TEXT("Third probe is -45 Y"), All[2].Y, -Sin45, 1.e-4);
		for (int32 i = 0; i < All.Num(); ++i)
		{
			float Nearest = 180.f;
			for (int32 j = 0; j < All.Num(); ++j)
			{
				if (i != j)
				{
					Nearest = FMath::Min(Nearest, AngleDeg(All[i], All[j]));
				}
			}
			CheckNear(TEXT("Each probe's nearest neighbour is 45 degrees away"), Nearest, 45.0, 1.0);
			for (int32 j = i + 1; j < All.Num(); ++j)
			{
				TestTrue(TEXT("All 8 probe directions are distinct"), AngleDeg(All[i], All[j]) > 1.0);
			}
		}
	}

	// A different view forward rotates the whole list (not fixed world axes).
	{
		FProbeList East;
		GBMantle::GetProbeDirections(FVector2D(1.f, 0.f), FVector2D::ZeroVector, East);
		FProbeList North;
		GBMantle::GetProbeDirections(FVector2D(0.f, 1.f), FVector2D::ZeroVector, North);
		TestTrue(TEXT("Rotated view still gives 8 directions"), North.Num() == 8);
		for (int32 i = 0; i < FMath::Min(East.Num(), North.Num()); ++i)
		{
			// Counterclockwise quarter turn: (x, y) -> (-y, x).
			const FVector2D Rotated(-East[i].Y, East[i].X);
			CheckNear(TEXT("Rotated view rotates the list X"), North[i].X, Rotated.X, 1.e-3);
			CheckNear(TEXT("Rotated view rotates the list Y"), North[i].Y, Rotated.Y, 1.e-3);
		}
		CheckNear(TEXT("North-first probe is the north view X"), North[0].X, 0.0, 1.e-4);
		CheckNear(TEXT("North-first probe is the north view Y"), North[0].Y, 1.0, 1.e-4);
		// The whole priority order: nearest to the view first, counterclockwise before clockwise.
		const float OrderDeg[8] = { 0.f, 45.f, -45.f, 90.f, -90.f, 135.f, -135.f, 180.f };
		for (int32 i = 0; i < FMath::Min(8, East.Num()); ++i)
		{
			const float Rad = FMath::DegreesToRadians(OrderDeg[i]);
			CheckNear(TEXT("Probe order X (0, +45, -45, +90, -90, +135, -135, 180)"), East[i].X, FMath::Cos(Rad), 1.e-4);
			CheckNear(TEXT("Probe order Y (0, +45, -45, +90, -90, +135, -135, 180)"), East[i].Y, FMath::Sin(Rad), 1.e-4);
		}
	}

	// TurnFraction: smoothstep from 0 at Alpha 0 to 1 at Alpha = RiseShare, 1 after.
	{
		constexpr float Share = 0.6f;
		CheckNear(TEXT("TurnFraction is 0 at 0"), GBMantle::TurnFraction(0.f, Share), 0.0, 1.e-6);
		CheckNear(TEXT("TurnFraction is 1 at RiseShare"), GBMantle::TurnFraction(Share, Share), 1.0, 1.e-6);
		CheckNear(TEXT("TurnFraction is 1 at 1"), GBMantle::TurnFraction(1.f, Share), 1.0, 1.e-6);
		const float Half = GBMantle::TurnFraction(Share * 0.5f, Share);
		TestTrue(TEXT("TurnFraction is strictly between at half the rise"), Half > 0.01f && Half < 0.99f);
		// Smoothstep, not linear: eases in and out (3t^2 - 2t^3).
		CheckNear(TEXT("TurnFraction at a quarter of the rise is smoothstep"), GBMantle::TurnFraction(Share * 0.25f, Share), 0.15625, 1.e-4);
		CheckNear(TEXT("TurnFraction at three quarters of the rise is smoothstep"), GBMantle::TurnFraction(Share * 0.75f, Share), 0.84375, 1.e-4);
		float Prev = -1.f;
		for (int32 i = 0; i <= 50; ++i)
		{
			const float Value = GBMantle::TurnFraction(i / 50.f, Share);
			TestTrue(TEXT("TurnFraction never decreases"), Value >= Prev - 1.e-6f);
			Prev = Value;
		}
		const float AtPointThreeA = GBMantle::TurnFraction(0.3f, 0.6f);
		const float AtPointThreeB = GBMantle::TurnFraction(0.3f, 0.8f);
		TestTrue(TEXT("TurnFraction at 0.3 moves with RiseShare"), FMath::Abs(AtPointThreeA - AtPointThreeB) > 0.05f);
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
