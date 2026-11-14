#include "World/GBLadder.h"

#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Player/GBPlayerCharacter.h"

namespace
{
	// The sphere the player's interact query finds, at chest height over the standing point. Collision helper.
	constexpr float LadderInteractRadius = 70.f;
	constexpr float LadderInteractHeight = 110.f;
	// An end is used from its own floor: feet more than this above or below its standing point are on another level.
	constexpr float LadderUseHeightCm = 120.f;
}

AGBLadder::AGBLadder()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	InteractVolume = CreateDefaultSubobject<USphereComponent>(TEXT("InteractVolume"));
	InteractVolume->SetupAttachment(RootComponent);
	InteractVolume->SetRelativeLocation(FVector(0.f, 0.f, LadderInteractHeight));
	InteractVolume->InitSphereRadius(LadderInteractRadius);
	InteractVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractVolume->SetCollisionObjectType(ECC_WorldDynamic);
	InteractVolume->SetCollisionResponseToAllChannels(ECR_Overlap);
	InteractVolume->SetCanEverAffectNavigation(false);
}

FVector AGBLadder::GetOtherEnd() const
{
	FVector Ahead = GetActorForwardVector();
	Ahead.Z = 0.f;
	return GetActorLocation() + Ahead.GetSafeNormal() * StepOff + FVector::UpVector * Rise;
}

float AGBLadder::ComputeClimbSeconds(float RiseCm, float SpeedCmPerSec, float InStepSeconds)
{
	const float Speed = FMath::Max(SpeedCmPerSec, 10.f);
	return FMath::Abs(RiseCm) / Speed + FMath::Max(InStepSeconds, 0.f);
}

bool AGBLadder::IsWithinUse(float LevelDistanceCm, float HeightDifferenceCm, float InUseDistance)
{
	return LevelDistanceCm <= InUseDistance && FMath::Abs(HeightDifferenceCm) <= LadderUseHeightCm;
}

bool AGBLadder::CanInteract(const AGBPlayerCharacter* Player) const
{
	if (!Player || Player->IsMantling() || FMath::IsNearlyZero(Rise))
	{
		return false;
	}
	// Only from this end's own standing place. The interact query reaches 2.5 m and through thin things: without
	// this the ladder could be taken from across a railing or from the floor under its head (code review, 2026-11-14).
	const FVector Feet = Player->GetActorLocation() - FVector(0.f, 0.f, Player->GetSimpleCollisionHalfHeight());
	return IsWithinUse(static_cast<float>(FVector::Dist2D(Feet, GetActorLocation())), static_cast<float>(Feet.Z - GetActorLocation().Z), UseDistance);
}

void AGBLadder::Interact(AGBPlayerCharacter* Player)
{
	if (!CanInteract(Player))
	{
		return;
	}
	const FVector Lift(0.f, 0.f, Player->GetSimpleCollisionHalfHeight());
	const FVector Here = GetActorLocation() + Lift;
	const FVector There = GetOtherEnd() + Lift;
	// On the ladder the climber always faces the way from its foot to its head.
	const float YawUp = GoesUp() ? GetActorRotation().Yaw : GetActorRotation().Yaw + 180.f;
	Player->StartLadder(GoesUp() ? Here : There, GoesUp() ? There : Here, GoesUp(),
		ComputeClimbSeconds(Rise, ClimbSpeed, StepSeconds), YawUp, bSlanted);
}

FString AGBLadder::GetInteractPrompt(const AGBPlayerCharacter* Player) const
{
	return GoesUp() ? TEXT("Climb up") : TEXT("Climb down");
}
