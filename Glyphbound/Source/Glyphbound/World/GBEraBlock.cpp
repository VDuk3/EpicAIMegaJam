#include "World/GBEraBlock.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "NavigationSystem.h"
#include "UObject/ConstructorHelpers.h"
#include "World/GBEraComponent.h"

AGBEraBlock::AGBEraBlock()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;
	// Movable: presence flips at run time and the dynamic navmesh must see the change.
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	Mesh->SetCanEverAffectNavigation(true);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		Mesh->SetStaticMesh(CubeMesh.Object);
	}

	EraComponent = CreateDefaultSubobject<UGBEraComponent>(TEXT("Era"));
}

bool AGBEraBlock::IsPresent() const
{
	return !EraComponent || EraComponent->IsPresent();
}

void AGBEraBlock::Configure(EGBEraPresence InPresence, FName InRequiredFlag, FName InRemovedByFlag)
{
	EraComponent->Configure(InPresence, InRequiredFlag, InRemovedByFlag);
}

void AGBEraBlock::BeginPlay()
{
	Super::BeginPlay();
	// The era component (added before this one is begun? order is not guaranteed) may already have applied its state.
	EraComponent->OnPresenceChanged.AddUObject(this, &AGBEraBlock::HandlePresenceChanged);
	HandlePresenceChanged();
}

void AGBEraBlock::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (EraComponent)
	{
		EraComponent->OnPresenceChanged.RemoveAll(this);
	}
	Super::EndPlay(EndPlayReason);
}

void AGBEraBlock::HandlePresenceChanged()
{
	// Actor-level collision changes do not reach the navigation octree by themselves: tell it.
	FNavigationSystem::UpdateComponentData(*Mesh);
}
