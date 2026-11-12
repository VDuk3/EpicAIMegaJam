// The player's held slots: one staff, one gun, and the elemental sword once obtained (GDD 8).
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
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
	 * Gives a weapon from Weapons.json. It goes to the slot its definition says; a weapon already in that slot is
	 * replaced (and, if bDropOld, left on the floor as a pickup - provisional rule for swapping guns and staffs).
	 * bAutoSwitch brings it up immediately. Returns the new item, or null if the id is unknown.
	 */
	AGBHeldItem* GiveWeapon(FName WeaponId, bool bAutoSwitch = true, bool bDropOld = true);

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
	UPROPERTY() TMap<EGBHeldSlot, TObjectPtr<AGBHeldItem>> Items;
	EGBHeldSlot ActiveSlot = EGBHeldSlot::Staff;
	bool bSwitchLocked = false;
};
