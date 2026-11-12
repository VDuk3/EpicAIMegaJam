// A data-driven enemy: orcs of the past (pixel-art sprites), armed orcs of the future (3D), later bosses.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Core/GBTypes.h"
#include "Data/GBDataTypes.h"
#include "GBEnemyCharacter.generated.h"

class UGBStatsComponent;
class UGBAttributeComponent;
class UGBStatusEffectComponent;
class UGBEraComponent;
class UBillboardComponent;
class UStaticMeshComponent;
class UDamageType;

UENUM(BlueprintType)
enum class EGBEnemyState : uint8
{
	Idle,
	Chase,
	Windup,
	Recover,
	Dead
};

DECLARE_MULTICAST_DELEGATE_OneParam(FGBOnEnemyDied, class AGBEnemyCharacter* /*Enemy*/);

/** Data keys used by enemies besides GBStat / GBString. */
namespace GBEnemyKey
{
	// Game.json "Values"
	inline const FName CorpseTime(TEXT("CorpseTime"));                         // seconds the corpse stays after death
	inline const FName MeleeReachTolerance(TEXT("EnemyMeleeReachTolerance"));  // melee hit lands within AttackRange * this
	// Movement around obstacles (navmesh path when it works, local steering otherwise)
	inline const FName NavRetryDelay(TEXT("EnemyNavRetryDelay"));              // s of local steering after a failed or stuck navmesh move
	inline const FName SteerFeelerLength(TEXT("EnemySteerFeelerLength"));      // cm the capsule is swept along each candidate direction
	inline const FName SteerAngleStep(TEXT("EnemySteerAngleStep"));            // degrees between candidate directions
	inline const FName SteerCandidatesPerSide(TEXT("EnemySteerCandidatesPerSide")); // candidates on each side of the straight one
	inline const FName SteerSideHoldTime(TEXT("EnemySteerSideHoldTime"));      // s a chosen side is kept (no flipping at corners)
	inline const FName StuckTime(TEXT("EnemyStuckTime"));                      // s window of the stuck check
	inline const FName StuckDistance(TEXT("EnemyStuckDistance"));              // cm: moved less than this in StuckTime while moving = stuck
	inline const FName GoAroundTime(TEXT("EnemyGoAroundTime"));                // s committed to one side after being stuck
	// Enemies.json "Strings"
	inline const FName ProjectileColor(TEXT("ProjectileColor"));               // "#RRGGBB", 3D projectiles of ranged enemies
}

/**
 * Built from a FGBEnemyDefinition (Enemies.json): stats, attack mode ("Melee" / "Ranged"), look, drops.
 * Simple state machine in C++ (no behavior tree assets): Idle until the player is within AggroRange and
 * present in the same era -> Chase (navmesh path if there is one; local obstacle steering when there is no nav
 * data, the path request fails or the enemy stops making progress) -> Windup (AttackWindup seconds, telegraphed;
 * needs line of sight) -> the hit (melee check in AttackRange, or an AGBProjectile) -> Recover (AttackInterval;
 * a ranged enemy that loses sight of the player moves meanwhile) -> Chase.
 * Slows and other effects work through UGBStatsComponent modifiers (MoveSpeed).
 * Headshots: hits on the top HeadFraction of the body deal the weapon's HeadshotMult (see UGBCombatLibrary).
 * Art rule: bPixelArt enemies are camera-facing sprites; the others are 3D placeholder meshes.
 * Enemies never respawn. On death: drops are rolled and the corpse is removed after a short delay.
 */
UCLASS()
class GLYPHBOUND_API AGBEnemyCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AGBEnemyCharacter();

	/**
	 * Spawns an enemy from Enemies.json at Location (adjusted to stand on the ground), using the definition's
	 * ActorClass or this class. Presence sets in which era it exists. Returns null if the id is unknown.
	 */
	static AGBEnemyCharacter* SpawnEnemy(UWorld* World, FName EnemyId, const FVector& Location, const FRotator& Rotation, EGBEraPresence Presence);

	FName GetEnemyId() const { return EnemyId; }
	const FGBEnemyDefinition* GetDefinition() const;
	EGBEnemyState GetState() const { return State; }
	bool IsDead() const { return State == EGBEnemyState::Dead; }

	UGBStatsComponent* GetStats() const { return Stats; }
	UGBAttributeComponent* GetAttributes() const { return Attributes; }
	UGBStatusEffectComponent* GetStatusEffects() const { return StatusEffects; }
	UGBEraComponent* GetEraComponent() const { return EraComponent; }

	/** True if a world-space hit location is on the head (top HeadFraction of the capsule). */
	bool IsHeadshotLocation(const FVector& HitLocation) const;

	FGBOnEnemyDied OnDied;

	/** One line for logs and the smoke run: how the enemy is moving right now (navmesh or steering, stuck state). */
	FString DescribeMovement() const;

	virtual void Tick(float DeltaSeconds) override;
	/** Dies through the attribute component (so its encounter moves on) instead of being silently destroyed. */
	virtual void FellOutOfWorld(const UDamageType& DamageType) override;

