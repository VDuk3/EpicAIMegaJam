#include "Spells/GBSpell_Projectile.h"
#include "Combat/GBProjectile.h"
#include "Core/GBStatNames.h"
#include "Engine/World.h"
#include "Player/GBPlayerCharacter.h"

namespace
{
	// Extra seconds of life on top of Range / Speed, so a projectile is never removed before reaching its range.
	constexpr float ProjectileSpellLifetimeMargin = 1.f;
}

void AGBSpell_Projectile::StartSpell()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		EndSpell();
		return;
	}

	FGBProjectileConfig Config;
	Config.Damage = SpellCast.MakeDamageInfo(GBStat::Damage, this);
	Config.Speed = FMath::Max(1.f, ReadParam(GBStat::Speed, 3000.f));
	Config.Radius = FMath::Max(1.f, ReadParam(GBStat::Radius, 12.f));
	Config.MaxRange = FMath::Max(0.f, ReadParam(GBStat::Range, 5000.f));
	Config.ExplosionRadius = FMath::Max(0.f, SpellCast.Get(GBSpellParam::ExplosionRadius, 0.f));
	Config.bExplosionFalloff = SpellCast.Get(GBSpellParam::ExplosionFalloff, 1.f) > 0.5f;
	Config.ExplosionKnockback = FMath::Max(0.f, SpellCast.Get(GBSpellParam::ExplosionKnockback, Config.ExplosionKnockback));
	Config.ExplosionEffectTime = FMath::Max(0.f, SpellCast.Get(GBSpellParam::ExplosionEffectTime, Config.ExplosionEffectTime));
	Config.GravityScale = SpellCast.Get(GBStat::ProjectileGravity, 0.f);
	Config.bPierce = SpellCast.Get(GBSpellParam::Pierce, 0.f) > 0.5f;
	Config.HeadshotMult = SpellCast.Get(GBStat::HeadshotMult, 1.f);
	Config.bPixelArt = true;
	Config.SpriteId = GetSpriteId();
	Config.Color = GetSpellColor();
	Config.VisualScale = FMath::Max(0.01f, SpellCast.Get(GBSpellParam::VisualScale, 1.f));
	if (Config.MaxRange > 0.f)
	{
		Config.MaxLifetime = FMath::Max(Config.MaxLifetime, Config.MaxRange / Config.Speed + ProjectileSpellLifetimeMargin);
	}

	// Aim at what the crosshair is on, then fire from the hand so the shot does not start inside the camera.
	const FVector AimDir = SpellCast.Direction.GetSafeNormal().IsNearlyZero() ? GetActorForwardVector() : SpellCast.Direction.GetSafeNormal();
	const float TraceLength = Config.MaxRange > 0.f ? Config.MaxRange : Config.Speed * Config.MaxLifetime;
	FVector Target = SpellCast.Origin + AimDir * TraceLength;
	{
		FCollisionQueryParams Params(SCENE_QUERY_STAT(GBSpellProjectileAim), false, this);
		if (AActor* Caster = GetCaster())
		{
			Params.AddIgnoredActor(Caster);
		}
		FHitResult Hit;
		if (World->LineTraceSingleByChannel(Hit, SpellCast.Origin, Target, ECC_Visibility, Params))
		{
			Target = Hit.ImpactPoint;
		}
	}

	FVector Muzzle = SpellCast.Origin;
	if (const AGBPlayerCharacter* Player = Cast<AGBPlayerCharacter>(GetCaster()))
	{
		Muzzle = Player->GetMuzzleLocation();
	}
	FVector ShotDir = (Target - Muzzle).GetSafeNormal();
	if (ShotDir.IsNearlyZero() || FVector::DotProduct(ShotDir, AimDir) <= 0.f)
	{
		// The target is between the camera and the hand (point-blank wall): just fire along the aim.
		ShotDir = AimDir;
	}

	if (!AGBProjectile::SpawnProjectile(World, Config, Muzzle, ShotDir, GetCaster()))
	{
		UE_LOG(LogGB, Warning, TEXT("Spell '%s': the projectile could not be spawned."), *SpellCast.SpellId.ToString());
	}
	EndSpell();
}
