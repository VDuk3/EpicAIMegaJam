// Pure-logic tests: which owned weapon goes in each slot when a snapshot is loaded, and the stepping of a round-by-round reload.
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Weapons/GBEquipmentComponent.h"
#include "Weapons/GBGun.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBWeaponsSnapshotSlotsTest, "Glyphbound.Weapons.SnapshotSlots",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGBWeaponsSnapshotSlotsTest::RunTest(const FString& Parameters)
{
	const FName OakStaff(TEXT("OakStaff"));
	const FName FireStaff(TEXT("FireStaff"));
	const FName Pistol(TEXT("Pistol"));
	const FName Shotgun(TEXT("Shotgun"));
	const FName Gone(TEXT("Gone")); // not in the data any more

	auto SlotOf = [&](FName Id, EGBHeldSlot& OutSlot)
	{
		if (Id == OakStaff || Id == FireStaff) { OutSlot = EGBHeldSlot::Staff; return true; }
		if (Id == Pistol || Id == Shotgun) { OutSlot = EGBHeldSlot::Gun; return true; }
		return false;
	};
	const TArray<FName> Owned = { OakStaff, Pistol, FireStaff, Shotgun };

	// Old snapshot (no equipped list): the first owned weapon of each kind.
	{
		const TMap<EGBHeldSlot, FName> Slots = UGBEquipmentComponent::ChooseEquippedWeapons(Owned, {}, SlotOf);
		TestEqual(TEXT("Old snapshot: first staff"), Slots.FindRef(EGBHeldSlot::Staff), OakStaff);
		TestEqual(TEXT("Old snapshot: first gun"), Slots.FindRef(EGBHeldSlot::Gun), Pistol);
		TestFalse(TEXT("Old snapshot: no sword owned, sword slot empty"), Slots.Contains(EGBHeldSlot::Sword));
	}

	// New snapshot: the equipped list wins over the order obtained.
	{
		const TMap<EGBHeldSlot, FName> Slots = UGBEquipmentComponent::ChooseEquippedWeapons(Owned, { FireStaff, Shotgun }, SlotOf);
		TestEqual(TEXT("Equipped staff is the later one"), Slots.FindRef(EGBHeldSlot::Staff), FireStaff);
		TestEqual(TEXT("Equipped gun is the later one"), Slots.FindRef(EGBHeldSlot::Gun), Shotgun);
	}

	// Two equipped of one slot: the first listed wins. Equipped but not owned, or unknown: ignored, the slot falls back.
	{
		const TMap<EGBHeldSlot, FName> Slots = UGBEquipmentComponent::ChooseEquippedWeapons(Owned, { Shotgun, Pistol, Gone }, SlotOf);
		TestEqual(TEXT("First equipped of a slot wins"), Slots.FindRef(EGBHeldSlot::Gun), Shotgun);
		TestEqual(TEXT("Staff slot falls back to the first owned staff"), Slots.FindRef(EGBHeldSlot::Staff), OakStaff);

		const TMap<EGBHeldSlot, FName> NotOwned = UGBEquipmentComponent::ChooseEquippedWeapons({ OakStaff }, { FireStaff }, SlotOf);
		TestEqual(TEXT("A weapon that is not owned is never equipped"), NotOwned.FindRef(EGBHeldSlot::Staff), OakStaff);

		const TMap<EGBHeldSlot, FName> Unknown = UGBEquipmentComponent::ChooseEquippedWeapons({ Gone, Pistol }, { Gone }, SlotOf);
		TestEqual(TEXT("Unknown ids take no slot"), Unknown.Num(), 1);
		TestEqual(TEXT("Unknown ids do not block the known one"), Unknown.FindRef(EGBHeldSlot::Gun), Pistol);
	}
	return true;
}


