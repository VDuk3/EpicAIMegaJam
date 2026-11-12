// Game mode: default classes, the code-built test arena, death and respawn.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Engine/GameInstance.h"
#include "GBGameMode.generated.h"

class AGBPlayerCharacter;
class AGBTestArena;
class APlayerStart;

/** Keys of Game.json "Values" read by the game mode. */
namespace GBGameValue
{
	inline const FName RespawnDelay(TEXT("RespawnDelay"));   // seconds between death and the checkpoint restore
	/** cm, world height below which the player dies (falling out of the map). Relative to the floor of the code-built arena. */
	inline const FName KillZ(TEXT("KillZ"));
}

/** Exists so the project has its own game instance class to grow into; the state lives in the subsystems. */
UCLASS()
class GLYPHBOUND_API UGBGameInstance : public UGameInstance
{
	GENERATED_BODY()
};

/**
 * - Default pawn AGBPlayerCharacter, controller AGBPlayerController, HUD AGBHUD.
 * - If the world has no AGBTestArena yet and the map has no PlayerStart (or is an engine map like the
 *   default empty Entry map), it spawns AGBTestArena, which builds the whole test level in code, and uses
 *   the APlayerStart the arena created.
 * - Falling out of the map kills the player: InitGame sets the world's KillZ from Game.json "KillZ".
 * - Player death (checkpoints only, GDD 10): shows the death screen, then after Game.json "RespawnDelay"
 *   restores the last checkpoint (or restarts the map if there is none) and resets running encounters.
 */
UCLASS()
class GLYPHBOUND_API AGBGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AGBGameMode();

	static AGBGameMode* Get(const UObject* WorldContextObject);

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

	/** Called by the player character when it dies. */
	void HandlePlayerDeath(AGBPlayerCharacter* Player);

	/** Restores the checkpoint now (also bound to the death screen's confirm). */
	void RespawnPlayer();

	bool IsWaitingToRespawn() const { return bWaitingToRespawn; }

	/** Ticks while paused: the death screen pauses the game, which also stops the respawn timer. */
	virtual void Tick(float DeltaSeconds) override;

protected:
	/**
	 * Sets the world's KillZ from Game.json "KillZ" (if present). With the code-built arena it is relative to the
	 * arena floor; on a real level it is only applied if the level kept the engine's default KillZ.
	 */
	void ApplyKillZ(const AGBTestArena* Arena);

	bool bWaitingToRespawn = false;
	/** Real time (World->GetRealTimeSeconds) at which the respawn happens even if the game is paused. */
	double RespawnRealTime = 0.0;
	FTimerHandle RespawnTimer;
	/** Where the test arena wants the player to start, when it was built. */
	FTransform ArenaStart;
	bool bHasArenaStart = false;
	/** The APlayerStart the test arena created (returned by ChoosePlayerStart when set). */
	TWeakObjectPtr<APlayerStart> ArenaPlayerStart;
};
