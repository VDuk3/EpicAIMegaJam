// The seal of the Seal Room (GDD 4, "Opening sequence" step 3): touching it, once, sends the player to the future.
#pragma once

#include "CoreMinimal.h"
#include "World/GBInteractable.h"
#include "GBSeal.generated.h"

/** Progress flags and cutscene ids used by the seal. */
namespace GBSealName
{
	inline const FName SealUsedFlag(TEXT("SealUsed"));
	inline const FName DefaultCutscene(TEXT("Seal"));
}

/**
 * Present only in the past (editable on the instance like every era rule), usable once:
 * Interact sets SealUsed (so it cannot be used twice), plays the caption cutscene (default "Seal"), and when it ends
 * asks UGBEraSubsystem for the travel to the future, retrying every tick until it is accepted (another travel may be
 * running), WITHOUT teleporting: the player stays where it stands, the
 * future's Seal Vault is the same room. Placeholder look: a flat glowing purple disc on the floor, about 2 m wide;
 * it goes dark once used.
 * If the player dies before the travel starts (during the caption or while it waits), the seal is given back:
 * SealUsed is cleared so it can be used again after the respawn.
 */
UCLASS()
class GLYPHBOUND_API AGBSeal : public AGBInteractableActor
{
	GENERATED_BODY()

public:
	AGBSeal();

	/** Cutscene played on use (Cutscenes.json). Empty or unknown = travel at once. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glyphbound|Seal")
	FName CutsceneId = GBSealName::DefaultCutscene;

	/**
	 * Optional: the map to open instead of the travel in place (a long package name, for example
	 * /Game/Glyphbound/Maps/FutureBase). For a site whose past and future are two maps: after the cutscene the game
	 * opens that map, which starts in its own era. Empty, or not a map that exists = the travel in place.
	 * Only the progress flags cross to the new map (the same hand-over gap as AGBFinalPortal).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glyphbound|Seal")
	FString TravelMapPath;

	/** True if TravelMapPath names a map package this build can open (loose in the project, or cooked into the package). */
	bool HasTravelMap() const;

	/** True once the SealUsed flag is set. */
	bool IsUsed() const;

	virtual void Tick(float DeltaSeconds) override;

	// IGBInteractable
	virtual bool CanInteract(const AGBPlayerCharacter* Player) const override;
	virtual void Interact(AGBPlayerCharacter* Player) override;
	virtual FString GetInteractPrompt(const AGBPlayerCharacter* Player) const override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void HandleFlagChanged(FName Flag);
	void UpdateLook();
	/** After the cutscene: the travel to the future, in place; kept pending (ticking) until the era subsystem accepts it. */
	void BeginPendingTravel(AGBPlayerCharacter* Player);
	/** Gives the seal back (SealUsed cleared): the caption was cancelled or the player died before the travel. */
	void ResetUse();

	bool bTravelPending = false;
	TWeakObjectPtr<AGBPlayerCharacter> PendingPlayer;

	FDelegateHandle FlagChangedHandle;
};
