// The player's held slots: one staff, one gun, and the elemental sword once obtained (GDD 8).
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Templates/Function.h"
#include "Core/GBTypes.h"
#include "Weapons/GBHeldItem.h"
#include "GBEquipmentComponent.generated.h"

struct FGBPlayerSnapshot;

DECLARE_MULTICAST_DELEGATE_OneParam(FGBOnActiveItemChanged, AGBHeldItem* /*NewActive (may be null)*/);

UCLASS(ClassGroup = (Glyphbound), meta = (BlueprintSpawnableComponent))
class GLYPHBOUND_API UGBEquipmentComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGBEquipmentComponent();

	/**
	 * Gives a weapon from Weapons.json (owner's rule, 2026-11-12: a picked-up weapon never replaces the one in
	 * hand and nothing is left on the floor). The weapon joins the owned list. If the slot of its kind is empty
	 * it is put there (and, if bAutoSwitch, brought to the hands); if the slot is taken it just stays owned and
	 * the player equips it from the inventory with EquipOwnedWeapon.
	 * Returns the held item if the weapon is in its slot after the call, null otherwise (unknown id, or stored).
	 * bDropOld is ignored (kept so old callers compile).
	 */
	AGBHeldItem* GiveWeapon(FName WeaponId, bool bAutoSwitch = true, bool bDropOld = false);

	/** Every weapon the player owns, in the order they were obtained (equipped ones included). */
	const TArray<FName>& GetOwnedWeapons() const { return OwnedWeapons; }
	bool OwnsWeapon(FName WeaponId) const;
	/** True if this owned weapon is the one in its slot right now. */
	bool IsWeaponEquipped(FName WeaponId) const;
	/**
	 * Puts an owned weapon in the slot of its kind, replacing the one there (which stays owned). Each gun keeps
	 * its own ammo while stored. If the replaced weapon was in the hands, the new one comes up. False if the
	 * weapon is not owned, is already equipped, or switching is locked.
	 */
	bool EquipOwnedWeapon(FName WeaponId);

	/**
	 * Ammo of an owned gun, held or stored (a stored gun that was never held is full). False if the weapon is
	 * not owned or uses no ammo. For the inventory's weapon list.
	 */
	bool GetWeaponAmmo(FName WeaponId, FGBAmmoState& OutAmmo) const;
	/** Element of an owned weapon, held or stored. The definition's element if it was never changed. */
	EGBElement GetWeaponElement(FName WeaponId) const;
	/** Changes the element of an owned weapon, held or stored (the forged sword takes the gem's element). */
	void SetWeaponElement(FName WeaponId, EGBElement InElement);

	/**
	 * Pure rule (tested): which owned weapon goes in each slot when a snapshot is loaded. Ids in Equipped that are
	 * owned come first (the first one of a slot wins); a slot still empty gets the first owned weapon of its kind
	 * (old snapshots have no Equipped list). SlotOf returns false for an unknown id, which is then skipped.
	 */
	static TMap<EGBHeldSlot, FName> ChooseEquippedWeapons(const TArray<FName>& Owned, const TArray<FName>& Equipped,
		TFunctionRef<bool(FName /*WeaponId*/, EGBHeldSlot& /*OutSlot*/)> SlotOf);

	bool HasWeaponInSlot(EGBHeldSlot Slot) const;
	AGBHeldItem* GetItemInSlot(EGBHeldSlot Slot) const;

	AGBHeldItem* GetActiveItem() const;
	EGBHeldSlot GetActiveSlot() const { return ActiveSlot; }
	bool IsHoldingStaff() const;
	bool IsHoldingGun() const;

	/** False if the slot is empty or switching is locked. */
	bool SwitchTo(EGBHeldSlot Slot);
	/** Next / previous filled slot (mouse wheel, Alt, D-pad up). */
	void CycleSlot(int32 Direction = 1);

	/** While locked (QTE running, cutscene), SwitchTo and CycleSlot do nothing. */
	void SetSwitchLocked(bool bLocked) { bSwitchLocked = bLocked; }

	/** Refills every gun (era travel). */
	void RefillAllAmmo();
	/** Adds spare magazines to the gun in the gun slot. Returns how many were added. */
	int32 AddSpareMagazines(int32 Count);

	/** HUD skin asked by the active item; Rpg90s when the hands are empty. */
	EGBHudSkin GetHudSkin() const;

	void SaveToSnapshot(FGBPlayerSnapshot& Snapshot) const;
	void LoadFromSnapshot(const FGBPlayerSnapshot& Snapshot);

	FGBOnActiveItemChanged OnActiveItemChanged;
	/** Any change of weapons or ammo that the HUD should redraw. */
	FSimpleMulticastDelegate OnEquipmentChanged;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Spawns, attaches, initializes and hides an item (guns full of ammo). Not added to Items. Null if the id is unknown. */
	AGBHeldItem* SpawnItem(FName WeaponId);
	/** Puts the current item away and brings up the one in Slot, ignoring the lock. Cancels a sustained spell. */
	void EquipSlot(EGBHeldSlot Slot);
	void DestroyAllItems();
	/** Puts an item that leaves its slot away: stops it and remembers its ammo / element in the Stored maps. */
	void StoreItemState(AGBHeldItem* Item);
	/** Gives a freshly spawned item the ammo / element it had while stored, and forgets them. */
	void RestoreItemState(AGBHeldItem* Item);
	/** Full magazine and full spare magazines of a gun, with the owner's modifiers (as AGBHeldItem::GetStat reads them). */
	FGBAmmoState ComputeFullAmmo(FName WeaponId) const;
	/** Slot of a weapon id from its definition. False if the id is unknown. */
	bool FindWeaponSlot(FName WeaponId, EGBHeldSlot& OutSlot) const;
	/** The held item with this id, or null if it is stored. */
	AGBHeldItem* FindEquippedItem(FName WeaponId) const;

	UPROPERTY() TMap<EGBHeldSlot, TObjectPtr<AGBHeldItem>> Items;
	/** Owned weapon ids (equipped or stored). */
	UPROPERTY() TArray<FName> OwnedWeapons;
	/** Ammo of the guns that are owned but not in the slot right now. */
	UPROPERTY() TMap<FName, FGBAmmoState> StoredAmmo;
	/** Element of stored weapons whose element was changed (the sword). */
	UPROPERTY() TMap<FName, EGBElement> StoredElements;
	EGBHeldSlot ActiveSlot = EGBHeldSlot::Staff;
	bool bSwitchLocked = false;
};
