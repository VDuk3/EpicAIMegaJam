#include "AI/GBFlyerEnemy.h"

#include "EngineUtils.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"

#include "AI/GBSquad.h"
#include "AI/GBTactics.h"
#include "Combat/GBCombatLibrary.h"
#include "Core/GBStatNames.h"
#include "Data/GBDataSubsystem.h"
#include "Stats/GBStatsComponent.h"
#include "World/GBEraComponent.h"

namespace GBFlyerLocal
{
	constexpr float SightInterval = 0.1f;
	constexpr float GroundProbeDepth = 6000.f;     // cm searched below the drone for the floor
	constexpr float CeilingProbeHeight = 4000.f;
	constexpr float CeilingProbeInterval = 0.15f;
	constexpr float CeilingMargin = 20.f;          // cm kept under a ceiling
	constexpr int32 SideSteps = 5;                 // candidate directions on each side of the blocked one
	constexpr float SideStepAngle = 30.f;
	constexpr float SweepRadiusScale = 0.9f;
	constexpr float LostSightAbort = 0.3f;         // s without a line before a charge is dropped
	const TCHAR* const DefaultMuzzleFx = TEXT("");
	const TCHAR* const DefaultImpactFx = TEXT("/Game/GunFX/NiagaraSystem/ImpactFX/Concrete/NS_Impact_Concrete_Low");
}

AGBFlyerEnemy::AGBFlyerEnemy()
{
	VisualRoot = CreateDefaultSubobject<USceneComponent>(TEXT("VisualRoot"));
	VisualRoot->SetupAttachment(GetCapsuleComponent());

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->bOrientRotationToMovement = false;
		Move->DefaultLandMovementMode = MOVE_Flying;
		Move->GravityScale = 0.f;
		Move->BrakingDecelerationFlying = 1500.f;
		Move->MaxAcceleration = 1500.f;
	}
	// The mesh of ACharacter is not used.
	if (USkeletalMeshComponent* Unused = GetMesh())
	{
		Unused->SetVisibility(false);
	}
}

void AGBFlyerEnemy::ApplyMovementStats()
{
	Super::ApplyMovementStats();
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->MaxFlySpeed = FMath::Max(0.f, Move->MaxWalkSpeed);
	}
}

void AGBFlyerEnemy::InitFromDefinition(const FGBEnemyDefinition& Definition)
{
	Super::InitFromDefinition(Definition);
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->BrakingDecelerationFlying = FMath::Max(1.f, StatOr("BrakingDeceleration", 1500.f));
		Move->bUseRVOAvoidance = false;   // flyers keep apart through FlyerSeparation; RVO jittered them
		Move->MaxFlySpeed = FMath::Max(1.f, GetGaitSpeed(Gait));
		Move->SetMovementMode(MOVE_Flying);
	}
}

void AGBFlyerEnemy::BeginPlay()
{
	Super::BeginPlay();
	Random.Initialize(static_cast<int32>(GetUniqueID()) ^ FMath::Rand());
	OrbitSign = Random.FRand() < 0.5f ? 1.f : -1.f;
	SideSign = OrbitSign;
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->SetMovementMode(MOVE_Flying);
		Move->MaxFlySpeed = FMath::Max(1.f, GetGaitSpeed(Gait));
	}
	GroundZ = GetActorLocation().Z - GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	if (const FGBEnemyDefinition* Def = GetDefinition())
	{
		GBEnemy3D::LoadShotFx(ShotFx, *Def, EnemyId, GBFlyerLocal::DefaultMuzzleFx, TEXT("/Game/SciFi_Impacts/FX/NS_Tracer_Red"), GBFlyerLocal::DefaultImpactFx);
		bLookBuilt = BuildLook(*Def);
		if (bLookBuilt)
		{
			BodyMesh->SetVisibility(false);
		}
	}
}

