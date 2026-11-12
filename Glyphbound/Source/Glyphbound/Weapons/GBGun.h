// Guns (GDD 8): cyberpunk, 3D placeholder viewmodel. Shoot, aim, reload, magazines; the weapon enchant applies.
#pragma once

#include "CoreMinimal.h"
#include "Core/GBPlaceholder.h"
#include "Weapons/GBHeldItem.h"
#include "GBGun.generated.h"

class UStaticMeshComponent;

/**
 * Every number through GetStat: Damage, FireInterval, MagazineSize, SpareMagazines, ReloadTime, Range, Spread,
 * AimSpread, RecoilPitch, RecoilYaw, RecoilRecovery, Pellets, Falloff*, HeadshotMult, AimFOV, AimMoveSpeedMult,
 * AimTime, and ProjectileSpeed / ProjectileRadius / ProjectileGravity for FireMode "Projectile".
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
	UStaticMeshComponent* AddPart(FName Name, EGBShape Shape, const FVector& Location, const FVector& Size, const FLinearColor& Color, bool bGlow);

	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Parts;

	FGBAmmoState Ammo;
	bool bReloading = false;
	float ReloadElapsed = 0.f;
	float ReloadDuration = 0.f;

	bool bAiming = false;
	/** 0 = hip, 1 = fully aimed. */
	float AimAlpha = 0.f;
	float HipFOV = 90.f;
};
