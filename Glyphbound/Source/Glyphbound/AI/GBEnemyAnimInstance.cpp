#include "AI/GBEnemyAnimInstance.h"

#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/AnimationPoseData.h"
#include "AnimationRuntime.h"
#include "BoneContainer.h"
#include "BonePose.h"
#include "GameFramework/Pawn.h"

namespace GBEnemyAnimLocal
{
	// The bones of the UE5 mannequin skeleton that the retargeted humanoids use. Bones that do not exist are skipped
	// (the spider has its own skeleton: it gets no aim bend).
	const FName SpineBones[] = { FName(TEXT("spine_01")), FName(TEXT("spine_02")), FName(TEXT("spine_03")), FName(TEXT("spine_04")), FName(TEXT("spine_05")), FName(TEXT("neck_01")) };
	const FName UpperBodyRoot(TEXT("spine_01"));
	const FName ShoulderBone(TEXT("clavicle_r"));
	const FName HandBone(TEXT("hand_r"));
	constexpr int32 NumSpineBones = UE_ARRAY_COUNT(SpineBones);

	const TCHAR* const ClipNames[] = { TEXT("Idle"), TEXT("Walk"), TEXT("Run"), TEXT("StrafeLeft"), TEXT("StrafeRight"), TEXT("WalkBack"), TEXT("Attack"), TEXT("Hit"), TEXT("Death") };
	static_assert(UE_ARRAY_COUNT(ClipNames) == GBEnemyAnim::NumClips, "ClipNames must list every EGBEnemyClip");

	/** A rotation given in the actor's frame (pitch up, yaw, roll), turned into the mesh component's frame. */
	FQuat ActorToComponentSpace(const FRotator& InActorFrame, float MeshYawDegrees)
	{
		const FQuat ToActor = FRotator(0.f, MeshYawDegrees, 0.f).Quaternion();
		return ToActor.Inverse() * InActorFrame.Quaternion() * ToActor;
	}

	/**
	 * Rotates Bone about its own pivot by DeltaComponent (given in component space): the local rotation is corrected by the
	 * component-space rotation of its parent chain, so the children follow.
	 */
	void ApplyComponentDelta(FCompactPose& Pose, FCompactPoseBoneIndex Bone, const FQuat& DeltaComponent)
	{
		FQuat ParentCS = FQuat::Identity;
		for (FCompactPoseBoneIndex Parent = Pose.GetParentBoneIndex(Bone); Parent.GetInt() >= 0; Parent = Pose.GetParentBoneIndex(Parent))
		{
			ParentCS = Pose[Parent].GetRotation() * ParentCS;
		}
		const FQuat Local = Pose[Bone].GetRotation();
		Pose[Bone].SetRotation((ParentCS.Inverse() * DeltaComponent * ParentCS * Local).GetNormalized());
	}
}

const TCHAR* GBEnemyAnim::ClipName(EGBEnemyClip Clip)
{
	const int32 Index = static_cast<int32>(Clip);
	return Index >= 0 && Index < NumClips ? GBEnemyAnimLocal::ClipNames[Index] : TEXT("?");
}

// ---------------------------------------------------------------------------------------------------------------------
// The proxy: everything the pose evaluation needs, copied from the game thread in PreUpdate.
// ---------------------------------------------------------------------------------------------------------------------

