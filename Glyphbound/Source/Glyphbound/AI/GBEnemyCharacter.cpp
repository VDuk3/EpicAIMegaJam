#include "AI/GBEnemyCharacter.h"

#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"
#include "Components/BillboardComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"

#include "AI/GBSteering.h"
#include "Combat/GBCombatLibrary.h"
#include "Combat/GBProjectile.h"
#include "Combat/GBStatusEffectComponent.h"
#include "Core/GBPlaceholder.h"
#include "Core/GBStatNames.h"
#include "Data/GBDataSubsystem.h"
#include "Player/GBPlayerCharacter.h"
#include "Stats/GBAttributeComponent.h"
#include "Stats/GBStatsComponent.h"
#include "UI/GBPixelSpriteLibrary.h"
#include "World/GBEraComponent.h"
#include "World/GBPickup.h"

// Named (not anonymous) so unity builds cannot collide with another file's helpers.
namespace GBEnemyCharacterLocal
{
	const FName AttackModeMelee(TEXT("Melee"));
	const FName AttackModeRanged(TEXT("Ranged"));
	const TCHAR* const DefaultEnemyProjectileSprite = TEXT("enemy_bolt");

	// Pure layout / technical constants (not gameplay tuning).
	constexpr float GroundProbeDepth = 1000.f;     // cm below the requested spawn point searched for a floor
	constexpr float SpawnLift = 2.f;               // cm above the floor, so the capsule does not start inside it
	constexpr float ChestHeightFraction = 0.5f;    // of the capsule half height, above the center
	constexpr float SteerSweepRadiusScale = 0.9f;  // the steering sweep capsule is a bit thinner, so a wall it slides along is not "ahead"
	constexpr float StuckSpeedFraction = 0.25f;    // a slowed enemy is not stuck: the stuck distance never exceeds this share of its possible travel
	constexpr int32 MaxSteerCandidatesPerSide = 12;
	constexpr float TelegraphSpriteScale = 1.12f;
	constexpr float TelegraphTintBlend = 0.55f;    // towards white
}

AGBEnemyCharacter::AGBEnemyCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	Stats = CreateDefaultSubobject<UGBStatsComponent>(TEXT("Stats"));
	Attributes = CreateDefaultSubobject<UGBAttributeComponent>(TEXT("Attributes"));
	Attributes->Team = EGBTeam::Enemy;
	StatusEffects = CreateDefaultSubobject<UGBStatusEffectComponent>(TEXT("StatusEffects"));
	EraComponent = CreateDefaultSubobject<UGBEraComponent>(TEXT("Era"));

	Sprite = CreateDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));
	Sprite->SetupAttachment(GetCapsuleComponent());
	Sprite->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Sprite->SetCanEverAffectNavigation(false);

	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(GetCapsuleComponent());
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BodyMesh->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	BodyMesh->SetCanEverAffectNavigation(false);
	BodyMesh->SetGenerateOverlapEvents(false);

	// Pawn profile ignores Visibility; line-of-sight traces use it, so block it. Guns trace ECC_Camera: keep blocking it too.
	UCapsuleComponent* Capsule = GetCapsuleComponent();
	Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	Capsule->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Capsule->SetCollisionResponseToChannel(ECC_Camera, ECR_Block);

	AIControllerClass = AAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->bOrientRotationToMovement = true;
	}
}

