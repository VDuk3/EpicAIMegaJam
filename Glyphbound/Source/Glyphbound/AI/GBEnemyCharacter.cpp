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
	constexpr float NavRetryDelay = 1.f;           // seconds of straight steering after a failed path request
	constexpr float ChestHeightFraction = 0.5f;    // of the capsule half height, above the center
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

	// Pawn profile ignores Visibility; hitscan guns and line-of-sight traces use it, so block it.
	UCapsuleComponent* Capsule = GetCapsuleComponent();
	Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	Capsule->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

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
		if (bCanAttack && Distance <= AttackRange && (!IsRanged() || HasLineOfSight(Goal)))
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
		FaceTowards(Goal);
		if (StateTime >= Stats->GetStat(GBStat::AttackInterval, 0.f))
		{
			SetState(EGBEnemyState::Chase);
		}
		break;

	default:
		break;
	}
}

void AGBEnemyCharacter::MoveTowards(AActor* Goal, float DeltaSeconds)
{
	if (!Goal || GetCharacterMovement()->MaxWalkSpeed <= 0.f)
	{
		return;
	}

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
					NavRetryTimer = GBEnemyCharacterLocal::NavRetryDelay;
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
		const FVector Direction = (Goal->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
		AddMovementInput(Direction, 1.f);
	}
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
		if (ToGoal.Size() <= AttackRange * Tolerance)
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
	if (!EraComponent->IsPresent() && State != EGBEnemyState::Dead)
	{
		StopMoving();
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
	if (const FGBEnemyDefinition* Def = GetDefinition())
	{
		AGBPickup::SpawnDrops(World, Def->Drops, GetActorLocation());
	}

	OnDied.Broadcast(this);

	const UGBDataSubsystem* Data = UGBDataSubsystem::Get(this);
	const float CorpseTime = Data ? Data->GetGameValue(GBEnemyKey::CorpseTime, 1.5f) : 1.5f;
	// SetLifeSpan(0) would mean "forever"; keep a tiny minimum instead of destroying inside the damage call.
	SetLifeSpan(FMath::Max(CorpseTime, 0.05f));
}
