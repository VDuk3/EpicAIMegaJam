// The player: a medieval mage in first person (GDD 0, 3, 9).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Core/GBTypes.h"
#include "Player/GBMantle.h"
#include "QTE/GBQTELogic.h"
#include "GBPlayerCharacter.generated.h"

class UCameraComponent;
class UGBStatsComponent;
class UGBAttributeComponent;
class UGBStatusEffectComponent;
class UGBEquipmentComponent;
class UGBSpellbookComponent;
class UGBQTEComponent;
class UGBInventoryComponent;
class UGBSkillTreeComponent;
class UGBInputSet;
class IGBInteractable;
class UDamageType;
struct FInputActionValue;
struct FGBPlayerSnapshot;
struct FGBDamageInfo;

/**
 * Owns the components that make up the player and routes input to them. It holds no gameplay numbers:
 * movement reads the stats (MoveSpeed, JumpVelocity, MaxJumps, AirControl, GravityScale, DashDistance,
 * DashDuration, DashCooldown, InteractRange) from UGBStatsComponent and re-reads them on OnStatsChanged,
 * so air passives, armor and slows work by adding modifiers.
 *
 * Movement (GDD 9): walk, jump, double jump (MaxJumps), dash (Ctrl / Circle) in the movement direction, no sprint.
 * Ledge climb (mantle): jumping against a low wall climbs it instead (ground press or air grab); first-person
 * animation only (camera dip + pixel hands on the ledge + lowered held item, AGBHUD::DrawHands).
 * During the spell QTE: the camera is locked, W/A/S/D and the mouse go to the QTE, and the player keeps moving
 * in the direction it was moving (the last movement input is held).
 *
 * Jumps commit to their direction (GDD 18): OnJumped sets the horizontal velocity from the movement input held
 * at takeoff (AirControl stays 0, so the air applies no steering). The dash is the only steering in the air.
 */
