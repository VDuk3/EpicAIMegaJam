// Base class of everything the player holds: staffs, guns, the elemental sword.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/GBTypes.h"
#include "Data/GBDataTypes.h"
#include "GBHeldItem.generated.h"

class AGBPlayerCharacter;
class UGBStatsComponent;

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
 * (keys "Weapon.<Id>.<Stat>" and "Weapon.All.<Stat>" on the owner's UGBStatsComponent).
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

protected:
	AGBPlayerCharacter* GetPlayerOwner() const { return PlayerOwner.Get(); }
	UGBStatsComponent* GetOwnerStats() const;

	/** Builds the item's damage for one hit: Damage stat, element, instigator, SourceId = WeaponId, OnHitEffect from Strings. */
	FGBDamageInfo MakeDamageInfo() const;

	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;

	UPROPERTY() FName WeaponId = NAME_None;
	UPROPERTY() EGBHeldSlot Slot = EGBHeldSlot::Gun;
	UPROPERTY() EGBElement Element = EGBElement::Physical;
	TWeakObjectPtr<AGBPlayerCharacter> PlayerOwner;
	bool bEquipped = false;
	double LastAttackTime = -1000.0;
};