FVector AGBFlyerEnemy::DesiredFlightDirection(const AActor* Goal, float Distance, bool bSight)
{
	// It knows where the player is only while the squad sees him: otherwise it goes to the last known position.
	FVector Known = Goal->GetActorLocation();
	UGBSquadSubsystem* Squad = UGBSquadSubsystem::Get(this);
	if (!bSight && Squad)
	{
		float Age = 0.f;
		Squad->GetLastKnown(this, Known, Age);
	}
	const FVector Flat = (Known - GetActorLocation()).GetSafeNormal2D();
	if (Flat.IsNearlyZero())
	{
		return FVector::ZeroVector;
	}
	const float RangeMin = S(TEXT("PreferredRangeMin"), 500.f);
	const float RangeMax = FMath::Max(S(TEXT("PreferredRangeMax"), 1200.f), RangeMin);
	const float Flat2D = FVector::Dist2D(Known, GetActorLocation());

	// With a line of sight: an orbit slot (angles spread among the squad's flyers) instead of a random side.
	// The slot is asked for a few times a second (staggered): it moves slowly and the query traces.
	const float WorldNow = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	if (WorldNow >= OrbitNextTime)
	{
		OrbitNextTime = WorldNow + 0.3f + Random.FRandRange(0.f, 0.1f);
		bOrbitValid = Squad && Squad->GetOrbitPoint(this, 0.5f * (RangeMin + RangeMax), OrbitCache);
	}
	if (bSight && Flat2D <= RangeMax * 1.5f && bOrbitValid)
	{
		const FVector To = ClampToZone(OrbitCache) - GetActorLocation();
		return To.GetSafeNormal2D() * FMath::Clamp(To.Size2D() / 300.f, 0.f, 1.f);
	}

	// Radial pull into the band (always closing in without a line of sight), plus an orbit round the player.
	float Radial = 0.f;
	if (!bSight || Flat2D > RangeMax)
	{
		Radial = 1.f;
	}
	else if (Flat2D < RangeMin)
	{
		Radial = -1.f;
	}
	const FVector Tangent = FVector::CrossProduct(FVector::UpVector, Flat) * OrbitSign;
	const float Orbit = bSight ? S(TEXT("OrbitStrength"), 0.6f) : 0.f;
	return (Flat * Radial + Tangent * Orbit).GetClampedToMaxSize(1.f);
}

bool AGBFlyerEnemy::IsSweepBlocked(const FVector& Start, const FVector& Direction, float Distance, const AActor* Goal) const
{
	UWorld* World = GetWorld();
	const UCapsuleComponent* Capsule = GetCapsuleComponent();
	if (!World || !Capsule)
	{
		return false;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GBFlyerSweep), false, this);
	if (Goal)
	{
		Params.AddIgnoredActor(Goal);
	}
	FCollisionResponseParams Response;
	Response.CollisionResponse.SetResponse(ECC_Pawn, ECR_Ignore);
	const float Radius = Capsule->GetScaledCapsuleRadius() * GBFlyerLocal::SweepRadiusScale;
	const float HalfHeight = FMath::Max(Capsule->GetScaledCapsuleHalfHeight() * GBFlyerLocal::SweepRadiusScale, Radius);
	FHitResult Hit;
	return World->SweepSingleByChannel(Hit, Start, Start + Direction * Distance, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(Radius, HalfHeight), Params, Response)
		&& !Hit.bStartPenetrating;
}

FVector AGBFlyerEnemy::SteerAround(const FVector& Desired, const AActor* Goal, float DeltaSeconds)
{
	(void)DeltaSeconds;
	const float Length = Desired.Size2D();
	if (Length < 0.05f)
	{
		return FVector::ZeroVector;
	}
	const FVector Direction = Desired.GetSafeNormal2D();
	const FVector Start = GetActorLocation();
	const float Feeler = S(TEXT("FeelerLength"), 300.f);
	if (!IsSweepBlocked(Start, Direction, Feeler, Goal))
	{
		return Direction * Length;
	}
	// Over it: a low obstacle (a crate, a rail) is flown over by rising for a moment.
	const float Climb = S(TEXT("ClimbHeight"), 260.f);
	if (!IsSweepBlocked(Start + FVector::UpVector * Climb, Direction, Feeler, Goal))
	{
		LiftTimer = S(TEXT("LiftHold"), 1.0f);
		return Direction * Length;
	}
	// Round it: the nearest free direction, on the side that worked last.
	for (int32 Step = 1; Step <= GBFlyerLocal::SideSteps; ++Step)
	{
		for (const float Sign : { SideSign, -SideSign })
		{
			const FVector Candidate = Direction.RotateAngleAxis(Sign * Step * GBFlyerLocal::SideStepAngle, FVector::UpVector);
			if (!IsSweepBlocked(Start, Candidate, Feeler, Goal))
			{
				SideSign = Sign;
				return Candidate * Length;
			}
		}
	}
	return FVector::ZeroVector;
}

