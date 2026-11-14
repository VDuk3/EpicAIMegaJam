#include "AI/GBSentinelEnemy.h"

#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "PhysicsEngine/BodySetup.h"

#include "Combat/GBCombatLibrary.h"
#include "Core/GBPlaceholder.h"
#include "Core/GBStatNames.h"
#include "Stats/GBStatsComponent.h"

namespace GBSentinelLocal
{
	const FName CoreSocket(TEXT("Core"));
	const FName PhysicsActorProfile(TEXT("PhysicsActor"));
	const TCHAR* const DefaultTracerFx = TEXT("/Game/SciFi_Impacts/FX/NS_Tracer_Blue");
	const TCHAR* const DefaultImpactFx = TEXT("/Game/SciFi_Impacts/FX/NS_Energy_Impact_1");
	const TCHAR* const FallbackGlowParam = TEXT("EmissiveBoost");
	const FLinearColor AimLineColor(0.2f, 0.55f, 1.f);
	constexpr float AimLineThickness = 0.025f;     // scale of the engine cylinder (100 cm across)
}

AGBSentinelEnemy::AGBSentinelEnemy()
{
	ShellPart = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SentinelShell"));
	ShellPart->SetupAttachment(VisualRoot);
	CorePart = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SentinelCore"));
	CorePart->SetupAttachment(ShellPart, GBSentinelLocal::CoreSocket);
	for (UStaticMeshComponent* Part : { ShellPart.Get(), CorePart.Get() })
	{
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetCanEverAffectNavigation(false);
		Part->SetGenerateOverlapEvents(false);
		Part->SetVisibility(false);
	}

	// The aiming line: a thin glowing cylinder placed in world space each tick (it must not inherit any scale).
	AimLine = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AimLine"));
	AimLine->SetupAttachment(GetCapsuleComponent());
	AimLine->SetAbsolute(true, true, true);
	AimLine->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	AimLine->SetCanEverAffectNavigation(false);
	AimLine->SetGenerateOverlapEvents(false);
	AimLine->SetCastShadow(false);
	AimLine->SetVisibility(false);
}

bool AGBSentinelEnemy::BuildLook(const FGBEnemyDefinition& Def)
{
	UStaticMesh* ShellAsset = GBEnemy3D::LoadAsset<UStaticMesh>(GBEnemy3D::Str(Def, GBEnemy3DKey::Shell), EnemyId);
	UStaticMesh* CoreAsset = GBEnemy3D::LoadAsset<UStaticMesh>(GBEnemy3D::Str(Def, GBEnemy3DKey::Core), EnemyId);
	// The aiming line and the shot effects (the tracer and the impact of the beam) work with or without the art.
	GBPlaceholder::SetupShape(AimLine, EGBShape::Cylinder, FVector(100.f), GBSentinelLocal::AimLineColor, true);
	AimLine->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	AimLine->SetVisibility(false);
	if (!ShellAsset)
	{
		return false;
	}
	VisualRoot->SetRelativeScale3D(FVector(S(TEXT("MeshScale"), 1.f)));
	ShellPart->SetStaticMesh(ShellAsset);
	ShellPart->SetVisibility(true);
	if (CoreAsset)
	{
		CorePart->SetStaticMesh(CoreAsset);
		CorePart->SetVisibility(true);
		CoreMaterial = CorePart->CreateDynamicMaterialInstance(0);
		if (CoreMaterial)
		{
			float Ignored = 0.f;
			const FName Wanted(*GBEnemy3D::Str(Def, GBEnemy3DKey::FxGlowParam, GBSentinelLocal::FallbackGlowParam));
			if (CoreMaterial->GetScalarParameterValue(FHashedMaterialParameterInfo(Wanted), Ignored))
			{
				GlowParam = Wanted;
			}
			else if (CoreMaterial->GetScalarParameterValue(FHashedMaterialParameterInfo(FName(GBSentinelLocal::FallbackGlowParam)), Ignored))
			{
				GlowParam = FName(GBSentinelLocal::FallbackGlowParam);
			}
		}
	}
	return true;
}

FVector AGBSentinelEnemy::GetAimOrigin() const
{
	if (bLookBuilt && CorePart && CorePart->GetStaticMesh())
	{
		return CorePart->GetComponentLocation();
	}
	return GetActorLocation();
}

FVector AGBSentinelEnemy::GetShellLocation() const
{
	return ShellPart ? ShellPart->GetComponentLocation() : GetActorLocation();
}

bool AGBSentinelEnemy::IsHeadshotLocation(const FVector& HitLocation) const
{
	return FVector::Dist(HitLocation, GetAimOrigin()) <= S(TEXT("CoreRadius"), 40.f);
}

