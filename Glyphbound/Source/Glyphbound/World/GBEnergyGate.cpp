#include "World/GBEnergyGate.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Items/GBInventoryComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Player/GBPlayerCharacter.h"
#include "UI/GBHUD.h"
#include "UObject/ConstructorHelpers.h"
#include "World/GBProgressSubsystem.h"

namespace
{
	// Neither needs to react within a frame: flags change a few times in a whole game.
	constexpr float GateCheckInterval = 0.2f;
	constexpr float SocketCheckInterval = 0.2f;
	constexpr float SocketMessageSeconds = 3.f;
	const FName GateFadeParam(TEXT("Fade"));
}

AGBEnergyGate::AGBEnergyGate()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = GateCheckInterval;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	Mesh->SetCanEverAffectNavigation(false);
	Mesh->SetCastShadow(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		Mesh->SetStaticMesh(CubeMesh.Object);
	}
}

bool AGBEnergyGate::AllFlagsPresent(const TArray<FName>& Required, const TSet<FName>& Present)
{
	if (Required.IsEmpty())
	{
		return false;
	}
	for (const FName& Flag : Required)
	{
		if (!Present.Contains(Flag))
		{
			return false;
		}
	}
	return true;
}

void AGBEnergyGate::BeginPlay()
{
	Super::BeginPlay();
	if (UMaterialInterface* Material = Mesh->GetMaterial(0))
	{
		FadeInstance = UMaterialInstanceDynamic::Create(Material, this);
		Mesh->SetMaterial(0, FadeInstance);
		FadeInstance->SetScalarParameterValue(GateFadeParam, 1.f);
	}
}

void AGBEnergyGate::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bOut)
	{
		return;
	}
	if (Fade >= 1.f)
	{
		const UGBProgressSubsystem* Progress = UGBProgressSubsystem::Get(this);
		TSet<FName> Present;
		for (const FName& Flag : RequiredFlags)
		{
			if (Progress && Progress->HasFlag(Flag))
			{
				Present.Add(Flag);
			}
		}
		if (!AllFlagsPresent(RequiredFlags, Present))
		{
			return;
		}
		// From here it fades every frame.
		SetActorTickInterval(0.f);
	}
	Fade = FadeSeconds > KINDA_SMALL_NUMBER ? FMath::Max(0.f, Fade - DeltaSeconds / FadeSeconds) : 0.f;
	if (FadeInstance)
	{
		FadeInstance->SetScalarParameterValue(GateFadeParam, Fade);
	}
	if (Fade <= 0.f)
	{
		bOut = true;
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetVisibility(false);
		SetActorTickEnabled(false);
	}
}

AGBKeySocket::AGBKeySocket()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = SocketCheckInterval;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AGBKeySocket::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bFilled || ItemId.IsNone() || Flag.IsNone())
	{
		return;
	}
	UGBProgressSubsystem* Progress = UGBProgressSubsystem::Get(this);
	if (Progress && Progress->HasFlag(Flag))
	{
		bFilled = true;                    // filled before (a loaded game)
		SetActorTickEnabled(false);
		return;
	}
	const AGBPlayerCharacter* Player = AGBPlayerCharacter::Get(this);
	const bool bNear = Player && FVector::DistSquared(Player->GetActorLocation(), GetActorLocation()) < FMath::Square(Reach);
	const bool bArrived = bNear && !bPlayerNear;
	bPlayerNear = bNear;
	if (!bNear || !Progress)
	{
		return;
	}
	UGBInventoryComponent* Inventory = Player->GetInventory();
	if (Inventory && Inventory->RemoveItem(ItemId, 1))
	{
		bFilled = true;
		Progress->SetFlag(Flag, true);
		if (AGBHUD* Hud = AGBHUD::Get(this))
		{
			Hud->ShowMessage(FilledMessage, SocketMessageSeconds);
		}
		SetActorTickEnabled(false);
	}
	else if (bArrived)
	{
		// Said once per approach, not every check while the player stands there.
		if (AGBHUD* Hud = AGBHUD::Get(this))
		{
			Hud->ShowMessage(MissingMessage, SocketMessageSeconds);
		}
	}
}
