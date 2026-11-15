#include "AI/GBEncounter.h"

#include "Components/BoxComponent.h"
#include "Engine/World.h"

#include "AI/GBEnemyCharacter.h"
#include "NavigationSystem.h"
#include "AI/GBTactics.h"
#include "GameFramework/PlayerController.h"
#include "Data/GBDataSubsystem.h"
#include "Player/GBPlayerCharacter.h"
#include "Progression/GBSkillTreeComponent.h"
#include "UI/GBHUD.h"
#include "World/GBEraSubsystem.h"
#include "World/GBPickup.h"
#include "World/GBProgressSubsystem.h"
#include "EngineUtils.h"
#include "NiagaraComponent.h"

// Arena seal (owner, 2026-11-15, the tentacle boss): actors tagged "EncounterBarrier" plus the encounter's id (an invisible
// blocking wall, a smoke system) are off until the fight is on and the player is inside the trigger box; then the wall
// blocks and the smoke runs, so the player cannot leave. Cleared or reset (the player died): off again.
// File-local state, no header change.
namespace GBEncounterBarrier
{
	static TSet<TWeakObjectPtr<const AGBEncounter>> Sealed;

	static void Set(const AGBEncounter* Encounter, bool bOn)
	{
		UWorld* World = Encounter ? Encounter->GetWorld() : nullptr;
		if (!World || Encounter->EncounterId.IsNone())
		{
			return;
		}
		const FName BarrierTag(TEXT("EncounterBarrier"));
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* Actor = *It;
			if (!Actor->ActorHasTag(BarrierTag) || !Actor->ActorHasTag(Encounter->EncounterId))
			{
				continue;
			}
			TInlineComponentArray<UPrimitiveComponent*> Prims(Actor);
			for (UPrimitiveComponent* Prim : Prims)
			{
				if (!Prim->IsA<UNiagaraComponent>())
				{
					Prim->SetCollisionEnabled(bOn ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
				}
			}
			TInlineComponentArray<UNiagaraComponent*> Fx(Actor);
			for (UNiagaraComponent* Smoke : Fx)
			{
				if (bOn)
				{
					Smoke->SetVisibility(true);
					Smoke->Activate(true);
				}
				else
				{
					Smoke->Deactivate();
				}
			}
		}
		if (bOn)
		{
			Sealed.Add(Encounter);
		}
		else
		{
			Sealed.Remove(Encounter);
		}
	}
}

AGBEncounter::AGBEncounter()
{
	PrimaryActorTick.bCanEverTick = true;

	Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
	RootComponent = Trigger;
	Trigger->InitBoxExtent(TriggerExtent);
	Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Trigger->SetCollisionObjectType(ECC_WorldDynamic);
	Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Trigger->SetGenerateOverlapEvents(true);
	Trigger->SetCanEverAffectNavigation(false);
	Trigger->SetHiddenInGame(true);
}

void AGBEncounter::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Trigger->SetBoxExtent(TriggerExtent);
}

void AGBEncounter::BeginPlay()
{
	Super::BeginPlay();
	GBEncounterBarrier::Set(this, false);

	Trigger->SetBoxExtent(TriggerExtent);
	Trigger->OnComponentBeginOverlap.AddUniqueDynamic(this, &AGBEncounter::HandleTriggerOverlap);

	if (UGBEraSubsystem* Era = UGBEraSubsystem::Get(this))
	{
		Era->OnEraChanged.AddUObject(this, &AGBEncounter::HandleEraChanged);
	}

	ApplyClearedFlagIfSet();
	TryPrespawn();
}

void AGBEncounter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UGBEraSubsystem* Era = UGBEraSubsystem::Get(this))
	{
		Era->OnEraChanged.RemoveAll(this);
	}
	Super::EndPlay(EndPlayReason);
}

