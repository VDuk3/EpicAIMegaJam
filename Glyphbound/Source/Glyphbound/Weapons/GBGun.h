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

/** What the reload animation shows this frame: the same record for a magazine reload and for one round of a round-by-round reload. */
struct FGBReloadLook
{
	bool bActive = false;
	float Pose = 0.f;   // 0..1 how far the gun is in its reload pose
	float Dip = 0.f;    // 0..1 the dip as a magazine leaves
	float Bump = 0.f;   // 0..1 the seat bump
	float Drop = 0.f;   // 0..1 how far down the magazine / round (and the left hand) is
	float Slide = 0.f;  // 0..1 along the slide path (1 = fully out)
	bool bShowMag = true;
};

/**
 * Pure stepping of a round-by-round reload (the shotgun): no world, no actor, tested in Tests/GBWeaponsTests.cpp.
 * One round takes CycleSeconds (the gun's reload time over its magazine size). The round moves from the reserve into
 * the gun only when its cycle ENDS, so a reload stopped part-way loses nothing: the round in flight is still in the
 * reserve. When a cycle ends and the gun is not full and the reserve is not empty, the next cycle starts at once.
 */
struct FGBPerRoundReload
{
	float CycleSeconds = 0.f;
	/** Seconds into the current cycle. */
	float CycleElapsed = 0.f;
	/** Rounds moved so far. */
	int32 Moved = 0;
	/** Rounds this reload set out to load (the smaller of the empty space and the reserve). */
	int32 Planned = 0;
	bool bRunning = false;

	/** Starts a reload; one round takes TotalSeconds / Capacity. Not running at all when there is nothing to load. */
	void Begin(int32 Rounds, int32 Reserve, int32 Capacity, float TotalSeconds);
	/** Adds time and moves every round whose cycle ended (a long step moves several, none of the time is lost). Returns how many moved. */
	int32 Advance(float DeltaSeconds, int32& Rounds, int32& Reserve, int32 Capacity, int32 MaxRounds = MAX_int32);
	/** Stops part-way: the rounds already moved stay in the gun, the round in flight stays in the reserve. */
	void Stop();
	/** 0 to 1 over the whole reload (the rounds it set out to load). */
	float Progress() const;
	/** 0 to 1 inside the current round's cycle. */
	float CycleProgress() const;
};

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

	virtual void StartPrimary() override;
	virtual void StopPrimary() override { ReleaseTrigger(); } // a pending shot on an empty gun survives a tap
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
	virtual bool ReloadsPerRound() const override;
	virtual float GetReloadProgress() const override;

	int32 GetMagazineSize() const;
	/** Spread half-angle in degrees right now: hip or aim, plus the moving bonus (GDD 4). Shots and crosshair use it. */
	float GetCurrentSpread() const;
	/** Largest reserve: spare magazines, or spare ROUNDS (magazines x magazine size) for a round-by-round gun. */
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
	/** Whether the model has a pump (a forend that slides back after each shot). */
	bool HasPump() const;
	/** Distance in cm of the pump from its rest location, in the body's frame. */
	float GetViewModelPumpOffsetCm() const;
	/** 0 (rest) to 1 (back of its travel): how far the pump is, pinned or real. */
	float GetPumpAmount() const { return CurrentPumpAmount; }
	/** Per-model art value for the HUD: sprite pixels (x right, y down) the left hand rides with the pump at full travel, already scaled by the pump amount. */
	FVector2D GetPumpHandShift() const;
	/** Dev only (smoke run): pins the pump at this amount (0 rest, 1 back of its travel). */
	void SetPumpPreview(float Amount);
	/** Dev only: releases the pin; the pump goes back to rest. */
	void ClearPumpPreview();
	/** Whether the magazine mesh is showing (it is hidden while the old one is gone and the new one has not arrived). */
	bool IsViewModelMagazineVisible() const;
	/** Distance in cm of the viewmodel root from its rest location (reload pose and shot kick both move it). */
	float GetViewModelRootOffsetCm() const;
	/** Angle in degrees between the viewmodel root's rotation and its rest rotation. */
	float GetViewModelRootAngleDeg() const;
	/** Per-model art value for the HUD: how far (sprite pixels, x right, y down) the left hand moves to meet the magazine. */
	FVector2D GetReloadHandReach() const;
	/** The left hand's reload beat for the HUD (zero when no reload animation runs). Same numbers as GBReloadAnim::HandBeat for a magazine gun; a round-by-round gun repeats it per round. */
	GBReloadAnim::FHandBeat GetReloadHandBeat() const;
	/** 0..1 how far the gun is in its reload pose (what the right hand follows). */
	float GetReloadPoseAmount() const;
	/** Per-model art value for the HUD: how far (sprite pixels) the right hand moves at full pose so it stays on the grip. */
	FVector2D GetReloadRightHandFollow() const;
	/** Progress (0 to 1) the reload animation is showing: the pinned preview if any, else the real reload; -1 when none. For a round-by-round gun it is the progress inside ONE round's cycle. */
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
	/** A round-by-round reload ended by itself (gun full or reserve empty): the pose eases out. */
	void FinishPerRoundReload();
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
	void BeginReloadBlend(float Seconds);
	/** Pose, magazine/shell beat and visibility for this frame (one code path for both kinds of reload). */
	FGBReloadLook ComputeReloadLook() const;
	/** Starts the pump after a shot. */
	void StartPump();
	/** Pump at rest at once (a weapon switch, or a reload begins). */
	void ResetPump();
	/** Places the pump from its timer or its pin. */
	void ApplyPump();
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
	UPROPERTY() TObjectPtr<UStaticMeshComponent> VMPump = nullptr;
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
	/** Magazine capacity the display was last drawn for (its pips depend on it). */
	int32 ShownDisplayCapacity = -1;
	/** Magazine capacity seen on the last tick. Perks can change it while the game runs (Extended Mag, Extended Tube). */
	int32 LastKnownCapacity = -1;
	/** The one rule for ammo against the current capacity: loaded rounds above it go back to a round-by-round gun's
	 *  reserve (a magazine gun cannot store loose rounds), then the reserve is capped. Used by SetAmmo and on a change. */
	void ClampAmmoToCapacity(FGBAmmoState& InOut) const;
	/** Called every tick: when the capacity changed (a perk bought or reset) the reload stops and the ammo is clamped. */
	void ReconcileCapacity();
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
	float ReloadBlendPoseAmount = 0.f;
	FRotator ReloadBlendRot = FRotator::ZeroRotator;
	/** Seconds since a cancelled reload started blending to rest. Negative = no blend. */
	float ReloadBlendElapsed = -1.f;
	float ReloadBlendLength = 0.12f;
	/** Reload pose amount (0..1) the hands follow; a round-by-round gun keeps easing it out through the blend. */
	float CurrentPoseAmount = 0.f;
	/** Seconds since the last shot (pump timer); negative = the pump is at rest. */
	float PumpElapsed = -1.f;
	/** Pinned pump amount for the smoke run; negative = not pinned. */
	float PumpPreview = -1.f;
	float CurrentPumpAmount = 0.f;
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
	/** Stepping of a round-by-round reload (only while bReloading and the gun has the ReloadPerRound stat). */
	FGBPerRoundReload PerRound;
	/** Fire was pressed on an empty round-by-round gun during its reload: shoot once the first shell is in. */
	bool bFireWhenLoaded = false;

	bool bAiming = false;
	/** 0 = hip, 1 = fully aimed. */
	float AimAlpha = 0.f;
	float HipFOV = 90.f;
};