AGBEnemyCharacter* AGBEnemyCharacter::SpawnEnemy(UWorld* World, FName InEnemyId, const FVector& Location, const FRotator& Rotation, EGBEraPresence Presence)
{
	if (!World)
	{
		return nullptr;
	}

	const UGBDataSubsystem* Data = UGBDataSubsystem::Get(World);
	const FGBEnemyDefinition* Def = Data ? Data->FindEnemy(InEnemyId) : nullptr;
	if (!Def)
	{
		UE_LOG(LogGB, Warning, TEXT("SpawnEnemy: unknown enemy id '%s'"), *InEnemyId.ToString());
		return nullptr;
	}

	UClass* Class = AGBEnemyCharacter::StaticClass();
	if (!Def->ActorClass.IsEmpty())
	{
		if (UClass* Loaded = LoadClass<AGBEnemyCharacter>(nullptr, *Def->ActorClass))
		{
			Class = Loaded;
		}
		else
		{
			UE_LOG(LogGB, Warning, TEXT("SpawnEnemy: class '%s' of '%s' not found, using AGBEnemyCharacter"), *Def->ActorClass, *InEnemyId.ToString());
		}
	}

	const AGBEnemyCharacter* CDO = Class->GetDefaultObject<AGBEnemyCharacter>();
	const float DefaultHalfHeight = (CDO && CDO->GetCapsuleComponent()) ? CDO->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() : 0.f;
	const float* DefHalfHeight = Def->Stats.Find(GBStat::CapsuleHalfHeight);
	const float HalfHeight = DefHalfHeight ? FMath::Max(*DefHalfHeight, 1.f) : DefaultHalfHeight;

	// Stand on the floor under the requested point, if there is one.
	FVector Ground = Location;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GBEnemySpawnGround), false);
	if (World->LineTraceSingleByObjectType(Hit, Location + FVector::UpVector * HalfHeight, Location - FVector::UpVector * GBEnemyCharacterLocal::GroundProbeDepth,
		FCollisionObjectQueryParams(ECC_WorldStatic), Params))
	{
		Ground = Hit.ImpactPoint;
	}

	const FTransform SpawnTransform(FRotator(0.f, Rotation.Yaw, 0.f), Ground + FVector::UpVector * (HalfHeight + GBEnemyCharacterLocal::SpawnLift));
	AGBEnemyCharacter* Enemy = World->SpawnActorDeferred<AGBEnemyCharacter>(Class, SpawnTransform, nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (!Enemy)
	{
		UE_LOG(LogGB, Warning, TEXT("SpawnEnemy: spawn of '%s' failed"), *InEnemyId.ToString());
		return nullptr;
	}

	Enemy->EnemyId = InEnemyId;
	Enemy->InitFromDefinition(*Def);
	Enemy->EraComponent->Presence = Presence;
	Enemy->FinishSpawning(SpawnTransform);
	return Enemy;
}

const FGBEnemyDefinition* AGBEnemyCharacter::GetDefinition() const
{
	const UGBDataSubsystem* Data = UGBDataSubsystem::Get(this);
	return Data ? Data->FindEnemy(EnemyId) : nullptr;
}

void AGBEnemyCharacter::BeginPlay()
{
	Super::BeginPlay();

	// Placed in a level by hand (not through SpawnEnemy): read the definition now.
	if (!bInitialized)
	{
		if (const FGBEnemyDefinition* Def = GetDefinition())
		{
			InitFromDefinition(*Def);
		}
		else
		{
			UE_LOG(LogGB, Warning, TEXT("%s: unknown enemy id '%s'"), *GetName(), *EnemyId.ToString());
		}
	}

	EraComponent->OnPresenceChanged.AddUObject(this, &AGBEnemyCharacter::HandlePresenceChanged);
	if (UGBDataSubsystem* Data = UGBDataSubsystem::Get(this))
	{
		Data->OnDataReloaded.AddUObject(this, &AGBEnemyCharacter::HandleDataReloaded);
	}

	SteerSide = FMath::RandBool() ? 1 : -1;
	ResetStuck();
}

float AGBEnemyCharacter::GetGameValue(FName Key, float DefaultValue) const
{
	const UGBDataSubsystem* Data = UGBDataSubsystem::Get(this);
	return Data ? Data->GetGameValue(Key, DefaultValue) : DefaultValue;
}

void AGBEnemyCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UGBDataSubsystem* Data = UGBDataSubsystem::Get(this))
	{
		Data->OnDataReloaded.RemoveAll(this);
	}
	Super::EndPlay(EndPlayReason);
}

