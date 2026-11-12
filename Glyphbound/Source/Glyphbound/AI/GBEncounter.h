// An encounter: waves of enemies, a sub-boss or a boss, with its reward (GDD 5, 10).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/GBTypes.h"
#include "Data/GBDataTypes.h"
#include "GBEncounter.generated.h"

class UBoxComponent;
class AGBEnemyCharacter;
class UGBEraComponent;

UENUM(BlueprintType)
enum class EGBEncounterState : uint8
{
	Waiting,
	Active,
	Cleared
};

DECLARE_MULTICAST_DELEGATE_OneParam(FGBOnEncounterEvent, class AGBEncounter* /*Encounter*/);

/** Game.json "Values" keys used by encounters. */
namespace GBEncounterKey
{
	inline const FName SpawnRingRadius(TEXT("EncounterSpawnRingRadius"));  // cm, ring used when an encounter has no SpawnPoints
}

/**
 * Placed in the level (or spawned by the test arena) with an EncounterId from Encounters.json.
 * Starts when the player enters its trigger box in the right era; spawns each wave at its spawn points
 * (SpawnPoints, relative to the actor; a ring around the actor if none); the next wave comes when the current
 * one is dead; when the last one is dead the encounter is Cleared: skill points are given, its flag is set in
 * UGBProgressSubsystem (so doors open, zones unlock in the other era, and it never runs again), rewards drop,
 * and a checkpoint is saved. If the player dies mid-encounter, living enemies are removed and the encounter
 * goes back to Waiting (provisional rule).
 */
UCLASS()
class GLYPHBOUND_API AGBEncounter : public AActor
{
	GENERATED_BODY()

public:
	AGBEncounter();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glyphbound|Encounter")
	FName EncounterId = NAME_None;

	/** Era in which the encounter exists and its enemies are spawned. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glyphbound|Encounter")
	EGBEraPresence Presence = EGBEraPresence::Both;

	/** Relative to the actor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glyphbound|Encounter")
	TArray<FVector> SpawnPoints;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glyphbound|Encounter")
	FVector TriggerExtent = FVector(600.f, 600.f, 300.f);

	/** For runtime spawning: sets the fields above and refreshes the trigger. */
	void Configure(FName InEncounterId, EGBEraPresence InPresence, const FVector& InTriggerExtent, TConstArrayView<FVector> InSpawnPoints);

	EGBEncounterState GetState() const { return State; }
	int32 GetCurrentWave() const { return CurrentWave; }
	int32 GetNumWaves() const;
	int32 GetAliveCount() const;
	FName GetClearedFlag() const;

	/** Starts it now, without the trigger (bosses started by a cutscene or an interaction). */
	void StartEncounter();
	/** Removes living enemies and goes back to Waiting (player death). */
	void ResetEncounter();

	FGBOnEncounterEvent OnStarted;
	FGBOnEncounterEvent OnWaveStarted;
	FGBOnEncounterEvent OnCleared;

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleTriggerOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	void SpawnWave(int32 WaveIndex);
	void HandleEnemyDied(AGBEnemyCharacter* Enemy);
	void Clear();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void HandleEraChanged(EGBEra NewEra);
	/** Starts the encounter if it is Waiting, not cleared, in the right era and the living player is inside the trigger. */
	void TryStartFromTrigger(bool bPlayerKnownInside = false);
	/** Not kept across frames (data can be reloaded). */
	const FGBEncounterDefinition* FindDefinition() const;
	/** Called when the current wave has no living enemy left: schedules the next wave or clears. */
	void HandleWaveDefeated();
	/** Cleared flag already set (enemies never respawn): go straight to Cleared without rewards. */
	void ApplyClearedFlagIfSet();

	UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> Trigger;

	EGBEncounterState State = EGBEncounterState::Waiting;
	int32 CurrentWave = -1;
	/** Seconds until the next wave spawns; < 0 = not waiting. */
	float NextWaveTimer = -1.f;
	UPROPERTY() TArray<TObjectPtr<AGBEnemyCharacter>> AliveEnemies;
	bool bLoggedMissingDefinition = false;
};
