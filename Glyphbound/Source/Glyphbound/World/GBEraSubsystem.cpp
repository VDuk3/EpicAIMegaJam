#include "World/GBEraSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/LevelStreaming.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/MovementComponent.h"
#include "Data/GBDataSubsystem.h"
#include "Player/GBPlayerCharacter.h"
#include "Stats/GBAttributeComponent.h"
#include "World/GBProgressSubsystem.h"

namespace GBEraSubsystemLocal
{
	// A hitch (loading, breakpoint) must not skip the whole fade in one frame.
	constexpr float MaxFadeStep = 0.1f;
}

UGBEraSubsystem* UGBEraSubsystem::Get(const UObject* WorldContextObject)
{
	if (!WorldContextObject || !GEngine)
	{
		return nullptr;
	}
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
	return World ? World->GetSubsystem<UGBEraSubsystem>() : nullptr;
}

bool UGBEraSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UGBEraSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UGBEraSubsystem, STATGROUP_Tickables);
}

void UGBEraSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	// A map that is only the future (the base, while the castle has no map) says so on its player start.
	for (TActorIterator<APlayerStart> It(&InWorld); It; ++It)
	{
		if (It->ActorHasTag(FName(TEXT("GBStartFuture"))))
		{
			CurrentEra = EGBEra::Future;
			break;
		}
	}
	ApplyEra();
}

bool UGBEraSubsystem::IsPresentIn(EGBEraPresence Presence, EGBEra Era)
{
	switch (Presence)
	{
	case EGBEraPresence::PastOnly:   return Era == EGBEra::Past;
	case EGBEraPresence::FutureOnly: return Era == EGBEra::Future;
	default:                         return true;
	}
}

void UGBEraSubsystem::SetEraImmediate(EGBEra NewEra)
{
	CurrentEra = NewEra;
	ApplyEra();
}

bool UGBEraSubsystem::RequestTravel(EGBEra TargetEra, AActor* Traveller, bool bTeleport, const FTransform& Destination)
{
	if (bTravelling)
	{
		return false;
	}

	const UGBDataSubsystem* Data = UGBDataSubsystem::Get(this);
	if (!Data)
	{
		UE_LOG(LogGB, Warning, TEXT("Era travel: no data subsystem, using default fade times."));
	}
	FadeOutTime = Data ? Data->GetGameValue(GBGameValue::EraFadeOutTime, 0.35f) : 0.35f;
	HoldTime = Data ? Data->GetGameValue(GBGameValue::EraHoldTime, 0.25f) : 0.25f;
	FadeInTime = Data ? Data->GetGameValue(GBGameValue::EraFadeInTime, 0.45f) : 0.45f;

	bTravelling = true;
	TravelStage = 0;
	StageTime = 0.f;
	FadeAlpha = 0.f;
	PendingEra = TargetEra;
	PendingTraveller = Traveller;
	bPendingTeleport = bTeleport;
	PendingDestination = Destination;

	if (AGBPlayerCharacter* Player = Cast<AGBPlayerCharacter>(Traveller))
	{
		Player->SetControlEnabled(false);
	}
	return true;
}

void UGBEraSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bTravelling)
	{
		return;
	}

	// The fade runs in real time: the spell QTE slows the world, the fade must not slow with it.
	const UWorld* World = GetWorld();
	const float RealDelta = FMath::Clamp(World ? World->DeltaRealTimeSeconds : DeltaTime, 0.f, GBEraSubsystemLocal::MaxFadeStep);
	StageTime += RealDelta;

	if (TravelStage == 0)
	{
		FadeAlpha = FadeOutTime > 0.f ? FMath::Clamp(StageTime / FadeOutTime, 0.f, 1.f) : 1.f;
		if (StageTime >= FadeOutTime)
		{
			FadeAlpha = 1.f;
			PerformSwap();
			TravelStage = 1;
			StageTime = 0.f;
		}
	}
	else if (TravelStage == 1)
	{
		FadeAlpha = 1.f;
		if (StageTime >= HoldTime)
		{
			TravelStage = 2;
			StageTime = 0.f;
		}
	}
	else
	{
		FadeAlpha = FadeInTime > 0.f ? 1.f - FMath::Clamp(StageTime / FadeInTime, 0.f, 1.f) : 0.f;
		if (StageTime >= FadeInTime)
		{
			FinishTravel();
		}
	}
}

