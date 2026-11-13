#include "Combat/GBProjectile.h"

#include "AI/GBEnemyCharacter.h"
#include "Combat/GBCombatLibrary.h"
#include "Combat/GBExplosionEffect.h"
#include "Core/GBPlaceholder.h"
#include "Data/GBDataSubsystem.h"
#include "Stats/GBAttributeComponent.h"
#include "UI/GBPixelSpriteLibrary.h"
#include "CollisionQueryParams.h"
#include "Components/BillboardComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/ProjectileMovementComponent.h"

namespace GBProjectileLocal
{
	/** Same lift as the radial damage obstruction trace (GBCombatLibrary.cpp), so a blast on the floor is not blocked by it. */
	constexpr float KnockbackTraceLiftCm = 5.f;
}

AGBProjectile::AGBProjectile()
{
	PrimaryActorTick.bCanEverTick = true;

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetCollisionObjectType(ECC_WorldDynamic);
	Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Collision->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	Collision->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	Collision->SetGenerateOverlapEvents(true);
	Collision->SetCanEverAffectNavigation(false);
	Collision->OnComponentHit.AddDynamic(this, &AGBProjectile::OnCollisionHit);
	Collision->OnComponentBeginOverlap.AddDynamic(this, &AGBProjectile::OnCollisionOverlap);
	RootComponent = Collision;

	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->UpdatedComponent = Collision;
	Movement->bRotationFollowsVelocity = true;
	// The launch velocity is given in world space by InitProjectile.
	Movement->bInitialVelocityInLocalSpace = false;
	Movement->bShouldBounce = false;
	Movement->ProjectileGravityScale = 0.f;

	Sprite = CreateDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));
	Sprite->SetupAttachment(Collision);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Collision);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetGenerateOverlapEvents(false);
	Mesh->SetCastShadow(false);
	Mesh->SetCanEverAffectNavigation(false);

	// Energy bolt parts: empty and dark unless InitProjectile turns them on.
	BoltTrail = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BoltTrail"));
	BoltTrail->SetupAttachment(Collision);
	BoltTrail->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BoltTrail->SetGenerateOverlapEvents(false);
	BoltTrail->SetCastShadow(false);
	BoltTrail->SetCanEverAffectNavigation(false);

	BoltLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("BoltLight"));
	BoltLight->SetupAttachment(Collision);
	BoltLight->SetMobility(EComponentMobility::Movable);
	BoltLight->CastShadows = false;
	BoltLight->IntensityUnits = ELightUnits::Candelas;
	BoltLight->Intensity = 0.f;
	BoltLight->SetVisibility(false);
}

AGBProjectile* AGBProjectile::SpawnProjectile(UWorld* World, const FGBProjectileConfig& InConfig, const FVector& Origin, const FVector& Direction, AActor* InInstigator)
{
	if (!World)
	{
		return nullptr;
	}

	const FVector LaunchDirection = Direction.IsNearlyZero() ? FVector::ForwardVector : Direction.GetSafeNormal();
	const FTransform SpawnTransform(LaunchDirection.Rotation(), Origin);

	AGBProjectile* Projectile = World->SpawnActorDeferred<AGBProjectile>(AGBProjectile::StaticClass(), SpawnTransform, InInstigator,
		Cast<APawn>(InInstigator), ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Projectile)
	{
		UE_LOG(LogGB, Warning, TEXT("Projectile: spawn failed"));
		return nullptr;
	}

	Projectile->InitProjectile(InConfig, LaunchDirection, InInstigator);
	Projectile->FinishSpawning(SpawnTransform);

	if (IsValid(InInstigator) && IsValid(Projectile))
	{
		Projectile->Collision->IgnoreActorWhenMoving(InInstigator, true);
		if (UPrimitiveComponent* InstigatorRoot = Cast<UPrimitiveComponent>(InInstigator->GetRootComponent()))
		{
			InstigatorRoot->IgnoreActorWhenMoving(Projectile, true);
		}
	}

	return IsValid(Projectile) ? Projectile : nullptr;
}

