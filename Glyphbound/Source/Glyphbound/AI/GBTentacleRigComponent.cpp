#include "AI/GBTentacleRigComponent.h"

#include "Components/PoseableMeshComponent.h"
#include "BoneContainer.h"
#include "Data/GBDataSubsystem.h"
#include "AI/GBTentacleMath.h"

namespace GBTentacleRigLocal
{
	// Technical constants, not tuning: bone name shape, FABRIK budget, lag clamp, ripple shape.
	constexpr int32 MaxNameLength = 6;          // "T<cc>_<bb>"
	constexpr int32 ReachIterations = 10;
	constexpr float ReachToleranceCm = 0.5f;    // well under the 15 cm the smoke run allows
	constexpr float MaxLagDegrees = 30.f;       // clamp on a lag spring target
	constexpr float YawLeverCm = 100.f;         // yaw rate acts like sideways speed at this lever
	constexpr float MaxDeltaTime = 0.1f;        // a hitch never jumps the pose
	constexpr float RippleWidth = 4.f;          // sharpness of the travelling pulse
	constexpr float RippleWaves = 2.f;          // oscillations inside the pulse
	constexpr int32 RootLimitedBones = 2;       // bones _00/_01 keep the root bend limit

	FQuat AxisAngleQuat(const FVector& Axis, float AngleDeg)
	{
		FVector SafeAxis = Axis.GetSafeNormal();
		if (SafeAxis.IsNearlyZero())
		{
			return FQuat::Identity;
		}
		return FQuat(SafeAxis, FMath::DegreesToRadians(AngleDeg));
	}

	FVector AxisVector(int32 Axis)
	{
		switch (Axis)
		{
		case 0: return FVector::XAxisVector;
		case 1: return FVector::YAxisVector;
		default: return FVector::ZAxisVector;
		}
	}
}

UGBTentacleRigComponent::UGBTentacleRigComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UGBTentacleRigComponent::BeginPlay()
{
	Super::BeginPlay();
	if (AActor* Owner = GetOwner())
	{
		PrevYaw = Owner->GetActorRotation().Yaw;
		bHasPrevYaw = true;
	}
}

float UGBTentacleRigComponent::GetGameValue(FName Key, float DefaultValue) const
{
	const UGBDataSubsystem* Data = UGBDataSubsystem::Get(this);
	return Data ? Data->GetGameValue(Key, DefaultValue) : DefaultValue;
}

void UGBTentacleRigComponent::ReadTuning(FGBTentacleTuning& OutTuning) const
{
	using namespace GBEntityTentacle;
	OutTuning.WaveFreq = GetGameValue(WaveFreq, WaveFreqDefault);
	OutTuning.WaveAmp = GetGameValue(WaveAmp, WaveAmpDefault);
	OutTuning.WaveLength = GetGameValue(WaveLength, WaveLengthDefault);
	OutTuning.LagStrength = GetGameValue(LagStrength, LagStrengthDefault);
	OutTuning.LagStiffness = GetGameValue(LagStiffness, LagStiffnessDefault);
	OutTuning.LagDamping = GetGameValue(LagDamping, LagDampingDefault);
	OutTuning.RootLimit = GetGameValue(RootLimit, RootLimitDefault);
	OutTuning.ReachBlendIn = GetGameValue(ReachBlendIn, ReachBlendInDefault);
	OutTuning.ReachBlendOut = GetGameValue(ReachBlendOut, ReachBlendOutDefault);
	OutTuning.RippleStrength = GetGameValue(RippleStrength, RippleStrengthDefault);
	OutTuning.RippleDuration = GetGameValue(RippleDuration, RippleDurationDefault);
	OutTuning.LimpDroop = GetGameValue(LimpDroop, LimpDroopDefault);
}