void AGBEnemyCharacter::InitFromDefinition(const FGBEnemyDefinition& Definition)
{
	bInitialized = true;
	bPixelArtLook = Definition.bPixelArt;
	AttackModeName = Definition.AttackMode;

	Stats->SetBaseStats(Definition.Stats);
	Attributes->InitializeFromStats();

	UCapsuleComponent* Capsule = GetCapsuleComponent();
	const float Radius = FMath::Max(Stats->GetStat(GBStat::CapsuleRadius, Capsule->GetUnscaledCapsuleRadius()), 1.f);
	const float HalfHeight = FMath::Max(Stats->GetStat(GBStat::CapsuleHalfHeight, Capsule->GetUnscaledCapsuleHalfHeight()), Radius);
	Capsule->SetCapsuleSize(Radius, HalfHeight);

	if (Definition.bPixelArt)
	{
		const FString SpriteId = Definition.Strings.FindRef(GBString::Sprite);
		if (UGBPixelSpriteLibrary* Library = UGBPixelSpriteLibrary::Get(this))
		{
			Library->SetupBillboard(Sprite, FName(*SpriteId), 2.f * HalfHeight);
		}
		SpriteBaseScale = Sprite->GetRelativeScale3D();
		Sprite->SetVisibility(true);
		BodyMesh->SetVisibility(false);
	}
	else
	{
		BodyColor = UGBDataSubsystem::ParseColor(Definition.Strings.FindRef(GBString::Color), FLinearColor::Gray);
		GBPlaceholder::SetupShape(BodyMesh, EGBShape::Cylinder, FVector(2.f * Radius, 2.f * Radius, 2.f * HalfHeight), BodyColor);
		BodyMesh->SetVisibility(true);
		Sprite->SetVisibility(false);
	}

	Stats->OnStatsChanged.RemoveAll(this);
	Stats->OnStatsChanged.AddUObject(this, &AGBEnemyCharacter::ApplyMovementStats);
	Attributes->OnDeath.RemoveAll(this);
	Attributes->OnDeath.AddUObject(this, &AGBEnemyCharacter::HandleDeath);
	Attributes->OnDamaged.RemoveAll(this);
	Attributes->OnDamaged.AddUObject(this, &AGBEnemyCharacter::HandleDamaged);

	ApplyMovementStats();
}

void AGBEnemyCharacter::HandleDataReloaded()
{
	if (const FGBEnemyDefinition* Def = GetDefinition())
	{
		AttackModeName = Def->AttackMode;
		Stats->SetBaseStats(Def->Stats);
	}
}

void AGBEnemyCharacter::ApplyMovementStats()
{
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->MaxWalkSpeed = FMath::Max(0.f, Stats->GetStat(GBStat::MoveSpeed, Move->MaxWalkSpeed));
	}
}

AActor* AGBEnemyCharacter::FindTarget() const
{
	AGBPlayerCharacter* Player = AGBPlayerCharacter::Get(this);
	if (Player && UGBCombatLibrary::IsAlive(Player) && UGBCombatLibrary::AreHostile(this, Player))
	{
		return Player;
	}
	return nullptr;
}

bool AGBEnemyCharacter::IsRanged() const
{
	return AttackModeName.Equals(GBEnemyCharacterLocal::AttackModeRanged.ToString(), ESearchCase::IgnoreCase);
}

void AGBEnemyCharacter::SetState(EGBEnemyState NewState)
{
	if (State == EGBEnemyState::Windup && NewState != EGBEnemyState::Windup)
	{
		SetTelegraph(false);
	}
	State = NewState;
	StateTime = 0.f;
	if (NewState == EGBEnemyState::Windup)
	{
		SetTelegraph(true);
	}
}

