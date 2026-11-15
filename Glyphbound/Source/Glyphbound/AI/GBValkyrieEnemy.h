// The Valkyrie of the future: a boss fight on top of the skeletal shooter. Every number is in Enemies.json "Stats" of her entry
// (names below, "<Key>P2" overrides a key in phase 2), the defaults here only keep a missing key from crashing.
//
// The loop: she keeps a band round the player, strafing, and fires bursts of light bolts from her sword (the base class' burst).
// Every DecisionInterval she may pick, by weight and cooldown, one of:
//   Combo      player within MeleeRange: two or three sword swings (windup the player can read, a step forward, a damage cone at the
//              swing's hit time); afterwards a wing-assisted hop backwards.
//   Dash       middle range: a fast dash that closes in and opens a combo.
//   Judgement  she rises, marks circles on the floor round and at the player (a disc grows to the ring over JudgementTelegraph),
//              then a pillar of light strikes each (AGBJudgementMark).
//   Lances     a fan of light lances (projectiles that fly straight, dodgeable between the gaps).
//   Empower    (only with WeightEmpower > 0: the final boss) a flourish (Anim.Action6), then for EmpowerTime her swings deal
//              EmpowerDamageMult more and the blade carries the Fx.EmpowerTrail systems on the mesh sockets Fx.EmpowerSockets.
// Under PhaseTwoHealthFraction of her health: shorter cooldowns, more circles, longer combos (the "P2" keys).
// The look is only reached through AGBSkeletalEnemy (anim instance, weapon, muzzle), all guarded: a sprite look just has no clips,
// so every timing falls back to its data value.
#pragma once

#include "CoreMinimal.h"
#include "AI/GBSkeletalEnemy.h"
#include "GBValkyrieEnemy.generated.h"

class AGBValkyrieEnemy;
class UStaticMeshComponent;
class UPointLightComponent;
class UNiagaraSystem;
class USceneComponent;

/**
 * One circle of the Judgement spell: a glowing ring on the floor (it marks the final reach), a disc that grows to it over the
 * telegraph, then a pillar of light with a flash: damage to the target standing in the circle at that moment.
 */
UCLASS()
class GLYPHBOUND_API AGBJudgementMark : public AActor
{
	GENERATED_BODY()

public:
	AGBJudgementMark();

	/** StrikeDelay: seconds after the telegraph ends before this one strikes (the circles strike one after another). */
	void Init(AGBValkyrieEnemy* InOwner, AActor* InTarget, const FVector& InCenter, float InRadius, float InTelegraph, float InStrikeDelay,
		float InStrikeTime, float InDamage, const FLinearColor& InColor, UNiagaraSystem* InImpactFx);

	virtual void Tick(float DeltaSeconds) override;

	bool HasStruck() const { return bStruck; }

private:
	void Strike();

	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Disc;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Pillar;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UPointLightComponent> Light;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> RingParts;
	UPROPERTY() TObjectPtr<UNiagaraSystem> ImpactFx;

	TWeakObjectPtr<AGBValkyrieEnemy> Owner;
	TWeakObjectPtr<AActor> Target;
	FVector Center = FVector::ZeroVector;
	FLinearColor Color = FLinearColor::White;
	float Radius = 200.f;
	float TelegraphTime = 1.2f;
	float StrikeDelay = 0.f;
	float StrikeTime = 0.45f;
	float Damage = 0.f;
	float Age = 0.f;
	bool bStruck = false;
	float StruckAge = 0.f;
	bool bInitialized = false;
	/** The pixel Valkyrie's marks (her caster has the sprite look): flat pixel sprites and a baked pixel blast, no 3D shapes. */
	bool bPixel = false;
};

UCLASS()
class GLYPHBOUND_API AGBValkyrieEnemy : public AGBSkeletalEnemy
{
	GENERATED_BODY()

public:
	AGBValkyrieEnemy();

	/** What she is doing: the fight loop between actions, or one of the actions. */
	enum class EAct : uint8 { Fight, Combo, Dash, Hop, Judgement, Lances, Empower };

	/** The kinds of action, for the smoke's forced start. */
	enum class EKind : uint8 { Combo, Dash, Judgement, Lances, Empower };

	virtual FString DescribeTactics() const override;

	// ---- Smoke ----
	EAct GetAct() const { return Act; }
	bool IsPhaseTwo() const { return bPhaseTwo; }
	/** Raised off the floor by Judgement (flying while she casts). */
	bool IsLifted() const { return bLifted; }
	int32 GetSwingCount() const { return SwingCount; }
	int32 GetSwingHits() const { return SwingHits; }
	int32 GetComboCount() const { return ComboCount; }
	int32 GetDashCount() const { return DashCount; }
	int32 GetHopCount() const { return HopCount; }
	int32 GetJudgementCasts() const { return JudgementCasts; }
	int32 GetCirclesMade() const { return CirclesMade; }
	int32 GetCirclesStruck() const { return CirclesStruck; }
	int32 GetCircleHits() const { return CircleHits; }
	int32 GetLancesFired() const { return LancesFired; }
	int32 GetVolleys() const { return VolleyCount; }
	/** Action clips (swings, casts) that started playing: a fight with no clip loaded plays none. */
	int32 GetActionClipsPlayed() const { return ActionClipsPlayed; }
	/** Height the capsule is lifted off the floor by the Judgement cast right now (cm). */
	float GetLift() const { return LiftApplied; }
	/** True when the right hand has its grip curl and the clip it is playing as an action, for the log. */
	bool HasGripCurl() const { return AnimInst && AnimInst->HasGrip(); }
	/** The action starts at her next tick whatever its cooldown and the distance (it aborts a burst). */
	void DebugQueue(EKind Kind) { QueuedKind = static_cast<int32>(Kind); }
	/** Smoke: which kinds her own choice may pick (one bit per EKind, all by default); a queued action ignores it. */
	void DebugAutoMask(int32 Mask) { AutoMask = Mask; }
	/** Smoke: she stands still and does not shoot (only the fight loop; a queued action still runs). */
	void DebugFreeze(bool bOn) { bFrozen = bOn; }
	/** Phase two now, whatever her health. */
	void DebugPhaseTwo();
	/** Smoke: the circle marks that were made and are still standing. */
	int32 GetLiveMarks() const;

