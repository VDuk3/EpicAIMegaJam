// Base class of everything the player holds: staffs, guns, the elemental sword.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/GBTypes.h"
#include "Data/GBDataTypes.h"
#include "GBHeldItem.generated.h"

class AGBPlayerCharacter;
class UGBStatsComponent;

/** Weapon stats owned by the Weapons module that are not in Core/GBStatNames.h. */
namespace GBWeaponStat
{
	inline const FName AimTime(TEXT("AimTime"));                    // seconds to go from hip to full aim (FOV lerp)
}

/** Keys of the status-effect "Strings" map read by the gun enchant hook (GDD 6 "Weapon enchant"). */
namespace GBWeaponString
{
	inline const FName WeaponElement(TEXT("WeaponElement"));         // element name ("Fire", "Lightning", "Ice"...)
	inline const FName WeaponOnHitEffect(TEXT("WeaponOnHitEffect")); // status effect id applied by the enchanted gun's hits
}

namespace GBWeapon
{
	/**
	 * Channel for hitscan and aim traces. Not Visibility: the engine's Pawn profile ignores Visibility, so a
	 * Visibility trace would go through enemy capsules. Camera blocks pawns and walls and ignores triggers.
	 */
	inline constexpr ECollisionChannel AimTraceChannel = ECC_Camera;
}

/** Ammo of one weapon (GDD 8: fixed spare magazines, each with a fixed number of bullets). Zero for staffs and swords. */
USTRUCT(BlueprintType)
struct GLYPHBOUND_API FGBAmmoState
{
	GENERATED_BODY()

	UPROPERTY() int32 InMagazine = 0;
	UPROPERTY() int32 SpareMagazines = 0;
};

/**
 * A held item is an actor attached to the player's view. It is created from a FGBWeaponDefinition (Weapons.json)
 * by UGBEquipmentComponent and reads every number through GetStat(), so skill-tree and enchant modifiers apply
 * (keys "Weapon.<Id>.<Stat>", "Weapon.<Slot>.<Stat>" and "Weapon.All.<Stat>" on the owner's UGBStatsComponent).
 * Subclasses: AGBStaff (basic magic attack), AGBGun (shoot, aim, reload), AGBSword (melee).
 */
UCLASS(Abstract)
class GLYPHBOUND_API AGBHeldItem : public AActor
{
	GENERATED_BODY()

public:
	AGBHeldItem();

	/** Called once right after spawn. */
	virtual void InitFromDefinition(const FGBWeaponDefinition& Definition, AGBPlayerCharacter* InOwner);

	FName GetWeaponId() const { return WeaponId; }
	EGBHeldSlot GetSlot() const { return Slot; }
	/** May differ from the definition (the sword takes the gem's element). */
	EGBElement GetElement() const { return Element; }
	void SetElement(EGBElement InElement) { Element = InElement; }
	/** The definition, looked up by id each time (never cached across frames). May be null after a data reload that removed it. */
	const FGBWeaponDefinition* GetDefinition() const;
	FString GetDisplayName() const;

	/** Weapon stat with modifiers applied. */
	float GetStat(FName Stat, float DefaultValue = 0.f) const;

	/**
	 * Builds the item's damage for one hit: Damage stat, element, instigator, SourceId = WeaponId,
	 * OnHitEffect from Strings. When the hit's element comes from the weapon enchant, BuildUp is
	 * multiplied by EnchantBuildUpMult (Arcane Conduit); the sword's own element is never an enchant.
	 */
	FGBDamageInfo MakeDamageInfo() const;

	// ---- Input, routed by the player character ----
	virtual void StartPrimary() {}
	virtual void StopPrimary() {}
	virtual void StartSecondary() {}
	virtual void StopSecondary() {}
	virtual void Reload() {}

	/** Brought up / put away (the actor stays alive while not held, hidden). */
	virtual void OnEquipped();
	virtual void OnUnequipped();
	bool IsEquipped() const { return bEquipped; }

	// ---- Ammo (meaningful for guns; staffs and swords return false / zero) ----
	virtual bool UsesAmmo() const { return false; }
	virtual FGBAmmoState GetAmmo() const { return FGBAmmoState(); }
	virtual void SetAmmo(const FGBAmmoState& InAmmo) {}
	/** Full magazine and full spare magazines (era travel, GDD 8). */
	virtual void RefillAmmo() {}
	/** Adds spare magazines up to the SpareMagazines stat. Returns how many were really added. */
	virtual int32 AddSpareMagazines(int32 Count) { return 0; }
	virtual bool IsReloading() const { return false; }
	virtual bool IsAiming() const { return false; }
	/** 0..1 progress of the current reload, for the HUD. */
	virtual float GetReloadProgress() const { return 0.f; }

	/** HUD skin this item asks for (GDD 13): staff and sword = Rpg90s, gun = Cyberpunk. */
	virtual EGBHudSkin GetHudSkin() const;

	/** Seconds since the last primary attack, for HUD hand animation. */
	float GetTimeSinceLastAttack() const;

	/** Definition's bAutomatic (hold to keep attacking). */
	bool IsAutomatic() const;

	/**
	 * The weapon enchant active on the player, if any: the first active status effect whose definition has
	 * Strings["WeaponElement"]. OutOnHitEffect = Strings["WeaponOnHitEffect"] or None. Only guns use it (GDD 6).
	 */
	bool GetActiveEnchant(EGBElement& OutElement, FName& OutOnHitEffect) const;

	virtual void Tick(float DeltaSeconds) override;

protected:
	AGBPlayerCharacter* GetPlayerOwner() const { return PlayerOwner.Get(); }
	UGBStatsComponent* GetOwnerStats() const;

	/** True only for items the weapon enchant applies to (guns; never the sword, GDD 6). */
	virtual bool AcceptsEnchant() const { return false; }

	// ---- Trigger handling shared by every item (FireInterval, bAutomatic, a pressed shot is buffered until ready) ----
	void PressTrigger();
	void ReleaseTrigger();
	/** Attacks if the trigger asks for it and FireInterval has passed. Called from Tick and on press. */
	void UpdateTrigger();
	bool IsAttackReady() const;
	/** One attack. Return true if it happened (sets LastAttackTime); false if refused (no mana, empty magazine...). */
	virtual bool PerformAttack() { return false; }

	/** Point the camera aims at: trace from the aim origin up to Range; the far point when nothing is hit. */
	FVector FindAimPoint(float Range) const;

	/** Broadcasts the owner's UGBEquipmentComponent::OnEquipmentChanged (ammo and reload changes). */
	void NotifyEquipmentChanged() const;

	bool bTriggerHeld = false;
	bool bAttackQueued = false;
	FRandomStream Random;

	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;

	UPROPERTY() FName WeaponId = NAME_None;
	UPROPERTY() EGBHeldSlot Slot = EGBHeldSlot::Gun;
	UPROPERTY() EGBElement Element = EGBElement::Physical;
	TWeakObjectPtr<AGBPlayerCharacter> PlayerOwner;
	bool bEquipped = false;
	double LastAttackTime = -1000.0;
};