void AGBEncounter::Configure(FName InEncounterId, EGBEraPresence InPresence, const FVector& InTriggerExtent, TConstArrayView<FVector> InSpawnPoints)
{
	EncounterId = InEncounterId;
	Presence = InPresence;
	TriggerExtent = InTriggerExtent;
	SpawnPoints.Reset();
	SpawnPoints.Append(InSpawnPoints.GetData(), InSpawnPoints.Num());
	bLoggedMissingDefinition = false;

	Trigger->SetBoxExtent(TriggerExtent);
	if (HasActorBegunPlay())
	{
		ApplyClearedFlagIfSet();
	}
}

bool AGBEncounter::AreRequiredFlagsMet(TConstArrayView<FName> Required, const TSet<FName>& SetFlags)
{
	for (const FName Flag : Required)
	{
		if (!Flag.IsNone() && !SetFlags.Contains(Flag))
		{
			return false;
		}
	}
	return true;
}

bool AGBEncounter::AreRequiredFlagsMet() const
{
	if (RequiredFlags.Num() == 0)
	{
		return true;
	}
	const UGBProgressSubsystem* Progress = UGBProgressSubsystem::Get(this);
	return Progress && AreRequiredFlagsMet(RequiredFlags, Progress->GetFlags());
}

const FGBEncounterDefinition* AGBEncounter::FindDefinition() const
{
	const UGBDataSubsystem* Data = UGBDataSubsystem::Get(this);
	return Data ? Data->FindEncounter(EncounterId) : nullptr;
}

int32 AGBEncounter::GetNumWaves() const
{
	const FGBEncounterDefinition* Def = FindDefinition();
	return Def ? Def->Waves.Num() : 0;
}

int32 AGBEncounter::GetAliveCount() const
{
	int32 Count = 0;
	for (const TObjectPtr<AGBEnemyCharacter>& Enemy : AliveEnemies)
	{
		if (IsValid(Enemy) && !Enemy->IsDead())
		{
			++Count;
		}
	}
	return Count;
}

FName AGBEncounter::GetClearedFlag() const
{
	const FGBEncounterDefinition* Def = FindDefinition();
	if (Def && !Def->ClearedFlag.IsNone())
	{
		return Def->ClearedFlag;
	}
	return FName(*FString::Printf(TEXT("Encounter.%s.Cleared"), *EncounterId.ToString()));
}

void AGBEncounter::ApplyClearedFlagIfSet()
{
	if (State == EGBEncounterState::Cleared || EncounterId.IsNone())
	{
		return;
	}
	const UGBProgressSubsystem* Progress = UGBProgressSubsystem::Get(this);
	if (Progress && Progress->HasFlag(GetClearedFlag()))
	{
		ResetEncounter();
		State = EGBEncounterState::Cleared;
		SetActorTickEnabled(false);
	}
}

void AGBEncounter::HandleTriggerOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (OtherActor && OtherActor == AGBPlayerCharacter::Get(this))
	{
		TryStartFromTrigger(true);
	}
}

void AGBEncounter::HandleEraChanged(EGBEra NewEra)
{
	TryStartFromTrigger();
}

void AGBEncounter::TryStartFromTrigger(bool bPlayerKnownInside)
{
	if (State != EGBEncounterState::Waiting || EncounterId.IsNone() || !AreRequiredFlagsMet() || IsPrespawn())
	{
		return;   // a prespawned encounter starts when one of its enemies notices the player, not at its trigger
	}

	AGBPlayerCharacter* Player = AGBPlayerCharacter::Get(this);
	if (!Player || Player->IsDead())
	{
		return;
	}

	const UGBEraSubsystem* Era = UGBEraSubsystem::Get(this);
	const EGBEra CurrentEra = Era ? Era->GetCurrentEra() : EGBEra::Past;
	if (!UGBEraSubsystem::IsPresentIn(Presence, CurrentEra))
	{
		return;
	}

	if (!bPlayerKnownInside && !Trigger->IsOverlappingActor(Player))
	{
		return;
	}

	StartEncounter();
}

