// Runs the spell QTE on the player: slows time, routes input into FGBQTESession, reports the result.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "QTE/GBQTELogic.h"
#include "GBQTEComponent.generated.h"

DECLARE_DELEGATE_OneParam(FGBOnQTEFinished, const FGBQTEResult& /*Result*/);

/** Extra QTE.json "Stats" keys (recognizer thresholds), read through the owner's stats like the GBStat::QTE* ones. */
namespace GBQTEStat
{
	inline const FName GestureMinScore(TEXT("QTE.GestureMinScore"));             // a gesture below this counts as missed
	inline const FName GestureMinLength(TEXT("QTE.GestureMinLength"));           // pixels before a stroke is judged
	inline const FName GestureIdleSubmitTime(TEXT("QTE.GestureIdleSubmitTime")); // real seconds without input -> auto-submit
	inline const FName LineSubmitLengthMult(TEXT("QTE.LineSubmitLengthMult"));   // line auto-submits at GestureMinLength * this
	inline const FName StickGestureScale(TEXT("QTE.StickGestureScale"));         // right stick: pixels per second at full tilt
	inline const FName FailManaRefund(TEXT("QTE.FailManaRefund"));               // share of the mana cost given back when the spell fizzles (0 = no refund)
	inline const FName FailCooldownMult(TEXT("QTE.FailCooldownMult"));           // the cooldown of a fizzled spell is multiplied by this (1 = full cooldown)
	inline const FName ChoiceTimeLimit(TEXT("QTE.ChoiceTimeLimit"));             // real seconds for a Choice step (the enchant's element)
}

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

	/**
	 * Starts a QTE for SpellId. OnFinished fires exactly once (possibly before this returns, when the QTE is off). Returns
	 * false if one is already running. ChoiceKeys, when given, adds a first step that picks one option per key (the
	 * weapon enchant's element); the result's Choice says which (-1 = none, or the QTE is off).
	 */
	bool StartQTE(FName SpellId, FGBOnQTEFinished OnFinished, const TArray<EGBQTEKey>& ChoiceKeys = TArray<EGBQTEKey>());

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

	/** Result of the last finished QTE (also passed to its delegate). */
	const FGBQTEResult& GetLastResult() const { return LastResult; }

	/** QTE.StickGestureScale: the player character sends right-stick value * this * real delta seconds to HandleGestureInput. */
	float GetStickGestureScale() const;
	/** Share of the mana cost refunded when the spell fizzles (QTE.FailManaRefund, 0 = no refund). */
	float GetFailManaRefund() const;
	/** Multiplier on the cooldown of a fizzled spell (QTE.FailCooldownMult, 1 = full cooldown, 0 = no cooldown). */
	float GetFailCooldownMult() const;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void Finish();

	/** QTE.json "Stats"[Key] (Default when missing, with a warning), then the owner's modifiers for Key. */
	float ReadStat(FName Key, float Default) const;
	FGBQTEConfig BuildConfig() const;

	FGBQTESession Session;
	FGBQTEResult LastResult;
	/** Read at StartQTE so gear changes apply to the next QTE. */
	float GestureIdleSubmitTime = 0.f;	float LineSubmitLengthMult = 0.f;
	FGBOnQTEFinished FinishedDelegate;
	FName ActiveSpellId = NAME_None;
	bool bActive = false;
	/** The next Finish ends a CancelQTE: its result is forced to unsuccessful, whatever the score so far. */
	bool bForceFailResult = false;
	/** Real seconds without gesture input, to auto-submit a stroke when the hand pauses. */
	float GestureIdleTime = 0.f;
	FRandomStream Random;
};