void UGBTentacleRigComponent::Setup(UPoseableMeshComponent* InMesh)
{
	Chains.Reset();
	TentacleMesh = InMesh;
	ReachWeight = 0.f;
	ReachDesired = 0.f;

	if (!IsValid(InMesh) || !InMesh->GetSkinnedAsset())
	{
		UE_LOG(LogGB, Warning, TEXT("TentacleRig: no poseable mesh with a skeletal mesh, tentacles stay still"));
		return;
	}

	TArray<FName> BoneNames;
	InMesh->GetBoneNames(BoneNames);

	// Group T<cc>_<bb> bones into chains sorted by bone index.
	TMap<int32, TArray<TPair<int32, FName>>> Groups;
	for (const FName& BoneName : BoneNames)
	{
		const FString Name = BoneName.ToString();
		if (Name.Len() != GBTentacleRigLocal::MaxNameLength || !Name.StartsWith(TEXT("T")) || Name[3] != TCHAR('_'))
		{
			continue;
		}
		const int32 ChainIndex = FCString::Atoi(*Name.Mid(1, 2));
		const int32 BoneIndex = FCString::Atoi(*Name.Mid(4, 2));
		// Guard the shape: only T<digits>_<digits>.
		if (!FChar::IsDigit(Name[1]) || !FChar::IsDigit(Name[2]) || !FChar::IsDigit(Name[4]) || !FChar::IsDigit(Name[5]))
		{
			continue;
		}
		Groups.FindOrAdd(ChainIndex).Add(TPair<int32, FName>(BoneIndex, BoneName));
	}

	TArray<int32> ChainKeys;
	Groups.GetKeys(ChainKeys);
	ChainKeys.Sort();

	RootCompQuat = FQuat::Identity;
	if (BoneNames.Contains(TEXT("root")))
	{
		RootCompQuat = InMesh->GetBoneTransformByName(TEXT("root"), EBoneSpaces::ComponentSpace).GetRotation();
	}

	for (int32 ChainKey : ChainKeys)
	{
		TArray<TPair<int32, FName>>& Pairs = Groups.FindChecked(ChainKey);
		Pairs.Sort([](const TPair<int32, FName>& A, const TPair<int32, FName>& B) { return A.Key < B.Key; });

		FGBTentacleChain Chain;
		Chain.Phase = FMath::FRand();
		FQuat ParentComp = RootCompQuat;
		for (const TPair<int32, FName>& Pair : Pairs)
		{
			const FTransform CompT = InMesh->GetBoneTransformByName(Pair.Value, EBoneSpaces::ComponentSpace);
			Chain.Bones.Add(Pair.Value);
			Chain.RefComp.Add(CompT.GetRotation());
			Chain.RefJoints.Add(CompT.GetLocation());
			Chain.RefLocal.Add(ParentComp.Inverse() * CompT.GetRotation());
			ParentComp = CompT.GetRotation();
		}
		const int32 BoneCount = Chain.Bones.Num();
		if (BoneCount == 0)
		{
			continue;
		}
		// The tip joint continues past the last bone along the last segment.
		const FVector LastDir = (Chain.RefJoints[BoneCount - 1] - Chain.RefJoints[BoneCount - 2 >= 0 ? BoneCount - 2 : 0]).GetSafeNormal();
		const float LastLen = BoneCount >= 2 ? (Chain.RefJoints[BoneCount - 1] - Chain.RefJoints[BoneCount - 2]).Size() : 10.f;
		Chain.RefJoints.Add(Chain.RefJoints[BoneCount - 1] + LastDir * LastLen);

		for (int32 Bone = 0; Bone < BoneCount; ++Bone)
		{
			const float Segment = (Chain.RefJoints[Bone + 1] - Chain.RefJoints[Bone]).Size();
			Chain.SegLengths.Add(FMath::Max(Segment, KINDA_SMALL_NUMBER));
			const FVector RefDir = (Chain.RefJoints[Bone + 1] - Chain.RefJoints[Bone]).GetSafeNormal();
			const FVector LocalDir = Chain.RefComp[Bone].Inverse() * RefDir;
			Chain.LocalDirs.Add(LocalDir);
			// Bend about the two axes perpendicular to the bone's dominant axis, whatever the rig's convention is.
			const float AX = FMath::Abs(LocalDir.X);
			const float AY = FMath::Abs(LocalDir.Y);
			if (AX >= AY && AX >= FMath::Abs(LocalDir.Z))
			{
				Chain.BendAxisA.Add(1);
				Chain.BendAxisB.Add(2);
			}
			else if (AY >= AX && AY >= FMath::Abs(LocalDir.Z))
			{
				Chain.BendAxisA.Add(0);
				Chain.BendAxisB.Add(2);
			}
			else
			{
				Chain.BendAxisA.Add(0);
				Chain.BendAxisB.Add(1);
			}
		}
		Chain.FKJoints = Chain.RefJoints;
		Chains.Add(MoveTemp(Chain));
	}

	UE_LOG(LogGB, Display, TEXT("TentacleRig: %d chains ready"), Chains.Num());
}

