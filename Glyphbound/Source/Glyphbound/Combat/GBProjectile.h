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
class UPrimitiveComponent;
class UPointLightComponent;
class UGBPixelVfxComponent;

/** Keys of Game.json "Values" read when a projectile is launched. */
namespace GBProjectileKey
{
	inline const FName KnockbackMinLift(TEXT("KnockbackMinLift")); // smallest upward part of the blast push direction
	inline const FName ProjectileMaxLifetime(TEXT("ProjectileMaxLifetime")); // seconds before a projectile removes itself
	inline const FName SpellProjectileLifetimeMargin(TEXT("SpellProjectileLifetimeMargin")); // extra seconds on top of Range / Speed, so a spell bolt is never removed before reaching its range
}

/** Everything a projectile needs; filled from weapon stats, spell params or item stats by whoever fires it. */
USTRUCT(BlueprintType)
struct GLYPHBOUND_API FGBProjectileConfig
{
	GENERATED_BODY()

	/** The hit dealt on impact (Amount, Element, Instigator, SourceId, OnHitEffect...). Causer is set to the projectile. */
	UPROPERTY() FGBDamageInfo Damage;

	/** Weak point multiplier (GDD 3): 1 = no bonus. Applied to direct hits via UGBCombatLibrary::ApplyWeakPointBonus. */
	UPROPERTY() float HeadshotMult = 1.f;

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
	/** cm/s pushed onto hostile characters by the blast: full at the center, 0 at the edge. 0 = no push. */
	UPROPERTY() float ExplosionKnockback = 0.f;
	/** Seconds the explosion shockwave (AGBExplosionEffect) is shown. Every explosion shows it, hit or miss. */
	UPROPERTY() float ExplosionEffectTime = 0.35f;
	/** > 0: it does not explode on contact with the world; it bounces and explodes after this many seconds (grenades). */
	UPROPERTY() float FuseTime = 0.f;
	/** Bounce retention (UProjectileMovementComponent::Bounciness). Only used when FuseTime > 0. */
	UPROPERTY() float Bounciness = 0.6f;
	/** Surface friction when bouncing (UProjectileMovementComponent::Friction). Only used when FuseTime > 0. */
	UPROPERTY() float Friction = 0.2f;
	/** Keeps going through actors it damages (each actor once). */
	UPROPERTY() bool bPierce = false;

	// ---- Looks (art rule: bPixelArt = camera-facing sprite, otherwise a small 3D mesh) ----
	UPROPERTY() bool bPixelArt = true;
	/** Sprite id from Sprites.json (pixel art). */
	UPROPERTY() FName SpriteId = NAME_None;
	UPROPERTY() FLinearColor Color = FLinearColor::White;
	/** Visual scale relative to the collision radius. */
	UPROPERTY() float VisualScale = 1.f;

	// ---- Baked pixel VFX (GBPixelVfx plugin, clips in Content/Data/Vfx): optional, pixel-art projectiles only ----
	/** Clip shown instead of the sprite while it flies, centred on its bright centre. None = the sprite. */
	UPROPERTY() FName FlightVfxId = NAME_None;
	/** Height in cm of the whole frame of the flight clip (the flame fills about two thirds of it). */
	UPROPERTY() float FlightVfxHeight = 110.f;
	/** One-shot clip played where it explodes, on top of the shockwave. None = the shockwave alone. */
	UPROPERTY() FName ImpactVfxId = NAME_None;
	/** Width of the frame of the impact clip as a multiple of the blast diameter (the cloud fills about half the frame). */
	UPROPERTY() float ImpactVfxSize = 1.4f;
	/** Playback speed of the impact clip (1 = as baked). */
	UPROPERTY() float ImpactVfxRate = 1.5f;

	// ---- Energy bolt look (3D bosses): off by default, so every other projectile keeps its look ----
	/** Non-pixel-art only: an elongated glowing core along the velocity, a dim stretched trail behind it and a point light. */
	UPROPERTY() bool bEnergyBolt = false;
	/** Core length and thickness, cm. */
	UPROPERTY() float BoltLength = 75.f;
	UPROPERTY() float BoltThickness = 12.f;
	/** Length of the dim stretched trail behind the core, cm. */
	UPROPERTY() float BoltTrailLength = 220.f;
	/** Point light riding the bolt: candelas (0 = no light) and attenuation radius in cm. Shadows are off. */
	UPROPERTY() float BoltLightIntensity = 0.f;
	UPROPERTY() float BoltLightRadius = 600.f;
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

	/** Energy bolts: core length in cm (0 for any other projectile) and whether its point light is on. */
	float GetBoltLength() const;
	bool HasBoltLight() const;
	/** True while a baked pixel VFX clip is playing on it instead of the sprite. */
	bool HasFlightVfx() const;

	/** Fired once, when it hits something or explodes (before it is destroyed). Spells use it to chain behavior. */
	FGBOnProjectileImpact OnImpact;

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void InitProjectile(const FGBProjectileConfig& InConfig, const FVector& Direction, AActor* InInstigator);

	UFUNCTION()
	void OnCollisionHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	UFUNCTION()
	void OnCollisionOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	/** True if Other is something this projectile should damage now (hostile, alive, has attributes, not hit before). */
	bool IsValidTarget(const AActor* Other) const;
	/** Damages (or explodes on) a valid target, then finishes unless it pierces. */
	void HitTarget(AActor* Other, const FVector& ImpactPoint);
	/** Blast damage, the shockwave effect and the knockback. Called on every explosion, whatever it hit. */
	void Explode(const FVector& Origin);
	/** Pushes hostile characters in the blast away from Origin (never the instigator; walls block it like the damage). */
	void ApplyExplosionKnockback(const FVector& Origin);
	/** Plays Config.FlightVfxId on the projectile. False (and the sprite stays) when the clip is missing. */
	bool SetupFlightVfx();
	/** Plays Config.ImpactVfxId at Origin, sized to the blast. */
	void SpawnImpactVfx(const FVector& Origin);
	/** Broadcasts OnImpact once and destroys the projectile. */
	void Finish(AActor* HitActor, const FVector& Location);

	UPROPERTY(VisibleAnywhere) TObjectPtr<USphereComponent> Collision;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UProjectileMovementComponent> Movement;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UBillboardComponent> Sprite;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Mesh;
	/** Only used by energy bolts. */
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> BoltTrail;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UPointLightComponent> BoltLight;
	/** Only when Config.FlightVfxId names a baked clip. */
	UPROPERTY(Transient) TObjectPtr<UGBPixelVfxComponent> FlightVfx;

	UPROPERTY() FGBProjectileConfig Config;
	FVector StartLocation = FVector::ZeroVector;
	bool bFinished = false;
	/** Actors already damaged (pierce: each one is hit once). */
	TArray<TWeakObjectPtr<AActor>> DamagedActors;
};
