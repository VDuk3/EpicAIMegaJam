// The rift to the Entity: opens in the future once the two quest items are combined (FinalPortalOpen).
#pragma once

#include "CoreMinimal.h"
#include "World/GBInteractable.h"
#include "GBFinalPortal.generated.h"

class UStaticMeshComponent;

/** Progress flag and Game.json key used by the final portal. */
namespace GBFinalPortalName
{
	inline const FName OpenFlag(TEXT("FinalPortalOpen"));
	/** Game.json "Strings": the map the rift opens (empty until the Entity map exists). */
	inline const FName MapPathKey(TEXT("EntityMapPath"));
}

/**
 * Present in the future only, and only once FinalPortalOpen is set (editable like any era rule).
 * Interact: opens the map named by Game.json Strings "EntityMapPath" only if that is a real, existing world package;
 * otherwise (empty, invalid, missing) the HUD says "The rift is not stable yet" and nothing is touched.
 * NOT DONE YET: the hand-over of the player's state to the next map (see the TODO in Interact) must exist before a real
 * Entity map is plugged in.
 * Placeholder look: a tall red-violet ring standing on the floor.
 */
UCLASS()
class GLYPHBOUND_API AGBFinalPortal : public AGBInteractableActor
{
	GENERATED_BODY()

public:
	AGBFinalPortal();

	/** True if MapPath is a valid long package name of a .umap that exists (nothing is loaded). The rift opens only then. */
	static bool IsOpenableMap(const FString& MapPath);

	// IGBInteractable
	virtual bool CanInteract(const AGBPlayerCharacter* Player) const override;
	virtual void Interact(AGBPlayerCharacter* Player) override;
	virtual FString GetInteractPrompt(const AGBPlayerCharacter* Player) const override;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere) TArray<TObjectPtr<UStaticMeshComponent>> RingSegments;
};