void UGBEraSubsystem::PerformSwap()
{
	// The traveller died while the screen was covered: undo the travel and fade back in (no teleport,
	// no restore, no checkpoint for a dead player).
	bool bTravellerDead = false;
	if (AGBPlayerCharacter* DeadPlayer = Cast<AGBPlayerCharacter>(PendingTraveller.Get()))
	{
		bTravellerDead = DeadPlayer->IsDead();
	}
	else if (const UGBAttributeComponent* DeadAttributes = UGBAttributeComponent::Find(PendingTraveller.Get()))
	{
		bTravellerDead = DeadAttributes->IsDead();
	}
	if (bTravellerDead)
	{
		PendingEra = CurrentEra;
		return;
	}

	CurrentEra = PendingEra;
	ApplyEra();

	AActor* Traveller = PendingTraveller.Get();
	if (!Traveller)
	{
		return;
	}

	if (bPendingTeleport)
	{
		const FVector Location = PendingDestination.GetLocation();
		const FRotator Rotation = PendingDestination.Rotator();
		if (!Traveller->TeleportTo(Location, Rotation))
		{
			UE_LOG(LogGB, Warning, TEXT("Era travel: teleport of %s to %s failed."), *Traveller->GetName(), *Location.ToString());
		}
		if (UMovementComponent* Movement = Traveller->FindComponentByClass<UMovementComponent>())
		{
			Movement->StopMovementImmediately();
		}
		if (APawn* Pawn = Cast<APawn>(Traveller))
		{
			if (AController* Controller = Pawn->GetController())
			{
				FRotator Control = Controller->GetControlRotation();
				Control.Yaw = Rotation.Yaw;
				Control.Roll = 0.f;
				Controller->SetControlRotation(Control);
			}
		}
	}

	if (AGBPlayerCharacter* Player = Cast<AGBPlayerCharacter>(Traveller))
	{
		// GDD 4: era travel restores health, mana and ammo; portals are the checkpoints of the map.
		// Health and mana are only raised towards the fractions, never lowered. Never for a dead player
		// (death undoes the travel before this point).
		if (!Player->IsDead())
		{
			const UGBDataSubsystem* RestoreData = UGBDataSubsystem::Get(this);
			const float HealthFraction = RestoreData ? RestoreData->GetGameValue(GBGameValue::EraTravelHealthFraction, 1.f) : 1.f;
			const float ManaFraction = RestoreData ? RestoreData->GetGameValue(GBGameValue::EraTravelManaFraction, 1.f) : 1.f;
			const bool bRefillAmmo = RestoreData ? RestoreData->GetGameValue(GBGameValue::EraTravelRefillAmmo, 1.f) > 0.5f : true;
			Player->RestoreAll(HealthFraction, ManaFraction, bRefillAmmo, /*bOnlyRaise*/ true);
			if (UGBProgressSubsystem* Progress = UGBProgressSubsystem::Get(this))
			{
				Progress->SaveCheckpoint(Player);
			}
		}
	}
}

void UGBEraSubsystem::FinishTravel()
{
	FadeAlpha = 0.f;
	bTravelling = false;
	TravelStage = 0;
	StageTime = 0.f;

	if (AGBPlayerCharacter* Player = Cast<AGBPlayerCharacter>(PendingTraveller.Get()))
	{
		Player->SetControlEnabled(true);
	}
	PendingTraveller.Reset();

	OnTravelFinished.Broadcast(CurrentEra);
}

void UGBEraSubsystem::ApplyEra()
{
	if (UWorld* World = GetWorld())
	{
		for (ULevelStreaming* Level : World->GetStreamingLevels())
		{
			if (!Level)
			{
				continue;
			}
			const FString PackageName = Level->GetWorldAssetPackageName();
			const bool bPast = PackageName.EndsWith(TEXT("_Past"));
			const bool bFuture = PackageName.EndsWith(TEXT("_Future"));
			if (!bPast && !bFuture)
			{
				continue;
			}
			// Both versions stay loaded so the swap behind the fade is only a visibility change.
			Level->SetShouldBeLoaded(true);
			Level->SetShouldBeVisible(bPast ? CurrentEra == EGBEra::Past : CurrentEra == EGBEra::Future);
		}
	}

	OnEraChanged.Broadcast(CurrentEra);
}
