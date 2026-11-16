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

#include "AI/GBEngage.h"
#include "AI/GBEncounter.h"
#include "AI/GBSquad.h"
#include "AI/GBSteering.h"
#include "AI/GBTactics.h"
#include "Combat/GBCombatLibrary.h"
#include "Combat/GBElementalStatusComponent.h"
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
namespace GBAudio { bool Play(const UObject* Ctx, FName Key, const FVector* Loc, float PitchMul, float VolMul); void SpawnLoop(const UObject* Ctx, FName Key, const FVector& Loc); bool IsFutureMap(const UObject* Ctx); void MenuPing(const UObject* Ctx); void UpdateMusic(UWorld* World); }

// Named (not anonymous) so unity builds cannot collide with another file's helpers.
namespace GBEnemyCharacterLocal
{
	const FName AttackModeMelee(TEXT("Melee"));
	const FName AttackModeRanged(TEXT("Ranged"));
	const TCHAR* const DefaultEnemyProjectileSprite = TEXT("enemy_bolt");

	// Pure layout / technical constants (not gameplay tuning).
	constexpr float GroundProbeDepth = 1000.f;     // cm below the requested spawn point searched for a floor
	constexpr float SpawnLift = 2.f;               // cm above the floor, so the capsule does not start inside it
	constexpr int32 MaxSteerCandidatesPerSide = 12;
	constexpr float TelegraphSpriteScale = 1.12f;
	constexpr float TelegraphTintBlend = 0.55f;    // towards white
	constexpr float PatrolArrivalRadius = 80.f;    // cm: a patrol point this close is reached
	constexpr float NavProjectExtentXY = 250.f;
	constexpr float NavProjectExtentZ = 400.f;
	constexpr float PatrolPointTimeout = 25.f;     // s on one patrol point before it is skipped
	constexpr float SquadAlertLifetime = 60.f;     // s a squad counts as recently alerted
	constexpr float ZoneMoveTolerance = 80.f;      // cm a path or a step may leave the zone before it counts as leaving it
	constexpr float ZoneClampDistance = 250.f;     // cm outside the zone at which an enemy is put back
}

