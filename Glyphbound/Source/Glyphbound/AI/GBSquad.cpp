#include "AI/GBSquad.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "NavigationSystem.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/PlayerController.h"

#include "AI/GBEncounter.h"
#include "AI/GBEnemyCharacter.h"
#include "Core/GBTypes.h"
#include "Data/GBDataSubsystem.h"
#include "Player/GBPlayerCharacter.h"
#include "Stats/GBStatsComponent.h"
#include "World/GBEraComponent.h"

namespace GBSquadLocal
{
	constexpr float TickInterval = 0.25f;          // s between squad ticks
	constexpr float RoleInterval = 10.f;           // s between role reassignments when nobody joined, died or woke (no role churn)
	constexpr float TokenLease = 8.f;              // s a token may be held before the squad takes it back
	constexpr float TokenAskLifetime = 1.f;        // s a waiting request stays valid without being asked again
	constexpr float LogInterval = 4.f;
	constexpr float NavExtentXY = 150.f;
	constexpr float NavExtentZ = 260.f;
	constexpr float ViewConeCos = 0.55f;           // the player's forward vs the direction to a point: inside this is "in his view"
	constexpr float CrossFireDistance = 250.f;     // a path passing this close to the player crosses his line of fire
	constexpr float OrbitDegreesPerSecond = 8.f;
	constexpr int32 PathCheckedCandidates = 3;
	constexpr int32 ArcSteps = 4;                  // candidates each side of the arc centre
	constexpr float ArcStepDegrees = 28.f;

	float Game(const UObject* Ctx, const TCHAR* Key, float Default)
	{
		const UGBDataSubsystem* Data = UGBDataSubsystem::Get(Ctx);
		return Data ? Data->GetGameValue(FName(Key), Default) : Default;
	}

	float BearingDegrees(const FVector& From, const FVector& To)
	{
		return FMath::RadiansToDegrees(FMath::Atan2(To.Y - From.Y, To.X - From.X));
	}

	float AngleDelta(float A, float B)
	{
		return FMath::Abs(FRotator::NormalizeAxis(A - B));
	}

	bool bLogEnabled = false;
}

bool UGBSquadSubsystem::bLogTactics = false;

UGBSquadSubsystem* UGBSquadSubsystem::Get(const UObject* WorldContext)
{
	if (!GEngine || !WorldContext)
	{
		return nullptr;
	}
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull);
	return World ? World->GetSubsystem<UGBSquadSubsystem>() : nullptr;
}

float UGBSquadSubsystem::Now() const
{
	return GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
}

void UGBSquadSubsystem::ReportNoise(const UObject* WorldContext, const FVector& Location)
{
	if (UGBSquadSubsystem* Squad = Get(WorldContext))
	{
		for (TPair<TWeakObjectPtr<AGBEncounter>, FSquad>& Pair : Squad->Squads)
		{
			for (const FMember& Member : Pair.Value.Members)
			{
				if (AGBEnemyCharacter* Enemy = Member.Enemy.Get())
				{
					Enemy->HearNoise(Location);
				}
			}
		}
	}
}

UGBSquadSubsystem::FSquad* UGBSquadSubsystem::FindSquadOf(const AGBEnemyCharacter* Enemy)
{
	return Enemy ? Squads.Find(TWeakObjectPtr<AGBEncounter>(Enemy->GetEncounter())) : nullptr;
}

const UGBSquadSubsystem::FSquad* UGBSquadSubsystem::FindSquadOf(const AGBEnemyCharacter* Enemy) const
{
	return Enemy ? Squads.Find(TWeakObjectPtr<AGBEncounter>(Enemy->GetEncounter())) : nullptr;
}

UGBSquadSubsystem::FMember* UGBSquadSubsystem::FindMember(const AGBEnemyCharacter* Enemy)
{
	if (FSquad* Squad = FindSquadOf(Enemy))
	{
		for (FMember& Member : Squad->Members)
		{
			if (Member.Enemy.Get() == Enemy)
			{
				return &Member;
			}
		}
	}
	return nullptr;
}

const UGBSquadSubsystem::FMember* UGBSquadSubsystem::FindMember(const AGBEnemyCharacter* Enemy) const
{
	return const_cast<UGBSquadSubsystem*>(this)->FindMember(Enemy);
}

void UGBSquadSubsystem::Join(AGBEnemyCharacter* Enemy)
{
	if (!Enemy)
	{
		return;
	}
	FSquad& Squad = Squads.FindOrAdd(TWeakObjectPtr<AGBEncounter>(Enemy->GetEncounter()));
	for (const FMember& Member : Squad.Members)
	{
		if (Member.Enemy.Get() == Enemy)
		{
			return;
		}
	}
	FMember Member;
	Member.Enemy = Enemy;
	Squad.Members.Add(Member);
	Squad.NextRoleTime = 0.f;
}