void AGBFlyerEnemy::UpdateHeight(float DeltaSeconds)
{
	UWorld* World = GetWorld();
	UCharacterMovementComponent* Move = GetCharacterMovement();
	if (!World || !Move)
	{
		return;
	}
	const FVector Loc = GetActorLocation();
	FCollisionObjectQueryParams Objects(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GBFlyerGround), false, this);

	FHitResult Hit;
	if (World->LineTraceSingleByObjectType(Hit, Loc, Loc - FVector::UpVector * GBFlyerLocal::GroundProbeDepth, Objects, Params))
	{
		GroundZ = Hit.ImpactPoint.Z;
		bGroundKnown = true;
	}

	CeilingTimer -= DeltaSeconds;
	if (CeilingTimer <= 0.f)
	{
		CeilingTimer = GBFlyerLocal::CeilingProbeInterval;
		// The whole capsule is swept up (a wide body catches an edge that a centre line misses); the capsule centre may rise up to the hit.
		const UCapsuleComponent* Capsule = GetCapsuleComponent();
		FCollisionResponseParams Response;
		Response.CollisionResponse.SetResponse(ECC_Pawn, ECR_Ignore);
		const float Radius = Capsule->GetScaledCapsuleRadius() * GBFlyerLocal::SweepRadiusScale;
		const float HalfHeight = FMath::Max(Capsule->GetScaledCapsuleHalfHeight() * GBFlyerLocal::SweepRadiusScale, Radius);
		const bool bBlocked = World->SweepSingleByChannel(Hit, Loc, Loc + FVector::UpVector * GBFlyerLocal::CeilingProbeHeight, FQuat::Identity, ECC_Pawn,
			FCollisionShape::MakeCapsule(Radius, HalfHeight), Params, Response) && !Hit.bStartPenetrating;
		CeilingZ = bBlocked ? Hit.Location.Z + GBFlyerLocal::CeilingMargin : 1e9f;
	}

	LiftTimer = FMath::Max(0.f, LiftTimer - DeltaSeconds);
	const float Bob = FMath::Sin(FlightTime * UE_TWO_PI * S(TEXT("BobHz"), 0.4f)) * S(TEXT("BobAmp"), 15.f);
	WantedHeight = S(TEXT("HoverHeight"), 250.f) + Bob + (LiftTimer > 0.f ? S(TEXT("ClimbHeight"), 260.f) : 0.f);
	const float CeilingLimit = CeilingZ - GBFlyerLocal::CeilingMargin;   // CeilingZ is already the highest capsule centre the sweep allows
	float TargetZ = FMath::Min(GroundZ + WantedHeight, CeilingLimit);
	if (bHasHomeZone)
	{
		// The zone has a ceiling and a floor too.
		const float HalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		TargetZ = FMath::Clamp(TargetZ, HomeZone.Min.Z + HalfHeight, FMath::Max(HomeZone.Max.Z - HalfHeight, HomeZone.Min.Z + HalfHeight));
	}
	WantedHeight = TargetZ - GroundZ;   // what is really wanted: a low ceiling lowers it

	const float VerticalSpeed = S(TEXT("VerticalSpeed"), 350.f);
	const float WantedVelocityZ = FMath::Clamp((TargetZ - Loc.Z) * S(TEXT("VerticalGain"), 4.f), -VerticalSpeed, VerticalSpeed);
	// The vertical move is applied as a swept offset: the movement component brakes a flyer that has no input, which would eat a velocity set here.
	VerticalVelocity = FMath::FInterpTo(VerticalVelocity, WantedVelocityZ, DeltaSeconds, 8.f);
	Move->Velocity.Z = 0.f;
	AddActorWorldOffset(FVector(0.f, 0.f, VerticalVelocity * DeltaSeconds), true);
}

