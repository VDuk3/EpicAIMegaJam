// An automatic checkpoint (GDD 10): walking into it saves the player's state.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GBCheckpointVolume.generated.h"

class UBoxComponent;
class AGBPlayerCharacter;

/** Box trigger. Saves a checkpoint (UGBProgressSubsystem::SaveCheckpoint) once each time the player enters it. */
UCLASS()
class GLYPHBOUND_API AGBCheckpointVolume : public AActor
{
	GENERATED_BODY()

public:
	AGBCheckpointVolume();

	/** Half size of the box (cm). */
	void SetExtent(const FVector& Extent);

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	UFUNCTION()
	void HandleEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	void PlayerEntered(AGBPlayerCharacter* Player);
	/** Saves now, or on the next tick if the player has not begun play yet (it may spawn inside the box). */
	void SaveFor(AGBPlayerCharacter* Player);
	/** The player can start inside the box without an overlap event: checked once after spawn. */
	void CheckInitialOverlap();

	UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> Box;

	bool bPlayerInside = false;
};