void AGBEncounter::StartEncounter()
{
	if (State != EGBEncounterState::Waiting || !AreRequiredFlagsMet())
	{
		return;
	}

	ApplyClearedFlagIfSet();
	if (State == EGBEncounterState::Cleared)
	{
		return;
	}
	if (IsPrespawnedWaiting())
	{
		ActivatePrespawned();
		return;
	}

	const FGBEncounterDefinition* Def = FindDefinition();
	if (!Def)
	{
		if (!bLoggedMissingDefinition)
		{
			UE_LOG(LogGB, Warning, TEXT("%s: unknown encounter id '%s'"), *GetName(), *EncounterId.ToString());
			bLoggedMissingDefinition = true;
		}
		return;
	}
	const int32 NumWaves = Def->Waves.Num();

	State = EGBEncounterState::Active;
	CurrentWave = -1;
	NextWaveTimer = -1.f;
	SetActorTickEnabled(true);
	OnStarted.Broadcast(this);

	if (NumWaves == 0)
	{
		Clear();
		return;
	}
	// The first wave comes at once; DelayBefore counts from the previous wave being cleared.
	SpawnWave(0);
}

void AGBEncounter::SpawnWave(int32 WaveIndex)
{
	const FGBEncounterDefinition* Def = FindDefinition();
	if (!Def || !Def->Waves.IsValidIndex(WaveIndex))
	{
		Clear();
		return;
	}

	// A reinforcement wave appears out of the player's sight only: with every point in view the wave is held (see ComputeReinforcePoints).
	TArray<FVector> HiddenPoints;
	bool bWaitRanOut = false;
	if (Def->Prespawn && WaveIndex > 0 && !ComputeReinforcePoints(HiddenPoints, bWaitRanOut))
	{
		NextWaveTimer = 0.5f;
		return;
	}

	CurrentWave = WaveIndex;
	const bool bPrespawn = Def->Prespawn;
	const bool bReinforcement = bPrespawn && WaveIndex > 0;
	const FGBEncounterWave& Wave = Def->Waves[WaveIndex];
	const int32 NumWaves = Def->Waves.Num();

	int32 Total = 0;
	for (const FGBEncounterSpawn& Spawn : Wave.Spawns)
	{
		Total += FMath::Max(0, Spawn.Count);
	}

	const UGBDataSubsystem* Data = UGBDataSubsystem::Get(this);
	const float RingRadius = Data ? Data->GetGameValue(GBEncounterKey::SpawnRingRadius, 400.f) : 400.f;
	const FVector Center = GetActorLocation();
	const FTransform& ActorTransform = GetActorTransform();

	// Copy the spawn list: spawning runs BeginPlay code that could, in theory, reload data.
	const TArray<FGBEncounterSpawn> Spawns = Wave.Spawns;
	UWorld* World = GetWorld();
	int32 SpawnIndex = 0;

	// Zone, route candidates and (for reinforcements) the spawn points ordered: out of the player's sight first, the furthest first.
	const FBox Zone = bPrespawn ? GetZone() : FBox(ForceInit);
	const TArray<FVector> WorldPoints = GetWorldSpawnPoints();
	const TArray<FVector>& Ordered = bReinforcement ? HiddenPoints : WorldPoints;
	TArray<AGBEnemyCharacter*> NewEnemies;
	for (const FGBEncounterSpawn& Spawn : Spawns)
	{
		for (int32 i = 0; i < Spawn.Count; ++i, ++SpawnIndex)
		{
			FVector Location;
			if (bReinforcement && Ordered.Num() > 0)
			{
				// Several enemies may share a hidden point: the later ones stand spaced round it, on the navmesh.
				const int32 Round = SpawnIndex / Ordered.Num();
				Location = Ordered[SpawnIndex % Ordered.Num()];
				if (Round > 0)
				{
					const float Angle = FMath::DegreesToRadians(137.5f * static_cast<float>(SpawnIndex));
					const FVector HiddenBase = Location;
					Location += FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * (150.f * Round);
					if (const UNavigationSystemV1* Nav = UNavigationSystemV1::GetCurrent(World))
					{
						FNavLocation Projected;
						if (Nav->ProjectPointToNavigation(Location, Projected, FVector(200.f, 200.f, 300.f)))
						{
							Location = Projected.Location;
						}
						else
						{
							Location = HiddenBase;
						}
					}
					// The spaced spot must be hidden too: a spot in view falls back to the hidden point itself.
					if (const AGBPlayerCharacter* EyePlayer = AGBPlayerCharacter::Get(this))
					{
						FCollisionQueryParams EyeParams(SCENE_QUERY_STAT(GBReinforceSpot), false, EyePlayer);
						const FVector Eye = EyePlayer->GetAimOrigin();
						const bool bSpotHidden = World->LineTraceTestByChannel(Eye, Location + FVector(0.f, 0.f, 100.f), ECC_Visibility, EyeParams)
							&& World->LineTraceTestByChannel(Eye, Location + FVector(0.f, 0.f, 180.f), ECC_Visibility, EyeParams);
						if (!bSpotHidden)
						{
							Location = HiddenBase;
						}
					}
				}
			}
			else if (SpawnPoints.Num() > 0)
			{
				Location = ActorTransform.TransformPosition(SpawnPoints[SpawnIndex % SpawnPoints.Num()]);
			}
			else
			{
				const float Angle = 2.f * PI * static_cast<float>(SpawnIndex) / static_cast<float>(FMath::Max(Total, 1));
				Location = Center + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * RingRadius;
			}

			const FRotator Facing(0.f, (Center - Location).GetSafeNormal2D().Rotation().Yaw, 0.f);
			if (AGBEnemyCharacter* Enemy = AGBEnemyCharacter::SpawnEnemy(World, Spawn.EnemyId, Location, Facing, Presence))
			{
				Enemy->OnDied.AddUObject(this, &AGBEncounter::HandleEnemyDied);
				AliveEnemies.Add(Enemy);
				NewEnemies.Add(Enemy);
				if (bPrespawn)
				{
					EGBUnawareBehavior Behavior = EGBUnawareBehavior::Patrol;
					if (Spawn.Behavior == FName(TEXT("Guard")))
					{
						Behavior = EGBUnawareBehavior::Guard;
					}
					else if (Spawn.Behavior == FName(TEXT("Dormant")))
					{
						Behavior = EGBUnawareBehavior::Dormant;
					}
					TArray<FVector> Candidates;
					for (const FVector& Point : WorldPoints)
					{
						if (FVector::Dist2D(Point, Location) > 100.f)
						{
							Candidates.Add(Point);
						}
					}
					Enemy->SetEncounterContext(this, Zone, Location, Behavior, Candidates);
					if (WaveIndex == 0 && Behavior == EGBUnawareBehavior::Dormant && IsSealedNow())
					{
						Enemy->SetSealed(true);
						bEnemiesSealed = true;
					}
					if (bReinforcement)
					{
						Enemy->BecomeAwareNow();
					}
				}
				else
				{
					// The classic encounters start with the player inside: their enemies come already hunting.
					Enemy->BecomeAwareNow();
				}
			}
		}
	}

	if (bReinforcement)
	{
		// For the smoke run: how many of them could the player see the moment they appeared?
		++ReinforceWaves;
		ReinforceWaitStart = -1.f;
		bLastReinforceWaitRanOut = bWaitRanOut;
		LastReinforceVisible = 0;
		if (const AGBPlayerCharacter* Player = AGBPlayerCharacter::Get(this))
		{
			FCollisionQueryParams Params(SCENE_QUERY_STAT(GBReinforceSeen), false, Player);
			for (AGBEnemyCharacter* Enemy : NewEnemies)
			{
				Params.AddIgnoredActor(Enemy);
			}
			for (const AGBEnemyCharacter* Enemy : NewEnemies)
			{
				LastReinforceVisible += World->LineTraceTestByChannel(Player->GetAimOrigin(), Enemy->GetActorLocation() + FVector(0.f, 0.f, 40.f), ECC_Visibility, Params) ? 0 : 1;
			}
		}
		UE_LOG(LogGB, Display, TEXT("ENCOUNTER %s reinforcement wave %d: %d enemies, %d in the player's view at once, wait ran out=%d"), *EncounterId.ToString(), WaveIndex + 1, NewEnemies.Num(), LastReinforceVisible, bWaitRanOut ? 1 : 0);
	}
	if (!bPrespawn)
	{
		if (AGBHUD* HUD = AGBHUD::Get(this))
		{
			HUD->ShowMessage(FString::Printf(TEXT("Wave %d / %d"), WaveIndex + 1, NumWaves));
		}
	}
	OnWaveStarted.Broadcast(this);
}

