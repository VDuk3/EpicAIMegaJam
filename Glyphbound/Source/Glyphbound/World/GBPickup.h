// Things lying on the floor: ammo and items dropped by enemies, guns, staffs, spell knowledge.
#pragma once

#include "CoreMinimal.h"
#include "World/GBInteractable.h"
#include "Data/GBDataTypes.h"
#include "GBPickup.generated.h"

/** Keys of the item / weapon "Strings" maps read by pickups. */
namespace GBPickupString
{
	/** "3D" forces the 3D placeholder box even if the item has a Sprite (cyberpunk items whose Sprite is only their inventory icon). */
	inline const FName PickupLook(TEXT("PickupLook"));
}

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

	/**
	 * Gives one drop to the player with the pickup rules (item -> inventory, weapon -> equipment, spell group ->
	 * spellbook). Returns true if anything was taken. For items, InOutDrop.Count is reduced by what was taken;
	 * bOutAllTaken tells whether nothing is left. Shared by pickups and chests.
	 */
	static bool GiveDrop(AGBPlayerCharacter* Player, FGBDropDefinition& InOutDrop, bool& bOutAllTaken);

	/** English name of a drop for the HUD ("Pistol", "2x Health Potion", "Fire spellbook"). */
	static FString GetDropDisplayName(const UObject* WorldContextObject, const FGBDropDefinition& Drop, bool bWithCount = false);

	/** True if this drop's id exists in the data (and the drop is not empty). */
	static bool IsDropValid(const UObject* WorldContextObject, const FGBDropDefinition& Drop);

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

	/** Items and ammo are taken by walking over them; weapons and spell knowledge need Interact. */
	bool IsWalkOver() const;
	/** Sprite or 3D box per the art rule (GDD 3). */
	void ApplyLook();
	/** Gives, sets the flag, shows the message, destroys when empty. */
	void TryPickUp(AGBPlayerCharacter* Player);

	UPROPERTY() FGBDropDefinition Drop;
	UPROPERTY() FName FlagOnPickup = NAME_None;
};