protected:
	virtual void BeginPlay() override;

	/** Applies the definition: stats, capsule size, look. Called by SpawnEnemy before BeginPlay finishes. */
	virtual void InitFromDefinition(const FGBEnemyDefinition& Definition);
	virtual void PerformAttack();
	virtual void HandleDeath(const FGBDamageInfo& KillingBlow);
	void SetState(EGBEnemyState NewState);
	void ApplyMovementStats();
	AActor* FindTarget() const;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void HandleDamaged(const FGBDamageInfo& Info, float ActualDamage);
	void HandlePresenceChanged();
	void HandleDataReloaded();
	void MoveTowards(AActor* Goal, float DeltaSeconds);
	/** Local steering (GBSteering): sweeps the capsule along candidate directions around the one to the goal. */
	void SteerTowards(const AActor* Goal);
	/** A shrunken capsule (lifted by the step height) swept Distance along Direction hits something that blocks pawns. Ignores pawns. */
	bool IsDirectionBlocked(const FVector& Direction, float Distance, const AActor* Goal) const;
	/** Stuck check, once per tick after the state machine: moved less than StuckDistance over StuckTime while moving. */
	void UpdateStuck(float DeltaSeconds);
	void ResetStuck();
	float GetGameValue(FName Key, float DefaultValue) const;
	void StopMoving();
	void FaceTowards(const AActor* Goal);
	bool HasLineOfSight(const AActor* Goal) const;
	FVector GetChestLocation() const;
	bool IsRanged() const;
	/** Windup telegraph: brighter tint (3D) or a slightly bigger sprite (pixel art). */
	void SetTelegraph(bool bOn);

	UPROPERTY(VisibleAnywhere) TObjectPtr<UGBStatsComponent> Stats;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UGBAttributeComponent> Attributes;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UGBStatusEffectComponent> StatusEffects;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UGBEraComponent> EraComponent;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UBillboardComponent> Sprite;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> BodyMesh;

	UPROPERTY() FName EnemyId = NAME_None;
	EGBEnemyState State = EGBEnemyState::Idle;
	float StateTime = 0.f;
	TWeakObjectPtr<AActor> Target;

	bool bInitialized = false;
	/** Took damage: chases even outside AggroRange. */
	bool bProvoked = false;
	bool bPixelArtLook = true;
	FString AttackModeName;
	/** After a failed or stuck navmesh move, steer locally until this reaches 0. */
	float NavRetryTimer = 0.f;

	// Local steering and the stuck check.
	/** This tick the enemy asked to move (Chase, or a ranged enemy repositioning). */
	bool bWantsToMove = false;
	/** Last move was local steering (not a navmesh path). */
	bool bSteering = false;
	/** A ranged enemy moving during Recover to get line of sight again. */
	bool bRepositioning = false;
	/** +1 or -1 (see GBSteering::ChooseOffset); random at spawn so a group does not all go round the same way. */
	int32 SteerSide = 1;
	float SteerSideHoldTimer = 0.f;
	float GoAroundTimer = 0.f;
	float StuckTimer = 0.f;
	FVector StuckOrigin = FVector::ZeroVector;
	/** Totals for DescribeMovement. */
	int32 StuckCount = 0;
	float NavMoveTime = 0.f;
	float SteerMoveTime = 0.f;
	/** Fell below KillZ: no drops (they would fall forever). */
	bool bFellOutOfWorld = false;
	FLinearColor BodyColor = FLinearColor::Gray;
	FVector SpriteBaseScale = FVector::OneVector;
};