void AGBProjectile::InitProjectile(const FGBProjectileConfig& InConfig, const FVector& Direction, AActor* InInstigator)
{
	Config = InConfig;
	if (InInstigator)
	{
		Config.Damage.Instigator = InInstigator;
	}
	Config.Damage.Causer = this;
	StartLocation = GetActorLocation();

	const float Radius = FMath::Max(Config.Radius, 1.f);
	Collision->SetSphereRadius(Radius);

	Movement->InitialSpeed = Config.Speed;
	Movement->MaxSpeed = Config.Speed;
	Movement->ProjectileGravityScale = Config.GravityScale;
	Movement->bShouldBounce = Config.FuseTime > 0.f;
	Movement->Bounciness = Config.Bounciness;
	Movement->Friction = Config.Friction;
	Movement->Velocity = Direction.GetSafeNormal() * Config.Speed;

	const float VisualSize = Radius * 2.f * FMath::Max(Config.VisualScale, 0.01f);
	if (Config.bPixelArt)
	{
		Mesh->SetVisibility(false);
		if (UGBPixelSpriteLibrary* SpriteLibrary = UGBPixelSpriteLibrary::Get(this))
		{
			SpriteLibrary->SetupBillboard(Sprite, Config.SpriteId, VisualSize);
		}
		else
		{
			UE_LOG(LogGB, Warning, TEXT("Projectile: no pixel sprite library; sprite '%s' not shown"), *Config.SpriteId.ToString());
		}
	}
	else if (Config.bEnergyBolt)
	{
		Sprite->SetVisibility(false);
		Sprite->SetHiddenInGame(true);
		// Ellipsoids stretched along the local X axis, which the movement keeps pointing along the velocity.
		const float Length = FMath::Max(Config.BoltLength, VisualSize);
		const float Thick = FMath::Max(Config.BoltThickness, 1.f);
		const FLinearColor HotCore = FMath::Lerp(Config.Color, FLinearColor::White, 0.2f);
		GBPlaceholder::SetupShape(Mesh, EGBShape::Sphere, FVector(Length, Thick, Thick), HotCore, /*bGlow*/ true);
		const float TrailLength = FMath::Max(Config.BoltTrailLength, 0.f);
		if (TrailLength > 1.f)
		{
			GBPlaceholder::SetupShape(BoltTrail, EGBShape::Sphere, FVector(TrailLength, Thick * 0.6f, Thick * 0.6f), Config.Color * 0.35f, /*bGlow*/ true);
			BoltTrail->SetRelativeLocation(FVector(-(Length * 0.5f + TrailLength * 0.35f), 0.f, 0.f));
		}
		if (Config.BoltLightIntensity > 0.f)
		{
			BoltLight->SetLightColor(Config.Color);
			BoltLight->SetIntensity(Config.BoltLightIntensity);
			BoltLight->SetAttenuationRadius(FMath::Max(Config.BoltLightRadius, 1.f));
			BoltLight->SetVisibility(true);
		}
	}
	else
	{
		Sprite->SetVisibility(false);
		Sprite->SetHiddenInGame(true);
		GBPlaceholder::SetupShape(Mesh, EGBShape::Sphere, FVector(VisualSize), Config.Color, /*bGlow*/ true);
	}
}

float AGBProjectile::GetBoltLength() const
{
	return (Config.bEnergyBolt && !Config.bPixelArt && Mesh) ? Mesh->GetRelativeScale3D().X * 100.f : 0.f;
}

bool AGBProjectile::HasBoltLight() const
{
	return Config.bEnergyBolt && !Config.bPixelArt && BoltLight && BoltLight->IsVisible() && BoltLight->Intensity > 0.f;
}

void AGBProjectile::BeginPlay()
{
	Super::BeginPlay();
}

