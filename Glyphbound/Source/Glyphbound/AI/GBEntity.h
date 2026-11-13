// The Entity, phase 1 "the machine" (GDD 11): a floating metal shell with a big central eye
// (the weak point) and hologram tentacles driven by UGBTentacleRigComponent.
// The body, the eye and the tentacles are visuals parented to a scene root that floats above the
// ground capsule. The capsule keeps walking on the navmesh like every other enemy (pathing, local
// steering, ledge checks), but it no longer blocks the weapon channel: a sphere on the shell does
// (guns and projectiles hit what is drawn). The visual root flies: altitude drift, banking into
// motion, a 3D aim, a limited turn rate and spring knockback. The base class placeholder look is
// hidden once the real art loads; if any asset is missing the placeholder stays (one warning).
// Every tuning number is a Game.json "Values" entry prefixed Entity, with the default in code.
#pragma once

#include "CoreMinimal.h"
#include "AI/GBEnemyCharacter.h"
#include "GBEntity.generated.h"

class UGBTentacleRigComponent;
class UPoseableMeshComponent;
class UStaticMeshComponent;
class USceneComponent;
class UMeshComponent;
class UMaterialInstanceDynamic;
class USphereComponent;
class UPointLightComponent;
class AGBProjectile;

/** Critically-tunable spring: X follows Target with natural frequency Omega (rad/s) and damping ratio Zeta. */
struct FGBEntitySpring
{
	float X = 0.f;
	float V = 0.f;
	void Step(float Target, float Dt, float Omega, float Zeta);
};

/** Entity flight, turn, hit and light numbers; filled from Game.json every tick (defaults in GBEntity.cpp). */
struct FGBEntityTuning
{
	float TurnMaxRate = 90.f, TurnGain = 3.f, TurnAccel = 5.f;
	float PitchMax = 30.f, AimOmega = 6.f, EyeTrackMax = 12.f, EyeTrackSpeed = 14.f;
	float AltMin = 250.f, AltMax = 400.f, AltDriftAmp = 70.f, AltDriftHz = 0.22f, AltRise = 60.f, AltDip = 45.f, AltDipTime = 0.7f, AltOmega = 3.f;
	float BankMax = 20.f, BankPerYawRate = 0.07f, LeanDeg = 10.f, AccelPitchDeg = 8.f, TiltOmega = 7.f, TiltZeta = 0.75f;
	float Acceleration = 700.f, Braking = 600.f, GroundFriction = 2.f;
	float StrafeSpeed = 0.7f, StrafeFlipMin = 1.8f, StrafeFlipMax = 4.f, StrafeKeep = 0.5f, StrafeProbe = 220.f;
	float HitTiltKick = 90.f, HitPushKick = 220.f, HitRefDamage = 100.f, HitMinShare = 0.15f, HitWeakMult = 2.f, HitOmega = 8.f, HitZeta = 0.3f;
	float ShellRadius = 88.f, EyeWeakRadius = 40.f;
	float EyeLightIntensity = 3000.f, EyeLightRadius = 1600.f, EyeLightPulseHz = 0.7f, EyeLightPulse = 0.15f;
	float EyeLightWindupFlare = 0.8f, EyeLightHitFlare = 2.f, EyeLightFade = 0.8f;
	float BoltLength = 80.f, BoltThickness = 12.f, BoltTrail = 240.f, BoltLightIntensity = 4000.f, BoltLightRadius = 700.f;
	float Scale = 2.5f;
	float EyeLightInner = 10.f, EyeLightOuter = 28.f;
	float TentacleLightCount = 4.f, TentacleLightIntensity = 90.f, TentacleLightRadius = 600.f;
	float TentacleFlashShot = 1.f, TentacleFlashHit = 0.6f, TentacleFlashFade = 0.3f;
	float CeilingMargin = 40.f, CeilingProbeInterval = 0.15f;
	float MuzzleIntensity = 15000.f, MuzzleRadius = 700.f, MuzzleTime = 0.12f;
};

UCLASS()
class GLYPHBOUND_API AGBEntity : public AGBEnemyCharacter
{
	GENERATED_BODY()

public:
	AGBEntity();

	virtual void Tick(float DeltaSeconds) override;

	/** Weak point: a hit inside the eye sphere counts (the top of the shell does not). */
	virtual bool IsHeadshotLocation(const FVector& HitLocation) const override;
	/** Shots and lines of sight start at the floating shell, not at the ground capsule. */
	virtual FVector GetChestLocation() const override;
	/** Blasts and knockback measure to the nearest point of the shell. */
	virtual FVector GetDamageTargetLocation(const FVector& FromPoint) const override;
	/** The Entity turns with weight in Tick (rate-limited), so the base class snap does nothing. */
	virtual void FaceTowards(const AActor* Goal) override;

	UGBTentacleRigComponent* GetTentacleRig() const { return TentacleRig; }
	FVector GetEyeCentreWorld() const;
	FVector GetShellTopWorld() const;
	FVector GetShellCentreWorld() const;

	/** One line for the smoke run: where the visual is and whether every part renders. */
	FString DescribeVisual() const;

	/** Smoke/tests: true suppresses attacks and holds the actor still (windup reach stays off too). */
	void SetAttackSuppressed(bool bSuppressed) { bAttackSuppressed = bSuppressed; }

