// AGBPixelVfxActor: root component wrapper plus the static spawn helper.
#include "GBPixelVfxActor.h"

#include "GBPixelVfxComponent.h"
#include "GBPixelVfxSubsystem.h"

AGBPixelVfxActor::AGBPixelVfxActor()
{
	PixelVfx = CreateDefaultSubobject<UGBPixelVfxComponent>(TEXT("PixelVfx"));
	SetRootComponent(PixelVfx);
}

AGBPixelVfxActor* AGBPixelVfxActor::SpawnPixelVfx(UObject* WorldContext, FName ClipId,
	const FTransform& Transform, float SizeScale)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World)
	{
		UE_LOG(LogGBPixelVfx, Warning, TEXT("PixelVfx spawn '%s': no world."), *ClipId.ToString());
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AGBPixelVfxActor* Actor = World->SpawnActor<AGBPixelVfxActor>(AGBPixelVfxActor::StaticClass(), Transform, Params);
	if (!Actor)
	{
		return nullptr;
	}
	Actor->PixelVfx->SizeScale = SizeScale;
	const UGBPixelVfxSubsystem* Subsystem = World->GetGameInstance()
		? World->GetGameInstance()->GetSubsystem<UGBPixelVfxSubsystem>()
		: nullptr;
	const FGBPixelVfxClip* Clip = Subsystem ? Subsystem->FindClip(ClipId) : nullptr;
	if (Clip && !Clip->bLoop)
	{
		Actor->PixelVfx->bAutoDestroyOwnerWhenFinished = true;
	}
	if (!Actor->PixelVfx->Play(ClipId))
	{
		Actor->Destroy();
		return nullptr;
	}
	return Actor;
}