void AGBEncounter::HandleEnemyDied(AGBEnemyCharacter* Enemy)
{
	if (Enemy)
	{
		Enemy->OnDied.RemoveAll(this);
	}
	AliveEnemies.Remove(Enemy);

	if (IsPrespawnedWaiting() && GetAliveCount() == 0)
	{
		ActivatePrespawned();   // every one of them was killed before it noticed anything
	}
	if (State == EGBEncounterState::Active && GetAliveCount() == 0)
	{
		HandleWaveDefeated();
	}
}

void AGBEncounter::HandleWaveDefeated()
{
	if (State != EGBEncounterState::Active || NextWaveTimer >= 0.f)
	{
		return;
	}

	const FGBEncounterDefinition* Def = FindDefinition();
	const int32 NextWave = CurrentWave + 1;
	if (Def && Def->Waves.IsValidIndex(NextWave))
	{
		NextWaveTimer = FMath::Max(0.f, Def->Waves[NextWave].DelayBefore);
	}
	else
	{
		Clear();
	}
}

void AGBEncounter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (State == EGBEncounterState::Active && !GBEncounterBarrier::Sealed.Contains(this))
	{
		// seal only once the player is inside: an encounter woken from outside must not shut him out
		const APawn* PlayerPawn = GetWorld() && GetWorld()->GetFirstPlayerController() ? GetWorld()->GetFirstPlayerController()->GetPawn() : nullptr;
		if (PlayerPawn && Trigger && Trigger->Bounds.GetBox().IsInsideOrOn(PlayerPawn->GetActorLocation()))
		{
			GBEncounterBarrier::Set(this, true);
		}
	}

	switch (State)
	{
	case EGBEncounterState::Waiting:
		// Covers the player already standing inside (revived at a checkpoint in the box, encounter configured late).
		TryStartFromTrigger();
		TryPrespawn();
		if (bEnemiesSealed && !IsSealedNow())
		{
			WakeSealedEnemies();
		}
		if (IsPrespawnedWaiting())
		{
			AliveEnemies.RemoveAll([](const TObjectPtr<AGBEnemyCharacter>& Enemy) { return !IsValid(Enemy) || Enemy->IsDead(); });
			if (AliveEnemies.Num() == 0)
			{
				ActivatePrespawned();
			}
		}
		break;

	case EGBEncounterState::Active:
	{
		const AGBPlayerCharacter* Player = AGBPlayerCharacter::Get(this);
		if (Player && Player->IsDead())
		{
			ResetEncounter();
			break;
		}

		if (NextWaveTimer >= 0.f)
		{
			NextWaveTimer -= DeltaSeconds;
			if (NextWaveTimer <= 0.f)
			{
				NextWaveTimer = -1.f;
				SpawnWave(CurrentWave + 1);
			}
			break;
		}

		// Enemies removed without dying (fell out of the world, destroyed by a script) do not block the wave.
		AliveEnemies.RemoveAll([](const TObjectPtr<AGBEnemyCharacter>& Enemy) { return !IsValid(Enemy) || Enemy->IsDead(); });
		if (AliveEnemies.Num() == 0)
		{
			HandleWaveDefeated();
		}
		break;
	}

	case EGBEncounterState::Cleared:
	default:
		SetActorTickEnabled(false);
		break;
	}
}

