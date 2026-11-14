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
};

UCLASS()
class GLYPHBOUND_API UGBEnemyAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	/** Sets the clips (null entries are allowed) and the tuning. */
	void Configure(const TArray<UAnimSequence*>& InClips, const FGBEnemyAnimTuning& InTuning);

	bool HasClip(EGBEnemyClip Clip) const;
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

	/** Smoke: the dominant locomotion clip right now, the running overlay (or "-"), as one line. */
	FString DescribeClips() const;
	EGBEnemyClip GetDominantLocomotion() const { return Dominant; }
	/** Smoke: the pose sampled the clips at least once (the proxy ran). */
	int32 GetEvaluateCount() const { return EvaluateCount.load(); }
	void CountEvaluate() { ++EvaluateCount; }

	const FGBEnemyAnimFrame& GetFrame() const { return Frame; }
	const FGBEnemyAnimTuning& GetTuning() const { return Tuning; }
	UAnimSequence* GetClip(EGBEnemyClip Clip) const;

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

private:
	void ComputeLocomotionTargets(float Speed, float LocalAngleDegrees);

	UPROPERTY(Transient) TArray<TObjectPtr<UAnimSequence>> Clips;

	FGBEnemyAnimTuning Tuning;
	FGBEnemyAnimFrame Frame;
	float AimPitchTarget = 0.f;
	float AimYawTarget = 0.f;
	EGBEnemyClip Dominant = EGBEnemyClip::Idle;
	std::atomic<int32> EvaluateCount{ 0 };
};
