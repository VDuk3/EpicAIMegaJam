// Drives the Entity's hologram tentacles on a UPoseableMeshComponent (GDD 11, phase 1 "the machine").
// No AnimBP, no Control Rig. The motion is one coordinated creature, not ten wiggles:
//  1. ONE shared stroke cycle (like a jellyfish pulse): all tentacles gather in, then push out. Each chain
//     is offset by a small ordered phase from its angle around the body (a travelling ring, no random
//     phases). The stroke speeds up with the body's speed and in the windup, and idles slowly.
//  2. Intent layers on top of the stroke, all deterministic and smooth:
//     - moving: the tentacles trail opposite to the velocity and stream behind,
//     - aggro: the tentacles facing the player lift and orient towards it; the rear ones stay back,
//     - windup: all of them draw in and coil, then the chosen one reaches (FABRIK, blended by weight),
//     - a hit: all flinch away together along the hit direction, then settle (plus a travelling ripple),
//     - death: limp droop.
//  3. A verlet chain in WORLD space pulled to that pose by springs (softer at the tip) with a gravity
//     hang: follow-through and overshoot that smooth the purposeful motion, and the hang stays under the
//     body when it banks or pitches.
//  4. The final joints become bone rotations; bones _00/_01 are limited to a cone around the reference
//     direction on the FINAL pose. All local rotations are written in one batch.
// Every tuning number is a Game.json value (GBEntityTentacle::*) with the default in code.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GBTentacleRigComponent.generated.h"

class UPoseableMeshComponent;

/** Game.json "Values" keys of the Entity tentacle rig, with code defaults. */
namespace GBEntityTentacle
{
	inline const FName StrokeHz(TEXT("EntityTentacleStrokeHz"));             // idle stroke cycles per second
	inline const FName StrokeAmp(TEXT("EntityTentacleStrokeAmp"));           // degrees of the gather / push at the tip
	inline const FName StrokeSpeedGain(TEXT("EntityTentacleStrokeSpeedGain")); // extra stroke rate at full speed, as a share of the idle rate
	inline const FName StrokeWindupGain(TEXT("EntityTentacleStrokeWindupGain")); // extra stroke rate in the windup / aggro
	inline const FName StrokeSkew(TEXT("EntityTentacleStrokeSkew"));         // 0 = sine; above 0 = a quicker push and a slower recovery
	inline const FName RingPhase(TEXT("EntityTentacleRingPhase"));           // cycles of phase round the full ring of chains
	inline const FName StrokeLag(TEXT("EntityTentacleStrokeLag"));           // cycles of delay per bone, root to tip (the wave runs down the chain)
	inline const FName TipBias(TEXT("EntityTentacleTipBias"));               // 1 = linear growth to the tip; above 1 = more motion at the tip
	inline const FName StreamDeg(TEXT("EntityTentacleStreamDeg"));           // degrees at the tip that the tentacles stream back at full speed
	inline const FName AimDeg(TEXT("EntityTentacleAimDeg"));                 // degrees at the tip that the facing tentacles lift and orient to the target
	inline const FName CoilDeg(TEXT("EntityTentacleCoilDeg"));               // degrees at the tip that the windup draws all of them in
	inline const FName RecoilDeg(TEXT("EntityTentacleRecoilDeg"));           // degrees at the tip of the flinch on a full-strength hit
	inline const FName RecoilTime(TEXT("EntityTentacleRecoilTime"));         // seconds the flinch takes to settle
	inline const FName IntentRate(TEXT("EntityTentacleIntentRate"));         // 1/s: how fast the intent layers blend in and out
	inline const FName FollowStiffness(TEXT("EntityTentacleFollowStiffness")); // 1/s^2: how hard a joint is pulled to the pose
	inline const FName FollowDamping(TEXT("EntityTentacleFollowDamping"));   // 1/s: velocity loss of the verlet chain
	inline const FName TipSoft(TEXT("EntityTentacleTipSoft"));               // share of stiffness lost at the tip (0..1)
	inline const FName Hang(TEXT("EntityTentacleHang"));                     // gravity sag at the tip, as a share of the chain length
	inline const FName RootCone0(TEXT("EntityTentacleRootCone0"));           // degrees: final pose cone of bone _00 around its reference
	inline const FName RootCone1(TEXT("EntityTentacleRootCone1"));           // degrees: final pose cone of bone _01
	inline const FName ReachBlendIn(TEXT("EntityTentacleReachBlendIn"));     // seconds 0 -> 1
	inline const FName ReachBlendOut(TEXT("EntityTentacleReachBlendOut"));   // seconds 1 -> 0
	inline const FName RippleStrength(TEXT("EntityTentacleRippleStrength")); // degrees of the hit pulse
	inline const FName RippleDuration(TEXT("EntityTentacleRippleDuration")); // seconds root -> tip
	inline const FName LimpDroop(TEXT("EntityTentacleLimpDroop"));           // droop strength when limp