void UGBSquadSubsystem::Leave(AGBEnemyCharacter* Enemy)
{
	if (FSquad* Squad = FindSquadOf(Enemy))
	{
		Squad->Members.RemoveAll([Enemy](const FMember& Member) { return Member.Enemy.Get() == Enemy || !Member.Enemy.IsValid(); });
		Squad->NextRoleTime = 0.f;
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Knowledge
// ---------------------------------------------------------------------------------------------------------------------

void UGBSquadSubsystem::ReportSighting(const AGBEnemyCharacter* Enemy, const FVector& PlayerLocation)
{
	if (FSquad* Squad = FindSquadOf(Enemy))
	{
		Squad->LastKnown = PlayerLocation;
		Squad->LastSeenTime = Now();
		Squad->LastKnownTime = Now();
		Squad->bEngaged = true;
	}
}

bool UGBSquadSubsystem::GetLastKnown(const AGBEnemyCharacter* Enemy, FVector& OutLocation, float& OutAge) const
{
	const FSquad* Squad = FindSquadOf(Enemy);
	if (!Squad || Squad->LastKnownTime < -1.0e8f)
	{
		return false;
	}
	OutLocation = Squad->LastKnown;
	OutAge = Now() - Squad->LastKnownTime;
	return true;
}

float UGBSquadSubsystem::SecondsSinceAlert(const AGBEnemyCharacter* Enemy) const
{
	const FSquad* Squad = FindSquadOf(Enemy);
	return Squad ? Now() - Squad->LastAlertTime : 1.0e9f;
}

void UGBSquadSubsystem::AlertSquad(AGBEnemyCharacter* Source, const FVector& StimulusLocation, bool bConfirmedSighting)
{
	FSquad* Squad = FindSquadOf(Source);
	if (!Squad)
	{
		return;
	}
	Squad->LastAlertTime = Now();
	Squad->bEngaged = true;
	// The squad learns where the stimulus was (a shot heard, a hit taken); only a sighting is a confirmed position and time.
	Squad->LastKnown = StimulusLocation;
	Squad->LastKnownTime = Now();
	if (bConfirmedSighting)
	{
		Squad->LastSeenTime = Now();
	}
	const float DelayMin = GBSquadLocal::Game(this, TEXT("SquadAlertDelayMin"), 0.2f);
	const float DelayMax = FMath::Max(GBSquadLocal::Game(this, TEXT("SquadAlertDelayMax"), 1.0f), DelayMin);
	for (const FMember& Member : Squad->Members)
	{
		if (AGBEnemyCharacter* Enemy = Member.Enemy.Get())
		{
			if (Enemy != Source && !Enemy->IsAware())
			{
				Enemy->ScheduleAlert(FMath::FRandRange(DelayMin, DelayMax));
			}
		}
	}
	if (AGBEncounter* Encounter = Source->GetEncounter())
	{
		Encounter->NotifyEnemyAware(Source);
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Tokens
// ---------------------------------------------------------------------------------------------------------------------

bool UGBSquadSubsystem::TryAcquireToken(AGBEnemyCharacter* Enemy, float Weight)
{
	FSquad* Squad = FindSquadOf(Enemy);
	FMember* Member = FindMember(Enemy);
	if (!Squad || !Member)
	{
		return true;   // not in a squad: no limit
	}
	if (Member->bHasToken)
	{
		return true;
	}
	if (!Member->bWantsToken)
	{
		Member->bWantsToken = true;
		Member->WaitingSince = Now();
	}
	Member->TokenWeight = Weight;
	Member->LastAsk = Now();
	ServeTokens(*Squad);
	return Member->bHasToken;
}

void UGBSquadSubsystem::ReleaseToken(AGBEnemyCharacter* Enemy)
{
	if (FMember* Member = FindMember(Enemy))
	{
		if (Member->bHasToken)
		{
			Member->LastBurstEnd = Now();
		}
		Member->bHasToken = false;
		Member->bWantsToken = false;
	}
}

bool UGBSquadSubsystem::HoldsToken(const AGBEnemyCharacter* Enemy) const
{
	const FMember* Member = FindMember(Enemy);
	return !Member || Member->bHasToken;
}

void UGBSquadSubsystem::ServeTokens(FSquad& Squad)
{
	const float Time = Now();
	float Held = 0.f;
	int32 Total = 0;
	for (FMember& Member : Squad.Members)
	{
		if (!Member.Enemy.IsValid())
		{
			continue;
		}
		++Total;
		if (Member.bHasToken && Time - Member.TokenGrantTime > GBSquadLocal::TokenLease)
		{
			Member.bHasToken = false;
			Member.bWantsToken = false;
			Member.LastBurstEnd = Time;
		}
		if (Member.bHasToken)
		{
			Held += Member.TokenWeight;
		}
	}
	TArray<GBTactics::FTokenRequest> Waiting;
	for (int32 Index = 0; Index < Squad.Members.Num(); ++Index)
	{
		FMember& Member = Squad.Members[Index];
		if (Member.bWantsToken && !Member.bHasToken && Member.Enemy.IsValid())
		{
			if (Time - Member.LastAsk > GBSquadLocal::TokenAskLifetime)
			{
				Member.bWantsToken = false;
				continue;
			}
			GBTactics::FTokenRequest Request;
			Request.Id = Index;
			Request.WaitingSince = Member.WaitingSince;
			Request.LastBurstEnd = Member.LastBurstEnd;
			Request.Weight = Member.TokenWeight;
			Waiting.Add(Request);
		}
	}
	if (Waiting.Num() == 0)
	{
		return;
	}
	const TArray<int32> Granted = GBTactics::GrantTokens(Waiting, Held, GBSquadLocal::Game(this, TEXT("SquadMaxShooters"), 2.f), Total,
		FMath::RoundToInt(GBSquadLocal::Game(this, TEXT("SquadBigSize"), 5.f)), Time, GBSquadLocal::Game(this, TEXT("SquadTokenMinRest"), 1.5f));
	for (const int32 Id : Granted)
	{
		FMember& Member = Squad.Members[Id];
		Member.bHasToken = true;
		Member.bWantsToken = false;
		Member.TokenGrantTime = Time;
	}
}

int32 UGBSquadSubsystem::GetShootersNow() const
{
	int32 Count = 0;
	for (const TPair<TWeakObjectPtr<AGBEncounter>, FSquad>& Pair : Squads)
	{
		for (const FMember& Member : Pair.Value.Members)
		{
			Count += (Member.bHasToken && Member.Enemy.IsValid()) ? 1 : 0;
		}
	}
	return Count;
}

// ---------------------------------------------------------------------------------------------------------------------
// Roles
// ---------------------------------------------------------------------------------------------------------------------

EGBSquadRole UGBSquadSubsystem::GetRole(const AGBEnemyCharacter* Enemy) const
{
	const FMember* Member = FindMember(Enemy);
	return Member ? Member->Role : EGBSquadRole::None;
}

bool UGBSquadSubsystem::HasLiveAnchor(const AGBEnemyCharacter* Enemy) const
{
	const FSquad* Squad = FindSquadOf(Enemy);
	if (Squad)
	{
		for (const FMember& Member : Squad->Members)
		{
			const AGBEnemyCharacter* Other = Member.Enemy.Get();
			if (Other && !Other->IsDead() && Other->IsSquadAnchor())
			{
				return true;
			}
		}
	}
	return false;
}

void UGBSquadSubsystem::AssignRoles(FSquad& Squad)
{
	const AGBPlayerCharacter* Player = AGBPlayerCharacter::Get(this);
	if (!Player)
	{
		return;
	}
	TArray<int32> Indices;
	TArray<GBTactics::FRoleInput> Inputs;
	for (int32 Index = 0; Index < Squad.Members.Num(); ++Index)
	{
		const AGBEnemyCharacter* Enemy = Squad.Members[Index].Enemy.Get();
		if (Enemy && !Enemy->IsDead() && Enemy->IsAware() && Enemy->GetSquadClass() == EGBSquadClass::RangedGround)
		{
			GBTactics::FRoleInput Input;
			Input.DistanceToPlayer = FVector::Dist2D(Enemy->GetActorLocation(), Player->GetActorLocation());
			Input.Aggression = FMath::Clamp(Enemy->GetAggression(), 0.f, 1.f);
			Input.bAnchor = Enemy->IsSquadAnchor();
			Indices.Add(Index);
			Inputs.Add(Input);
		}
	}
	const TArray<EGBSquadRole> Roles = GBTactics::AssignRoles(Inputs);
	for (int32 Slot = 0; Slot < Indices.Num(); ++Slot)
	{
		FMember& Member = Squad.Members[Indices[Slot]];
		if (Member.Role != Roles[Slot])
		{
			Member.bRoleChanged = Member.Role != EGBSquadRole::None;
			Member.Role = Roles[Slot];
		}
	}
}

bool UGBSquadSubsystem::TakeRoleChanged(AGBEnemyCharacter* Enemy)
{
	if (FMember* Member = FindMember(Enemy))
	{
		const bool bChanged = Member->bRoleChanged;
		Member->bRoleChanged = false;
		return bChanged;
	}
	return false;
}

// ---------------------------------------------------------------------------------------------------------------------
// Positions
// ---------------------------------------------------------------------------------------------------------------------

void UGBSquadSubsystem::RequestPosition(AGBEnemyCharacter* Enemy)
{
	if (FMember* Member = FindMember(Enemy))
	{
		Member->bRequest = true;
		Member->bHasAssigned = false;
		Member->bHasReserved = false;
	}
}

bool UGBSquadSubsystem::HasPendingRequest(const AGBEnemyCharacter* Enemy) const
{
	const FMember* Member = FindMember(Enemy);
	return Member && Member->bRequest;
}

bool UGBSquadSubsystem::TakeAssignedPosition(AGBEnemyCharacter* Enemy, FGBTacticalPosition& Out)
{
	FMember* Member = FindMember(Enemy);
	if (!Member || !Member->bHasAssigned)
	{
		return false;
	}
	Member->bHasAssigned = false;
	Out = Member->Assigned;
	return true;
}

void UGBSquadSubsystem::AnswerOneRequest(FSquad& Squad)
{
	const int32 Count = Squad.Members.Num();
	for (int32 Offset = 0; Offset < Count; ++Offset)
	{
		const int32 Index = (Squad.NextQueryIndex + Offset) % Count;
		FMember& Member = Squad.Members[Index];
		if (!Member.bRequest || !Member.Enemy.IsValid())
		{
			continue;
		}
		Squad.NextQueryIndex = Index + 1;
		FGBTacticalPosition Position;
		FindPosition(Squad, Member, Position);
		Member.bRequest = false;
		Member.bHasAssigned = true;
		Member.Assigned = Position;
		Member.bHasReserved = Position.bValid;
		Member.Reserved = Position.Cover;
		return;
	}
}

bool UGBSquadSubsystem::FindPosition(FSquad& Squad, FMember& Member, FGBTacticalPosition& Out)
{
	using namespace GBSquadLocal;
	Out = FGBTacticalPosition();
	AGBEnemyCharacter* Enemy = Member.Enemy.Get();
	UWorld* World = GetWorld();
	const AGBPlayerCharacter* Player = AGBPlayerCharacter::Get(this);
	const UNavigationSystemV1* Nav = UNavigationSystemV1::GetCurrent(World);
	if (!Enemy || !World || !Player)
	{
		return false;
	}

	// Only what the squad knows (a sighting sets it to the current position, a noise or a hit to where it came from).
	const FVector PlayerPos = Squad.LastKnownTime > -1.0e8f ? Squad.LastKnown : Player->GetActorLocation();
	const FVector PlayerEye = PlayerPos + FVector(0.f, 0.f, Player->GetAimOrigin().Z - Player->GetActorLocation().Z);
	const FVector Me = Enemy->GetActorLocation();
	const float HalfHeight = Enemy->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const float Spacing = Game(this, TEXT("SquadSpacing"), 350.f);
	const float PeekDistance = Game(this, TEXT("SquadPeekDistance"), 120.f);

	float RangeMin = Enemy->StatValue("PreferredRangeMin", 600.f);
	float RangeMax = FMath::Max(Enemy->StatValue("PreferredRangeMax", 1400.f), RangeMin + 100.f);
	const float Aggression = FMath::Clamp(Enemy->GetAggression(), 0.f, 1.f);
	if (Member.Role == EGBSquadRole::Assault)
	{
		RangeMax = RangeMin + 0.35f * (RangeMax - RangeMin);
	}
	else if (Member.Role == EGBSquadRole::FlankLeft || Member.Role == EGBSquadRole::FlankRight)
	{
		RangeMax -= Aggression * 0.5f * (RangeMax - RangeMin);
	}

	// What the others occupy and who is the base of fire.
	TArray<FVector> Occupied;
	TArray<float> OtherBearings;
	float BaseBearing = BearingDegrees(PlayerPos, Me);
	bool bHasBase = false;
	for (const FMember& Other : Squad.Members)
	{
		const AGBEnemyCharacter* OtherEnemy = Other.Enemy.Get();
		if (!OtherEnemy || &Other == &Member || OtherEnemy->IsDead())
		{
			continue;
		}
		if (OtherEnemy->GetSquadClass() != EGBSquadClass::RangedGround)
		{
			continue;
		}
		Occupied.Add(OtherEnemy->GetActorLocation());
		OtherBearings.Add(BearingDegrees(PlayerPos, OtherEnemy->GetActorLocation()));
		if (Other.bHasReserved)
		{
			Occupied.Add(Other.Reserved);
			OtherBearings.Add(BearingDegrees(PlayerPos, Other.Reserved));
		}
		if (!bHasBase && (Other.Role == EGBSquadRole::Suppress || Other.Role == EGBSquadRole::Anchor))
		{
			bHasBase = true;
			BaseBearing = BearingDegrees(PlayerPos, OtherEnemy->GetActorLocation());
		}
	}

	// Candidates: arcs round the player at three radii (centred on the member's own side, or on the flank it wants), and points round the member.
	float ArcCentre = BearingDegrees(PlayerPos, Me);
	if (Member.Role == EGBSquadRole::FlankLeft)
	{
		ArcCentre = BaseBearing + 90.f;
	}
	else if (Member.Role == EGBSquadRole::FlankRight)
	{
		ArcCentre = BaseBearing - 90.f;
	}
	TArray<FVector> Raw;
	const float Radii[3] = { RangeMin * 1.05f, 0.5f * (RangeMin + RangeMax), RangeMax * 0.95f };
	for (const float Radius : Radii)
	{
		for (int32 Step = -ArcSteps; Step <= ArcSteps; ++Step)
		{
			const float Angle = FMath::DegreesToRadians(ArcCentre + Step * ArcStepDegrees);
			Raw.Add(PlayerPos + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * Radius);
		}
	}
	for (int32 Step = 0; Step < 8; ++Step)
	{
		const float Angle = FMath::DegreesToRadians(Step * 45.f);
		Raw.Add(Me + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * (Step % 2 == 0 ? 350.f : 600.f));
	}
	Raw.Add(Me);

	struct FCandidate
	{
		FVector Point;
		FVector FiringSpot;
		GBTactics::FPositionInput Input;
		float Score = 0.f;
	};
	TArray<FCandidate> Candidates;

	GBTactics::FPositionWeights Weights;
	Weights.CoverWithFiringSpot = Game(this, TEXT("TacCoverFiringSpot"), Weights.CoverWithFiringSpot);
	Weights.OpenWithLineOfFire = Game(this, TEXT("TacOpenLineOfFire"), Weights.OpenWithLineOfFire);
	Weights.CoverWithoutFiringSpot = Game(this, TEXT("TacCoverNoFiringSpot"), Weights.CoverWithoutFiringSpot);
	Weights.InBand = Game(this, TEXT("TacInBand"), Weights.InBand);
	Weights.Spread = Game(this, TEXT("TacSpread"), Weights.Spread);
	Weights.Flank = Game(this, TEXT("TacFlank"), Weights.Flank);
	Weights.Path = Game(this, TEXT("TacPath"), Weights.Path);
	Weights.CrossFire = Game(this, TEXT("TacCrossFire"), Weights.CrossFire);
	Weights.ViewCone = Game(this, TEXT("TacViewCone"), Weights.ViewCone);
	// Cornering is per enemy (the castle's), off for everyone else.
	Weights.Corner = Enemy->StatValue("TacCorner", 0.f);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(GBSquadCover), false, Enemy);
	Params.AddIgnoredActor(Player);
	FCollisionResponseParams IgnorePawns;
	IgnorePawns.CollisionResponse.SetResponse(ECC_Pawn, ECR_Ignore);
	auto Blocked = [&](const FVector& From)
	{
		return World->LineTraceTestByChannel(From, PlayerEye, ECC_Visibility, Params, IgnorePawns);
	};

	const bool bCrouch = Enemy->CanCrouchInCover();
	const float StandEye = Enemy->GetEyeHeightAboveFoot(false);
	const float CrouchEye = Enemy->GetEyeHeightAboveFoot(true);
	const FVector PlayerForward = Player->GetControlRotation().Vector().GetSafeNormal2D();
	const float ZoneInset = 20.f;

	for (const FVector& Point : Raw)
	{
		FVector P = Enemy->ClampToZone(Point);
		if (Nav && Nav->GetDefaultNavDataInstance() != nullptr)
		{
			FNavLocation Projected;
			if (!Nav->ProjectPointToNavigation(P, Projected, FVector(NavExtentXY, NavExtentXY, NavExtentZ)))
			{
				continue;
			}
			P = Projected.Location;
		}
		if (Enemy->HasHomeZone() && !GBTactics::IsInsideBox(Enemy->GetHomeZone().ExpandBy(-ZoneInset), P))
		{
			P = Enemy->ClampToZone(P);
		}
		const float DistanceToPlayer = FVector::Dist2D(P, PlayerPos);
		if (DistanceToPlayer < RangeMin * 0.5f || GBTactics::IsTooClose(P, Occupied, Spacing))
		{
			continue;
		}

		// The same origins real sight uses: the standing eye (chest) and a point a little above it (head).
		const FVector Chest = P + FVector(0.f, 0.f, StandEye);
		const FVector Head = P + FVector(0.f, 0.f, StandEye + HalfHeight * 0.3f);
		const bool bChestBlocked = Blocked(Chest);
		const bool bHeadBlocked = Blocked(Head);

		FCandidate Candidate;
		Candidate.Point = P;
		Candidate.FiringSpot = P;
		GBTactics::FPositionInput& In = Candidate.Input;
		In.DistanceToPlayer = DistanceToPlayer;
		In.RangeMin = RangeMin;
		In.RangeMax = RangeMax;
		In.Role = Member.Role == EGBSquadRole::None ? EGBSquadRole::Suppress : Member.Role;

		if (bCrouch && !bChestBlocked && Blocked(P + FVector(0.f, 0.f, CrouchEye)))
		{
			// Low cover: crouched behind it, standing up to shoot from the same point.
			In.bCovered = true;
			In.bFiringSpot = true;
		}
		else if (bChestBlocked && bHeadBlocked)
		{
			In.bCovered = true;
			const FVector ToPlayer = (PlayerPos - P).GetSafeNormal2D();
			const FVector Side = FVector::CrossProduct(FVector::UpVector, ToPlayer);
			float BestSpotDistance = 1.0e9f;
			for (const float Sign : { -1.f, 1.f })
			{
				FVector Spot = P + Side * Sign * PeekDistance;
				if (Nav && Nav->GetDefaultNavDataInstance() != nullptr)
				{
					FNavLocation Projected;
					if (!Nav->ProjectPointToNavigation(Spot, Projected, FVector(60.f, 60.f, 120.f)))
					{
						continue;
					}
					Spot = Projected.Location;
				}
				if (!Enemy->IsInsideZone(Spot))
				{
					continue;
				}
				if (!Blocked(Spot + FVector(0.f, 0.f, StandEye)))
				{
					const float D = FVector::Dist(Spot, Me);
					if (D < BestSpotDistance)
					{
						BestSpotDistance = D;
						Candidate.FiringSpot = Spot;
						In.bFiringSpot = true;
					}
				}
			}
		}
		else
		{
			In.bCovered = false;
			In.bOpenLineOfFire = !bChestBlocked;
		}

		const float Bearing = BearingDegrees(PlayerPos, P);
		In.MinAngleToOthers = 180.f;
		for (const float Other : OtherBearings)
		{
			In.MinAngleToOthers = FMath::Min(In.MinAngleToOthers, AngleDelta(Bearing, Other));
		}
		In.AngleFromBase = bHasBase ? AngleDelta(Bearing, BaseBearing) : 90.f;
		In.bInViewCone = FVector::DotProduct(PlayerForward, (P - PlayerPos).GetSafeNormal2D()) > ViewConeCos;
		// A straight run from where it is passing close to the player crosses his line of fire.
		In.bCrossesFireLine = FMath::PointDistToSegment(PlayerPos, Me, P) < CrossFireDistance && FVector::Dist2D(Me, P) > 300.f;
		In.PathLength = FVector::Dist2D(Me, P);
		In.EscapeOpenness = Weights.Corner > 0.f ? GetEscapeOpenness(Bearing) : 0.f;
		Candidate.Score = GBTactics::ScorePosition(In, Weights);
		Candidates.Add(Candidate);
	}
	if (Candidates.Num() == 0)
	{
		return false;
	}

	// The real path length only for the best few.
	Candidates.Sort([](const FCandidate& A, const FCandidate& B) { return A.Score > B.Score; });
	int32 BestIndex = INDEX_NONE;
	float BestScore = -1.0e9f;
	for (int32 Index = 0; Index < FMath::Min(Candidates.Num(), PathCheckedCandidates); ++Index)
	{
		FCandidate& Candidate = Candidates[Index];
		if (Nav && Nav->GetDefaultNavDataInstance() != nullptr)
		{
			double Length = 0.0;
			if (Nav->GetPathLength(Me, Candidate.Point, Length) != ENavigationQueryResult::Success)
			{
				continue;
			}
			Candidate.Input.PathLength = static_cast<float>(Length);
			Candidate.Score = GBTactics::ScorePosition(Candidate.Input, Weights);
		}
		if (Candidate.Score > BestScore)
		{
			BestScore = Candidate.Score;
			BestIndex = Index;
		}
	}
	if (BestIndex == INDEX_NONE || BestScore < -1.0e5f)
	{
		return false;
	}
	const FCandidate& Best = Candidates[BestIndex];
	Out.Cover = Best.Point;
	Out.FiringSpot = Best.FiringSpot;
	Out.bCovered = Best.Input.bCovered;
	Out.Score = BestScore;
	Out.bValid = true;
	return true;
}

// ---------------------------------------------------------------------------------------------------------------------
// Slots
// ---------------------------------------------------------------------------------------------------------------------

bool UGBSquadSubsystem::TryBeginMeleeAttack(AGBEnemyCharacter* Enemy)
{
	FSquad* Squad = FindSquadOf(Enemy);
	FMember* Self = FindMember(Enemy);
	if (!Squad || !Self)
	{
		return true;
	}
	if (Self->bMeleeCommitted)
	{
		return true;
	}
	const float Time = Now();
	int32 Committed = 0;
	for (FMember& Member : Squad->Members)
	{
		if (Member.bMeleeCommitted && (!Member.Enemy.IsValid() || Time - Member.MeleeLeaseStart > 4.f))
		{
			Member.bMeleeCommitted = false;   // a lease nobody released
		}
		Committed += (Member.bMeleeCommitted && Member.Enemy.IsValid()) ? 1 : 0;
	}
	const int32 Max = FMath::Max(1, FMath::RoundToInt(GBSquadLocal::Game(this, TEXT("SquadMaxMeleeAttackers"), 2.f)));
	if (Committed >= Max)
	{
		return false;
	}
	Self->bMeleeCommitted = true;
	Self->MeleeLeaseStart = Time;
	Self->bPincerWaiting = false;

	// Smoke / log: a pincer is two commits a moment apart from well apart round the player.
	if (const AGBPlayerCharacter* Player = AGBPlayerCharacter::Get(this))
	{
		const float Bearing = GBSquadLocal::BearingDegrees(Player->GetActorLocation(), Enemy->GetActorLocation());
		const float Window = GBSquadLocal::Game(this, TEXT("SquadPincerWindow"), 0.8f);
		for (const FMember& Other : Squad->Members)
		{
			if (&Other == Self || !Other.Enemy.IsValid() || Time - Other.LastCommitTime > Window)
			{
				continue;
			}
			const float Apart = GBSquadLocal::AngleDelta(Bearing, Other.CommitBearing);
			if (Apart >= 90.f)
			{
				++PincerCount;
				BestPincerAngle = FMath::Max(BestPincerAngle, Apart);
				if (bLogTactics)
				{
					UE_LOG(LogGB, Display, TEXT("TACTICS pincer: %s and %s closed in %.2f s apart from %.0f degrees apart"), *Enemy->GetEnemyId().ToString(),
						*Other.Enemy->GetEnemyId().ToString(), Time - Other.LastCommitTime, Apart);
				}
			}
		}
		Self->LastCommitTime = Time;
		Self->CommitBearing = Bearing;
	}
	return true;
}

bool UGBSquadSubsystem::MayCommitMelee(AGBEnemyCharacter* Enemy)
{
	FSquad* Squad = FindSquadOf(Enemy);
	FMember* Self = FindMember(Enemy);
	const AGBPlayerCharacter* Player = AGBPlayerCharacter::Get(this);
	const float MaxWait = Enemy ? Enemy->StatValue("PincerWait", 0.f) : 0.f;
	if (!Squad || !Self || !Player || MaxWait <= 0.f)
	{
		return true;
	}
	const float Time = Now();
	const FVector PlayerPos = Player->GetActorLocation();
	const float Window = GBSquadLocal::Game(this, TEXT("SquadPincerWindow"), 0.8f);
	const float ReachMult = GBSquadLocal::Game(this, TEXT("SquadPincerReach"), 1.3f);

	// The squad's attackers: its nearest pincer melee members (as many as may attack at once).
	TArray<const AGBEnemyCharacter*> Melee;
	for (const FMember& Member : Squad->Members)
	{
		const AGBEnemyCharacter* Other = Member.Enemy.Get();
		if (Other && !Other->IsDead() && Other->IsReadyToFight() && Other->GetSquadClass() == EGBSquadClass::Melee && Other->StatValue("PincerWait", 0.f) > 0.f
			&& Other->GetEraComponent()->IsPresent())
		{
			if (Other != Enemy && Time - Member.LastCommitTime < Window)
			{
				Self->bPincerWaiting = false;
				return true;   // a partner just went in: this one is the other jaw
			}
			Melee.Add(Other);
		}
	}
	SortPincerAttackers(*Squad, Melee, PlayerPos);
	const int32 MaxAttackers = FMath::Max(1, FMath::RoundToInt(GBSquadLocal::Game(this, TEXT("SquadMaxMeleeAttackers"), 2.f)));
	const int32 Attackers = FMath::Min(MaxAttackers, Melee.Num());
	const int32 SelfIndex = Melee.IndexOfByKey(Enemy);
	if (Attackers < 2 || SelfIndex == INDEX_NONE)
	{
		Self->bPincerWaiting = false;
		return true;   // alone: nothing to wait for
	}
	if (SelfIndex >= Attackers)
	{
		Self->bPincerWaiting = false;   // it lost its attack place: a later turn waits afresh
		return false;  // not its turn (or it just attacked): it waits at the wait radius
	}
	bool bAllInPlace = true;
	for (int32 Index = 0; Index < Attackers; ++Index)
	{
		const AGBEnemyCharacter* Other = Melee[Index];
		if (Other == Enemy)
		{
			continue;
		}
		const float Reach = FMath::Max(Other->StatValue("AttackRange", 150.f), Other->StatValue("LungeRange", 0.f)) * ReachMult;
		bAllInPlace &= FVector::Dist2D(Other->GetActorLocation(), PlayerPos) <= Reach;
	}
	// The wait starts once, at the first ask of this attempt, and holds across gaps between asks (long frames, a partner that
	// never arrives); it ends when it commits (TryBeginMeleeAttack), gives up here, or loses its attack place.
	if (!Self->bPincerWaiting)
	{
		Self->bPincerWaiting = true;
		Self->PincerWaitStart = Time;
	}
	if (bAllInPlace || Time - Self->PincerWaitStart >= MaxWait)
	{
		Self->bPincerWaiting = false;
		return true;
	}
	return false;
}

void UGBSquadSubsystem::SortPincerAttackers(const FSquad& Squad, TArray<const AGBEnemyCharacter*>& InOut, const FVector& PlayerPos) const
{
	struct FKey
	{
		int32 Group = 2;
		float LastCommit = 0.f;
		float Distance = 0.f;
	};
	const float RingRadius = GBSquadLocal::Game(this, TEXT("MeleeWaitRadius"), 450.f);
	TMap<const AGBEnemyCharacter*, FKey> Keys;
	for (const AGBEnemyCharacter* Enemy : InOut)
	{
		FKey& Key = Keys.Add(Enemy);
		Key.Distance = FVector::Dist2D(Enemy->GetActorLocation(), PlayerPos);
		const float Ring = FMath::Max(Enemy->StatValue("MeleeWaitRadius", RingRadius), Enemy->StatValue("AttackRange", 150.f) * 1.5f);
		for (const FMember& Member : Squad.Members)
		{
			if (Member.Enemy.Get() == Enemy)
			{
				Key.LastCommit = Member.LastCommitTime;
				Key.Group = Member.bMeleeCommitted ? 0 : (Key.Distance <= Ring * 1.5f ? 1 : 2);
				break;
			}
		}
	}
	InOut.Sort([&Keys](const AGBEnemyCharacter& A, const AGBEnemyCharacter& B)
	{
		const FKey& KA = Keys.FindChecked(&A);
		const FKey& KB = Keys.FindChecked(&B);
		if (KA.Group != KB.Group)
		{
			return KA.Group < KB.Group;
		}
		if (KA.Group == 1 && !FMath::IsNearlyEqual(KA.LastCommit, KB.LastCommit))
		{
			return KA.LastCommit < KB.LastCommit;
		}
		return KA.Distance < KB.Distance;
	});
}

float UGBSquadSubsystem::GetEscapeOpenness(float BearingDegrees) const
{
	const int32 Count = EscapeOpen.Num();
	if (Count == 0)
	{
		return 1.f;
	}
	float Slot = FRotator::ClampAxis(BearingDegrees) / (360.f / Count);
	const int32 A = FMath::FloorToInt(Slot) % Count;
	const int32 B = (A + 1) % Count;
	const float Alpha = Slot - FMath::FloorToFloat(Slot);
	return FMath::Lerp(EscapeOpen[A], EscapeOpen[B], Alpha);
}

void UGBSquadSubsystem::UpdateEscape()
{
	UWorld* World = GetWorld();
	const AGBPlayerCharacter* Player = AGBPlayerCharacter::Get(this);
	if (!World || !Player)
	{
		return;
	}
	constexpr int32 Directions = 12;
	const float Probe = FMath::Max(GBSquadLocal::Game(this, TEXT("SquadCornerProbe"), 700.f), 50.f);
	EscapeOpen.SetNum(Directions);
	const FVector From = Player->GetActorLocation();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GBSquadEscape), false, Player);
	FCollisionResponseParams IgnorePawns;
	IgnorePawns.CollisionResponse.SetResponse(ECC_Pawn, ECR_Ignore);
	for (int32 Index = 0; Index < Directions; ++Index)
	{
		const float Angle = FMath::DegreesToRadians(Index * 360.f / Directions);
		const FVector To = From + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * Probe;
		FHitResult Hit;
		EscapeOpen[Index] = World->LineTraceSingleByChannel(Hit, From, To, ECC_Visibility, Params, IgnorePawns) ? FMath::Clamp(Hit.Distance / Probe, 0.f, 1.f) : 1.f;
	}
}

void UGBSquadSubsystem::EndMeleeAttack(AGBEnemyCharacter* Enemy)
{
	if (FMember* Member = FindMember(Enemy))
	{
		Member->bMeleeCommitted = false;
	}
}

void UGBSquadSubsystem::GetFlyerNeighbours(const AGBEnemyCharacter* Enemy, TArray<const AGBEnemyCharacter*>& Out) const
{
	Out.Reset();
	const FSquad* Squad = FindSquadOf(Enemy);
	if (!Squad || !Enemy || !Enemy->GetEraComponent()->IsPresent())
	{
		return;   // a flyer of the other era has no neighbours (era filter before the loop)
	}
	for (const FMember& Member : Squad->Members)
	{
		const AGBEnemyCharacter* Other = Member.Enemy.Get();
		if (Other && Other != Enemy && !Other->IsDead() && Other->GetSquadClass() == EGBSquadClass::Flyer && Other->GetEraComponent()->IsPresent())
		{
			Out.Add(Other);
		}
	}
}

float UGBSquadSubsystem::NearestRangedMateDistance(const AGBEnemyCharacter* Enemy) const
{
	float Best = 1.0e9f;
	const FSquad* Squad = FindSquadOf(Enemy);
	if (!Squad || !Enemy)
	{
		return Best;
	}
	for (const FMember& Member : Squad->Members)
	{
		const AGBEnemyCharacter* Other = Member.Enemy.Get();
		if (Other && Other != Enemy && !Other->IsDead() && Other->GetSquadClass() == EGBSquadClass::RangedGround && Other->GetEraComponent()->IsPresent())
		{
			Best = FMath::Min(Best, float(FVector::Dist2D(Other->GetActorLocation(), Enemy->GetActorLocation())));
		}
	}
	return Best;
}

bool UGBSquadSubsystem::GetMeleeSlot(const AGBEnemyCharacter* Enemy, FVector& OutPoint, bool& bOutMayAttack) const
{
	const FSquad* Squad = FindSquadOf(Enemy);
	const AGBPlayerCharacter* Player = AGBPlayerCharacter::Get(this);
	if (!Squad || !Player)
	{
		return false;
	}
	const FVector PlayerPos = Player->GetActorLocation();
	TArray<const AGBEnemyCharacter*> Melee;
	for (const FMember& Member : Squad->Members)
	{
		const AGBEnemyCharacter* Other = Member.Enemy.Get();
		if (Other && !Other->IsDead() && Other->IsReadyToFight() && Other->GetSquadClass() == EGBSquadClass::Melee)
		{
			Melee.Add(Other);
		}
	}
	if (Melee.Num() < 2 || !Melee.Contains(Enemy))
	{
		return false;
	}
	// Attackers: the nearest ones. Slots: evenly round the player, in the order of their bearings.
	TArray<const AGBEnemyCharacter*> ByDistance = Melee;
	ByDistance.Sort([&PlayerPos](const AGBEnemyCharacter& A, const AGBEnemyCharacter& B) { return FVector::DistSquared2D(A.GetActorLocation(), PlayerPos) < FVector::DistSquared2D(B.GetActorLocation(), PlayerPos); });
	const int32 MaxAttackers = FMath::Max(1, FMath::RoundToInt(GBSquadLocal::Game(this, TEXT("SquadMaxMeleeAttackers"), 2.f)));
	bOutMayAttack = Enemy->AlwaysMeleeAttacks() || ByDistance.IndexOfByKey(Enemy) < MaxAttackers;

	if (Enemy->StatValue("PincerWait", 0.f) > 0.f)
	{
		// The pincer ring (the castle's melee): the attackers spread evenly round the player among themselves (two = opposite
		// sides), the ring turned towards his open sides (CornerWeight) so he is pushed to the walls; the others wait between them.
		// They take turns: one that just attacked yields its place for a moment.
		SortPincerAttackers(*Squad, ByDistance, PlayerPos);
		bOutMayAttack = Enemy->AlwaysMeleeAttacks() || ByDistance.IndexOfByKey(Enemy) < MaxAttackers;
		const int32 NumAttackers = FMath::Min(MaxAttackers, ByDistance.Num());
		const float Base = GBSquadLocal::BearingDegrees(PlayerPos, ByDistance[0]->GetActorLocation());
		float Offset = 0.f;
		const float Corner = Enemy->StatValue("CornerWeight", 0.f);
		if (Corner > 0.f && EscapeOpen.Num() > 0)
		{
			float Best = -1.0e9f;
			for (int32 Step = -3; Step <= 3; ++Step)
			{
				const float Turn = Step * 20.f;
				float Score = -FMath::Abs(Turn) / 60.f;   // a short walk round him beats a long one
				for (int32 K = 0; K < NumAttackers; ++K)
				{
					Score += Corner * GetEscapeOpenness(Base + Turn + K * 360.f / NumAttackers);
				}
				if (Score > Best)
				{
					Best = Score;
					Offset = Turn;
				}
			}
		}
		TArray<const AGBEnemyCharacter*> Group;
		const bool bAttacker = ByDistance.IndexOfByKey(Enemy) < NumAttackers;
		for (int32 Index = bAttacker ? 0 : NumAttackers; Index < (bAttacker ? NumAttackers : ByDistance.Num()); ++Index)
		{
			Group.Add(ByDistance[Index]);
		}
		// Each takes the slot on its own side: sorted by bearing measured from the nearest attacker.
		auto Around = [&PlayerPos, Base](const AGBEnemyCharacter* A) { return FRotator::ClampAxis(GBSquadLocal::BearingDegrees(PlayerPos, A->GetActorLocation()) - Base); };
		Group.Sort([&Around](const AGBEnemyCharacter& A, const AGBEnemyCharacter& B) { return Around(&A) < Around(&B); });
		const int32 Slot = Group.IndexOfByKey(Enemy);
		const float Step = 360.f / FMath::Max(Group.Num(), 1);
		const float AngleDeg = Base + Offset + (bAttacker ? Slot * Step : (Slot + 0.5f) * Step);
		const float AttackRange = Enemy->StatValue("AttackRange", 150.f);
		const float Radius = bOutMayAttack ? AttackRange * 0.85f : FMath::Max(Enemy->StatValue("MeleeWaitRadius", GBSquadLocal::Game(this, TEXT("MeleeWaitRadius"), 450.f)), AttackRange * 1.5f);
		OutPoint = PlayerPos + FVector(FMath::Cos(FMath::DegreesToRadians(AngleDeg)), FMath::Sin(FMath::DegreesToRadians(AngleDeg)), 0.f) * Radius;
		return true;
	}

	Melee.Sort([&PlayerPos](const AGBEnemyCharacter& A, const AGBEnemyCharacter& B) { return GBSquadLocal::BearingDegrees(PlayerPos, A.GetActorLocation()) < GBSquadLocal::BearingDegrees(PlayerPos, B.GetActorLocation()); });
	const int32 Slot = Melee.IndexOfByKey(Enemy);
	const float Start = GBSquadLocal::BearingDegrees(PlayerPos, Melee[0]->GetActorLocation());
	const float Angle = FMath::DegreesToRadians(Start + 360.f * static_cast<float>(Slot) / static_cast<float>(Melee.Num()));
	const float AttackRange = Enemy->StatValue("AttackRange", 150.f);
	const float Radius = bOutMayAttack ? AttackRange * 0.85f : FMath::Max(Enemy->StatValue("MeleeWaitRadius", GBSquadLocal::Game(this, TEXT("MeleeWaitRadius"), 450.f)), AttackRange * 1.5f);
	OutPoint = PlayerPos + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * Radius;
	return true;
}

bool UGBSquadSubsystem::GetOrbitPoint(const AGBEnemyCharacter* Enemy, float Radius, FVector& OutPoint) const
{
	const FSquad* Squad = FindSquadOf(Enemy);
	const AGBPlayerCharacter* Player = AGBPlayerCharacter::Get(this);
	if (!Squad || !Player)
	{
		return false;
	}
	int32 Total = 0;
	int32 Slot = -1;
	for (const FMember& Member : Squad->Members)
	{
		const AGBEnemyCharacter* Other = Member.Enemy.Get();
		if (Other && !Other->IsDead() && Other->GetSquadClass() == EGBSquadClass::Flyer && Other->IsReadyToFight())
		{
			if (Other == Enemy)
			{
				Slot = Total;
			}
			++Total;
		}
	}
	if (Slot < 0 || Total == 0)
	{
		return false;
	}
	const float BaseDegrees = 360.f * static_cast<float>(Slot) / static_cast<float>(Total) + Now() * Enemy->StatValue("OrbitStrength", 0.35f) * GBSquadLocal::OrbitDegreesPerSecond * 4.f;
	// A slot behind a wall is no slot: the nearest angle (alternating sides) with a clear line from the player's eye to it.
	const UWorld* World = GetWorld();
	const FVector Eye = Player->GetAimOrigin();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GBOrbitSlot), false, Enemy);
	Params.AddIgnoredActor(Player);
	FCollisionResponseParams IgnorePawns;
	IgnorePawns.CollisionResponse.SetResponse(ECC_Pawn, ECR_Ignore);
	for (int32 Try = 0; Try < 7; ++Try)
	{
		const float Offset = (Try == 0 ? 0.f : ((Try % 2) ? 1.f : -1.f) * 25.f * ((Try + 1) / 2));
		const float Angle = FMath::DegreesToRadians(BaseDegrees + Offset);
		const FVector Point = FVector(Player->GetActorLocation().X, Player->GetActorLocation().Y, Eye.Z) + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * Radius;
		if (!World || !World->LineTraceTestByChannel(Eye, Point, ECC_Visibility, Params, IgnorePawns))
		{
			OutPoint = Point;
			return true;
		}
	}
	return false;
}