float AGBFlyerEnemy::GetBodyTurnRate() const
{
	return S(TEXT("TurnRate"), 360.f);
}

bool AGBFlyerEnemy::TickCustomBehavior(AActor* Goal, float DeltaSeconds)
{
	FlightTime += DeltaSeconds;
	AttackTimer = FMath::Max(0.f, AttackTimer - DeltaSeconds);
	OrbitTimer -= DeltaSeconds;
	if (OrbitTimer <= 0.f)
	{
		OrbitTimer = Random.FRandRange(S(TEXT("OrbitFlipMin"), 3.f), FMath::Max(S(TEXT("OrbitFlipMax"), 6.f), S(TEXT("OrbitFlipMin"), 3.f)));
		OrbitSign = Random.FRand() < 0.4f ? -OrbitSign : OrbitSign;
	}

	bSightNow = bSightCached;
	LostSightTime = bSightNow ? 0.f : LostSightTime + DeltaSeconds;

	const float Aggro = Stats->GetStat(GBStat::AggroRange, 0.f);
	const float Distance = FVector::Dist(GetActorLocation(), Goal->GetActorLocation());
	(void)Aggro;
	if (State == EGBEnemyState::Idle)
	{
		SetState(EGBEnemyState::Chase);   // aware: the base class has already decided
	}

	// The whole body turns towards the player at its own rate.
	const FVector ToGoal = (Goal->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
	if (!ToGoal.IsNearlyZero())
	{
		SetActorRotation(FMath::RInterpConstantTo(GetActorRotation(), FRotator(0.f, ToGoal.Rotation().Yaw, 0.f), DeltaSeconds, GetBodyTurnRate()));
	}

	// Flight: pulled into the band and round the player, steered round whatever is in the way.
	FVector Desired = DesiredFlightDirection(Goal, Distance, bSightNow);
	// Flyers keep their distance from each other: the push is recomputed a few times a second (staggered), not every frame.
	{
		SeparationTimer -= DeltaSeconds;
		if (SeparationTimer <= 0.f)
		{
			SeparationTimer = 0.25f + Random.FRandRange(0.f, 0.1f);
			const float Separation = S(TEXT("FlyerSeparation"), 250.f);
			SeparationAway = FVector::ZeroVector;
			TArray<const AGBEnemyCharacter*> Neighbours;
			if (UGBSquadSubsystem* SeparationSquad = UGBSquadSubsystem::Get(this))
			{
				SeparationSquad->GetFlyerNeighbours(this, Neighbours);   // own squad, era filter before the loop
			}
			for (const AGBEnemyCharacter* Other : Neighbours)
			{
				const FVector Offset = GetActorLocation() - Other->GetActorLocation();
				const float Gap = Offset.Size();
				if (Gap < Separation && Gap > KINDA_SMALL_NUMBER)
				{
					SeparationAway += Offset / Gap * (1.f - Gap / Separation);
				}
			}
			SeparationAway.Z = 0.f;
		}
		Desired = (Desired + SeparationAway * 1.5f).GetClampedToMaxSize(1.f);
	}
	if (State == EGBEnemyState::Windup)
	{
		Desired *= S(TEXT("WindupMoveScale"), 0.3f);
	}
	SetGait(EGBGait::Run);
	FVector Steered = SteerAround(Desired, Goal, DeltaSeconds);
	if (bHasHomeZone && !Steered.IsNearlyZero())
	{
		// Never leaves its zone: a step that would cross the edge is turned back in (it stops at the edge).
		const FVector Loc = GetActorLocation();
		const float Speed = GetVelocity().Size2D();
		const float Brake = FMath::Max(1.f, GetCharacterMovement()->BrakingDecelerationFlying);
		const float Lookahead = GetCapsuleComponent()->GetScaledCapsuleRadius() * 3.f + 100.f + Speed * Speed / (2.f * Brake);   // room to stop
		const FVector Ahead = Loc + Steered.GetSafeNormal2D() * Lookahead;
		if (!IsInsideZone(FVector(Loc.X, Loc.Y, Ahead.Z)))
		{
			// Already outside: straight back in.
			FVector Back = ClampToZone(FVector(Loc.X, Loc.Y, Loc.Z)) - Loc;
			Back.Z = 0.f;
			Steered = Back.GetSafeNormal2D();
		}
		else if (!IsInsideZone(FVector(Ahead.X, Ahead.Y, Loc.Z)))
		{
			FVector Inward = ClampToZone(FVector(Ahead.X, Ahead.Y, Loc.Z)) - Loc;
			Inward.Z = 0.f;
			Steered = Inward.GetSafeNormal2D() * Steered.Size2D() * FMath::Clamp(Inward.Size2D() / FMath::Max(Lookahead, 1.f), 0.f, 1.f);
		}
	}
	if (!Steered.IsNearlyZero())
	{
		AddMovementInput(Steered, 1.f);
	}

	// Attack flow.
	const float AttackRange = Stats->GetStat(GBStat::AttackRange, 0.f);
	const bool bCanAttack = AttackRange > 0.f && Stats->GetStat(GBStat::AttackDamage, 0.f) > 0.f;
	switch (State)
	{
	case EGBEnemyState::Chase:
		if (bCanAttack && AttackTimer <= 0.f && Distance <= AttackRange && bSightNow)
		{
			// A flyer counts as half a shooter in the squad's attack tokens.
			UGBSquadSubsystem* TokenSquad = UGBSquadSubsystem::Get(this);
			if (!TokenSquad || TokenSquad->TryAcquireToken(this, 0.5f))
			{
				SetState(EGBEnemyState::Windup);
				OnWindupChanged(true);
			}
		}
		break;

	case EGBEnemyState::Windup:
		if (LostSightTime > GBFlyerLocal::LostSightAbort)
		{
			SetState(EGBEnemyState::Chase);
			AttackTimer = S(TEXT("RetryDelay"), 0.4f);
			OnWindupChanged(false);
			if (UGBSquadSubsystem* TokenSquad = UGBSquadSubsystem::Get(this))
			{
				TokenSquad->ReleaseToken(this);
			}
		}
		else if (StateTime >= Stats->GetStat(GBStat::AttackWindup, 0.f))
		{
			PerformAttack();
			OnWindupChanged(false);
			if (UGBSquadSubsystem* TokenSquad = UGBSquadSubsystem::Get(this))
			{
				TokenSquad->ReleaseToken(this);
			}
			if (State == EGBEnemyState::Windup)
			{
				SetState(EGBEnemyState::Recover);
			}
		}
		break;

	case EGBEnemyState::Recover:
		if (StateTime >= Stats->GetStat(GBStat::AttackInterval, 0.f))
		{
			SetState(EGBEnemyState::Chase);
			if (UGBSquadSubsystem* TokenSquad = UGBSquadSubsystem::Get(this))
			{
				TokenSquad->ReleaseToken(this);
			}
		}
		break;

	default:
		break;
	}
	return true;
}

void AGBFlyerEnemy::TickUnaware(AActor* Goal, float DeltaSeconds)
{
	FlightTime += DeltaSeconds;
	if (!bHasHomeZone)
	{
		return;
	}
	if (!bRouteBuilt)
	{
		BuildPatrolRoute();
	}
	if (bReturningToPost)
	{
		// Lost him: fly back to the post first, then guard / patrol / dormant resume.
		ReturnTimer += DeltaSeconds;
		if (FVector::Dist2D(GetActorLocation(), PostLocation) < 150.f || ReturnTimer > 40.f)
		{
			bReturningToPost = false;
		}
		else
		{
			if (State != EGBEnemyState::Idle)
			{
				SetState(EGBEnemyState::Idle);
			}
			SetGait(EGBGait::Patrol);
			const FVector Back = SteerAround((PostLocation - GetActorLocation()).GetSafeNormal2D(), nullptr, DeltaSeconds);
			if (!Back.IsNearlyZero())
			{
				AddMovementInput(Back, 1.f);
				SetActorRotation(FMath::RInterpConstantTo(GetActorRotation(), FRotator(0.f, Back.Rotation().Yaw, 0.f), DeltaSeconds, GetBodyTurnRate()));
			}
			return;
		}
	}
	if (UnawareBehavior != EGBUnawareBehavior::Patrol || PatrolRoute.Num() < 2)
	{
		Super::TickUnaware(Goal, DeltaSeconds);   // guard / dormant: stand in the air
		return;
	}
	if (State != EGBEnemyState::Idle)
	{
		SetState(EGBEnemyState::Idle);
	}
	SetGait(EGBGait::Patrol);
	if (PatrolPauseTimer > 0.f)
	{
		PatrolPauseTimer -= DeltaSeconds;
		return;
	}
	const FVector Dest = PatrolRoute[PatrolIndex];
	PatrolPointTimer += DeltaSeconds;
	if (FVector::Dist2D(GetActorLocation(), Dest) <= 160.f || PatrolPointTimer > 25.f)
	{
		PatrolPointTimer = 0.f;
		PatrolPauseTimer = AwareRandom.FRandRange(StatOr("PatrolPauseMin", 1.f), FMath::Max(StatOr("PatrolPauseMax", 3.f), StatOr("PatrolPauseMin", 1.f)));
		PatrolIndex = (PatrolIndex + 1) % PatrolRoute.Num();
		return;
	}
	const FVector Direction = (Dest - GetActorLocation()).GetSafeNormal2D();
	const FVector Steered = SteerAround(Direction, nullptr, DeltaSeconds);
	if (!Steered.IsNearlyZero())
	{
		AddMovementInput(Steered, 1.f);
		SetActorRotation(FMath::RInterpConstantTo(GetActorRotation(), FRotator(0.f, Steered.Rotation().Yaw, 0.f), DeltaSeconds, GetBodyTurnRate()));
	}
}

void AGBFlyerEnemy::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (State != EGBEnemyState::Dead && EraComponent->IsPresent())
	{
		UpdateHeight(DeltaSeconds);
	}
	UpdateVisuals(DeltaSeconds, Target.Get(), State == EGBEnemyState::Windup);
}