void AGBEnemyCharacter::SetTelegraph(bool bOn)
{
	if (bPixelArtLook)
	{
		Sprite->SetRelativeScale3D(bOn ? SpriteBaseScale * GBEnemyCharacterLocal::TelegraphSpriteScale : SpriteBaseScale);
	}
	else
	{
		const FLinearColor Color = bOn ? BodyColor + (FLinearColor::White - BodyColor) * GBEnemyCharacterLocal::TelegraphTintBlend : BodyColor;
		GBPlaceholder::SetShapeColor(BodyMesh, Color);
	}
}

void AGBEnemyCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (State == EGBEnemyState::Dead || !EraComponent->IsPresent())
	{
		return;
	}

	StateTime += DeltaSeconds;
	NavRetryTimer = FMath::Max(0.f, NavRetryTimer - DeltaSeconds);
	SteerSideHoldTimer = FMath::Max(0.f, SteerSideHoldTimer - DeltaSeconds);
	GoAroundTimer = FMath::Max(0.f, GoAroundTimer - DeltaSeconds);
	bWantsToMove = false;

	AActor* Goal = FindTarget();
	Target = Goal;
	if (!Goal)
	{
		if (State != EGBEnemyState::Idle)
		{
			StopMoving();
			SetState(EGBEnemyState::Idle);
		}
		bProvoked = false;
		bRepositioning = false;
		ResetStuck();
		return;
	}

	const float AggroRange = Stats->GetStat(GBStat::AggroRange, 0.f);
	const float AttackRange = Stats->GetStat(GBStat::AttackRange, 0.f);
	const bool bCanAttack = AttackRange > 0.f && Stats->GetStat(GBStat::AttackDamage, 0.f) > 0.f;
	const float Distance = FVector::Dist(GetActorLocation(), Goal->GetActorLocation());

	switch (State)
	{
	case EGBEnemyState::Idle:
		// AggroRange <= 0: never acts (training dummy), even when hit.
		if (AggroRange > 0.f && (bProvoked || Distance <= AggroRange))
		{
			SetState(EGBEnemyState::Chase);
		}
		break;

	case EGBEnemyState::Chase:
		// Line of sight for melee too: an orc on the other side of a low wall keeps going around it instead of swinging at stone.
		if (bCanAttack && Distance <= AttackRange && HasLineOfSight(Goal))
		{
			StopMoving();
			FaceTowards(Goal);
			SetState(EGBEnemyState::Windup);
		}
		else
		{
			MoveTowards(Goal, DeltaSeconds);
		}
		break;

	case EGBEnemyState::Windup:
		// A shot needs a clear line: the player stepped behind cover, so reposition instead of shooting the wall.
		if (IsRanged() && !HasLineOfSight(Goal))
		{
			SetState(EGBEnemyState::Chase);
			MoveTowards(Goal, DeltaSeconds);
			break;
		}
		FaceTowards(Goal);
		if (StateTime >= Stats->GetStat(GBStat::AttackWindup, 0.f))
		{
			PerformAttack();
			if (State == EGBEnemyState::Windup)
			{
				SetState(EGBEnemyState::Recover);
			}
		}
		break;

	case EGBEnemyState::Recover:
		// Ranged enemies do not wait out the interval behind a wall: they move until they see the player again.
		if (IsRanged() && !HasLineOfSight(Goal))
		{
			MoveTowards(Goal, DeltaSeconds);
			bRepositioning = true;
		}
		else
		{
			if (bRepositioning)
			{
				StopMoving();
				bRepositioning = false;
			}
			FaceTowards(Goal);
		}
		if (StateTime >= Stats->GetStat(GBStat::AttackInterval, 0.f))
		{
			bRepositioning = false;
			SetState(EGBEnemyState::Chase);
		}
		break;

	default:
		break;
	}

	UpdateStuck(DeltaSeconds);
}

