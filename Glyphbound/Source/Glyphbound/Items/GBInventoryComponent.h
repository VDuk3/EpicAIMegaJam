// Items the player carries: consumables and the quick-use slot, armor, key items, materials, recipes.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/GBTypes.h"
#include "Data/GBDataTypes.h"
#include "GBInventoryComponent.generated.h"

struct FGBPlayerSnapshot;

/** Item stat keys (Items.json "Stats") that are not shared with weapons or spells. */
namespace GBItemStat
{
	inline const FName ThrowPitch(TEXT("ThrowPitch"));   // degrees added above the aim direction when throwing
	inline const FName Bounciness(TEXT("Bounciness"));   // bounce retention of a throwable (UProjectileMovementComponent default 0.6)
	inline const FName Friction(TEXT("Friction"));       // surface friction of a throwable when bouncing (UProjectileMovementComponent default 0.2)
}

DECLARE_MULTICAST_DELEGATE_TwoParams(FGBOnItemCountChanged, FName /*ItemId*/, int32 /*NewCount*/);

/**
 * GDD 8, 9, 11, 12. Everything is driven by Items.json:
 *  - Consumable: using it applies its UseEffect status effect to the player (potions over time, instant injection);
 *  - Throwable: using it throws an AGBProjectile built from its Stats (electric grenade);
 *  - Armor: one full-armor slot; equipping applies its Modifiers under the source "Armor";
 *  - KeyItem / Material: only counted, used by recipes (hilt + gem = elemental sword, quest items = final portal);
 *  - Ammo: never stored; picking it up adds spare magazines to the gun.
 * The quick-use slot (F / R1) holds one bQuickUse item at a time and cycles with Shift / D-pad left-right.
 */
UCLASS(ClassGroup = (Glyphbound), meta = (BlueprintSpawnableComponent))
class GLYPHBOUND_API UGBInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGBInventoryComponent();

	/** Gives the starting items from Player.json. Called by the player on BeginPlay. */
	void InitializeFromData();

	/** Adds up to Count (limited by MaxStack). Ammo items go straight to the gun. Returns how many were really taken. */
	int32 AddItem(FName ItemId, int32 Count = 1);
	/** How many ammo items AddedMagazines pays for at MagazinesPerItem each, rounded up (a partly-filled gun keeps the remainder on the pickup). Pure helper. */
	static int32 UsedAmmoItems(int32 AddedMagazines, int32 MagazinesPerItem);	/** Returns false (and removes nothing) if there are fewer than Count. */
	bool RemoveItem(FName ItemId, int32 Count = 1);
	int32 GetCount(FName ItemId) const;
	bool HasItem(FName ItemId) const { return GetCount(ItemId) > 0; }
	/** Item id -> count, only items with count > 0. */
	const TMap<FName, int32>& GetItems() const { return Items; }

	// ---- Using ----
	/** Uses one unit of a Consumable or Throwable. False if there is none or it cannot be used now. */
	bool UseItem(FName ItemId);

	// ---- Quick-use slot ----
	FName GetQuickUseItem() const { return QuickUseItem; }
	/** Next / previous owned bQuickUse item. */
	void CycleQuickUse(int32 Direction = 1);
	bool UseQuickItem();

	// ---- Armor ----
	FName GetEquippedArmor() const { return EquippedArmor; }
	/** NAME_None unequips. The armor stays in the inventory count while equipped. */
	bool EquipArmor(FName ItemId);

	// ---- Recipes ----
	/** The recipe that combines these two items (in either order), or null. */
	const FGBRecipeDefinition* FindRecipe(FName ItemA, FName ItemB) const;
	/** Applies a recipe if the player has both items: consumes, gives the result item / weapon, sets the flag. */
	bool TryCombine(FName ItemA, FName ItemB);

	void SaveToSnapshot(FGBPlayerSnapshot& Snapshot) const;
	void LoadFromSnapshot(const FGBPlayerSnapshot& Snapshot);

	FGBOnItemCountChanged OnItemCountChanged;
	/** Anything the HUD or the inventory screen should redraw (counts, quick slot, armor). */
	FSimpleMulticastDelegate OnInventoryChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** First owned bQuickUse item after From in Items.json order (Direction +1 / -1, wrapping; From itself last). */
	FName FindQuickUseFrom(FName From, int32 Direction) const;
	/** Removes the "Armor" modifier source and re-adds the equipped armor's modifiers (equip, load, data reload). */
	void ApplyArmorModifiers();
	/** Called when an item's count reaches 0: moves the quick slot and unequips the armor if needed. */
	void HandleStackEmptied(FName ItemId);
	bool UseThrowable(const FGBItemDefinition& Def);
	void HandleDataReloaded();
	void ShowHudMessage(const FString& Text) const;

	UPROPERTY() TMap<FName, int32> Items;
	UPROPERTY() FName QuickUseItem = NAME_None;
	UPROPERTY() FName EquippedArmor = NAME_None;

	FDelegateHandle DataReloadedHandle;
};
