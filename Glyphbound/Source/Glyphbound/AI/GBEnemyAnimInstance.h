// Animation of 3D enemies without any AnimBlueprint asset: a native UAnimInstance whose proxy samples the enemy's
// UAnimSequence clips itself (GetAnimationPose) and blends them (locomotion by speed and direction, a one-shot
// overlay, death), then bends the spine for the aim and adds procedural kicks (recoil, hit flinch) on top.
// Every clip is optional; with none at all the mesh stays in its reference pose. Used by AGBSkeletalEnemy.
#pragma once

#include "CoreMinimal.h"
#include <atomic>
#include "Animation/AnimInstance.h"
#include "GBEnemyAnimInstance.generated.h"

class UAnimSequence;

/** The clips of an enemy, in the order of Enemies.json "Anim.<Name>". */
enum class EGBEnemyClip : uint8
{
	Idle,
	Walk,
	Run,
	StrafeLeft,
	StrafeRight,
	WalkBack,
	Attack,
	Hit,
	Death,
	// Optional extras (Enemies.json Anim.RelaxedIdle / PatrolWalk / Sprint / CrouchIdle / CrouchWalk): they replace a locomotion slot by posture.
	RelaxedIdle,
	PatrolWalk,
	Sprint,
	CrouchIdle,
	CrouchWalk,
	// Full-body one-shot actions of a boss (Enemies.json Anim.Action0 .. Anim.Action5): played with PlayOneShot, never cut by a hit reaction.
	Action0,
	Action1,
	Action2,
	Action3,
	Action4,
	Action5,
	Count
};

namespace GBEnemyAnim
{
	inline constexpr int32 NumClips = static_cast<int32>(EGBEnemyClip::Count);
	/** Locomotion clips are the first six (Idle .. WalkBack). */
	inline constexpr int32 NumLocomotion = 6;
	GLYPHBOUND_API const TCHAR* ClipName(EGBEnemyClip Clip);
}

/** Numbers of the procedural part; the enemy's stats overwrite them (Enemies.json "Stats"). */
struct FGBEnemyAnimTuning
{
	float WalkAnimSpeed = 150.f;     // cm/s at which the Walk clip's feet match the ground
	float RunAnimSpeed = 400.f;      // cm/s at which the Run clip's feet match the ground
	float BlendTime = 0.15f;         // s of the crossfade when the dominant locomotion clip changes
	float OverlayBlend = 0.1f;       // s blending a one-shot in and out
	float AimSmooth = 14.f;          // 1/s, how fast the aim follows
	float RecoilPitch = 5.f;         // degrees of upper body pitch of one shot
	float RecoilDecay = 14.f;        // 1/s
	float FlinchPitch = 10.f;        // degrees
	float FlinchYaw = 7.f;           // degrees
	float FlinchDecay = 8.f;         // 1/s
	float MeshYaw = -90.f;           // the mesh component's yaw in the actor (the bend axes are turned by it)
	float MeshScale = 1.f;           // the mesh component's scale (authored ground speed is measured in its units)
	float AnimRateMin = 0.7f;        // locomotion play rate limits: a little foot slide is better than slow motion
	float AnimRateMax = 1.35f;
	float MoveBlendSpeed = 60.f;     // cm/s of ground speed at which the idle clip has faded out completely
	float DriftMin = 15.f;           // cm: a clip whose travel bone moves less than this has no drift
	FName TravelBone = FName(TEXT("pelvis"));   // the bone that carries a clip's travel (the root bone never moves in these clips)
};

/** What the game thread measured on each clip when it was configured (the proxy reads it on the animation thread). */
struct FGBClipInfo
{
	/** Travel bone, last frame minus first frame, in the bone's local space. */
	FVector Drift = FVector::ZeroVector;
	/** Travel bone at time 0 (the attack overlay is pinned to it). */
	FVector BoneStart = FVector::ZeroVector;
	bool bHasDrift = false;
	bool bHasBone = false;
	/** cm/s the clip was authored for: drift length x mesh scale / clip length; the stat fallback for clips without drift. */
	float RefSpeed = 150.f;
	float Length = 0.f;
};

