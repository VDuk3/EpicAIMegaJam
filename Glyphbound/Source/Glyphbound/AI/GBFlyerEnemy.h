// Base of the flying 3D enemies (the laser drone and the sentinel): the capsule flies (MOVE_Flying) at a hover height
// above the ground under it, steers round obstacles with capsule sweeps (and climbs over low ones), keeps a distance
// band, and runs the attack flow Chase -> Windup -> shot -> Recover itself. The visual parts, the shot and the death
// are left to the subclass. Every number is an Enemies.json stat; the defaults here are technical fallbacks.
#pragma once

#include "CoreMinimal.h"
#include "AI/GBEnemyCharacter.h"
#include "AI/GBEnemy3D.h"
#include "GBFlyerEnemy.generated.h"

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

	/** Where the shots and the line of sight start (the eye, the core). */
	virtual FVector GetAimOrigin() const { return GetActorLocation(); }

	/** Smoke: the capsule centre's height above the ground under it, and the height it is trying to keep. */
	float GetHeightAboveGround() const { return FMath::Max(0.f, GetActorLocation().Z - GroundZ); }
	float GetWantedHeight() const { return WantedHeight; }
	bool HasArtLook() const { return bLookBuilt; }
	int32 GetShotCount() const { return ShotCount; }
	/** One line for the smoke run. */
	virtual FString DescribeFlyer() const;

protected:
	virtual void BeginPlay() override;
	virtual void InitFromDefinition(const FGBEnemyDefinition& Definition) override;
	virtual bool TickCustomBehavior(AActor* Goal, float DeltaSeconds) override;
	virtual void HandleDeath(const FGBDamageInfo& KillingBlow) override;
	virtual void OnDamagedVisual(const FGBDamageInfo& Info, float ActualDamage) override;
	/** A flyer moves at MaxFlySpeed: Slow, data reloads and every other MoveSpeed change must reach it. */
	virtual void ApplyMovementStats() override;

	/** Builds the visual parts from Enemies.json. Returns false when the art is missing (the placeholder stays). */
	virtual bool BuildLook(const FGBEnemyDefinition& Def) { (void)Def; return false; }
	/** Called every tick while alive, with the target (null when there is none). bWindup: the charge is running. */
	virtual void UpdateVisuals(float DeltaSeconds, const AActor* Goal, bool bWindup) { (void)DeltaSeconds; (void)Goal; (void)bWindup; }
	/** Default pull towards and around the player; the sentinel overrides it. Returns a horizontal vector of length 0..1. */
	virtual FVector DesiredFlightDirection(const AActor* Goal, float Distance, bool bSight);
	/** Death with art: drop the parts. The default makes the capsule fall (see StartCapsuleFall). */
	virtual void OnDeathFall(const FGBDamageInfo& KillingBlow);
	/** Windup started / ended (the glow of the eye, the aiming line). */
	virtual void OnWindupChanged(bool bStarted) { (void)bStarted; }
	/** Turn rate of the whole body towards the player, degrees per second. */
	virtual float GetBodyTurnRate() const;

	void StartCapsuleFall();
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
	float AttackTimer = 0.f;
	bool bSightNow = false;

private:
	FVector SteerAround(const FVector& Desired, const AActor* Goal, float DeltaSeconds);
	void UpdateHeight(float DeltaSeconds);

	float SightTimer = 0.f;
	float OrbitSign = 1.f;
	float OrbitTimer = 0.f;
	float SideSign = 1.f;
	float LiftTimer = 0.f;
	float CeilingTimer = 0.f;
	float CeilingZ = 1e9f;
	float LostSightTime = 0.f;
	float VerticalVelocity = 0.f;
	bool bGroundKnown = false;
};
