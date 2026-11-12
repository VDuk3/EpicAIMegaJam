#include "Core/GBGameMode.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "UObject/Package.h"
#include "AI/GBEncounter.h"
#include "Core/GBTypes.h"
#include "Data/GBDataSubsystem.h"
#include "Player/GBPlayerCharacter.h"
#include "Player/GBPlayerController.h"
#include "UI/GBHUD.h"
#include "World/GBProgressSubsystem.h"
#include "World/GBTestArena.h"

AGBGameMode::AGBGameMode()
{
	DefaultPawnClass = AGBPlayerCharacter::StaticClass();
	PlayerControllerClass = AGBPlayerController::StaticClass();
	HUDClass = AGBHUD::StaticClass();

	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
}

AGBGameMode* AGBGameMode::Get(const UObject* WorldContextObject)
{
	if (!WorldContextObject || !GEngine)
	{
		return nullptr;
	}
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
	return World ? Cast<AGBGameMode>(World->GetAuthGameMode()) : nullptr;
}

void AGBGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	bool bHasPlayerStart = false;
	for (TActorIterator<APlayerStart> It(World); It; ++It)
	{
		bHasPlayerStart = true;
		break;
	}

	AGBTestArena* ExistingArena = nullptr;
	for (TActorIterator<AGBTestArena> It(World); It; ++It)
	{
		ExistingArena = *It;
		break;
	}

	// The default map is the engine's empty Entry map (it has a PlayerStart but no floor): the test level
	// is built in code whenever the map is an engine map or has no PlayerStart of its own.
	const FString PackageName = UWorld::RemovePIEPrefix(World->GetPackage()->GetName());
	const bool bEngineMap = PackageName.StartsWith(TEXT("/Engine/"));

	if (ExistingArena)
	{
		ArenaPlayerStart = ExistingArena->GetPlayerStartActor();
		bHasArenaStart = ArenaPlayerStart.IsValid();
		if (bHasArenaStart)
		{
			ArenaStart = ArenaPlayerStart->GetActorTransform();
		}
		return;
	}
	if (bHasPlayerStart && !bEngineMap)
	{
		return;
	}

	// The default map is the engine's empty Entry map: build the test level in code before the player spawns.
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AGBTestArena* Arena = World->SpawnActor<AGBTestArena>(AGBTestArena::StaticClass(), FTransform::Identity, Params);
	if (!Arena)
	{
		UE_LOG(LogGB, Error, TEXT("GameMode: could not spawn the test arena."));
		return;
	}
	Arena->BuildStatic();
	ArenaStart = Arena->GetPlayerStartTransform();
	bHasArenaStart = true;
	ArenaPlayerStart = Arena->GetPlayerStartActor();
}

AActor* AGBGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	// The arena built a real APlayerStart: use it, not the one that came with an engine map.
	if (APlayerStart* Start = ArenaPlayerStart.Get())
	{
		return Start;
	}
	return Super::ChoosePlayerStart_Implementation(Player);
}

void AGBGameMode::HandlePlayerDeath(AGBPlayerCharacter* Player)
{
	if (bWaitingToRespawn)
	{
		return;
	}
	bWaitingToRespawn = true;

	if (Player)
	{
		if (AGBPlayerController* Controller = Cast<AGBPlayerController>(Player->GetController()))
		{
			Controller->SetMenu(EGBMenu::Death);
		}
	}

	const UGBDataSubsystem* Data = UGBDataSubsystem::Get(this);
	if (!Data)
	{
		UE_LOG(LogGB, Warning, TEXT("GameMode: no data subsystem, using the default respawn delay."));
	}
	const float Delay = FMath::Max(0.f, Data ? Data->GetGameValue(GBGameValue::RespawnDelay, 2.5f) : 2.5f);
	if (Delay <= 0.f)
	{
		RespawnPlayer();
		return;
	}

	RespawnRealTime = GetWorld()->GetRealTimeSeconds() + Delay;
	GetWorldTimerManager().SetTimer(RespawnTimer, this, &AGBGameMode::RespawnPlayer, Delay, false);
}

void AGBGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Timers do not run while the game is paused (the death screen pauses it): fall back to real time.
	if (bWaitingToRespawn && GetWorld() && GetWorld()->GetRealTimeSeconds() >= RespawnRealTime)
	{
		RespawnPlayer();
	}
}

void AGBGameMode::RespawnPlayer()
{
	if (!bWaitingToRespawn)
	{
		return;
	}
	bWaitingToRespawn = false;

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	GetWorldTimerManager().ClearTimer(RespawnTimer);

	// Provisional rule (GDD 10): a fight in progress starts over; cleared encounters stay cleared.
	for (TActorIterator<AGBEncounter> It(World); It; ++It)
	{
		if (It->GetState() == EGBEncounterState::Active)
		{
			It->ResetEncounter();
		}
	}

	AGBPlayerCharacter* Player = AGBPlayerCharacter::Get(this);
	AGBPlayerController* Controller = Player ? Cast<AGBPlayerController>(Player->GetController()) : nullptr;
	if (!Controller)
	{
		Controller = Cast<AGBPlayerController>(World->GetFirstPlayerController());
	}

	UGBProgressSubsystem* Progress = UGBProgressSubsystem::Get(this);
	const bool bRestored = Player && Progress && Progress->RestoreCheckpoint(Player);

	if (Controller && Controller->GetOpenMenu() == EGBMenu::Death)
	{
		Controller->SetMenu(EGBMenu::None);
	}

	if (bRestored)
	{
		Player->SetControlEnabled(true);
		return;
	}

	// No checkpoint yet: start the map again (progress flags survive in the game instance).
	UE_LOG(LogGB, Log, TEXT("GameMode: no checkpoint to restore, reloading the map."));
	const FString MapPath = UWorld::RemovePIEPrefix(World->GetPackage()->GetName());
	UGameplayStatics::OpenLevel(this, FName(*MapPath));
}
