// Pure-logic test of the wreck simulation of the flying enemies (AI/GBDebris): no world, the "world" is a floor plane and a wall.
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "AI/GBDebris.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GBDebrisTestLocal
{
	constexpr float Radius = 20.f;
	constexpr float WallX = 500.f;

	/** A sphere of Radius against the floor z = 0 (normal up) and the wall x = WallX (normal -X); the earlier contact wins. */
	bool SweepBox(const FVector& From, const FVector& To, FGBDebrisHit& Out)
	{
		float BestTime = 2.f;
		if (To.Z < Radius && To.Z < From.Z)
		{
			const float Time = FMath::Clamp((From.Z - Radius) / (From.Z - To.Z), 0.f, 1.f);
			if (Time < BestTime)
			{
				BestTime = Time;
				Out.Normal = FVector::UpVector;
				Out.Position = FMath::Lerp(From, To, Time);
				Out.ImpactPoint = FVector(Out.Position.X, Out.Position.Y, 0.f);
			}
		}
		if (To.X > WallX - Radius && To.X > From.X)
		{
			const float Time = FMath::Clamp((WallX - Radius - From.X) / (To.X - From.X), 0.f, 1.f);
			if (Time < BestTime)
			{
				BestTime = Time;
				Out.Normal = FVector(-1.f, 0.f, 0.f);
				Out.Position = FMath::Lerp(From, To, Time);
				Out.ImpactPoint = FVector(WallX, Out.Position.Y, Out.Position.Z);
			}
		}
		return BestTime <= 1.f;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBDebrisBounceTest, "Glyphbound.Debris.Bounce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGBDebrisBounceTest::RunTest(const FString& Parameters)
{
	using namespace GBDebrisTestLocal;
	const FGBDebrisParams Params;
	const float Dt = 1.f / 60.f;
	auto Sweep = [](const FVector& From, const FVector& To, FGBDebrisHit& Out) { return SweepBox(From, To, Out); };

	// 1. Dropped from 500 cm: it must bounce, each apex lower than the one before, and end resting ON the floor.
	{
		FGBDebrisState State;
		State.Position = FVector(0.f, 0.f, 500.f);
		const float DropHeight = State.Position.Z - Radius;
		TArray<float> Apexes;
		float Apex = 0.f;
		bool bWasRising = false;
		float LowestZ = State.Position.Z;
		int32 Steps = 0;
		for (; Steps < 900 && !State.bAtRest; ++Steps)
		{
			GBDebris::Step(State, Dt, Params, Sweep);
			LowestZ = FMath::Min(LowestZ, State.Position.Z);
			if (State.Velocity.Z > 0.f)
			{
				bWasRising = true;
				Apex = FMath::Max(Apex, State.Position.Z);
			}
			else if (bWasRising)
			{
				Apexes.Add(Apex - Radius);   // height of the sphere's bottom over the floor
				bWasRising = false;
				Apex = 0.f;
			}
		}
		TestTrue(TEXT("The piece comes to rest within 15 s"), State.bAtRest);
		TestTrue(TEXT("It bounced at least twice"), State.Bounces >= 2);
		TestTrue(TEXT("It never sank under the floor"), LowestZ >= Radius - 0.5f);
		TestTrue(TEXT("It rests on the floor, not floating over it"), FMath::Abs(State.Position.Z - Radius) < 1.f);
		TestTrue(TEXT("There was a first bounce to measure"), Apexes.Num() >= 1);
		if (Apexes.Num() >= 1)
		{
			// The first bounce keeps Restitution^2 of the height: well under the drop, clearly above a dead stop.
			TestTrue(FString::Printf(TEXT("The first bounce is lower than the drop (%.1f of %.1f)"), Apexes[0], DropHeight), Apexes[0] < 0.4f * DropHeight);
			TestTrue(FString::Printf(TEXT("The first bounce is not dead (%.1f)"), Apexes[0]), Apexes[0] > 0.03f * DropHeight);
		}
		for (int32 Index = 1; Index < Apexes.Num(); ++Index)
		{
			TestTrue(FString::Printf(TEXT("Bounce %d (%.1f) is lower than bounce %d (%.1f)"), Index + 1, Apexes[Index], Index, Apexes[Index - 1]), Apexes[Index] < Apexes[Index - 1]);
		}
	}

	// 2. Thrown sideways at a wall: it comes back (moves away from the wall) and never passes through it.
	{
		FGBDebrisState State;
		State.Position = FVector(0.f, 0.f, 300.f);
		State.Velocity = FVector(800.f, 0.f, 0.f);
		float MaxX = State.Position.X;
		bool bCameBack = false;
		for (int32 Step = 0; Step < 600 && !State.bAtRest; ++Step)
		{
			GBDebris::Step(State, Dt, Params, Sweep);
			MaxX = FMath::Max(MaxX, State.Position.X);
			bCameBack |= State.Velocity.X < -20.f;
		}
		TestTrue(TEXT("It hit the wall and came back"), bCameBack);
		TestTrue(TEXT("It never went through the wall"), MaxX <= WallX - Radius + 0.5f);
		TestTrue(TEXT("It ends in front of the wall"), State.Position.X < WallX - Radius);
		TestTrue(TEXT("It ends resting on the floor"), State.bAtRest && FMath::Abs(State.Position.Z - Radius) < 1.f);
	}
	return true;
}

#endif
