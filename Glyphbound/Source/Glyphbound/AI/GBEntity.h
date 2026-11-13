// The Entity, phase 1 "the machine" (GDD 11): a floating metal shell with a big central eye
// (the weak point) and hologram tentacles driven by UGBTentacleRigComponent.
// The capsule is the only collision; the body, the eye and the tentacles are visuals parented to
// a scene root offset so the tentacle tips clear the floor. The base class placeholder look is
// hidden once the real art loads; if any asset is missing the placeholder stays (one warning).
// Movement stays on the ground capsule (walking mode with a hovering visual), so navmesh pathing,
// local steering and ledge checks work exactly like every other enemy.
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

UCLASS()
class GLYPHBOUND_API AGBEntity : public AGBEnemyCharacter
{
	GENERATED_BODY()

public:
	AGBEntity();

	virtual void Tick(float DeltaSeconds) override;

	/** Weak point: a hit inside the eye sphere counts (the top of the shell does not). */
	virtual bool IsHeadshotLocation(const FVector& HitLocation) const override;

	UGBTentacleRigComponent* GetTentacleRig() const { return TentacleRig; }
	FVector GetEyeCentreWorld() const;
	FVector GetShellTopWorld() const;

	/** One line for the smoke run: where the visual is and whether every part renders. */
	FString DescribeVisual() const;

	/** Smoke/tests: true suppresses attacks and holds the actor still (windup reach stays off too). */
	void SetAttackSuppressed(bool bSuppressed) { bAttackSuppressed = bSuppressed; }

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

	float VisualBaseZ = 0.f;
	float FlashValue = 0.f;
	float BobTime = 0.f;
	bool bAttackSuppressed = false;
	bool bVisualBuilt = false;
};
