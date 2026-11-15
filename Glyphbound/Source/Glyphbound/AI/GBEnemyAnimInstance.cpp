#include "AI/GBEnemyAnimInstance.h"

#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/AnimationPoseData.h"
#include "AnimationRuntime.h"
#include "BoneContainer.h"
#include "BonePose.h"
#include "Animation/Skeleton.h"
#include "GameFramework/Pawn.h"

namespace GBEnemyAnimLocal
{
	// The bones of the UE5 mannequin skeleton that the retargeted humanoids use. Bones that do not exist are skipped
	// (the spider has its own skeleton: it gets no aim bend).
	const FName SpineBones[] = { FName(TEXT("spine_01")), FName(TEXT("spine_02")), FName(TEXT("spine_03")), FName(TEXT("spine_04")), FName(TEXT("spine_05")), FName(TEXT("neck_01")) };
	const FName UpperBodyRoot(TEXT("spine_01"));
	const FName ShoulderBone(TEXT("clavicle_r"));
	const FName HandBone(TEXT("hand_r"));
	// The arms driven by FGBArmIK: left (0) and right (1), upper arm, forearm, hand.
	const FName ArmBones[2][3] = { { FName(TEXT("upperarm_l")), FName(TEXT("lowerarm_l")), FName(TEXT("hand_l")) }, { FName(TEXT("upperarm_r")), FName(TEXT("lowerarm_r")), FName(TEXT("hand_r")) } };
	constexpr int32 NumSpineBones = UE_ARRAY_COUNT(SpineBones);