int32 UGBTentacleRigComponent::GetBoneCount() const
{
	int32 Count = 0;
	for (const FGBTentacleChain& Chain : Chains)
	{
		Count += Chain.Bones.Num();
	}
	return Count;
}

FVector UGBTentacleRigComponent::GetChainTipPosition(int32 ChainIndex) const
{
	if (!Chains.IsValidIndex(ChainIndex) || Chains[ChainIndex].FKJoints.Num() == 0)
	{
		return FVector::ZeroVector;
	}
	return Chains[ChainIndex].FKJoints.Last();
}

FVector UGBTentacleRigComponent::GetChainTipWorldPosition(int32 ChainIndex) const
{
	if (!TentacleMesh.IsValid())
	{
		return FVector::ZeroVector;
	}
	return TentacleMesh->GetComponentTransform().TransformPosition(GetChainTipPosition(ChainIndex));
}

void UGBTentacleRigComponent::SetReachTarget(const FVector& WorldTarget, float Weight)
{
	ReachTargetWorld = WorldTarget;
	ReachDesired = FMath::Clamp(Weight, 0.f, 1.f);
}

void UGBTentacleRigComponent::ClearReach()
{
	ReachDesired = 0.f;
}

void UGBTentacleRigComponent::SetReachChain(int32 ChainIndex)
{
	ReachChain = (Chains.IsValidIndex(ChainIndex) || ChainIndex < 0) ? ChainIndex : -1;
}

void UGBTentacleRigComponent::AddRipple(float Strength)
{
	RippleElapsed = 0.f;
	RippleStrengthNow = Strength;
}

void UGBTentacleRigComponent::SetLimp(bool bInLimp)
{
	bLimp = bInLimp;
}

int32 UGBTentacleRigComponent::PickReachChain() const
{
	if (Chains.Num() == 0)
	{
		return INDEX_NONE;
	}
	if (Chains.IsValidIndex(ReachChain))
	{
		return ReachChain;
	}
	// The chain whose tip is currently closest to the target reaches.
	const FVector TargetLocal = TentacleMesh.IsValid()
		? TentacleMesh->GetComponentTransform().InverseTransformPosition(ReachTargetWorld)
		: ReachTargetWorld;
	int32 Best = 0;
	float BestDist = FLT_MAX;
	for (int32 Index = 0; Index < Chains.Num(); ++Index)
	{
		const float Dist = FVector::Dist(GetChainTipPosition(Index), TargetLocal);
		if (Dist < BestDist)
		{
			BestDist = Dist;
			Best = Index;
		}
	}
	return Best;
}

void UGBTentacleRigComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!IsReady())
	{
		return;
	}

	FGBTentacleTuning Tuning;
	ReadTuning(Tuning);

	const float Dt = FMath::Min(FMath::Max(DeltaTime, 0.f), GBTentacleRigLocal::MaxDeltaTime);
	Time += Dt;

	// Inertia: actor velocity and yaw rate in the actor frame (X forward, Y right, Z up).
	FVector LocalVel = FVector::ZeroVector;
	float YawRate = 0.f;
	if (AActor* Owner = GetOwner())
	{
		const FQuat ActorQuat = Owner->GetActorQuat();
		LocalVel = ActorQuat.Inverse().RotateVector(Owner->GetVelocity());
		const float Yaw = Owner->GetActorRotation().Yaw;
		if (bHasPrevYaw && Dt > KINDA_SMALL_NUMBER)
		{
			YawRate = FMath::DegreesToRadians(FMath::FindDeltaAngleDegrees(PrevYaw, Yaw) / Dt);
		}
		PrevYaw = Yaw;
		bHasPrevYaw = true;
	}

	const float FwdTarget = FMath::Clamp(LocalVel.X * Tuning.LagStrength,
		-GBTentacleRigLocal::MaxLagDegrees, GBTentacleRigLocal::MaxLagDegrees);
	const float SideTarget = FMath::Clamp(-(LocalVel.Y + YawRate * GBTentacleRigLocal::YawLeverCm) * Tuning.LagStrength,
		-GBTentacleRigLocal::MaxLagDegrees, GBTentacleRigLocal::MaxLagDegrees);
	GBTentacle::SpringStep(LagFwd, LagFwdVel, FwdTarget, Tuning.LagStiffness, Tuning.LagDamping, Dt);
	GBTentacle::SpringStep(LagSide, LagSideVel, SideTarget, Tuning.LagStiffness, Tuning.LagDamping, Dt);

	// Ramps: reach weight and limp.
	const float BlendIn = FMath::Max(Tuning.ReachBlendIn, KINDA_SMALL_NUMBER);
	const float BlendOut = FMath::Max(Tuning.ReachBlendOut, KINDA_SMALL_NUMBER);
	const float ReachStep = (ReachDesired > ReachWeight ? Dt / BlendIn : Dt / BlendOut);
	ReachWeight = FMath::Clamp(ReachWeight + FMath::Clamp(ReachDesired - ReachWeight, -ReachStep, ReachStep), 0.f, 1.f);

	const float LimpGoal = bLimp ? 1.f : 0.f;
	LimpWeight = FMath::Clamp(LimpWeight + FMath::Clamp(LimpGoal - LimpWeight, -Dt, Dt), 0.f, 1.f);
	const float WaveScale = 1.f - LimpWeight;

	RippleElapsed += Dt;

	const int32 SolvingChain = (ReachWeight > KINDA_SMALL_NUMBER) ? PickReachChain() : INDEX_NONE;
	for (int32 Index = 0; Index < Chains.Num(); ++Index)
	{
		PoseChain(Chains[Index], Tuning, WaveScale, Index == SolvingChain);
	}
}