void AGBEnemyCharacter::MoveTowards(AActor* Goal, float DeltaSeconds)
{
	if (!Goal || GetCharacterMovement()->MaxWalkSpeed <= 0.f)
	{
		return;
	}
	bWantsToMove = true;

	bool bPathing = false;
	AAIController* AI = Cast<AAIController>(GetController());
	if (AI && NavRetryTimer <= 0.f)
	{
		const UNavigationSystemV1* Nav = UNavigationSystemV1::GetCurrent(GetWorld());
		if (Nav && Nav->GetDefaultNavDataInstance() != nullptr)
		{
			const UPathFollowingComponent* PathFollowing = AI->GetPathFollowingComponent();
			if (AI->GetMoveStatus() != EPathFollowingStatus::Idle && PathFollowing && PathFollowing->GetMoveGoal() == Goal)
			{
				bPathing = true;
			}
			else
			{
				// Melee stops a bit inside its reach; ranged keeps closing in until it has line of sight.
				const float Acceptance = IsRanged() ? 0.f : Stats->GetStat(GBStat::AttackRange, 0.f) * 0.5f;
				const EPathFollowingRequestResult::Type Result = AI->MoveToActor(Goal, Acceptance, true, true, false);
				bPathing = Result == EPathFollowingRequestResult::RequestSuccessful;
				if (Result == EPathFollowingRequestResult::Failed)
				{
					NavRetryTimer = GetGameValue(GBEnemyKey::NavRetryDelay, 1.f);
				}
			}
		}
	}

	if (!bPathing)
	{
		if (AI && AI->GetMoveStatus() != EPathFollowingStatus::Idle)
		{
			AI->StopMovement();
		}
		SteerTowards(Goal);
	}
	bSteering = !bPathing;
	if (bPathing)
	{
		NavMoveTime += DeltaSeconds;
	}
	else
	{
		SteerMoveTime += DeltaSeconds;
	}
}

void AGBEnemyCharacter::SteerTowards(const AActor* Goal)
{
	const FVector ToGoal = (Goal->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
	if (ToGoal.IsNearlyZero())
	{
		return;
	}

	const float Feeler = FMath::Max(GetGameValue(GBEnemyKey::SteerFeelerLength, 150.f), 1.f);
	const float AngleStep = FMath::Clamp(GetGameValue(GBEnemyKey::SteerAngleStep, 25.f), 1.f, 180.f);
	const int32 PerSide = FMath::Clamp(FMath::RoundToInt(GetGameValue(GBEnemyKey::SteerCandidatesPerSide, 6.f)), 1, GBEnemyCharacterLocal::MaxSteerCandidatesPerSide);

	TArray<bool, TInlineAllocator<2 * GBEnemyCharacterLocal::MaxSteerCandidatesPerSide + 1>> Blocked;
	Blocked.SetNum(2 * PerSide + 1);
	for (int32 Offset = -PerSide; Offset <= PerSide; ++Offset)
	{
		Blocked[Offset + PerSide] = IsDirectionBlocked(ToGoal.RotateAngleAxis(Offset * AngleStep, FVector::UpVector), Feeler, Goal);
	}

	const EGBSteerMode Mode = GoAroundTimer > 0.f ? EGBSteerMode::GoAround
		: (SteerSideHoldTimer > 0.f ? EGBSteerMode::HoldSide : EGBSteerMode::Free);
	const TOptional<int32> Choice = GBSteering::ChooseOffset(Blocked, Mode, SteerSide);
	if (!Choice.IsSet())
	{
		// Boxed in on every side but the back: step back; the stuck check then commits to a side.
		AddMovementInput(-ToGoal, 1.f);
		return;
	}
	if (Choice.GetValue() != 0)
	{
		SteerSide = Choice.GetValue() > 0 ? 1 : -1;
		SteerSideHoldTimer = FMath::Max(SteerSideHoldTimer, GetGameValue(GBEnemyKey::SteerSideHoldTime, 1.f));
	}
	AddMovementInput(ToGoal.RotateAngleAxis(Choice.GetValue() * AngleStep, FVector::UpVector), 1.f);
}

bool AGBEnemyCharacter::IsDirectionBlocked(const FVector& Direction, float Distance, const AActor* Goal) const
{
	UWorld* World = GetWorld();
	const UCapsuleComponent* Capsule = GetCapsuleComponent();
	const UCharacterMovementComponent* Move = GetCharacterMovement();
	if (!World || !Capsule || !Move)
	{
		return false;
	}

	// Bottom lifted by the step height (the character walks over anything lower), top unchanged, a bit thinner.
	const float StepHeight = FMath::Max(Move->MaxStepHeight, 0.f);
	const float Radius = Capsule->GetScaledCapsuleRadius() * GBEnemyCharacterLocal::SteerSweepRadiusScale;
	const float HalfHeight = FMath::Max(Capsule->GetScaledCapsuleHalfHeight() - StepHeight * 0.5f, Radius);
	const FVector Start = GetActorLocation() + FVector::UpVector * (StepHeight * 0.5f);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(GBEnemySteer), false, this);
	if (Goal)
	{
		Params.AddIgnoredActor(Goal);
	}
	// Other enemies are not obstacles to plan around: they move, and the capsules push each other aside.
	FCollisionResponseParams Response;
	Response.CollisionResponse.SetResponse(ECC_Pawn, ECR_Ignore);

	FHitResult Hit;
	if (!World->SweepSingleByChannel(Hit, Start, Start + Direction * Distance, FQuat::Identity, ECC_Pawn,
		FCollisionShape::MakeCapsule(Radius, HalfHeight), Params, Response))
	{
		return false;
	}
	if (Hit.bStartPenetrating)
	{
		// Already touching something: only directions into it are blocked.
		return FVector::DotProduct(Hit.Normal, Direction) < 0.f;
	}
	// A ramp or a slope it can walk up is not an obstacle.
	return Hit.ImpactNormal.Z < Move->GetWalkableFloorZ();
}

