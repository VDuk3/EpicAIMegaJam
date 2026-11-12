// Pure-logic test of which owned weapon goes in each slot when a snapshot is loaded (owned + equipped lists).
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Weapons/GBEquipmentComponent.h"

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

#endif // WITH_DEV_AUTOMATION_TESTS
