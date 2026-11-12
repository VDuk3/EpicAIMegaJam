// Runs the spell QTE on the player: slows time, routes input into FGBQTESession, reports the result.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "QTE/GBQTELogic.h"
#include "GBQTEComponent.generated.h"

DECLARE_DELEGATE_OneParam(FGBOnQTEFinished, const FGBQTEResult& /*Result*/);

/**
 * GDD 7 requirements handled here:
 *  - time slows to the QTE.TimeDilation stat (read through the owner's UGBStatsComponent, so gear can change it);
 *  - the 3 s limit (QTE.TimeLimit) is real time and covers both parts;
 *  - it can be switched off (QTE.json "bEnabled": false, or console "gb.QTE 0"): StartQTE then finishes at once
 *    with bUsedQTE = false and the spell casts at its default strength.
 * The player character asks IsActive() to lock the camera, hold the last movement input and send
 * W/A/S/D + mouse / face buttons + right stick here instead of to movement and look.
 */
UCLASS(ClassGroup = (Glyphbound), meta = (BlueprintSpawnableComponent))
class GLYPHBOUND_API UGBQTEComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGBQTEComponent();

	/** Starts a QTE for SpellId. OnFinished fires exactly once (possibly before this returns, when the QTE is off). Returns false if one is already running. */
	bool StartQTE(FName SpellId, FGBOnQTEFinished OnFinished);

	/** Ends the running QTE now; unfinished steps count as missed (death, cutscene). */
	void CancelQTE();

	bool IsActive() const { return bActive; }
	bool IsEnabled() const;
	FName GetSpellId() const { return ActiveSpellId; }

	// ---- Input (from the player character) ----
	void HandleKey(EGBQTEKey Key);
	/** Mouse delta in pixels, or right-stick value * a per-second scale, for part 2. */
	void HandleGestureInput(const FVector2D& Delta);

	/** The running session, for the HUD. Only meaningful while IsActive(). */
	const FGBQTESession& GetSession() const { return Session; }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void Finish();

	FGBQTESession Session;
	FGBOnQTEFinished FinishedDelegate;
	FName ActiveSpellId = NAME_None;
	bool bActive = false;
	/** Real seconds without gesture input, to auto-submit a stroke when the hand pauses. */
	float GestureIdleTime = 0.f;
	FRandomStream Random;
};
