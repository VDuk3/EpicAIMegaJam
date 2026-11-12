#include "World/GBPortal.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Core/GBPlaceholder.h"
#include "Player/GBPlayerCharacter.h"
#include "World/GBEraSubsystem.h"

namespace GBPortalLocal
{
	// Frame layout (cm), visuals only. The opening faces +X / -X; pillars stand on +Y / -Y.
	const FVector PillarSize(40.f, 40.f, 360.f);
	constexpr float PillarOffsetY = 140.f;
	const FVector LintelSize(40.f, 320.f, 40.f);
	const FVector SwirlSize(40.f, 240.f, 320.f);
	constexpr float PortalInteractRadius = 150.f;
	const FName PortalSpriteId(TEXT("portal"));
	// Cold cyan leads to the future, warm orange leads to the past.
	const FLinearColor ToFutureColor(0.15f, 0.85f, 1.f);
	const FLinearColor ToPastColor(1.f, 0.55f, 0.15f);

	UStaticMeshComponent* MakeFramePiece(AActor* Owner, USceneComponent* Parent, const TCHAR* Name)
	{
		UStaticMeshComponent* Piece = Owner->CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Piece->SetupAttachment(Parent);
		Piece->SetMobility(EComponentMobility::Movable);
		Piece->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		Piece->SetGenerateOverlapEvents(false);
		Piece->SetCastShadow(false);
		return Piece;
	}
}

AGBPortal::AGBPortal()
{
	PillarLeft = GBPortalLocal::MakeFramePiece(this, Root, TEXT("PillarLeft"));
	PillarLeft->SetRelativeLocation(FVector(0.f, -GBPortalLocal::PillarOffsetY, GBPortalLocal::PillarSize.Z * 0.5f));
	PillarRight = GBPortalLocal::MakeFramePiece(this, Root, TEXT("PillarRight"));
	PillarRight->SetRelativeLocation(FVector(0.f, GBPortalLocal::PillarOffsetY, GBPortalLocal::PillarSize.Z * 0.5f));
	Lintel = GBPortalLocal::MakeFramePiece(this, Root, TEXT("Lintel"));
	Lintel->SetRelativeLocation(FVector(0.f, 0.f, GBPortalLocal::PillarSize.Z + GBPortalLocal::LintelSize.Z * 0.5f));

	InteractVolume->InitSphereRadius(GBPortalLocal::PortalInteractRadius);
}

void AGBPortal::BeginPlay()
{
	Super::BeginPlay();

	SetPlaceholderLook(true, GBPortalLocal::PortalSpriteId, FLinearColor::White, GBPortalLocal::SwirlSize);
	GBPlaceholder::SetupShape(PillarLeft, EGBShape::Cube, GBPortalLocal::PillarSize, GBPortalLocal::ToFutureColor, true);
	GBPlaceholder::SetupShape(PillarRight, EGBShape::Cube, GBPortalLocal::PillarSize, GBPortalLocal::ToFutureColor, true);
	GBPlaceholder::SetupShape(Lintel, EGBShape::Cube, GBPortalLocal::LintelSize, GBPortalLocal::ToFutureColor, true);

	if (UGBEraSubsystem* Era = UGBEraSubsystem::Get(this))
	{
		EraChangedHandle = Era->OnEraChanged.AddUObject(this, &AGBPortal::UpdateColor);
		UpdateColor(Era->GetCurrentEra());
	}
}

void AGBPortal::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UGBEraSubsystem* Era = UGBEraSubsystem::Get(this))
	{
		Era->OnEraChanged.Remove(EraChangedHandle);
	}
	Super::EndPlay(EndPlayReason);
}

void AGBPortal::UpdateColor(EGBEra CurrentEra)
{
	const FLinearColor Color = UGBEraSubsystem::OtherEra(CurrentEra) == EGBEra::Future ? GBPortalLocal::ToFutureColor : GBPortalLocal::ToPastColor;
	GBPlaceholder::SetShapeColor(PillarLeft, Color);
	GBPlaceholder::SetShapeColor(PillarRight, Color);
	GBPlaceholder::SetShapeColor(Lintel, Color);
}

EGBEra AGBPortal::GetTargetEra() const
{
	const UGBEraSubsystem* Era = UGBEraSubsystem::Get(this);
	return UGBEraSubsystem::OtherEra(Era ? Era->GetCurrentEra() : EGBEra::Past);
}

bool AGBPortal::CanInteract(const AGBPlayerCharacter* Player) const
{
	const UGBEraSubsystem* Era = UGBEraSubsystem::Get(this);
	return Super::CanInteract(Player) && Era && !Era->IsTravelling() && Player && !Player->IsDead();
}

void AGBPortal::Interact(AGBPlayerCharacter* Player)
{
	if (!CanInteract(Player))
	{
		return;
	}
	if (UGBEraSubsystem* Era = UGBEraSubsystem::Get(this))
	{
		Era->RequestTravel(GetTargetEra(), Player, bTeleport, Destination);
	}
}

FString AGBPortal::GetInteractPrompt(const AGBPlayerCharacter* Player) const
{
	return GetTargetEra() == EGBEra::Future ? TEXT("Travel to the future") : TEXT("Travel to the past");
}
