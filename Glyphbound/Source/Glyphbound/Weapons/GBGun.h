// Guns (GDD 8): cyberpunk, 3D placeholder viewmodel. Shoot, aim, reload, magazines; the weapon enchant applies.
#pragma once

#include "CoreMinimal.h"
#include "Core/GBPlaceholder.h"
#include "Weapons/GBHeldItem.h"
#include "GBGun.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;

/**
 * Every number through GetStat: Damage, FireInterval, MagazineSize, SpareMagazines, ReloadTime, Range, Spread,
 * AimSpread, RecoilPitch, RecoilYaw, RecoilRecovery, Pellets, Falloff*, HeadshotMult, AimFOV, AimMoveSpeedMult,
 * AimTime, EnchantBuildUpMult, and ProjectileSpeed / ProjectileRadius / ProjectileGravity for FireMode "Projectile".
 * Reload: consumes one spare magazine, fills the magazine; the rounds left in the old one are lost
 * (provisional, classic tactical style). Switching weapon cancels it.
 */
UCLASS()
class GLYPHBOUND_API AGBGun : public AGBHeldItem
{
	GENERATED_BODY()

public:
	virtual void InitFromDefinition(const FGBWeaponDefinition& Definition, AGBPlayerCharacter* InOwner) override;
	virtual void Tick(float DeltaSeconds) override;

	virtual void StartPrimary() override { PressTrigger(); }
	virtual void StopPrimary() override { ReleaseTrigger(); }
	virtual void StartSecondary() override;
	virtual void StopSecondary() override;
	virtual void Reload() override;

	virtual void OnUnequipped() override;

	virtual bool UsesAmmo() const override { return true; }
	virtual FGBAmmoState GetAmmo() const override { return Ammo; }
	virtual void SetAmmo(const FGBAmmoState& InAmmo) override;
	virtual void RefillAmmo() override;
	virtual int32 AddSpareMagazines(int32 Count) override;
	virtual bool IsReloading() const override { return bReloading; }
	virtual bool IsAiming() const override { return bAiming; }
	virtual float GetReloadProgress() const override;

	int32 GetMagazineSize() const;
	/** Spread half-angle in degrees right now: hip or aim, plus the moving bonus (GDD 4). Shots and crosshair use it. */
	float GetCurrentSpread() const;
	int32 GetMaxSpareMagazines() const;

	/** True when the real pistol meshes were built (Definition.ViewModel == Pistol and all four loaded). */
	bool HasPistolViewModel() const { return bPistolViewModel; }
	/** Pistol parts with a loaded SM_Pistol_* mesh (must be 4). */
	int32 GetPistolMeshCount() const;
	/** Dynamic materials made for the pistol parts (must be more than 4: the body alone has many slots). */
	int32 GetPistolMIDCount() const { return PistolMIDs.Num(); }
	/** Distance in cm of the magazine from its rest location, in the body's frame. */
	float GetPistolMagazineOffsetCm() const;
	/** Element whose color is currently sent to the pistol materials (only meaningful when enchanted). */
	EGBElement GetPistolGlowElement() const { return LastGlowElement; }
	/** Whether the pistol glow currently comes from the weapon enchant. */
	bool IsPistolGlowEnchanted() const { return bLastGlowEnchanted; }
	/** Last ImbueRemaining value sent to the pistol materials. */
	float GetPistolImbueSent() const { return LastImbueSent; }

protected:
	virtual bool AcceptsEnchant() const override { return true; }
	virtual bool PerformAttack() override;

	void FireHitscan(const FGBDamageInfo& BaseInfo, float SpreadDegrees);
	void FireProjectile(const FGBDamageInfo& BaseInfo, float SpreadDegrees);
	void CancelReload();
	void FinishReload();
	void SetAiming(bool bInAiming);
	/** Moves the FOV towards the aim target; snaps back to the hip FOV when bInstant. */
	void UpdateAimFOV(float DeltaSeconds, bool bInstant);
	void BuildViewModel(const FGBWeaponDefinition& Definition);
	void BuildPlaceholderViewModel(const FGBWeaponDefinition& Definition);
	UStaticMeshComponent* AddPart(FName Name, EGBShape Shape, const FVector& Location, const FVector& Size, const FLinearColor& Color, bool bGlow);

	/** Loads the four SM_Pistol_* meshes and attaches them. False when any mesh is missing (caller falls back to placeholders). */
	bool TryBuildPistolViewModel();
	/** Trigger pull, magazine slide and glow color, only while the pistol viewmodel exists. */
	void UpdatePistolViewModel(float DeltaSeconds);

	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Parts;

	/** Real pistol viewmodel (only when Definition.ViewModel == Pistol): body, trigger, magazine, energy bar. */
	UPROPERTY() TObjectPtr<UStaticMeshComponent> PistolBody = nullptr;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> PistolTrigger = nullptr;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> PistolMagazine = nullptr;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> PistolEnergyBar = nullptr;
	/** One dynamic instance per material slot of the four pistol parts, so glow and imbue can be set cheaply. */
	UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> PistolMIDs;
	bool bPistolViewModel = false;
	/** Seconds since the last shot started the trigger pull. Negative = the trigger is at rest. */
	float TriggerPullElapsed = -1.f;
	/** Last values sent to the pistol materials (parameters are only set when they change). */
	bool bHasLastGlow = false;
	FLinearColor LastGlowColor = FLinearColor::Black;
	float LastImbueSent = 0.f;
	EGBElement LastGlowElement = EGBElement::Physical;
	bool bLastGlowEnchanted = false;

	FGBAmmoState Ammo;
	bool bReloading = false;
	float ReloadElapsed = 0.f;
	float ReloadDuration = 0.f;

	bool bAiming = false;
	/** 0 = hip, 1 = fully aimed. */
	float AimAlpha = 0.f;
	float HipFOV = 90.f;
};
