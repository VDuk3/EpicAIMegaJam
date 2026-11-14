// Pure-logic test of the enemies' local steering decision: which candidate direction is taken around an obstacle.
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "AI/GBSteering.h"
#include "AI/GBEngage.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GBAITestsLocal
{
	/** Three steps per side (7 candidates, offsets -3..3), every candidate blocked except the listed offsets. */
	TArray<bool> OnlyFree(std::initializer_list<int32> FreeOffsets)
	{
		constexpr int32 N = 3;
		TArray<bool> Blocked;
		Blocked.Init(true, 2 * N + 1);
		for (const int32 Offset : FreeOffsets)
		{
			Blocked[Offset + N] = false;
		}
		return Blocked;
	}

	int32 Pick(std::initializer_list<int32> FreeOffsets, EGBSteerMode Mode, int32 Side)
	{
		const TOptional<int32> Choice = GBSteering::ChooseOffset(OnlyFree(FreeOffsets), Mode, Side);
		return Choice.IsSet() ? Choice.GetValue() : 99;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBAISteerChoiceTest, "Glyphbound.AI.SteerChoice",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGBAISteerChoiceTest::RunTest(const FString& Parameters)
{
	using namespace GBAITestsLocal;
	const EGBSteerMode Free = EGBSteerMode::Free;
	const EGBSteerMode Hold = EGBSteerMode::HoldSide;
	const EGBSteerMode Around = EGBSteerMode::GoAround;

	// Free: straight wins; otherwise the smallest angle, the preferred side only breaks ties.
	TestEqual(TEXT("Free: straight when it is free"), Pick({ 0, 1, -1 }, Free, -1), 0);
	TestEqual(TEXT("Free: closest angle beats the preferred side"), Pick({ -1, 2 }, Free, 1), -1);
	TestEqual(TEXT("Free: tie goes to the preferred side (-)"), Pick({ 1, -1 }, Free, -1), -1);
	TestEqual(TEXT("Free: tie goes to the preferred side (+)"), Pick({ 1, -1 }, Free, 1), 1);
	TestEqual(TEXT("Free: widest angle when it is the only way"), Pick({ -3 }, Free, 1), -3);

	// HoldSide: no flipping at a corner while the held side still has a way.
	TestEqual(TEXT("Hold: straight still first"), Pick({ 0, 2 }, Hold, 1), 0);
	TestEqual(TEXT("Hold: wider angle on the held side beats the other side"), Pick({ -1, 2 }, Hold, 1), 2);
	TestEqual(TEXT("Hold: held side blocked, falls back to the other side"), Pick({ -3 }, Hold, 1), -3);

	// GoAround: committed to a side, straight only as a last resort.
	TestEqual(TEXT("Around: skips a free straight direction"), Pick({ 0, -1, 1 }, Around, -1), -1);
	TestEqual(TEXT("Around: other side when the committed side is closed"), Pick({ 0, 2 }, Around, -1), 2);
	TestEqual(TEXT("Around: straight when nothing else is free"), Pick({ 0 }, Around, 1), 0);

	// Boxed in, or a malformed candidate list: no direction.
	TestFalse(TEXT("All blocked: no direction"), GBSteering::ChooseOffset(OnlyFree({}), Free, 1).IsSet());
	const TArray<bool> EvenSized = { false, false };
	TestFalse(TEXT("Even-sized list: no direction"), GBSteering::ChooseOffset(EvenSized, Free, 1).IsSet());
	return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBAIEngageTest, "Glyphbound.AI.Engage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGBAIEngageTest::RunTest(const FString& Parameters)
{
	using GBEngage::Decide;
	const EGBEngageAction Advance = EGBEngageAction::Advance;
	const EGBEngageAction Hold = EGBEngageAction::Hold;
	const EGBEngageAction Retreat = EGBEngageAction::Retreat;

	// Band 600..1400.
	TestTrue(TEXT("Too far: advance"), Decide(1800.f, true, 600.f, 1400.f, true) == Advance);
	TestTrue(TEXT("No sight in the band: advance"), Decide(1000.f, false, 600.f, 1400.f, true) == Advance);
	TestTrue(TEXT("No sight and too close: still advance (it has nothing to shoot at)"), Decide(300.f, false, 600.f, 1400.f, true) == Advance);
	TestTrue(TEXT("In the band with sight: hold"), Decide(1000.f, true, 600.f, 1400.f, true) == Hold);
	TestTrue(TEXT("Edges of the band are inside it (max)"), Decide(1400.f, true, 600.f, 1400.f, true) == Hold);
	TestTrue(TEXT("Edges of the band are inside it (min)"), Decide(600.f, true, 600.f, 1400.f, true) == Hold);
	TestTrue(TEXT("Too close with room behind: retreat"), Decide(300.f, true, 600.f, 1400.f, true) == Retreat);
	TestTrue(TEXT("Too close with a wall behind: hold, do not retreat into it"), Decide(300.f, true, 600.f, 1400.f, false) == Hold);

	// Notice rule: cone 140 degrees (half angle 70), aggro 2600.
	using GBEngage::Notices;
	TestTrue(TEXT("In front, in range, sight: notices"), Notices(1500.f, 20.f, true, 2600.f, 140.f, false));
	TestFalse(TEXT("Behind its back: does not notice"), Notices(1500.f, 120.f, true, 2600.f, 140.f, false));
	TestFalse(TEXT("Behind a wall: does not notice"), Notices(1500.f, 20.f, false, 2600.f, 140.f, false));
	TestFalse(TEXT("Out of range: does not notice"), Notices(3000.f, 20.f, true, 2600.f, 140.f, false));
	TestTrue(TEXT("Hit or alerted: notices from behind, out of range, without sight"), Notices(5000.f, 170.f, false, 2600.f, 140.f, true));
	TestFalse(TEXT("Aggro range 0 (a dummy) never notices, even when hit"), Notices(100.f, 0.f, true, 0.f, 140.f, true));
	TestTrue(TEXT("Negative angle counts like the positive one"), Notices(1500.f, -60.f, true, 2600.f, 140.f, false));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