struct FGBEnemyAnimProxy : public FAnimInstanceProxy
{
	FGBEnemyAnimProxy() = default;
	explicit FGBEnemyAnimProxy(UAnimInstance* InInstance) : FAnimInstanceProxy(InInstance) {}

	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override
	{
		FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);
		if (UGBEnemyAnimInstance* Instance = Cast<UGBEnemyAnimInstance>(InAnimInstance))
		{
			Owner = Instance;
			Frame = Instance->GetFrame();
			Tuning = Instance->GetTuning();
			for (int32 Index = 0; Index < GBEnemyAnim::NumClips; ++Index)
			{
				Clips[Index] = Instance->GetClip(static_cast<EGBEnemyClip>(Index));
			}
		}
	}

	virtual void Update(float DeltaSeconds) override
	{
		// Crossfade: every locomotion weight moves towards its target at the same rate.
		const float Step = DeltaSeconds / FMath::Max(Tuning.BlendTime, 0.01f);
		for (int32 Index = 0; Index < GBEnemyAnim::NumLocomotion; ++Index)
		{
			Weight[Index] = FMath::FInterpConstantTo(Weight[Index], Frame.Target[Index], 1.f, Step);
		}

		// Time-synced locomotion: one normalized phase for all moving clips, its rate the weighted mean of what each clip needs.
		float MovingWeight = 0.f;
		float CyclesPerSecond = 0.f;
		for (int32 Index = 1; Index < GBEnemyAnim::NumLocomotion; ++Index)
		{
			const UAnimSequence* Clip = Clips[Index];
			if (!Clip || Weight[Index] <= KINDA_SMALL_NUMBER)
			{
				continue;
			}
			const float RefSpeed = FMath::Max(Index == static_cast<int32>(EGBEnemyClip::Run) ? Tuning.RunAnimSpeed : Tuning.WalkAnimSpeed, 1.f);
			const float Length = FMath::Max(Clip->GetPlayLength(), 0.05f);
			CyclesPerSecond += Weight[Index] * (Frame.Speed / RefSpeed) / Length;
			MovingWeight += Weight[Index];
		}
		if (MovingWeight > KINDA_SMALL_NUMBER)
		{
			Phase = FMath::Fmod(Phase + DeltaSeconds * CyclesPerSecond / MovingWeight, 1.f);
		}
		IdleTime += DeltaSeconds;
	}

	virtual bool Evaluate(FPoseContext& Output) override
	{
		if (Owner.IsValid())
		{
			Owner->CountEvaluate();
		}
		Output.ResetToRefPose();
		FPoseContext Scratch(Output);

		// 1. Locomotion: the weighted mean of the clips, accumulated two at a time.
		float Accumulated = 0.f;
		for (int32 Index = 0; Index < GBEnemyAnim::NumLocomotion; ++Index)
		{
			const UAnimSequence* Clip = Clips[Index];
			if (!Clip || Weight[Index] <= 0.001f)
			{
				continue;
			}
			const float Length = FMath::Max(Clip->GetPlayLength(), 0.01f);
			const float Time = Index == 0 ? FMath::Fmod(IdleTime, Length) : Phase * Length;
			const FAnimExtractContext Context(static_cast<double>(Time), false, FDeltaTimeRecord(), true);
			if (Accumulated <= 0.f)
			{
				FAnimationPoseData Data(Output);
				Clip->GetAnimationPose(Data, Context);
			}
			else
			{
				Scratch.ResetToRefPose();
				FAnimationPoseData ScratchData(Scratch);
				Clip->GetAnimationPose(ScratchData, Context);
				FAnimationPoseData OutputData(Output);
				FAnimationRuntime::BlendTwoPosesTogetherInPlace(OutputData, ScratchData, Accumulated / (Accumulated + Weight[Index]));
			}
			Accumulated += Weight[Index];
		}

		RefreshBoneCache(Output.Pose.GetBoneContainer());

		// 2. One-shot overlay (Attack / Hit), full body or from spine_01 up.
		if (Frame.OverlayClip >= 0 && Frame.OverlayWeight > 0.001f && Clips[Frame.OverlayClip])
		{
			const UAnimSequence* Clip = Clips[Frame.OverlayClip];
			const FAnimExtractContext OverlayContext(static_cast<double>(FMath::Min(Frame.OverlayTime, Clip->GetPlayLength())), false, FDeltaTimeRecord(), false);
			if (Clip->IsValidAdditive())
			{
				// An additive clip (the rifle's fire kick) is added on top of the pose, not blended over it.
				Scratch.ResetToAdditiveIdentity();
				FAnimationPoseData AdditiveData(Scratch);
				Clip->GetAnimationPose(AdditiveData, OverlayContext);
				FAnimationPoseData BaseData(Output);
				FAnimationRuntime::AccumulateAdditivePose(BaseData, AdditiveData, Frame.OverlayWeight, Clip->GetAdditiveAnimType());
			}
			else
			{
				Scratch.ResetToRefPose();
				FAnimationPoseData ScratchData(Scratch);
				Clip->GetAnimationPose(ScratchData, OverlayContext);
				BlendOver(Output.Pose, Scratch.Pose, Frame.OverlayWeight, Frame.bOverlayUpperBody);
			}
		}

		// 3. Death: the clip, played once and held on its last frame, blended in over the pose.
		if (Frame.bDead && Clips[static_cast<int32>(EGBEnemyClip::Death)])
		{
			const UAnimSequence* Clip = Clips[static_cast<int32>(EGBEnemyClip::Death)];
			Scratch.ResetToRefPose();
			FAnimationPoseData ScratchData(Scratch);
			Clip->GetAnimationPose(ScratchData, FAnimExtractContext(static_cast<double>(FMath::Min(Frame.DeathTime, Clip->GetPlayLength())), false, FDeltaTimeRecord(), false));
			BlendOver(Output.Pose, Scratch.Pose, Frame.DeathWeight, false);
		}
		else if (!Frame.bDead)
		{
			ApplyUpperBodyOffsets(Output.Pose);
		}
		return true;
	}

	/** Pose = lerp(Pose, Over, Alpha) per bone, all bones or only spine_01 and its descendants. */
	void BlendOver(FCompactPose& Pose, const FCompactPose& Over, float Alpha, bool bUpperBodyOnly) const
	{
		for (const FCompactPoseBoneIndex Bone : Pose.ForEachBoneIndex())
		{
			if (bUpperBodyOnly && UpperBodyRootIndex.GetInt() >= 0 && !IsUpperBody[Bone.GetInt()])
			{
				continue;
			}
			Pose[Bone].BlendWith(Over[Bone], FMath::Clamp(Alpha, 0.f, 1.f));
		}
	}

	/** Aim pitch / yaw spread over the spine and neck, plus the recoil and the flinch. */
	void ApplyUpperBodyOffsets(FCompactPose& Pose) const
	{
		using namespace GBEnemyAnimLocal;
		const float Pitch = Frame.AimPitch + Frame.RecoilKick * Tuning.RecoilPitch + Frame.FlinchKick * Tuning.FlinchPitch;
		const float Yaw = Frame.AimYaw + Frame.FlinchKick * Tuning.FlinchYaw * Frame.FlinchSide;
		int32 Present = 0;
		for (int32 Index = 0; Index < NumSpineBones; ++Index)
		{
			Present += SpineIndex[Index].GetInt() >= 0 ? 1 : 0;
		}
		if (Present > 0 && (FMath::Abs(Pitch) > 0.01f || FMath::Abs(Yaw) > 0.01f))
		{
			const FQuat Share = ActorToComponentSpace(FRotator(Pitch / Present, Yaw / Present, 0.f), Tuning.MeshYaw);
			for (int32 Index = 0; Index < NumSpineBones; ++Index)
			{
				if (SpineIndex[Index].GetInt() >= 0)
				{
					ApplyComponentDelta(Pose, SpineIndex[Index], Share);
				}
			}
		}
		// The shooting arm kicks a little more than the body.
		if (Frame.RecoilKick > 0.01f)
		{
			const FQuat ArmKick = ActorToComponentSpace(FRotator(Frame.RecoilKick * Tuning.RecoilPitch * 1.5f, 0.f, 0.f), Tuning.MeshYaw);
			if (ShoulderIndex.GetInt() >= 0)
			{
				ApplyComponentDelta(Pose, ShoulderIndex, ArmKick);
			}
			if (HandIndex.GetInt() >= 0)
			{
				ApplyComponentDelta(Pose, HandIndex, ArmKick);
			}
		}
	}

	void RefreshBoneCache(const FBoneContainer& Bones)
	{
		using namespace GBEnemyAnimLocal;
		if (CachedSerial == Bones.GetSerialNumber() && CachedBoneCount == Bones.GetCompactPoseNumBones())
		{
			return;
		}
		CachedSerial = Bones.GetSerialNumber();
		CachedBoneCount = Bones.GetCompactPoseNumBones();
		auto Find = [&Bones](const FName& Name)
		{
			const int32 MeshIndex = Bones.GetPoseBoneIndexForBoneName(Name);
			return MeshIndex == INDEX_NONE ? FCompactPoseBoneIndex(INDEX_NONE) : Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(MeshIndex));
		};
		for (int32 Index = 0; Index < NumSpineBones; ++Index)
		{
			SpineIndex[Index] = Find(SpineBones[Index]);
		}
		ShoulderIndex = Find(ShoulderBone);
		HandIndex = Find(HandBone);
		UpperBodyRootIndex = Find(UpperBodyRoot);

		IsUpperBody.Init(false, CachedBoneCount);
		if (UpperBodyRootIndex.GetInt() >= 0)
		{
			for (int32 Bone = 0; Bone < CachedBoneCount; ++Bone)
			{
				for (FCompactPoseBoneIndex Walk(Bone); Walk.GetInt() >= 0; Walk = Bones.GetParentBoneIndex(Walk))
				{
					if (Walk == UpperBodyRootIndex)
					{
						IsUpperBody[Bone] = true;
						break;
					}
				}
			}
		}
	}

	TWeakObjectPtr<UGBEnemyAnimInstance> Owner;
	FGBEnemyAnimFrame Frame;
	FGBEnemyAnimTuning Tuning;
	const UAnimSequence* Clips[GBEnemyAnim::NumClips] = {};
	float Weight[GBEnemyAnim::NumLocomotion] = { 1.f, 0.f, 0.f, 0.f, 0.f, 0.f };
	float Phase = 0.f;
	float IdleTime = 0.f;

	uint16 CachedSerial = 0xFFFF;
	int32 CachedBoneCount = -1;
	FCompactPoseBoneIndex SpineIndex[GBEnemyAnimLocal::NumSpineBones] = { FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE),
		FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE) };
	FCompactPoseBoneIndex ShoulderIndex = FCompactPoseBoneIndex(INDEX_NONE);
	FCompactPoseBoneIndex HandIndex = FCompactPoseBoneIndex(INDEX_NONE);
	FCompactPoseBoneIndex UpperBodyRootIndex = FCompactPoseBoneIndex(INDEX_NONE);
	TArray<bool> IsUpperBody;
};