	const TCHAR* const ClipNames[] = { TEXT("Idle"), TEXT("Walk"), TEXT("Run"), TEXT("StrafeLeft"), TEXT("StrafeRight"), TEXT("WalkBack"), TEXT("Attack"), TEXT("Hit"), TEXT("Death"),
		TEXT("RelaxedIdle"), TEXT("PatrolWalk"), TEXT("Sprint"), TEXT("CrouchIdle"), TEXT("CrouchWalk"),
		TEXT("Action0"), TEXT("Action1"), TEXT("Action2"), TEXT("Action3"), TEXT("Action4"), TEXT("Action5"), TEXT("Action6") };
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
				const int32 Resolved = Index < GBEnemyAnim::NumLocomotion ? Instance->ResolveSlot(Index) : Index;
				Clips[Index] = Instance->GetClip(static_cast<EGBEnemyClip>(Resolved));
				Info[Index] = Instance->GetClipInfo(Resolved);
			}
			PoseOffsets = Instance->GetPoseOffsets();
			GripOffsets = Instance->GetGripOffsets();
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
			// Each clip plays at the rate its own authored ground speed asks for, kept within the limits (foot slide beats slow motion).
			const float RefSpeed = FMath::Max(Info[Index].RefSpeed, 1.f);
			const float Rate = FMath::Clamp(Frame.Speed / RefSpeed, Tuning.AnimRateMin, FMath::Max(Tuning.AnimRateMax, Tuning.AnimRateMin));
			const float Length = FMath::Max(Clip->GetPlayLength(), 0.05f);
			CyclesPerSecond += Weight[Index] * Rate / Length;
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
		RefreshBoneCache(Output.Pose.GetBoneContainer());

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
				RemoveDrift(Output.Pose, Index, Time);
			}
			else
			{
				Scratch.ResetToRefPose();
				FAnimationPoseData ScratchData(Scratch);
				Clip->GetAnimationPose(ScratchData, Context);
				RemoveDrift(Scratch.Pose, Index, Time);
				FAnimationPoseData OutputData(Output);
				FAnimationRuntime::BlendTwoPosesTogetherInPlace(OutputData, ScratchData, Accumulated / (Accumulated + Weight[Index]));
			}
			Accumulated += Weight[Index];
		}

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
				if (Frame.OverlayClip == static_cast<int32>(EGBEnemyClip::Attack) || Frame.OverlayClip >= static_cast<int32>(EGBEnemyClip::Action0))
				{
					// The swing's travel stays in the clip's own space: the body is pinned over the capsule (horizontal axes of the travel bone).
					PinTravelBone(Scratch.Pose, Frame.OverlayClip);
				}
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
			// The fall plays in place: the pelvis's horizontal travel stays at the clip's first frame (the height still drops), like the swing.
			PinTravelBone(Scratch.Pose, static_cast<int32>(EGBEnemyClip::Death));
			BlendOver(Output.Pose, Scratch.Pose, Frame.DeathWeight, false);
		}
		else if (!Frame.bDead)
		{
			ApplyUpperBodyOffsets(Output.Pose);
			ApplyPoseLayer(Output.Pose);
			ApplyArmIK(Output.Pose, 0);
			ApplyArmIK(Output.Pose, 1);
			ApplyGrip(Output.Pose);
		}
		return true;
	}

	/** The travel bone's local translation minus the clip's drift scaled by how far into the clip we are. */
	void RemoveDrift(FCompactPose& Pose, int32 ClipIndex, float Time) const
	{
		const FGBClipInfo& Clip = Info[ClipIndex];
		if (!Clip.bHasDrift || PelvisIndex.GetInt() < 0 || Clip.Length <= KINDA_SMALL_NUMBER)
		{
			return;
		}
		const float Alpha = FMath::Clamp(Time / Clip.Length, 0.f, 1.f);
		FTransform& Bone = Pose[PelvisIndex];
		Bone.SetTranslation(Bone.GetTranslation() - Clip.Drift * Alpha);
	}

	/** Keeps the travel bone's first two local axes at their frame-0 values (the travel of the swing), the third untouched. */
	void PinTravelBone(FCompactPose& Pose, int32 ClipIndex) const
	{
		const FGBClipInfo& Clip = Info[ClipIndex];
		if (!Clip.bHasBone || PelvisIndex.GetInt() < 0)
		{
			return;
		}
		FTransform& Bone = Pose[PelvisIndex];
		const FVector Current = Bone.GetTranslation();
		Bone.SetTranslation(FVector(Clip.BoneStart.X, Clip.BoneStart.Y, Current.Z));
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

	/** The data-driven pose (a raised shield): local rotation offsets of named bones, applied last, scaled by PoseAlpha. */
	void ApplyPoseLayer(FCompactPose& Pose) const
	{
		if (Frame.PoseAlpha <= 0.001f || PoseOffsets.Num() == 0)
		{
			return;
		}
		const FBoneContainer& Bones = Pose.GetBoneContainer();
		for (const FGBPoseOffset& Offset : PoseOffsets)
		{
			const int32 MeshIndex = Bones.GetPoseBoneIndexForBoneName(Offset.Bone);
			if (MeshIndex == INDEX_NONE)
			{
				continue;
			}
			const FCompactPoseBoneIndex Index = Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(MeshIndex));
			if (Index.GetInt() < 0)
			{
				continue;
			}
			const FQuat Delta = FQuat::Slerp(FQuat::Identity, Offset.Delta.Quaternion(), Frame.PoseAlpha);
			Pose[Index].SetRotation((Pose[Index].GetRotation() * Delta).GetNormalized());
		}
	}

	/** The closed right hand: a fixed local rotation on the finger bones, always on (the clips are unarmed and leave the fingers open). */
	void ApplyGrip(FCompactPose& Pose) const
	{
		const FBoneContainer& Bones = Pose.GetBoneContainer();
		for (const FGBPoseOffset& Offset : GripOffsets)
		{
			const int32 MeshIndex = Bones.GetPoseBoneIndexForBoneName(Offset.Bone);
			if (MeshIndex == INDEX_NONE)
			{
				continue;
			}
			const FCompactPoseBoneIndex Index = Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(MeshIndex));
			if (Index.GetInt() >= 0)
			{
				Pose[Index].SetRotation((Pose[Index].GetRotation() * Offset.Delta.Quaternion()).GetNormalized());
			}
		}
	}

	/**
	 * Two-bone IK of one arm (upperarm, lowerarm, hand): the hand goes to the goal, the elbow bends towards the pole, the hand
	 * keeps its rotation relative to the forearm unless the goal gives one. Lengths never stretch: a goal out of reach gets
	 * the straight arm pointing at it.
	 */
	void ApplyArmIK(FCompactPose& Pose, int32 Side) const
	{
		const FGBArmIK& Goal = Frame.Arm[Side];
		const FCompactPoseBoneIndex U = ArmIndex[Side][0];
		const FCompactPoseBoneIndex L = ArmIndex[Side][1];
		const FCompactPoseBoneIndex H = ArmIndex[Side][2];
		if (Goal.Alpha <= 0.001f || U.GetInt() < 0 || L.GetInt() < 0 || H.GetInt() < 0 || Pose.GetParentBoneIndex(L) != U || Pose.GetParentBoneIndex(H) != L)
		{
			return;
		}
		FTransform ParentCS = FTransform::Identity;
		for (FCompactPoseBoneIndex Parent = Pose.GetParentBoneIndex(U); Parent.GetInt() >= 0; Parent = Pose.GetParentBoneIndex(Parent))
		{
			ParentCS = ParentCS * Pose[Parent];
		}
		const FTransform UCS = Pose[U] * ParentCS;
		const FTransform LCS = Pose[L] * UCS;
		const FTransform HCS = Pose[H] * LCS;
		const FVector A = UCS.GetLocation();
		const FVector J0 = LCS.GetLocation();
		const FVector E0 = HCS.GetLocation();
		const double UpperLen = FVector::Dist(A, J0);
		const double LowerLen = FVector::Dist(J0, E0);
		if (UpperLen < 1.0 || LowerLen < 1.0)
		{
			return;
		}

		FVector Target = Goal.bFromShoulder ? A + Goal.Location * (UpperLen + LowerLen) : Goal.Location;
		FQuat Recoil = FQuat::Identity;
		if (Goal.RecoilDegrees > 0.f && Frame.RecoilKick > 0.01f)
		{
			// The kick turns the aim up about the arm's horizontal axis.
			const FVector Axis = FVector::CrossProduct((Target - A).GetSafeNormal(), FVector::UpVector).GetSafeNormal();
			if (!Axis.IsNearlyZero())
			{
				Recoil = FQuat(Axis, FMath::DegreesToRadians(Goal.RecoilDegrees * Frame.RecoilKick));
				Target = A + Recoil.RotateVector(Target - A);
			}
		}
		Target = FMath::Lerp(E0, Target, FMath::Clamp(Goal.Alpha, 0.f, 1.f));

		const FVector ToTarget = Target - A;
		const double Reach = ToTarget.Size();
		if (Reach < 1.0)
		{
			return;
		}
		const FVector Dir = ToTarget / Reach;
		const double D = FMath::Clamp(Reach, FMath::Abs(UpperLen - LowerLen) + 0.1, (UpperLen + LowerLen) * 0.999);
		const double CosA = FMath::Clamp((UpperLen * UpperLen + D * D - LowerLen * LowerLen) / (2.0 * UpperLen * D), -1.0, 1.0);
		const double SinA = FMath::Sqrt(FMath::Max(0.0, 1.0 - CosA * CosA));
		FVector Bend = Goal.Pole - Dir * FVector::DotProduct(Goal.Pole, Dir);
		if (Bend.SizeSquared() < 1e-4)
		{
			Bend = (J0 - A) - Dir * FVector::DotProduct(J0 - A, Dir);
		}
		if (!Bend.Normalize())
		{
			return;
		}
		const FVector J = A + Dir * (UpperLen * CosA) + Bend * (UpperLen * SinA);
		const FVector E = A + Dir * D;

		const FQuat DeltaUpper = FQuat::FindBetweenNormals((J0 - A).GetSafeNormal(), (J - A).GetSafeNormal());
		const FQuat DeltaLower = FQuat::FindBetweenNormals(DeltaUpper.RotateVector(E0 - J0).GetSafeNormal(), (E - J).GetSafeNormal());
		const FQuat NewU = DeltaUpper * UCS.GetRotation();
		const FQuat NewL = DeltaLower * DeltaUpper * LCS.GetRotation();
		FQuat NewH = DeltaLower * DeltaUpper * HCS.GetRotation();
		if (Goal.bRotation)
		{
			NewH = FQuat::Slerp(NewH, Recoil * Goal.Rotation, FMath::Clamp(Goal.Alpha, 0.f, 1.f));
		}
		// Component rotation = parent's component rotation * local rotation (unit scales along the arm).
		Pose[U].SetRotation((ParentCS.GetRotation().Inverse() * NewU).GetNormalized());
		Pose[L].SetRotation((NewU.Inverse() * NewL).GetNormalized());
		Pose[H].SetRotation((NewL.Inverse() * NewH).GetNormalized());
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
		PelvisIndex = Find(Tuning.TravelBone);
		ShoulderIndex = Find(ShoulderBone);
		HandIndex = Find(HandBone);
		UpperBodyRootIndex = Find(UpperBodyRoot);
		for (int32 Side = 0; Side < 2; ++Side)
		{
			for (int32 Link = 0; Link < 3; ++Link)
			{
				ArmIndex[Side][Link] = Find(ArmBones[Side][Link]);
			}
		}

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
	TArray<FGBPoseOffset> PoseOffsets;
	TArray<FGBPoseOffset> GripOffsets;
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
	FCompactPoseBoneIndex PelvisIndex = FCompactPoseBoneIndex(INDEX_NONE);
	FGBClipInfo Info[GBEnemyAnim::NumClips];
	FCompactPoseBoneIndex ShoulderIndex = FCompactPoseBoneIndex(INDEX_NONE);
	FCompactPoseBoneIndex HandIndex = FCompactPoseBoneIndex(INDEX_NONE);
	FCompactPoseBoneIndex UpperBodyRootIndex = FCompactPoseBoneIndex(INDEX_NONE);
	FCompactPoseBoneIndex ArmIndex[2][3] = { { FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE) },
		{ FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE) } };
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

	// Every clip of the retargeted humanoids carries its travel in the pelvis (the root never moves): measure it once, here.
	for (int32 Index = 0; Index < GBEnemyAnim::NumClips; ++Index)
	{
		FGBClipInfo& Info = ClipInfo[Index];
		Info = FGBClipInfo();
		Info.RefSpeed = FMath::Max(Index == static_cast<int32>(EGBEnemyClip::Run) ? Tuning.RunAnimSpeed : Tuning.WalkAnimSpeed, 1.f);
		const UAnimSequence* Clip = Clips[Index];
		if (!Clip)
		{
			continue;
		}
		Info.Length = Clip->GetPlayLength();
		const USkeleton* Skeleton = Clip->GetSkeleton();
		const int32 Bone = Skeleton ? Skeleton->GetReferenceSkeleton().FindBoneIndex(Tuning.TravelBone) : INDEX_NONE;
		if (Bone == INDEX_NONE || Info.Length <= KINDA_SMALL_NUMBER)
		{
			continue;
		}
		FTransform First;
		FTransform Last;
		Clip->GetBoneTransform(First, FSkeletonPoseBoneIndex(Bone), FAnimExtractContext(0.0, false, FDeltaTimeRecord(), false), false);
		Clip->GetBoneTransform(Last, FSkeletonPoseBoneIndex(Bone), FAnimExtractContext(static_cast<double>(Info.Length), false, FDeltaTimeRecord(), false), false);
		Info.bHasBone = true;
		Info.BoneStart = First.GetTranslation();
		Info.Drift = Last.GetTranslation() - First.GetTranslation();
		// Only the loops that carry travel (locomotion) are corrected by their drift; the Attack clip is pinned instead.
		const bool bLoop = Index < GBEnemyAnim::NumLocomotion || (Index >= static_cast<int32>(EGBEnemyClip::RelaxedIdle) && Index < static_cast<int32>(EGBEnemyClip::Action0));   // the extra posture clips are loops too
		if (bLoop && Info.Drift.Size() >= Tuning.DriftMin)
		{
			Info.bHasDrift = true;
			Info.RefSpeed = FMath::Max(Info.Drift.Size() * Tuning.MeshScale / Info.Length, 1.f);
		}
	}
}

