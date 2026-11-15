// Pure-logic test of the segment clipping the tunnel fog uses to write a row of collision spheres for a shot.
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "World/GBFluidFog.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBFluidFogClipTest, "Glyphbound.FluidFog.Clip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGBFluidFogClipTest::RunTest(const FString& Parameters)
{
	const FBox Box(FVector(0.f, -100.f, -100.f), FVector(1000.f, 100.f, 100.f));

	// A segment that misses: beside the box, and in line with it but stopping short.
	{
		TArray<FVector> Points;
		TestEqual(TEXT("Parallel beside the box: no points"), GBFluidFog::ClipSegmentPoints(FVector(0.f, 300.f, 0.f), FVector(1000.f, 300.f, 0.f), Box, 40.f, 64, Points), 0);
		TestEqual(TEXT("Stops short of the box: no points"), GBFluidFog::ClipSegmentPoints(FVector(-500.f, 0.f, 0.f), FVector(-10.f, 0.f, 0.f), Box, 40.f, 64, Points), 0);
		TestEqual(TEXT("Nothing added to the output"), Points.Num(), 0);
	}

	// A segment that crosses it end to end: only the part inside (x 0..1000) is sampled, every 100 cm.
	{
		TArray<FVector> Points;
		const int32 Count = GBFluidFog::ClipSegmentPoints(FVector(-500.f, 0.f, 0.f), FVector(1500.f, 0.f, 0.f), Box, 100.f, 64, Points);
		TestEqual(TEXT("Crossing: 0, 100 ... 1000 is eleven points"), Count, 11);
		if (Points.Num() == 11)
		{
			TestTrue(TEXT("Crossing: the first point is where it enters"), Points[0].Equals(FVector(0.f, 0.f, 0.f), 0.5f));
			TestTrue(TEXT("Crossing: the last point is where it leaves"), Points[10].Equals(FVector(1000.f, 0.f, 0.f), 0.5f));
		}
	}

	// A segment that starts inside: the first point is its start, the points stop at the box face.
	{
		TArray<FVector> Points;
		const int32 Count = GBFluidFog::ClipSegmentPoints(FVector(500.f, 0.f, 0.f), FVector(2000.f, 0.f, 0.f), Box, 100.f, 64, Points);
		TestEqual(TEXT("Starting inside: 500 ... 1000 is six points"), Count, 6);
		if (Points.Num() == 6)
		{
			TestTrue(TEXT("Starting inside: the first point is the start"), Points[0].Equals(FVector(500.f, 0.f, 0.f), 0.5f));
			TestTrue(TEXT("Starting inside: the last point is on the far face"), Points[5].Equals(FVector(1000.f, 0.f, 0.f), 0.5f));
		}
	}

	// A slanted segment: it enters through one side face and leaves through the other.
	{
		float T0 = 0.f;
		float T1 = 0.f;
		TestTrue(TEXT("Diagonal crosses the box"), GBFluidFog::ClipSegment(FVector(500.f, -300.f, -100.f), FVector(500.f, 300.f, 100.f), Box, T0, T1));
		// y reaches -100 at t = 1/3 and +100 at t = 2/3; z only leaves the box at t = 1.
		TestTrue(TEXT("Diagonal enters at a third"), FMath::IsNearlyEqual(T0, 1.f / 3.f, 0.001f));
		TestTrue(TEXT("Diagonal leaves at two thirds"), FMath::IsNearlyEqual(T1, 2.f / 3.f, 0.001f));
	}

	// The cap: a long row is spread over exactly MaxPoints and still ends on the far face.
	{
		TArray<FVector> Points;
		const int32 Count = GBFluidFog::ClipSegmentPoints(FVector(0.f, 0.f, 0.f), FVector(1000.f, 0.f, 0.f), Box, 10.f, 5, Points);
		TestEqual(TEXT("Capped at five points"), Count, 5);
		if (Points.Num() == 5)
		{
			TestTrue(TEXT("Capped row ends on the far face"), Points[4].Equals(FVector(1000.f, 0.f, 0.f), 0.5f));
			TestTrue(TEXT("Capped row is evenly spread"), Points[2].Equals(FVector(500.f, 0.f, 0.f), 0.5f));
		}
	}

	return true;
}

#endif
