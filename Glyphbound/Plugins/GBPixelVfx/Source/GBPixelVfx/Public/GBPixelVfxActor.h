// Actor wrapper around UGBPixelVfxComponent, with a static spawn helper.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GBPixelVfxActor.generated.h"

class UGBPixelVfxComponent;

UCLASS()
class GBPIXELVFX_API AGBPixelVfxActor : public AActor
{
	GENERATED_BODY()

public:
	AGBPixelVfxActor();

	// One-shot clips destroy the actor when finished; loops live until destroyed.
	// Returns null when the clip id is unknown.
	static AGBPixelVfxActor* SpawnPixelVfx(UObject* WorldContext, FName ClipId,
		const FTransform& Transform, float SizeScale = 1.f);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PixelVfx")
	TObjectPtr<UGBPixelVfxComponent> PixelVfx;
};
