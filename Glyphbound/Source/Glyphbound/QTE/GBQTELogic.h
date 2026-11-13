// Pure logic of the spell-casting quick-time event (GDD 7). No engine world needed: unit-tested.
#pragma once

#include "CoreMinimal.h"
#include "GBQTELogic.generated.h"

/** Part 1 inputs: W A S D on keyboard, the four face buttons on gamepad. */
UENUM(BlueprintType)
enum class EGBQTEKey : uint8
{
	Up,     // W / Triangle (Y)
	Left,   // A / Square (X)
	Down,   // S / Cross (A)
	Right   // D / Circle (B)
};

/** Part 2 gestures, drawn with the mouse or the right stick. Screen space: +X right, +Y down. */
UENUM(BlueprintType)
enum class EGBGesture : uint8
{
	LineRight,
	LineLeft,
	LineDown,
	LineUp,
	CircleClockwise,
	CircleCounterClockwise
};

UENUM(BlueprintType)
enum class EGBQTEPhase : uint8
{
	Inactive,
	Sequence,   // part 1 -> power
	Gesture,    // part 2 -> reach
	Finished,
	Choice      // optional step before part 1: pick one of fixed options (the weapon enchant's element)
};

/** Outcome of one QTE. */
USTRUCT(BlueprintType)
struct GLYPHBOUND_API FGBQTEResult
{
	GENERATED_BODY()

	/** Part 1 score 0..1 = right steps / steps. */
	UPROPERTY() float Power = 0.f;
	/** Part 2 score 0..1 = average gesture score. */
	UPROPERTY() float Reach = 0.f;
	/** False when the QTE is off: the spell casts at its fixed default strength. */
	UPROPERTY() bool bUsedQTE = false;
	/** "The spell is cast if the player gets at least one step right" (always true when the QTE is off). */
	UPROPERTY() bool bSuccess = false;
	UPROPERTY() bool bTimedOut = false;
	/** Index into FGBQTEConfig::ChoiceKeys of the option picked in the Choice step; -1 = none picked (no step, or its time ran out). */
	UPROPERTY() int32 Choice = -1;
};

/** Numbers of one QTE run, read from QTE.json (through the player's stats, so gear can change them). */
struct GLYPHBOUND_API FGBQTEConfig
{
	int32 SequenceLength = 4;
	int32 GestureCount = 1;
	/** Real seconds for the whole QTE, both parts together. */
	float TimeLimit = 3.f;
	/** A gesture scoring below this counts as a missed step. */
	float GestureMinScore = 0.35f;
	/** Minimum path length (pixels, or stick units scaled by the component) before a gesture is judged. */
	float GestureMinLength = 120.f;
	/** Gestures that may be asked for. Empty = all. */
	TArray<EGBGesture> AllowedGestures;
	/**
	 * Optional first step: one key per option, always the same keys in the same order (owner: the enchant's elements never
	 * move). Empty = no Choice step. It has its own time limit, so choosing never eats the time of part 1 and part 2.
	 */
	TArray<EGBQTEKey> ChoiceKeys;
	/** Real seconds for the Choice step; when it runs out nothing is picked (Choice = -1) and part 1 starts. */
	float ChoiceTimeLimit = 3.f;
};

/**
 * Scores how well a drawn path matches a gesture, 0..1. Rules that the tests pin down:
 *  - a straight stroke in the asked direction scores near 1; the opposite direction scores 0;
 *  - a full circle in the asked rotation scores near 1; the opposite rotation scores 0;
 *  - a path shorter than MinLength scores 0 (not judged yet).
 * Points are in screen space (+Y down); CircleClockwise is clockwise as seen on screen.
 */
struct GLYPHBOUND_API FGBGestureRecognizer
{
	static float Score(TConstArrayView<FVector2D> Points, EGBGesture Gesture, float MinLength);
	static float PathLength(TConstArrayView<FVector2D> Points);
	/** Signed total turning of the path in degrees; positive = clockwise on screen. */
	static float TotalTurnDegrees(TConstArrayView<FVector2D> Points);
};

/**
 * State machine of one QTE run. The component feeds it input and real time; the HUD reads it.
 *   Start -> phase Sequence: each key press consumes one step, right or wrong (a mistake lowers the result of its part)
 *         -> phase Gesture: points are accumulated; SubmitGesture() judges the current stroke as one step
 *         -> Finished when every step is consumed or the time limit runs out (unfinished steps count as missed).
 */
struct GLYPHBOUND_API FGBQTESession
{
	void Start(const FGBQTEConfig& InConfig, FRandomStream& Random);

	/** Choice step: a key of Config.ChoiceKeys picks that option and part 1 starts; any other key is ignored. Part 1: returns
	 *  true if the key was the expected one. Ignored in the other phases. */
	bool PressKey(EGBQTEKey Key);

	/** Part 2. Adds a movement delta to the current stroke. Ignored outside the Gesture phase. */
	void AddGestureDelta(const FVector2D& Delta);
	/** Judges the current stroke as one gesture step and moves on. Returns its score. */
	float SubmitGesture();
	/** True once the current stroke is long enough to be judged (the component auto-submits when the input pauses or closes a circle). */
	bool IsGestureReadyToJudge() const;

	/** Advances real time. Finishes the session when the limit is reached. */
	void Advance(float RealDeltaSeconds);

	bool IsFinished() const { return Phase == EGBQTEPhase::Finished; }
	FGBQTEResult GetResult() const;

	// ---- Read by the HUD ----
	EGBQTEPhase Phase = EGBQTEPhase::Inactive;
	FGBQTEConfig Config;
	TArray<EGBQTEKey> Sequence;
	/** One entry per consumed step of part 1: was it right. */
	TArray<bool> SequenceResults;
	TArray<EGBGesture> Gestures;
	/** One entry per judged gesture: its score 0..1 (0 = missed). */
	TArray<float> GestureScores;
	/** The stroke being drawn, in accumulated screen units starting at (0,0). */
	TArray<FVector2D> CurrentStroke;
	float TimeElapsed = 0.f;
	bool bTimedOut = false;
	/** Choice step: the picked index into Config.ChoiceKeys (-1 = none yet) and the real seconds spent on it. */
	int32 ChosenIndex = -1;
	float ChoiceElapsed = 0.f;
	float GetChoiceTimeRemaining() const { return FMath::Max(0.f, Config.ChoiceTimeLimit - ChoiceElapsed); }

	float GetTimeRemaining() const { return FMath::Max(0.f, Config.TimeLimit - TimeElapsed); }

private:
	/** The phase part 1 starts in (or part 2, or Finished when there are no steps). */
	EGBQTEPhase FirstStepPhase() const;
};
