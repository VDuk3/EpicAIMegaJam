// Caption cutscenes (GDD 4): English lines over a dimmed screen, the player locked and invulnerable meanwhile.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Data/GBDataTypes.h"
#include "GBCutsceneSubsystem.generated.h"

class AGBPlayerCharacter;

/** Keys of Game.json "Values" read by cutscenes (seconds). */
namespace GBCutsceneKey
{
	/** Time after the start before Interact / Esc can skip the rest (so the first press that started it does not skip it). */
	inline const FName SkipDelay(TEXT("CutsceneSkipDelay"));
}

/**
 * The smallest cutscene system the GDD needs. Data: Content/Data/Cutscenes.json (id, lines with text and seconds,
 * optional TriggerFlag = plays by itself the first time that progress flag is set, e.g. "GotFirstGun").
 * While one plays the player is held by the cutscene's OWN locks (SetCutsceneLocked on the player, cutscene
 * invulnerability on the attributes, cutscene switch lock on the equipment), separate from the flags other systems
 * set, so nothing is saved and restored (a spell QTE or an era travel finishing mid-caption cannot be undone by it).
 * A running QTE is cancelled and an open menu closed when a caption starts. Time is real (undilated) time.
 * Death cancels it: the OnFinished callback does NOT run, OnCancelled does (also for queued ones). Skippable with Interact or Esc after CutsceneSkipDelay seconds
 * (skips every remaining line). The HUD draws it (AGBHUD::DrawCutscene) from the getters below.
 * Cutscenes asked while another one plays, or while an era travel runs, wait their turn.
 */
UCLASS()
class GLYPHBOUND_API UGBCutsceneSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UGBCutsceneSubsystem* Get(const UObject* WorldContextObject);

	/**
	 * Plays cutscene Id. OnFinished (optional) runs when it ends, skipped or not, after the player is given back.
	 * An unknown or empty cutscene logs a warning and runs OnFinished at once. Returns true if it started or was queued.
	 */
	bool Play(FName Id, TFunction<void()> OnFinished = nullptr, TFunction<void()> OnCancelled = nullptr);

	bool IsPlaying() const { return bPlaying; }
	FName GetPlayingId() const { return PlayingId; }
	int32 GetLineIndex() const { return LineIndex; }
	int32 GetNumLines() const { return Lines.Num(); }
	const FString& GetLineText() const;
	/** Seconds the current line has been up, and how long it stays. */
	float GetLineElapsed() const { return LineElapsed; }
	float GetLineSeconds() const;
	/** Seconds since the cutscene started. */
	float GetTotalElapsed() const { return TotalElapsed; }
	/** True once Interact / Esc can skip. */
	bool CanSkip() const { return bPlaying && TotalElapsed >= SkipDelay; }

	/** Interact or Esc: skips the rest if CanSkip(); otherwise nothing. Returns true if the press was used (a cutscene is playing). */
	bool RequestSkip();

	/** Ends it now as if finished (OnFinished runs). */
	void Stop();
	/** Cancels the running one and clears the queue: their OnFinished never runs, their OnCancelled does. Player death does this by itself. */
	void Cancel();

	// UTickableWorldSubsystem
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

protected:
	struct FQueued
	{
		FName Id = NAME_None;
		TFunction<void()> OnFinished;
		TFunction<void()> OnCancelled;
	};

	void Begin(const FQueued& Entry);
	void Finish();
	/** Gives the player back (cutscene locks off). */
	void ReleasePlayer();
	void HandleFlagChanged(FName Flag);

	bool bPlaying = false;
	FName PlayingId = NAME_None;
	TArray<FGBCutsceneLine> Lines;
	int32 LineIndex = 0;
	float LineElapsed = 0.f;
	float TotalElapsed = 0.f;
	float SkipDelay = 1.f;
	TFunction<void()> PlayingOnFinished;
	TFunction<void()> PlayingOnCancelled;
	TArray<FQueued> Queue;

	TWeakObjectPtr<AGBPlayerCharacter> LockedPlayer;

	FDelegateHandle FlagChangedHandle;
};
