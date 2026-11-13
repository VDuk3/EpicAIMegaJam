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
struct FGBGunModelArt;

/** Reload animation shared by the gun viewmodel and the HUD hands: everything is a function of the reload progress (0 to 1). */
namespace GBReloadAnim
{
	/** Where the 2D left hand is, as three 0..1 amounts: Reach (towards the magazine well), Drop (off the bottom of the screen) and Push (the seat bump). */
	struct FHandBeat
	{
		float Reach = 0.f;
		float Drop = 0.f;
		float Push = 0.f;
	};
	/** Zero beats for a negative progress (no reload animation running). */
	FHandBeat HandBeat(float Progress);
	/** 0..1: how far the gun is in its reload pose (eases in over the first beat, out over the last). 0 for a negative progress. */
	float PoseAmount(float Progress);
}

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

	/** True when the real model meshes were built (Definition.ViewModel names a model and all its meshes loaded). */
	bool HasViewModel() const { return bViewModel; }
	/** Parts with a loaded mesh of the model (pistol 6, rifle 5). */
	int32 GetViewModelMeshCount() const;
	/** Dynamic materials made for the model's parts (the body alone has many slots). */
	int32 GetViewModelMIDCount() const { return ViewModelMIDs.Num(); }
	/** Distance in cm of the magazine from its rest location, in the body's frame. */
	float GetViewModelMagazineOffsetCm() const;
	/** Element whose color is currently sent to the model materials (only meaningful when enchanted). */
	EGBElement GetViewModelGlowElement() const { return LastGlowElement; }
	/** Whether the model glow currently comes from the weapon enchant. */
	bool IsViewModelGlowEnchanted() const { return bLastGlowEnchanted; }
	/** Last ImbueRemaining value sent to the model materials. */
	float GetViewModelImbueSent() const { return LastImbueSent; }
	/** Whether the energy bar is in its last-seconds blink (the enchant is about to end). The value sent is 0 in the dark half of each blink. */
	bool IsViewModelEnergyBarBlinking() const { return bEnergyBarBlinking; }
	/** Text currently drawn on the holographic ammo display (rounds in the magazine, "--" while reloading). */
	const FString& GetViewModelDisplayText() const { return ShownDisplayText; }
	/** Last Flash value sent to the weak-point light (0 = off, 1 = full glow). */
	float GetViewModelWeakPointFlash() const { return LastFlashSent; }
	/** Whether the muzzle flash shape is showing right now. */
	bool IsMuzzleFlashVisible() const;
	/** How far back in cm the shot kick has pushed the viewmodel right now (0 at rest). */
	float GetViewModelKickBackCm() const { return CurrentKickBackCm; }
	/** Whether the magazine mesh is showing (it is hidden while the old one is gone and the new one has not arrived). */
	bool IsViewModelMagazineVisible() const;
	/** Distance in cm of the viewmodel root from its rest location (reload pose and shot kick both move it). */
	float GetViewModelRootOffsetCm() const;
	/** Angle in degrees between the viewmodel root's rotation and its rest rotation. */
	float GetViewModelRootAngleDeg() const;
	/** Per-model art value for the HUD: how far (sprite pixels, x right, y down) the left hand moves to meet the magazine. */
	FVector2D GetReloadHandReach() const;
	/** Per-model art value for the HUD: how far (sprite pixels) the right hand moves at full pose so it stays on the grip. */
	FVector2D GetReloadRightHandFollow() const;
	/** Progress (0 to 1) the reload animation is showing: the pinned preview if any, else the real reload; -1 when none. */
	float GetReloadAnimProgress() const;
	/** Dev only (smoke run): pins the reload animation at this progress without touching ammo or the reload itself. */
	void SetReloadPreview(float Progress);
	/** Dev only: releases the pin; the gun blends back to rest. */
	void ClearReloadPreview();

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

	/** Loads the model's SM_* meshes and attaches them. False when any mesh is missing (caller falls back to placeholders). */
	bool TryBuildViewModel(const FGBGunModelArt& Art);
	/** Trigger pull, magazine slide, glow color, weak-point light, ammo text, kick and muzzle flash, only while the viewmodel exists. */
	void UpdateViewModel(float DeltaSeconds);
	/** Starts the shot kick and the muzzle flash. */
	void StartShotEffects();
	/** The one place that writes the viewmodel root's transform: rest pose + reload pose + shot kick. */
	void ApplyViewModelPose();
	/** Places the magazine (slide, fall on screen, hidden, rise, slide in) from the reload animation progress. */
	void ApplyReloadMagazine();
	/** Starts the short blend from the current reload pose back to rest (a cancelled reload or a released preview). */
	void BeginReloadBlend();
	/** Magazine at rest and visible, pose at rest, no blend: used when the gun leaves the hand. */
	void ResetReloadVisuals();
	void HideMuzzleFlash();
	/** Global damage event: a weak-point hit by this gun's owner while this gun is equipped lights the weak-point light. */
	void HandleDamageDealt(AActor* Target, const FGBDamageInfo& Info, float DamageDealt);
	/** Draws ShownDisplayText into the ammo display render target (its update delegate). */
	UFUNCTION()
	void DrawViewModelDisplay(UCanvas* Canvas, int32 Width, int32 Height);
	/** Sends the Flash value to the weak-point light material, only when it changed. */
	void SetWeakPointFlash(float Value);

	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Parts;

	/** The art description of the model in use (one constant per model in GBGun.cpp); null without a real viewmodel. */
	const FGBGunModelArt* ModelArt = nullptr;
	/** Real viewmodel (only when Definition.ViewModel names a model): body, trigger, magazine, optional energy bar, weak-point light, ammo display. */
	UPROPERTY() TObjectPtr<UStaticMeshComponent> VMBody = nullptr;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> VMTrigger = nullptr;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> VMMagazine = nullptr;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> VMEnergyBar = nullptr;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> VMWeakPointLight = nullptr;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> VMAmmoDisplay = nullptr;
	/** Muzzle flash: two crossed glowing quads at the muzzle tip, shown briefly on each shot. */
	UPROPERTY() TObjectPtr<UStaticMeshComponent> MuzzleFlash = nullptr;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> MuzzleFlashCross = nullptr;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> MuzzleFlashMID = nullptr;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> MuzzleFlashCrossMID = nullptr;
	/** One dynamic instance per material slot of the model parts, so glow and imbue can be set cheaply. */
	UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> ViewModelMIDs;
	/** Instance of the weak-point light's material (parameter Flash); also in ViewModelMIDs. */
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> ViewModelIndicatorMID = nullptr;
	/** Instance of the ammo display's material (parameter DisplayTexture); also in ViewModelMIDs. */
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> ViewModelDisplayMID = nullptr;
	/** Runtime render target the ammo text is drawn into (redrawn only when the text changes). */
	UPROPERTY() TObjectPtr<UCanvasRenderTarget2D> ViewModelDisplayTarget = nullptr;
	/** Text last drawn into the render target (the cache that decides whether a redraw is needed). */
	FString ShownDisplayText;
	/** Seconds since the last weak-point hit. Negative = the light is off. */
	float WeakPointFlashElapsed = -1.f;
	float LastFlashSent = -1.f;
	FDelegateHandle DamageDealtHandle;
	bool bViewModel = false;
	/** Seconds since the last shot started the kick / the muzzle flash. Negative = at rest / hidden. */
	float KickElapsed = -1.f;
	float MuzzleFlashElapsed = -1.f;
	float CurrentKickBackCm = 0.f;
	/** Pinned reload progress for the smoke run; negative = not pinned. */
	float ReloadPreview = -1.f;
	/** The reload pose's contribution to the root right now (location cm, rotation degrees); a cancel blends it to zero. */
	FVector ReloadPoseLoc = FVector::ZeroVector;
	FRotator ReloadPoseRot = FRotator::ZeroRotator;
	FVector ReloadBlendLoc = FVector::ZeroVector;
	FRotator ReloadBlendRot = FRotator::ZeroRotator;
	/** Seconds since a cancelled reload started blending to rest. Negative = no blend. */
	float ReloadBlendElapsed = -1.f;
	/** Seconds since the last shot started the trigger pull. Negative = the trigger is at rest. */
	float TriggerPullElapsed = -1.f;
	/** Last values sent to the model materials (parameters are only set when they change). */
	bool bHasLastGlow = false;
	FLinearColor LastGlowColor = FLinearColor::Black;
	float LastImbueSent = 0.f;
	EGBElement LastGlowElement = EGBElement::Physical;
	bool bLastGlowEnchanted = false;
	bool bEnergyBarBlinking = false;

	FGBAmmoState Ammo;
	bool bReloading = false;
	float ReloadElapsed = 0.f;
	float ReloadDuration = 0.f;

	bool bAiming = false;
	/** 0 = hip, 1 = fully aimed. */
	float AimAlpha = 0.f;
	float HipFOV = 90.f;
};
