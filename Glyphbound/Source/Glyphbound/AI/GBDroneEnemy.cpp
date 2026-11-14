#include "AI/GBDroneEnemy.h"

#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "PhysicsEngine/BodySetup.h"

#include "Combat/GBCombatLibrary.h"
#include "Core/GBStatNames.h"
#include "Stats/GBStatsComponent.h"

namespace GBDroneLocal
{
	const FName EyeSocket(TEXT("Eye"));
	const FName PhysicsActorProfile(TEXT("PhysicsActor"));
	const TCHAR* const RotorKeys[4] = { TEXT("Rotor_0"), TEXT("Rotor_1"), TEXT("Rotor_2"), TEXT("Rotor_3") };
	const TCHAR* const FallbackGlowParam = TEXT("EmissiveBoost");
	constexpr float BankInterpSpeed = 6.f;
}

AGBDroneEnemy::AGBDroneEnemy()
{
	// Parts exist from the start (no mesh yet); BuildLook gives them their meshes when the assets load.
	BodyPart = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DroneBody"));
	BodyPart->SetupAttachment(VisualRoot);
	for (int32 Index = 0; Index < NumRotors; ++Index)
	{
		RotorParts[Index] = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("DroneRotor%d"), Index));
		RotorParts[Index]->SetupAttachment(BodyPart, GBDroneLocal::RotorKeys[Index]);
	}
	EyePart = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DroneEye"));
	EyePart->SetupAttachment(BodyPart, GBDroneLocal::EyeSocket);

	for (UStaticMeshComponent* Part : { BodyPart.Get(), RotorParts[0].Get(), RotorParts[1].Get(), RotorParts[2].Get(), RotorParts[3].Get(), EyePart.Get() })
	{
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetCanEverAffectNavigation(false);
		Part->SetGenerateOverlapEvents(false);
		Part->SetVisibility(false);
	}
}

bool AGBDroneEnemy::BuildLook(const FGBEnemyDefinition& Def)
{
	UStaticMesh* BodyMeshAsset = GBEnemy3D::LoadAsset<UStaticMesh>(GBEnemy3D::Str(Def, GBEnemy3DKey::Body), EnemyId);
	if (!BodyMeshAsset)
	{
		return false;
	}
	VisualRoot->SetRelativeScale3D(FVector(S(TEXT("MeshScale"), 1.f)));
	BodyPart->SetStaticMesh(BodyMeshAsset);
	BodyPart->SetVisibility(true);

	for (int32 Index = 0; Index < NumRotors; ++Index)
	{
		if (UStaticMesh* Rotor = GBEnemy3D::LoadAsset<UStaticMesh>(GBEnemy3D::Str(Def, FName(GBDroneLocal::RotorKeys[Index])), EnemyId))
		{
			RotorParts[Index]->SetStaticMesh(Rotor);
			RotorParts[Index]->SetVisibility(true);
		}
	}
	if (UStaticMesh* Eye = GBEnemy3D::LoadAsset<UStaticMesh>(GBEnemy3D::Str(Def, GBEnemy3DKey::Eye), EnemyId))
	{
		EyePart->SetStaticMesh(Eye);
		EyePart->SetVisibility(true);
		EyeMaterial = EyePart->CreateDynamicMaterialInstance(0);
		if (EyeMaterial)
		{
			// The glow parameter named in Strings, or "EmissiveBoost" when the material has it.
			float Ignored = 0.f;
			const FName Wanted(*GBEnemy3D::Str(Def, GBEnemy3DKey::FxGlowParam, GBDroneLocal::FallbackGlowParam));
			if (EyeMaterial->GetScalarParameterValue(FHashedMaterialParameterInfo(Wanted), Ignored))
			{
				GlowParam = Wanted;
			}
			else if (EyeMaterial->GetScalarParameterValue(FHashedMaterialParameterInfo(FName(GBDroneLocal::FallbackGlowParam)), Ignored))
			{
				GlowParam = FName(GBDroneLocal::FallbackGlowParam);
			}
		}
	}
	return true;
}

FVector AGBDroneEnemy::GetAimOrigin() const
{
	if (bLookBuilt && EyePart && EyePart->GetStaticMesh())
	{
		return EyePart->GetComponentLocation();
	}
	return GetActorLocation();
}