void UGBEnemyAnimInstance::SetPoseLayer(const FString& Spec)
{
	PoseOffsets.Reset();
	TArray<FString> Parts;
	Spec.ParseIntoArray(Parts, TEXT(";"), true);
	for (FString& Part : Parts)
	{
		FString BoneName;
		FString Angles;
		if (!Part.TrimStartAndEnd().Split(TEXT(":"), &BoneName, &Angles))
		{
			continue;
		}
		TArray<FString> Numbers;
		Angles.ParseIntoArray(Numbers, TEXT(","), true);
		if (Numbers.Num() < 3)
		{
			continue;
		}
		FGBPoseOffset Offset;
		Offset.Bone = FName(*BoneName.TrimStartAndEnd());
		Offset.Delta = FRotator(FCString::Atof(*Numbers[0]), FCString::Atof(*Numbers[1]), FCString::Atof(*Numbers[2]));
		PoseOffsets.Add(Offset);
	}
}

void UGBEnemyAnimInstance::SetGripCurl(float FingerDegrees, const FVector& FingerAxis, float ThumbDegrees, const FVector& ThumbAxis)
{
	GripOffsets.Reset();
	// Each finger has three joints that curl a little differently; the axis is (pitch, yaw, roll) of the bone's local frame.
	static const TCHAR* const Fingers[] = { TEXT("index"), TEXT("middle"), TEXT("ring"), TEXT("pinky") };
	static const float FingerShare[] = { 0.9f, 1.f, 1.05f, 1.1f };
	static const float JointShare[] = { 1.f, 1.15f, 0.8f };
	for (int32 Finger = 0; Finger < UE_ARRAY_COUNT(Fingers); ++Finger)
	{
		for (int32 Joint = 0; Joint < 3; ++Joint)
		{
			FGBPoseOffset Offset;
			Offset.Bone = FName(*FString::Printf(TEXT("%s_%02d_r"), Fingers[Finger], Joint + 1));
			const FVector Angles = FingerAxis * FingerDegrees * FingerShare[Finger] * JointShare[Joint];
			Offset.Delta = FRotator(Angles.X, Angles.Y, Angles.Z);
			GripOffsets.Add(Offset);
		}
	}
	for (int32 Joint = 0; Joint < 3; ++Joint)
	{
		FGBPoseOffset Offset;
		Offset.Bone = FName(*FString::Printf(TEXT("thumb_%02d_r"), Joint + 1));
		const FVector Angles = ThumbAxis * ThumbDegrees * JointShare[Joint];
		Offset.Delta = FRotator(Angles.X, Angles.Y, Angles.Z);
		GripOffsets.Add(Offset);
	}
}

