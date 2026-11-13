// GBPixelVfx runtime module: log category plus the pvfx.Spawn console command (non-shipping).
#include "GBPixelVfxLog.h"

#include "GBPixelVfxActor.h"
#include "GBPixelVfxComponent.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogGBPixelVfx);

class FGBPixelVfxModule : public FDefaultGameModuleImpl
{
};

IMPLEMENT_MODULE(FGBPixelVfxModule, GBPixelVfx);

#if !UE_BUILD_SHIPPING
static void PixelVfxSpawnCommand(const TArray<FString>& Args, UWorld* World)
{
	if (!World)
	{
		UE_LOG(LogGBPixelVfx, Warning, TEXT("pvfx.Spawn: no world."));
		return;
	}
	if (Args.Num() < 1)
	{
		UE_LOG(LogGBPixelVfx, Warning, TEXT("pvfx.Spawn usage: pvfx.Spawn <ClipId> [SizeScale] [DistanceCm]"));
		return;
	}
	const FName ClipId(*Args[0]);
	float SizeScale = 1.f;
	if (Args.Num() > 1)
	{
		SizeScale = FCString::Atof(*Args[1]);
	}
	float DistanceCm = 300.f;
	if (Args.Num() > 2)
	{
		DistanceCm = FMath::Clamp(FCString::Atof(*Args[2]), 50.f, 5000.f);
	}
	APlayerCameraManager* Manager = UGameplayStatics::GetPlayerCameraManager(World, 0);
	if (!Manager)
	{
		UE_LOG(LogGBPixelVfx, Warning, TEXT("pvfx.Spawn: no player camera."));
		return;
	}
	const FVector ViewPos = Manager->GetCameraLocation();
	const FVector ViewDir = Manager->GetCameraRotation().Vector();
	const FVector Point = ViewPos + ViewDir * DistanceCm;
	FVector SpawnAt = Point;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GBPixelVfxSpawn), false);
	if (World->LineTraceSingleByChannel(Hit, Point, Point - FVector(0.f, 0.f, 2000.f), ECC_Visibility, Params))
	{
		SpawnAt = Hit.Location;
	}
	if (!AGBPixelVfxActor::SpawnPixelVfx(World, ClipId, FTransform(SpawnAt), SizeScale))
	{
		UE_LOG(LogGBPixelVfx, Warning, TEXT("pvfx.Spawn: unknown clip '%s'."), *ClipId.ToString());
	}
}

static FAutoConsoleCommandWithWorldAndArgs GBPixelVfxSpawnCmd(
	TEXT("pvfx.Spawn"),
	TEXT("Spawns a baked pixel VFX clip on the ground in front of the view (300 cm by default): pvfx.Spawn <ClipId> [SizeScale] [DistanceCm]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&PixelVfxSpawnCommand));
#endif
