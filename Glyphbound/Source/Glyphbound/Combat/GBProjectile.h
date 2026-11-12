// One generic projectile for everything that flies: staff bolts, fireballs, lightning bolts, enemy shots, grenades.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/GBTypes.h"
#include "GBProjectile.generated.h"

class USphereComponent;
class UProjectileMovementComponent;
class UBillboardComponent;
class UStaticMeshComponent;

/** Everything a projectile needs; filled from weapon stats, spell params or item stats by whoever fires it. */
USTRUCT(BlueprintType)
struct GLYPHBOUND_API FGBProjectileConfig
{
	GENERATED_BODY()

	/** The hit dealt on impact (Amount, Element, Instigator, SourceId, OnHitEffect...). Causer is set to the projectile. */
	UPROPERTY() FGBDamageInfo Damage;

	UPROPERTY() float Speed = 3000.f;
	/** Collision radius, cm. */
	UPROPERTY() float Radius = 12.f;
	/** 0 = flies straight; 1 = normal gravity (grenades). */
	UPROPERTY() float GravityScale = 0.f;
	/** The projectile disappears after travelling this far (cm). 0 = use MaxLifetime only. */
	UPROPERTY() float MaxRange = 5000.f;
	UPROPERTY() float MaxLifetime = 10.f;
	/** > 0: on impact (or when the fuse / range ends) it deals Damage in this radius instead of a single hit. */
	UPROPERTY() float ExplosionRadius = 0.f;
	UPROPERTY() bool bExplosionFalloff = true;
	/** > 0: it does not explode on contact with the world; it bounces and explodes after this many seconds (grenades). */
	UPROPERTY() float FuseTime = 0.f;
	/** Keeps going through actors it damages (each actor once). */
	UPROPERTY() bool bPierce = false;

	// ---- Looks (art rule: bPixelArt = camera-facing sprite, otherwise a small 3D mesh) ----
	UPROPERTY() bool bPixelArt = true;
	/** Sprite id from Sprites.json (pixel art). */
	UPROPERTY() FName SpriteId = NAME_None;
	UPROPERTY() FLinearColor Color = FLinearColor::White;
	/** Visual scale relative to the collision radius. */
	UPROPERTY() float VisualScale = 1.f;
};

DECLARE_MULTICAST_DELEGATE_ThreeParams(FGBOnProjectileImpact, class AGBProjectile* /*Projectile*/, AActor* /*HitActor (may be null)*/, const FVector& /*Location*/);

UCLASS()
class GLYPHBOUND_API AGBProjectile : public AActor
{
	GENERATED_BODY()

public:
	AGBProjectile();

	/**
	 * Spawns and launches a projectile. It ignores its instigator and the instigator's team.
	 * Direction does not need to be normalized. Returns null if the spawn fails.
	 */
	static AGBProjectile* SpawnProjectile(UWorld* World, const FGBProjectileConfig& Config, const FVector& Origin, const FVector& Direction, AActor* InInstigator);

	const FGBProjectileConfig& GetConfig() const { return Config; }

	/** Fired once, when it hits something or explodes (before it is destroyed). Spells use it to chain behavior. */
	FGBOnProjectileImpact OnImpact;

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

	void InitProjectile(const FGBProjectileConfig& InConfig, const FVector& Direction, AActor* InInstigator);

	UPROPERTY(VisibleAnywhere) TObjectPtr<USphereComponent> Collision;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UProjectileMovementComponent> Movement;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UBillboardComponent> Sprite;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY() FGBProjectileConfig Config;
	FVector StartLocation = FVector::ZeroVector;
	bool bFinished = false;
};
