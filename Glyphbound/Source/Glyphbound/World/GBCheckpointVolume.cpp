#include "World/GBCheckpointVolume.h"

#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Player/GBPlayerCharacter.h"
#include "UI/GBHUD.h"
#include "World/GBProgressSubsystem.h"

AGBCheckpointVolume::AGBCheckpointVolume()
{
	PrimaryActorTick.bCanEverTick = false;

	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	RootComponent = Box;
	Box->InitBoxExtent(FVector(300.f, 300.f, 200.f));
	Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Box->SetCollisionObjectType(ECC_WorldDynamic);
	Box->SetCollisionResponseToAllChannels(ECR_Ignore);
	Box->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Box->SetGenerateOverlapEvents(true);
	Box->SetCanEverAffectNavigation(false);
	Box->SetHiddenInGame(true);
}

void AGBCheckpointVolume::SetExtent(const FVector& Extent)
{
	Box->SetBoxExtent(Extent, true);
}

void AGBCheckpointVolume::BeginPlay()
{
	Super::BeginPlay();
	Box->OnComponentBeginOverlap.AddDynamic(this, &AGBCheckpointVolume::HandleBeginOverlap);
	Box->OnComponentEndOverlap.AddDynamic(this, &AGBCheckpointVolume::HandleEndOverlap);
	GetWorldTimerManager().SetTimerForNextTick(this, &AGBCheckpointVolume::CheckInitialOverlap);
}

void AGBCheckpointVolume::HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	AGBPlayerCharacter* Player = Cast<AGBPlayerCharacter>(OtherActor);
	if (Player && OtherComp == Player->GetRootComponent())
	{
		PlayerEntered(Player);
	}
}

void AGBCheckpointVolume::HandleEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	AGBPlayerCharacter* Player = Cast<AGBPlayerCharacter>(OtherActor);
	if (Player && OtherComp == Player->GetRootComponent())
	{
		bPlayerInside = false;
	}
}

void AGBCheckpointVolume::CheckInitialOverlap()
{
	if (bPlayerInside)
	{
		return;
	}
	AGBPlayerCharacter* Player = AGBPlayerCharacter::Get(this);
	if (Player && Player->GetRootComponent() && Box->IsOverlappingComponent(Cast<UPrimitiveComponent>(Player->GetRootComponent())))
	{
		PlayerEntered(Player);
	}
}

void AGBCheckpointVolume::PlayerEntered(AGBPlayerCharacter* Player)
{
	if (bPlayerInside || !Player || Player->IsDead())
	{
		return;
	}
	bPlayerInside = true;
	SaveFor(Player);
}

void AGBCheckpointVolume::SaveFor(AGBPlayerCharacter* Player)
{
	if (!Player)
	{
		return;
	}
	if (!Player->HasActorBegunPlay())
	{
		// Its components are not initialized yet: a snapshot now would be empty.
		TWeakObjectPtr<AGBPlayerCharacter> WeakPlayer(Player);
		GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this, WeakPlayer]()
		{
			SaveFor(WeakPlayer.Get());
		}));
		return;
	}

	if (UGBProgressSubsystem* Progress = UGBProgressSubsystem::Get(this))
	{
		Progress->SaveCheckpoint(Player);
	}
	if (AGBHUD* Hud = AGBHUD::Get(this))
	{
		Hud->ShowMessage(TEXT("Checkpoint"));
	}
}