void AGBProjectile::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Undo the instigator's ignore entry so its list does not grow with every shot.
	AActor* ProjectileInstigator = Config.Damage.Instigator.Get();
	if (IsValid(ProjectileInstigator))
	{
		if (UPrimitiveComponent* InstigatorRoot = Cast<UPrimitiveComponent>(ProjectileInstigator->GetRootComponent()))
		{
			InstigatorRoot->IgnoreActorWhenMoving(this, false);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void AGBProjectile::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bFinished)
	{
		return;
	}

	const FVector Location = GetActorLocation();
	const float Age = GetGameTimeSinceCreation();
	const bool bOutOfRange = Config.MaxRange > 0.f && FVector::Dist(StartLocation, Location) >= Config.MaxRange;
	const bool bTooOld = Config.MaxLifetime > 0.f && Age > Config.MaxLifetime;
	const bool bFuseDone = Config.FuseTime > 0.f && Age >= Config.FuseTime;

	if (bOutOfRange || bTooOld || bFuseDone)
	{
		if (Config.ExplosionRadius > 0.f)
		{
			Explode(Location);
		}
		Finish(nullptr, Location);
	}
}

bool AGBProjectile::IsValidTarget(const AActor* Other) const
{
	if (!IsValid(Other) || Other == this || Other->IsA<AGBProjectile>())
	{
		return false;
	}

	const AActor* ProjectileInstigator = Config.Damage.Instigator.Get();
	if (Other == ProjectileInstigator)
	{
		return false;
	}

	if (!UGBCombatLibrary::CanDamage(ProjectileInstigator, Other))
	{
		return false;
	}

	for (const TWeakObjectPtr<AActor>& Damaged : DamagedActors)
	{
		if (Damaged.Get() == Other)
		{
			return false;
		}
	}
	return true;
}

void AGBProjectile::HitTarget(AActor* Other, const FVector& ImpactPoint)
{
	DamagedActors.Add(Other);

	if (Config.ExplosionRadius > 0.f)
	{
		Explode(ImpactPoint);
	}
	else
	{
		FGBDamageInfo Info = Config.Damage;
		Info.Causer = this;
		Info.HitLocation = ImpactPoint;
		Info.HitDirection = Movement->Velocity.GetSafeNormal();
		// Only shots with a head bonus (damage or build-up) look for the weak point: a spell bolt on the
		// head is a normal hit, so its damage number is not shown as a headshot.
		if (Config.HeadshotMult > 1.f || Info.WeakPointBuildUpMult > 1.f)
		{
			UGBCombatLibrary::ApplyWeakPointBonus(Other, Info, Config.HeadshotMult);
		}
		UGBCombatLibrary::DealDamage(Other, Info);
	}

	// Neutral things with attributes (ice walls, destructibles) stop the shot even when it pierces.
	const bool bHitNeutral = UGBCombatLibrary::GetTeam(Other) == EGBTeam::Neutral;
	if (!Config.bPierce || bHitNeutral)
	{
		Finish(Other, ImpactPoint);
	}
}

void AGBProjectile::OnCollisionOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (bFinished || !IsValidTarget(OtherActor))
	{
		return;
	}

	const FVector ImpactPoint = (bFromSweep && !SweepResult.bStartPenetrating) ? FVector(SweepResult.ImpactPoint) : GetActorLocation();
	HitTarget(OtherActor, ImpactPoint);
}

void AGBProjectile::OnCollisionHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	FVector NormalImpulse, const FHitResult& Hit)
{
	if (bFinished)
	{
		return;
	}

	const FVector ImpactPoint = Hit.ImpactPoint;

	if (IsValidTarget(OtherActor))
	{
		HitTarget(OtherActor, ImpactPoint);
		return;
	}

	// The world: grenades keep bouncing until the fuse ends.
	if (Config.FuseTime > 0.f)
	{
		return;
	}

	if (Config.ExplosionRadius > 0.f)
	{
		Explode(ImpactPoint);
	}
	Finish(OtherActor, ImpactPoint);
}