FGBDamageInfo AGBFlyerEnemy::MakeShotInfo() const
{
	FGBDamageInfo Info;
	if (const FGBEnemyDefinition* Def = GetDefinition())
	{
		Info.Element = Def->Element;
		Info.OnHitEffect = FName(*Def->Strings.FindRef(GBString::OnHitEffect));
	}
	Info.Amount = Stats->GetStat(GBStat::AttackDamage, 0.f);
	Info.Instigator = const_cast<AGBFlyerEnemy*>(this);
	Info.Causer = const_cast<AGBFlyerEnemy*>(this);
	Info.SourceId = EnemyId;
	return Info;
}

void AGBFlyerEnemy::OnDamagedVisual(const FGBDamageInfo& Info, float ActualDamage)
{
	if (ActualDamage > 0.f && ShotFx.HitImpact && !Info.HitLocation.IsNearlyZero())
	{
		GBEnemy3D::SpawnFx(this, ShotFx.HitImpact, Info.HitLocation, (-Info.HitDirection).Rotation());
	}
}

void AGBFlyerEnemy::HandleDeath(const FGBDamageInfo& KillingBlow)
{
	const bool bWasDead = State == EGBEnemyState::Dead;
	Super::HandleDeath(KillingBlow);
	if (!bWasDead)
	{
		bFalling = true;
		OnDeathFall(KillingBlow);
	}
}

void AGBFlyerEnemy::OnDeathFall(const FGBDamageInfo& KillingBlow)
{
	(void)KillingBlow;
	StartCapsuleFall();
}

void AGBFlyerEnemy::StartCapsuleFall()
{
	// The parent switched collision and movement off; let the capsule drop to the floor and stay there until the corpse is removed.
	UCapsuleComponent* Capsule = GetCapsuleComponent();
	SetActorEnableCollision(true);
	Capsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Capsule->SetCollisionResponseToAllChannels(ECR_Ignore);
	Capsule->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	Capsule->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->GravityScale = 1.5f;
		Move->SetMovementMode(MOVE_Falling);
	}
}

FString AGBFlyerEnemy::DescribeFlyer() const
{
	return FString::Printf(TEXT("%s art=%d state=%d height=%.0f wanted=%.0f z=%.0f ground=%.0f shots=%d"), *EnemyId.ToString(), bLookBuilt ? 1 : 0,
		static_cast<int32>(State), GetHeightAboveGround(), WantedHeight, GetActorLocation().Z, GroundZ, ShotCount);
}