AGBEnemyCharacter::AGBEnemyCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	Stats = CreateDefaultSubobject<UGBStatsComponent>(TEXT("Stats"));
	Attributes = CreateDefaultSubobject<UGBAttributeComponent>(TEXT("Attributes"));
	Attributes->Team = EGBTeam::Enemy;
	StatusEffects = CreateDefaultSubobject<UGBStatusEffectComponent>(TEXT("StatusEffects"));
	ElementalStatus = CreateDefaultSubobject<UGBElementalStatusComponent>(TEXT("ElementalStatus"));
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
	if (ElementalStatus)
	{
		ElementalStatus->OnStatusStarted.AddUObject(this, &AGBEnemyCharacter::HandleElementalStatusStarted);
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
	if (ElementalStatus)
	{
		ElementalStatus->OnStatusStarted.RemoveAll(this);
	}
	if (UGBSquadSubsystem* Squad = UGBSquadSubsystem::Get(this))
	{
		Squad->Leave(this);
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

	if (UsesAwareness())
	{
		if (UCharacterMovementComponent* Move = GetCharacterMovement())
		{
			Move->MaxAcceleration = FMath::Max(1.f, StatOr("Acceleration", 1000.f));
			Move->BrakingDecelerationWalking = FMath::Max(1.f, StatOr("BrakingDeceleration", 1400.f));
			Move->bRequestedMoveUseAcceleration = true;
			Move->bUseRVOAvoidance = StatOr("UseAvoidance", 1.f) > 0.5f;
			Move->AvoidanceConsiderationRadius = Radius * 3.f + 80.f;
			Move->AvoidanceWeight = 0.5f;
		}
		AwareRandom.Initialize(static_cast<int32>(GetUniqueID()) * 7919 ^ FMath::Rand());
		SightCacheTimer = AwareRandom.FRandRange(0.f, 0.15f);
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

float AGBEnemyCharacter::StatOr(const TCHAR* Key, float Default) const
{
	return Stats ? Stats->GetStat(FName(Key), Default) : Default;
}

float AGBEnemyCharacter::GetGaitSpeed(EGBGait InGait) const
{
	const UCharacterMovementComponent* Move = GetCharacterMovement();
	const float Fallback = Move ? Move->MaxWalkSpeed : 0.f;
	const float Modified = Stats->GetStat(GBStat::MoveSpeed, Fallback);
	if (!UsesAwareness() || InGait == EGBGait::Run)
	{
		return FMath::Max(0.f, Modified);
	}
	// The slows are modifiers of MoveSpeed: all three gaits scale by the same ratio.
	float Base = Modified;
	if (const FGBEnemyDefinition* Def = GetDefinition())
	{
		if (const float* Value = Def->Stats.Find(GBStat::MoveSpeed))
		{
			Base = *Value;
		}
	}
	const float Ratio = Base > 1.f ? Modified / Base : 1.f;
	const float Speed = InGait == EGBGait::Patrol ? Stats->GetStat(FName(TEXT("PatrolSpeed")), Base * 0.4f) : Stats->GetStat(FName(TEXT("CombatSpeed")), Base * 0.65f);
	return FMath::Max(0.f, Speed * Ratio);
}

void AGBEnemyCharacter::SetGait(EGBGait InGait)
{
	if (InGait == Gait)
	{
		return;
	}
	Gait = InGait;
	ApplyMovementStats();
}

void AGBEnemyCharacter::ApplyMovementStats()
{
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->MaxWalkSpeed = GetGaitSpeed(Gait);
	}
}

void AGBEnemyCharacter::TurnYawTowards(float TargetYaw, float DeltaSeconds, float RateDegreesPerSecond)
{
	const FRotator Current = GetActorRotation();
	const FRotator Wanted(0.f, TargetYaw, 0.f);
	SetActorRotation(FMath::RInterpConstantTo(Current, Wanted, DeltaSeconds, FMath::Max(RateDegreesPerSecond, 1.f)));
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
	if (NewState != State && State != EGBEnemyState::Dead)
	{
		const FVector SoundAt = GetActorLocation();
		if (NewState == EGBEnemyState::Windup)
		{
			GBAudio::Play(this, FName(GBAudio::IsFutureMap(this) ? TEXT("enemy_attack_future") : TEXT("enemy_attack_past")), &SoundAt, 1.f, 1.f);
		}
		else if (NewState == EGBEnemyState::Chase && State == EGBEnemyState::Idle)
		{
			GBAudio::Play(this, FName(GBAudio::IsFutureMap(this) ? TEXT("enemy_alert_future") : TEXT("enemy_alert_past")), &SoundAt, 1.f, 1.f);
		}
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

	if (ControlLockTime > 0.f)
	{
		ControlLockTime -= DeltaSeconds;
		ResetStuck();
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

	if (UsesAwareness() && !TickAwareness(Goal, DeltaSeconds))
	{
		UpdateStuck(DeltaSeconds);
		return;
	}

	if (TickCustomBehavior(Goal, DeltaSeconds))
	{
		UpdateStuck(DeltaSeconds);
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
		if (bCanAttack && Distance <= AttackRange && HasLineOfSight(Goal) && TryStartMelee())
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
		// Opt-in by data (RecoverPause stat, the castle melee ids): melee does not stand out its recovery while the player
		// backs away (architect, 2026-11-15: reading the code, they were easy to kite); after the pause, past its reach it
		// keeps after him and swings as soon as the interval ends. Only on the ground: a lunge flies in Recover.
		else if (!IsRanged() && bPixelArtLook && StatOr(TEXT("RecoverPause"), -1.f) >= 0.f && StateTime >= StatOr(TEXT("RecoverPause"), -1.f)
			&& GetCharacterMovement()->IsMovingOnGround()
			&& Distance > AttackRange * GetGameValue(GBEnemyKey::MeleeAcceptanceFraction, 0.5f) * 1.6f)
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

	// A slot round the player, or a player outside the zone (the enemy stops at the edge): go to a clamped point instead of the actor.
	FVector SlotPoint;
	const bool bSlot = GetMoveGoalOverride(SlotPoint);
	if (bSlot && FVector::DistSquared2D(SlotPoint, GetActorLocation()) < 1.f)
	{
		// The override's "hold here": stand still and face him (no move request, so the stuck check does not fire).
		AAIController* HoldAI = Cast<AAIController>(GetController());
		if (HoldAI && HoldAI->GetMoveStatus() != EPathFollowingStatus::Idle)
		{
			HoldAI->StopMovement();
		}
		if (GetVelocity().Size2D() > 10.f)
		{
			StopMoving();
		}
		const FVector To = (Goal->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
		if (!To.IsNearlyZero())
		{
			TurnYawTowards(To.Rotation().Yaw, DeltaSeconds, StatOr(TEXT("TurnRate"), 420.f));
		}
		return;
	}
	if (bSlot || (bHasHomeZone && !IsInsideZone(Goal->GetActorLocation())))
	{
		const float Acceptance = IsRanged() ? 20.f : Stats->GetStat(GBStat::AttackRange, 0.f) * GetGameValue(GBEnemyKey::MeleeAcceptanceFraction, 0.5f);
		MoveToZonePoint(bSlot ? SlotPoint : Goal->GetActorLocation(), Acceptance, Goal, DeltaSeconds);
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
				const float Acceptance = IsRanged() ? 0.f : Stats->GetStat(GBStat::AttackRange, 0.f) * GetGameValue(GBEnemyKey::MeleeAcceptanceFraction, 0.5f);
				const EPathFollowingRequestResult::Type Result = AI->MoveToActor(Goal, Acceptance, true, true, false);
				bPathing = Result == EPathFollowingRequestResult::RequestSuccessful;
				if (bPathing && bHasHomeZone && !PathStaysInZone(AI))
				{
					AI->StopMovement();   // the common validation: a path that leaves the zone is not walked
					bPathing = false;
					NavRetryTimer = GetGameValue(GBEnemyKey::NavRetryDelay, 1.f) * 0.5f;
				}
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
	SteerTowardsPoint(Goal->GetActorLocation(), Goal);
}

bool AGBEnemyCharacter::PathStaysInZone(const AAIController* AI) const
{
	if (!bHasHomeZone || !AI || !AI->GetPathFollowingComponent())
	{
		return true;
	}
	const FNavPathSharedPtr Path = AI->GetPathFollowingComponent()->GetPath();
	if (!Path.IsValid())
	{
		return true;
	}
	const FBox Grown = HomeZone.ExpandBy(GBEnemyCharacterLocal::ZoneMoveTolerance);
	for (const FNavPathPoint& PathPoint : Path->GetPathPoints())
	{
		if (!Grown.IsInside(PathPoint.Location))
		{
			return false;
		}
	}
	return true;
}

void AGBEnemyCharacter::MoveToZonePoint(const FVector& Point, float Acceptance, const AActor* IgnoreActor, float DeltaSeconds)
{
	bWantsToMove = true;
	const FVector Dest = ClampToZone(Point);
	NavRepathTimer -= DeltaSeconds;

	bool bPathing = false;
	AAIController* AI = Cast<AAIController>(GetController());
	if (AI && NavRetryTimer <= 0.f)
	{
		const UNavigationSystemV1* Nav = UNavigationSystemV1::GetCurrent(GetWorld());
		if (Nav && Nav->GetDefaultNavDataInstance() != nullptr)
		{
			if (AI->GetMoveStatus() != EPathFollowingStatus::Idle && NavRepathTimer > 0.f)
			{
				bPathing = true;
			}
			else
			{
				const EPathFollowingRequestResult::Type Result = AI->MoveToLocation(Dest, Acceptance, true, true, true, true);
				NavRepathTimer = GetGameValue(FName(TEXT("EnemyNavRepathInterval")), 0.5f);
				bPathing = Result != EPathFollowingRequestResult::Failed;
				if (bPathing && bHasHomeZone && !PathStaysInZone(AI))
				{
					// A path between two points inside the zone may still go round an obstacle outside it: such a path is thrown away.
					AI->StopMovement();
					bPathing = false;
				}
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
		SteerTowardsPoint(Dest, IgnoreActor);
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

void AGBEnemyCharacter::SteerTowardsPoint(const FVector& Point, const AActor* Goal)
{
	const FVector ToGoal = (Point - GetActorLocation()).GetSafeNormal2D();
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
		// Boxed in on every side but the back: step back if it is free, otherwise stay in place this tick.
		const FVector Back = -ToGoal;
		if (IsDirectionBlocked(Back, Feeler, Goal))
		{
			StopMoving();
			return;
		}
		AddMovementInput(Back, 1.f);
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
	if (bHasHomeZone)
	{
		// A direction that leaves the zone (and does not bring it nearer to it) is as blocked as a wall.
		const FBox Grown = HomeZone.ExpandBy(GBEnemyCharacterLocal::ZoneMoveTolerance);
		const FVector Loc = GetActorLocation();
		const FVector End = Loc + Direction * Distance;
		if (!Grown.IsInside(End))
		{
			const float Now = FVector::DistSquared(Loc, GBTactics::ClampToBox(Grown, Loc));
			const float Then = FVector::DistSquared(End, GBTactics::ClampToBox(Grown, End));
			if (Grown.IsInside(Loc) || Then >= Now)
			{
				return true;
			}
		}
	}

	// Bottom lifted by the step height (the character walks over anything lower), top unchanged, a bit thinner.
	const float StepHeight = FMath::Max(Move->MaxStepHeight, 0.f);
	const float Radius = Capsule->GetScaledCapsuleRadius() * GetGameValue(GBEnemyKey::SteerSweepRadiusScale, 0.9f);
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
	if (World->SweepSingleByChannel(Hit, Start, Start + Direction * Distance, FQuat::Identity, ECC_Pawn,
		FCollisionShape::MakeCapsule(Radius, HalfHeight), Params, Response))
	{
		if (Hit.bStartPenetrating)
		{
			// Already touching something: only directions into it are blocked.
			if (FVector::DotProduct(Hit.Normal, Direction) < 0.f)
			{
				return true;
			}
		}
		else
		{
			// A ramp or a slope it can walk up is not an obstacle.
			if (Hit.ImpactNormal.Z < Move->GetWalkableFloorZ())
			{
				return true;
			}
		}
	}

	// No walkable ground ahead: the direction leads off a ledge.
	const float ProbeDistance = FMath::Max(GetGameValue(GBEnemyKey::SteerLedgeProbeDistance, 80.f), 0.f);
	const float LedgeDrop = FMath::Max(GetGameValue(GBEnemyKey::SteerLedgeDrop, 150.f), 0.f);
	if (ProbeDistance <= 0.f)
	{
		return false;
	}
	if (Goal && FVector::Dist2D(GetActorLocation(), Goal->GetActorLocation()) <= ProbeDistance)
	{
		// The goal itself is within the probe distance: its position is never a ledge.
		return false;
	}
	const float ScaledHalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const FVector Feet = GetActorLocation() - FVector::UpVector * ScaledHalfHeight;
	const FVector LedgeTop = FVector(Feet.X + Direction.X * ProbeDistance, Feet.Y + Direction.Y * ProbeDistance, Feet.Z + StepHeight);
	const FVector LedgeBottom = LedgeTop - FVector::UpVector * (LedgeDrop + StepHeight);
	FCollisionQueryParams LedgeParams(SCENE_QUERY_STAT(GBEnemySteerLedge), false, this);
	if (Goal)
	{
		LedgeParams.AddIgnoredActor(Goal);
	}
	FCollisionResponseParams LedgeResponse;
	LedgeResponse.CollisionResponse.SetResponse(ECC_Pawn, ECR_Ignore);
	FHitResult LedgeHit;
	if (!World->LineTraceSingleByChannel(LedgeHit, LedgeTop, LedgeBottom, ECC_Pawn, LedgeParams, LedgeResponse))
	{
		return true;
	}
	return LedgeHit.ImpactNormal.Z < Move->GetWalkableFloorZ();
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
	const float Threshold = FMath::Min(GetGameValue(GBEnemyKey::StuckDistance, 50.f), Speed * Window * GetGameValue(GBEnemyKey::StuckSpeedFraction, 0.25f));
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

float AGBEnemyCharacter::GetEyeHeightAboveFoot(bool bCrouched) const
{
	const float HalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	if (bCrouched)
	{
		return StatOr("CrouchEyeHeight", 100.f);
	}
	return HalfHeight * (1.f + GetGameValue(GBEnemyKey::AimHeightFraction, 0.5f));
}

FVector AGBEnemyCharacter::GetChestLocation() const
{
	if (UsesAwareness())
	{
		const float HalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		return GetActorLocation() - FVector::UpVector * HalfHeight + FVector::UpVector * GetEyeHeightAboveFoot(IsCrouchedPose());
	}
	return GetActorLocation() + FVector::UpVector * (GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * GetGameValue(GBEnemyKey::AimHeightFraction, 0.5f));
}

bool AGBEnemyCharacter::HasLineOfSight(const AActor* Goal) const
{
	if (!Goal || !GetWorld())
	{
		return false;
	}
	const FVector Start = GetChestLocation();
	const FVector End = Goal->GetActorLocation();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GBEnemyLineOfSight), false, this);
	for (int32 Attempt = 0; Attempt < 8; ++Attempt)
	{
		FHitResult Hit;
		if (!GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
		{
			return true;
		}
		AActor* HitActor = Hit.GetActor();
		if (HitActor == Goal)
		{
			return true;
		}
		if (HitActor && HitActor->IsA<APawn>())
		{
			Params.AddIgnoredActor(HitActor);
			continue;
		}
		return false;
	}
	return false;
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
		Config.GravityScale = Stats->GetStat(GBStat::ProjectileGravity, 0.f);
		Config.MaxRange = Stats->GetStat(GBStat::AggroRange, 0.f) * GetGameValue(GBEnemyKey::ProjectileRangeMult, 1.5f);
		Config.MaxLifetime = GetGameValue(GBProjectileKey::ProjectileMaxLifetime, Config.MaxLifetime);
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
		// The squad learns where the hit came from, not where the player is.
		if (Info.Instigator)
		{
			StimulusPos = Info.Instigator->GetActorLocation();
		}
		else if (!Info.HitDirection.IsNearlyZero())
		{
			StimulusPos = GetActorLocation() - Info.HitDirection.GetSafeNormal() * 500.f;
		}
		bStimulusConfirmed = false;

		// Stagger (pixel-art melee, StaggerFraction stat; 0 = never): one hit of at least that share of its max health stops it
		// for StaggerTime and breaks its windup, at most once per StaggerCooldown, so a stream of small hits cannot stun-lock it.
		const float StaggerFraction = StatOr(TEXT("StaggerFraction"), 0.f);
		const float MaxHealth = Attributes ? Attributes->GetMaxHealth() : 0.f;
		if (StaggerFraction > 0.f && bPixelArtLook && !IsRanged() && MaxHealth > 0.f && ActualDamage >= StaggerFraction * MaxHealth && GetWorld()
			&& GetCharacterMovement()->IsMovingOnGround())   // not mid-lunge: the lock's stop would drop it out of the air
		{
			static TMap<TWeakObjectPtr<const AGBEnemyCharacter>, double> LastStagger;   // no header change mid-jam (Live Coding)
			const double Now = GetWorld()->GetTimeSeconds();
			double& Last = LastStagger.FindOrAdd(this, -1000.0);
			if (Now < Last || Now - Last >= StatOr(TEXT("StaggerCooldown"), 1.5f))   // Now < Last: a new PIE world
			{
				Last = Now;
				ApplyControlLock(StatOr(TEXT("StaggerTime"), 0.3f));
			}
			if (LastStagger.Num() > 256)
			{
				for (auto It = LastStagger.CreateIterator(); It; ++It)
				{
					if (!It.Key().IsValid())
					{
						It.RemoveCurrent();
					}
				}
			}
		}
	}
	OnDamagedVisual(Info, ActualDamage);
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
	ControlLockTime = 0.f;
	{
		const FString ClassName = GetClass()->GetName();
		const bool bBoss = ClassName.Contains(TEXT("Valkyrie")) || ClassName.Contains(TEXT("Entity"));
		const FVector DeathAt = GetActorLocation();
		GBAudio::Play(this, FName(bBoss ? TEXT("death_boss") : (GBAudio::IsFutureMap(this) ? TEXT("death_future") : TEXT("death_past"))), &DeathAt, 1.f, 1.f);
	}
	if (UGBSquadSubsystem* Squad = UGBSquadSubsystem::Get(this))
	{
		Squad->Leave(this);
	}

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

void AGBEnemyCharacter::InterruptAttack()
{
	if (State == EGBEnemyState::Windup)
	{
		SetState(EGBEnemyState::Recover);
	}
}

void AGBEnemyCharacter::ApplyControlLock(float Seconds)
{
	if (State == EGBEnemyState::Dead || bSealed)
	{
		return;
	}
	ControlLockTime = FMath::Max(ControlLockTime, Seconds);
	InterruptAttack();
	StopMoving();
}

void AGBEnemyCharacter::HandleElementalStatusStarted(EGBElement Element, FName Reaction, float Duration)
{
	if (Element == EGBElement::Lightning)
	{
		if (Reaction == GBElemental::ReactionStun)
		{
			ApplyControlLock(Duration);
		}
		else if (Reaction == GBElemental::ReactionInterrupt)
		{
			InterruptAttack();
		}
	}
	else if (Element == EGBElement::Ice)
	{
		if (Reaction == GBElemental::ReactionFreeze)
		{
			ApplyControlLock(Duration);
		}
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Zone, awareness, patrol
// ---------------------------------------------------------------------------------------------------------------------

void AGBEnemyCharacter::SetEncounterContext(AGBEncounter* InEncounter, const FBox& Zone, const FVector& Post, EGBUnawareBehavior Behavior, const TArray<FVector>& InRouteCandidates)
{
	Encounter = InEncounter;
	HomeZone = Zone;
	bHasHomeZone = Zone.IsValid != 0;
	PostLocation = Post;
	UnawareBehavior = Behavior;
	RouteCandidates = InRouteCandidates;
	bRouteBuilt = false;
	AwareYawBase = GetActorRotation().Yaw;
	LastUnawarePos = GetActorLocation();
	SetGait(EGBGait::Patrol);
}

FVector AGBEnemyCharacter::ClampToZone(const FVector& Point) const
{
	return bHasHomeZone ? GBTactics::ClampToBox(HomeZone, Point) : Point;
}

bool AGBEnemyCharacter::IsInsideZone(const FVector& Point) const
{
	return !bHasHomeZone || GBTactics::IsInsideBox(HomeZone, Point);
}

void AGBEnemyCharacter::UpdateSightCache(const AActor* Goal, float DeltaSeconds)
{
	SightCacheTimer -= DeltaSeconds;
	if (SightCacheTimer <= 0.f)
	{
		bSightCached = HasLineOfSight(Goal);
		SightCacheTimer = 0.1f + AwareRandom.FRandRange(0.f, 0.05f);
	}
}

void AGBEnemyCharacter::BecomeAwareNow()
{
	if (State == EGBEnemyState::Dead || bAware)
	{
		return;
	}
	bAware = true;
	bProvoked = true;
	AlertTimer = 0.f;
	PendingAlertDelay = -1.f;
	SetGait(EGBGait::Run);
	if (State == EGBEnemyState::Idle)
	{
		SetState(EGBEnemyState::Chase);
	}
	OnBecameAware(false);
}

void AGBEnemyCharacter::SetSealed(bool bInSealed)
{
	if (bInSealed == bSealed || !Attributes)
	{
		return;
	}
	bSealed = bInSealed;
	if (bSealed)
	{
		bSealedWasInvulnerable = Attributes->IsPlainInvulnerable();
		Attributes->SetInvulnerable(true);
		bProvoked = false;
		bHeardNoise = false;
		PendingAlertDelay = -1.f;
		StopMoving();
	}
	else
	{
		Attributes->SetInvulnerable(bSealedWasInvulnerable);
	}
}

void AGBEnemyCharacter::BeginWake(const FVector& WakePoint)
{
	if (State == EGBEnemyState::Dead)
	{
		return;
	}
	SetSealed(false);
	WakeTarget = WakePoint;
	bPendingWakeLeap = true;
	if (const AActor* Player = FindTarget())
	{
		StimulusPos = Player->GetActorLocation();
		bStimulusConfirmed = true;
	}
	NoticePlayer(true);
}

void AGBEnemyCharacter::LaunchWakeLeap()
{
	UWorld* World = GetWorld();
	UCharacterMovementComponent* Move = GetCharacterMovement();
	const UCapsuleComponent* Capsule = GetCapsuleComponent();
	const FVector Start = GetActorLocation();
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const float Radius = Capsule->GetScaledCapsuleRadius();
	const float Gravity = FMath::Max(FMath::Abs(Move->GetGravityZ()), 1.f);
	FVector Toward = WakeTarget - Start;
	Toward.Z = 0.f;
	const float Distance0 = Toward.Size();
	const FVector Dir = Toward.IsNearlyZero() ? GetActorForwardVector().GetSafeNormal2D() : Toward.GetSafeNormal();
	const float MaxSpeed = StatOr("WakeLeapSpeed", 900.f);
	const float BaseUp = StatOr("WakeLeapUp", 450.f);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GBWakeLeap), false, this);
	const FCollisionShape Shape = FCollisionShape::MakeCapsule(Radius * 0.9f, HalfHeight * 0.9f);

	// Candidate landings towards the encounter's middle: each must be on the navmesh, fit the capsule, be reachable with the launch
	// computed for its real height, and have a clear arc (sweeps along it).
	for (const float Scale : { 1.f, 0.8f, 0.6f, 1.25f, 0.45f })
	{
		for (const float Yaw : { 0.f, 20.f, -20.f, 40.f, -40.f })
		{
			FVector Foot = ClampToZone(Start + Dir.RotateAngleAxis(Yaw, FVector::UpVector) * Distance0 * Scale);
			if (!ProjectToNav(Foot))
			{
				continue;
			}
			const FVector Landing = Foot + FVector::UpVector * (HalfHeight + 2.f);
			if (World->OverlapBlockingTestByChannel(Landing, FQuat::Identity, ECC_Pawn, Shape, Params))
			{
				continue;
			}
			const float DeltaZ = Landing.Z - Start.Z;
			float UpSpeed = BaseUp;
			float Discriminant = UpSpeed * UpSpeed - 2.f * Gravity * DeltaZ;
			if (Discriminant < 0.f)
			{
				UpSpeed *= 1.35f;
				Discriminant = UpSpeed * UpSpeed - 2.f * Gravity * DeltaZ;
				if (Discriminant < 0.f)
				{
					continue;
				}
			}
			const float AirTime = (UpSpeed + FMath::Sqrt(Discriminant)) / Gravity;
			FVector Flat = Landing - Start;
			Flat.Z = 0.f;
			const float Horizontal = Flat.Size() / FMath::Max(AirTime, 0.1f);
			if (Horizontal > MaxSpeed)
			{
				continue;   // not reachable at the allowed speed: no capped, short landing
			}
			const FVector FlatDir = Flat.GetSafeNormal();
			bool bClear = true;
			FVector Previous = Start;
			for (int32 Step = 1; Step <= 6 && bClear; ++Step)
			{
				const float T = AirTime * static_cast<float>(Step) / 6.f;
				const FVector Point = Start + FlatDir * Horizontal * T + FVector::UpVector * (UpSpeed * T - 0.5f * Gravity * T * T);
				bClear = !World->SweepTestByChannel(Previous, Point, FQuat::Identity, ECC_Pawn, Shape, Params);
				Previous = Point;
			}
			if (!bClear)
			{
				continue;
			}
			SetActorRotation(FRotator(0.f, FlatDir.Rotation().Yaw, 0.f));
			LaunchCharacter(FlatDir * Horizontal + FVector::UpVector * UpSpeed, true, true);
			bWakeLeaping = true;
			WakeLeapTime = 0.f;
			return;
		}
	}

	// No valid leap: walk out. She is put 150 cm forward onto the navmesh (behind the alert beat), the last resort.
	FVector Forward = ClampToZone(Start + Dir * 150.f);
	if (ProjectToNav(Forward))
	{
		SetActorLocation(Forward + FVector::UpVector * (HalfHeight + 2.f), false, nullptr, ETeleportType::TeleportPhysics);
		StopMoving();
	}
	UE_LOG(LogGB, Display, TEXT("%s: no valid wake leap, walked out to %s"), *EnemyId.ToString(), *GetActorLocation().ToCompactString());
}

void AGBEnemyCharacter::ScheduleAlert(float Delay)
{
	if (State == EGBEnemyState::Dead || bAware || bSealed)
	{
		return;
	}
	PendingAlertDelay = PendingAlertDelay < 0.f ? FMath::Max(Delay, 0.f) : FMath::Min(PendingAlertDelay, Delay);
}

void AGBEnemyCharacter::HearNoise(const FVector& Location)
{
	if (!UsesAwareness() || bAware || bSealed || State == EGBEnemyState::Dead || !EraComponent->IsPresent() || UnawareBehavior == EGBUnawareBehavior::Dormant)
	{
		return;
	}
	if (FVector::Dist(GetActorLocation(), Location) <= StatOr("HearingRange", 2500.f))
	{
		bHeardNoise = true;
		StimulusPos = Location;
		bStimulusConfirmed = false;
	}
}

void AGBEnemyCharacter::ReturnToUnaware()
{
	if (State == EGBEnemyState::Dead || !UsesAwareness())
	{
		return;
	}
	bAware = false;
	bProvoked = false;
	bHeardNoise = false;
	AlertTimer = 0.f;
	NoticeTimer = 0.f;
	PendingAlertDelay = -1.f;
	bPatrolMoving = false;
	PatrolPauseTimer = 0.f;
	bRepositioning = false;
	bReturningToPost = bHasHomeZone;
	ReturnTimer = 0.f;
	StimulusPos = FVector::ZeroVector;
	StopMoving();
	if (State != EGBEnemyState::Idle)
	{
		SetState(EGBEnemyState::Idle);
	}
	SetGait(EGBGait::Patrol);
	OnReturnedToUnaware();
}

void AGBEnemyCharacter::NoticePlayer(bool bSource)
{
	if (bAware || State == EGBEnemyState::Dead)
	{
		return;
	}
	bAware = true;
	bProvoked = true;
	bHeardNoise = false;
	AlertTimer = FMath::Max(0.f, StatOr("AlertPause", 0.5f)) * (bSource ? 1.f : 0.4f);
	PendingAlertDelay = -1.f;
	bPatrolMoving = false;
	StopMoving();
	SetGait(EGBGait::Run);
	if (State == EGBEnemyState::Idle)
	{
		SetState(EGBEnemyState::Chase);
	}
	OnBecameAware(bSource);
}

void AGBEnemyCharacter::OnBecameAware(bool bSource)
{
	if (UGBSquadSubsystem* Squad = UGBSquadSubsystem::Get(this))
	{
		if (bSource)
		{
			Squad->AlertSquad(this, StimulusPos, bStimulusConfirmed);
		}
	}
	if (AGBEncounter* Enc = Encounter.Get())
	{
		Enc->NotifyEnemyAware(this);
	}
}

bool AGBEnemyCharacter::TickAwareness(AActor* Goal, float DeltaSeconds)
{
	if (bSealed)
	{
		// Sealed: no sight, no hearing, no alert, no proximity wake; it does not turn or move.
		bProvoked = false;
		bHeardNoise = false;
		PendingAlertDelay = -1.f;
		return false;
	}
	UGBSquadSubsystem* Squad = UGBSquadSubsystem::Get(this);
	if (Squad && !bJoinedSquad)
	{
		Squad->Join(this);
		bJoinedSquad = true;
	}
	UpdateSightCache(Goal, DeltaSeconds);

	// Last resort: an enemy found outside its zone (pushed, launched, a path that slipped) is put back in.
	if (bHasHomeZone && !HomeZone.ExpandBy(GBEnemyCharacterLocal::ZoneClampDistance).IsInside(GetActorLocation()))
	{
		++ZoneClamps;
		UE_LOG(LogGB, Display, TEXT("%s was found outside its zone at %s and put back"), *EnemyId.ToString(), *GetActorLocation().ToCompactString());
		SetActorLocation(ClampToZone(GetActorLocation()), false, nullptr, ETeleportType::TeleportPhysics);
		StopMoving();
	}

	const FVector ToGoal = Goal->GetActorLocation() - GetActorLocation();
	if (bAware)
	{
		if (bSightCached && Squad)
		{
			Squad->ReportSighting(this, Goal->GetActorLocation());
		}
		if (AlertTimer > 0.f)
		{
			// The alert beat: stop, turn to him, then act.
			AlertTimer -= DeltaSeconds;
			if (!ToGoal.GetSafeNormal2D().IsNearlyZero())
			{
				TurnYawTowards(ToGoal.Rotation().Yaw, DeltaSeconds, StatOr("TurnRate", 420.f));
			}
			return false;
		}
		if (bPendingWakeLeap)
		{
			// Woken by the flag: after the alert beat she comes out (a validated leap, or a walk out).
			bPendingWakeLeap = false;
			LaunchWakeLeap();
		}
		if (bWakeLeaping)
		{
			WakeLeapTime += DeltaSeconds;
			if ((WakeLeapTime > 0.3f && GetCharacterMovement()->IsMovingOnGround()) || WakeLeapTime > 4.f)
			{
				bWakeLeaping = false;
				StopMoving();
			}
			return false;
		}
		return true;
	}

	// Unaware. An alert from the squad arrives after its staggered delay.
	if (PendingAlertDelay >= 0.f)
	{
		PendingAlertDelay -= DeltaSeconds;
		if (PendingAlertDelay <= 0.f)
		{
			NoticePlayer(false);
			return false;
		}
	}

	const float Distance = ToGoal.Size();
	bool bNotice = bProvoked || bHeardNoise;
	if (bNotice && !bHeardNoise && StimulusPos.IsNearlyZero())
	{
		StimulusPos = GetActorLocation();   // told or hurt with no known source: it only knows where it stands
		bStimulusConfirmed = false;
	}
	if (!bNotice)
	{
		if (UnawareBehavior == EGBUnawareBehavior::Dormant)
		{
			bNotice = Distance <= StatOr("DormantWakeRange", 600.f);
			if (bNotice)
			{
				StimulusPos = Goal->GetActorLocation();
				bStimulusConfirmed = true;
			}
		}
		else
		{
			const FVector Forward = GetActorForwardVector().GetSafeNormal2D();
			const float Angle = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Forward, ToGoal.GetSafeNormal2D()), -1.f, 1.f)));
			const float Aggro = Stats->GetStat(GBStat::AggroRange, 0.f);
			if (GBEngage::Notices(Distance, Angle, bSightCached, Aggro, StatOr("SightAngle", 140.f), false))
			{
				// Held for NoticeTime: shorter the closer he is, short after the squad was alerted lately.
				float Need = StatOr("NoticeTime", 0.6f) * FMath::Clamp(Distance / FMath::Max(Aggro, 1.f), 0.35f, 1.f);
				if (Squad && Squad->SecondsSinceAlert(this) < GBEnemyCharacterLocal::SquadAlertLifetime)
				{
					Need = FMath::Min(Need, 0.25f);
				}
				NoticeTimer += DeltaSeconds;
				bNotice = NoticeTimer >= Need;
				if (bNotice)
				{
					StimulusPos = Goal->GetActorLocation();
					bStimulusConfirmed = true;   // he was seen: the only stimulus that is a confirmed position
				}
			}
			else
			{
				NoticeTimer = FMath::Max(0.f, NoticeTimer - 2.f * DeltaSeconds);
			}
		}
	}
	if (bNotice)
	{
		NoticePlayer(true);
		return false;
	}

	// The movement component moves the actor after this tick, so the distance walked is the change since the last unaware tick.
	UnawareTravel += FVector::Dist2D(LastUnawarePos, GetActorLocation());
	LastUnawarePos = GetActorLocation();
	TickUnaware(Goal, DeltaSeconds);
	return false;
}

bool AGBEnemyCharacter::ProjectToNav(FVector& InOutPoint) const
{
	const UNavigationSystemV1* Nav = UNavigationSystemV1::GetCurrent(GetWorld());
	if (!Nav || Nav->GetDefaultNavDataInstance() == nullptr)
	{
		return false;
	}
	FNavLocation Projected;
	if (!Nav->ProjectPointToNavigation(InOutPoint, Projected, FVector(GBEnemyCharacterLocal::NavProjectExtentXY, GBEnemyCharacterLocal::NavProjectExtentXY, GBEnemyCharacterLocal::NavProjectExtentZ)))
	{
		return false;
	}
	InOutPoint = Projected.Location;
	return true;
}

void AGBEnemyCharacter::BuildPatrolRoute()
{
	bRouteBuilt = true;
	PatrolRoute.Reset();
	PatrolIndex = 0;
	if (UnawareBehavior != EGBUnawareBehavior::Patrol || !bHasHomeZone)
	{
		return;
	}

	const bool bNav = PatrolsOnNavMesh();
	auto Prepare = [this, bNav](FVector Point) -> TOptional<FVector>
	{
		Point = ClampToZone(Point);
		// A point the navmesh does not know (its tile is not built yet far from the player) is kept: the walk falls back to local steering.
		if (bNav)
		{
			ProjectToNav(Point);
		}
		return Point;
	};

	// The post first, then other spawn points of the encounter, shuffled; two to four points in all.
	if (TOptional<FVector> Post = Prepare(PostLocation))
	{
		PatrolRoute.Add(Post.GetValue());
	}
	else
	{
		PatrolRoute.Add(PostLocation);
	}
	TArray<FVector> Others = RouteCandidates;
	for (int32 Index = Others.Num() - 1; Index > 0; --Index)
	{
		Others.Swap(Index, AwareRandom.RandRange(0, Index));
	}
	const int32 Wanted = AwareRandom.RandRange(2, 4);
	for (const FVector& Candidate : Others)
	{
		if (PatrolRoute.Num() >= Wanted)
		{
			break;
		}
		TOptional<FVector> Point = Prepare(Candidate);
		if (Point && !GBTactics::IsTooClose(Point.GetValue(), PatrolRoute, 300.f))
		{
			PatrolRoute.Add(Point.GetValue());
		}
	}
	// Not enough spawn points: random reachable points near the post (ground) or round it (air).
	if (PatrolRoute.Num() < 2)
	{
		if (bNav)
		{
			if (const UNavigationSystemV1* Nav = UNavigationSystemV1::GetCurrent(GetWorld()))
			{
				for (int32 Try = 0; Try < 6 && PatrolRoute.Num() < 3; ++Try)
				{
					FNavLocation RandomPoint;
					if (Nav->GetRandomReachablePointInRadius(PostLocation, StatOr("PatrolRadius", 700.f), RandomPoint))
					{
						const FVector Point = ClampToZone(RandomPoint.Location);
						if (!GBTactics::IsTooClose(Point, PatrolRoute, 300.f))
						{
							PatrolRoute.Add(Point);
						}
					}
				}
			}
		}
		else
		{
			for (int32 Try = 0; Try < 2; ++Try)
			{
				const float Angle = AwareRandom.FRandRange(0.f, UE_TWO_PI);
				PatrolRoute.Add(ClampToZone(PostLocation + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * StatOr(TEXT("PatrolRadius"), 700.f)));
			}
		}
	}
	if (PatrolRoute.Num() < 2)
	{
		// The navmesh of a far room may not be built yet: try again in a moment.
		PatrolRoute.Reset();
		RouteRetryTimer = 2.f;
		return;
	}
	// Two patrollers must not walk the same loop in step: each starts at a different point.
	PatrolIndex = AwareRandom.RandRange(0, PatrolRoute.Num() - 1);
	PatrolPauseTimer = AwareRandom.FRandRange(0.f, StatOr("PatrolPauseMax", 3.f));
}

void AGBEnemyCharacter::TickUnaware(AActor* Goal, float DeltaSeconds)
{
	(void)Goal;
	if (!bHasHomeZone)
	{
		return;   // an enemy without an encounter stands where it was put
	}
	if (State != EGBEnemyState::Idle)
	{
		SetState(EGBEnemyState::Idle);
	}
	SetGait(EGBGait::Patrol);

	// Lost him: back to the post first (at patrol speed), then guard / patrol / dormant resume.
	if (bReturningToPost)
	{
		ReturnTimer += DeltaSeconds;
		// No progress towards the post for 8 s (boxed in by something): it gives up the walk and resumes from where it stands.
		const float DistanceToPost = FVector::Dist2D(GetActorLocation(), PostLocation);
		if (ReturnTimer < DeltaSeconds * 1.5f || DistanceToPost < ReturnBestDistance - 50.f)
		{
			ReturnBestDistance = DistanceToPost;
			ReturnStallTimer = 0.f;
		}
		else
		{
			ReturnStallTimer += DeltaSeconds;
		}
		if (DistanceToPost < 120.f || ReturnTimer > 40.f || ReturnStallTimer > 8.f)
		{
			bReturningToPost = false;
			StopMoving();
		}
		else
		{
			MoveToZonePoint(PostLocation, 60.f, nullptr, DeltaSeconds);
			const FVector Velocity = GetVelocity();
			if (Velocity.Size2D() > 25.f)
			{
				TurnYawTowards(Velocity.Rotation().Yaw, DeltaSeconds, StatOr("TurnRate", 420.f));
			}
			return;
		}
	}

	if (UnawareBehavior == EGBUnawareBehavior::Dormant)
	{
		return;
	}
	if (!bRouteBuilt)
	{
		BuildPatrolRoute();
	}
	else if (UnawareBehavior == EGBUnawareBehavior::Patrol && PatrolRoute.Num() < 2)
	{
		RouteRetryTimer -= DeltaSeconds;
		if (RouteRetryTimer <= 0.f)
		{
			bRouteBuilt = false;
		}
	}

	const bool bPatrol = UnawareBehavior == EGBUnawareBehavior::Patrol && PatrolRoute.Num() >= 2;
	if (!bPatrol)
	{
		// Guard: stand at the post and look around.
		GuardTimer -= DeltaSeconds;
		if (GuardTimer <= 0.f)
		{
			GuardTimer = AwareRandom.FRandRange(StatOr("GuardTurnMin", 2.5f), FMath::Max(StatOr("GuardTurnMax", 5.f), StatOr("GuardTurnMin", 2.5f)));
			GuardYaw = FRotator::NormalizeAxis(AwareYawBase + AwareRandom.FRandRange(-110.f, 110.f));
		}
		TurnYawTowards(GuardYaw, DeltaSeconds, StatOr("TurnRate", 420.f) * 0.3f);
		return;
	}

	if (PatrolPauseTimer > 0.f)
	{
		PatrolPauseTimer -= DeltaSeconds;
		return;
	}
	const FVector Dest = PatrolRoute[PatrolIndex];
	PatrolPointTimer += DeltaSeconds;
	if (FVector::Dist2D(GetActorLocation(), Dest) <= GBEnemyCharacterLocal::PatrolArrivalRadius || PatrolPointTimer > GBEnemyCharacterLocal::PatrolPointTimeout)
	{
		StopMoving();
		PatrolPointTimer = 0.f;
		PatrolPauseTimer = AwareRandom.FRandRange(StatOr("PatrolPauseMin", 1.f), FMath::Max(StatOr("PatrolPauseMax", 3.f), StatOr("PatrolPauseMin", 1.f)));
		PatrolIndex = (PatrolIndex + 1) % PatrolRoute.Num();
		return;
	}
	MoveToZonePoint(Dest, GBEnemyCharacterLocal::PatrolArrivalRadius * 0.5f, nullptr, DeltaSeconds);
	const FVector Velocity = GetVelocity();
	if (Velocity.Size2D() > 25.f)
	{
		TurnYawTowards(Velocity.Rotation().Yaw, DeltaSeconds, StatOr("TurnRate", 420.f));
	}
}
