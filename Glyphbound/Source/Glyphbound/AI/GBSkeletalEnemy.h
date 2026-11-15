// A 3D enemy with a skeletal mesh: the humanoid shooters of the future (robot, armored orcs) and the spider.
// Everything comes from Enemies.json (mesh, armor pieces, weapon, animation clips, effects, tuning stats) and every
// asset may be missing: the capsule then keeps the placeholder look of AGBEnemyCharacter.
//
// AttackMode "Ranged": a squad shooter (see AI/GBSquad): unaware it patrols / guards / lies dormant (the base class); aware it
// asks the squad for a tactical position (cover with a firing spot), runs or walks there, holds it hidden, and when it has an
// attack token steps to its firing spot, telegraphs and fires a burst of hitscan shots with spread and Niagara effects.
// It relocates when the cover is compromised, it stayed too long, it was hurt, the player came too close or its role changed.
// AttackMode "Melee": the parent's chase and bite (a slot round the player), plus an optional lunge (LungeRange stat).
// Two looks, chosen by data: the skeletal mesh (UGBEnemyAnimInstance, no AnimBlueprint asset) or, with Strings SpriteSheet,
// the castle's 8-direction pixel-art sheet (UGBSpriteAnimComponent); the AI is the same, the Look* calls branch between them.
// Animation of the mesh: UGBEnemyAnimInstance. Death: never a ragdoll. The enemy's own Death clip plays once and holds its
// last frame, the body is eased down to the floor, the weapon falls from the hand a moment in (when its mesh has collision), the body lies for CorpseTime and sinks into the floor.
#pragma once

#include "CoreMinimal.h"
#include "AI/GBEnemyCharacter.h"
#include "AI/GBEnemy3D.h"
#include "AI/GBEnemyAnimInstance.h"
#include "AI/GBSquad.h"
#include "GBSkeletalEnemy.generated.h"

