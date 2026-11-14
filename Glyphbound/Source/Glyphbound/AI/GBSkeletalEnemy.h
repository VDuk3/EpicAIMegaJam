// A 3D enemy with a skeletal mesh: the humanoid shooters of the future (robot, armored orcs) and the spider.
// Everything comes from Enemies.json (mesh, armor pieces, weapon, animation clips, effects, tuning stats) and every
// asset may be missing: the capsule then keeps the placeholder look of AGBEnemyCharacter.
//
// AttackMode "Ranged": a squad shooter (see AI/GBSquad): unaware it patrols / guards / lies dormant (the base class); aware it
// asks the squad for a tactical position (cover with a firing spot), runs or walks there, holds it hidden, and when it has an
// attack token steps to its firing spot, telegraphs and fires a burst of hitscan shots with spread and Niagara effects.
// It relocates when the cover is compromised, it stayed too long, it was hurt, the player came too close or its role changed.
// AttackMode "Melee": the parent's chase and bite (a slot round the player), plus an optional lunge (LungeRange stat).
// Animation: UGBEnemyAnimInstance (no AnimBlueprint asset). Death: ragdoll if the mesh has a physics asset, else the
// Death clip, else the parent's behaviour; the weapon falls off.
#pragma once

#include "CoreMinimal.h"
#include "AI/GBEnemyCharacter.h"
#include "AI/GBEnemy3D.h"
#include "AI/GBEnemyAnimInstance.h"
#include "AI/GBSquad.h"
#include "GBSkeletalEnemy.generated.h"

class USkeletalMeshComponent;
class UStaticMeshComponent;

UCLASS()
class GLYPHBOUND_API AGBSkeletalEnemy : public AGBEnemyCharacter
{
	GENERATED_BODY()

public:
	AGBSkeletalEnemy();

	/** What a ranged member is doing between positions. */
	enum class ECombatState : uint8 { Hold, MoveToPosition, Peek };

	/** True when the skeletal mesh loaded and replaced the placeholder. */
	bool HasSkeletalLook() const { return bLookBuilt; }
	UGBEnemyAnimInstance* GetEnemyAnim() const { return AnimInst; }
	virtual bool UsesAwareness() const override { return true; }
	virtual EGBSquadClass GetSquadClass() const override { return IsRanged() ? EGBSquadClass::RangedGround : EGBSquadClass::Melee; }
	virtual bool IsCrouchedPose() const override { return bCrouchPose; }
	virtual bool TryStartMelee() override;
	/** Smoke: a melee enemy in its windup or in a lunge (counted against SquadMaxMeleeAttackers). */
	bool IsMeleeCommitted() const { return State == EGBEnemyState::Windup || bLunging || bLungeQueued; }
	virtual bool CanCrouchInCover() const override { return AnimInst && AnimInst->HasClip(EGBEnemyClip::CrouchIdle); }
	bool IsRagdolling() const { return bRagdoll; }
	bool HasWeapon() const { return WeaponComp != nullptr; }
	/** True when the weapon has a Muzzle (or Tip) socket; otherwise the muzzle is the +X end of its bounds. */
	bool HasMuzzleSocket() const { return bHasMuzzleSocket; }
	/** Where shots leave: the weapon's Muzzle socket, else the +X end of its bounds, else in front of the chest. */
	FVector GetMuzzleLocation() const;
	/** Shots this enemy fired (hitscan), and bursts started. */
	int32 GetShotCount() const { return ShotCount; }
	int32 GetBurstCount() const { return BurstCount; }
	int32 GetLungeCount() const { return LungeCount; }
	/** One line for the smoke run: the look, the clips in use. */
	FString DescribeLook() const;
	virtual FString DescribeTactics() const override;

	// ---- Smoke ----
	ECombatState GetCombatState() const { return CState; }
	/** Seconds spent holding a covered position, and bursts fired from a peek. */
	float GetCoverTime() const { return CoverTime; }
	int32 GetPeekBursts() const { return PeekBursts; }
	int32 GetRelocations() const { return Relocations; }
	/** The largest 2D distance from the pelvis (world) to the capsule centre seen while tracking was on. */
	float GetMaxPelvisDistance() const { return MaxPelvisDistance; }
	/** Horizontal distance from the pelvis bone to the capsule centre right now (-1 without a mesh / pelvis). */
	float GetPelvisDistance() const;
	static bool bTrackPelvis;
	/** Smoke: 0 normal, 1 relaxed (unaware pose), 2 crouched. */
	void DebugForcePosture(int32 Mode) { ForcedPosture = Mode; }
	bool IsBurstActive() const { return BurstPhase != EBurstPhase::Cooldown; }
	bool HasPosition() const { return bHasPosition; }
	const FGBTacticalPosition& GetPosition() const { return Position; }

