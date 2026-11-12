// Things lying on the floor: ammo and items dropped by enemies, guns, staffs, spell knowledge.
#pragma once

#include "CoreMinimal.h"
#include "World/GBInteractable.h"
#include "Data/GBDataTypes.h"
#include "GBPickup.generated.h"

/**
 * One pickup = one FGBDropDefinition (item, weapon or spell group).
 *  - Items and ammo are taken by walking over them (if there is room).
 *  - Weapons and spell knowledge need the Interact key (GDD 4: picking up the first gun is a deliberate step).
 * A pickup can set a progress flag when taken ("GotFirstGun"), which is how story gates are built.
 */
UCLASS()
class GLYPHBOUND_API AGBPickup : public AGBInteractableActor
{
	GENERATED_BODY()

public:
	AGBPickup();

	/** Spawns a pickup at Location (snapped to the ground). Returns null if the drop is empty or unknown. */
	static AGBPickup* SpawnPickup(UWorld* World, const FGBDropDefinition& Drop, const FVector& Location, FName FlagOnPickup = NAME_None);

	/** Rolls every drop's Chance and spawns the winners scattered around Location. Returns how many were spawned. */
	static int32 SpawnDrops(UWorld* World, TConstArrayView<FGBDropDefinition> Drops, const FVector& Location);

	const FGBDropDefinition& GetDrop() const { return Drop; }

	// IGBInteractable
	virtual bool CanInteract(const AGBPlayerCharacter* Player) const override;
	virtual void Interact(AGBPlayerCharacter* Player) override;
	virtual FString GetInteractPrompt(const AGBPlayerCharacter* Player) const override;

protected:
	virtual void BeginPlay() override;

	void InitPickup(const FGBDropDefinition& InDrop, FName InFlagOnPickup);
	/** Gives the content to the player. Returns false if nothing could be taken (inventory full). */
	bool GiveTo(AGBPlayerCharacter* Player);

	UFUNCTION()
	void HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UPROPERTY() FGBDropDefinition Drop;
	UPROPERTY() FName FlagOnPickup = NAME_None;
};
