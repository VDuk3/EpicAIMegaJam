#include "Combat/GBProjectile.h"

#include "AI/GBEnemyCharacter.h"
#include "Combat/GBCombatLibrary.h"
#include "Core/GBPlaceholder.h"
#include "Stats/GBAttributeComponent.h"
#include "UI/GBPixelSpriteLibrary.h"
#include "Components/BillboardComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/ProjectileMovementComponent.h"

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
	else
	{
		Sprite->SetVisibility(false);
		Sprite->SetHiddenInGame(true);
		GBPlaceholder::SetupShape(Mesh, EGBShape::Sphere, FVector(VisualSize), Config.Color, /*bGlow*/ true);
	}
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

	if (!UGBAttributeComponent::Find(Other) || !UGBCombatLibrary::IsAlive(Other))
	{
		return false;
	}

	// No instigator (traps, world hazards): anything with attributes is a target.
	if (IsValid(ProjectileInstigator) && !UGBCombatLibrary::AreHostile(ProjectileInstigator, Other))
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
		if (const AGBEnemyCharacter* Enemy = Cast<AGBEnemyCharacter>(Other))
		{
			Info.bHeadshot = Enemy->IsHeadshotLocation(ImpactPoint);
		}
		UGBCombatLibrary::DealDamage(Other, Info);
	}

	if (!Config.bPierce)
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