bool AGBSentinelEnemy::IsAimLineVisible() const
{
	return AimLine && AimLine->IsVisible();
}

float AGBSentinelEnemy::GetBodyTurnRate() const
{
	return S(TEXT("TurnRate"), 40.f);
}

void AGBSentinelEnemy::SetTelegraph(bool bOn)
{
	if (!bLookBuilt)
	{
		Super::SetTelegraph(bOn);
	}
}

FVector AGBSentinelEnemy::DesiredFlightDirection(const AActor* Goal, float Distance, bool bSight)
{
	// It never circles and never chases fast (its MoveSpeed is low): it drifts to get a line of sight or back into its band.
	(void)Distance;
	const FVector Flat = (Goal->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
	const float Flat2D = FVector::Dist2D(Goal->GetActorLocation(), GetActorLocation());
	const float RangeMin = S(TEXT("PreferredRangeMin"), 700.f);
	const float RangeMax = FMath::Max(S(TEXT("PreferredRangeMax"), 1600.f), RangeMin);
	if (Flat.IsNearlyZero())
	{
		return FVector::ZeroVector;
	}
	if (!bSight || Flat2D > RangeMax)
	{
		return Flat;
	}
	if (Flat2D < RangeMin)
	{
		return -Flat * 0.5f;
	}
	return FVector::ZeroVector;
}

void AGBSentinelEnemy::OnWindupChanged(bool bStarted)
{
	if (bStarted)
	{
		// The line starts where the player is and then follows him with a lag.
		if (AActor* Goal = Target.Get())
		{
			AimPoint = GBEnemy3D::TargetChest(Goal);
			bAimPointValid = true;
		}
	}
	else
	{
		UpdateAimLine(FVector::ZeroVector, FVector::ZeroVector, false);
	}
}

void AGBSentinelEnemy::UpdateAimLine(const FVector& From, const FVector& To, bool bVisible)
{
	if (!AimLine)
	{
		return;
	}
	if (!bVisible)
	{
		AimLine->SetVisibility(false);
		return;
	}
	const FVector Delta = To - From;
	const float Length = Delta.Size();
	if (Length < 1.f)
	{
		AimLine->SetVisibility(false);
		return;
	}
	const FQuat Rotation = FRotationMatrix::MakeFromZ(Delta / Length).ToQuat();
	AimLine->SetWorldTransform(FTransform(Rotation, From + Delta * 0.5f, FVector(GBSentinelLocal::AimLineThickness, GBSentinelLocal::AimLineThickness, Length / 100.f)));
	AimLine->SetVisibility(true);
}

void AGBSentinelEnemy::UpdateVisuals(float DeltaSeconds, const AActor* Goal, bool bWindup)
{
	UWorld* World = GetWorld();
	const bool bDead = State == EGBEnemyState::Dead;
	if (bDead)
	{
		UpdateAimLine(FVector::ZeroVector, FVector::ZeroVector, false);
		return;
	}

	// The aiming point follows the player with a lag; the last moments of the charge it is locked.
	FVector From = GetAimOrigin();
	if (bWindup && Goal && bAimPointValid && World)
	{
		const float Windup = FMath::Max(Stats->GetStat(GBStat::AttackWindup, 1.f), 0.05f);
		const bool bLocked = StateTime >= Windup - S(TEXT("AimLockTime"), 0.3f);
		if (!bLocked)
		{
			AimPoint = FMath::VInterpTo(AimPoint, GBEnemy3D::TargetChest(Goal), DeltaSeconds, S(TEXT("AimLag"), 2.5f));
			LockedAimPoint = AimPoint;
		}
		// The line ends where the beam would stop: the first thing in its way.
		const FVector Direction = (LockedAimPoint - From).GetSafeNormal();
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(GBSentinelAimLine), false, this);
		Params.AddIgnoredActor(Goal);
		const float Range = Stats->GetStat(GBStat::AggroRange, 3000.f);
		const FVector End = World->LineTraceSingleByChannel(Hit, From, From + Direction * Range, ECC_Visibility, Params) ? FVector(Hit.ImpactPoint) : From + Direction * Range;
		UpdateAimLine(From, End, true);
	}
	else if (!bWindup && AimLine && AimLine->IsVisible())
	{
		UpdateAimLine(FVector::ZeroVector, FVector::ZeroVector, false);
	}

	if (!bLookBuilt)
	{
		return;
	}

	// The core turns faster than the shell, inside the opening.
	float WantedYaw = 0.f;
	float WantedPitch = 0.f;
	if (Goal && State != EGBEnemyState::Idle)
	{
		const FTransform Socket = ShellPart->GetSocketTransform(GBSentinelLocal::CoreSocket);
		const FVector LocalDir = Socket.InverseTransformVectorNoScale((GBEnemy3D::TargetChest(Goal) - Socket.GetLocation()).GetSafeNormal());
		WantedYaw = FMath::RadiansToDegrees(FMath::Atan2(LocalDir.Y, LocalDir.X));
		WantedPitch = FMath::RadiansToDegrees(FMath::Atan2(LocalDir.Z, FVector2D(LocalDir.X, LocalDir.Y).Size()));
	}
	else
	{
		WantedYaw = FMath::Sin(FlightTime * S(TEXT("CoreScanHz"), 0.5f)) * S(TEXT("CoreScanDegrees"), 15.f);
	}
	const float Clamp = S(TEXT("CoreTrackAngle"), 25.f);
	const float Smooth = S(TEXT("CoreSmooth"), 10.f);
	CoreYaw = FMath::FInterpTo(CoreYaw, FMath::Clamp(WantedYaw, -Clamp, Clamp), DeltaSeconds, Smooth);
	CorePitch = FMath::FInterpTo(CorePitch, FMath::Clamp(WantedPitch, -Clamp, Clamp), DeltaSeconds, Smooth);
	CorePart->SetRelativeRotation(FRotator(CorePitch, CoreYaw, 0.f));

	// The pulse: slow when idle, fast and growing through the charge.
	const float Time = FlightTime;
	if (bWindup)
	{
		const float Windup = FMath::Max(Stats->GetStat(GBStat::AttackWindup, 1.f), 0.05f);
		const float Charge = FMath::Clamp(StateTime / Windup, 0.f, 1.f);
		PulseValue = S(TEXT("PulseCharge"), 3.f) * (0.5f + 0.5f * Charge) * (0.85f + 0.15f * FMath::Sin(Time * S(TEXT("PulseFastHz"), 14.f)));
	}
	else
	{
		PulseValue = S(TEXT("PulseIdle"), 1.f) * (0.8f + 0.2f * FMath::Sin(Time * S(TEXT("PulseSlowHz"), 2.f)));
	}
	if (CoreMaterial && !GlowParam.IsNone())
	{
		CoreMaterial->SetScalarParameterValue(GlowParam, PulseValue);
	}
}

