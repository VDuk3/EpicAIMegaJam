// Committed jump takeoff: horizontal velocity from the input held at takeoff (GDD 18).
#include "Player/GBPlayerCharacter.h"
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

#endif // WITH_DEV_AUTOMATION_TESTS
