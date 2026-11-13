#include "AI/GBTentacleRigComponent.h"

#include "Components/PoseableMeshComponent.h"
#include "BoneContainer.h"
#include "Data/GBDataSubsystem.h"
#include "AI/GBTentacleMath.h"

namespace GBTentacleRigLocal
{
	// Technical constants, not tuning: bone name shape, FABRIK budget, ripple shape, sim sub-steps.
	constexpr int32 MaxNameLength = 6;          // "T<cc>_<bb>"
	constexpr int32 ReachIterations = 10;
	constexpr float ReachToleranceCm = 0.5f;    // well under the 15 cm the smoke run allows
	constexpr float MaxDeltaTime = 0.5f;        // a long hitch is simulated in sub-steps up to this, then dropped
	constexpr float SimStep = 1.f / 60.f;       // longest verlet sub-step
	constexpr int32 SimConstraintPasses = 2;
	constexpr float ReachStiffnessBoost = 3.f;  // the reaching chain follows its solved pose this much harder
	constexpr float SimResetDistanceShare = 1.5f; // tip farther than this share of the chain length from its pose: re-seed the sim
	constexpr float RippleWidth = 4.f;          // sharpness of the travelling pulse
	constexpr float RippleWaves = 2.f;          // oscillations inside the pulse
	constexpr int32 RootLimitedBones = 2;       // bones _00/_01 keep the final cone
	constexpr float WindupStrokeDamp = 0.6f;    // share of the stroke that gives way to the coil at full windup
	constexpr float StreamBackDown = 0.5f;      // streaming tentacles trail back and a bit down
	constexpr float AimForward = 0.8f;          // the facing tentacles orient towards the target, this much sideways...
	constexpr float AimLift = 0.6f;             // ...and this much up
	constexpr float RecoilDown = 0.3f;          // the flinch pushes along the hit and a bit down
	constexpr float LimpDroopScale = 4.f;       // degrees per unit of LimpDroop * weight * growth
	constexpr float WindupBlendBoost = 2.f;     // the windup blends in this much faster than the other intents
	constexpr float AggroStrokeShare = 0.5f;    // the stroke quickens by this share of the windup gain just from aggro
	constexpr int32 MinSyncSamples = 8;

	FQuat AxisAngleQuat(const FVector& Axis, float AngleDeg)
	{
		FVector SafeAxis = Axis.GetSafeNormal();
		if (SafeAxis.IsNearlyZero())
		{
			return FQuat::Identity;
		}
		return FQuat(SafeAxis, FMath::DegreesToRadians(AngleDeg));
	}

	/** Rotates Direction towards RefDir until it is within MaxDeg of it. */
	FVector LimitToCone(const FVector& Direction, const FVector& RefDir, float MaxDeg)
	{
		const float Dot = FMath::Clamp(FVector::DotProduct(Direction, RefDir), -1.f, 1.f);
		const float Angle = FMath::RadiansToDegrees(FMath::Acos(Dot));
		if (Angle <= MaxDeg)
		{
			return Direction;
		}
		FVector Axis = FVector::CrossProduct(RefDir, Direction);
		if (Axis.SizeSquared() < KINDA_SMALL_NUMBER)
		{
			Axis = FVector::CrossProduct(RefDir, FVector::ZAxisVector);
			if (Axis.SizeSquared() < KINDA_SMALL_NUMBER)
			{
				Axis = FVector::YAxisVector;
			}
		}
		return AxisAngleQuat(Axis, MaxDeg).RotateVector(RefDir).GetSafeNormal();
	}

	/** Turns Dir towards Want by at most Deg degrees (never past it). */
	FVector TiltToward(const FVector& Dir, const FVector& Want, float Deg)
	{
		if (Deg <= KINDA_SMALL_NUMBER || Want.IsNearlyZero())
		{
			return Dir;
		}
		const float Angle = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Dir, Want), -1.f, 1.f)));
		const FVector Axis = FVector::CrossProduct(Dir, Want);
		if (Angle < KINDA_SMALL_NUMBER || Axis.SizeSquared() < KINDA_SMALL_NUMBER)
		{
			return Dir;
		}
		return AxisAngleQuat(Axis, FMath::Min(Deg, Angle)).RotateVector(Dir).GetSafeNormal();
	}
}

