#include "AI/GBEncounter.h"

#include "Components/BoxComponent.h"
#include "Engine/World.h"

#include "AI/GBEnemyCharacter.h"
#include "Data/GBDataSubsystem.h"
#include "Player/GBPlayerCharacter.h"
#include "Progression/GBSkillTreeComponent.h"
#include "UI/GBHUD.h"
#include "World/GBEraSubsystem.h"
#include "World/GBPickup.h"
#include "World/GBProgressSubsystem.h"

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

	Trigger->SetBoxExtent(TriggerExtent);
	Trigger->OnComponentBeginOverlap.AddUniqueDynamic(this, &AGBEncounter::HandleTriggerOverlap);

	if (UGBEraSubsystem* Era = UGBEraSubsystem::Get(this))
	{
		Era->OnEraChanged.AddUObject(this, &AGBEncounter::HandleEraChanged);
	}

	ApplyClearedFlagIfSet();
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
	if (State != EGBEncounterState::Waiting || EncounterId.IsNone())
	{
		return;
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
	if (State != EGBEncounterState::Waiting)
	{
		return;
	}

	ApplyClearedFlagIfSet();
	if (State == EGBEncounterState::Cleared)
	{
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

	CurrentWave = WaveIndex;
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
	for (const FGBEncounterSpawn& Spawn : Spawns)
	{
		for (int32 i = 0; i < Spawn.Count; ++i, ++SpawnIndex)
		{
			FVector Location;
			if (SpawnPoints.Num() > 0)
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
			}
		}
	}

	if (AGBHUD* HUD = AGBHUD::Get(this))
	{
		HUD->ShowMessage(FString::Printf(TEXT("Wave %d / %d"), WaveIndex + 1, NumWaves));
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

	switch (State)
	{
	case EGBEncounterState::Waiting:
		// Covers the player already standing inside (revived at a checkpoint in the box, encounter configured late).
		TryStartFromTrigger();
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

	State = EGBEncounterState::Waiting;
	CurrentWave = -1;
	NextWaveTimer = -1.f;
	SetActorTickEnabled(true);
}