UCLASS()
class GLYPHBOUND_API AGBPlayerCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AGBPlayerCharacter();

	/** The local player character, or null. */
	static AGBPlayerCharacter* Get(const UObject* WorldContextObject);

	// ---- Components ----
	UCameraComponent* GetCamera() const { return Camera; }
	UGBStatsComponent* GetStats() const { return Stats; }
	UGBAttributeComponent* GetAttributes() const { return Attributes; }
	UGBStatusEffectComponent* GetStatusEffects() const { return StatusEffects; }
	UGBEquipmentComponent* GetEquipment() const { return Equipment; }
	UGBSpellbookComponent* GetSpellbook() const { return Spellbook; }
	UGBQTEComponent* GetQTE() const { return QTE; }
	UGBInventoryComponent* GetInventory() const { return Inventory; }
	UGBSkillTreeComponent* GetSkillTree() const { return SkillTree; }

	/** Held items attach here: a scene component under the camera (right-hand side of the view). */
	USceneComponent* GetHeldItemAnchor() const { return HeldItemAnchor; }

	// ---- Aiming ----
	/** Camera location and forward vector: where shots and spells start and point. */
	FVector GetAimOrigin() const;
	FVector GetAimDirection() const;
	/** A point a little in front of the right hand, for projectiles that should not start inside the camera. */
	FVector GetMuzzleLocation() const;
	/** Kicks the view (degrees). It recovers on its own at RecoverPerSecond. */
	void AddRecoil(float Pitch, float Yaw, float RecoverPerSecond);
	/**
	 * View-only shake of the first-person camera (GDD 18): offsets the camera component's relative
	 * rotation with a decaying shake for Duration seconds, then returns it exactly to zero. The aim
	 * (control rotation) does not move. Calling it again while shaking restarts with the stronger amplitude.
	 */
	void StartViewShake(float Duration, float AmplitudeDeg, float Frequency);
	/**
	 * True while the input action of the loadout slot holding SpellId is currently held
	 * (slot 1 = right click / L1, slots 2-5 = keys 1-4, gamepad L2 + face button).
	 * A caster that is not this player, or a spell that is not in the loadout, counts as
	 * held (then a sustained spell only ends by mana, death, switch or its duration rules).
	 */
	bool IsSpellInputHeld(FName SpellId) const;

	// ---- Movement state ----
	bool IsDashing() const { return bDashing; }
	/** Seconds until the dash is ready again; 0 when ready. */
	float GetDashCooldownRemaining() const;
	/** Multiplies walk speed while set (aiming down sights). 1 = normal. */
	void SetMoveSpeedScale(float Scale);

	// ---- Ledge climb (mantle) ----
	/** A climbable ledge in front of the player: start/end of the climb, its edge and the wall it was found on. */
	struct FGBMantleLedge
	{
		FVector Start = FVector::ZeroVector;
		FVector End = FVector::ZeroVector;
		/** The ledge edge: the wall face XY at the top surface height (what the HUD's climbing hands grip). */
		FVector LedgePoint = FVector::ZeroVector;
		float RiseCm = 0.f;
		FVector2D WallNormal2D = FVector2D::ZeroVector;
	};
	/**
	 * Looks for a climbable ledge in one probe direction (a 2D unit vector): wall sweep along it, top, room and
	 * support, with dot(ProbeDir, -WallNormal2D) >= MantleApproachMinDot. The caller tries each probe direction
	 * from GBMantle::GetProbeDirections in order (intent first, else all around starting with the view).
	 * bFromGround uses the ground limits (MinHeight .. JumpRise + MaxReach), otherwise the air limits.
	 */
	bool FindMantleLedge(FGBMantleLedge& Out, bool bFromGround, const FVector2D& ProbeDir) const;
	/** True while the climb animation runs (movement input, jump, dash and attacks are blocked). */
	bool IsMantling() const { return bMantling; }
	/** 0..1 along the climb; 0 when not mantling. */
	float GetMantleAlpha() const;
	/** The ledge edge being gripped (valid while mantling). */
	FVector GetMantleLedgePoint() const { return MantleLedgePoint; }
	/** View pose weights (zero when not mantling). */
	GBMantle::FMantlePose GetMantlePose() const;
	/** To Falling, everything visual back to rest. Called by anything that takes control away from the player. */
	void CancelMantle();
	/** Jump press: climbs a ledge in front instead when there is one (the smoke run calls this directly). */
	void PressJump();
	/** Test hook for the smoke run (it has no keys): the next PressJump reads this as the held movement input, and a jump takes off in its direction. */
	void SetMoveInputForTest(const FVector2D& Input);

	// ---- Interaction ----
	/** The interactable the player is looking at and can use right now, or null. The HUD shows its prompt. */
	IGBInteractable* GetFocusedInteractable() const;
	AActor* GetFocusedInteractableActor() const { return FocusedInteractable.Get(); }

	// ---- State ----
	bool IsDead() const;
	/** Restores health, mana and ammo (respawn and era travel, GDD 4). Fractions of the maximums (1 = full); bOnlyRaise raises health/mana towards them without lowering. */
	void RestoreAll(float HealthFraction = 1.f, float ManaFraction = 1.f, bool bRefillAmmo = true, bool bOnlyRaise = false);
	/** Stops input-driven actions and hides the held item (death, cutscene). */
	void SetControlEnabled(bool bEnabled);
	/** False while SetControlEnabled(false) (death, era travel) OR a cutscene holds the player. */
	bool IsControlEnabled() const { return bControlEnabled && !bCutsceneLocked; }
	/** The cutscene's own hold on control, separate from SetControlEnabled so a travel finishing mid-caption cannot give control back. */
	void SetCutsceneLocked(bool bLocked);
	/** Releases everything held down (fire, aim, held movement). Called when a menu opens or control is lost. */
	void ReleaseHeldInputs();

	/** Whole-player snapshot: transform, era, attributes, plus every component's part. */
	void SaveToSnapshot(FGBPlayerSnapshot& Snapshot) const;
	void LoadFromSnapshot(const FGBPlayerSnapshot& Snapshot);
	/**
	 * Death respawn at the checkpoint's place and era with full health, mana and ammo. Everything else
	 * owned (items, weapons, spells, skill points, armor) is kept as it is now: dying never rolls it back.
	 */
	void RespawnAt(const FGBPlayerSnapshot& Snapshot);

	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	/**
	 * Below the world's KillZ (set by AGBGameMode from Game.json "KillZ"): the pawn is never destroyed. It is killed
	 * through its attributes (even if invulnerable), so the normal death screen and checkpoint respawn run, and
	 * the fall stops until RespawnAt. Called every frame while below KillZ; only the first call kills.
	 */
	virtual void FellOutOfWorld(const UDamageType& DmgType) override;

	/**
	 * Committed jump takeoff (GDD 18): inside OnJumped, JumpCurrentCount is 1 for a jump from the ground and
	 * 2+ for a jump that starts in the air (the double jump, or the first jump after walking off a ledge:
	 * CheckJumpInput pre-increments the count to 1 while falling, then the successful jump increments it to 2).
	 */
	virtual void OnJumped_Implementation() override;

	/**
	 * Horizontal velocity a committed jump takes off with (pure, tested).
	 * InputDirection is the world-space movement direction held at takeoff (zero when none).
	 * No input: returns CurrentVelocity unchanged (the jump keeps its drift). With input: the input direction
	 * at max(CurrentSpeed, MaxWalkSpeed * MinSpeedMult).
	 */
	static FVector2D ComputeJumpHorizontalVelocity(const FVector2D& CurrentVelocity, const FVector2D& InputDirection, float MaxWalkSpeed, float MinSpeedMult);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Reads the movement stats into the character movement component. Bound to OnStatsChanged. */
	void ApplyMovementStats();
	void HandleDeath(const FGBDamageInfo& KillingBlow);
	/**
	 * Precision Recovery (GDD 18): bound to UGBCombatLibrary::OnDamageDealt. When anything dies from a
	 * weak point killing blow by this player and the perk's cooldown is over, heals the perk's fraction
	 * of max health. Once per death by construction: only a blow that really removed health fires the event.
	 */
	void HandlePrecisionRecovery(AActor* Target, const FGBDamageInfo& Info, float DamageDealt);
	void UpdateFocusedInteractable();
	void HandleDataReloaded();

	/** Stat with modifiers; warns once if the base value is missing from Player.json. */
	float ReadStat(FName Stat, float DefaultValue) const;
	bool CanUseGameplayInput() const;
	bool IsQTEActive() const;
	/** World-space horizontal direction of a 2D movement input (x = right, y = forward) relative to the view yaw. Zero when there is no input. */
	FVector GetMoveInputWorldDirection(const FVector2D& Input) const;
	/** Movement input (x = right, y = forward) relative to the view yaw. */
	void ApplyMoveInput(const FVector2D& Input);
	/** Turns the view (degrees). Positive pitch looks up. */
	void AddLookDelta(float YawDegrees, float PitchDegrees);
	void UpdateWalkSpeed();
	void EndDash();
	void TickRecoil(float DeltaSeconds);
	void TickViewShake(float DeltaSeconds);
	/** Sets the camera rotation (shake + mantle) and location, and the held item anchor, from the current pose. */
	void ApplyCameraPose();
	/** Mantle tunables from Player.json (read with modifiers and missing-stat warnings like every movement stat). */
	GBMantle::FMantleParams GetMantleParams() const;
	/** True when a mantle may start or continue: alive, enabled, free (no QTE / dash / freeze / mantle). */
	bool CanMantleNow() const;
	/** Starts the climb when a ledge is found (ground or air limits); false when there is none. */
	bool TryStartMantle(bool bFromGround);
	void StartMantle(const FGBMantleLedge& Ledge);
	void TickMantle(float DeltaSeconds);

	// Input handlers (bound to the actions of UGBInputSet)
	void InputMove(const FInputActionValue& Value);
	void InputMoveCompleted(const FInputActionValue& Value);
	void InputLook(const FInputActionValue& Value);
	void InputLookStick(const FInputActionValue& Value);
	/** Gamepad face button: QTE key, L2 + face = spell slots 2-5 with the staff, or jump / dash / reload / interact. */
	void InputFace(EGBQTEKey Key);
	void InputJump();
	void InputJumpReleased();
	void InputDash();
	void InputPrimaryPressed();
	void InputPrimaryReleased();
	void InputSecondaryPressed();
	void InputSecondaryReleased();
	void InputReload();
	void InputSwitchWeapon(const FInputActionValue& Value);
	void InputSpellSlot(int32 Slot);
	void InputEnchant();
	void InputInteract();
	void InputQuickUse();
	void InputCycleQuickUse(const FInputActionValue& Value);
	void InputQTEKey(int32 KeyIndex);
	/** Q / L2: raise / lower the summoned shield (UGBSpellbookComponent::SetBlockHeld). */
	void InputBlockPressed();
	void InputBlockReleased();
	/** True while the summoned shield is raised: attacks and spells are ignored. */
	bool IsBlocking() const;
	/** Stops the fall after a KillZ death (movement off) and turns movement back on at respawn. */
	void FreezeMovement();
	void UnfreezeMovement();

	UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> Camera;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> HeldItemAnchor;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UGBStatsComponent> Stats;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UGBAttributeComponent> Attributes;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UGBStatusEffectComponent> StatusEffects;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UGBEquipmentComponent> Equipment;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UGBSpellbookComponent> Spellbook;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UGBQTEComponent> QTE;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UGBInventoryComponent> Inventory;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UGBSkillTreeComponent> SkillTree;

	/** Input actions and mapping contexts, built in code (see Player/GBInputSet.h). */
	UPROPERTY() TObjectPtr<UGBInputSet> InputSet;

	TWeakObjectPtr<AActor> FocusedInteractable;

	bool bControlEnabled = true;
	bool bCutsceneLocked = false;
	/** Movement turned off by a KillZ death, until RespawnAt / LoadFromSnapshot. */
	bool bMovementFrozen = false;
	bool bDashing = false;
	float DashTimeLeft = 0.f;
	double DashReadyTime = 0.0;
	FVector DashVelocity = FVector::ZeroVector;
	float MoveSpeedScale = 1.f;
	/** Last movement input (x = right, y = forward), held during the QTE. */
	FVector2D LastMoveInput = FVector2D::ZeroVector;
	/** Frame of the last Move event outside the QTE; LastMoveInput is cleared when it gets stale. */
	uint64 LastMoveInputFrame = 0;
	/** Set by SetMoveInputForTest, consumed by the next PressJump. */
	TOptional<FVector2D> MoveInputTestOverride;
	/**
	 * The movement input held right now (x = right, y = forward), read from the Move action's current value.
	 * The jump's Started event fires before the Move event of the same frame, so LastMoveInput can be a frame old
	 * (pressing W and jump together would read no direction). Falls back to LastMoveInput without Enhanced Input.
	 */
	FVector2D GetLiveMoveInput() const;
	/** Secondary (right click / L2) is held: on gamepad with the staff, L2 + face button casts slots 2-5. */
	bool bSecondaryHeld = false;
	/** World time of the last Precision Recovery heal (a stamp, not a timer: no reset on death). */
	double LastPrecisionRecoveryTime = -1000000.0;
	FDelegateHandle DamageDealtHandle;
	FDelegateHandle DataReloadedHandle;
	/** Recoil still to be recovered (degrees) and its recovery speed. */
	FRotator PendingRecoil = FRotator::ZeroRotator;
	float RecoilRecoverSpeed = 0.f;
	/** View shake (visual only): time left / total, initial amplitude (degrees) and frequency (Hz). */
	float ViewShakeTimeLeft = 0.f;
	float ViewShakeDuration = 0.f;
	float ViewShakeAmplitudeDeg = 0.f;
	float ViewShakeFrequency = 0.f;
	/** Current view shake rotation offset (degrees); ApplyCameraPose combines it with the mantle dip. */
	FRotator ViewShakeRot = FRotator::ZeroRotator;
	// ---- Ledge climb (mantle) ----
	bool bMantling = false;
	float MantleElapsed = 0.f;
	float MantleDuration = 0.f;
	float MantleRiseShare = 0.6f;
	FVector MantleStart = FVector::ZeroVector;
	FVector MantleEnd = FVector::ZeroVector;
	FVector MantleLedgePoint = FVector::ZeroVector;
	/** Signed yaw the view turns to face the wall during the climb (0 = the wall was already in front). */
	float MantleTotalTurn = 0.f;
	/** TurnFraction at the last mantle tick: each tick adds the delta times the total turn to the view yaw. */
	float MantlePrevTurnFraction = 0.f;
	/** Jump count when the climb started: restored if the climb is cancelled (releasing jump while Flying clears it). */
	int32 MantleSavedJumpCount = 0;
};