void AGBEnemyCharacter::ResetStuck()
{
	StuckTimer = 0.f;
	StuckOrigin = GetActorLocation();
}

void AGBEnemyCharacter::UpdateStuck(float DeltaSeconds)
{
	if (!bWantsToMove)
	{
		ResetStuck();
		return;
	}

	StuckTimer += DeltaSeconds;
	const float Window = FMath::Max(GetGameValue(GBEnemyKey::StuckTime, 0.8f), 0.1f);
	if (StuckTimer < Window)
	{
		return;
	}

	const float Moved = FVector::Dist2D(GetActorLocation(), StuckOrigin);
	const float Speed = GetCharacterMovement() ? GetCharacterMovement()->MaxWalkSpeed : 0.f;
	const float Threshold = FMath::Min(GetGameValue(GBEnemyKey::StuckDistance, 50.f), Speed * Window * GBEnemyCharacterLocal::StuckSpeedFraction);
	ResetStuck();
	if (Moved >= Threshold)
	{
		return;
	}

	++StuckCount;
	const float GoAround = FMath::Max(GetGameValue(GBEnemyKey::GoAroundTime, 1.5f), 0.f);
	if (!bSteering)
	{
		// The navmesh path is not getting anywhere (snagged on a corner, the mesh still building): steer for a while.
		NavRetryTimer = FMath::Max(GetGameValue(GBEnemyKey::NavRetryDelay, 1.f), GoAround);
		if (AAIController* AI = Cast<AAIController>(GetController()))
		{
			AI->StopMovement();
		}
	}
	else if (GoAroundTimer > 0.f)
	{
		// Already going around this way and still stuck (a dead end): try the other side.
		SteerSide = -SteerSide;
	}
	GoAroundTimer = GoAround;
	SteerSideHoldTimer = GoAround;
	UE_LOG(LogGB, Verbose, TEXT("%s stuck (moved %.0f cm): going around on side %d"), *GetName(), Moved, SteerSide);
}