/**
 * A two-bone IK goal for one arm (upperarm, lowerarm, hand), in the mesh component's space (unscaled mesh cm).
 * The hand keeps its rotation relative to the forearm unless bRotation gives one in component space.
 */
struct FGBArmIK
{
	float Alpha = 0.f;               // 0 = the clip's arm, 1 = the goal
	/** False: Location is a point. True: Location is a direction from the upper arm's root, its length a fraction of the arm's reach. */
	bool bFromShoulder = false;
	FVector Location = FVector::ZeroVector;
	bool bRotation = false;
	FQuat Rotation = FQuat::Identity;
	/** Component-space direction, from the shoulder, towards which the elbow bends. */
	FVector Pole = FVector(0.f, 0.f, -1.f);
	/** Degrees the recoil kick turns this arm's aim up (0 for an arm that does not shoot). */
	float RecoilDegrees = 0.f;
};

/** What the game thread hands to the proxy each frame. */
struct FGBEnemyAnimFrame
{
	float Target[GBEnemyAnim::NumLocomotion] = { 1.f, 0.f, 0.f, 0.f, 0.f, 0.f };
	float Speed = 0.f;
	int32 OverlayClip = -1;          // EGBEnemyClip index of the running one-shot, -1 = none
	float OverlayTime = 0.f;
	float OverlayWeight = 0.f;
	bool bOverlayUpperBody = true;
	bool bDead = false;
	float DeathTime = 0.f;
	float DeathWeight = 0.f;
	float AimPitch = 0.f;            // degrees, positive = looking up
	float AimYaw = 0.f;              // degrees offset of the torso towards the target
	float RecoilKick = 0.f;          // 0..1, decaying
	float FlinchKick = 0.f;          // 0..1, decaying
	float FlinchSide = 1.f;          // +1 / -1
	float PoseAlpha = 0.f;           // 0..1: how much of the data-driven pose layer (a raised shield) is applied
	FGBArmIK Arm[2];                 // two-bone IK of the left (0) and right (1) arm, applied last
};

/** One bone rotation of the pose layer (Enemies.json "Pose.ShieldRaised" = "bone:pitch,yaw,roll;bone:..."). */
struct FGBPoseOffset
{
	FName Bone = NAME_None;
	FRotator Delta = FRotator::ZeroRotator;
};

