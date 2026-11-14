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
class UGBElementalStatusComponent;
class UGBEraComponent;
class UBillboardComponent;
class UStaticMeshComponent;
class UDamageType;
class AGBEncounter;
class AAIController;

UENUM(BlueprintType)
enum class EGBEnemyState : uint8
{
	Idle,
	Chase,
	Windup,
	Recover,
	Dead
};

/** Which part of the squad logic an enemy belongs to. */
enum class EGBSquadClass : uint8
{
	RangedGround,
	Melee,
	Flyer
};

/** What an unaware enemy of an encounter does. */
UENUM(BlueprintType)
enum class EGBUnawareBehavior : uint8
{
	/** Walks a loop of two to four points inside its zone, pausing at each. */
	Patrol,
	/** Stands at its post and looks around. */
	Guard,
	/** Does not move or look; wakes on damage, an alert or the player coming close. */
	Dormant
};

/** The three speeds of a walker: unaware, in combat (repositioning, peeking), running (far moves, flanking, falling back). */
enum class EGBGait : uint8
{
	Patrol,
	Combat,
	Run
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
	inline const FName SteerLedgeProbeDistance(TEXT("EnemySteerLedgeProbeDistance")); // cm ahead to check for ground (no walking off ledges)
	inline const FName SteerLedgeDrop(TEXT("EnemySteerLedgeDrop"));            // cm down searched for ground ahead
	inline const FName StuckTime(TEXT("EnemyStuckTime"));                      // s window of the stuck check
	inline const FName StuckDistance(TEXT("EnemyStuckDistance"));              // cm: moved less than this in StuckTime while moving = stuck
	inline const FName GoAroundTime(TEXT("EnemyGoAroundTime"));                // s committed to one side after being stuck
	inline const FName MeleeAcceptanceFraction(TEXT("EnemyMeleeAcceptanceFraction")); // melee navmesh stops within AttackRange * this
	inline const FName ProjectileRangeMult(TEXT("EnemyProjectileRangeMult"));         // ranged shots fly AggroRange * this
	inline const FName AimHeightFraction(TEXT("EnemyAimHeightFraction"));             // of the capsule half height, above the center, where shots start
	inline const FName StuckSpeedFraction(TEXT("EnemyStuckSpeedFraction"));           // a slowed enemy is not stuck: the stuck distance never exceeds this share of its possible travel
	inline const FName SteerSweepRadiusScale(TEXT("EnemySteerSweepRadiusScale"));      // the steering sweep capsule is a bit thinner, so a wall it slides along is not "ahead"
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
	UGBElementalStatusComponent* GetElementalStatus() const { return ElementalStatus; }
	UGBEraComponent* GetEraComponent() const { return EraComponent; }

	/**
	 * The point area damage, blasts and knockback measure to, seen from FromPoint. Default: the actor location.
	 * The Entity returns the nearest point of its floating shell, so a blast at the shell hurts it.
	 */
	virtual FVector GetDamageTargetLocation(const FVector& FromPoint) const { (void)FromPoint; return GetActorLocation(); }

	/** True if a world-space hit location is on the head (top HeadFraction of the capsule). */
	virtual bool IsHeadshotLocation(const FVector& HitLocation) const;

	/** Seconds of control lock left (stun / freeze debug view). */
	float GetControlLockTime() const { return ControlLockTime; }
	void InterruptAttack();
	virtual void ApplyControlLock(float Seconds);
	/** Another enemy noticed the player: this one is provoked too (chases or engages even outside its aggro range). */
	void Alert() { if (State != EGBEnemyState::Dead && !bSealed) { bProvoked = true; } }

	/** Sealed (an encounter's WakeFlag is not set yet): notices nothing, cannot be hurt, does not move or turn. */
	void SetSealed(bool bInSealed);
	bool IsSealed() const { return bSealed; }
	/** Wakes a sealed enemy: it notices the player (the alert beat) and then leaps to Target (a launch from its spot, landing on Target). */
	void BeginWake(const FVector& WakePoint);
	/** Puts a point on the navmesh when there is one (the wake leap must land on it). */
	void ProjectPointToNavigationForWake(FVector& InOutPoint) const { ProjectToNav(InOutPoint); }
	bool IsWakeLeaping() const { return bPendingWakeLeap || bWakeLeaping; }