	constexpr float StrokeHzDefault = 0.55f;
	constexpr float StrokeAmpDefault = 30.f;
	constexpr float StrokeSpeedGainDefault = 1.f;
	constexpr float StrokeWindupGainDefault = 1.2f;
	constexpr float StrokeSkewDefault = 0.3f;
	constexpr float RingPhaseDefault = 0.25f;
	constexpr float StrokeLagDefault = 0.07f;
	constexpr float TipBiasDefault = 1.4f;
	constexpr float StreamDegDefault = 35.f;
	constexpr float AimDegDefault = 40.f;
	constexpr float CoilDegDefault = 32.f;
	constexpr float RecoilDegDefault = 30.f;
	constexpr float RecoilTimeDefault = 0.8f;
	constexpr float IntentRateDefault = 4.f;
	constexpr float FollowStiffnessDefault = 90.f;
	constexpr float FollowDampingDefault = 6.f;
	constexpr float TipSoftDefault = 0.45f;
	constexpr float HangDefault = 0.12f;
	constexpr float RootCone0Default = 30.f;
	constexpr float RootCone1Default = 55.f;
	constexpr float ReachBlendInDefault = 0.3f;
	constexpr float ReachBlendOutDefault = 0.4f;
	constexpr float RippleStrengthDefault = 10.f;
	constexpr float RippleDurationDefault = 0.5f;
	constexpr float LimpDroopDefault = 2.f;
}

/** Tuning values read once per tick from Game.json (with the defaults above). */
struct FGBTentacleTuning
{
	float StrokeHz = GBEntityTentacle::StrokeHzDefault;
	float StrokeAmp = GBEntityTentacle::StrokeAmpDefault;
	float StrokeSpeedGain = GBEntityTentacle::StrokeSpeedGainDefault;
	float StrokeWindupGain = GBEntityTentacle::StrokeWindupGainDefault;
	float StrokeSkew = GBEntityTentacle::StrokeSkewDefault;
	float RingPhase = GBEntityTentacle::RingPhaseDefault;
	float StrokeLag = GBEntityTentacle::StrokeLagDefault;
	float TipBias = GBEntityTentacle::TipBiasDefault;
	float StreamDeg = GBEntityTentacle::StreamDegDefault;
	float AimDeg = GBEntityTentacle::AimDegDefault;
	float CoilDeg = GBEntityTentacle::CoilDegDefault;
	float RecoilDeg = GBEntityTentacle::RecoilDegDefault;
	float RecoilTime = GBEntityTentacle::RecoilTimeDefault;
	float IntentRate = GBEntityTentacle::IntentRateDefault;
	float FollowStiffness = GBEntityTentacle::FollowStiffnessDefault;
	float FollowDamping = GBEntityTentacle::FollowDampingDefault;
	float TipSoft = GBEntityTentacle::TipSoftDefault;
	float Hang = GBEntityTentacle::HangDefault;
	float RootCone0 = GBEntityTentacle::RootCone0Default;
	float RootCone1 = GBEntityTentacle::RootCone1Default;
	float ReachBlendIn = GBEntityTentacle::ReachBlendInDefault;
	float ReachBlendOut = GBEntityTentacle::ReachBlendOutDefault;
	float RippleStrength = GBEntityTentacle::RippleStrengthDefault;
	float RippleDuration = GBEntityTentacle::RippleDurationDefault;
	float LimpDroop = GBEntityTentacle::LimpDroopDefault;
};

/** What the owner is doing, fed every tick: the rig turns it into coordinated motion. */
struct FGBTentacleIntent
{
	/** World velocity of the body and its full speed (for the stream-back layer). */
	FVector WorldVelocity = FVector::ZeroVector;
	float FullSpeed = 1.f;
	/** The player (aggro): the tentacles facing it lift and orient. */
	bool bHasTarget = false;
	FVector WorldTarget = FVector::ZeroVector;
	/** 0..1, the windup: all tentacles draw in and coil. */
	float Windup = 0.f;
};

/** One tentacle chain: bone names plus everything derived from the reference pose at Setup. */
struct FGBTentacleChain
{
	/** Bone names from the chain root (_00) to the tip bone (_07), in order. */
	TArray<FName> Bones;
	/** Skeleton index of each bone (for the batched local pose write). */
	TArray<int32> BoneIndices;
	/** Reference component-space positions: one joint per bone. The last joint is the real tip bone. */
	TArray<FVector> RefJoints;
	/** Reference direction of each bone towards the next joint (the last bone repeats the one before it). */
	TArray<FVector> RefDirs;
	/** Reference segment lengths between consecutive joints (one fewer than bones). */
	TArray<float> SegLengths;
	/** Reference component-space rotations, one per bone. */
	TArray<FQuat> RefComp;
	/** Current joint positions in component space, updated every tick (for FABRIK and tip queries). */
	TArray<FVector> FKJoints;
	/** Verlet state in world space. */
	TArray<FVector> SimPos;
	TArray<FVector> SimPrev;
	bool bSimReady = false;
	/** Where the chain sits round the body (component space, horizontal): angle, outward direction, the tangent it swings about. */
	float RingAngle = 0.f;
	FVector Radial = FVector::XAxisVector;
	FVector Tangent = FVector::YAxisVector;
	/** Ordered stroke phase of this chain, in cycles (from its angle, never random). */
	float StrokePhase = 0.f;