// ---------------------------------------------------------------------------------------------------------------------
// Tick
// ---------------------------------------------------------------------------------------------------------------------

void UGBSquadSubsystem::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	TickAccumulator += DeltaSeconds;
	if (TickAccumulator < GBSquadLocal::TickInterval)
	{
		return;
	}
	TickAccumulator = 0.f;
	const float Time = Now();
	const float LoseTime = GBSquadLocal::Game(this, TEXT("EncounterLoseTargetTime"), 10.f);
	if (Squads.Num() > 0)
	{
		UpdateEscape();
	}

	TArray<TWeakObjectPtr<AGBEncounter>> Empty;
	for (TPair<TWeakObjectPtr<AGBEncounter>, FSquad>& Pair : Squads)
	{
		FSquad& Squad = Pair.Value;
		Squad.Members.RemoveAll([](const FMember& Member) { const AGBEnemyCharacter* Enemy = Member.Enemy.Get(); return !Enemy || Enemy->IsDead(); });
		if (Squad.Members.Num() == 0)
		{
			Empty.Add(Pair.Key);
			continue;
		}

		// Nobody has seen him for a while: back to the posts (only enemies with a zone).
		if (Squad.bEngaged && Pair.Key.IsValid() && Time - FMath::Max(Squad.LastSeenTime, Squad.LastAlertTime) > LoseTime)
		{
			Squad.bEngaged = false;
			for (FMember& Member : Squad.Members)
			{
				Member.bHasToken = false;
				Member.bWantsToken = false;
				Member.bRequest = false;
				Member.Role = EGBSquadRole::None;
				if (AGBEnemyCharacter* Enemy = Member.Enemy.Get())
				{
					Enemy->ReturnToUnaware();
				}
			}
			continue;
		}

		int32 Signature = 0;
		for (const FMember& Member : Squad.Members)
		{
			const AGBEnemyCharacter* Enemy = Member.Enemy.Get();
			if (Enemy && Enemy->IsAware() && Enemy->GetSquadClass() == EGBSquadClass::RangedGround)
			{
				Signature = HashCombine(Signature, GetTypeHash(Enemy->GetUniqueID()));
			}
		}
		if (Signature != Squad.RoleSignature || Time >= Squad.NextRoleTime)
		{
			Squad.RoleSignature = Signature;
			Squad.NextRoleTime = Time + GBSquadLocal::RoleInterval;
			AssignRoles(Squad);
		}
		ServeTokens(Squad);
		AnswerOneRequest(Squad);

		int32 Shooters = 0;
		for (const FMember& Member : Squad.Members)
		{
			Shooters += Member.bHasToken ? 1 : 0;
		}
		MaxShootersSeen = FMath::Max(MaxShootersSeen, GetShootersNow());
		(void)Shooters;
	}
	for (const TWeakObjectPtr<AGBEncounter>& Key : Empty)
	{
		Squads.Remove(Key);
	}

	if (bLogTactics)
	{
		LogAccumulator += GBSquadLocal::TickInterval;
		if (LogAccumulator >= GBSquadLocal::LogInterval)
		{
			LogAccumulator = 0.f;
			LogTactics(TEXT("TACTICS"));
		}
	}
}