FString AGBEnemyCharacter::DescribeMovement() const
{
	return FString::Printf(TEXT("navTime=%.1f steerTime=%.1f stuckEvents=%d side=%d goAround=%.1f state=%d"),
		NavMoveTime, SteerMoveTime, StuckCount, SteerSide, GoAroundTimer, static_cast<int32>(State));
}

void AGBEnemyCharacter::FellOutOfWorld(const UDamageType& DamageType)
{
	if (State == EGBEnemyState::Dead)
	{
		Super::FellOutOfWorld(DamageType);
		return;
	}

	// Die like any other death so the encounter counts it; no drops down there.
	bFellOutOfWorld = true;
	FGBDamageInfo Info;
	Info.Amount = Attributes->GetMaxHealth() + Attributes->GetHealth() + 1.f;
	Info.SourceId = FName(TEXT("FellOutOfWorld"));
	Attributes->SetInvulnerable(false);
	Attributes->ApplyDamage(Info);
	if (State != EGBEnemyState::Dead)
	{
		// The hit was cancelled (a shield, DamageTaken 0): it still cannot come back from below the world.
		HandleDeath(Info);
	}
	SetActorHiddenInGame(true);
}

void AGBEnemyCharacter::StopMoving()
{
	if (AController* C = GetController())
	{
		C->StopMovement();
	}
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->StopMovementImmediately();
	}
}

void AGBEnemyCharacter::FaceTowards(const AActor* Goal)
{
	if (!Goal)
	{
		return;
	}
	const FVector Direction = (Goal->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
	if (!Direction.IsNearlyZero())
	{
		SetActorRotation(FRotator(0.f, Direction.Rotation().Yaw, 0.f));
	}
}

FVector AGBEnemyCharacter::GetChestLocation() const
{
	return GetActorLocation() + FVector::UpVector * (GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * GBEnemyCharacterLocal::ChestHeightFraction);
}

bool AGBEnemyCharacter::HasLineOfSight(const AActor* Goal) const
{
	if (!Goal || !GetWorld())
	{
		return false;
	}
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GBEnemyLineOfSight), false, this);
	if (!GetWorld()->LineTraceSingleByChannel(Hit, GetChestLocation(), Goal->GetActorLocation(), ECC_Visibility, Params))
	{
		return true;
	}
	// Allies in the way do not block: enemy shots ignore the enemy team.
	const AActor* HitActor = Hit.GetActor();
	return HitActor == Goal || (HitActor && !UGBCombatLibrary::AreHostile(this, HitActor) && UGBCombatLibrary::GetTeam(HitActor) == EGBTeam::Enemy);
}