	// Sync measurement (Pearson sums of the stroke against the measured gather of the tip).
	int32 SyncN = 0;
	double SyncX = 0.0, SyncY = 0.0, SyncXX = 0.0, SyncYY = 0.0, SyncXY = 0.0;
};

UCLASS(ClassGroup = (Glyphbound), meta = (BlueprintSpawnableComponent))
class GLYPHBOUND_API UGBTentacleRigComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGBTentacleRigComponent();

	/** Reads the reference pose and finds the T<cc>_<bb> chains. Safe to call once the skeletal mesh is set. */
	void Setup(UPoseableMeshComponent* InMesh);

	bool IsReady() const { return TentacleMesh.IsValid() && Chains.Num() > 0; }
	int32 GetChainCount() const { return Chains.Num(); }
	/** Total driven bones over all chains. */
	int32 GetBoneCount() const;
	/** Joints (= bones) of a chain. */
	int32 GetJointCount(int32 ChainIndex) const;
	/** Component-space position of the real tip bone (_07) of a chain, from the last tick. */
	FVector GetChainTipPosition(int32 ChainIndex) const;
	FVector GetChainTipWorldPosition(int32 ChainIndex) const;
	/** World position of one joint (bone) of a chain, from the last tick. */
	FVector GetChainJointWorldPosition(int32 ChainIndex, int32 JointIndex) const;
	/** World position the reach chain is currently solving towards (or zero when idle). */
	FVector GetReachTargetWorld() const { return ReachTargetWorld; }

	/** The owner's intent for this tick (read at the next rig tick). */
	void SetIntent(const FGBTentacleIntent& InIntent) { Intent = InIntent; }
	/** A hit: every tentacle flinches along WorldDirection (the way the shot travelled), Strength 0..1. */
	void AddRecoil(const FVector& WorldDirection, float Strength);

	/**
	 * Reach with a chain towards a world target. The chain whose tip is closest solves FABRIK,
	 * unless SetReachChain picked one. Weight 0..1 is the goal; the actual weight ramps at the
	 * blend in/out rates. The other layers fade out of that chain by the weight.
	 */
	void SetReachTarget(const FVector& WorldTarget, float Weight);
	void ClearReach();
	/** -1 (default) = the chain whose tip is closest to the target reaches. */
	void SetReachChain(int32 ChainIndex);
	bool IsReaching() const { return ReachDesired > 0.f; }

	void AddRipple(float Strength);
	void SetLimp(bool bLimp);
	bool IsLimp() const { return bLimp; }

	/** Smoke: starts / reads the sync measure: mean over the chains of the correlation between the shared stroke and the measured gather of each tip. */
	void StartSyncMeasure();
	float GetSyncCorrelation() const;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;

	float GetGameValue(FName Key, float DefaultValue) const;

private:
	void PoseChain(FGBTentacleChain& Chain, const FGBTentacleTuning& Tuning, float WaveScale, bool bSolving, float Dt);
	void ReadTuning(FGBTentacleTuning& OutTuning) const;
	int32 PickReachChain() const;
	/** The shared stroke of one chain at the current phase plus a per-bone delay (cycles): -1 (push out) .. +1 (gather in). */
	float StrokeValue(const FGBTentacleChain& Chain, const FGBTentacleTuning& Tuning, float BoneDelay) const;

	TWeakObjectPtr<UPoseableMeshComponent> TentacleMesh;
	TArray<FGBTentacleChain> Chains;
	/** Reference component-space rotation of the chains' parent bone (the skeleton root). */
	FQuat RootCompQuat = FQuat::Identity;

	float Time = 0.f;
	/** Shared stroke phase, in cycles: advances faster with speed and windup. */
	float StrokeCycle = 0.f;

	FGBTentacleIntent Intent;
	// Smoothed intent: speed share, aggro weight, windup weight, and the flinch (direction in world space, envelope 0..1).
	float SpeedBlend = 0.f;
	float AggroBlend = 0.f;
	float WindupBlend = 0.f;
	FVector RecoilDirWorld = FVector::ZeroVector;
	float RecoilEnvelope = 0.f;
	bool bMeasuringSync = false;

	// Reach: world target, ramped weight, and which chain solves (-1 = closest tip).
	FVector ReachTargetWorld = FVector::ZeroVector;
	float ReachWeight = 0.f;
	float ReachDesired = 0.f;
	int32 ReachChain = -1;

	// Hit ripple: time since AddRipple and its strength.
	float RippleElapsed = 1000.f;
	float RippleStrengthNow = 0.f;

	bool bLimp = false;
	float LimpWeight = 0.f;
};
