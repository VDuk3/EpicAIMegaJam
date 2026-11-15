// The horde chief: an elite armed orc with a shield and a heavy pistol (owner, 2026-11-14). A skeletal shooter with a role of
// its own (Anchor): he walks at the player with the shield raised, always facing him but turning slowly, lowers the shield to
// aim (a long telegraph), fires ONE heavy shot, stays open for a moment and raises it again. While raised, damage coming from
// inside the frontal arc is multiplied by ShieldDamageTaken (0) through the attribute component's incoming-damage hook, so
// bullets, projectiles and spells are all blocked; a box as big as the shield stops the player's shot traces. Enough blocked
// damage, a lightning stun or a freeze knocks the shield aside for StaggerTime. Close to him he bashes with the shield.
// The shield's place is kept on the capsule (raised: upright in front of his chest, where its hit box is; lowered: turned to
// his left side at the hip; pushed forward in the bash) and his left hand is put on its handle by two-bone IK, so the shield
// is always in his fist. The right arm holds the pistol one-handed, like a revolver: low at his side while he walks behind
// the shield, stretched out at the player while he aims (IK too, the pistol turned along the aim). Shots and hits use
// procedural kicks, never a clip (the clips are two-handed and would pull the arms off). When he dies it goes with the hand.
// Places (cm at scale 1, from the capsule centre): ShieldHandleForward / Side / Height, ShieldLoweredForward / Side / Height,
// ShieldLoweredYaw; ShieldScale (the mesh's scale, default MeshScale), ShieldGripBack, BashPull, BashPush.
// Everything comes from Enemies.json (Strings: Shield, ShieldSocket, Pose.ShieldRaised; stats: ShieldArc,
// ShieldDamageTaken, ShieldRecover, ShieldStaggerDamage, StaggerTime, ShieldWidth, ShieldHeight, ShieldThickness,
// ShieldPoseBlend, MeleeRange, MeleeDamageMult). Without the shield art a slab of the same size stands in.
#pragma once

#include "CoreMinimal.h"
#include "AI/GBSkeletalEnemy.h"
#include "AI/GBTactics.h"
#include "GBShieldEnemy.generated.h"

class UBoxComponent;

UCLASS()
class GLYPHBOUND_API AGBShieldEnemy : public AGBSkeletalEnemy
{
	GENERATED_BODY()

public:
	AGBShieldEnemy();

	virtual bool IsSquadAnchor() const override { return true; }
	virtual FString DescribeTactics() const override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void ApplyControlLock(float Seconds) override;

	/** True while the shield blocks (raised, not knocked aside, alive). */
	bool IsShieldBlocking() const { return bShieldUp && StaggerTimer <= 0.f && State != EGBEnemyState::Dead && ShieldAlpha >= 0.8f; }
	/** The shield is meant to be up (the pose it moves towards): blocking starts only when it has got there. */
	bool WantsShieldUp() const { return bShieldUp && StaggerTimer <= 0.f && State != EGBEnemyState::Dead; }
	bool IsShieldUp() const { return bShieldUp; }
	bool IsStaggered() const { return StaggerTimer > 0.f; }
	int32 GetBlockedHits() const { return BlockedHits; }
	/** Smoke: the times of his shots, and how many were fired while the shield was still up (must be 0). */
	const TArray<float>& GetShotTimes() const { return ShotTimes; }
	int32 GetShotsWithShieldUp() const { return ShotsWithShieldUp; }
	UBoxComponent* GetShieldBox() const { return ShieldBox; }
	UStaticMeshComponent* GetShieldMesh() const { return ShieldMesh; }
	/** Smoke: lowers or raises the shield now (a screenshot of each pose). */
	void DebugSetShield(bool bUp);
	/** Smoke closeups: 0 normal behaviour, 1 hold the shield raised, 2 hold it lowered with the pistol aimed at the player, 3 hold the bash's strike. */
	void DebugHold(int32 Mode) { DebugHoldMode = Mode; }
	/** Smoke: replaces the raised-shield pose layer (bone:pitch,yaw,roll;...) to try values on the closeups. */
	void DebugSetPose(const FString& Spec);
	/** Smoke: the damage hook as a pure function of what is raised (used with a damage info built by the check). */
	void DebugApplyIncoming(FGBDamageInfo& Info) { HandleIncomingDamage(Info); }

protected:
	virtual void BeginPlay() override;
	virtual bool TickRanged(AActor* Goal, float DeltaSeconds) override;
	virtual void HandleDeath(const FGBDamageInfo& KillingBlow) override;
	virtual void OnWeaponDropTime() override;

private:
	enum class EChiefPhase : uint8 { Approach, Aim, Open, Bash };

	void BuildShield();
	void HandleIncomingDamage(FGBDamageInfo& Info);
	void UpdateShieldVisuals(float DeltaSeconds = 0.f);
	/** The arms' IK goals: the left hand on the shield's handle, the right one with the pistol (low, or aimed). */
	void UpdateArms(float DeltaSeconds);
	/** cm the shield is pushed forward by the bash right now (negative while he pulls it back). */
	float ComputeBashPush() const;

	UPROPERTY() TObjectPtr<UStaticMeshComponent> ShieldMesh;
	UPROPERTY() TObjectPtr<UBoxComponent> ShieldBox;

	bool bShieldUp = true;
	int32 DebugHoldMode = 0;
	float StaggerTimer = 0.f;
	/** The blocked hits of the last few seconds (the stagger sums only those). */
	TArray<GBTactics::FBlockedHit> BlockedLog;
	int32 BlockedHits = 0;
	EChiefPhase Phase = EChiefPhase::Approach;
	float PhaseTimer = 0.f;
	float AttackCooldown = 0.f;
	float BashCooldown = 0.f;
	int32 ShotsWithShieldUp = 0;
	TArray<float> ShotTimes;
	bool bChiefAware = false;
	float ShieldHeightCm = 210.f;
	float ShieldWidthCm = 150.f;
	float PoseBlend = 0.3f;
	/** 1 raised, 0 lowered: where the shield is between its two places on the capsule. */
	float ShieldAlpha = 1.f;
	FTransform ShieldRaisedPlace = FTransform::Identity;
	FTransform ShieldLoweredPlace = FTransform::Identity;
	/** The shield's place on the capsule this frame (the hand's IK goal follows it). */
	FTransform ShieldPlace = FTransform::Identity;
	float ShieldScale = 1.f;
	/** 0 pistol low at his side, 1 aimed at the player. */
	float AimAlpha = 0.f;
	FVector AimPoint = FVector::ZeroVector;
	bool bHasAimPoint = false;
	/** Seconds left of the bash's follow-through (the shield comes back). */
	float BashRecover = 0.f;
};