// ---------------------------------------------------------------------------------------------------------------------

FAnimInstanceProxy* UGBEnemyAnimInstance::CreateAnimInstanceProxy()
{
	return new FGBEnemyAnimProxy(this);
}

void UGBEnemyAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy)
{
	delete static_cast<FGBEnemyAnimProxy*>(InProxy);
}

void UGBEnemyAnimInstance::Configure(const TArray<UAnimSequence*>& InClips, const FGBEnemyAnimTuning& InTuning)
{
	Clips.Reset();
	for (int32 Index = 0; Index < GBEnemyAnim::NumClips; ++Index)
	{
		Clips.Add(InClips.IsValidIndex(Index) ? InClips[Index] : nullptr);
	}
	Tuning = InTuning;
}

UAnimSequence* UGBEnemyAnimInstance::GetClip(EGBEnemyClip Clip) const
{
	const int32 Index = static_cast<int32>(Clip);
	return Clips.IsValidIndex(Index) ? Clips[Index].Get() : nullptr;
}

bool UGBEnemyAnimInstance::HasClip(EGBEnemyClip Clip) const
{
	return GetClip(Clip) != nullptr;
}

int32 UGBEnemyAnimInstance::GetClipCount() const
{
	int32 Count = 0;
	for (const TObjectPtr<UAnimSequence>& Clip : Clips)
	{
		Count += Clip ? 1 : 0;
	}
	return Count;
}

