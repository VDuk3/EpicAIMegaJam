// The look of the castle's pixel-art characters: an 8-direction animated sprite sheet (Content/Data/Sprites/<SheetId>.png
// + .json, format in Prompts/sprite_sheet_format.md) drawn on an upright quad that turns round the vertical axis to face
// the camera, lit by the scene (M_PixelVfxSpriteLit, so torches and the moon light it). The AI asks for clips by name;
// locomotion (Idle / Walk / Run) follows the owner's speed, a one-shot action (Attack, Cast, Leap...) plays over it, a
// short Hurt returns to locomotion, and Death is held on its last frame. Used by AGBSkeletalEnemy's sprite look.
#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GBSpriteAnimComponent.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UTexture2D;

/** One clip of a sheet: Frames rows from Row, one column per view direction. */
struct FGBSpriteClip
{
	int32 Row = 0;
	int32 Frames = 1;
	float Fps = 8.f;
	bool bLoop = false;
	/** Frame (0-based) at which the blow lands or the projectile leaves; -1 = none. */
	int32 HitFrame = -1;
	/** Where a projectile leaves, in cell pixels of the d=0 column. */
	bool bHasOrigin = false;
	FVector2D Origin = FVector2D::ZeroVector;
};

/** A sheet as described by its JSON. */
struct GLYPHBOUND_API FGBSpriteSheet
{
	FName Id = NAME_None;
	int32 CellWidth = 0;
	int32 CellHeight = 0;
	float PivotX = 0.f;
	float PivotY = 0.f;
	float PixelsPerMeter = 44.f;
	int32 Columns = 8;
	TMap<FName, FGBSpriteClip> Clips;

	bool LoadFromJsonString(const FString& Text);
	const FGBSpriteClip* FindClip(FName Clip) const { return Clips.Find(Clip); }
	/** Rows the clips use (the atlas must be at least this tall). */
	int32 RowCount() const;
};

/** Pure math of the sprite look (unit-tested in Tests/GBSpriteAnimTests.cpp). */
namespace GBSpriteAnim
{
	inline constexpr int32 NumDirections = 8;

	/**
	 * The sheet column (0..7) to show: the angle from the character's forward to the direction of the viewer, measured
	 * counter-clockwise seen from above (0 = the viewer sees its face, 2 = its left flank, 4 = its back, 6 = its right
	 * flank). Unreal yaw grows clockwise seen from above, so the angle is ActorYaw - ViewerBearing. PrevSector >= 0 keeps
	 * the previous column until the angle is HysteresisDegrees past the border between the two.
	 */
	GLYPHBOUND_API int32 ViewSector(float ActorYawDegrees, float ViewerBearingDegrees, int32 PrevSector, float HysteresisDegrees);

	/** Stepped frame of a clip at Seconds (negative = 0). Loops wrap; one-shots hold the last frame and set bOutFinished once it was shown for one frame. */
	GLYPHBOUND_API int32 FrameAtTime(int32 Frames, float Fps, bool bLoop, float Seconds, bool& bOutFinished);

	/** Seconds a one-shot plays (Frames / Fps), 0 for a bad clip. */
	GLYPHBOUND_API float ClipLength(int32 Frames, float Fps);

	/**
	 * Play rate and start time so that a one-shot's HitFrame begins HitInSeconds from now: rate = natural hit time / HitInSeconds
	 * clamped to [MinRate, MaxRate]; when the clamped rate still gets there early, the clip waits on frame 0 (OutStartSeconds < 0).
	 */
	GLYPHBOUND_API void TimeToHit(int32 HitFrame, float Fps, float HitInSeconds, float MinRate, float MaxRate, float& OutRate, float& OutStartSeconds);

	/** The clip that plays for a wanted one: itself when the sheet has it, else Attack2 -> Attack, Run -> Walk, Cast / Shoot / Leap -> Attack, Walk -> Idle; Hurt and others -> None. */
	GLYPHBOUND_API FName ResolveClip(const FGBSpriteSheet& Sheet, FName Wanted);
}

/** Loads sheets once per game (many enemies share one) and keeps their atlases alive. */
UCLASS()
class GLYPHBOUND_API UGBSpriteSheetSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UGBSpriteSheetSubsystem* Get(const UObject* WorldContext);
	static FString SheetDir();

	/** Null (one warning per id) when the files are missing or broken. */
	const FGBSpriteSheet* FindSheet(FName Id);
	UTexture2D* GetAtlas(FName Id);
	/** Why the last FindSheet / GetAtlas of this id failed (empty when it loaded). */
	FString GetFailure(FName Id) const { return Failures.FindRef(Id); }
	/** Forget the failures (a sheet baked after the first try is found on the next one). */
	void ClearMissing() { Failures.Reset(); }