// ---- Round-by-round reload (the shotgun): pure stepping, FGBPerRoundReload ----
// Times are exact multiples of a cycle (ReloadTime / MagazineSize); the stepping's tolerance is 0.1 ms, so "just before" uses 1 ms.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBWeaponsPerRoundFullTest, "Glyphbound.Weapons.PerRoundFullReload",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGBWeaponsPerRoundFullTest::RunTest(const FString& Parameters)
{
	// Empty 6-round gun, 24 in reserve, ReloadTime 2 s: complete exactly at 2 s, not complete 1 ms before.
	constexpr float ReloadTime = 2.f;
	int32 Rounds = 0;
	int32 Reserve = 24;
	FGBPerRoundReload Reload;
	Reload.Begin(Rounds, Reserve, 6, ReloadTime);
	TestTrue(TEXT("A reload with space and reserve runs"), Reload.bRunning);
	TestEqual(TEXT("It plans to load the whole magazine"), Reload.Planned, 6);

	TestEqual(TEXT("1 ms before the first cycle ends nothing moved"), Reload.Advance(ReloadTime / 6.f - 0.001f, Rounds, Reserve, 6), 0);
	TestEqual(TEXT("Nothing is loaded before a cycle ends"), Rounds, 0);
	TestEqual(TEXT("The first round lands when its cycle ends"), Reload.Advance(0.001f, Rounds, Reserve, 6), 1);
	TestEqual(TEXT("It came out of the reserve"), Reserve, 23);

	// Up to 1 ms before ReloadTime: five rounds, still running. Then exactly ReloadTime: six and done.
	Reload.Advance(ReloadTime - ReloadTime / 6.f - 0.001f, Rounds, Reserve, 6);
	TestEqual(TEXT("Five rounds 1 ms before ReloadTime"), Rounds, 5);
	TestTrue(TEXT("Still running 1 ms before ReloadTime"), Reload.bRunning);
	TestTrue(TEXT("Progress is not complete 1 ms before ReloadTime"), Reload.Progress() < 1.f);

	Reload.Advance(0.001f, Rounds, Reserve, 6);
	TestEqual(TEXT("Six rounds exactly at ReloadTime"), Rounds, 6);
	TestEqual(TEXT("Exactly the magazine's worth left the reserve"), Reserve, 18);
	TestFalse(TEXT("A full gun ends the reload at ReloadTime"), Reload.bRunning);
	TestEqual(TEXT("Progress is complete"), Reload.Progress(), 1.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBWeaponsPerRoundReserveTest, "Glyphbound.Weapons.PerRoundShortReserve",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGBWeaponsPerRoundReserveTest::RunTest(const FString& Parameters)
{
	// 2 rounds in a 6-round gun but only 1 in reserve: one cycle is ReloadTime / MagazineSize (not / planned rounds).
	constexpr float ReloadTime = 2.f;
	const float Cycle = ReloadTime / 6.f;
	int32 Rounds = 2;
	int32 Reserve = 1;
	FGBPerRoundReload Reload;
	Reload.Begin(Rounds, Reserve, 6, ReloadTime);
	TestEqual(TEXT("It plans only what the reserve can give"), Reload.Planned, 1);
	TestEqual(TEXT("1 ms before one cycle: nothing"), Reload.Advance(Cycle - 0.001f, Rounds, Reserve, 6), 0);
	TestEqual(TEXT("Gun unchanged just before the cycle ends"), Rounds, 2);
	TestEqual(TEXT("Exactly at one cycle: the round lands"), Reload.Advance(0.001f, Rounds, Reserve, 6), 1);
	TestEqual(TEXT("The one round is loaded"), Rounds, 3);
	TestEqual(TEXT("The reserve is empty, not negative"), Reserve, 0);
	TestFalse(TEXT("It stops when the reserve is gone"), Reload.bRunning);
	TestEqual(TEXT("Further time moves nothing"), Reload.Advance(10.f, Rounds, Reserve, 6), 0);
	TestEqual(TEXT("Still 3 rounds"), Rounds, 3);

	// Nothing to load at all: it never runs.
	int32 FullRounds = 6;
	int32 FullReserve = 24;
	FGBPerRoundReload Full;
	Full.Begin(FullRounds, FullReserve, 6, ReloadTime);
	TestFalse(TEXT("A full gun has nothing to load"), Full.bRunning);
	int32 NoRounds = 0;
	int32 NoReserve = 0;
	FGBPerRoundReload Dry;
	Dry.Begin(NoRounds, NoReserve, 6, ReloadTime);
	TestFalse(TEXT("An empty reserve has nothing to load"), Dry.bRunning);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBWeaponsPerRoundBigStepTest, "Glyphbound.Weapons.PerRoundLongFrame",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGBWeaponsPerRoundBigStepTest::RunTest(const FString& Parameters)
{
	// One frame of 3.4 cycles: three rounds move and 0.4 cycle is kept; the next call must use that kept time.
	const float Cycle = 2.f / 6.f;
	int32 Rounds = 0;
	int32 Reserve = 24;
	FGBPerRoundReload Reload;
	Reload.Begin(Rounds, Reserve, 6, 2.f);
	TestEqual(TEXT("Three rounds in one long frame"), Reload.Advance(3.4f * Cycle, Rounds, Reserve, 6), 3);
	TestEqual(TEXT("Gun"), Rounds, 3);
	TestEqual(TEXT("Reserve"), Reserve, 21);
	TestTrue(TEXT("The remainder of the time is kept"), FMath::IsNearlyEqual(Reload.CycleElapsed, 0.4f * Cycle, 0.001f));

	// 0.4 kept + 0.5 = 0.9 of a cycle: still nothing. (Keeping fmod of the step, 0.5, or dropping the time, would differ below.)
	TestEqual(TEXT("0.4 kept plus 0.5 is still short of a cycle"), Reload.Advance(0.5f * Cycle, Rounds, Reserve, 6), 0);
	TestEqual(TEXT("Still three rounds"), Rounds, 3);
	TestTrue(TEXT("The remainder accumulates to 0.9 cycle"), FMath::IsNearlyEqual(Reload.CycleElapsed, 0.9f * Cycle, 0.001f));
	TestEqual(TEXT("0.9 kept plus 0.2 crosses the boundary"), Reload.Advance(0.2f * Cycle, Rounds, Reserve, 6), 1);
	TestEqual(TEXT("Four rounds"), Rounds, 4);
	TestEqual(TEXT("Reserve after the fourth"), Reserve, 20);
	TestTrue(TEXT("0.1 cycle is left over"), FMath::IsNearlyEqual(Reload.CycleElapsed, 0.1f * Cycle, 0.001f));

	// A pending shot makes the stepping yield after one round and keep the rest of the time.
	int32 PRounds = 0;
	int32 PReserve = 24;
	FGBPerRoundReload Pending;
	Pending.Begin(PRounds, PReserve, 6, 2.f);
	TestEqual(TEXT("With a limit of one round, a step of 3.4 cycles moves exactly one"), Pending.Advance(3.4f * Cycle, PRounds, PReserve, 6, 1), 1);
	TestEqual(TEXT("One round in the gun"), PRounds, 1);
	TestEqual(TEXT("One round out of the reserve"), PReserve, 23);
	TestTrue(TEXT("The reload is still running"), Pending.bRunning);
	TestTrue(TEXT("The rest of the time is kept (2.4 cycles)"), FMath::IsNearlyEqual(Pending.CycleElapsed, 2.4f * Cycle, 0.001f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBWeaponsPerRoundStopTest, "Glyphbound.Weapons.PerRoundStopPartWay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGBWeaponsPerRoundStopTest::RunTest(const FString& Parameters)
{
	// 1 round in a 6-round gun: the cycle is ReloadTime / MagazineSize (1/3 s), not ReloadTime / the 5 empty places.
	const float Cycle = 2.f / 6.f;
	int32 Rounds = 1;
	int32 Reserve = 24;
	FGBPerRoundReload Reload;
	Reload.Begin(Rounds, Reserve, 6, 2.f);
	TestEqual(TEXT("1 ms before the first boundary nothing moved"), Reload.Advance(Cycle - 0.001f, Rounds, Reserve, 6), 0);
	TestEqual(TEXT("Gun unchanged before the boundary"), Rounds, 1);
	TestEqual(TEXT("At the boundary the first round moved"), Reload.Advance(0.001f, Rounds, Reserve, 6), 1);
	TestEqual(TEXT("Two rounds in the gun"), Rounds, 2);

	// Another 1.5 cycles: one more round lands, half a cycle is in flight. Then it is stopped.
	Reload.Advance(1.5f * Cycle, Rounds, Reserve, 6);
	TestEqual(TEXT("Three rounds so far"), Rounds, 3);
	Reload.Stop();
	TestFalse(TEXT("Stopped"), Reload.bRunning);
	TestEqual(TEXT("The rounds already moved stay in the gun"), Rounds, 3);
	TestEqual(TEXT("Only the moved rounds left the reserve; the one in flight is still there"), Reserve, 22);
	TestEqual(TEXT("Rounds in the gun and in the reserve still add up"), Rounds + Reserve, 25);
	TestEqual(TEXT("A stopped reload moves nothing more"), Reload.Advance(5.f, Rounds, Reserve, 6), 0);
	TestEqual(TEXT("Gun unchanged after the stop"), Rounds, 3);
	TestEqual(TEXT("Reserve unchanged after the stop"), Reserve, 22);
	return true;
}

// ---- View pose (first-person guns): one shared bob, art-px-to-cm conversion, FOV conversion ----

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBWeaponsViewPoseBobTest, "Glyphbound.Weapons.ViewPose.Bob",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGBWeaponsViewPoseBobTest::RunTest(const FString& Parameters)
{
	// Still: no bob at all (the gun and the hands rest).
	TestTrue(TEXT("Zero speed at time zero"), GBViewBob::CalcBob(0.f, 0.f).IsZero());
	TestTrue(TEXT("Zero speed late in the run"), GBViewBob::CalcBob(1234.5f, 0.f).IsZero());

	// Running: bounded and on the move (a constant or a wrong axis would fail below).
	const FVector2D Start = GBViewBob::CalcBob(0.f, 1.f);
	TestTrue(TEXT("Full speed at time zero is (3, 0)"), Start.Equals(FVector2D(3.f, 0.f), 0.001f));
	const FVector2D Quarter = GBViewBob::CalcBob(UE_HALF_PI / 9.f, 1.f);
	TestTrue(TEXT("A quarter period later it is (0, 2)"), Quarter.Equals(FVector2D(0.f, 2.f), 0.01f));
	for (float Time = 0.f; Time < 10.f; Time += 0.07f)
	{
		const FVector2D Bob = GBViewBob::CalcBob(Time, 1.f);
		if (FMath::Abs(Bob.X) > 3.001f || Bob.Y < -0.001f || Bob.Y > 2.001f)
		{
			AddError(FString::Printf(TEXT("Bob out of bounds at t=%.2f: (%.3f, %.3f), expected |x|<=3 and 0<=y<=2"), Time, Bob.X, Bob.Y));
			return false;
		}
	}

	// Periodic in the phase (9 rad/s): the same time in the next swing gives the same shift.
	const FVector2D A = GBViewBob::CalcBob(1.3f, 0.6f);
	const FVector2D B = GBViewBob::CalcBob(1.3f + 2.f * UE_PI / 9.f, 0.6f);
	TestTrue(TEXT("Periodic with 2 pi / 9 s"), A.Equals(B, 0.01f));
	const FVector2D Half = GBViewBob::CalcBob(1.3f + UE_PI / 9.f, 0.6f);
	TestFalse(TEXT("Half a period later it moved"), A.Equals(Half, 0.1f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBWeaponsViewPoseArtPxToCmTest, "Glyphbound.Weapons.ViewPose.ArtPxToCm",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGBWeaponsViewPoseArtPxToCmTest::RunTest(const FString& Parameters)
{
	// Exact value, computed independently: 4 px/art px * 2 * 30 cm * tan(58.72/2) / 720 px.
	const float Expected = 4.f * 2.f * 30.f * FMath::Tan(FMath::DegreesToRadians(58.72f * 0.5f)) / 720.f;
	const float Got = GBViewBob::CmPerArtPixel(4.f, 30.f, 58.72f, 720.f);
	TestTrue(TEXT("4 art px at 30 cm, weapon FOV 58.72, 720p"), FMath::IsNearlyEqual(Got, Expected, 0.0001f));
	TestFalse(TEXT("Nonzero for sane input"), FMath::IsNearlyZero(Got, 1e-6f));

	// A shift of N art px projects back to N art px on screen at any depth and screen height: forward (cm)
	// then back through the same vertical FOV must be the identity (a missing factor of 2, a wrong FOV or a
	// ClipX mix-up fails this).
	const float Shifts[3] = { 1.f, 3.f, -2.5f };
	const float Depths[3] = { 10.f, 30.f, 60.f };
	const float Heights[3] = { 540.f, 720.f, 1080.f };
	for (float Shift : Shifts)
	{
		for (float Depth : Depths)
		{
			for (float ClipY : Heights)
			{
				const float Cm = Shift * GBViewBob::CmPerArtPixel(4.f, Depth, 58.72f, ClipY);
				const float BackPx = Cm / (2.f * Depth * FMath::Tan(FMath::DegreesToRadians(58.72f * 0.5f))) * ClipY;
				const float BackArt = BackPx / 4.f;
				if (!FMath::IsNearlyEqual(BackArt, Shift, 0.001f))
				{
					AddError(FString::Printf(TEXT("Roundtrip failed: %.2f art px at %.0f cm / %.0f px came back as %.4f"), Shift, Depth, ClipY, BackArt));
					return false;
				}
			}
		}
	}

	// Guards: no world, no screen, no depth, no FOV.
	TestEqual(TEXT("Zero with no screen height"), GBViewBob::CmPerArtPixel(4.f, 30.f, 58.72f, 0.f), 0.f);
	TestEqual(TEXT("Zero with no depth"), GBViewBob::CmPerArtPixel(4.f, 0.f, 58.72f, 720.f), 0.f);
	TestEqual(TEXT("Zero with no pixel size"), GBViewBob::CmPerArtPixel(0.f, 30.f, 58.72f, 720.f), 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBWeaponsViewPoseFovConvertTest, "Glyphbound.Weapons.ViewPose.FovConvert",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGBWeaponsViewPoseFovConvertTest::RunTest(const FString& Parameters)
{
	// The shipped conversions (vertical FOV at 16:9 for the same horizontal framing). A linear rescale
	// (h * 9/16) gives 50.6 for 90 and fails; the formula is 2*atan(tan(h/2)*9/16).
	TestTrue(TEXT("Hip 90 at 16:9 is 58.72"), FMath::IsNearlyEqual(GBViewBob::HorizontalToVerticalFov(90.f, 16.f / 9.f), 58.72f, 0.05f));
	TestTrue(TEXT("Pistol aim 70 at 16:9 is 43.0"), FMath::IsNearlyEqual(GBViewBob::HorizontalToVerticalFov(70.f, 16.f / 9.f), 43.0f, 0.05f));
	TestTrue(TEXT("Rifle aim 60 at 16:9 is 36.0"), FMath::IsNearlyEqual(GBViewBob::HorizontalToVerticalFov(60.f, 16.f / 9.f), 36.0f, 0.05f));
	// Same horizontal framing on a narrower screen: the vertical FOV grows (73.74 at 4:3, more sky and floor).
	TestTrue(TEXT("4:3 sees more vertically than 16:9"), GBViewBob::HorizontalToVerticalFov(90.f, 4.f / 3.f) > 58.72f);
	TestTrue(TEXT("90 at 4:3 is 73.74"), FMath::IsNearlyEqual(GBViewBob::HorizontalToVerticalFov(90.f, 4.f / 3.f), 73.74f, 0.05f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBWeaponsViewPoseMotionGridTest, "Glyphbound.Weapons.ViewPose.MotionGrid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGBWeaponsViewPoseMotionGridTest::RunTest(const FString& Parameters)
{
	// One motion grid: art * ClipY/120 snapped to the active grid. Hand grid = round(ClipY/120),
	// overlay grid = 3 * round(ClipY/270) (CENTRAL 3x). The result must be a multiple of the active grid.
	const float ClipHeights[3] = { 540.f, 720.f, 1080.f };
	const FVector2D Motions[4] = { FVector2D(3.f, 0.f), FVector2D(-2.5f, 1.5f), FVector2D(0.3f, -0.7f), FVector2D(7.5f, -37.f) };
	for (float ClipY : ClipHeights)
	{
		const float HandGrid = FMath::Max(1.f, FMath::RoundToFloat(ClipY / 120.f));
		const float OverlayGrid = FMath::Max(1.f, 3.f * FMath::RoundToFloat(ClipY / 270.f));
		for (const FVector2D& Motion : Motions)
		{
			for (int32 Overlay = 0; Overlay < 2; ++Overlay)
			{
				const bool bOverlay = Overlay == 1;
				const float Grid = bOverlay ? OverlayGrid : HandGrid;
				const FVector2D Screen = GBViewBob::MotionToScreenPx(Motion, ClipY, bOverlay);
				// Multiple of the active grid (a wrong factor like ClipY/270 for the gun fails here).
				const float Qx = Screen.X / Grid;
				const float Qy = Screen.Y / Grid;
				if (!FMath::IsNearlyEqual(Qx, FMath::RoundToFloat(Qx), 0.001f)
					|| !FMath::IsNearlyEqual(Qy, FMath::RoundToFloat(Qy), 0.001f))
				{
					AddError(FString::Printf(TEXT("Not on the %s grid at ClipY %.0f: motion (%.2f, %.2f) -> screen (%.2f, %.2f), grid %.2f"),
						bOverlay ? TEXT("overlay") : TEXT("hand"), ClipY, Motion.X, Motion.Y, Screen.X, Screen.Y, Grid));
					return false;
				}
				// A gun shift projects back to exactly the hands' shift: cm then back through the same
				// vertical FOV must be the identity (a missing factor of 2 or a wrong FOV fails this).
				const float Depth = 30.f;
				const float Fov = 58.72f;
				const float HalfV = FMath::DegreesToRadians(FMath::Clamp(Fov, 0.001f, 179.999f) * 0.5f);
				const float CmPerScreenPx = 2.f * Depth * FMath::Tan(HalfV) / ClipY;
				const FVector2D Cm(Screen.X * CmPerScreenPx, Screen.Y * CmPerScreenPx);
				const FVector2D BackPx(Cm.X / CmPerScreenPx, Cm.Y / CmPerScreenPx);
				if (!BackPx.Equals(Screen, 0.01f))
				{
					AddError(FString::Printf(TEXT("Roundtrip failed at ClipY %.0f: screen (%.2f, %.2f) came back (%.2f, %.2f)"),
						ClipY, Screen.X, Screen.Y, BackPx.X, BackPx.Y));
					return false;
				}
			}
		}
	}
	// Grids differ at 1080p (hand 9, overlay 12): the overlay snaps coarser than the hands.
	TestEqual(TEXT("Hand grid at 1080p is 9"), FMath::RoundToInt(1080.f / 120.f), 9);
	{
		const FVector2D Hand = GBViewBob::MotionToScreenPx(FVector2D(1.f, 0.f), 1080.f, false);
		const FVector2D Overlay = GBViewBob::MotionToScreenPx(FVector2D(1.f, 0.f), 1080.f, true);
		TestTrue(TEXT("One hand art pixel is 9 screen px at 1080p"), Hand.Equals(FVector2D(9.f, 0.f), 0.01f));
		TestTrue(TEXT("One hand art pixel snaps to 12 screen px with the overlay at 1080p"), Overlay.Equals(FVector2D(12.f, 0.f), 0.01f));
	}
	// Guards.
	TestTrue(TEXT("Zero motion stays zero"), GBViewBob::MotionToScreenPx(FVector2D::ZeroVector, 1080.f, false).IsZero());
	TestTrue(TEXT("Zero height stays zero"), GBViewBob::MotionToScreenPx(FVector2D(1.f, 1.f), 0.f, false).IsZero());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