	/** Smoke/tests: true keeps a reach set by hand alive (it is otherwise cleared when the windup ends). */
	void SetHoldReach(bool bHold) { bHoldReach = bHold; }
	float GetVisualScale() const { return Tuning.Scale; }
	/** World radius of the hit sphere on the shell. */
	float GetShellRadiusWorld() const;
	/** Height in cm of the union of the drawn parts (body, eye, tentacles). */
	float GetVisualBoundsHeight() const;

	/** Smoke/tests: strafe round the target for this long even with attacks suppressed. */
	void ForceStrafe(float Seconds) { ForcedStrafeTime = Seconds; }
	/** Smoke/tests: fires one energy bolt from the closest tentacle tip along Direction (Speed > 0 overrides the speed). */
	AGBProjectile* FireDebugBolt(const FVector& Direction, float Speed);
	AGBProjectile* GetLastBolt() const;

	/** Smoke: peak values since the last reset. */
	void ResetMotionStats();
	float GetPeakYawRate() const { return PeakYawRate; }
	float GetShellZRange() const { return ShellZMax - ShellZMin; }
	float GetPeakRoll() const { return PeakRoll; }

protected:
	virtual void BeginPlay() override;
	virtual void InitFromDefinition(const FGBEnemyDefinition& Definition) override;
	virtual void PerformAttack() override;
	virtual void HandleDeath(const FGBDamageInfo& KillingBlow) override;
	virtual void OnDamagedVisual(const FGBDamageInfo& Info, float ActualDamage) override;

private:
	/** World start point of the ranged shot: the tip closest to the target, or the chest fallback. */
	FVector GetAttackOrigin(AActor* Goal) const;
	FString DescribeMaterials(UMeshComponent* InMesh) const;
	void ReadTuning();
	AGBProjectile* FireBolt(AActor* Goal, const FVector& Origin, const FVector& Direction, float SpeedOverride);
	FVector GetGoalHeadLocation(const AActor* Goal) const;
	void UpdateTurn(float Dt, const AActor* Goal);
	void UpdateStrafe(float Dt, const AActor* Goal);
	void UpdateFlight(float Dt, const AActor* Goal);
	void UpdateLights(float Dt);
	void UpdateCeiling(float Dt);
	void SetCapsuleMovementOnly(bool bMovementOnly);
	void FlashTentacles(float Amount);
	void BuildVisual();
	void ApplyFlash();

	UPROPERTY() TObjectPtr<USceneComponent> VisualRoot;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> EntityBody;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> EntityEye;
	UPROPERTY() TObjectPtr<UPoseableMeshComponent> EntityTentacles;
	UPROPERTY() TObjectPtr<UGBTentacleRigComponent> TentacleRig;

	UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> BodyMIDs;
	UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> EyeMIDs;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> TentacleMID;

	UPROPERTY() TObjectPtr<USphereComponent> ShellHit;
	UPROPERTY() TObjectPtr<UPointLightComponent> EyeLight;
	UPROPERTY() TObjectPtr<UPointLightComponent> MuzzleLight;
	UPROPERTY() TArray<TObjectPtr<UPointLightComponent>> TentacleLights;
	UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> TentacleMIDs;
	TWeakObjectPtr<AGBProjectile> LastBolt;

	FGBEntityTuning Tuning;
	float CapsuleHalfHeightCached = 135.f;

	// Turn
	float YawRateDeg = 0.f;       // current turn rate, deg/s
	float PrevActorYaw = 0.f;
	bool bHasPrevYaw = false;
	float PeakYawRate = 0.f;
	bool bHoldReach = false;
	float TentacleFlash = 0.f;
	float CeilingAltCm = 1e9f;   // highest shell centre altitude the geometry above allows
	float CeilingTimer = 0.f;

	// Flight springs: altitude of the shell centre above the floor, aim pitch, motion pitch, bank roll,
	// hit tilt and the push offset (actor frame).
	FGBEntitySpring AltSpring, AimPitchSpring, MotionPitchSpring, BankRollSpring, HitPitchSpring, HitRollSpring;
	FGBEntitySpring PushX, PushY, PushZ;
	float EyeYaw = 0.f;
	float EyePitch = 0.f;
	float PrevLocalFwd = 0.f;
	float FwdAccelSmoothed = 0.f;
	float AltDipTimer = 0.f;
	float WindupAlpha = 0.f;
	bool bAltInitialised = false;
	float ShellZMin = 1e9f;
	float ShellZMax = -1e9f;
	float PeakRoll = 0.f;

	// Strafe
	float StrafeSign = 1.f;
	float StrafeTimer = 0.f;
	float ForcedStrafeTime = 0.f;
	bool bStrafingLastTick = false;

	// Lights
	float MuzzleTimer = 0.f;
	float DeathAlpha = 1.f;
	bool bDying = false;
	FLinearColor EyeLightColor = FLinearColor(0.23f, 1.f, 0.69f);
	FLinearColor TentacleColor = FLinearColor(0.23f, 1.f, 0.69f);
	bool bVisualFailed = false;

	float VisualBaseZ = 0.f;
	float FlashValue = 0.f;
	float BobTime = 0.f;
	bool bAttackSuppressed = false;
	bool bVisualBuilt = false;
};
