// A fixed portal between the two eras (GDD 4). Portals are the safe points of the map: arriving saves a checkpoint.
#pragma once

#include "CoreMinimal.h"
#include "World/GBInteractable.h"
#include "Core/GBTypes.h"
#include "GBPortal.generated.h"

class UStaticMeshComponent;

/**
 * Interact = travel to the other era (always the other one) through UGBEraSubsystem::RequestTravel.
 * Optionally teleports the player to Destination while the screen is covered (bTeleport); otherwise the player
 * stays where it is, in the other time. Look: a glowing frame (two pillars and a lintel) with a pixel-art swirl.
 */
UCLASS()
class GLYPHBOUND_API AGBPortal : public AGBInteractableActor
{
	GENERATED_BODY()

public:
	AGBPortal();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glyphbound|Portal")
	bool bTeleport = false;

	/** World transform the player arrives at when bTeleport (yaw becomes the view direction). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glyphbound|Portal")
	FTransform Destination;

	/** The era this portal leads to right now (the other one). */
	EGBEra GetTargetEra() const;

	// IGBInteractable
	virtual bool CanInteract(const AGBPlayerCharacter* Player) const override;
	virtual void Interact(AGBPlayerCharacter* Player) override;
	virtual FString GetInteractPrompt(const AGBPlayerCharacter* Player) const override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Frame color follows the era it leads to. */
	void UpdateColor(EGBEra CurrentEra);

	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> PillarLeft;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> PillarRight;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Lintel;

	FDelegateHandle EraChangedHandle;
};
