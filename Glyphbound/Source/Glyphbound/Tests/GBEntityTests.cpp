// Pure-logic tests of the Entity tentacle math (AI/GBTentacleMath): FABRIK reach and the lag spring.
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "AI/GBTentacleMath.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GBEntityTestsLocal
{
	TArray<FVector> StraightChain(int32 Joints, float Spacing)
	{
		TArray<FVector> Chain;
		for (int32 Index = 0; Index < Joints; ++Index)
		{
			Chain.Add(FVector(Index * Spacing, 0.f, 0.f));
		}
		return Chain;
	}

	bool SegmentsKept(const TArray<FVector>& Chain, float Spacing, float Tolerance)
	{
		for (int32 Index = 0; Index + 1 < Chain.Num(); ++Index)
		{
			if (!FMath::IsNearlyEqual((Chain[Index + 1] - Chain[Index]).Size(), Spacing, Tolerance))
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
	TArray<FVector> Joints = StraightChain(5, 40.f);
	const FVector Target(100.f, 80.f, 0.f); // 128 cm from the root, inside the 160 cm reach
	GBTentacle::SolveFabrik(Joints, Target, 50, 0.1f);
	TestTrue(TEXT("Tip ends within the tolerance"), (Joints.Last() - Target).Size() <= 0.1f);
	TestTrue(TEXT("Root does not move"), Joints[0].Equals(FVector::ZeroVector));
	TestTrue(TEXT("Every segment length is kept"), SegmentsKept(Joints, 40.f, 0.1f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBEntityFabrikTooFarTest, "Glyphbound.Entity.FabrikTooFar",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGBEntityFabrikTooFarTest::RunTest(const FString& Parameters)
{
	using namespace GBEntityTestsLocal;
	TArray<FVector> Joints = StraightChain(5, 40.f);
	const FVector Target(500.f, 60.f, 0.f); // far outside the 160 cm reach
	GBTentacle::SolveFabrik(Joints, Target, 50, 0.1f);
	TestTrue(TEXT("Every segment length is kept"), SegmentsKept(Joints, 40.f, 0.1f));
	const FVector Direction = (Target - FVector::ZeroVector).GetSafeNormal();
	bool bStraight = true;
	for (const FVector& Joint : Joints)
	{
		// Distance of the joint from the root-to-target line.
		const float LineDist = FVector::CrossProduct(Joint - FVector::ZeroVector, Direction).Size();
		bStraight = bStraight && LineDist <= 0.1f;
	}
	TestTrue(TEXT("The chain is straight and points at the target"), bStraight);
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
	for (int32 Step = 0; Step < 60; ++Step) // 1 s at 60 Hz
	{
		GBTentacle::SpringStep(Value, Velocity, 10.f, Stiffness, Damping, 1.f / 60.f);
		bOvershot = bOvershot || Value > 10.5f;
	}
	TestTrue(TEXT("Never overshoots by more than 5 percent"), !bOvershot);
	TestTrue(TEXT("Settled within 0.1 of the target in 1 s"), FMath::Abs(Value - 10.f) <= 0.1f);

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
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