void AGBEncounter::Clear()
{
	if (State == EGBEncounterState::Cleared)
	{
		return;
	}

	State = EGBEncounterState::Cleared;
	NextWaveTimer = -1.f;
	GBEncounterBarrier::Set(this, false);

	const FGBEncounterDefinition* Def = FindDefinition();
	const int32 Points = Def ? FMath::Max(0, Def->SkillPoints) : 0;
	const TArray<FGBDropDefinition> Rewards = Def ? Def->Rewards : TArray<FGBDropDefinition>();

	UGBProgressSubsystem* Progress = UGBProgressSubsystem::Get(this);
	if (Progress)
	{
		Progress->SetFlag(GetClearedFlag(), true);
	}

	AGBPlayerCharacter* Player = AGBPlayerCharacter::Get(this);
	if (Player && Points > 0)
	{
		if (UGBSkillTreeComponent* SkillTree = Player->GetSkillTree())
		{
			SkillTree->AddPoints(Points);
		}
	}

	if (Rewards.Num() > 0)
	{
		AGBPickup::SpawnDrops(GetWorld(), Rewards, GetActorLocation());
	}

	if (AGBHUD* HUD = AGBHUD::Get(this))
	{
		const FString Text = Points > 0
			? FString::Printf(TEXT("Encounter cleared: +%d skill point%s"), Points, Points == 1 ? TEXT("") : TEXT("s"))
			: FString(TEXT("Encounter cleared"));
		HUD->ShowMessage(Text);
	}

	if (Progress && Player && !Player->IsDead())
	{
		Progress->SaveCheckpoint(Player);
	}

	OnCleared.Broadcast(this);
	SetActorTickEnabled(false);
}