class USkeletalMeshComponent;
class UStaticMeshComponent;
class UBillboardComponent;
class UGBSpriteAnimComponent;

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
	/** The sprite look (Strings SpriteSheet, the castle's pixel-art characters): the sheet loaded and replaced the static sprite. */
	bool HasSpriteLook() const { return bSpriteLook; }
	UGBSpriteAnimComponent* GetSpriteAnim() const { return SpriteAnim; }
	/** Strings ShotType "Projectile": ranged shots are visible AGBProjectiles (arrows, arcane bolts), not hitscan. */
	bool FiresProjectiles() const { return bProjectileShots; }
	int32 GetProjectileCount() const { return ProjectileCount; }
	/** Projectile shots held because the world blocked their way. */
	int32 GetBlockedShots() const { return BlockedShots; }
	/** Smoke: melee blows that reached the damage check (a swing at its hit frame), and slashes shown. */
	int32 GetMeleeSwingCount() const { return MeleeSwingCount; }
	virtual bool UsesAwareness() const override { return true; }
	virtual EGBSquadClass GetSquadClass() const override { return IsRanged() ? EGBSquadClass::RangedGround : EGBSquadClass::Melee; }
	virtual bool IsCrouchedPose() const override { return bCrouchPose; }
	virtual bool TryStartMelee() override;
	/** Smoke: a melee enemy in its windup or in a lunge (counted against SquadMaxMeleeAttackers). */
	bool IsMeleeCommitted() const { return State == EGBEnemyState::Windup || bLunging || bLungeQueued; }
	virtual bool CanCrouchInCover() const override { return AnimInst && AnimInst->HasClip(EGBEnemyClip::CrouchIdle); }
	/** Smoke: the death clip is playing or holding its last frame. */
	bool IsDeathClipPlaying() const;
	float GetDeathClipLength() const { return DeathClipLength; }
	float GetDeathElapsed() const { return DeathElapsed; }
	/** Smoke: seconds from the death to the removal of the corpse: the clip, the lie (CorpseTime), the sinking. */
	float GetDeathTotalSeconds() const { return DeathClipLength + LieTime + SinkTime; }
	/** Smoke: height of the pelvis bone over the floor under it (negative = under it). PelvisNoBone without a mesh or a pelvis bone, PelvisNoFloor with the bone but no floor under it. */
	static constexpr float PelvisNoBone = -10000.f;
	static constexpr float PelvisNoFloor = -10001.f;
	float GetPelvisHeightAboveFloor() const;
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
	/** Death: the fall to the floor, the weapon, the lie and the sinking. Runs from Tick while dead. */
	void TickDeath(float DeltaSeconds);
	/** A moment into the death clip: the weapon (and the chief's shield) fall from the hand. */
	virtual void OnWeaponDropTime();
	/** Detaches a static mesh piece and lets it fall as a simulated body. False (it stays where it is) when its mesh has no simple collision. */
	bool DropStaticPiece(UStaticMeshComponent* Piece, const FVector& Impulse);

	// ---- The look: the few calls the AI makes, each branching once between the skeletal mesh (AnimInst) and the sprite sheet
	// (SpriteAnim). Protected so subclasses (the Valkyrie) drive their look the same way. Clip names are the sheet's.
	/** Builds the sprite look from Strings SpriteSheet. False when there is none or it failed (the static sprite stays). */
	bool BuildSpriteLook(const FGBEnemyDefinition& Def);
	/** A windup started: the attack clip is timed so its hit frame lands HitInSeconds from now (sprite); nothing on the mesh (it plays at the blow). */
	void LookStartAttack(FName Clip, float HitInSeconds);
	/** The windup ended without the blow (an interrupt, a lost line): the clip stops. */
	void LookCancelAttack();
	/** The blow lands now (melee damage check, a lunge, the swing of a ranged enemy). Clip: the mesh's one-shot / the sprite's fallback when no windup played it. */
	void LookAttackNow(FName Clip);
	void LookNotifyShot();
	void LookNotifyHit(float Side);
	/** Plays the death clip; returns its length (0 without one). */
	float LookPlayDeath();
	void LookSetAim(float PitchDegrees);
	/** The clip names of this enemy's attacks (Strings Clip.Attack / Clip.Shoot / Clip.Leap / Clip.Swing). */
	FName GetLookClip(const TCHAR* Key, const TCHAR* Default) const;
	/**
	 * A visible projectile shot (ShotType Projectile) from the shoot clip's Origin. It is not loosed when the world blocks its
	 * first stretch (a box it peeks over, its own cover while it suppresses): then bShotBlocked asks for a new position.
	 */
	void FireProjectile(AActor* Goal, const FVector& AimPoint, float ExtraSpread);
	/** The short pixel slash of a melee blow, in front of the enemy towards Goal. */
	void ShowSlash(const AActor* Goal);

	UPROPERTY() TObjectPtr<UGBSpriteAnimComponent> SpriteAnim;
	UPROPERTY() TObjectPtr<UBillboardComponent> SlashSprite;
	bool bSpriteLook = false;
	bool bProjectileShots = false;
	/** The windup's blow was released (the clip is not cancelled when the windup ends). */
	bool bAttackReleased = false;
	bool bAlternateAttack = false;
	bool bAwareOnSpawnChecked = false;
	/** A projectile shot was held because the world blocked it: relocate (cleared when the new position is asked for). */
	bool bShotBlocked = false;
	float SlashTimer = 0.f;
	int32 ProjectileCount = 0;
	int32 BlockedShots = 0;
	int32 MeleeSwingCount = 0;

	UPROPERTY() TArray<TObjectPtr<USkeletalMeshComponent>> ArmorParts;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> WeaponComp;
	UPROPERTY() TObjectPtr<UGBEnemyAnimInstance> AnimInst;
	UPROPERTY() FGBEnemyShotFx ShotFx;

	bool bLookBuilt = false;
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

	// Death
	enum class EDeathFall : uint8 { None, Ease, Gravity };
	bool bDying = false;
	EDeathFall DeathFall = EDeathFall::None;
	FVector DeathHitDirection = FVector::ForwardVector;
	float DeathElapsed = 0.f;
	float DeathClipLength = 0.f;
	float LieTime = 0.f;
	float SinkTime = 0.f;
	float SinkDepth = 0.f;
	float SinkElapsed = 0.f;
	float DeathFloorTargetZ = 0.f;     // actor Z with the capsule's bottom on the floor
	float DeathFallStartZ = 0.f;
	float DeathFallSpeed = 0.f;
	bool bDeathHasFloor = false;
	float WeaponDropTimer = -1.f;      // seconds until the weapon falls; < 0 = nothing pending
};
