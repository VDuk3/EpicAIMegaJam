// The squad of an encounter (or of the encounter-less enemies together): what its members know about the player (the last
// known position), who has told whom, who may fire, who goes where. A world subsystem ticked a few times a second; the
// enemies call into it from their own ticks. No behaviour tree or EQS assets: C++ and Game.json / Enemies.json only.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "AI/GBTactics.h"
#include "GBSquad.generated.h"

class AGBEnemyCharacter;
class AGBEncounter;

/** A tactical position handed to a ranged member. */
struct FGBTacticalPosition
{
	/** Where it stands (the cover point, or the open point). */
	FVector Cover = FVector::ZeroVector;
	/** Where it steps to fire from: the cover point itself for an open position. */
	FVector FiringSpot = FVector::ZeroVector;
	bool bCovered = false;
	float Score = 0.f;
	bool bValid = false;
};

UCLASS()
class GLYPHBOUND_API UGBSquadSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Null when there is no world. */
	static UGBSquadSubsystem* Get(const UObject* WorldContext);
	/** Every enemy that hears a shot fired at Location within its HearingRange notices (the player's guns call this). */
	static void ReportNoise(const UObject* WorldContext, const FVector& Location);

	virtual void Tick(float DeltaSeconds) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UGBSquadSubsystem, STATGROUP_Tickables); }
	virtual bool IsTickableWhenPaused() const override { return false; }

	/** Idempotent: puts the enemy into its encounter's squad (or the shared encounter-less squad). */
	void Join(AGBEnemyCharacter* Enemy);
	void Leave(AGBEnemyCharacter* Enemy);

	// ---- What the squad knows ----
	/** Someone of the squad sees the player now. */
	void ReportSighting(const AGBEnemyCharacter* Enemy, const FVector& PlayerLocation);
	/** The shared last known position; false when nobody has seen the player yet. Age in seconds. */
	bool GetLastKnown(const AGBEnemyCharacter* Enemy, FVector& OutLocation, float& OutAge) const;
	/** Seconds since this squad was alerted (large when never). */
	float SecondsSinceAlert(const AGBEnemyCharacter* Enemy) const;
	/** Source noticed the player: the other unaware members are alerted with staggered delays, the encounter is told. */
	void AlertSquad(AGBEnemyCharacter* Source, const FVector& StimulusLocation, bool bConfirmedSighting);

	// ---- Tokens (M2) ----
	/** True when the member holds a token now or was granted one. Members that wait are served by the squad tick. */
	bool TryAcquireToken(AGBEnemyCharacter* Enemy, float Weight);
	void ReleaseToken(AGBEnemyCharacter* Enemy);
	bool HoldsToken(const AGBEnemyCharacter* Enemy) const;

	// ---- Roles and positions (M2) ----
	EGBSquadRole GetRole(const AGBEnemyCharacter* Enemy) const;
	/** Asks for a new tactical position (answered by a later squad tick, one query per tick). */
	void RequestPosition(AGBEnemyCharacter* Enemy);
	bool HasPendingRequest(const AGBEnemyCharacter* Enemy) const;
	/** The position assigned to it since its request; true once, then the assignment is its own. */
	bool TakeAssignedPosition(AGBEnemyCharacter* Enemy, FGBTacticalPosition& Out);
	/** True when a role was changed since the member last asked (it should relocate). */
	bool TakeRoleChanged(AGBEnemyCharacter* Enemy);
	/**
	 * A melee member asks at the moment it starts a windup or a lunge: true when fewer than SquadMaxMeleeAttackers members hold an attack place
	 * (the place is held until EndMeleeAttack, death or a 4 s lease). Members outside a squad are never limited.
	 */
	bool TryBeginMeleeAttack(AGBEnemyCharacter* Enemy);
	void EndMeleeAttack(AGBEnemyCharacter* Enemy);
	/**
	 * The pincer (members with the stat PincerWait > 0, the castle's melee): an attacker at its slot waits for the other attackers
	 * of the squad to reach theirs, so they close in together from different sides; it goes at once when a partner has just
	 * committed (the other jaw), when it is the only attacker, or after PincerWait seconds. Members without the stat: always true.
	 */
	bool MayCommitMelee(AGBEnemyCharacter* Enemy);
	/** 0..1: how open the player's escape is towards a bearing (degrees) from him; 1 = nothing blocks him that way (also before the first sample). */
	float GetEscapeOpenness(float BearingDegrees) const;
	/** The flyers of the member's own squad (alive, present in the current era), for their separation. */
	void GetFlyerNeighbours(const AGBEnemyCharacter* Enemy, TArray<const AGBEnemyCharacter*>& Out) const;
	/** Melee slot round the player: the point to go to, and whether it may attack now (otherwise it waits at the wait radius). */
	bool GetMeleeSlot(const AGBEnemyCharacter* Enemy, FVector& OutPoint, bool& bOutMayAttack) const;
	/** Orbit slot of a flyer: a point round the player at the radius. */
	bool GetOrbitPoint(const AGBEnemyCharacter* Enemy, float Radius, FVector& OutPoint) const;
	bool HasLiveAnchor(const AGBEnemyCharacter* Enemy) const;

	// ---- Smoke ----
	/** When true the squad logs a TACTICS line per squad every few seconds (smoke runs). */
	static bool bLogTactics;
	static float BearingDegrees2D(const FVector& From, const FVector& To);
	/** One TACTICS line per squad (roles, states, tokens, positions); logs when the interval passed. */
	void LogTactics(const TCHAR* Prefix);
	int32 GetMaxConcurrentShooters() const { return MaxShootersSeen; }
	int32 GetShootersNow() const;
	void ResetSmokeCounters() { MaxShootersSeen = 0; PincerCount = 0; }
	/** Two melee members that committed within the pincer window from at least 90 degrees apart round the player. */
	int32 GetPincerCount() const { return PincerCount; }
	/** The widest angle (degrees, round the player) seen between two such commits. */
	float GetBestPincerAngle() const { return BestPincerAngle; }

