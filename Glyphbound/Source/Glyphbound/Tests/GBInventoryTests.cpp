// Pure-logic tests of the ammo pickup remainder: a partly-filled gun keeps the rest on the pickup.
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Items/GBInventoryComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBItemsAmmoRemainderTest, "Glyphbound.Items.AmmoRemainder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGBItemsAmmoRemainderTest::RunTest(const FString& Parameters)
{
	// Args: added magazines, magazines per item. A wrong rounding (down instead of up) fails these.
	TestEqual(TEXT("Full fit pays for every item"), UGBInventoryComponent::UsedAmmoItems(6, 2), 3);
	TestEqual(TEXT("One magazine of two left over still costs the item"), UGBInventoryComponent::UsedAmmoItems(5, 2), 3);
	TestEqual(TEXT("One leftover magazine of three costs one item"), UGBInventoryComponent::UsedAmmoItems(4, 3), 2);
	TestEqual(TEXT("Nothing added costs nothing"), UGBInventoryComponent::UsedAmmoItems(0, 2), 0);
	TestEqual(TEXT("One magazine per item is exact"), UGBInventoryComponent::UsedAmmoItems(2, 1), 2);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