int32 UGBEnemyAnimInstance::ResolveSlot(int32 Slot) const
{
	auto Has = [this](EGBEnemyClip Clip) { return HasClip(Clip); };
	if (Slot == static_cast<int32>(EGBEnemyClip::Idle))
	{
		if (bCrouched && Has(EGBEnemyClip::CrouchIdle)) { return static_cast<int32>(EGBEnemyClip::CrouchIdle); }
		if (bRelaxed && Has(EGBEnemyClip::RelaxedIdle)) { return static_cast<int32>(EGBEnemyClip::RelaxedIdle); }
	}
	else if (Slot == static_cast<int32>(EGBEnemyClip::Walk))
	{
		if (bCrouched && Has(EGBEnemyClip::CrouchWalk)) { return static_cast<int32>(EGBEnemyClip::CrouchWalk); }
		if (bRelaxed && Has(EGBEnemyClip::PatrolWalk)) { return static_cast<int32>(EGBEnemyClip::PatrolWalk); }
	}
	else if (Slot == static_cast<int32>(EGBEnemyClip::Run))
	{
		if (bUseSprint && Has(EGBEnemyClip::Sprint)) { return static_cast<int32>(EGBEnemyClip::Sprint); }
	}
	return Slot;
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
	// An arm driven by IK fires with the procedural kick: a clip would fight the IK.
	if (Frame.Arm[1].Alpha > 0.f || !PlayOneShot(EGBEnemyClip::Attack, true))
	{
		Frame.RecoilKick = 1.f;
	}
}