	virtual void Tick(float DeltaSeconds) override;

protected:
	enum class EMoveIntent : uint8 { None, Advance, Retreat, Strafe };
	enum class EBurstPhase : uint8 { Cooldown, Telegraph, Firing };

	virtual void BeginPlay() override;
	virtual void InitFromDefinition(const FGBEnemyDefinition& Definition) override;
	virtual bool TickCustomBehavior(AActor* Goal, float DeltaSeconds) override;
	virtual void PerformAttack() override;
	virtual void HandleDeath(const FGBDamageInfo& KillingBlow) override;
	virtual void OnDamagedVisual(const FGBDamageInfo& Info, float ActualDamage) override;
	virtual void SetTelegraph(bool bOn) override;
	virtual bool GetMoveGoalOverride(FVector& OutPoint) override;
	virtual void OnReturnedToUnaware() override;
	virtual void OnBecameAware(bool bSource) override;

	void BuildLook();
	float S(const TCHAR* Key, float Default) const { return GBEnemy3D::Stat(this, Key, Default); }
	FGBDamageInfo MakeShotInfo() const;

	// Ranged
	/** The aware behaviour of a ranged member. Virtual: the shield chief has a role of his own. */
	virtual bool TickRanged(AActor* Goal, float DeltaSeconds);
	void TurnTowards(const AActor* Goal, float DeltaSeconds);
	/** Moves towards a point, clamped into the zone; a navmesh path or local steering. */
	void MoveToPoint(const FVector& Point, const AActor* Goal, float DeltaSeconds);
	void StartBurst();
	/** Runs the telegraph and the shots of a burst. bCanAim: it has a line to the player, or aims at the squad's last known position. */
	void TickBurst(AActor* Goal, float DeltaSeconds, bool bCanAim, const FVector& AimPoint);
	void FireShot(AActor* Goal, const FVector& AimPoint, float ExtraSpread);
	/** Close-range swing of a ranged enemy with a MeleeRange stat (the Valkyrie). Returns true while it is busy swinging. */
	bool TickMeleeSwing(AActor* Goal, float DeltaSeconds, float Distance);
	void AbortBurst(float Cooldown);

	// Melee lunge
	void TickLunge(AActor* Goal, float DeltaSeconds);
	void StartLunge(AActor* Goal);

	void UpdateAnimAim(const AActor* Goal, bool bEngaged);
	void StartRagdoll(const FGBDamageInfo& KillingBlow);

	UPROPERTY() TArray<TObjectPtr<USkeletalMeshComponent>> ArmorParts;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> WeaponComp;
	UPROPERTY() TObjectPtr<UGBEnemyAnimInstance> AnimInst;
	UPROPERTY() FGBEnemyShotFx ShotFx;

	bool bLookBuilt = false;
	bool bRagdoll = false;
	bool bHasMuzzleSocket = false;
	FName MuzzleSocketName = NAME_None;
	FString MeshName;

	// Ranged AI
	bool bWasAware = false;
	bool bSight = false;
	EMoveIntent Intent = EMoveIntent::None;
	EBurstPhase BurstPhase = EBurstPhase::Cooldown;
	float BurstTimer = 0.f;
	float BurstCooldown = 0.f;
	int32 ShotsLeft = 0;
	float ShotTimer = 0.f;
	int32 ShotCount = 0;
	int32 BurstCount = 0;
	FRandomStream Random;
	FVector BurstAimPoint = FVector::ZeroVector;
	bool bBurstBlind = false;

	// Tactics
	ECombatState CState = ECombatState::Hold;
	FGBTacticalPosition Position;
	bool bHasPosition = false;
	bool bAwaitingPosition = false;
	float StayTimer = 0.f;
	float CoverCompromisedTimer = 0.f;
	float NoLineTimer = 0.f;
	float SearchTimer = 0.f;
	float RelocateCooldown = 0.f;
	float PeekTimer = 0.f;
	float MoveTimer = 0.f;
	int32 BurstsDone = 0;
	float HealthWindowStart = -1.f;
	float HealthWindowTime = 0.f;
	float CoverTime = 0.f;
	int32 PeekBursts = 0;
	int32 Relocations = 0;
	float MaxPelvisDistance = 0.f;
	bool bCrouchPose = false;
	int32 ForcedPosture = 0;

	// Melee swing of a ranged enemy
	float SwingTimer = -1.f;
	float SwingCooldown = 0.f;

	// Melee lunge
	bool bLungeQueued = false;
	bool bLunging = false;
	bool bLungeHit = false;
	float LungeTime = 0.f;
	float LungeCooldown = 0.f;
	int32 LungeCount = 0;

	float LastHitReactTime = -10.f;
};