void UGBSquadSubsystem::LogTactics(const TCHAR* Prefix)
{
	const AGBPlayerCharacter* Player = AGBPlayerCharacter::Get(this);
	for (const TPair<TWeakObjectPtr<AGBEncounter>, FSquad>& Pair : Squads)
	{
		const FSquad& Squad = Pair.Value;
		FString Line = FString::Printf(TEXT("%s squad=%s members=%d engaged=%d lastSeenAge=%.1f |"), Prefix, Pair.Key.IsValid() ? *Pair.Key->EncounterId.ToString() : TEXT("adhoc"),
			Squad.Members.Num(), Squad.bEngaged ? 1 : 0, Now() - Squad.LastSeenTime);
		for (const FMember& Member : Squad.Members)
		{
			const AGBEnemyCharacter* Enemy = Member.Enemy.Get();
			if (!Enemy)
			{
				continue;
			}
			const float Bearing = Player ? BearingDegrees2D(Player->GetActorLocation(), Enemy->GetActorLocation()) : 0.f;
			Line += FString::Printf(TEXT(" [%s role=%s token=%d aware=%d bearing=%.0f dist=%.0f %s]"), *Enemy->GetEnemyId().ToString(), GBTactics::RoleName(Member.Role), Member.bHasToken ? 1 : 0,
				Enemy->IsAware() ? 1 : 0, Bearing, Player ? FVector::Dist2D(Player->GetActorLocation(), Enemy->GetActorLocation()) : 0.f, *Enemy->DescribeTactics());
		}
		UE_LOG(LogGB, Display, TEXT("%s"), *Line);
	}
}

float UGBSquadSubsystem::BearingDegrees2D(const FVector& From, const FVector& To)
{
	return GBSquadLocal::BearingDegrees(From, To);
}
