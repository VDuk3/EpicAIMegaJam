// Drives the Entity's hologram tentacles on a UPoseableMeshComponent (GDD 11, phase 1 "the machine").
// No AnimBP, no Control Rig: every tick each chain's pose is composed from four layers as rotations
// on top of the reference pose, root to tip:
//  1. idle wave (two perpendicular bends per bone, out of step per chain),
//  2. inertia lag (the actor's velocity and yaw rate trail the tips through a damped spring),
//  3. reach (FABRIK towards a world target on one chain, blended in/out by weight),
//  4. hit ripple (a pulse running root to tip).
// SetLimp fades the waves out and droops the chains towards -Z. Bones _00/_01 keep a root bend limit
// so a chain cannot fold back through the shell.
// Every tuning number is a Game.json value (GBEntityTentacle::*) with the default in code.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GBTentacleRigComponent.generated.h"

class UPoseableMeshComponent;

/** Game.json "Values" keys of the Entity tentacle rig, with code defaults. */
namespace GBEntityTentacle
{
	inline const FName WaveFreq(TEXT("EntityTentacleWaveFreq"));             // Hz of the idle wave
	inline const FName WaveAmp(TEXT("EntityTentacleWaveAmp"));               // degrees at the tip
	inline const FName WaveLength(TEXT("EntityTentacleWaveLength"));         // wave number, in bones
	inline const FName LagStrength(TEXT("EntityTentacleLagStrength"));       // lag degrees per cm/s
	inline const FName LagStiffness(TEXT("EntityTentacleLagStiffness"));     // spring stiffness of the lag
	inline const FName LagDamping(TEXT("EntityTentacleLagDamping"));        // spring damping of the lag
	inline const FName RootLimit(TEXT("EntityTentacleRootLimit"));           // degrees: max bend of bones _00/_01
	inline const FName ReachBlendIn(TEXT("EntityTentacleReachBlendIn"));     // seconds 0 -> 1
	inline const FName ReachBlendOut(TEXT("EntityTentacleReachBlendOut"));   // seconds 1 -> 0
	inline const FName RippleStrength(TEXT("EntityTentacleRippleStrength")); // degrees of the hit pulse
	inline const FName RippleDuration(TEXT("EntityTentacleRippleDuration")); // seconds root -> tip
	inline const FName LimpDroop(TEXT("EntityTentacleLimpDroop"));           // droop degrees per bone when limp

	constexpr float WaveFreqDefault = 0.6f;
	constexpr float WaveAmpDefault = 6.f;
	constexpr float WaveLengthDefault = 2.5f;
	constexpr float LagStrengthDefault = 0.025f;
	constexpr float LagStiffnessDefault = 40.f;
	constexpr float LagDampingDefault = 12.f;
	constexpr float RootLimitDefault = 10.f;
	constexpr float ReachBlendInDefault = 0.3f;
	constexpr float ReachBlendOutDefault = 0.4f;
	constexpr float RippleStrengthDefault = 10.f;
	constexpr float RippleDurationDefault = 0.5f;
	constexpr float LimpDroopDefault = 2.f;
}

/** Tuning values read once per tick from Game.json (with the defaults above). */
struct FGBTentacleTuning
{
	float WaveFreq = GBEntityTentacle::WaveFreqDefault;
	float WaveAmp = GBEntityTentacle::WaveAmpDefault;
	float WaveLength = GBEntityTentacle::WaveLengthDefault;
	float LagStrength = GBEntityTentacle::LagStrengthDefault;
	float LagStiffness = GBEntityTentacle::LagStiffnessDefault;
	float LagDamping = GBEntityTentacle::LagDampingDefault;
	float RootLimit = GBEntityTentacle::RootLimitDefault;
	float ReachBlendIn = GBEntityTentacle::ReachBlendInDefault;
	float ReachBlendOut = GBEntityTentacle::ReachBlendOutDefault;
	float RippleStrength = GBEntityTentacle::RippleStrengthDefault;
	float RippleDuration = GBEntityTentacle::RippleDurationDefault;
	float LimpDroop = GBEntityTentacle::LimpDroopDefault;
};

/** One tentacle chain: bone names plus everything derived from the reference pose at Setup. */
struct FGBTentacleChain
{
	/** Bone names from the chain root (_00) to the tip, in order. */
	TArray<FName> Bones;
	/** Reference component-space positions: one joint per bone plus the extrapolated tip. */
	TArray<FVector> RefJoints;
	/** Reference segment lengths between consecutive joints. */
	TArray<float> SegLengths;
	/** Reference local (parent-space) rotations, one per bone. */
	TArray<FQuat> RefLocal;
	/** Reference component-space rotations, one per bone. */
	TArray<FQuat> RefComp;
	/** Reference bone direction in the bone's own frame (dominant axis), for wave bends. */
	TArray<FVector> LocalDirs;
	/** Which two local axes a bone bends about (both perpendicular to its dominant axis). */
	TArray<int32> BendAxisA;
	TArray<int32> BendAxisB;
	/** Current joint positions in component space, updated every tick (for FABRIK and tip queries). */
	TArray<FVector> FKJoints;
	/** Random wave phase of this chain, in cycles. */
	float Phase = 0.f;
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
	/** Component-space tip (end of the last segment) of a chain, from the last tick. */
	FVector GetChainTipPosition(int32 ChainIndex) const;
	FVector GetChainTipWorldPosition(int32 ChainIndex) const;
	/** World position the reach chain is currently solving towards (or zero when idle). */
	FVector GetReachTargetWorld() const { return ReachTargetWorld; }

	/**
	 * Reach with a chain towards a world target. The chain whose tip is closest solves FABRIK,
	 * unless SetReachChain picked one. Weight 0..1 is the goal; the actual weight ramps at the
	 * blend in/out rates. The other layers fade out of that chain by the weight.
	 */
	void SetReachTarget(const FVector& WorldTarget, float Weight);
	void ClearReach();
	/** -1 (default) = the chain whose tip is closest to the target reaches. */
	void SetReachChain(int32 ChainIndex);

	void AddRipple(float Strength);
	void SetLimp(bool bLimp);
	bool IsLimp() const { return bLimp; }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;

	float GetGameValue(FName Key, float DefaultValue) const;

private:
	void PoseChain(FGBTentacleChain& Chain, const FGBTentacleTuning& Tuning, float WaveScale, bool bSolving);
	void ReadTuning(FGBTentacleTuning& OutTuning) const;
	int32 PickReachChain() const;

	TWeakObjectPtr<UPoseableMeshComponent> TentacleMesh;
	TArray<FGBTentacleChain> Chains;
	/** Reference component-space rotation of the chains' parent bone (the skeleton root). */
	FQuat RootCompQuat = FQuat::Identity;

	float Time = 0.f;

	// Inertia lag: fore/aft and side bend angles (degrees) with spring velocities.
	float LagFwd = 0.f;
	float LagFwdVel = 0.f;
	float LagSide = 0.f;
	float LagSideVel = 0.f;
	float PrevYaw = 0.f;
	bool bHasPrevYaw = false;

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