	/** A circle of her Judgement struck: whether the target stood inside it. */
	void NotifyMarkStruck(bool bHitTarget);

protected:
	virtual void BeginPlay() override;
	/** Her Judgement marks are actors of their own: they go with her (destroyed during a telegraph, level change). */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual bool TickRanged(AActor* Goal, float DeltaSeconds) override;
	virtual void HandleDeath(const FGBDamageInfo& KillingBlow) override;
	virtual void OnReturnedToUnaware() override;

	/** A stat of this fight: "<Key>P2" in phase two when it exists, else "<Key>", else the default. */
	float V(const TCHAR* Key, float Default) const;

	void TickFight(AActor* Goal, float DeltaSeconds, float Distance);
	void TickFightMovement(AActor* Goal, float DeltaSeconds, float Distance);
	void TickBursts(AActor* Goal, float DeltaSeconds, float Distance);
	void ChooseAction(AActor* Goal, float Distance);
	void StartAction(EKind Kind, AActor* Goal);

	void StartCombo(AActor* Goal);
	void StartSwing(int32 Index, AActor* Goal);
	void TickCombo(AActor* Goal, float DeltaSeconds);
	void SwingHit(AActor* Goal);
	void EndCombo(AActor* Goal);

	void StartDash(AActor* Goal, float Distance);
	void TickDash(AActor* Goal, float DeltaSeconds);

	void StartHop(AActor* Goal);
	void TickHop(AActor* Goal, float DeltaSeconds);

	void StartJudgement(AActor* Goal);
	void TickJudgement(AActor* Goal, float DeltaSeconds);
	void SpawnMarks(AActor* Goal);
	void SetLift(float Wanted);
	void EndLift();
	void EnterPhaseTwo();

	void StartEmpower(AActor* Goal);
	void TickEmpower(AActor* Goal, float DeltaSeconds);
	/** Empowered blade on (the trail systems on the blade sockets) or off. */
	void SetEmpowered(bool bOn);
	bool IsEmpowered() const { return EmpowerTimeLeft > 0.f; }

	void StartLances(AActor* Goal);
	void TickLances(AActor* Goal, float DeltaSeconds);
	void FireVolley(AActor* Goal);

	/** Back to the fight loop with a pause; clears every action state (also when she dies or loses the player). */
	void FinishAction(float Pause);
	void ResetFightState();
	void DestroyMarks();
	/** Plays action clip Index (EGBEnemyClip::Action0 + Index); false when the clip is missing. Its length (or the fallback) in OutLength. */
	bool PlayActionClip(int32 Index, float& OutLength);
	/** Sets the horizontal velocity on the ground (a step, a dash, a hop). */
	void PushHorizontal(const FVector& Direction, float Speed);
	/** The floor under a point, found from above (false: no floor within reach). */
	bool FindFloor(const FVector& Around, float ReferenceZ, FVector& OutPoint) const;

	EAct Act = EAct::Fight;
	bool bFightInit = false;
	bool bPhaseTwo = false;
	int32 QueuedKind = -1;
	int32 AutoMask = 0x1F;
	bool bFrozen = false;

	// Fight loop
	float DecisionTimer = 0.f;
	float ActionPause = 0.f;
	float StrafeTimer = 0.f;
	float StrafeStall = 0.f;
	float StrafeSide = 1.f;
	float ComboCooldown = 0.f;
	float DashCooldown = 0.f;
	float SpellCooldown = 0.f;
	float LanceCooldown = 0.f;
	EBurstPhase LastBurstPhase = EBurstPhase::Cooldown;

	// Combo
	int32 ComboSwingsWanted = 2;
	int32 SwingIndex = 0;
	float SwingTime = 0.f;
	float SwingHitAt = 0.4f;
	float SwingNextAt = 0.8f;
	bool bSwingHit = false;
	bool bComboAfterDash = false;

	// Dash, hop
	float ActTime = 0.f;
	float DashTimeLeft = 0.f;
	FVector HopDirection = FVector::ZeroVector;

	// Judgement
	float JudgementTotal = 0.f;
	bool bMarksSpawned = false;
	float LiftApplied = 0.f;
	float LiftBaseZ = 0.f;
	bool bLifted = false;
	int32 MarksWanted = 0;
	TArray<TWeakObjectPtr<AGBJudgementMark>> Marks;

	// Empower
	float EmpowerCooldown = 0.f;
	float EmpowerTimeLeft = 0.f;
	float EmpowerClipLength = 0.f;
	UPROPERTY() TArray<TObjectPtr<class UNiagaraComponent>> EmpowerFx;

	// Lances
	int32 VolleysDone = 0;
	int32 VolleysWanted = 1;

	// Counters
	int32 SwingCount = 0;
	int32 SwingHits = 0;
	int32 ComboCount = 0;
	int32 DashCount = 0;
	int32 HopCount = 0;
	int32 JudgementCasts = 0;
	int32 CirclesMade = 0;
	int32 CirclesStruck = 0;
	int32 CircleHits = 0;
	int32 LancesFired = 0;
	int32 VolleyCount = 0;
	int32 ActionClipsPlayed = 0;
};
