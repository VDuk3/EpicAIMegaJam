// Base of the flying 3D enemies (the laser drone and the sentinel): the capsule flies (MOVE_Flying) at a hover height
// above the ground under it, steers round obstacles with capsule sweeps (and climbs over low ones), keeps a distance
// band, and runs the attack flow Chase -> Windup -> shot -> Recover itself. The visual parts, the shot and the death
// are left to the subclass. Every number is an Enemies.json stat; the defaults here are technical fallbacks.
#pragma once

#include "CoreMinimal.h"
#include "AI/GBEnemyCharacter.h"
#include "AI/GBEnemy3D.h"
#include "AI/GBDebris.h"
#include "GBFlyerEnemy.generated.h"

class UNiagaraSystem;
class USceneComponent;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;

UCLASS(Abstract)
class GLYPHBOUND_API AGBFlyerEnemy : public AGBEnemyCharacter
{
	GENERATED_BODY()

public:
	AGBFlyerEnemy();

	virtual void Tick(float DeltaSeconds) override;
	virtual FVector GetChestLocation() const override { return GetAimOrigin(); }
	virtual bool UsesAwareness() const override { return true; }
	virtual EGBSquadClass GetSquadClass() const override { return EGBSquadClass::Flyer; }

	/** Where the shots and the line of sight start (the eye, the core). */
	virtual FVector GetAimOrigin() const { return GetActorLocation(); }

	/** Smoke: the capsule centre's height above the ground under it, and the height it is trying to keep. */
	float GetHeightAboveGround() const { return FMath::Max(0.f, GetActorLocation().Z - GroundZ); }
	float GetWantedHeight() const { return WantedHeight; }
	bool HasArtLook() const { return bLookBuilt; }
	int32 GetShotCount() const { return ShotCount; }
	/** Smoke: seconds of falling before the first ground contact (-1 = none yet), bounces and pieces of the wreck, sparks spat. */
	float GetFallTime() const { return ContactTime; }
	int32 GetWreckBounces() const { return Debris.TotalBounces(); }
	int32 GetWreckPieces() const { return Debris.NumPieces(); }
	int32 GetSparkBursts() const { return SparkBursts; }
	bool HasLanded() const { return bLanded; }
	/** Smoke: an upper bound of the seconds from the death to the removal of the wreck (fall, wreck time, sinking). */
	float GetDeathTotalSeconds() const;
	/** Smoke: seconds since the ground under it last jumped (a course block or an edge it flew over): the hover height is judged once it settled. */
	float GetSecondsSinceGroundChange() const { return FlightTime - GroundChangeTime; }
	/** One line for the smoke run. */
	virtual FString DescribeFlyer() const;

protected:
	virtual void BeginPlay() override;
	virtual void InitFromDefinition(const FGBEnemyDefinition& Definition) override;
	virtual bool TickCustomBehavior(AActor* Goal, float DeltaSeconds) override;
	virtual void TickUnaware(AActor* Goal, float DeltaSeconds) override;
	virtual void HandleDeath(const FGBDamageInfo& KillingBlow) override;
	virtual void OnDamagedVisual(const FGBDamageInfo& Info, float ActualDamage) override;
	/** A flyer moves at MaxFlySpeed: Slow, data reloads and every other MoveSpeed change must reach it. */
	virtual void ApplyMovementStats() override;
	virtual bool PatrolsOnNavMesh() const override { return false; }