FVector AGBDroneEnemy::GetRotorLocation(int32 Index) const
{
	return (Index >= 0 && Index < NumRotors && RotorParts[Index]) ? RotorParts[Index]->GetComponentLocation() : FVector::ZeroVector;
}

FVector AGBDroneEnemy::GetBodyLocation() const
{
	return BodyPart ? BodyPart->GetComponentLocation() : GetActorLocation();
}

bool AGBDroneEnemy::IsBodySimulating() const
{
	return BodyPart && BodyPart->IsSimulatingPhysics();
}

void AGBDroneEnemy::SetTelegraph(bool bOn)
{
	if (!bLookBuilt)
	{
		Super::SetTelegraph(bOn);
	}
}

void AGBDroneEnemy::UpdateVisuals(float DeltaSeconds, const AActor* Goal, bool bWindup)
{
	if (!bLookBuilt)
	{
		return;
	}
	const bool bDead = State == EGBEnemyState::Dead;
	const float MoveSpeed = FMath::Max(1.f, Stats->GetStat(GBStat::MoveSpeed, 600.f));

	// Rotors: faster while moving, opposite directions on the diagonals, running down after death.
	const FVector Velocity = GetVelocity();
	const float MoveShare = FMath::Clamp(Velocity.Size2D() / MoveSpeed, 0.f, 1.f);
	RotorSpin = bDead ? FMath::FInterpTo(RotorSpin, 0.f, DeltaSeconds, 1.5f) : 1.f;
	const float Speed = S(TEXT("RotorSpeed"), 1800.f) * (1.f + MoveShare * S(TEXT("RotorMoveBoost"), 0.6f)) * RotorSpin;
	for (int32 Index = 0; Index < NumRotors; ++Index)
	{
		const float Direction = (Index == 0 || Index == 3) ? 1.f : -1.f;
		const float Step = Direction * Speed * DeltaSeconds;
		RotorAngle[Index] = FMath::UnwindDegrees(RotorAngle[Index] + Step);
		RotorTravel += FMath::Abs(Step);
		RotorParts[Index]->SetRelativeRotation(FRotator(0.f, RotorAngle[Index], 0.f));
	}
	if (bDead)
	{
		if (bTumbling)
		{
			VisualRoot->AddLocalRotation(FRotator(TumbleRate.X, TumbleRate.Y, TumbleRate.Z) * DeltaSeconds);
		}
		return;
	}

	// Bank into the motion; the capsule stays upright.
	const FVector Local = GetActorRotation().UnrotateVector(Velocity);
	const float BankMax = S(TEXT("BankMax"), 18.f);
	const FRotator WantedBank(-FMath::Clamp(Local.X / MoveSpeed, -1.f, 1.f) * BankMax, 0.f, FMath::Clamp(Local.Y / MoveSpeed, -1.f, 1.f) * BankMax);
	BankRotation = FMath::RInterpTo(BankRotation, WantedBank, DeltaSeconds, GBDroneLocal::BankInterpSpeed);
	VisualRoot->SetRelativeRotation(BankRotation);

	// Eye: looks at the player (yaw and pitch clamped, smoothed); scans slowly left and right with no target.
	float WantedYaw = 0.f;
	float WantedPitch = 0.f;
	if (Goal && State != EGBEnemyState::Idle)
	{
		const FTransform Socket = BodyPart->GetSocketTransform(GBDroneLocal::EyeSocket);
		const FVector LocalDir = Socket.InverseTransformVectorNoScale((GBEnemy3D::TargetChest(Goal) - Socket.GetLocation()).GetSafeNormal());
		WantedYaw = FMath::RadiansToDegrees(FMath::Atan2(LocalDir.Y, LocalDir.X));
		WantedPitch = FMath::RadiansToDegrees(FMath::Atan2(LocalDir.Z, FVector2D(LocalDir.X, LocalDir.Y).Size()));
	}
	else
	{
		WantedYaw = FMath::Sin(FlightTime * S(TEXT("EyeScanHz"), 0.6f)) * S(TEXT("EyeScanDegrees"), 40.f);
	}
	const float YawMax = S(TEXT("EyeYawMax"), 70.f);
	const float PitchMax = S(TEXT("EyePitchMax"), 40.f);
	const float Smooth = S(TEXT("EyeSmooth"), 8.f);
	EyeYaw = FMath::FInterpTo(EyeYaw, FMath::Clamp(WantedYaw, -YawMax, YawMax), DeltaSeconds, Smooth);
	EyePitch = FMath::FInterpTo(EyePitch, FMath::Clamp(WantedPitch, -PitchMax, PitchMax), DeltaSeconds, Smooth);
	EyePart->SetRelativeRotation(FRotator(EyePitch, EyeYaw, 0.f));

	// The eye's glow rises through the charge.
	if (EyeMaterial && !GlowParam.IsNone())
	{
		const float Idle = S(TEXT("GlowIdle"), 1.f);
		const float Charge = S(TEXT("GlowWindup"), 6.f);
		GlowValue = FMath::FInterpTo(GlowValue, bWindup ? Charge : Idle, DeltaSeconds, bWindup ? S(TEXT("GlowRise"), 10.f) : S(TEXT("GlowFall"), 5.f));
		EyeMaterial->SetScalarParameterValue(GlowParam, GlowValue);
	}
}