void AGBSentinelEnemy::PerformAttack()
{
	AActor* Goal = Target.Get();
	if (!Goal || !GetWorld())
	{
		return;
	}
	// The beam goes where the line was aiming, not where the player is now.
	const FVector Origin = GetAimOrigin();
	const FVector At = bAimPointValid ? LockedAimPoint : GBEnemy3D::TargetChest(Goal);
	FVector Direction = (At - Origin).GetSafeNormal();
	if (Direction.IsNearlyZero())
	{
		Direction = GetActorForwardVector();
	}
	const float Range = Stats->GetStat(GBStat::AggroRange, 3000.f) * GetGameValue(GBEnemyKey::ProjectileRangeMult, 1.5f);
	GBEnemy3D::FireHitscan(this, Origin, Direction, Range, MakeShotInfo(), ShotFx, Goal);
	++ShotCount;
	bAimPointValid = false;
	UpdateAimLine(FVector::ZeroVector, FVector::ZeroVector, false);
}

void AGBSentinelEnemy::OnDeathFall(const FGBDamageInfo& KillingBlow)
{
	UStaticMesh* DeathMesh = ShellPart ? ShellPart->GetStaticMesh() : nullptr;
	const bool bHasCollision = DeathMesh && DeathMesh->GetBodySetup() && DeathMesh->GetBodySetup()->AggGeom.GetElementCount() > 0;
	if (bLookBuilt && bHasCollision)
	{
		// The parent switched all collision off, which vetoes a simulated mesh: turn the actor's collision back on and keep only the capsule off.
		SetActorEnableCollision(true);
		GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ShellPart->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		ShellPart->SetCollisionProfileName(GBSentinelLocal::PhysicsActorProfile);
		ShellPart->SetSimulatePhysics(true);
		ShellPart->AddImpulse(KillingBlow.HitDirection.GetSafeNormal() * S(TEXT("DeathImpulse"), 200.f), NAME_None, true);
		return;
	}
	StartCapsuleFall();
}

FString AGBSentinelEnemy::DescribeFlyer() const
{
	return FString::Printf(TEXT("%s coreYaw=%.0f pulse=%.1f aimLine=%d glowParam=%s"), *Super::DescribeFlyer(), CoreYaw, PulseValue, IsAimLineVisible() ? 1 : 0,
		GlowParam.IsNone() ? TEXT("-") : *GlowParam.ToString());
}
