// One end of a hand ladder: the player uses it with the Interact key and is carried to the other end.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "World/GBInteractable.h"
#include "GBLadder.generated.h"

class USphereComponent;

/**
 * A ladder is two of these, one at each end, each placed on the floor where the player stands at that end and
 * turned so that its forward points (level) towards where the other end is. The other end is Rise above it
 * (negative = below) and StepOff ahead. Interact starts the climb on the player (AGBPlayerCharacter::StartLadder,
 * the ledge climb's own machinery: hands grip, the held item drops, the view turns to the ladder), up the ladder's
 * line and then across onto the landing, or back from the landing and then down.
 * It has no mesh: the ladder that is seen is level art placed beside it.
 */
UCLASS()
class GLYPHBOUND_API AGBLadder : public AActor, public IGBInteractable
{
	GENERATED_BODY()

public:
	AGBLadder();

	/** How far above this end the other one is (cm; negative = this is the top). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glyphbound|Ladder")
	float Rise = 600.f;

	/** How far ahead of this end, level, the other end's standing point is (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glyphbound|Ladder")
	float StepOff = 110.f;

	/** The player must stand this close to this end's standing point to use it (cm, level distance). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glyphbound|Ladder")
	float UseDistance = 160.f;

	/** Climbing speed along the ladder (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glyphbound|Ladder", meta = (ClampMin = "10.0"))
	float ClimbSpeed = 300.f;

	/**
	 * A ship's ladder (a steep stair with handrails): the player goes in one straight slanted line between the two
	 * standing points. Off = a vertical ladder: straight up its line, then across onto the landing.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glyphbound|Ladder")
	bool bSlanted = false;

	/** Seconds added for stepping on or off at the top. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glyphbound|Ladder")
	float StepSeconds = 0.5f;

	bool GoesUp() const { return Rise > 0.f; }
	/** Where the player stands at the other end (floor level). */
	FVector GetOtherEnd() const;
	/** Pure rule, for tests: whether feet at FeetOffset from this end's standing point (level distance, height) may use it. */
	static bool IsWithinUse(float LevelDistanceCm, float HeightDifferenceCm, float InUseDistance);
	/** Pure rule, for tests: seconds a climb of RiseCm takes. */
	static float ComputeClimbSeconds(float RiseCm, float SpeedCmPerSec, float InStepSeconds);

	// IGBInteractable
	virtual bool CanInteract(const AGBPlayerCharacter* Player) const override;
	virtual void Interact(AGBPlayerCharacter* Player) override;
	virtual FString GetInteractPrompt(const AGBPlayerCharacter* Player) const override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Glyphbound|Ladder", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USphereComponent> InteractVolume;
};
