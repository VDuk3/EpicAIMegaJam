#include "Core/GBGameMode.h"
#include "Misc/App.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/WorldSettings.h"
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
namespace GBAudio { bool Play(const UObject* Ctx, FName Key, const FVector* Loc, float PitchMul, float VolMul); void SpawnLoop(const UObject* Ctx, FName Key, const FVector& Loc); bool IsFutureMap(const UObject* Ctx); void MenuPing(const UObject* Ctx); void UpdateMusic(UWorld* World); }

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
		ApplyKillZ(ExistingArena);
		return;
	}
	if (bHasPlayerStart && !bEngineMap)
	{
		ApplyKillZ(nullptr);
		return;
	}

	// The default map is the engine's empty Entry map: build the test level in code before the player spawns.
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AGBTestArena* Arena = World->SpawnActor<AGBTestArena>(AGBTestArena::StaticClass(), FTransform::Identity, Params);
	if (!Arena)
	{
		UE_LOG(LogGB, Error, TEXT("GameMode: could not spawn the test arena."));
		ApplyKillZ(nullptr);
		return;
	}
	Arena->BuildStatic();
	ArenaStart = Arena->GetPlayerStartTransform();
	bHasArenaStart = true;
	ArenaPlayerStart = Arena->GetPlayerStartActor();
	ApplyKillZ(Arena);
}

void AGBGameMode::ApplyKillZ(const AGBTestArena* Arena)
{
	UWorld* World = GetWorld();
	AWorldSettings* WorldSettings = World ? World->GetWorldSettings() : nullptr;
	const UGBDataSubsystem* Data = UGBDataSubsystem::Get(this);
	if (!WorldSettings || !Data)
	{
		UE_LOG(LogGB, Warning, TEXT("GameMode: no world settings or data; KillZ left as the level has it."));
		return;
	}

	// GetGameValue logs a warning once when the key is missing; the sentinel tells "missing" from any real height.
	const float Missing = -TNumericLimits<float>::Max();
	const float Value = Data->GetGameValue(GBGameValue::KillZ, Missing);
	if (Value == Missing)
	{
		return;
	}

	if (Arena)
	{
		// Floor top of the code-built arena is its actor's height (GBTestArena.cpp layout).
		WorldSettings->KillZ = static_cast<float>(Arena->GetActorLocation().Z) + Value;
		return;
	}

	// A real level that set its own KillZ keeps it.
	const AWorldSettings* Defaults = WorldSettings->GetClass()->GetDefaultObject<AWorldSettings>();
	if (Defaults && !FMath::IsNearlyEqual(WorldSettings->KillZ, Defaults->KillZ))
	{
		return;
	}
	WorldSettings->KillZ = Value;
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
	GBAudio::Play(this, FName(TEXT("player_death")), nullptr, 1.f, 1.f);
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
	{
		static float MusicClock = 0.f;
		MusicClock += FApp::GetDeltaTime();
		if (MusicClock >= 0.4f)
		{
			MusicClock = 0.f;
			GBAudio::UpdateMusic(GetWorld());
		}
	}

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
