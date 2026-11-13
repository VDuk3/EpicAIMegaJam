// Pure-logic tests of the Entity tentacle math (AI/GBTentacleMath): FABRIK reach and the follow spring.
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "AI/GBTentacleMath.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GBEntityTestsLocal
{
	/** A chain along X from Origin with the given segment lengths (unequal lengths catch code that assumes one length). */
	TArray<FVector> ChainAlongX(const FVector& Origin, const TArray<float>& Lengths)
	{
		TArray<FVector> Chain;
		FVector At = Origin;
		Chain.Add(At);
		for (float Length : Lengths)
		{
			At += FVector(Length, 0.f, 0.f);
			Chain.Add(At);
		}
		return Chain;
	}

	bool SegmentsKept(const TArray<FVector>& Chain, const TArray<float>& Lengths, float Tolerance)
	{
		for (int32 Index = 0; Index + 1 < Chain.Num(); ++Index)
		{
			if (!FMath::IsNearlyEqual((Chain[Index + 1] - Chain[Index]).Size(), Lengths[Index], Tolerance))
			{
				return false;
			}
		}
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBEntityFabrikReachTest, "Glyphbound.Entity.FabrikReach",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGBEntityFabrikReachTest::RunTest(const FString& Parameters)
{
	using namespace GBEntityTestsLocal;

	// A translated chain with unequal segments and a target off every axis, inside the 140 cm reach.
	const FVector Root(120.f, -70.f, 35.f);
	const TArray<float> Lengths = { 50.f, 30.f, 40.f, 20.f };
	TArray<FVector> Joints = ChainAlongX(Root, Lengths);
	const FVector Target = Root + FVector(40.f, 55.f, -45.f); // about 83 cm away
	GBTentacle::SolveFabrik(Joints, Target, 50, 0.1f);
	TestTrue(TEXT("Tip ends within the tolerance"), (Joints.Last() - Target).Size() <= 0.1f);
	TestTrue(TEXT("Root does not move"), Joints[0].Equals(Root));
	TestTrue(TEXT("Every unequal segment length is kept"), SegmentsKept(Joints, Lengths, 0.1f));

	// Coincident joints (a chain that starts folded) must not collapse a segment: the 40 cm segments stay 40 cm.
	const TArray<float> Equal = { 40.f, 40.f, 40.f, 40.f };
	TArray<FVector> Stacked = ChainAlongX(FVector::ZeroVector, Equal);
	Stacked[2] = Stacked[1]; // two joints in one place
	Stacked[4] = Stacked[3];
	const TArray<FVector> StackedStart = Stacked;
	GBTentacle::SolveFabrik(Stacked, FVector(60.f, 30.f, 0.f), 60, 0.1f);
	TestTrue(TEXT("Coincident start: root fixed"), Stacked[0].Equals(StackedStart[0]));
	bool bAnySegmentCollapsed = false;
	for (int32 Index = 0; Index + 1 < Stacked.Num(); ++Index)
	{
		const float Length = (Stacked[Index + 1] - Stacked[Index]).Size();
		const float Authored = (StackedStart[Index + 1] - StackedStart[Index]).Size();
		bAnySegmentCollapsed = bAnySegmentCollapsed || !FMath::IsNearlyEqual(Length, Authored, 0.1f);
	}
	TestFalse(TEXT("Coincident start: no segment changes length"), bAnySegmentCollapsed);

	// Collinear chain, target on its own line but inside the reach: the chain has to fold, and the tip must get there.
	TArray<FVector> Line = ChainAlongX(FVector::ZeroVector, Equal);
	const FVector OnLine(120.f, 0.f, 0.f);
	GBTentacle::SolveFabrik(Line, OnLine, 100, 0.5f);
	TestTrue(TEXT("Collinear: tip reaches a target on the line"), (Line.Last() - OnLine).Size() <= 0.5f);
	TestTrue(TEXT("Collinear: segment lengths kept"), SegmentsKept(Line, Equal, 0.2f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBEntityFabrikTooFarTest, "Glyphbound.Entity.FabrikTooFar",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGBEntityFabrikTooFarTest::RunTest(const FString& Parameters)
{
	using namespace GBEntityTestsLocal;
	const FVector Root(-30.f, 80.f, 15.f);
	const TArray<float> Lengths = { 40.f, 25.f, 40.f, 35.f };
	TArray<FVector> Joints = ChainAlongX(Root, Lengths);
	const FVector Target = Root + FVector(300.f, 200.f, -120.f); // far outside the 140 cm reach
	GBTentacle::SolveFabrik(Joints, Target, 50, 0.1f);
	TestTrue(TEXT("Root does not move"), Joints[0].Equals(Root));
	TestTrue(TEXT("Every segment length is kept"), SegmentsKept(Joints, Lengths, 0.1f));

	// Straight and pointing at the target, with no folding: each joint sits at its cumulative length along the direction.
	const FVector Direction = (Target - Root).GetSafeNormal();
	float Expected = 0.f;
	bool bStraightAndForward = true;
	for (int32 Index = 1; Index < Joints.Num(); ++Index)
	{
		Expected += Lengths[Index - 1];
		const float Projection = FVector::DotProduct(Joints[Index] - Root, Direction);
		const float OffLine = FVector::CrossProduct(Joints[Index] - Root, Direction).Size();
		bStraightAndForward = bStraightAndForward && FMath::IsNearlyEqual(Projection, Expected, 0.1f) && OffLine <= 0.1f;
	}
	TestTrue(TEXT("The chain is straight, unfolded and points at the target"), bStraightAndForward);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBEntitySpringSettlesTest, "Glyphbound.Entity.SpringSettles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGBEntitySpringSettlesTest::RunTest(const FString& Parameters)
{
	constexpr float Stiffness = 60.f;
	const float Damping = 2.f * FMath::Sqrt(Stiffness); // critical-ish damping
	bool bOvershot = false;
	float Value = 0.f;
	float Velocity = 0.f;
	float ValueAfterTenth = 0.f;
	float VelocityAfterTenth = 0.f;
	for (int32 Step = 0; Step < 60; ++Step) // 1 s at 60 Hz
	{
		GBTentacle::SpringStep(Value, Velocity, 10.f, Stiffness, Damping, 1.f / 60.f);
		bOvershot = bOvershot || Value > 10.5f;
		if (Step == 5)
		{
			ValueAfterTenth = Value;
			VelocityAfterTenth = Velocity;
		}
	}
	TestTrue(TEXT("Never overshoots by more than 5 percent"), !bOvershot);
	TestTrue(TEXT("Settled within 0.1 of the target in 1 s"), FMath::Abs(Value - 10.f) <= 0.1f);
	// Not an instant snap: after 0.1 s it is on its way (moving, but far from the target).
	TestTrue(TEXT("Transient: still short of the target after 0.1 s"), ValueAfterTenth > 0.3f && ValueAfterTenth < 8.f);
	TestTrue(TEXT("Transient: moving towards the target after 0.1 s"), VelocityAfterTenth > 1.f);

	// A long frame (dt 0.1) stays finite and settles too.
	float BigValue = 0.f;
	float BigVelocity = 0.f;
	bool bFinite = true;
	for (int32 Step = 0; Step < 200; ++Step)
	{
		GBTentacle::SpringStep(BigValue, BigVelocity, 10.f, Stiffness, Damping, 0.1f);
		bFinite = bFinite && FMath::IsFinite(BigValue) && FMath::IsFinite(BigVelocity);
	}
	TestTrue(TEXT("Stable at dt 0.1"), bFinite);
	TestTrue(TEXT("Settled at dt 0.1 too"), FMath::Abs(BigValue - 10.f) <= 0.2f);

	// The same 0.3 s gives the same value whatever the frame time (the sub-stepping is consistent).
	float SmallValue = 0.f, SmallVelocity = 0.f, LongValue = 0.f, LongVelocity = 0.f;
	for (int32 Step = 0; Step < 18; ++Step)
	{
		GBTentacle::SpringStep(SmallValue, SmallVelocity, 10.f, Stiffness, Damping, 1.f / 60.f);
	}
	for (int32 Step = 0; Step < 3; ++Step)
	{
		GBTentacle::SpringStep(LongValue, LongVelocity, 10.f, Stiffness, Damping, 0.1f);
	}
	TestTrue(TEXT("Frame-time independent over 0.3 s"), FMath::Abs(SmallValue - LongValue) <= 0.1f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
