// A chest (GDD 8): staffs, rarely armor, anything else as drops. Opened once, remembered by a progress flag.
#pragma once

#include "CoreMinimal.h"
#include "World/GBInteractable.h"
#include "Data/GBDataTypes.h"
#include "GBChest.generated.h"

class UStaticMeshComponent;

/**
 * Solid. Interact gives every entry of Contents with the pickup rules (AGBPickup::GiveDrop); what the player
 * cannot take is dropped as pickups in front of it. Then it sets OpenedFlag, shows its open look and is no longer
 * interactable. A chest whose flag is already set starts open (flags are never rolled back).
 */
UCLASS()
class GLYPHBOUND_API AGBChest : public AGBInteractableActor
{
	GENERATED_BODY()

public:
	AGBChest();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glyphbound|Chest")
	TArray<FGBDropDefinition> Contents;

	/** Progress flag that remembers it was opened. Empty = "Chest.<ActorName>" (only stable for chests placed in a map). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glyphbound|Chest")
	FName OpenedFlag = NAME_None;

	/** For runtime spawning: call right after spawn. */
	void Configure(TConstArrayView<FGBDropDefinition> InContents, FName InOpenedFlag);

	bool IsOpen() const { return bOpen; }

	// IGBInteractable
	virtual bool CanInteract(const AGBPlayerCharacter* Player) const override;
	virtual void Interact(AGBPlayerCharacter* Player) override;
	virtual FString GetInteractPrompt(const AGBPlayerCharacter* Player) const override;

protected:
	virtual void BeginPlay() override;

	FName GetOpenedFlag() const;
	void SetOpenLook(bool bInOpen);

	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Lid;

	bool bOpen = false;
};
