// A mimic (owner, 2026-11-15): the same chest, until the player tries to open it or hits it. Then its lid becomes a
// jaw with teeth and a tongue, and it hops after the player and bites hard.
#pragma once

#include "CoreMinimal.h"
#include "World/GBChest.h"
#include "GBMimic.generated.h"

class UGBAttributeComponent;
class UStaticMeshComponent;
struct FGBDamageInfo;

/**
 * Asleep it is an AGBChest in every way the player can see (same boxes, same materials, same prompt). Interacting
 * with it, or damaging it, wakes it: it turns hostile, shows blocky teeth, a tongue and two eyes (plain blocks, the
 * project's pixel look), and from then on it hops towards the player and bites when close. Killed, it drops its
 * Contents as pickups and its flag keeps it gone.
 */
UCLASS()
class GLYPHBOUND_API AGBMimic : public AGBChest
{
	GENERATED_BODY()

public:
	AGBMimic();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glyphbound|Mimic")
	float MaxHealth = 220.f;

	/** Damage of one bite (the player has 100 health). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glyphbound|Mimic")
	float BiteDamage = 45.f;

	/** cm between the two centres at which a bite lands. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glyphbound|Mimic")
	float BiteRange = 230.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glyphbound|Mimic")
	float BiteCooldown = 1.1f;

	/** cm covered by one hop, and its seconds in the air and on the ground. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glyphbound|Mimic")
	float HopDistance = 260.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glyphbound|Mimic")
	float HopSeconds = 0.42f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glyphbound|Mimic")
	float HopRestSeconds = 0.3f;

	/** It gives up beyond this distance from the player (cm) and waits there, awake. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glyphbound|Mimic")
	float ChaseRange = 3000.f;

	bool IsAwake() const { return bAwake; }

	virtual void Tick(float DeltaSeconds) override;
	virtual void Interact(AGBPlayerCharacter* Player) override;

	/** Where a point of the lid is (actor space) with the jaw open by AngleDeg. HingeFrame is the point measured
	 *  from the hinge line: X along it, Y towards the front, Z up out of the closed lid. */
	static FVector LidPoint(const FVector& HingeFrame, float AngleDeg);

protected:
	virtual void BeginPlay() override;

	void Wake();
	void HandleDamaged(const FGBDamageInfo& Info, float ActualDamage);
	void HandleDeath(const FGBDamageInfo& KillingBlow);
	void SetJaw(float AngleDeg);
	void Bite(AActor* Target);
	FName GetDeadFlag() const;
	UStaticMeshComponent* AddBlock(const TCHAR* Name, const FVector& SizeCm, const FLinearColor& Color, bool bGlow);

	UPROPERTY(Transient) TObjectPtr<UGBAttributeComponent> Attributes;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> UpperTeeth;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Eyes;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Palate;

	bool bAwake = false;
	bool bDying = false;
	bool bHopping = false;
	float StateSeconds = 0.f;
	float BiteSeconds = 0.f;      // counts down to the next bite
	float SnapSeconds = 0.f;      // > 0 while the jaw is snapping shut
	float AwakeSeconds = 0.f;
	FVector HopFrom = FVector::ZeroVector;
	FVector HopTo = FVector::ZeroVector;
};
