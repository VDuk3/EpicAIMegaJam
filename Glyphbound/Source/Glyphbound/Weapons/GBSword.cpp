#include "Weapons/GBSword.h"

#include "Combat/GBCombatLibrary.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/GBPlaceholder.h"
#include "Core/GBStatNames.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Player/GBPlayerCharacter.h"
#include "Stats/GBAttributeComponent.h"

namespace
{
	UStaticMeshComponent* MakeSwordHiltPart(AActor* Owner, USceneComponent* Parent, FName Name, EGBShape Shape, const FVector& Location,
		const FVector& Size, const FLinearColor& Color, bool bGlow)
	{
		UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(Owner, Name);
		Part->SetupAttachment(Parent);
		Part->SetRelativeLocation(Location);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetGenerateOverlapEvents(false);
		Part->SetCastShadow(false);
		Part->RegisterComponent();
		GBPlaceholder::SetupShape(Part, Shape, Size, Color, bGlow);
		return Part;
	}
}

void AGBSword::InitFromDefinition(const FGBWeaponDefinition& Definition, AGBPlayerCharacter* InOwner)
{
	Super::InitFromDefinition(Definition, InOwner);

	const FName Required[] = { GBStat::Damage, GBStat::FireInterval, GBStat::MeleeRange, GBStat::MeleeArc };
	for (const FName& Stat : Required)
	{
		if (!Definition.Stats.Contains(Stat))
		{
			UE_LOG(LogGB, Warning, TEXT("Weapons.json '%s': missing stat '%s', using a default"), *Definition.Id.ToString(), *Stat.ToString());
		}
	}

	// Placeholder 3D hilt (art rule: the hilt comes from the future). Sizes in cm are purely visual; the blade
	// rises along +Z from the guard and is drawn by the HUD as pixel art.
	Grip = MakeSwordHiltPart(this, Root, TEXT("Grip"), EGBShape::Cylinder, FVector(0.f, 0.f, -5.f), FVector(3.f, 3.f, 10.f), FLinearColor(0.10f, 0.10f, 0.12f), false);
	Guard = MakeSwordHiltPart(this, Root, TEXT("Guard"), EGBShape::Cube, FVector(0.f, 0.f, 1.f), FVector(4.f, 14.f, 2.f), FLinearColor(0.35f, 0.35f, 0.40f), false);
	ShownElement = Element;
	Gem = MakeSwordHiltPart(this, Root, TEXT("Gem"), EGBShape::Sphere, FVector(2.5f, 0.f, 1.f), FVector(2.5f, 2.5f, 2.5f),
		GBPlaceholder::ElementColor(static_cast<uint8>(ShownElement)), true);
}

void AGBSword::Tick(float DeltaSeconds)
{
	if (Gem && ShownElement != Element)
	{
		ShownElement = Element;
		GBPlaceholder::SetShapeColor(Gem, GBPlaceholder::ElementColor(static_cast<uint8>(ShownElement)));
	}
	Super::Tick(DeltaSeconds);
}

void AGBSword::GetBodySegment(AActor* Target, FVector& OutBottom, FVector& OutTop)
{
	if (const ACharacter* Character = Cast<ACharacter>(Target))
	{
		if (const UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
		{
			const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
			const FVector Center = Character->GetActorLocation();
			OutBottom = FVector(Center.X, Center.Y, Center.Z - HalfHeight);
			OutTop = FVector(Center.X, Center.Y, Center.Z + HalfHeight);
			return;
		}
	}
	if (const UCapsuleComponent* Capsule = Target ? Target->FindComponentByClass<UCapsuleComponent>() : nullptr)
	{
		const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
		const FVector Center = Capsule->GetComponentLocation();
		OutBottom = FVector(Center.X, Center.Y, Center.Z - HalfHeight);
		OutTop = FVector(Center.X, Center.Y, Center.Z + HalfHeight);
		return;
	}
	FVector Origin;
	FVector Extent;
	Target->GetActorBounds(false, Origin, Extent, false);
	OutBottom = FVector(Origin.X, Origin.Y, Origin.Z - Extent.Z);
	OutTop = FVector(Origin.X, Origin.Y, Origin.Z + Extent.Z);
}

bool AGBSword::PerformAttack()
{
	AGBPlayerCharacter* Player = GetPlayerOwner();
	UWorld* World = GetWorld();
	if (!Player || !World)
	{
		return false;
	}

	const FVector Origin = Player->GetAimOrigin();
	const FVector Forward = Player->GetAimDirection().GetSafeNormal();
	const float Range = GetStat(GBStat::MeleeRange, 200.f);
	const float HalfArcCos = FMath::Cos(FMath::DegreesToRadians(FMath::Clamp(GetStat(GBStat::MeleeArc, 90.f), 0.f, 360.f) * 0.5f));
	if (Range <= 0.f)
	{
		return true;
	}

	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
	ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);
	FCollisionQueryParams OverlapParams(SCENE_QUERY_STAT(GBSwordOverlap), false, Player);
	OverlapParams.AddIgnoredActor(this);

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps, Origin, FQuat::Identity, ObjectParams, FCollisionShape::MakeSphere(Range), OverlapParams);

	const FGBDamageInfo BaseInfo = MakeDamageInfo();
	const float HeadshotMult = GetStat(GBStat::HeadshotMult, 1.f);
	TSet<AActor*> Done;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Target = Overlap.GetActor();
		if (!Target || Done.Contains(Target))
		{
			continue;
		}
		Done.Add(Target);

		if (!UGBAttributeComponent::Find(Target) || !UGBCombatLibrary::IsAlive(Target) || !UGBCombatLibrary::AreHostile(Player, Target))
		{
			continue;
		}

		// Arc and range selection stays on the target's center, so nothing previously hit is missed.
		const FVector TargetLocation = Target->GetActorLocation();
		const FVector ToTarget = TargetLocation - Origin;
		const FVector ToTargetDir = ToTarget.GetSafeNormal();
		if (!ToTargetDir.IsNearlyZero() && FVector::DotProduct(Forward, ToTargetDir) < HalfArcCos)
		{
			continue;
		}

		FVector BodyBottom;
		FVector BodyTop;
		GetBodySegment(Target, BodyBottom, BodyTop);
		const FVector HitLocation = UGBCombatLibrary::ClosestBodyPointToAim(Origin, Forward, Range, BodyBottom, BodyTop);

		// Not through walls: trace to the aimed hit location, not the target's center.
		FCollisionQueryParams WallParams(SCENE_QUERY_STAT(GBSwordWall), false, Player);
		WallParams.AddIgnoredActor(this);
		WallParams.AddIgnoredActor(Target);
		if (World->LineTraceTestByChannel(Origin, HitLocation, ECC_Visibility, WallParams))
		{
			continue;
		}

		FGBDamageInfo Info = BaseInfo;
		Info.HitLocation = HitLocation;
		Info.HitDirection = ToTargetDir;
		UGBCombatLibrary::ApplyWeakPointBonus(Target, Info, HeadshotMult);
		UGBCombatLibrary::DealDamage(Target, Info);
	}
	return true;
}
