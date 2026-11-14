// A door leaf that slides open when the player comes near and closes again behind them.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GBDoor.generated.h"

class UStaticMeshComponent;
class USceneComponent;

/**
 * One static mesh (set per placed instance) that slides by SlideOffset (in the door's own axes) while the player
 * is within OpenDistance of the door's centre (the actor's position plus CentreOffset, also in the door's axes).
 * With RequiredFlag set, the door stays shut until that progress flag exists (an encounter cleared, a switch).
 * The leaf blocks like any wall while it is shut; it is left out of the navmesh, so paths go through the doorway.
 */
UCLASS()
class GLYPHBOUND_API AGBDoor : public AActor
{
	GENERATED_BODY()

public:
	AGBDoor();

	virtual void Tick(float DeltaSeconds) override;

	UStaticMeshComponent* GetMesh() const { return Mesh; }
	/** 0 = shut, 1 = fully open. */
	float GetOpenAlpha() const { return OpenAlpha; }

	/** Where the leaf goes when open, relative to its shut position, in the door's own axes (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glyphbound|Door")
	FVector SlideOffset = FVector(0.f, 0.f, 300.f);

	/** The middle of the doorway, relative to the actor, in the door's own axes (cm). Distances are measured from it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glyphbound|Door")
	FVector CentreOffset = FVector::ZeroVector;

	/** The door opens while the player is closer than this to the doorway's middle (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glyphbound|Door")
	float OpenDistance = 450.f;

	/** Seconds from shut to fully open. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glyphbound|Door")
	float OpenSeconds = 0.6f;

	/** If set, the door stays shut until this progress flag exists. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glyphbound|Door")
	FName RequiredFlag = NAME_None;

	/** Pure rule, for tests: where the leaf is for an open amount of 0..1 (eased at both ends). */
	static FVector ComputeLeafOffset(const FVector& InSlideOffset, float Alpha);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Glyphbound|Door", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Glyphbound|Door", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> Mesh;

	float OpenAlpha = 0.f;
};