bool UGBEnemyAnimInstance::PlayOneShot(EGBEnemyClip Clip, bool bUpperBodyOnly)
{
	if (!HasClip(Clip) || Frame.bDead)
	{
		return false;
	}
	Frame.OverlayClip = static_cast<int32>(Clip);
	Frame.OverlayTime = 0.f;
	Frame.OverlayWeight = 0.f;
	Frame.bOverlayUpperBody = bUpperBodyOnly;
	return true;
}

void UGBEnemyAnimInstance::NotifyShot()
{
	if (!PlayOneShot(EGBEnemyClip::Attack, true))
	{
		Frame.RecoilKick = 1.f;
	}
}

void UGBEnemyAnimInstance::NotifyHit(float Side)
{
	// A hit never cuts a running Attack / Hit overlay short and a flinch never stacks on one.
	if (Frame.OverlayClip == static_cast<int32>(EGBEnemyClip::Attack) || Frame.OverlayClip == static_cast<int32>(EGBEnemyClip::Hit))
	{
		return;
	}
	if (!PlayOneShot(EGBEnemyClip::Hit, true))
	{
		Frame.FlinchKick = 1.f;
		Frame.FlinchSide = Side >= 0.f ? 1.f : -1.f;
	}
}

bool UGBEnemyAnimInstance::PlayDeath()
{
	Frame.bDead = true;
	Frame.DeathTime = 0.f;
	Frame.OverlayClip = -1;
	return HasClip(EGBEnemyClip::Death);
}

