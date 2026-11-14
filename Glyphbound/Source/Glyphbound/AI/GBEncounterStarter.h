// Starts an encounter when the player interacts with it (the Valkyrie's rune cage, the cryo tube console).
#pragma once

#include "CoreMinimal.h"
#include "World/GBInteractable.h"
#include "GBEncounterStarter.generated.h"

class AGBEncounter;

/**
 * Placed in the level next to an AGBEncounter (set Encounter on the instance). While the encounter is waiting, not
 * cleared and present in the current era, the player can use it:
 *  - all of the encounter's RequiredFlags set: the prompt is Prompt and Interact calls StartEncounter();
 *  - some missing: the prompt is LockedText and Interact only shows it as a message.
 * (Deviation from the brief, on purpose: if CanInteract were false while flags are missing, the player could never
 * see LockedText.) The look is a small console placeholder; turn it off to put your own mesh there.
 */
UCLASS()
class GLYPHBOUND_API AGBEncounterStarter : public AGBInteractableActor
{
	GENERATED_BODY()

public:
	AGBEncounterStarter();

	/** The encounter this starts. Must be an actor placed in the same level. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glyphbound|Encounter Starter")
	TObjectPtr<AGBEncounter> Encounter;

	/** Shown when it can be used. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glyphbound|Encounter Starter")
	FString Prompt = TEXT("Begin");

	/** Shown (as prompt and as message) while the encounter's required flags are not all set. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glyphbound|Encounter Starter")
	FString LockedText = TEXT("Something still holds it shut");

	/** A small placeholder console. Off = invisible (your own mesh stands there). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glyphbound|Encounter Starter")
	bool bShowPlaceholder = true;

	/** True if the encounter exists, is waiting, and is present in the current era (flags aside). */
	bool IsEncounterAvailable() const;
	/** True if it is available and every required flag is set: Interact will start it. */
	bool IsReady() const;

	// IGBInteractable
	virtual bool CanInteract(const AGBPlayerCharacter* Player) const override;
	virtual void Interact(AGBPlayerCharacter* Player) override;
	virtual FString GetInteractPrompt(const AGBPlayerCharacter* Player) const override;

protected:
	virtual void BeginPlay() override;
};
