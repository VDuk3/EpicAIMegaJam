// The sentinel: a floating sphere turret with a blue eye. The shell turns slowly towards the player; the core (the eye,
// in the shell's front opening) turns faster inside it, clamped to the opening, so the eye visibly tracks him, and
// pulses. The attack is a charge with a thin aiming line that follows the player with a lag (moving dodges it), then one
// heavy hitscan beam. The core is the weak point. Parts: Enemies.json "Strings" Shell (front +X, socket Core) and Core
// (pivot at its centre); without them the placeholder stays.
#pragma once

#include "CoreMinimal.h"
#include "AI/GBFlyerEnemy.h"
#include "GBSentinelEnemy.generated.h"

UCLASS()
class GLYPHBOUND_API AGBSentinelEnemy : public AGBFlyerEnemy
{
	GENERATED_BODY()

public:
	AGBSentinelEnemy();

	virtual FVector GetAimOrigin() const override;
	/** Weak point: a hit within CoreRadius of the core. */
	virtual bool IsHeadshotLocation(const FVector& HitLocation) const override;
	virtual FString DescribeFlyer() const override;

	/** Smoke: the core's yaw inside the shell, the aiming line's visibility, the pulse value. */
	float GetCoreYaw() const { return CoreYaw; }
	bool IsAimLineVisible() const;
	float GetPulse() const { return PulseValue; }
	FVector GetCoreWorldLocation() const { return GetAimOrigin(); }
	FVector GetShellLocation() const;

protected:
	virtual bool BuildLook(const FGBEnemyDefinition& Def) override;
	virtual void UpdateVisuals(float DeltaSeconds, const AActor* Goal, bool bWindup) override;
	virtual FVector DesiredFlightDirection(const AActor* Goal, float Distance, bool bSight) override;
	virtual void PerformAttack() override;
	virtual void OnWindupChanged(bool bStarted) override;
	virtual float GetBodyTurnRate() const override;
	virtual void OnDeathFall(const FGBDamageInfo& KillingBlow) override;
	virtual void SetTelegraph(bool bOn) override;

private:
	void UpdateAimLine(const FVector& From, const FVector& To, bool bVisible);

	UPROPERTY() TObjectPtr<UStaticMeshComponent> ShellPart;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> CorePart;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> AimLine;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> CoreMaterial;

	FName GlowParam = NAME_None;
	float CoreYaw = 0.f;
	float CorePitch = 0.f;
	float PulseValue = 1.f;
	FVector AimPoint = FVector::ZeroVector;
	bool bAimPointValid = false;
	FVector LockedAimPoint = FVector::ZeroVector;
};