void AGBDroneEnemy::PerformAttack()
{
	AActor* Goal = Target.Get();
	if (!Goal || !GetWorld())
	{
		return;
	}
	const FVector Origin = GetAimOrigin();
	FVector Direction = (GBEnemy3D::TargetChest(Goal) - Origin).GetSafeNormal();
	if (Direction.IsNearlyZero())
	{
		Direction = GetActorForwardVector();
	}
	FRandomStream Spread(static_cast<int32>(GetUniqueID()) + ShotCount);
	Direction = UGBCombatLibrary::ApplySpread(Direction, S(TEXT("SpreadDegrees"), 1.5f), Spread);
	const float Range = Stats->GetStat(GBStat::AggroRange, 3000.f) * GetGameValue(GBEnemyKey::ProjectileRangeMult, 1.5f);
	GBEnemy3D::FireHitscan(this, Origin, Direction, Range, MakeShotInfo(), ShotFx, Goal);
	++ShotCount;
	AttackTimer = 0.f;
}

void AGBDroneEnemy::OnDeathFall(const FGBDamageInfo& KillingBlow)
{
	UStaticMesh* DeathMesh = BodyPart ? BodyPart->GetStaticMesh() : nullptr;
	const bool bHasCollision = DeathMesh && DeathMesh->GetBodySetup() && DeathMesh->GetBodySetup()->AggGeom.GetElementCount() > 0;
	if (bLookBuilt && bHasCollision)
	{
		// The parent switched all collision off, which vetoes a simulated mesh: turn the actor's collision back on and keep only the capsule off.
		SetActorEnableCollision(true);
		GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		// The body leaves the capsule and falls as a simulated mesh (the rotors and the eye are attached to it).
		BodyPart->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		BodyPart->SetCollisionProfileName(GBDroneLocal::PhysicsActorProfile);
		BodyPart->SetSimulatePhysics(true);
		const FVector Push = KillingBlow.HitDirection.GetSafeNormal() * S(TEXT("DeathImpulse"), 250.f);
		BodyPart->AddImpulse(Push, NAME_None, true);
		BodyPart->AddAngularImpulseInDegrees(FVector(Random.FRandRange(-1.f, 1.f), Random.FRandRange(-1.f, 1.f), Random.FRandRange(-1.f, 1.f)) * S(TEXT("DeathSpin"), 400.f), NAME_None, true);
		return;
	}
	// Without art or collision: the capsule drops and the visual root tumbles.
	StartCapsuleFall();
	bTumbling = bLookBuilt;
	TumbleRate = FVector(Random.FRandRange(-240.f, 240.f), Random.FRandRange(-240.f, 240.f), Random.FRandRange(-240.f, 240.f));
}

FString AGBDroneEnemy::DescribeFlyer() const
{
	return FString::Printf(TEXT("%s rotorTravel=%.0f eyeYaw=%.0f glow=%.1f glowParam=%s bodySim=%d"), *Super::DescribeFlyer(), RotorTravel, EyeYaw, GlowValue,
		GlowParam.IsNone() ? TEXT("-") : *GlowParam.ToString(), IsBodySimulating() ? 1 : 0);
}