	// ---- Zone, awareness, squad (enemies that UsesAwareness) ----
	/** Gives the enemy to an encounter: its zone (it never leaves it), its post and what it does while unaware. */
	void SetEncounterContext(AGBEncounter* InEncounter, const FBox& Zone, const FVector& Post, EGBUnawareBehavior Behavior, const TArray<FVector>& RouteCandidates);
	AGBEncounter* GetEncounter() const { return Encounter.Get(); }
	bool HasHomeZone() const { return bHasHomeZone; }
	const FBox& GetHomeZone() const { return HomeZone; }
	/** The point itself when the enemy has no zone. */
	FVector ClampToZone(const FVector& Point) const;
	bool IsInsideZone(const FVector& Point) const;
	float StatValue(const TCHAR* Key, float Default) const { return StatOr(Key, Default); }
	float StatValue(const ANSICHAR* Key, float Default) const { return StatOr(ANSI_TO_TCHAR(Key), Default); }
	virtual EGBSquadClass GetSquadClass() const { return EGBSquadClass::Melee; }
	/** 0..1: how much the enemy pushes (flanks close, stays short in cover). */
	float GetAggression() const { return StatOr("Aggression", 0.5f); }
	/** The shield chief: the squad's base of fire, never takes cover. */
	virtual bool IsSquadAnchor() const { return false; }
	virtual bool CanCrouchInCover() const { return false; }
	/** A melee enemy that may always be inside attack range (stat MeleeAlwaysAttacks). */
	bool AlwaysMeleeAttacks() const { return StatOr("MeleeAlwaysAttacks", 0.f) > 0.5f; }
	/** One short text for the TACTICS log line. */
	virtual FString DescribeTactics() const { return FString(); }
	/** True once the enemy has noticed the player (or was told). Enemies that do not use awareness count as aware. */
	bool IsAware() const { return bAware || !UsesAwareness(); }
	/** Smoke / squad: it has noticed and has finished its alert beat. */
	bool IsReadyToFight() const { return bAware && AlertTimer <= 0.f; }
	EGBUnawareBehavior GetUnawareBehavior() const { return UnawareBehavior; }
	/** Eye / chest height above the feet: one source for the cover query and for real sight (standing and crouched). */
	float GetEyeHeightAboveFoot(bool bCrouched) const;
	/** How many times an enemy found outside its zone was put back (smoke). */
	int32 GetZoneClamps() const { return ZoneClamps; }
	bool IsReturningToPost() const { return bReturningToPost; }
	/** Becomes aware at once, without a beat (a reinforcement that arrives running). */
	void BecomeAwareNow();
	/** The squad told it: becomes aware after Delay seconds. */
	void ScheduleAlert(float Delay);
	/** A shot was fired at Location: noticed when closer than its HearingRange. */
	void HearNoise(const FVector& Location);
	/** Back to its post and patrol (nobody has seen the player for a while). */
	void ReturnToUnaware();
	/** Total path distance walked while unaware and the route points (smoke). */
	float GetUnawareTravel() const { return UnawareTravel; }
	int32 GetRouteNum() const { return PatrolRoute.Num(); }
	FVector GetPost() const { return PostLocation; }
	bool WasEverUnawareMoving() const { return UnawareTravel > 1.f; }
	/** This many seconds the enemy has been in view of the player or heard a shot (smoke). */
	virtual bool UsesAwareness() const { return false; }

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
	/**
	 * Called once per tick, after the control lock and the "no target" checks, before the Idle / Chase / Windup / Recover
	 * flow. A subclass with its own behaviour (skeletal shooters, flyers) returns true to replace that flow for this tick.
	 */
	virtual bool TickCustomBehavior(AActor* Goal, float DeltaSeconds) { (void)Goal; (void)DeltaSeconds; return false; }
	virtual void HandleDeath(const FGBDamageInfo& KillingBlow);
	void SetState(EGBEnemyState NewState);
	virtual void ApplyMovementStats();
	/** Speed of a gait: stats PatrolSpeed / CombatSpeed / MoveSpeed with the MoveSpeed modifiers (slows) applied to all three. */
	float GetGaitSpeed(EGBGait Gait) const;
	void SetGait(EGBGait Gait);
	EGBGait GetGait() const { return Gait; }
	float StatOr(const TCHAR* Key, float Default) const;
	float StatOr(const ANSICHAR* Key, float Default) const { return StatOr(ANSI_TO_TCHAR(Key), Default); }
	/** Turns the actor towards a yaw at a stat TurnRate (deg/s) without snapping. */
	void TurnYawTowards(float TargetYaw, float DeltaSeconds, float RateDegreesPerSecond);
	/** Sight check cached every SightCacheInterval (the player in view of the enemy's chest). */
	void UpdateSightCache(const AActor* Goal, float DeltaSeconds);
	/** The part of Tick that decides between unaware, the alert beat and acting. True: run the aware behaviour this tick. */
	bool TickAwareness(AActor* Goal, float DeltaSeconds);
	virtual void TickUnaware(AActor* Goal, float DeltaSeconds);
	/** Called once when the enemy turns aware (the squad is told by the source, the encounter starts). */
	virtual void OnBecameAware(bool bSource);
	virtual void OnReturnedToUnaware() {}
	/** Notice the player now (sight, noise, damage) or when told by the squad (bSource false). */
	void NoticePlayer(bool bSource);
	/** The pose the sight origin follows (a crouched enemy sees from lower). */
	virtual bool IsCrouchedPose() const { return false; }
	/** A melee enemy with a slot round the player may only start an attack when the squad lets it. */
	virtual bool TryStartMelee() { return true; }
	/** Ground enemies walk patrol points on the navmesh; flyers fly their loop. */
	virtual bool PatrolsOnNavMesh() const { return true; }
	bool ProjectToNav(FVector& InOutPoint) const;
	/** True when the AI controller's current path has no point outside the zone (grown by a small tolerance). */
	bool PathStaysInZone(const AAIController* AI) const;
	/** The wake leap: a launch to a valid landing on the navmesh (capsule fits, arc clear), else a walk out (a short put-forward). */
	void LaunchWakeLeap();
	/** Builds the patrol loop (once, lazily) from the post and the encounter's other spawn points, inside the zone. */
	void BuildPatrolRoute();
	/** Point goal of melee ground enemies (a slot round the player): false = chase the player actor. */
	virtual bool GetMoveGoalOverride(FVector& OutPoint) { (void)OutPoint; return false; }
	AActor* FindTarget() const;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void HandleDamaged(const FGBDamageInfo& Info, float ActualDamage);
	/** Subclass visual reaction to a landed hit (the Entity ripples its tentacles and flashes the eye). */
	virtual void OnDamagedVisual(const FGBDamageInfo& Info, float ActualDamage) { (void)Info; (void)ActualDamage; }
	void HandlePresenceChanged();
	void HandleDataReloaded();
	void HandleElementalStatusStarted(EGBElement Element, FName Reaction, float Duration);
	void MoveTowards(AActor* Goal, float DeltaSeconds);
	/** Moves to a point: a navmesh path (re-requested every NavRepathInterval) or local steering. Clamped into the zone. */
	void MoveToZonePoint(const FVector& Point, float Acceptance, const AActor* IgnoreActor, float DeltaSeconds);
	/** Local steering (GBSteering): sweeps the capsule along candidate directions around the one to the goal. */
	void SteerTowards(const AActor* Goal);
	void SteerTowardsPoint(const FVector& Point, const AActor* IgnoreActor);
	/** A shrunken capsule (lifted by the step height) swept Distance along Direction hits something that blocks pawns. Ignores pawns. */
	bool IsDirectionBlocked(const FVector& Direction, float Distance, const AActor* Goal) const;
	/** Stuck check, once per tick after the state machine: moved less than StuckDistance over StuckTime while moving. */
	void UpdateStuck(float DeltaSeconds);
	void ResetStuck();
	float GetGameValue(FName Key, float DefaultValue) const;
	void StopMoving();
	virtual void FaceTowards(const AActor* Goal);
	bool HasLineOfSight(const AActor* Goal) const;
	/** Where a shot or a line of sight starts or aims (the Entity overrides it: its shell floats above the capsule). */
	virtual FVector GetChestLocation() const;
	bool IsRanged() const;
	/** Windup telegraph: brighter tint (3D) or a slightly bigger sprite (pixel art). */
	virtual void SetTelegraph(bool bOn);