void UGBTentacleRigComponent::PoseChain(FGBTentacleChain& Chain, const FGBTentacleTuning& Tuning, float WaveScale, bool bSolving)
{
	UPoseableMeshComponent* Mesh = TentacleMesh.Get();
	if (!Mesh)
	{
		return;
	}
	const int32 BoneCount = Chain.Bones.Num();
	if (BoneCount == 0)
	{
		return;
	}

	// Reach blend of this chain: the other layers fade out by the weight.
	const float ReachW = bSolving ? ReachWeight : 0.f;
	const float OtherScale = (1.f - ReachW) * WaveScale;

	// Hit ripple: a pulse running from the root to the tip over RippleDuration.
	float RippleAmp = 0.f;
	float RippleFreq = 0.f;
	const float RippleDuration = FMath::Max(Tuning.RippleDuration, KINDA_SMALL_NUMBER);
	const float RippleFront = RippleElapsed / RippleDuration;
	if (RippleFront < 1.5f)
	{
		RippleAmp = RippleStrengthNow;
		RippleFreq = RippleFront;
	}

	// FABRIK for the solving chain, from the current pose towards the target.
	TArray<FVector> Solved;
	if (bSolving)
	{
		Solved = Chain.FKJoints;
		const FVector TargetLocal = Mesh->GetComponentTransform().InverseTransformPosition(ReachTargetWorld);
		GBTentacle::SolveFabrik(Solved, TargetLocal, GBTentacleRigLocal::ReachIterations, GBTentacleRigLocal::ReachToleranceCm);
	}

	FQuat ParentComp = RootCompQuat;
	for (int32 Bone = 0; Bone < BoneCount; ++Bone)
	{
		const float Growth = static_cast<float>(Bone + 1) / static_cast<float>(BoneCount);
		const FVector RefDir = (Chain.RefJoints[Bone + 1] - Chain.RefJoints[Bone]).GetSafeNormal();

		// Layer 1: idle wave, two perpendicular bends, out of step per chain.
		float BendA = GBTentacle::WaveAngle(Time, Tuning.WaveFreq, Chain.Phase, static_cast<float>(Bone),
			Tuning.WaveLength, Tuning.WaveAmp, BoneCount);
		float BendB = GBTentacle::WaveAngle(Time, Tuning.WaveFreq, Chain.Phase + 0.25f, static_cast<float>(Bone),
			Tuning.WaveLength, Tuning.WaveAmp, BoneCount);
		// Layer 4: hit ripple on the first bend axis.
		if (RippleAmp != 0.f)
		{
			const float D = (RippleFreq - Growth) * GBTentacleRigLocal::RippleWidth;
			BendA += RippleAmp * FMath::Exp(-D * D)
				* FMath::Sin(2.f * PI * (GBTentacleRigLocal::RippleWaves * RippleFreq - Growth));
		}
		BendA *= OtherScale;
		BendB *= OtherScale;
		if (Bone < GBTentacleRigLocal::RootLimitedBones)
		{
			BendA = FMath::Clamp(BendA, -Tuning.RootLimit, Tuning.RootLimit);
			BendB = FMath::Clamp(BendB, -Tuning.RootLimit, Tuning.RootLimit);
		}
		const FQuat Extra = FQuat(GBTentacleRigLocal::AxisVector(Chain.BendAxisA[Bone]), FMath::DegreesToRadians(BendA))
			* FQuat(GBTentacleRigLocal::AxisVector(Chain.BendAxisB[Bone]), FMath::DegreesToRadians(BendB));
		const FQuat Local = Chain.RefLocal[Bone] * Extra;

		// Layer 2: inertia lag as a small component-space bend (scaled out on the reach chain).
		const float LagScale = Growth * (1.f - ReachW);
		const FQuat LagDelta = FQuat(FVector::YAxisVector, FMath::DegreesToRadians(LagFwd * LagScale))
			* FQuat(FVector::XAxisVector, FMath::DegreesToRadians(LagSide * LagScale));
		FQuat Comp = LagDelta * ParentComp * Local;

		// Limp droop: rotate the bone direction towards -Z (frame-independent).
		const float Droop = Tuning.LimpDroop * LimpWeight * Growth * (1.f - ReachW);
		if (FMath::Abs(Droop) > KINDA_SMALL_NUMBER)
		{
			const FVector Dir = (Comp * Chain.LocalDirs[Bone]).GetSafeNormal();
			const FVector Axis = FVector::CrossProduct(Dir, FVector::DownVector);
			if (!Axis.IsNearlyZero())
			{
				Comp = GBTentacleRigLocal::AxisAngleQuat(Axis, Droop) * Comp;
			}
		}

		if (bSolving && Solved.Num() == Chain.FKJoints.Num())
		{
			// Layer 3: turn the solved joint positions back into bone rotations (twist-minimal
			// against the reference) and blend the composed pose towards them by the weight.
			// At full weight the FK below reproduces the solved joints exactly.
			const FVector SolvedDir = (Solved[Bone + 1] - Solved[Bone]).GetSafeNormal();
			const FQuat SolvedComp = FQuat::FindBetweenNormals(RefDir, SolvedDir) * Chain.RefComp[Bone];
			Comp = FQuat::Slerp(Comp, SolvedComp, ReachW);
		}
		Comp.Normalize();

		Mesh->SetBoneRotationByName(Chain.Bones[Bone], Comp.Rotator(), EBoneSpaces::ComponentSpace);

		// Forward kinematics for the next bone (and the tip queries): exact, from the same rotations.
		const FVector BoneDir = (Comp * Chain.LocalDirs[Bone]).GetSafeNormal();
		Chain.FKJoints[Bone + 1] = Chain.FKJoints[Bone] + BoneDir * Chain.SegLengths[Bone];
		ParentComp = Comp;
	}
	// The root joint never moves.
	Chain.FKJoints[0] = Chain.RefJoints[0];
}
