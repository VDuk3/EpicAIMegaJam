#include "World/GBDoor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Player/GBPlayerCharacter.h"
#include "UObject/ConstructorHelpers.h"
#include "World/GBProgressSubsystem.h"

AGBDoor::AGBDoor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.f;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Root);
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	// Out of the navmesh: enemies path through the doorway; the leaf only stops them while the player is far away.
	Mesh->SetCanEverAffectNavigation(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		Mesh->SetStaticMesh(CubeMesh.Object);
	}
}

FVector AGBDoor::ComputeLeafOffset(const FVector& InSlideOffset, float Alpha)
{
	const float Clamped = FMath::Clamp(Alpha, 0.f, 1.f);
	// Smoothstep: the leaf starts and stops gently.
	const float Eased = Clamped * Clamped * (3.f - 2.f * Clamped);
	return InSlideOffset * Eased;
}

void AGBDoor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	bool bWantOpen = false;
	if (const AGBPlayerCharacter* Player = AGBPlayerCharacter::Get(this))
	{
		const FVector Centre = GetActorTransform().TransformPosition(CentreOffset);
		const bool bNear = FVector::DistSquared(Player->GetActorLocation(), Centre) < FMath::Square(OpenDistance);
		bool bUnlocked = RequiredFlag.IsNone();
		if (!bUnlocked)
		{
			const UGBProgressSubsystem* Progress = UGBProgressSubsystem::Get(this);
			bUnlocked = Progress && Progress->HasFlag(RequiredFlag);
		}
		bWantOpen = bNear && bUnlocked;
	}

	const float Step = OpenSeconds > KINDA_SMALL_NUMBER ? DeltaSeconds / OpenSeconds : 1.f;
	const float NewAlpha = FMath::Clamp(OpenAlpha + (bWantOpen ? Step : -Step), 0.f, 1.f);
	if (!FMath::IsNearlyEqual(NewAlpha, OpenAlpha))
	{
		OpenAlpha = NewAlpha;
		Mesh->SetRelativeLocation(ComputeLeafOffset(SlideOffset, OpenAlpha));
	}
}