void AGBEncounter::ResetEncounter()
{
	if (State == EGBEncounterState::Cleared)
	{
		return;
	}

	for (const TObjectPtr<AGBEnemyCharacter>& Enemy : AliveEnemies)
	{
		if (IsValid(Enemy))
		{
			Enemy->OnDied.RemoveAll(this);
			Enemy->Destroy();
		}
	}
	AliveEnemies.Reset();
	bEnemiesSealed = false;
	GBEncounterBarrier::Set(this, false);

	State = EGBEncounterState::Waiting;
	CurrentWave = -1;
	NextWaveTimer = -1.f;
	SetActorTickEnabled(true);
	TryPrespawn();   // a prespawned encounter stands in the world again, unaware
}

bool AGBEncounter::IsSealedNow() const
{
	const FGBEncounterDefinition* Def = FindDefinition();
	if (!Def || Def->WakeFlag.IsNone())
	{
		return false;
	}
	const UGBProgressSubsystem* Progress = UGBProgressSubsystem::Get(this);
	return !(Progress && Progress->HasFlag(Def->WakeFlag));
}

void AGBEncounter::WakeSealedEnemies()
{
	bEnemiesSealed = false;
	const FVector Centre = GetActorLocation();
	for (const TObjectPtr<AGBEnemyCharacter>& Enemy : AliveEnemies)
	{
		if (!IsValid(Enemy) || Enemy->IsDead() || !Enemy->IsSealed())
		{
			continue;
		}
		// She comes out to a point in front of where she lay, towards the encounter's middle, at least 5 m away, inside the zone, on the navmesh.
		FVector Dir = (Centre - Enemy->GetActorLocation()).GetSafeNormal2D();
		if (Dir.IsNearlyZero())
		{
			Dir = Enemy->GetActorForwardVector();
		}
		const float Distance = FMath::Max(FVector::Dist2D(Centre, Enemy->GetActorLocation()), 500.f);
		FVector Target = Enemy->ClampToZone(Enemy->GetActorLocation() + Dir * Distance);
		Enemy->ProjectPointToNavigationForWake(Target);
		UE_LOG(LogGB, Display, TEXT("ENCOUNTER %s: wake flag set, %s comes out to %s"), *EncounterId.ToString(), *Enemy->GetEnemyId().ToString(), *Target.ToCompactString());
		Enemy->BeginWake(Target);
	}
}

bool AGBEncounter::IsPrespawn() const
{
	const FGBEncounterDefinition* Def = FindDefinition();
	return Def && Def->Prespawn;
}

TArray<FVector> AGBEncounter::GetWorldSpawnPoints() const
{
	TArray<FVector> Points;
	const FTransform& ActorTransform = GetActorTransform();
	for (const FVector& Local : SpawnPoints)
	{
		Points.Add(ActorTransform.TransformPosition(Local));
	}
	return Points;
}

