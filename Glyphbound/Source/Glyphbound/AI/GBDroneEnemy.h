// The laser drone of the future base: a static-mesh body with four spinning rotors and an eye that turns to look at the
// player. It flies (AGBFlyerEnemy), banks into its motion, charges (the eye glows) and fires a short red hitscan laser from
// the eye. On death the body falls and tumbles as a debris piece (AI/GBDebris), the rotors run down; at the first ground contact it sparks, explodes and the rotors and the eye fly off. All parts come from Enemies.json "Strings"
// (Body with sockets Rotor_0..Rotor_3 and Eye, Rotor_0..Rotor_3, Eye); a missing body keeps the placeholder look.
#pragma once

#include "CoreMinimal.h"
#include "AI/GBFlyerEnemy.h"
#include "GBDroneEnemy.generated.h"

UCLASS()
class GLYPHBOUND_API AGBDroneEnemy : public AGBFlyerEnemy
{
	GENERATED_BODY()

public:
	AGBDroneEnemy();

	virtual FVector GetAimOrigin() const override;
	virtual FString DescribeFlyer() const override;

	/** Smoke: rotor angle travelled since the last call (degrees, summed over the rotors) and the eye's current yaw / glow. */
	float GetRotorTravelDegrees() const { return RotorTravel; }
	float GetEyeYaw() const { return EyeYaw; }
	float GetGlow() const { return GlowValue; }
	bool HasGlowParameter() const { return !GlowParam.IsNone(); }
	bool IsBodySimulating() const;
	/** Smoke: where the parts are in the world (rotors on their sockets, the eye on its socket, the body pivot). */
	FVector GetRotorLocation(int32 Index) const;
	FVector GetBodyLocation() const;

protected:
	virtual bool BuildLook(const FGBEnemyDefinition& Def) override;
	virtual void UpdateVisuals(float DeltaSeconds, const AActor* Goal, bool bWindup) override;
	virtual void PerformAttack() override;
	virtual void OnDeathFall(const FGBDamageInfo& KillingBlow) override;
	virtual void OnFirstGroundContact(const FVector& Point, const FVector& Normal) override;
	virtual USceneComponent* GetWreckCentre() const override;
	virtual void SetTelegraph(bool bOn) override;

private:
	static constexpr int32 NumRotors = 4;

	UPROPERTY() TObjectPtr<UStaticMeshComponent> BodyPart;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> RotorParts[4];
	UPROPERTY() TObjectPtr<UStaticMeshComponent> EyePart;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> EyeMaterial;

	FName GlowParam = NAME_None;
	float RotorAngle[NumRotors] = { 0.f, 0.f, 0.f, 0.f };
	float RotorTravel = 0.f;
	float RotorSpin = 1.f;          // 0..1 of the rotor speed, falls to 0 after death
	float EyeYaw = 0.f;
	float EyePitch = 0.f;
	FRotator BankRotation = FRotator::ZeroRotator;
	bool bBrokenApart = false;      // the rotors and the eye came off at the first ground contact
	float GlowValue = 1.f;
};