void AGBEnemyCharacter::PerformAttack()
{
	AActor* Goal = Target.Get();
	const FGBEnemyDefinition* Def = GetDefinition();
	if (!Goal || !Def || !GetWorld())
	{
		return;
	}

	const float Damage = Stats->GetStat(GBStat::AttackDamage, 0.f);
	const float AttackRange = Stats->GetStat(GBStat::AttackRange, 0.f);

	FGBDamageInfo Info;
	Info.Amount = Damage;
	Info.Element = Def->Element;
	Info.Instigator = this;
	Info.Causer = this;
	Info.SourceId = EnemyId;
	Info.OnHitEffect = FName(*Def->Strings.FindRef(GBString::OnHitEffect));

	const FName Mode(*Def->AttackMode);
	if (Mode == GBEnemyCharacterLocal::AttackModeMelee)
	{
		const UGBDataSubsystem* Data = UGBDataSubsystem::Get(this);
		const float Tolerance = Data ? Data->GetGameValue(GBEnemyKey::MeleeReachTolerance, 1.2f) : 1.2f;
		const FVector ToGoal = Goal->GetActorLocation() - GetActorLocation();
		if (ToGoal.Size() <= AttackRange * Tolerance && HasLineOfSight(Goal))
		{
			Info.HitLocation = Goal->GetActorLocation();
			Info.HitDirection = ToGoal.GetSafeNormal();
			UGBCombatLibrary::DealDamage(Goal, Info);
		}
	}
	else if (Mode == GBEnemyCharacterLocal::AttackModeRanged)
	{
		FGBProjectileConfig Config;
		Config.Damage = Info;
		Config.Damage.Causer = nullptr;
		Config.Speed = Stats->GetStat(GBStat::ProjectileSpeed, Config.Speed);
		Config.Radius = Stats->GetStat(GBStat::ProjectileRadius, Config.Radius);
		Config.MaxRange = Stats->GetStat(GBStat::AggroRange, 0.f) * 1.5f;
		Config.bPixelArt = Def->bPixelArt;
		if (Def->bPixelArt)
		{
			const FString SpriteId = Def->Strings.FindRef(GBString::ProjectileSprite);
			Config.SpriteId = FName(SpriteId.IsEmpty() ? GBEnemyCharacterLocal::DefaultEnemyProjectileSprite : *SpriteId);
		}
		else
		{
			Config.Color = UGBDataSubsystem::ParseColor(Def->Strings.FindRef(GBEnemyKey::ProjectileColor),
				GBPlaceholder::ElementColor(static_cast<uint8>(Def->Element)));
		}

		const FVector Forward = GetActorForwardVector();
		const FVector Origin = GetChestLocation() + Forward * (GetCapsuleComponent()->GetScaledCapsuleRadius() + Config.Radius);
		FVector Direction = Goal->GetActorLocation() - Origin;
		if (Direction.IsNearlyZero())
		{
			Direction = Forward;
		}
		AGBProjectile::SpawnProjectile(GetWorld(), Config, Origin, Direction, this);
	}
	else
	{
		UE_LOG(LogGB, Warning, TEXT("%s: unknown AttackMode '%s'"), *EnemyId.ToString(), *Def->AttackMode);
	}
}

bool AGBEnemyCharacter::IsHeadshotLocation(const FVector& HitLocation) const
{
	const float HalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const float FeetZ = GetActorLocation().Z - HalfHeight;
	return UGBCombatLibrary::IsHeadshot(HitLocation.Z, FeetZ, 2.f * HalfHeight, Stats->GetStat(GBStat::HeadFraction, 0.f));
}

void AGBEnemyCharacter::HandleDamaged(const FGBDamageInfo& Info, float ActualDamage)
{
	if (State != EGBEnemyState::Dead)
	{
		bProvoked = true;
	}
}

void AGBEnemyCharacter::HandlePresenceChanged()
{
	ResetStuck();
	if (!EraComponent->IsPresent() && State != EGBEnemyState::Dead)
	{
		StopMoving();
		bRepositioning = false;
		if (State == EGBEnemyState::Windup)
		{
			SetState(EGBEnemyState::Chase);
		}
	}
}

void AGBEnemyCharacter::HandleDeath(const FGBDamageInfo& KillingBlow)
{
	if (State == EGBEnemyState::Dead)
	{
		return;
	}
	SetState(EGBEnemyState::Dead);

	StopMoving();
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->DisableMovement();
	}
	DetachFromControllerPendingDestroy();
	SetActorEnableCollision(false);

	UWorld* World = GetWorld();
	const FGBEnemyDefinition* Def = GetDefinition();
	if (Def && !bFellOutOfWorld)
	{
		AGBPickup::SpawnDrops(World, Def->Drops, GetActorLocation());
	}

	OnDied.Broadcast(this);

	const UGBDataSubsystem* Data = UGBDataSubsystem::Get(this);
	const float CorpseTime = Data ? Data->GetGameValue(GBEnemyKey::CorpseTime, 1.5f) : 1.5f;
	// SetLifeSpan(0) would mean "forever"; keep a tiny minimum instead of destroying inside the damage call.
	SetLifeSpan(FMath::Max(CorpseTime, 0.05f));
}