FBox AGBEncounter::GetZone() const
{
	const UGBDataSubsystem* Data = UGBDataSubsystem::Get(this);
	const FGBEncounterDefinition* Def = FindDefinition();
	float Padding = Data ? Data->GetGameValue(FName(TEXT("EncounterLeashPadding")), 400.f) : 400.f;
	if (Def && Def->LeashPadding >= 0.f)
	{
		Padding = Def->LeashPadding;
	}
	const float ZMargin = Data ? Data->GetGameValue(FName(TEXT("EncounterLeashZMargin")), 400.f) : 400.f;
	return GBTactics::MakeZone(Trigger->Bounds.GetBox(), GetWorldSpawnPoints(), Padding, ZMargin);
}

bool AGBEncounter::ComputeReinforcePoints(TArray<FVector>& OutPoints, bool& bOutWaitRanOut)
{
	bOutWaitRanOut = false;
	OutPoints.Reset();
	TArray<FVector> Points = GetWorldSpawnPoints();
	if (Points.Num() == 0)
	{
		Points.Add(GetActorLocation());
	}
	const AGBPlayerCharacter* Player = AGBPlayerCharacter::Get(this);
	UWorld* World = GetWorld();
	if (!Player || !World)
	{
		OutPoints = Points;
		return true;
	}
	const FVector Eye = Player->GetAimOrigin();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GBReinforcement), false, Player);
	// Hidden: no line from his eyes to the point at chest or head height.
	for (const FVector& Point : Points)
	{
		const bool bChestHidden = World->LineTraceTestByChannel(Eye, Point + FVector(0.f, 0.f, 100.f), ECC_Visibility, Params);
		const bool bHeadHidden = World->LineTraceTestByChannel(Eye, Point + FVector(0.f, 0.f, 180.f), ECC_Visibility, Params);
		if (bChestHidden && bHeadHidden)
		{
			OutPoints.Add(Point);
		}
	}
	auto FurthestFirst = [&Eye](TArray<FVector>& Array)
	{
		Array.Sort([&Eye](const FVector& A, const FVector& B) { return FVector::DistSquared(A, Eye) > FVector::DistSquared(B, Eye); });
	};
	if (OutPoints.Num() > 0)
	{
		FurthestFirst(OutPoints);
		return true;
	}
	// Every point is in view: hold the wave; after EncounterReinforceMaxWait use the furthest ones.
	const float Now = World->GetTimeSeconds();
	if (ReinforceWaitStart < 0.f)
	{
		ReinforceWaitStart = Now;
	}
	const UGBDataSubsystem* Data = UGBDataSubsystem::Get(this);
	const float MaxWait = Data ? Data->GetGameValue(FName(TEXT("EncounterReinforceMaxWait")), 6.f) : 6.f;
	if (Now - ReinforceWaitStart < MaxWait)
	{
		return false;
	}
	FurthestFirst(Points);
	OutPoints = Points;
	bOutWaitRanOut = true;
	return true;
}

void AGBEncounter::TryPrespawn()
{
	if (State != EGBEncounterState::Waiting || CurrentWave >= 0 || EncounterId.IsNone() || !IsPrespawn() || !HasActorBegunPlay())
	{
		return;
	}
	if (!AreRequiredFlagsMet())
	{
		return;
	}
	const UGBProgressSubsystem* Progress = UGBProgressSubsystem::Get(this);
	if (Progress && Progress->HasFlag(GetClearedFlag()))
	{
		return;
	}
	SpawnWave(0);
}

void AGBEncounter::ActivatePrespawned()
{
	if (State != EGBEncounterState::Waiting)
	{
		return;
	}
	State = EGBEncounterState::Active;
	NextWaveTimer = -1.f;
	SetActorTickEnabled(true);
	OnStarted.Broadcast(this);
	if (GetAliveCount() == 0)
	{
		HandleWaveDefeated();
	}
}

void AGBEncounter::NotifyEnemyAware(AGBEnemyCharacter* Enemy)
{
	(void)Enemy;
	if (IsPrespawnedWaiting())
	{
		ActivatePrespawned();
	}
}