	/** Builds the visual parts from Enemies.json. Returns false when the art is missing (the placeholder stays). */
	virtual bool BuildLook(const FGBEnemyDefinition& Def) { (void)Def; return false; }
	/** Called every tick while alive, with the target (null when there is none). bWindup: the charge is running. */
	virtual void UpdateVisuals(float DeltaSeconds, const AActor* Goal, bool bWindup) { (void)DeltaSeconds; (void)Goal; (void)bWindup; }
	/** Default pull towards and around the player; the sentinel overrides it. Returns a horizontal vector of length 0..1. */
	virtual FVector DesiredFlightDirection(const AActor* Goal, float Distance, bool bSight);
	/**
	 * The wreck starts: put the parts into the debris field (AddDebrisPiece). The default makes the placeholder shape the one
	 * piece. DeathVelocity is what the enemy was doing when it died.
	 */
	virtual void OnDeathFall(const FGBDamageInfo& KillingBlow);
	/** The first piece touched the ground: the sparks, the explosion, the parts that come off. Point is on the surface. */
	virtual void OnFirstGroundContact(const FVector& Point, const FVector& Normal);
	/** The part the sparks come out of while the wreck lies there (the body). Null = the actor location. */
	virtual USceneComponent* GetWreckCentre() const;
	/** Windup started / ended (the glow of the eye, the aiming line). */
	virtual void OnWindupChanged(bool bStarted) { (void)bStarted; }
	/** Turn rate of the whole body towards the player, degrees per second. */
	virtual float GetBodyTurnRate() const;

	/** Adds a piece to the wreck: it leaves its parent and flies with Velocity, tumbling at SpinDegrees (degrees/s). */
	FGBDebrisPiece* AddDebrisPiece(USceneComponent* Component, const FVector& Velocity, const FVector& SpinDegrees);
	FGBDebrisParams MakeDebrisParams() const;
	/** A random unit direction within ConeDegrees of Direction. */
	FVector RandomDirectionNear(const FVector& Direction, float ConeDegrees);
	void SpawnDeathFx(UNiagaraSystem* System, const FVector& Location, const FRotator& Rotation, float Scale) const;
	FGBDamageInfo MakeShotInfo() const;
	float S(const TCHAR* Key, float Default) const { return GBEnemy3D::Stat(this, Key, Default); }
	bool IsSweepBlocked(const FVector& Start, const FVector& Direction, float Distance, const AActor* Goal) const;

	UPROPERTY() TObjectPtr<USceneComponent> VisualRoot;
	UPROPERTY() FGBEnemyShotFx ShotFx;

	bool bLookBuilt = false;
	bool bFalling = false;
	int32 ShotCount = 0;
	FRandomStream Random;
	float GroundZ = 0.f;
	float WantedHeight = 0.f;
	float FlightTime = 0.f;
	// The wreck
	FGBDebrisField Debris;
	FVector DeathVelocity = FVector::ZeroVector;
	float DeathElapsed = 0.f;
	float ContactTime = -1.f;           // DeathElapsed at the first ground contact
	float WreckElapsed = 0.f;           // seconds since the first ground contact
	float SinkElapsed = 0.f;
	float SparkTimer = 0.f;
	int32 SparkBursts = 0;
	bool bLanded = false;
	bool bSinking = false;
	UPROPERTY() TObjectPtr<UNiagaraSystem> DeathSparksFx;
	UPROPERTY() TObjectPtr<UNiagaraSystem> DeathExplosionFx;
	float AttackTimer = 0.f;
	bool bSightNow = false;

private:
	/** Moves the wreck. Returns true when it destroyed the actor. */
	bool TickDeath(float DeltaSeconds);
	FVector SteerAround(const FVector& Desired, const AActor* Goal, float DeltaSeconds);
	void UpdateHeight(float DeltaSeconds);

	float SightTimer = 0.f;
	float SeparationTimer = 0.f;
	FVector SeparationAway = FVector::ZeroVector;
	float OrbitNextTime = 0.f;
	bool bOrbitValid = false;
	FVector OrbitCache = FVector::ZeroVector;
	float OrbitSign = 1.f;
	float OrbitTimer = 0.f;
	float SideSign = 1.f;
	float LiftTimer = 0.f;
	float CeilingTimer = 0.f;
	float CeilingZ = 1e9f;
	float LostSightTime = 0.f;
	float VerticalVelocity = 0.f;
	bool bGroundKnown = false;
	float GroundChangeTime = -100.f;   // FlightTime when the ground under it last jumped by more than a step
};
