// A 3D enemy with a skeletal mesh: the humanoid shooters of the future (robot, armored orcs) and the spider.
// Everything comes from Enemies.json (mesh, armor pieces, weapon, animation clips, effects, tuning stats) and every
// asset may be missing: the capsule then keeps the placeholder look of AGBEnemyCharacter.
//
// AttackMode "Ranged": a small first-person-shooter AI (notice by sight cone, alert nearby enemies, keep a distance
// band, strafe on the navmesh, shoot bursts of hitscan shots with spread and Niagara effects).
// AttackMode "Melee": the parent's chase and bite, plus an optional lunge (LungeRange stat).
// Animation: UGBEnemyAnimInstance (no AnimBlueprint asset). Death: ragdoll if the mesh has a physics asset, else the
// Death clip, else the parent's behaviour; the weapon falls off.
#pragma once

#include "CoreMinimal.h"
#include "AI/GBEnemyCharacter.h"
#include "AI/GBEnemy3D.h"
#include "AI/GBEnemyAnimInstance.h"
#include "GBSkeletalEnemy.generated.h"

class USkeletalMeshComponent;
class UStaticMeshComponent;

UCLASS()
class GLYPHBOUND_API AGBSkeletalEnemy : public AGBEnemyCharacter
{
	GENERATED_BODY()

public:
	AGBSkeletalEnemy();

	/** True when the skeletal mesh loaded and replaced the placeholder. */
	bool HasSkeletalLook() const { return bLookBuilt; }
	UGBEnemyAnimInstance* GetEnemyAnim() const { return AnimInst; }
	bool IsAware() const { return bAware; }
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

protected:
	virtual void BeginPlay() override;
	virtual void InitFromDefinition(const FGBEnemyDefinition& Definition) override;
	virtual bool TickCustomBehavior(AActor* Goal, float DeltaSeconds) override;
	virtual void PerformAttack() override;
	virtual void HandleDeath(const FGBDamageInfo& KillingBlow) override;
	virtual void OnDamagedVisual(const FGBDamageInfo& Info, float ActualDamage) override;
	virtual void SetTelegraph(bool bOn) override;

private:
	enum class EMoveIntent : uint8 { None, Advance, Retreat, Strafe };
	enum class EBurstPhase : uint8 { Cooldown, Telegraph, Firing };

	void BuildLook();
	float S(const TCHAR* Key, float Default) const { return GBEnemy3D::Stat(this, Key, Default); }
	FGBDamageInfo MakeShotInfo() const;

	// Ranged
	bool TickRanged(AActor* Goal, float DeltaSeconds);
	void TurnTowards(const AActor* Goal, float DeltaSeconds);
	void AlertOthers();
	void SetIntent(EMoveIntent Intent);
	/** Moves towards a point: a navmesh path (requested only when bRequestPath) or, without one, local movement fed every tick. */
	void MoveToPoint(const FVector& Point, const AActor* Goal, bool bRequestPath);
	bool PickStrafeGoal(const AActor* Goal, float RangeMin, float RangeMax);
	void TickBurst(AActor* Goal, float DeltaSeconds, bool bSight, float Distance);
	void FireShot(AActor* Goal);

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
	bool bAware = false;
	bool bSight = false;
	float SightTimer = 0.f;
	EMoveIntent Intent = EMoveIntent::None;
	bool bPointPathing = false;
	float RepathTimer = 0.f;
	float StrafeTimer = 0.f;
	FVector StrafeGoal = FVector::ZeroVector;
	bool bHasStrafeGoal = false;
	EBurstPhase BurstPhase = EBurstPhase::Cooldown;
	float BurstTimer = 0.f;
	int32 ShotsLeft = 0;
	float ShotTimer = 0.f;
	int32 ShotCount = 0;
	int32 BurstCount = 0;
	int32 StrafePicks = 0;
	int32 StrafePickFails = 0;
	FRandomStream Random;

	// Melee lunge
	bool bLungeQueued = false;
	bool bLunging = false;
	bool bLungeHit = false;
	float LungeTime = 0.f;
	float LungeCooldown = 0.f;
	int32 LungeCount = 0;

	float LastHitReactTime = -10.f;
};
