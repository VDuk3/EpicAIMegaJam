#include "World/GBTorch.h"

#include "Components/StaticMeshComponent.h"
#include "GBPixelVfxComponent.h"

AGBTorch::AGBTorch()
{
	PrimaryActorTick.bCanEverTick = false;
	Holder = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Holder"));
	Holder->SetCollisionProfileName(TEXT("NoCollision"));
	Holder->SetMobility(EComponentMobility::Static);
	RootComponent = Holder;
	Flame = CreateDefaultSubobject<UGBPixelVfxComponent>(TEXT("Flame"));
	Flame->SetupAttachment(Holder);
}

void AGBTorch::BeginPlay()
{
	Super::BeginPlay();
	Flame->SizeScale = FlameSize;
	Flame->LightIntensityScale = LightScale;
	Flame->bLightCastsShadows = bLightCastsShadows;
	Flame->bLightCastsShadowsSet = true;
	// Torches lit at one moment would flicker in step: each starts at its own point of the loop.
	Flame->StartTimeOffset = FMath::FRandRange(0.f, 2.f);
	Flame->Play(ClipId);
}
