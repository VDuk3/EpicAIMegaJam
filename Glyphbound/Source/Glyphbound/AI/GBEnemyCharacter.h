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

/**
 * Built from a FGBEnemyDefinition (Enemies.json): stats, attack mode ("Melee" / "Ranged"), look, drops.
 * Simple state machine in C++ (no behavior tree assets): Idle until the player is within AggroRange and
 * present in the same era -> Chase (navmesh path if there is one, straight steering otherwise) -> Windup
 * (AttackWindup seconds, telegraphed) -> the hit (melee check in AttackRange, or an AGBProjectile) -> Recover
 * (AttackInterval) -> Chase. Slows and other effects work through UGBStatsComponent modifiers (MoveSpeed).
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

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

	/** Applies the definition: stats, capsule size, look. Called by SpawnEnemy before BeginPlay finishes. */
	virtual void InitFromDefinition(const FGBEnemyDefinition& Definition);
	virtual void PerformAttack();
	virtual void HandleDeath(const FGBDamageInfo& KillingBlow);
	void SetState(EGBEnemyState NewState);
	void ApplyMovementStats();
	AActor* FindTarget() const;

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
};
