#include "Glyphbound.h"
#include "Modules/ModuleManager.h"
#include "HAL/IConsoleManager.h"
#include "Engine/World.h"
#include "Core/GBTypes.h"
#include "Data/GBDataSubsystem.h"

IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, Glyphbound, "Glyphbound");

DEFINE_LOG_CATEGORY(LogGB);

static FAutoConsoleCommandWithWorld GGBReloadDataCommand(
	TEXT("gb.ReloadData"),
	TEXT("Reloads every Content/Data/*.json file and tells the game systems to re-read their numbers."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (UGBDataSubsystem* Data = UGBDataSubsystem::Get(World))
		{
			Data->ReloadAll();
		}
		else
		{
			UE_LOG(LogGB, Warning, TEXT("gb.ReloadData: no game instance in this world."));
		}
	}));
