#include "World/GBInteractable.h"

#include "Components/BillboardComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Core/GBPlaceholder.h"
#include "Core/GBTypes.h"
#include "UI/GBPixelSpriteLibrary.h"
#include "World/GBEraComponent.h"

namespace GBInteractableLocal
{
	// Size of the sphere the player's interact query finds (cm). Collision helper, not a gameplay number.
	constexpr float DefaultInteractRadius = 80.f;
}

AGBInteractableActor::AGBInteractableActor()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Root);
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	Mesh->SetGenerateOverlapEvents(false);
	Mesh->SetVisibility(false);

	Sprite = CreateDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));
	Sprite->SetupAttachment(Root);
	Sprite->SetHiddenInGame(true);

	InteractVolume = CreateDefaultSubobject<USphereComponent>(TEXT("InteractVolume"));
	InteractVolume->SetupAttachment(Root);
	InteractVolume->InitSphereRadius(GBInteractableLocal::DefaultInteractRadius);
	InteractVolume->SetRelativeLocation(FVector(0.f, 0.f, GBInteractableLocal::DefaultInteractRadius));
	InteractVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractVolume->SetCollisionObjectType(ECC_WorldDynamic);
	InteractVolume->SetCollisionResponseToAllChannels(ECR_Overlap);
	InteractVolume->SetGenerateOverlapEvents(true);
	InteractVolume->SetCanEverAffectNavigation(false);

	EraComponent = CreateDefaultSubobject<UGBEraComponent>(TEXT("Era"));
}

bool AGBInteractableActor::CanInteract(const AGBPlayerCharacter* Player) const
{
	return !EraComponent || EraComponent->IsPresent();
}

void AGBInteractableActor::SetPlaceholderLook(bool bPixelArt, FName SpriteId, const FLinearColor& Color, const FVector& Size)
{
	const float HalfHeight = Size.Z * 0.5f;

	// The box is set up when it is seen (3D look) or when it must block (solid pixel-art things keep an invisible box).
	if (!bPixelArt || bSolid)
	{
		Mesh->SetRelativeLocation(FVector(0.f, 0.f, HalfHeight));
		GBPlaceholder::SetupShape(Mesh, EGBShape::Cube, Size, Color, false);
		Mesh->SetVisibility(!bPixelArt);
		Mesh->SetCollisionProfileName(bSolid ? UCollisionProfile::BlockAll_ProfileName : UCollisionProfile::NoCollision_ProfileName);
	}
	else
	{
		Mesh->SetVisibility(false);
		Mesh->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	}

	if (bPixelArt)
	{
		Sprite->SetRelativeLocation(FVector(0.f, 0.f, HalfHeight));
		if (UGBPixelSpriteLibrary* Library = UGBPixelSpriteLibrary::Get(this))
		{
			Library->SetupBillboard(Sprite, SpriteId, Size.Z);
		}
		else
		{
			UE_LOG(LogGB, Warning, TEXT("%s: no pixel sprite library, sprite %s not shown."), *GetName(), *SpriteId.ToString());
		}
		Sprite->SetHiddenInGame(false);
		Sprite->SetVisibility(true);
	}
	else
	{
		Sprite->SetHiddenInGame(true);
	}

	InteractVolume->SetRelativeLocation(FVector(0.f, 0.f, FMath::Max(HalfHeight, InteractVolume->GetUnscaledSphereRadius() * 0.5f)));
}
