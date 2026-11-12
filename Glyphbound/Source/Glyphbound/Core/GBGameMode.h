// Game mode: default classes, the code-built test arena, death and respawn.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Engine/GameInstance.h"
#include "GBGameMode.generated.h"

class AGBPlayerCharacter;

/** Exists so the project has its own game instance class to grow into; the state lives in the subsystems. */
UCLASS()
class GLYPHBOUND_API UGBGameInstance : public UGameInstance
{
	GENERATED_BODY()
};

/**
 * - Default pawn AGBPlayerCharacter, controller AGBPlayerController, HUD AGBHUD.
 * - If the map has no PlayerStart (the engine's empty Entry map is the default map), it spawns AGBTestArena,
 *   which builds the whole test level in code, before the player is spawned.
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

protected:
	bool bWaitingToRespawn = false;
	FTimerHandle RespawnTimer;
	/** Where the test arena wants the player to start, when it was built. */
	FTransform ArenaStart;
	bool bHasArenaStart = false;
};
