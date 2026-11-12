#include "World/GBEraComponent.h"

#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"
#include "World/GBEraSubsystem.h"
#include "World/GBProgressSubsystem.h"

UGBEraComponent::UGBEraComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UGBEraComponent::Configure(EGBEraPresence InPresence, FName InRequiredFlag, FName InRemovedByFlag)
{
	Presence = InPresence;
	RequiredFlag = InRequiredFlag;
	RemovedByFlag = InRemovedByFlag;
	if (HasBegunPlay())
	{
		Refresh();
	}
}

bool UGBEraComponent::ComputePresent(EGBEraPresence InPresence, EGBEra CurrentEra, bool bRequiredFlagOk, bool bRemovedFlagSet)
{
	return UGBEraSubsystem::IsPresentIn(InPresence, CurrentEra) && bRequiredFlagOk && !bRemovedFlagSet;
}

void UGBEraComponent::BeginPlay()
{
	Super::BeginPlay();

	if (UGBEraSubsystem* Era = UGBEraSubsystem::Get(this))
	{
		EraChangedHandle = Era->OnEraChanged.AddUObject(this, &UGBEraComponent::HandleEraChanged);
	}
	if (UGBProgressSubsystem* Progress = UGBProgressSubsystem::Get(this))
	{
		FlagChangedHandle = Progress->OnFlagChanged.AddUObject(this, &UGBEraComponent::HandleFlagChanged);
	}
	Refresh();
}

void UGBEraComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UGBEraSubsystem* Era = UGBEraSubsystem::Get(this))
	{
		Era->OnEraChanged.Remove(EraChangedHandle);
	}
	if (UGBProgressSubsystem* Progress = UGBProgressSubsystem::Get(this))
	{
		Progress->OnFlagChanged.Remove(FlagChangedHandle);
	}
	EraChangedHandle.Reset();
	FlagChangedHandle.Reset();

	Super::EndPlay(EndPlayReason);
}

void UGBEraComponent::HandleEraChanged(EGBEra NewEra)
{
	Refresh();
}

void UGBEraComponent::HandleFlagChanged(FName Flag)
{
	if (Flag == RequiredFlag || Flag == RemovedByFlag)
	{
		Refresh();
	}
}

void UGBEraComponent::Refresh()
{
	const UGBEraSubsystem* Era = UGBEraSubsystem::Get(this);
	const EGBEra CurrentEra = Era ? Era->GetCurrentEra() : EGBEra::Past;

	const UGBProgressSubsystem* Progress = UGBProgressSubsystem::Get(this);
	const bool bRequiredOk = RequiredFlag.IsNone() || (Progress && Progress->HasFlag(RequiredFlag));
	const bool bRemoved = !RemovedByFlag.IsNone() && Progress && Progress->HasFlag(RemovedByFlag);

	ApplyPresence(ComputePresent(Presence, CurrentEra, bRequiredOk, bRemoved));
}

void UGBEraComponent::ApplyPresence(bool bNewPresent)
{
	AActor* Owner = GetOwner();
	if (!Owner || bNewPresent == bPresent)
	{
		return;
	}
	bPresent = bNewPresent;

	APawn* Pawn = Cast<APawn>(Owner);
	UMovementComponent* Movement = Pawn ? Pawn->GetMovementComponent() : nullptr;

	if (!bNewPresent)
	{
		// Remember the owner's own settings (it may have turned collision or tick off itself).
		bSavedHidden = Owner->IsHidden();
		bSavedCollision = Owner->GetActorEnableCollision();
		bSavedTick = Owner->IsActorTickEnabled();
		bHasSaved = true;

		Owner->SetActorHiddenInGame(true);
		Owner->SetActorEnableCollision(false);
		Owner->SetActorTickEnabled(false);
		// An enemy of the other era must neither fall (its collision is off) nor walk.
		if (Movement)
		{
			Movement->StopMovementImmediately();
			Movement->SetComponentTickEnabled(false);
		}
	}
	else
	{
		Owner->SetActorHiddenInGame(bHasSaved ? bSavedHidden : false);
		Owner->SetActorEnableCollision(bHasSaved ? bSavedCollision : true);
		Owner->SetActorTickEnabled(bHasSaved ? bSavedTick : true);
		if (Movement)
		{
			Movement->SetComponentTickEnabled(true);
		}
	}

	OnPresenceChanged.Broadcast();
}