void AGBProjectile::Explode(const FVector& Origin)
{
	FGBDamageInfo Info = Config.Damage;
	Info.Causer = this;
	UGBCombatLibrary::DealRadialDamage(this, Origin, Config.ExplosionRadius, Info, TConstArrayView<AActor*>(), Config.bExplosionFalloff);

	// Seen and felt even when the blast hits nothing.
	AGBExplosionEffect::Spawn(GetWorld(), Origin, Config.ExplosionRadius, Config.Color, Config.ExplosionEffectTime);
	if (Config.ExplosionKnockback > 0.f)
	{
		ApplyExplosionKnockback(Origin);
	}
}

void AGBProjectile::ApplyExplosionKnockback(const FVector& Origin)
{
	UWorld* World = GetWorld();
	const float Radius = Config.ExplosionRadius;
	if (!World || Radius <= 0.f)
	{
		return;
	}

	// Smallest upward part of the push direction: a walking character pushed only sideways is stopped at once by
	// ground friction; a little lift makes it fly, so the push is felt even for a blast at the target's height.
	const UGBDataSubsystem* Data = UGBDataSubsystem::Get(this);
	const float KnockbackMinLift = Data ? Data->GetGameValue(GBProjectileKey::KnockbackMinLift, 0.35f) : 0.35f;

	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(GBExplosionKnockback), false, this);
	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps, Origin, FQuat::Identity, ObjectParams, FCollisionShape::MakeSphere(Radius), QueryParams);

	AActor* ProjectileInstigator = Config.Damage.Instigator.Get();
	if (!IsValid(ProjectileInstigator))
	{
		ProjectileInstigator = nullptr;
	}

	TSet<ACharacter*> Visited;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		ACharacter* Character = Cast<ACharacter>(Overlap.GetActor());
		if (!IsValid(Character) || Character == ProjectileInstigator || Visited.Contains(Character))
		{
			continue;
		}
		Visited.Add(Character);

		if (!UGBCombatLibrary::CanDamage(ProjectileInstigator, Character))
		{
			continue;
		}

		const FVector TargetLocation = UGBCombatLibrary::GetDamageTargetLocation(Character, Origin);
		const float Distance = FVector::Dist(TargetLocation, Origin);
		if (Distance > Radius)
		{
			continue;
		}

		// Same rule as the blast damage: solid geometry between the blast and the target stops the push.
		FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(GBKnockbackObstruction), true, this);
		TraceParams.AddIgnoredActor(Character);
		FHitResult Obstruction;
		const FVector TraceStart = Origin + FVector(0.f, 0.f, GBProjectileLocal::KnockbackTraceLiftCm);
		if (World->LineTraceSingleByChannel(Obstruction, TraceStart, TargetLocation, ECC_Visibility, TraceParams)
			&& Obstruction.bBlockingHit && Obstruction.GetActor() && Obstruction.GetActor() != Character)
		{
			continue;
		}

		const float Strength = Config.ExplosionKnockback * FMath::Clamp(1.f - Distance / Radius, 0.f, 1.f);
		if (Strength <= 0.f)
		{
			continue;
		}

		FVector Direction = (TargetLocation - Origin).GetSafeNormal();
		if (Direction.IsNearlyZero())
		{
			// Blast right on the target: push it back along the projectile's flight.
			Direction = Movement ? Movement->Velocity.GetSafeNormal() : FVector::ZeroVector;
		}
		Direction.Z = FMath::Max(Direction.Z, KnockbackMinLift);
		Direction = Direction.GetSafeNormal();
		Character->LaunchCharacter(Direction * Strength, /*bXYOverride*/ true, /*bZOverride*/ true);
	}
}

void AGBProjectile::Finish(AActor* HitActor, const FVector& Location)
{
	if (bFinished)
	{
		return;
	}
	bFinished = true;

	if (Movement)
	{
		Movement->StopMovementImmediately();
	}
	OnImpact.Broadcast(this, HitActor, Location);
	Destroy();
}