private:
	bool Load(FName Id);

	UPROPERTY(Transient) TMap<FName, TObjectPtr<UTexture2D>> Atlases;
	/** Heap-held so the pointers handed to the components stay valid when the map grows. */
	TMap<FName, TSharedPtr<FGBSpriteSheet>> Sheets;
	TMap<FName, FString> Failures;
};

UCLASS(ClassGroup = "Glyphbound")
class GLYPHBOUND_API UGBSpriteAnimComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UGBSpriteAnimComponent();

	/** Loads the sheet and builds the quad. False (nothing is drawn) when the sheet is missing; the reason is in GetFailure. */
	bool Setup(FName SheetId, float InScale, const FLinearColor& InTint, float InGlow);
	bool IsReady() const { return Sheet != nullptr && Quad != nullptr; }
	FName GetSheetId() const { return SheetId; }
	const FString& GetFailure() const { return Failure; }
	bool HasClip(FName Clip) const;

	/** Locomotion: Idle below WalkSpeed, Walk below RunSpeed, Run above (cm/s of the owner's ground speed). */
	void SetLocomotionSpeeds(float InWalkSpeed, float InRunSpeed) { WalkSpeed = InWalkSpeed; RunSpeed = FMath::Max(InRunSpeed, InWalkSpeed + 1.f); }
	/** A clip that replaces Idle (Dormant while asleep); None = Idle. */
	void SetIdleOverride(FName Clip) { IdleOverride = Clip; }

	/**
	 * A one-shot action over the locomotion. HitInSeconds > 0 times the clip so its HitFrame begins then (the AI's windup);
	 * otherwise it plays at its own speed. Returns false when the sheet has no clip for it (after the fallbacks).
	 */
	bool PlayAction(FName Clip, float HitInSeconds = -1.f);
	void CancelAction();
	bool IsActionPlaying() const { return !ActionClip.IsNone(); }
	/** Seconds the playing action lasts from its start at its play rate (0 when none plays). */
	float GetActionLength() const;
	FName GetActionClip() const { return ActionClip; }
	/** True once each time a playing action crosses its HitFrame. */
	bool TakeHitFrame();
	/** A hit: the Hurt clip when nothing else plays (it returns to locomotion), and a short flash. */
	void NotifyHit();
	/** Death: played once and held on its last frame. Returns its length in seconds (0 without a Death clip: nothing changes). */
	float PlayDeath();
	bool IsDeathPlaying() const { return bDead; }
	/** World point where a projectile of this clip leaves (its Origin, seen from the front); false when the clip has none. */
	bool GetClipOriginWorld(FName Clip, FVector& OutWorld) const;
	/** World height of the sprite's cell (cm). */
	float GetWorldHeight() const;

	// ---- Smoke ----
	FName GetCurrentClip() const { return CurrentClip; }
	int32 GetCurrentFrame() const { return CurrentFrame; }
	/** Frames of a clip of the sheet (0 when it has none). */
	int32 GetClipFrames(FName Clip) const;
	int32 GetViewSector() const { return Sector; }
	int32 GetActionsPlayed() const { return ActionsPlayed; }
	int32 GetHitFramesCrossed() const { return HitFramesCrossed; }
	/** Seconds the Death clip has been held on its last frame (0 while it plays). */
	float GetDeathHeldSeconds() const;
	FString Describe() const;
	/** Plane UVs: true when the engine plane's U grows along its local +X (then the image is flipped to read unmirrored). */
	static bool PlaneUGrowsAlongX();

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;

private:
	void UpdateFrame(float DeltaTime);
	void UpdateQuad();
	void ApplyCell(int32 Row, int32 Column);
	FName PickLocomotion() const;

	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Quad;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> Material;

	FName SheetId = NAME_None;
	const FGBSpriteSheet* Sheet = nullptr;   // owned by the subsystem (never removed while the game runs)
	FString Failure;
	float Scale = 1.f;
	FLinearColor Tint = FLinearColor::White;
	float Glow = 0.f;
	float WalkSpeed = 30.f;
	float RunSpeed = 350.f;
	FName IdleOverride = NAME_None;

	FName LocoClip = NAME_None;
	float LocoTime = 0.f;
	FName ActionClip = NAME_None;
	float ActionTime = 0.f;
	float ActionRate = 1.f;
	bool bHitPending = false;
	bool bHitCrossed = false;
	FName HurtClip = NAME_None;
	float HurtTime = 0.f;
	bool bDead = false;
	FName DeathClip = NAME_None;
	float DeathTime = 0.f;
	float FlashTime = 0.f;

	FName CurrentClip = NAME_None;
	int32 CurrentFrame = -1;
	int32 Sector = -1;
	int32 AppliedRow = -1;
	int32 AppliedColumn = -1;
	float AppliedGlow = -1.f;
	int32 ActionsPlayed = 0;
	int32 HitFramesCrossed = 0;
};
