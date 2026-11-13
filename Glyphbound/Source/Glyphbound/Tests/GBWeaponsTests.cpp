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

#endif // WITH_DEV_AUTOMATION_TESTS
