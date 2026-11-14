#include "World/GBFinalPortal.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"
#include "HAL/FileManager.h"
#include "Core/GBPlaceholder.h"
#include "Data/GBDataSubsystem.h"
#include "Player/GBPlayerCharacter.h"
#include "UI/GBHUD.h"
#include "World/GBEraComponent.h"

namespace GBFinalPortalLocal
{
	// Ring made of straight segments on an ellipse in the Y-Z plane (the opening faces X). cm, visuals only.
	constexpr int32 SegmentCount = 16;
	constexpr float RadiusY = 130.f;
	constexpr float RadiusZ = 230.f;
	constexpr float CenterZ = 250.f;
	constexpr float SegmentThickness = 28.f;
	const FLinearColor RingColor(0.9f, 0.1f, 0.45f);
	constexpr float InteractRadius = 230.f;
}

AGBFinalPortal::AGBFinalPortal()
{
	EraComponent->Presence = EGBEraPresence::FutureOnly;
	EraComponent->RequiredFlag = GBFinalPortalName::OpenFlag;
	InteractVolume->InitSphereRadius(GBFinalPortalLocal::InteractRadius);

	for (int32 Index = 0; Index < GBFinalPortalLocal::SegmentCount; ++Index)
	{
		const FString Name = FString::Printf(TEXT("Ring%d"), Index);
		UStaticMeshComponent* Segment = CreateDefaultSubobject<UStaticMeshComponent>(*Name);
		Segment->SetupAttachment(Root);
		Segment->SetMobility(EComponentMobility::Movable);
		Segment->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		Segment->SetGenerateOverlapEvents(false);
		Segment->SetCastShadow(false);
		RingSegments.Add(Segment);
	}
}

void AGBFinalPortal::BeginPlay()
{
	Super::BeginPlay();

	using namespace GBFinalPortalLocal;
	for (int32 Index = 0; Index < RingSegments.Num(); ++Index)
	{
		const float A0 = 2.f * PI * static_cast<float>(Index) / SegmentCount;
		const float A1 = 2.f * PI * static_cast<float>(Index + 1) / SegmentCount;
		const FVector2D P0(FMath::Cos(A0) * RadiusY, CenterZ + FMath::Sin(A0) * RadiusZ);
		const FVector2D P1(FMath::Cos(A1) * RadiusY, CenterZ + FMath::Sin(A1) * RadiusZ);
		const FVector2D Mid = (P0 + P1) * 0.5f;
		const FVector2D Delta = P1 - P0;

		UStaticMeshComponent* Segment = RingSegments[Index];
		// The segment is long along Y: turn it along the chord in the Y-Z plane.
		Segment->SetRelativeLocation(FVector(0.f, Mid.X, Mid.Y));
		Segment->SetRelativeRotation(FQuat::FindBetweenNormals(FVector::YAxisVector, FVector(0.f, Delta.X, Delta.Y).GetSafeNormal()));
		GBPlaceholder::SetupShape(Segment, EGBShape::Cube, FVector(SegmentThickness, Delta.Size() * 1.08f, SegmentThickness), RingColor, true);
	}
	InteractVolume->SetRelativeLocation(FVector(0.f, 0.f, CenterZ));
}

bool AGBFinalPortal::IsOpenableMap(const FString& MapPath)
{
	if (MapPath.IsEmpty() || !FPackageName::IsValidLongPackageName(MapPath, false))
	{
		return false;
	}
	// A world package is a .umap file (loose in the project, or in the cooked content).
	FString Filename;
	return FPackageName::TryConvertLongPackageNameToFilename(MapPath, Filename, FPackageName::GetMapPackageExtension())
		&& IFileManager::Get().FileExists(*Filename);
}

bool AGBFinalPortal::CanInteract(const AGBPlayerCharacter* Player) const
{
	return Super::CanInteract(Player) && Player && !Player->IsDead();
}

FString AGBFinalPortal::GetInteractPrompt(const AGBPlayerCharacter* Player) const
{
	return TEXT("Enter the rift");
}

void AGBFinalPortal::Interact(AGBPlayerCharacter* Player)
{
	if (!CanInteract(Player))
	{
		return;
	}

	const UGBDataSubsystem* Data = UGBDataSubsystem::Get(this);
	const FString MapPath = Data ? Data->GetGameString(GBFinalPortalName::MapPathKey, FString()) : FString();
	if (!IsOpenableMap(MapPath))
	{
		if (!MapPath.IsEmpty())
		{
			UE_LOG(LogGB, Warning, TEXT("%s: Game.json EntityMapPath '%s' is not a world package that exists; the rift stays shut."), *GetName(), *MapPath);
		}
		if (AGBHUD* Hud = AGBHUD::Get(this))
		{
			Hud->ShowMessage(TEXT("The rift is not stable yet"));
		}
		return;
	}

	// TODO before a real Entity map is plugged in: a map-transition snapshot handover is still missing. The player
	// pawn is destroyed by OpenLevel, and the only thing that survives is the progress subsystem (flags, checkpoint).
	// The checkpoint is the LAST portal / encounter save (coordinates of this map, not a hand-over of the current
	// inventory, weapons, spells and skill points), so nothing is saved here on purpose: saving it now would give
	// the new map stale coordinates and still lose what was picked up since. The new map must restore the player
	// from a snapshot taken at this moment (and ignore its transform).
	UGameplayStatics::OpenLevel(this, FName(*MapPath));
}