void UGBEnemyAnimInstance::NotifyHit(float Side)
{
	// A hit never cuts a running Attack / Hit overlay short and a flinch never stacks on one; nothing plays over the death pose.
	if (Frame.bDead)
	{
		return;
	}
	if (Frame.OverlayClip == static_cast<int32>(EGBEnemyClip::Attack) || Frame.OverlayClip == static_cast<int32>(EGBEnemyClip::Hit) || Frame.OverlayClip >= static_cast<int32>(EGBEnemyClip::Action0))
	{
		return;
	}
	if (HasArmIK() || !PlayOneShot(EGBEnemyClip::Hit, true))
	{
		Frame.FlinchKick = 1.f;
		Frame.FlinchSide = Side >= 0.f ? 1.f : -1.f;
	}
}

bool UGBEnemyAnimInstance::PlayDeath()
{
	Frame.bDead = true;
	Frame.DeathTime = 0.f;
	Frame.DeathWeight = 0.f;
	Frame.OverlayClip = -1;
	Frame.OverlayWeight = 0.f;
	// Nothing of the live pose may stay on top of the death clip: no aim, no flinch, no recoil, no raised shield.
	AimPitchTarget = 0.f;
	AimYawTarget = 0.f;
	Frame.AimPitch = 0.f;
	Frame.AimYaw = 0.f;
	Frame.FlinchKick = 0.f;
	Frame.RecoilKick = 0.f;
	PoseTarget = 0.f;
	Frame.PoseAlpha = 0.f;
	return HasClip(EGBEnemyClip::Death);
}