FString UGBEnemyAnimInstance::DescribeClips() const
{
	const FString Overlay = Frame.OverlayClip >= 0 ? FString(GBEnemyAnim::ClipName(static_cast<EGBEnemyClip>(Frame.OverlayClip))) : FString(TEXT("-"));
	return FString::Printf(TEXT("locomotion=%s%s overlay=%s dead=%d clips=%d evals=%d"), GBEnemyAnim::ClipName(Dominant), HasClip(Dominant) ? TEXT("") : TEXT("(no clip)"),
		*Overlay, Frame.bDead ? 1 : 0, GetClipCount(), EvaluateCount.load());
}

void UGBEnemyAnimInstance::ComputeLocomotionTargets(float Speed, float LocalAngleDegrees)
{
	const float Walk = FMath::Max(Tuning.WalkAnimSpeed, 1.f);
	const float Run = FMath::Max(Tuning.RunAnimSpeed, Walk + 1.f);
	const float Move = FMath::Clamp(Speed / Walk, 0.f, 1.f);
	const float RunShare = FMath::Clamp((Speed - Walk) / (Run - Walk), 0.f, 1.f);

	const float Radians = FMath::DegreesToRadians(LocalAngleDegrees);
	float Forward = FMath::Max(0.f, FMath::Cos(Radians));
	float Back = FMath::Max(0.f, -FMath::Cos(Radians));
	float Right = FMath::Max(0.f, FMath::Sin(Radians));
	float Left = FMath::Max(0.f, -FMath::Sin(Radians));
	const float Sum = FMath::Max(Forward + Back + Right + Left, KINDA_SMALL_NUMBER);
	Forward /= Sum;
	Back /= Sum;
	Right /= Sum;
	Left /= Sum;

	const int32 IIdle = static_cast<int32>(EGBEnemyClip::Idle);
	const int32 IWalk = static_cast<int32>(EGBEnemyClip::Walk);
	const int32 IRun = static_cast<int32>(EGBEnemyClip::Run);
	float* T = Frame.Target;
	T[IIdle] = 1.f - Move;
	T[IWalk] = Move * Forward * (1.f - RunShare);
	T[IRun] = Move * Forward * RunShare;
	T[static_cast<int32>(EGBEnemyClip::StrafeLeft)] = Move * Left;
	T[static_cast<int32>(EGBEnemyClip::StrafeRight)] = Move * Right;
	T[static_cast<int32>(EGBEnemyClip::WalkBack)] = Move * Back;

	// Missing clips hand their share over: Walk <-> Run, the sideways and backwards ones to the forward clip, all of it to Idle.
	if (!HasClip(EGBEnemyClip::Walk) && HasClip(EGBEnemyClip::Run))
	{
		T[IRun] += T[IWalk];
		T[IWalk] = 0.f;
	}
	if (!HasClip(EGBEnemyClip::Run) && HasClip(EGBEnemyClip::Walk))
	{
		T[IWalk] += T[IRun];
		T[IRun] = 0.f;
	}
	const int32 Fallback = HasClip(EGBEnemyClip::Walk) ? IWalk : (HasClip(EGBEnemyClip::Run) ? IRun : IIdle);
	for (int32 Index = IRun + 1; Index < GBEnemyAnim::NumLocomotion; ++Index)
	{
		if (!HasClip(static_cast<EGBEnemyClip>(Index)))
		{
			T[Fallback] += T[Index];
			T[Index] = 0.f;
		}
	}
	if (Fallback == IIdle)
	{
		T[IIdle] += T[IWalk] + T[IRun];
		T[IWalk] = 0.f;
		T[IRun] = 0.f;
	}

	int32 Best = IIdle;
	for (int32 Index = 0; Index < GBEnemyAnim::NumLocomotion; ++Index)
	{
		if (T[Index] > T[Best])
		{
			Best = Index;
		}
	}
	Dominant = static_cast<EGBEnemyClip>(Best);
}

void UGBEnemyAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (const APawn* Pawn = TryGetPawnOwner())
	{
		const FVector Velocity = Pawn->GetVelocity();
		const FVector Local = Pawn->GetActorRotation().UnrotateVector(Velocity);
		Frame.Speed = Velocity.Size2D();
		ComputeLocomotionTargets(Frame.Speed, FMath::RadiansToDegrees(FMath::Atan2(Local.Y, Local.X)));
	}

	// Aim follows its target with a smoothing; the kicks decay.
	Frame.AimPitch = FMath::FInterpTo(Frame.AimPitch, AimPitchTarget, DeltaSeconds, Tuning.AimSmooth);
	Frame.AimYaw = FMath::FInterpTo(Frame.AimYaw, AimYawTarget, DeltaSeconds, Tuning.AimSmooth);
	Frame.RecoilKick = FMath::Max(0.f, Frame.RecoilKick - Frame.RecoilKick * FMath::Min(1.f, Tuning.RecoilDecay * DeltaSeconds) - DeltaSeconds * 0.05f);
	Frame.FlinchKick = FMath::Max(0.f, Frame.FlinchKick - Frame.FlinchKick * FMath::Min(1.f, Tuning.FlinchDecay * DeltaSeconds) - DeltaSeconds * 0.05f);

	// The one-shot overlay: in over OverlayBlend, out over the same before its end.
	if (Frame.OverlayClip >= 0)
	{
		const UAnimSequence* Clip = GetClip(static_cast<EGBEnemyClip>(Frame.OverlayClip));
		const float Length = Clip ? Clip->GetPlayLength() : 0.f;
		Frame.OverlayTime += DeltaSeconds;
		if (!Clip || Frame.OverlayTime >= Length)
		{
			Frame.OverlayClip = -1;
			Frame.OverlayWeight = 0.f;
		}
		else
		{
			const float Blend = FMath::Max(Tuning.OverlayBlend, 0.01f);
			Frame.OverlayWeight = FMath::Clamp(FMath::Min(Frame.OverlayTime / Blend, (Length - Frame.OverlayTime) / Blend), 0.f, 1.f);
		}
	}

	if (Frame.bDead)
	{
		Frame.DeathTime += DeltaSeconds;
		Frame.DeathWeight = FMath::Clamp(Frame.DeathTime / FMath::Max(Tuning.BlendTime, 0.01f), 0.f, 1.f);
	}
}