UCLASS()
class GLYPHBOUND_API UGBEnemyAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	/** Sets the clips (null entries are allowed) and the tuning. */
	void Configure(const TArray<UAnimSequence*>& InClips, const FGBEnemyAnimTuning& InTuning);

	bool HasClip(EGBEnemyClip Clip) const;
	/** Unaware (relaxed idle / patrol walk) and crouched (crouch idle / walk): the clips used in the locomotion slots. Missing clips change nothing. */
	void SetPosture(bool bInRelaxed, bool bInCrouched) { bRelaxed = bInRelaxed; bCrouched = bInCrouched; }
	/** Parses a pose layer string and keeps it; PoseTarget (0..1) is blended to over BlendSeconds. */
	void SetPoseLayer(const FString& Spec);
	void SetPoseTarget(float Target, float BlendSeconds) { PoseTarget = Target; PoseBlend = FMath::Max(BlendSeconds, 0.01f); }
	/** Two-bone IK of an arm (0 left, 1 right); Alpha 0 turns it off. While an arm is driven, shots and hits use the procedural kicks, not clips. */
	void SetArmIK(int32 Side, const FGBArmIK& Goal) { if (Side == 0 || Side == 1) { Frame.Arm[Side] = Goal; } }
	bool HasArmIK() const { return Frame.Arm[0].Alpha > 0.f || Frame.Arm[1].Alpha > 0.f; }
	const TArray<FGBPoseOffset>& GetPoseOffsets() const { return PoseOffsets; }
	/** Closes the right hand's fingers round a held weapon: a fixed curl (degrees about the given pitch/yaw/roll axis of each bone) on the finger and thumb bones. */
	void SetGripCurl(float FingerDegrees, const FVector& FingerAxis, float ThumbDegrees, const FVector& ThumbAxis);
	const TArray<FGBPoseOffset>& GetGripOffsets() const { return GripOffsets; }
	bool HasGrip() const { return GripOffsets.Num() > 0; }
	/** Smoke: the index of the running one-shot clip (EGBEnemyClip value) or -1. */
	int32 GetOverlayClipIndex() const { return Frame.OverlayClip; }
	/** The slot a locomotion slot currently plays (posture and sprint substitutes). */
	int32 ResolveSlot(int32 Slot) const;
	int32 GetClipCount() const;

	/** Aim of the upper body: pitch in degrees (positive = up), yaw offset in degrees. Smoothed. */
	void SetAim(float PitchDegrees, float YawDegrees) { AimPitchTarget = PitchDegrees; AimYawTarget = YawDegrees; }

	/** One-shot overlay clip over the locomotion. Returns false when the clip does not exist. */
	bool PlayOneShot(EGBEnemyClip Clip, bool bUpperBodyOnly);
	/** A shot was fired: the Attack clip (upper body) when there is one, a recoil kick otherwise. */
	void NotifyShot();
	/** The enemy was hit: the Hit clip when there is one, a flinch otherwise. Side picks the flinch direction. */
	void NotifyHit(float Side);
	/** Death pose: the Death clip, played once and held. Returns false when there is no clip (nothing changes). */
	bool PlayDeath();
	/** Smoke / death timing: the death pose is on, the length of the Death clip (0 without one), seconds since it started. */
	bool IsDead() const { return Frame.bDead; }
	float GetDeathClipLength() const;
	float GetDeathTime() const { return Frame.DeathTime; }

	/** Smoke: the dominant locomotion clip right now, the running overlay (or "-"), as one line. */
	FString DescribeClips() const;
	EGBEnemyClip GetDominantLocomotion() const { return Dominant; }
	/** Smoke: the pose sampled the clips at least once (the proxy ran). */
	int32 GetEvaluateCount() const { return EvaluateCount.load(); }
	void CountEvaluate() { ++EvaluateCount; }

	const FGBEnemyAnimFrame& GetFrame() const { return Frame; }
	const FGBEnemyAnimTuning& GetTuning() const { return Tuning; }
	const FGBClipInfo& GetClipInfo(int32 Index) const { return ClipInfo[Index]; }
	/** Smoke / log: the authored speed of a locomotion clip (the stat fallback when it has no drift). */
	float GetClipRefSpeed(EGBEnemyClip Clip) const { return ClipInfo[static_cast<int32>(Clip)].RefSpeed; }
	bool ClipHasDrift(EGBEnemyClip Clip) const { return ClipInfo[static_cast<int32>(Clip)].bHasDrift; }
	UAnimSequence* GetClip(EGBEnemyClip Clip) const;

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

private:
	void ComputeLocomotionTargets(float Speed, float LocalAngleDegrees);
	bool bRelaxed = false;
	bool bCrouched = false;
	bool bUseSprint = false;
	TArray<FGBPoseOffset> PoseOffsets;
	TArray<FGBPoseOffset> GripOffsets;
	float PoseTarget = 0.f;
	float PoseBlend = 0.3f;

	UPROPERTY(Transient) TArray<TObjectPtr<UAnimSequence>> Clips;

	FGBEnemyAnimTuning Tuning;
	FGBClipInfo ClipInfo[GBEnemyAnim::NumClips];
	FGBEnemyAnimFrame Frame;
	float AimPitchTarget = 0.f;
	float AimYawTarget = 0.f;
	EGBEnemyClip Dominant = EGBEnemyClip::Idle;
	std::atomic<int32> EvaluateCount{ 0 };
};