private:
	struct FMember
	{
		TWeakObjectPtr<AGBEnemyCharacter> Enemy;
		EGBSquadRole Role = EGBSquadRole::None;
		bool bRoleChanged = false;
		bool bHasToken = false;
		float TokenWeight = 1.f;
		float WaitingSince = -1.f;
		float LastBurstEnd = -1000.f;
		bool bWantsToken = false;
		bool bMeleeCommitted = false;
		float MeleeLeaseStart = 0.f;
		float LastAsk = -1000.f;
		float TokenGrantTime = 0.f;
		bool bRequest = false;
		bool bHasAssigned = false;
		FGBTacticalPosition Assigned;
		FVector Reserved = FVector::ZeroVector;
		bool bHasReserved = false;
		// Pincer
		float PincerWaitStart = 0.f;
		bool bPincerWaiting = false;
		float LastCommitTime = -1000.f;
		float CommitBearing = 0.f;
	};
	struct FSquad
	{
		TArray<FMember> Members;
		FVector LastKnown = FVector::ZeroVector;
		float LastSeenTime = -1.0e9f;
		/** When LastKnown was last set: by a sighting (also sets LastSeenTime) or by a stimulus (a shot heard, a hit taken). */
		float LastKnownTime = -1.0e9f;
		float LastAlertTime = -1.0e9f;
		bool bEngaged = false;
		float NextRoleTime = 0.f;
		int32 RoleSignature = 0;
		int32 NextQueryIndex = 0;
	};

	FSquad* FindSquadOf(const AGBEnemyCharacter* Enemy);
	const FSquad* FindSquadOf(const AGBEnemyCharacter* Enemy) const;
	FMember* FindMember(const AGBEnemyCharacter* Enemy);
	const FMember* FindMember(const AGBEnemyCharacter* Enemy) const;
	void AssignRoles(FSquad& Squad);
	void ServeTokens(FSquad& Squad);
	void AnswerOneRequest(FSquad& Squad);
	bool FindPosition(FSquad& Squad, FMember& Member, FGBTacticalPosition& Out);
	float Now() const;

	/** Samples how far the player could run in 12 directions (every squad tick). */
	void UpdateEscape();
	/**
	 * The pincer squad's order of attack, a round robin: first the members swinging now (nearest first), then those at the
	 * waiting ring or closer, the one whose last attack is OLDEST first (never attacked = oldest; ties: nearest), then the ones
	 * still far out (nearest first). A member keeps its turn while it walks in, so a slow orc is not pushed back by the barghests
	 * the moment they have rested (a distance penalty that ran out did exactly that: the Champion never got a turn).
	 */
	void SortPincerAttackers(const FSquad& Squad, TArray<const AGBEnemyCharacter*>& InOut, const FVector& PlayerPos) const;

	TMap<TWeakObjectPtr<AGBEncounter>, FSquad> Squads;
	float TickAccumulator = 0.f;
	float LogAccumulator = 0.f;
	int32 MaxShootersSeen = 0;
	int32 PincerCount = 0;
	float BestPincerAngle = 0.f;
	TArray<float> EscapeOpen;
};
