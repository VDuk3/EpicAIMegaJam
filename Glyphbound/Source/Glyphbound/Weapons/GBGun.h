// Guns (GDD 8): cyberpunk, 3D placeholder viewmodel. Shoot, aim, reload, magazines; the weapon enchant applies.
#pragma once

#include "CoreMinimal.h"
#include "Core/GBPlaceholder.h"
#include "Weapons/GBHeldItem.h"
#include "GBGun.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UCanvas;
class UCanvasRenderTarget2D;
struct FGBDamageInfo;

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
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

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

	/** True when the real pistol meshes were built (Definition.ViewModel == Pistol and all six loaded). */
	bool HasPistolViewModel() const { return bPistolViewModel; }
	/** Pistol parts with a loaded SM_Pistol_* mesh (must be 6). */
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
	/** Text currently drawn on the holographic ammo display (rounds in the magazine, "--" while reloading). */
	const FString& GetPistolDisplayText() const { return ShownDisplayText; }
	/** Last Flash value sent to the weak-point light (0 = off, 1 = full glow). */
	float GetPistolWeakPointFlash() const { return LastFlashSent; }

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

	/** Loads the six SM_Pistol_* meshes and attaches them. False when any mesh is missing (caller falls back to placeholders). */
	bool TryBuildPistolViewModel();
	/** Trigger pull, magazine slide and glow color, only while the pistol viewmodel exists. */
	void UpdatePistolViewModel(float DeltaSeconds);
	/** Global damage event: a weak-point hit by this gun's owner while this gun is equipped lights the weak-point light. */
	void HandleDamageDealt(AActor* Target, const FGBDamageInfo& Info, float DamageDealt);
	/** Draws ShownDisplayText into the ammo display render target (its update delegate). */
	UFUNCTION()
	void DrawPistolDisplay(UCanvas* Canvas, int32 Width, int32 Height);
	/** Sends the Flash value to the weak-point light material, only when it changed. */
	void SetWeakPointFlash(float Value);

	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Parts;

	/** Real pistol viewmodel (only when Definition.ViewModel == Pistol): body, trigger, magazine, energy bar, weak-point light, ammo display. */
	UPROPERTY() TObjectPtr<UStaticMeshComponent> PistolBody = nullptr;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> PistolTrigger = nullptr;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> PistolMagazine = nullptr;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> PistolEnergyBar = nullptr;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> PistolWeakPointLight = nullptr;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> PistolAmmoDisplay = nullptr;
	/** One dynamic instance per material slot of the six pistol parts, so glow and imbue can be set cheaply. */
	UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> PistolMIDs;
	/** Instance of the weak-point light's material (parameter Flash); also in PistolMIDs. */
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> PistolIndicatorMID = nullptr;
	/** Instance of the ammo display's material (parameter DisplayTexture); also in PistolMIDs. */
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> PistolDisplayMID = nullptr;
	/** Runtime render target the ammo text is drawn into (redrawn only when the text changes). */
	UPROPERTY() TObjectPtr<UCanvasRenderTarget2D> PistolDisplayTarget = nullptr;
	/** Text last drawn into the render target (the cache that decides whether a redraw is needed). */
	FString ShownDisplayText;
	/** Seconds since the last weak-point hit. Negative = the light is off. */
	float WeakPointFlashElapsed = -1.f;
	float LastFlashSent = -1.f;
	FDelegateHandle DamageDealtHandle;
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
