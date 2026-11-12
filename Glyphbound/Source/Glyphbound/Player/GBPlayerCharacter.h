// The player: a medieval mage in first person (GDD 0, 3, 9).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Core/GBTypes.h"
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

	// ---- Movement state ----
	bool IsDashing() const { return bDashing; }
	/** Seconds until the dash is ready again; 0 when ready. */
	float GetDashCooldownRemaining() const;
	/** Multiplies walk speed while set (aiming down sights). 1 = normal. */
	void SetMoveSpeedScale(float Scale);

	// ---- Interaction ----
	/** The interactable the player is looking at and can use right now, or null. The HUD shows its prompt. */
	IGBInteractable* GetFocusedInteractable() const;
	AActor* GetFocusedInteractableActor() const { return FocusedInteractable.Get(); }

	// ---- State ----
	bool IsDead() const;
	/** Full health, mana and ammo (era travel, GDD 4). */
	void RestoreAll();
	/** Stops input-driven actions and hides the held item (death, cutscene). */
	void SetControlEnabled(bool bEnabled);
	bool IsControlEnabled() const { return bControlEnabled; }
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
	/** Secondary (right click / L2) is held: on gamepad with the staff, L2 + face button casts slots 2-5. */
	bool bSecondaryHeld = false;
	FDelegateHandle DataReloadedHandle;
	/** Recoil still to be recovered (degrees) and its recovery speed. */
	FRotator PendingRecoil = FRotator::ZeroRotator;
	float RecoilRecoverSpeed = 0.f;
};
