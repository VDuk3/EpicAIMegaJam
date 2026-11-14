#include "World/GBSeal.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Core/GBPlaceholder.h"
#include "Player/GBPlayerCharacter.h"
#include "World/GBCutsceneSubsystem.h"
#include "World/GBEraComponent.h"
#include "World/GBEraSubsystem.h"
#include "World/GBProgressSubsystem.h"

namespace GBSealLocal
{
	// Placeholder disc (cm) and colors: visuals only.
	const FVector DiscSize(200.f, 200.f, 6.f);
	constexpr float InteractRadius = 170.f;
	const FLinearColor ReadyColor(0.6f, 0.15f, 1.f);
	const FLinearColor UsedColor(0.08f, 0.04f, 0.12f);
}

AGBSeal::AGBSeal()
{
	// Ticks only while a travel waits to be accepted.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	// Past only (a hand-placed instance can change this like any era rule).
	EraComponent->Presence = EGBEraPresence::PastOnly;
	InteractVolume->InitSphereRadius(GBSealLocal::InteractRadius);
}

void AGBSeal::BeginPlay()
{
	Super::BeginPlay();

	Mesh->SetRelativeLocation(FVector(0.f, 0.f, GBSealLocal::DiscSize.Z * 0.5f));
	GBPlaceholder::SetupShape(Mesh, EGBShape::Cylinder, GBSealLocal::DiscSize, GBSealLocal::ReadyColor, true);
	Mesh->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	Mesh->SetCastShadow(false);
	Mesh->SetVisibility(true);
	InteractVolume->SetRelativeLocation(FVector(0.f, 0.f, 60.f));

	if (UGBProgressSubsystem* Progress = UGBProgressSubsystem::Get(this))
	{
		FlagChangedHandle = Progress->OnFlagChanged.AddUObject(this, &AGBSeal::HandleFlagChanged);
	}
	UpdateLook();
}

void AGBSeal::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UGBProgressSubsystem* Progress = UGBProgressSubsystem::Get(this))
	{
		Progress->OnFlagChanged.Remove(FlagChangedHandle);
	}
	FlagChangedHandle.Reset();
	Super::EndPlay(EndPlayReason);
}

void AGBSeal::HandleFlagChanged(FName Flag)
{
	if (Flag == GBSealName::SealUsedFlag)
	{
		UpdateLook();
	}
}

void AGBSeal::UpdateLook()
{
	GBPlaceholder::SetShapeColor(Mesh, IsUsed() ? GBSealLocal::UsedColor : GBSealLocal::ReadyColor);
}

bool AGBSeal::IsUsed() const
{
	const UGBProgressSubsystem* Progress = UGBProgressSubsystem::Get(this);
	return Progress && Progress->HasFlag(GBSealName::SealUsedFlag);
}

bool AGBSeal::CanInteract(const AGBPlayerCharacter* Player) const
{
	const UGBEraSubsystem* Era = UGBEraSubsystem::Get(this);
	return Super::CanInteract(Player) && !IsUsed() && Era && !Era->IsTravelling() && Player && !Player->IsDead();
}

FString AGBSeal::GetInteractPrompt(const AGBPlayerCharacter* Player) const
{
	return TEXT("Touch the seal");
}

void AGBSeal::Interact(AGBPlayerCharacter* Player)
{
	if (!CanInteract(Player))
	{
		return;
	}

	// Marked used first: nothing can use it twice, whatever happens during the cutscene.
	if (UGBProgressSubsystem* Progress = UGBProgressSubsystem::Get(this))
	{
		Progress->SetFlag(GBSealName::SealUsedFlag);
	}

	TWeakObjectPtr<AGBSeal> WeakThis(this);
	TWeakObjectPtr<AGBPlayerCharacter> WeakPlayer(Player);
	auto Travel = [WeakThis, WeakPlayer]()
	{
		if (AGBSeal* Seal = WeakThis.Get())
		{
			Seal->BeginPendingTravel(WeakPlayer.Get());
		}
	};
	auto Cancelled = [WeakThis]()
	{
		if (AGBSeal* Seal = WeakThis.Get())
		{
			Seal->ResetUse();
		}
	};

	UGBCutsceneSubsystem* Cutscenes = UGBCutsceneSubsystem::Get(this);
	if (Cutscenes && !CutsceneId.IsNone())
	{
		// An unknown id runs Travel at once (Play logs the warning).
		Cutscenes->Play(CutsceneId, Travel, Cancelled);
	}
	else
	{
		Travel();
	}
}

void AGBSeal::BeginPendingTravel(AGBPlayerCharacter* Player)
{
	if (!Player || Player->IsDead())
	{
		ResetUse();
		return;
	}
	bTravelPending = true;
	PendingPlayer = Player;
	// Try now; if the era subsystem refuses (a travel is running) Tick retries every frame.
	Tick(0.f);
	if (bTravelPending)
	{
		SetActorTickEnabled(true);
	}
}

void AGBSeal::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bTravelPending)
	{
		SetActorTickEnabled(false);
		return;
	}
	AGBPlayerCharacter* Player = PendingPlayer.Get();
	if (!Player || Player->IsDead())
	{
		ResetUse();
		return;
	}
	UGBEraSubsystem* Era = UGBEraSubsystem::Get(this);
	if (Era && Era->RequestTravel(EGBEra::Future, Player, false))
	{
		bTravelPending = false;
		PendingPlayer.Reset();
		SetActorTickEnabled(false);
	}
}

void AGBSeal::ResetUse()
{
	bTravelPending = false;
	PendingPlayer.Reset();
	SetActorTickEnabled(false);
	if (UGBProgressSubsystem* Progress = UGBProgressSubsystem::Get(this))
	{
		Progress->SetFlag(GBSealName::SealUsedFlag, false);
	}
}