	UPROPERTY(VisibleAnywhere) TObjectPtr<UGBStatsComponent> Stats;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UGBAttributeComponent> Attributes;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UGBStatusEffectComponent> StatusEffects;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UGBElementalStatusComponent> ElementalStatus;
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
	/** Control lock (stun / freeze): state timers pause while > 0. */
	float ControlLockTime = 0.f;
	FLinearColor BodyColor = FLinearColor::Gray;
	FVector SpriteBaseScale = FVector::OneVector;

	// Zone and awareness.
	TWeakObjectPtr<AGBEncounter> Encounter;
	bool bHasHomeZone = false;
	FBox HomeZone = FBox(ForceInit);
	FVector PostLocation = FVector::ZeroVector;
	EGBUnawareBehavior UnawareBehavior = EGBUnawareBehavior::Patrol;
	TArray<FVector> RouteCandidates;
	TArray<FVector> PatrolRoute;
	bool bRouteBuilt = false;
	int32 PatrolIndex = 0;
	float PatrolPauseTimer = 0.f;
	bool bPatrolMoving = false;
	float PatrolRepathTimer = 0.f;
	float GuardTimer = 0.f;
	float GuardYaw = 0.f;
	float UnawareTravel = 0.f;
	FVector LastUnawarePos = FVector::ZeroVector;
	bool bAware = false;
	bool bHeardNoise = false;
	bool bJoinedSquad = false;
	float PatrolPointTimer = 0.f;
	float RouteRetryTimer = 0.f;
	bool bReturningToPost = false;
	float ReturnTimer = 0.f;
	float ReturnBestDistance = 0.f;
	float ReturnStallTimer = 0.f;
	int32 ZoneClamps = 0;
	/** Where the stimulus that woke it came from (a shot heard, a hit taken); only a sighting is a confirmed position. */
	FVector StimulusPos = FVector::ZeroVector;
	bool bStimulusConfirmed = false;
	bool bSealed = false;
	bool bSealedWasInvulnerable = false;
	bool bPendingWakeLeap = false;
	bool bWakeLeaping = false;
	float WakeLeapTime = 0.f;
	FVector WakeTarget = FVector::ZeroVector;
	bool bHoldsMeleeSlot = false;
	float AwareYawBase = 0.f;
	float AlertTimer = 0.f;
	float NoticeTimer = 0.f;
	float PendingAlertDelay = -1.f;
	float NavRepathTimer = 0.f;
	float SightCacheTimer = 0.f;
	/** Cached line of sight to the player (UpdateSightCache). */
	bool bSightCached = false;
	EGBGait Gait = EGBGait::Run;
	FRandomStream AwareRandom;
};
