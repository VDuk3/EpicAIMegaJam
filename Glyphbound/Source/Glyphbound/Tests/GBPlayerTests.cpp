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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBPlayerLadderSwayTest, "Glyphbound.Player.LadderSway",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGBPlayerLadderSwayTest::RunTest(const FString& Parameters)
{
	// Still on every rung, leaning one way and then the other between them, never more than the full lean.
	for (int32 Rung = 0; Rung <= 4; ++Rung)
	{
		TestTrue(TEXT("No lean or dip on a rung"), GBMantle::LadderSway(30.f * Rung, 30.f).IsNearlyZero(0.001f));
	}
	const FVector2D First = GBMantle::LadderSway(15.f, 30.f);
	const FVector2D Second = GBMantle::LadderSway(45.f, 30.f);
	TestTrue(TEXT("Full lean and dip half way between rungs"), FMath::IsNearlyEqual(FMath::Abs(First.X), 1.f, 0.001f) && FMath::IsNearlyEqual(First.Y, 1.f, 0.001f));
	TestTrue(TEXT("The next rung leans the other way"), First.X * Second.X < 0.f);
	TestTrue(TEXT("The dip is never upwards"), Second.Y >= 0.f && GBMantle::LadderSway(52.f, 30.f).Y >= 0.f);
	TestTrue(TEXT("Going down a ladder sways too"), !GBMantle::LadderSway(-15.f, 30.f).IsNearlyZero(0.001f));
	TestTrue(TEXT("A ladder without rungs does not sway"), GBMantle::LadderSway(15.f, 0.f).IsNearlyZero());
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

// ---- Vault over a thin obstacle ----

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBPlayerVaultQualifyTest, "Glyphbound.Player.VaultQualify",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGBPlayerVaultQualifyTest::RunTest(const FString& Parameters)
{
	GBMantle::FVaultParams P;
	P.MinHeight = 50.f;
	P.MaxHeight = 150.f;
	P.MaxDepth = 60.f;
	P.Reach = 220.f;
	P.MaxDrop = 450.f;
	P.MinTopAboveLanding = 10.f;
	// A baseline that qualifies; each check below moves one number across its limit.
	TestTrue(TEXT("Baseline qualifies"), GBMantle::CanVault(127.f, 10.f, 150.f, 0.f, 127.f, P));
	TestTrue(TEXT("Height: MinHeight qualifies"), GBMantle::CanVault(50.f, 10.f, 150.f, 0.f, 50.f, P));
	TestTrue(TEXT("Height: below MinHeight does not"), !GBMantle::CanVault(49.9f, 10.f, 150.f, 0.f, 49.9f, P));
	TestTrue(TEXT("Height: MaxHeight qualifies"), GBMantle::CanVault(150.f, 10.f, 150.f, 0.f, 150.f, P));
	TestTrue(TEXT("Height: above MaxHeight does not (a 200 cm wall)"), !GBMantle::CanVault(200.f, 10.f, 150.f, 0.f, 200.f, P));
	TestTrue(TEXT("Depth: MaxDepth qualifies"), GBMantle::CanVault(127.f, 60.f, 150.f, 0.f, 127.f, P));
	TestTrue(TEXT("Depth: past MaxDepth does not (a thick wall is a ledge)"), !GBMantle::CanVault(127.f, 60.1f, 150.f, 0.f, 127.f, P));
	TestTrue(TEXT("Reach: Reach qualifies"), GBMantle::CanVault(127.f, 10.f, 220.f, 0.f, 127.f, P));
	TestTrue(TEXT("Reach: past Reach does not"), !GBMantle::CanVault(127.f, 10.f, 220.1f, 0.f, 127.f, P));
	TestTrue(TEXT("Drop: MaxDrop qualifies"), GBMantle::CanVault(127.f, 10.f, 150.f, 450.f, 577.f, P));
	TestTrue(TEXT("Drop: past MaxDrop does not (no floor close enough)"), !GBMantle::CanVault(127.f, 10.f, 150.f, 450.1f, 577.1f, P));
	TestTrue(TEXT("Drop: a floor above the feet is fine while it is below the top"), GBMantle::CanVault(127.f, 10.f, 150.f, -30.f, 97.f, P));
	TestTrue(TEXT("Landing: top 10 above the landing floor qualifies"), GBMantle::CanVault(127.f, 10.f, 150.f, 117.f, 10.f, P));
	TestTrue(TEXT("Landing: top less than 10 above the landing floor does not (that is a ledge)"), !GBMantle::CanVault(127.f, 10.f, 150.f, 118.f, 9.f, P));
	// The limits follow the parameters, not fixed numbers.
	GBMantle::FVaultParams Other;
	Other.MinHeight = 80.f;
	Other.MaxHeight = 100.f;
	Other.MaxDepth = 20.f;
	Other.Reach = 100.f;
	Other.MaxDrop = 30.f;
	TestTrue(TEXT("Other: 90 high, 20 deep, reach 100, drop 30 qualifies"), GBMantle::CanVault(90.f, 20.f, 100.f, 30.f, 90.f, Other));
	TestTrue(TEXT("Other: 79 high does not"), !GBMantle::CanVault(79.f, 20.f, 100.f, 30.f, 79.f, Other));
	TestTrue(TEXT("Other: 101 high does not"), !GBMantle::CanVault(101.f, 20.f, 100.f, 30.f, 101.f, Other));
	TestTrue(TEXT("Other: 21 deep does not"), !GBMantle::CanVault(90.f, 21.f, 100.f, 30.f, 90.f, Other));
	TestTrue(TEXT("Other: drop 31 does not"), !GBMantle::CanVault(90.f, 20.f, 100.f, 31.f, 90.f, Other));
	TestTrue(TEXT("Other: reach 101 does not"), !GBMantle::CanVault(90.f, 20.f, 101.f, 30.f, 90.f, Other));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBPlayerVaultEndFeetTest, "Glyphbound.Player.VaultEndFeet",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGBPlayerVaultEndFeetTest::RunTest(const FString& Parameters)
{
	// The far floor is the same as the near one (0), the top 120 above: he ends on the floor.
	TestTrue(TEXT("Same floor: ends on the far floor"), FMath::IsNearlyEqual(GBMantle::VaultEndFeetZ(0.f, 120.f, 130.f), 0.f, 1.e-4f));
	// A pit 250 below a platform whose top is 370: he ends 130 under the top (240) and falls the rest.
	TestTrue(TEXT("Deep far floor: ends under the top and falls"), FMath::IsNearlyEqual(GBMantle::VaultEndFeetZ(0.f, 370.f, 130.f), 240.f, 1.e-4f));
	// A far floor higher than that is where he ends.
	TestTrue(TEXT("Far floor within reach: ends on it"), FMath::IsNearlyEqual(GBMantle::VaultEndFeetZ(300.f, 370.f, 130.f), 300.f, 1.e-4f));
	TestTrue(TEXT("ExitBelowTop is a parameter"), FMath::IsNearlyEqual(GBMantle::VaultEndFeetZ(0.f, 370.f, 50.f), 320.f, 1.e-4f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBPlayerVaultPathTest, "Glyphbound.Player.VaultPath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGBPlayerVaultPathTest::RunTest(const FString& Parameters)
{
	const FVector Start(100.f, 200.f, 90.f);
	const FVector End(100.f, 300.f, 20.f);
	constexpr float Apex = 224.f;
	constexpr float Rise = 0.3f;
	constexpr float Drop = 0.62f;
	auto At = [&](float Alpha) { return GBMantle::EvaluateVaultPath(Start, End, Apex, Alpha, Rise, Drop); };

	TestTrue(TEXT("Alpha 0 is exactly Start"), At(0.f).Equals(Start, 1.e-4f));
	TestTrue(TEXT("Alpha 1 is exactly End"), At(1.f).Equals(End, 1.e-4f));
	TestTrue(TEXT("Alpha is clamped below"), At(-1.f).Equals(Start, 1.e-4f));
	TestTrue(TEXT("Alpha is clamped above"), At(2.f).Equals(End, 1.e-4f));
	TestTrue(TEXT("The rise ends at the apex height"), FMath::IsNearlyEqual(At(Rise).Z, Apex, 0.01f));
	TestTrue(TEXT("The drop starts at the apex height"), FMath::IsNearlyEqual(At(Drop).Z, Apex, 0.01f));
	TestTrue(TEXT("Over the top the body holds the apex"), FMath::IsNearlyEqual(At(0.46f).Z, Apex, 0.01f));

	float PrevZ = Start.Z;
	bool bRising = true;
	for (int32 i = 0; i <= 100; ++i)
	{
		const float Alpha = i / 100.f;
		const FVector P = At(Alpha);
		TestTrue(TEXT("Never above the apex"), P.Z <= Apex + 0.01f);
		if (bRising && P.Z < PrevZ - 1.e-3f)
		{
			bRising = false;
		}
		TestTrue(TEXT("Z rises, holds, then only falls (never rises again)"), bRising || P.Z <= PrevZ + 1.e-3f);
		PrevZ = P.Z;
		// The core rule: the body crosses over the obstacle only at the apex (a railing is passed over, not through).
		const float Crossed = static_cast<float>(FMath::Abs(P.Y - Start.Y) / FMath::Abs(End.Y - Start.Y));
		if (Crossed > 0.02f && Crossed < 0.98f)
		{
			TestTrue(TEXT("Moving across only at the apex height"), FMath::IsNearlyEqual(P.Z, Apex, 0.01f));
		}
		if (Alpha < Rise - 1.e-4f)
		{
			TestTrue(TEXT("No sideways motion during the rise"), FMath::IsNearlyEqual(P.Y, Start.Y, 1.e-3f));
		}
		if (Alpha > Drop + 1.e-4f)
		{
			TestTrue(TEXT("No sideways motion during the drop"), FMath::IsNearlyEqual(P.Y, End.Y, 1.e-3f));
		}
	}
	// An apex below the start or the end never digs the path under them.
	const FVector Low = GBMantle::EvaluateVaultPath(Start, End, 10.f, 0.46f, Rise, Drop);
	TestTrue(TEXT("A too low apex is lifted to the higher of Start and End"), FMath::IsNearlyEqual(Low.Z, Start.Z, 0.01f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBPlayerVaultStepsTest, "Glyphbound.Player.VaultSteps",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGBPlayerVaultStepsTest::RunTest(const FString& Parameters)
{
	using namespace GBMantle;
	TestEqual(TEXT("No time passed is one step"), VaultSubsteps(0.f, 0.03f), 1);
	TestEqual(TEXT("Exactly one step's worth is one step"), VaultSubsteps(0.03f, 0.03f), 1);
	TestEqual(TEXT("A little over one step's worth is two"), VaultSubsteps(0.0301f, 0.03f), 2);
	TestEqual(TEXT("0.9 of the move is 30 steps"), VaultSubsteps(0.9f, 0.03f), 30);
	TestEqual(TEXT("The sign does not matter"), VaultSubsteps(-0.9f, 0.03f), 30);
	TestEqual(TEXT("A zero step size falls back to one step"), VaultSubsteps(0.5f, 0.f), 1);

	// A 1 s hitch (the whole vault in one frame): cut into steps, no step of the path is longer than the capsule is wide.
	const FVector Start(100.f, 200.f, 90.f);
	const FVector End(100.f, 300.f, 20.f);
	auto LongestSegment = [&](int32 Steps)
	{
		float Longest = 0.f;
		FVector Prev = Start;
		for (int32 i = 1; i <= Steps; ++i)
		{
			const FVector P = EvaluateVaultPath(Start, End, 224.f, static_cast<float>(i) / Steps, 0.3f, 0.62f);
			Longest = FMath::Max(Longest, static_cast<float>(FVector::Dist(P, Prev)));
			Prev = P;
		}
		return Longest;
	};
	TestTrue(TEXT("In steps of VaultMaxAlphaStep no segment is longer than 40 cm"), LongestSegment(VaultSubsteps(1.f, VaultMaxAlphaStep)) <= 40.f);
	TestTrue(TEXT("In one step the segment would jump across the obstacle (over 100 cm)"), LongestSegment(1) > 100.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBPlayerVaultPoseTest, "Glyphbound.Player.VaultPose",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGBPlayerVaultPoseTest::RunTest(const FString& Parameters)
{
	constexpr float Rise = 0.3f;
	constexpr float Drop = 0.62f;
	for (const float Edge : { 0.f, 1.f })
	{
		const GBMantle::FVaultPose Pose = GBMantle::EvaluateVaultPose(Edge, Rise, Drop);
		TestTrue(TEXT("Every weight is exactly 0 at the ends"),
			Pose.ItemLower01 == 0.f && Pose.Grip01 == 0.f && Pose.PitchDown01 == 0.f && Pose.Roll01 == 0.f && Pose.Dip01 == 0.f);
	}
	const GBMantle::FVaultPose Top = GBMantle::EvaluateVaultPose(0.46f, Rise, Drop);
	TestTrue(TEXT("Over the top: the item is lowered"), Top.ItemLower01 > 0.99f);
	TestTrue(TEXT("Over the top: the hand is on the obstacle"), Top.Grip01 > 0.99f);
	TestTrue(TEXT("Over the top: the view is tipped forward"), Top.PitchDown01 > 0.99f);
	TestTrue(TEXT("Over the top: the view is rolled"), Top.Roll01 > 0.99f);
	TestTrue(TEXT("Over the top: the camera dips"), Top.Dip01 > 0.99f);
	// On the way up the view has tipped forward but not rolled yet; on the way down it has settled.
	const GBMantle::FVaultPose Early = GBMantle::EvaluateVaultPose(0.12f, Rise, Drop);
	TestTrue(TEXT("On the way up: tipping forward has begun"), Early.PitchDown01 > 0.1f && Early.PitchDown01 < 0.9f);
	TestTrue(TEXT("On the way up: the roll is behind the tip"), Early.Roll01 < Early.PitchDown01);
	const GBMantle::FVaultPose Late = GBMantle::EvaluateVaultPose(0.92f, Rise, Drop);
	TestTrue(TEXT("On the way down: the tip has mostly settled"), Late.PitchDown01 < 0.2f);
	TestTrue(TEXT("On the way down: the hand has let go"), Late.Grip01 < 0.05f);
	TestTrue(TEXT("On the way down: the roll has mostly settled"), Late.Roll01 < 0.2f);
	for (int32 i = 0; i <= 100; ++i)
	{
		const GBMantle::FVaultPose Pose = GBMantle::EvaluateVaultPose(i / 100.f, Rise, Drop);
		const float Weights[5] = { Pose.ItemLower01, Pose.Grip01, Pose.PitchDown01, Pose.Roll01, Pose.Dip01 };
		for (const float W : Weights)
		{
			TestTrue(TEXT("Weights stay within 0..1"), W >= 0.f && W <= 1.f);
		}
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