UGBTentacleRigComponent::UGBTentacleRigComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UGBTentacleRigComponent::BeginPlay()
{
	Super::BeginPlay();
}

float UGBTentacleRigComponent::GetGameValue(FName Key, float DefaultValue) const
{
	const UGBDataSubsystem* Data = UGBDataSubsystem::Get(this);
	return Data ? Data->GetGameValue(Key, DefaultValue) : DefaultValue;
}

void UGBTentacleRigComponent::ReadTuning(FGBTentacleTuning& OutTuning) const
{
	using namespace GBEntityTentacle;
	OutTuning.StrokeHz = FMath::Max(GetGameValue(StrokeHz, StrokeHzDefault), 0.f);
	OutTuning.StrokeAmp = GetGameValue(StrokeAmp, StrokeAmpDefault);
	OutTuning.StrokeSpeedGain = GetGameValue(StrokeSpeedGain, StrokeSpeedGainDefault);
	OutTuning.StrokeWindupGain = GetGameValue(StrokeWindupGain, StrokeWindupGainDefault);
	OutTuning.StrokeSkew = FMath::Clamp(GetGameValue(StrokeSkew, StrokeSkewDefault), 0.f, 0.9f);
	OutTuning.RingPhase = GetGameValue(RingPhase, RingPhaseDefault);
	OutTuning.StrokeLag = GetGameValue(StrokeLag, StrokeLagDefault);
	OutTuning.TipBias = FMath::Max(GetGameValue(TipBias, TipBiasDefault), 0.1f);
	OutTuning.StreamDeg = GetGameValue(StreamDeg, StreamDegDefault);
	OutTuning.AimDeg = GetGameValue(AimDeg, AimDegDefault);
	OutTuning.CoilDeg = GetGameValue(CoilDeg, CoilDegDefault);
	OutTuning.RecoilDeg = GetGameValue(RecoilDeg, RecoilDegDefault);
	OutTuning.RecoilTime = FMath::Max(GetGameValue(RecoilTime, RecoilTimeDefault), 0.05f);
	OutTuning.IntentRate = FMath::Max(GetGameValue(IntentRate, IntentRateDefault), 0.1f);
	OutTuning.FollowStiffness = FMath::Max(GetGameValue(FollowStiffness, FollowStiffnessDefault), 0.f);
	OutTuning.FollowDamping = FMath::Max(GetGameValue(FollowDamping, FollowDampingDefault), 0.f);
	OutTuning.TipSoft = FMath::Clamp(GetGameValue(TipSoft, TipSoftDefault), 0.f, 0.95f);
	OutTuning.Hang = GetGameValue(Hang, HangDefault);
	OutTuning.RootCone0 = GetGameValue(RootCone0, RootCone0Default);
	OutTuning.RootCone1 = GetGameValue(RootCone1, RootCone1Default);
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
		// Guard the shape: only T<digits>_<digits>.
		if (!FChar::IsDigit(Name[1]) || !FChar::IsDigit(Name[2]) || !FChar::IsDigit(Name[4]) || !FChar::IsDigit(Name[5]))
		{
			continue;
		}
		const int32 ChainIndex = FCString::Atoi(*Name.Mid(1, 2));
		const int32 BoneIndex = FCString::Atoi(*Name.Mid(4, 2));
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
		if (Pairs.Num() < 2)
		{
			continue;
		}

		FGBTentacleChain Chain;
		for (const TPair<int32, FName>& Pair : Pairs)
		{
			const FTransform CompT = InMesh->GetBoneTransformByName(Pair.Value, EBoneSpaces::ComponentSpace);
			Chain.Bones.Add(Pair.Value);
			Chain.BoneIndices.Add(InMesh->GetBoneIndex(Pair.Value));
			Chain.RefComp.Add(CompT.GetRotation());
			Chain.RefJoints.Add(CompT.GetLocation());
		}
		const int32 BoneCount = Chain.Bones.Num();

		// Joints are the real bones: the tip is bone _07 itself. Its own direction repeats the one before.
		for (int32 Bone = 0; Bone < BoneCount; ++Bone)
		{
			FVector RefDir;
			if (Bone + 1 < BoneCount)
			{
				const FVector Delta = Chain.RefJoints[Bone + 1] - Chain.RefJoints[Bone];
				Chain.SegLengths.Add(FMath::Max(Delta.Size(), KINDA_SMALL_NUMBER));
				RefDir = Delta.GetSafeNormal();
				if (RefDir.IsNearlyZero())
				{
					RefDir = Chain.RefDirs.Num() > 0 ? Chain.RefDirs.Last() : FVector::ZAxisVector;
				}
			}
			else
			{
				RefDir = Chain.RefDirs.Last();
			}
			Chain.RefDirs.Add(RefDir);
		}
		Chain.FKJoints = Chain.RefJoints;
		Chain.SimPos.SetNum(BoneCount);
		Chain.SimPrev.SetNum(BoneCount);
		Chains.Add(MoveTemp(Chain));
	}

	// Ring layout: each chain's outward direction and its place round the body. The stroke phase comes from
	// that angle (an ordered travelling ring), never from a random number.
	FVector Centre = FVector::ZeroVector;
	for (const FGBTentacleChain& Chain : Chains)
	{
		Centre += FVector(Chain.RefJoints[0].X, Chain.RefJoints[0].Y, 0.f);
	}
	if (Chains.Num() > 0)
	{
		Centre /= static_cast<float>(Chains.Num());
	}
	for (FGBTentacleChain& Chain : Chains)
	{
		FVector Out = FVector(Chain.RefJoints[0].X, Chain.RefJoints[0].Y, 0.f) - Centre;
		if (Out.SizeSquared() < KINDA_SMALL_NUMBER)
		{
			Out = FVector(Chain.RefJoints.Last().X, Chain.RefJoints.Last().Y, 0.f) - Centre;
		}
		Chain.Radial = Out.GetSafeNormal();
		if (Chain.Radial.IsNearlyZero())
		{
			Chain.Radial = FVector::XAxisVector;
		}
		Chain.Tangent = FVector::CrossProduct(FVector::ZAxisVector, Chain.Radial).GetSafeNormal();
		Chain.RingAngle = FMath::Atan2(Chain.Radial.Y, Chain.Radial.X);
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

int32 UGBTentacleRigComponent::GetJointCount(int32 ChainIndex) const
{
	return Chains.IsValidIndex(ChainIndex) ? Chains[ChainIndex].FKJoints.Num() : 0;
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

FVector UGBTentacleRigComponent::GetChainJointWorldPosition(int32 ChainIndex, int32 JointIndex) const
{
	if (!TentacleMesh.IsValid() || !Chains.IsValidIndex(ChainIndex) || !Chains[ChainIndex].FKJoints.IsValidIndex(JointIndex))
	{
		return FVector::ZeroVector;
	}
	return TentacleMesh->GetComponentTransform().TransformPosition(Chains[ChainIndex].FKJoints[JointIndex]);
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

void UGBTentacleRigComponent::AddRecoil(const FVector& WorldDirection, float Strength)
{
	const FVector Direction = WorldDirection.GetSafeNormal();
	if (Direction.IsNearlyZero())
	{
		return;
	}
	RecoilDirWorld = Direction;
	RecoilEnvelope = FMath::Max(RecoilEnvelope, FMath::Clamp(Strength, 0.f, 1.f));
}

void UGBTentacleRigComponent::SetLimp(bool bInLimp)
{
	bLimp = bInLimp;
}

void UGBTentacleRigComponent::StartSyncMeasure()
{
	for (FGBTentacleChain& Chain : Chains)
	{
		Chain.SyncN = 0;
		Chain.SyncX = Chain.SyncY = Chain.SyncXX = Chain.SyncYY = Chain.SyncXY = 0.0;
	}
	bMeasuringSync = true;
}

float UGBTentacleRigComponent::GetSyncCorrelation() const
{
	double Total = 0.0;
	int32 Used = 0;
	for (const FGBTentacleChain& Chain : Chains)
	{
		if (Chain.SyncN < GBTentacleRigLocal::MinSyncSamples)
		{
			continue;
		}
		const double N = Chain.SyncN;
		const double VarX = N * Chain.SyncXX - Chain.SyncX * Chain.SyncX;
		const double VarY = N * Chain.SyncYY - Chain.SyncY * Chain.SyncY;
		if (VarX <= 1e-9 || VarY <= 1e-9)
		{
			continue;
		}
		Total += (N * Chain.SyncXY - Chain.SyncX * Chain.SyncY) / FMath::Sqrt(VarX * VarY);
		++Used;
	}
	return Used > 0 ? static_cast<float>(Total / Used) : 0.f;
}

float UGBTentacleRigComponent::StrokeValue(const FGBTentacleChain& Chain, const FGBTentacleTuning& Tuning, float BoneDelay) const
{
	const float Phase = 2.f * PI * (StrokeCycle - Chain.StrokePhase - BoneDelay);
	// A sine with a second harmonic: a quicker push out and a slower, gathering recovery.
	return (FMath::Sin(Phase) + Tuning.StrokeSkew * FMath::Sin(2.f * Phase)) / (1.f + Tuning.StrokeSkew);
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
	UPoseableMeshComponent* Mesh = TentacleMesh.Get();
	if (!IsReady() || !Mesh)
	{
		return;
	}

	FGBTentacleTuning Tuning;
	ReadTuning(Tuning);

	// The whole frame is simulated (in sub-steps), so the motion keeps its speed at low FPS; only a huge hitch is cut.
	const float Dt = FMath::Min(FMath::Max(DeltaTime, 0.f), GBTentacleRigLocal::MaxDeltaTime);
	Time += Dt;

	// Intent, smoothed: how fast it moves, whether it hunts, whether it winds up.
	const float SpeedGoal = FMath::Clamp(Intent.WorldVelocity.Size2D() / FMath::Max(Intent.FullSpeed, 1.f), 0.f, 1.f);
	SpeedBlend = FMath::FInterpTo(SpeedBlend, SpeedGoal, Dt, Tuning.IntentRate);
	AggroBlend = FMath::FInterpTo(AggroBlend, Intent.bHasTarget ? 1.f : 0.f, Dt, Tuning.IntentRate);
	WindupBlend = FMath::FInterpTo(WindupBlend, FMath::Clamp(Intent.Windup, 0.f, 1.f), Dt, Tuning.IntentRate * GBTentacleRigLocal::WindupBlendBoost);
	RecoilEnvelope = FMath::Max(0.f, RecoilEnvelope - Dt / Tuning.RecoilTime);

	// ONE stroke for the whole body: it speeds up when the body moves fast or prepares an attack.
	const float Quicken = 1.f + Tuning.StrokeSpeedGain * SpeedBlend
		+ Tuning.StrokeWindupGain * FMath::Max(WindupBlend, GBTentacleRigLocal::AggroStrokeShare * AggroBlend);
	StrokeCycle += Dt * Tuning.StrokeHz * Quicken;

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
		PoseChain(Chains[Index], Tuning, WaveScale, Index == SolvingChain, Dt);
	}
	// One refresh for all the local rotations written above.
	Mesh->MarkRefreshTransformDirty();
}

void UGBTentacleRigComponent::PoseChain(FGBTentacleChain& Chain, const FGBTentacleTuning& Tuning, float WaveScale, bool bSolving, float Dt)
{
	UPoseableMeshComponent* Mesh = TentacleMesh.Get();
	const int32 N = Chain.Bones.Num();
	if (!Mesh || N < 2)
	{
		return;
	}

	// Reach blend of this chain: the other layers fade out by the weight.
	const float ReachW = bSolving ? ReachWeight : 0.f;
	const float OtherScale = (1.f - ReachW) * WaveScale;
	const FTransform ToWorld = Mesh->GetComponentTransform();

	// Hit ripple: a pulse running from the root to the tip over RippleDuration (the same on every chain).
	float RippleAmp = 0.f;
	float RippleFront = 0.f;
	const float RippleDuration = FMath::Max(Tuning.RippleDuration, KINDA_SMALL_NUMBER);
	if (RippleElapsed / RippleDuration < 1.5f)
	{
		RippleAmp = RippleStrengthNow;
		RippleFront = RippleElapsed / RippleDuration;
	}

	// Intent directions in component space.
	const FVector StreamBack = (-ToWorld.InverseTransformVectorNoScale(FVector(Intent.WorldVelocity.X, Intent.WorldVelocity.Y, 0.f)).GetSafeNormal()
		+ FVector(0.f, 0.f, -GBTentacleRigLocal::StreamBackDown)).GetSafeNormal();
	FVector ToTargetH = FVector::ZeroVector;
	if (Intent.bHasTarget)
	{
		ToTargetH = ToWorld.InverseTransformPosition(Intent.WorldTarget);
		ToTargetH.Z = 0.f;
		ToTargetH = ToTargetH.GetSafeNormal();
	}
	const float Facing = FMath::Clamp(FVector::DotProduct(Chain.Radial, ToTargetH), 0.f, 1.f); // rear chains stay back
	const FVector AimWant = (ToTargetH * GBTentacleRigLocal::AimForward + FVector(0.f, 0.f, GBTentacleRigLocal::AimLift)).GetSafeNormal();
	const FVector RecoilWant = (ToWorld.InverseTransformVectorNoScale(RecoilDirWorld).GetSafeNormal()
		+ FVector(0.f, 0.f, -GBTentacleRigLocal::RecoilDown)).GetSafeNormal();
	const float RecoilAmount = Tuning.RecoilDeg * FMath::Square(RecoilEnvelope);

	// 1. Target pose (component space): the shared stroke swings every bone about the chain's tangent
	// (+ gathers in, - pushes out), then the intent layers tilt the bone directions.
	TArray<FVector, TInlineAllocator<16>> TargetJoint;
	TArray<FVector, TInlineAllocator<16>> TargetDir;
	TargetJoint.SetNum(N);
	TargetDir.SetNum(N);
	TargetJoint[0] = Chain.RefJoints[0];
	float SharedTip = 0.f; // the shared body command (stroke and coil) at the tip bone, for the sync measure
	for (int32 Bone = 0; Bone < N; ++Bone)
	{
		const float Growth = static_cast<float>(Bone + 1) / static_cast<float>(N);
		const float Weight = FMath::Pow(Growth, Tuning.TipBias); // more motion towards the tip

		const float Stroke = StrokeValue(Chain, Tuning, static_cast<float>(Bone) * Tuning.StrokeLag);
		float Theta = (Tuning.StrokeAmp * Stroke * (1.f - GBTentacleRigLocal::WindupStrokeDamp * WindupBlend) + Tuning.CoilDeg * WindupBlend) * Weight;
		if (Bone == N - 1)
		{
			SharedTip = Theta;
		}
		if (RippleAmp != 0.f)
		{
			const float D = (RippleFront - Growth) * GBTentacleRigLocal::RippleWidth;
			Theta += RippleAmp * FMath::Exp(-D * D) * FMath::Sin(2.f * PI * (GBTentacleRigLocal::RippleWaves * RippleFront - Growth));
		}
		FVector Dir = GBTentacleRigLocal::AxisAngleQuat(Chain.Tangent, Theta * OtherScale).RotateVector(Chain.RefDirs[Bone]);
		Dir = GBTentacleRigLocal::TiltToward(Dir, StreamBack, Tuning.StreamDeg * SpeedBlend * Weight * OtherScale);
		Dir = GBTentacleRigLocal::TiltToward(Dir, AimWant, Tuning.AimDeg * AggroBlend * Facing * (1.f - 0.5f * WindupBlend) * Weight * OtherScale);
		Dir = GBTentacleRigLocal::TiltToward(Dir, RecoilWant, RecoilAmount * Weight * OtherScale);
		// Death: droop towards -Z.
		Dir = GBTentacleRigLocal::TiltToward(Dir, FVector::DownVector, Tuning.LimpDroop * LimpWeight * Growth * GBTentacleRigLocal::LimpDroopScale);
		TargetDir[Bone] = Dir.GetSafeNormal();
		if (Bone + 1 < N)
		{
			TargetJoint[Bone + 1] = TargetJoint[Bone] + TargetDir[Bone] * Chain.SegLengths[Bone];
		}
	}

	// 2. Verlet chain in world space, pulled to the target pose. The root rides the body, so any body motion
	// (flight, bank, pitch, knockback) reaches the tips late and overshoots; gravity keeps the hang.
	const float Scale = FMath::Max(ToWorld.GetScale3D().X, KINDA_SMALL_NUMBER);
	TArray<FVector, TInlineAllocator<16>> TargetWorld;
	TArray<float, TInlineAllocator<16>> LengthW;
	TargetWorld.SetNum(N);
	LengthW.SetNum(N);
	float ChainLengthW = 0.f;
	for (int32 Joint = 0; Joint < N; ++Joint)
	{
		TargetWorld[Joint] = ToWorld.TransformPosition(TargetJoint[Joint]);
		LengthW[Joint] = Joint + 1 < N ? Chain.SegLengths[Joint] * Scale : 0.f;
		ChainLengthW += LengthW[Joint];
	}
	if (Chain.bSimReady && (Chain.SimPos[N - 1] - TargetWorld[N - 1]).Size() > ChainLengthW * GBTentacleRigLocal::SimResetDistanceShare)
	{
		Chain.bSimReady = false; // teleported or never settled: start again from the pose
	}
	if (!Chain.bSimReady)
	{
		for (int32 Joint = 0; Joint < N; ++Joint)
		{
			Chain.SimPos[Joint] = TargetWorld[Joint];
			Chain.SimPrev[Joint] = TargetWorld[Joint];
		}
		Chain.bSimReady = true;
	}

	const int32 Steps = FMath::Clamp(FMath::CeilToInt(Dt / GBTentacleRigLocal::SimStep), 1, 40);
	const float H = Dt / static_cast<float>(Steps);
	const float VelocityKeep = FMath::Exp(-Tuning.FollowDamping * H);
	const float HangScale = Tuning.Hang * (1.f + 2.f * LimpWeight);
	for (int32 Step = 0; Step < Steps; ++Step)
	{
		Chain.SimPos[0] = TargetWorld[0];
		Chain.SimPrev[0] = TargetWorld[0];
		for (int32 Joint = 1; Joint < N; ++Joint)
		{
			const float Along = static_cast<float>(Joint) / static_cast<float>(N - 1);
			const float Stiffness = Tuning.FollowStiffness * (1.f - Tuning.TipSoft * Along)
				* (1.f + GBTentacleRigLocal::ReachStiffnessBoost * ReachW);
			const FVector Velocity = (Chain.SimPos[Joint] - Chain.SimPrev[Joint]) * VelocityKeep;
			FVector Acceleration = (TargetWorld[Joint] - Chain.SimPos[Joint]) * Stiffness;
			Acceleration.Z -= Stiffness * HangScale * ChainLengthW * Along;
			Chain.SimPrev[Joint] = Chain.SimPos[Joint];
			Chain.SimPos[Joint] += Velocity + Acceleration * H * H;
		}
		for (int32 Pass = 0; Pass < GBTentacleRigLocal::SimConstraintPasses; ++Pass)
		{
			for (int32 Joint = 1; Joint < N; ++Joint)
			{
				const FVector Delta = Chain.SimPos[Joint] - Chain.SimPos[Joint - 1];
				const float Length = Delta.Size();
				const FVector Direction = Length > KINDA_SMALL_NUMBER ? Delta / Length : FVector(TargetDir[Joint - 1]);
				Chain.SimPos[Joint] = Chain.SimPos[Joint - 1] + Direction * LengthW[Joint - 1];
			}
		}
	}

	// 3. Joints in component space; the reaching chain is blended towards its FABRIK solution.
	TArray<FVector, TInlineAllocator<16>> Joints;
	Joints.SetNum(N);
	for (int32 Joint = 0; Joint < N; ++Joint)
	{
		Joints[Joint] = ToWorld.InverseTransformPosition(Chain.SimPos[Joint]);
	}
	if (bSolving)
	{
		TArray<FVector> Solved = Chain.FKJoints;
		const FVector TargetLocal = ToWorld.InverseTransformPosition(ReachTargetWorld);
		GBTentacle::SolveFabrik(Solved, TargetLocal, GBTentacleRigLocal::ReachIterations, GBTentacleRigLocal::ReachToleranceCm);
		for (int32 Joint = 1; Joint < N && Joint < Solved.Num(); ++Joint)
		{
			Joints[Joint] = FMath::Lerp(Joints[Joint], Solved[Joint], ReachW);
		}
	}

	// 4. Final pose: directions from the joints, bones _00/_01 limited to a cone around their reference
	// direction (so hang or reach cannot push them through the shell), lengths kept exactly.
	TArray<FVector, TInlineAllocator<16>> Final;
	TArray<FVector, TInlineAllocator<16>> FinalDir;
	Final.SetNum(N);
	FinalDir.SetNum(N);
	Final[0] = Chain.RefJoints[0];
	for (int32 Bone = 0; Bone + 1 < N; ++Bone)
	{
		FVector Direction = (Joints[Bone + 1] - Final[Bone]).GetSafeNormal();
		if (Direction.IsNearlyZero())
		{
			Direction = TargetDir[Bone];
		}
		if (Bone < GBTentacleRigLocal::RootLimitedBones)
		{
			Direction = GBTentacleRigLocal::LimitToCone(Direction, Chain.RefDirs[Bone], Bone == 0 ? Tuning.RootCone0 : Tuning.RootCone1);
		}
		FinalDir[Bone] = Direction;
		Final[Bone + 1] = Final[Bone] + Direction * Chain.SegLengths[Bone];
	}
	FinalDir[N - 1] = FinalDir[N - 2];

	// 5. Bone rotations (twist-minimal against the reference), written as local rotations in one batch.
	FQuat PrevComp = RootCompQuat;
	for (int32 Bone = 0; Bone < N; ++Bone)
	{
		FQuat Comp = FQuat::FindBetweenNormals(Chain.RefDirs[Bone], FinalDir[Bone]) * Chain.RefComp[Bone];
		Comp.Normalize();
		const int32 SkeletonIndex = Chain.BoneIndices[Bone];
		if (Mesh->BoneSpaceTransforms.IsValidIndex(SkeletonIndex))
		{
			Mesh->BoneSpaceTransforms[SkeletonIndex].SetRotation(PrevComp.Inverse() * Comp);
		}
		PrevComp = Comp;
	}

	// 6. Remember the result for the tip queries and as the next sim state.
	for (int32 Joint = 0; Joint < N; ++Joint)
	{
		Chain.FKJoints[Joint] = Final[Joint];
		Chain.SimPos[Joint] = ToWorld.TransformPosition(Final[Joint]);
		if (ReachW > 0.5f)
		{
			Chain.SimPrev[Joint] = Chain.SimPos[Joint];
		}
	}

	// Smoke: the shared stroke against how far this tip really gathers in (radial, towards the body axis).
	if (bMeasuringSync)
	{
		const double X = SharedTip;
		const double Y = -FVector::DotProduct(Chain.FKJoints.Last() - Chain.RefJoints.Last(), Chain.Radial);
		Chain.SyncN++;
		Chain.SyncX += X;
		Chain.SyncY += Y;
		Chain.SyncXX += X * X;
		Chain.SyncYY += Y * Y;
		Chain.SyncXY += X * Y;
	}
}
