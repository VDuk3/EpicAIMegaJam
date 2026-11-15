#include "Spells/GBSpell_Projectile.h"
#include "Combat/GBProjectile.h"
#include "Core/GBStatNames.h"
#include "Data/GBDataSubsystem.h"
#include "Engine/World.h"
#include "Player/GBPlayerCharacter.h"
#include "Combat/GBCombatLibrary.h"
#include "Engine/OverlapResult.h"

// Defined in Combat/GBProjectile.cpp (instant lightning look).
namespace GBLightningFx
{
	bool IsArcClip(FName ClipId);
	void SpawnArc(UWorld* World, const FVector& Start, const FVector& End, float HeightPer100);
	void SpawnImpact(UWorld* World, FName ClipId, const FVector& At, float MinWidthCm, float HeightPer100);
}

// Area hit on impact (owner, 2026-11-15, Lightning Bolt): Spells.json Params "SplashRadius" (cm, 0 = none) and
// "SplashDamageFraction" (share of the hit dealt to the other hostiles around the hit point, no falloff). Never the
// caster; the directly hit enemy already took the full hit. Same rule as the Storm staff's splash (GBStaff.cpp).
// A lightning spell also draws a small arc and a hit to each hostile around.
namespace GBSpellSplash
{
	const FName Radius(TEXT("SplashRadius"));
	const FName DamageFraction(TEXT("SplashDamageFraction"));
	constexpr int32 MaxSmallArcs = 6;

	void Bind(AGBProjectile* Projectile, const FGBProjectileConfig& Config, float SplashRadius, float Fraction, AActor* Caster)
	{
		if (!Projectile || SplashRadius <= 0.f || Fraction <= 0.f)
		{
			return;
		}
		FGBDamageInfo SplashInfo = Config.Damage;
		SplashInfo.Amount *= Fraction;
		SplashInfo.BuildUp *= Fraction;
		SplashInfo.OnHitEffect = NAME_None;
		const bool bLightning = GBLightningFx::IsArcClip(Config.FlightVfxId);
		const FName ImpactClip = Config.ImpactVfxId;
		const float HeightPer100 = Config.FlightVfxHeight;
		const TWeakObjectPtr<AActor> WeakCaster(Caster);
		Projectile->OnImpact.AddLambda([SplashInfo, SplashRadius, bLightning, ImpactClip, HeightPer100, WeakCaster](AGBProjectile* Shot, AActor* HitActor, const FVector& Location)
		{
			// Null = it reached the end of its range in the air: no area hit.
			UWorld* World = Shot ? Shot->GetWorld() : nullptr;
			if (!HitActor || !World)
			{
				return;
			}
			AActor* Owner = WeakCaster.Get();
			TArray<AActor*, TInlineAllocator<2>> Ignore;
			if (Owner)
			{
				Ignore.Add(Owner);
			}
			if (UGBCombatLibrary::CanDamage(Owner, HitActor))
			{
				Ignore.Add(HitActor);
			}
			if (bLightning)
			{
				// Visual only: small arcs from the hit point to the hostiles around it (before the damage, which may kill them).
				FCollisionObjectQueryParams Objects;
				Objects.AddObjectTypesToQuery(ECC_Pawn);
				TArray<FOverlapResult> Overlaps;
				World->OverlapMultiByObjectType(Overlaps, Location, FQuat::Identity, Objects, FCollisionShape::MakeSphere(SplashRadius));
				TSet<AActor*> Seen;
				for (const FOverlapResult& Overlap : Overlaps)
				{
					AActor* Other = Overlap.GetActor();
					if (Seen.Num() >= MaxSmallArcs || !IsValid(Other) || Seen.Contains(Other) || Ignore.Contains(Other) || !UGBCombatLibrary::CanDamage(Owner, Other))
					{
						continue;
					}
					Seen.Add(Other);
					const FVector To = UGBCombatLibrary::GetDamageTargetLocation(Other, Location);
					GBLightningFx::SpawnArc(World, Location, To, HeightPer100 * 0.5f);
					GBLightningFx::SpawnImpact(World, ImpactClip, To, 60.f, HeightPer100 * 0.5f);
				}
			}
			FGBDamageInfo Info = SplashInfo;
			Info.Causer = Shot;
			UGBCombatLibrary::DealRadialDamage(Shot, Location, SplashRadius, Info, Ignore, /*bLinearFalloff*/ false);
		});
	}
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
	// Optional baked clips (GBPixelVfx): Strings "FlightVfx" / "ImpactVfx", sizes in Params. Without them the spell looks as before.
	const FString FlightVfx = SpellCast.GetString(GBString::FlightVfx);
	const FString ImpactVfx = SpellCast.GetString(GBString::ImpactVfx);
	Config.FlightVfxId = FlightVfx.IsEmpty() ? FName(NAME_None) : FName(*FlightVfx);
	Config.ImpactVfxId = ImpactVfx.IsEmpty() ? FName(NAME_None) : FName(*ImpactVfx);
	Config.FlightVfxHeight = SpellCast.Get(TEXT("FlightVfxHeight"), Config.FlightVfxHeight);
	Config.ImpactVfxSize = SpellCast.Get(TEXT("ImpactVfxSize"), Config.ImpactVfxSize);
	Config.ImpactVfxRate = SpellCast.Get(TEXT("ImpactVfxRate"), Config.ImpactVfxRate);
	const UGBDataSubsystem* Data = UGBDataSubsystem::Get(this);
	Config.MaxLifetime = Data ? Data->GetGameValue(GBProjectileKey::ProjectileMaxLifetime, Config.MaxLifetime) : Config.MaxLifetime;
	if (Config.MaxRange > 0.f && Config.Speed > 0.f)
	{
		// Extra seconds of life on top of Range / Speed, so the projectile is never removed before reaching its range.
		const float Margin = Data ? Data->GetGameValue(GBProjectileKey::SpellProjectileLifetimeMargin, 1.f) : 1.f;
		Config.MaxLifetime = FMath::Max(Config.MaxLifetime, Config.MaxRange / Config.Speed + Margin);
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

	AGBProjectile* Projectile = AGBProjectile::SpawnProjectile(World, Config, Muzzle, ShotDir, GetCaster());
	if (!Projectile)
	{
		UE_LOG(LogGB, Warning, TEXT("Spell '%s': the projectile could not be spawned."), *SpellCast.SpellId.ToString());
	}
	else
	{
		GBSpellSplash::Bind(Projectile, Config, SpellCast.Get(GBSpellSplash::Radius, 0.f), SpellCast.Get(GBSpellSplash::DamageFraction, 0.f), GetCaster());
	}
	EndSpell();
}