float UGBEnemyAnimInstance::GetDeathClipLength() const
{
	const UAnimSequence* Clip = GetClip(EGBEnemyClip::Death);
	return Clip ? Clip->GetPlayLength() : 0.f;
}

FString UGBEnemyAnimInstance::DescribeClips() const
{
	const FString Overlay = Frame.OverlayClip >= 0 ? FString(GBEnemyAnim::ClipName(static_cast<EGBEnemyClip>(Frame.OverlayClip))) : FString(TEXT("-"));
	return FString::Printf(TEXT("locomotion=%s%s overlay=%s dead=%d clips=%d evals=%d"), GBEnemyAnim::ClipName(Dominant), HasClip(Dominant) ? TEXT("") : TEXT("(no clip)"),
		*Overlay, Frame.bDead ? 1 : 0, GetClipCount(), EvaluateCount.load());
}

void UGBEnemyAnimInstance::ComputeLocomotionTargets(float Speed, float LocalAngleDegrees)
{
	// The authored ground speeds decide: the walk clip is good up to its speed times AnimRateMax, the run clip from its speed times AnimRateMin.
	const float WalkRef = ClipInfo[static_cast<int32>(EGBEnemyClip::Walk)].RefSpeed;
	const float RunRef = ClipInfo[static_cast<int32>(EGBEnemyClip::Run)].RefSpeed;
	const float WalkEnd = WalkRef * Tuning.AnimRateMax;
	const float RunStart = RunRef * Tuning.AnimRateMin;
	const float Middle = 0.5f * (WalkEnd + RunStart);
	const float HalfWidth = FMath::Max(0.5f * FMath::Abs(RunStart - WalkEnd), 25.f);
	const float Move = FMath::Clamp(Speed / FMath::Max(Tuning.MoveBlendSpeed, 1.f), 0.f, 1.f);
	const float RunShare = FMath::Clamp((Speed - (Middle - HalfWidth)) / (2.f * HalfWidth), 0.f, 1.f);

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

	// Sprint replaces Run above the run clip's authored speed; the pose layer follows its target.
	const float RunRef = ClipInfo[static_cast<int32>(EGBEnemyClip::Run)].RefSpeed;
	bUseSprint = Frame.Speed > RunRef * Tuning.AnimRateMax * (bUseSprint ? 0.95f : 1.05f);
	Frame.PoseAlpha = FMath::FInterpConstantTo(Frame.PoseAlpha, PoseTarget, DeltaSeconds, 1.f / PoseBlend);

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
