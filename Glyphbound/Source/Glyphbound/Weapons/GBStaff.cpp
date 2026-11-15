#include "Weapons/GBStaff.h"

#include "Combat/GBCombatLibrary.h"
#include "Combat/GBExplosionEffect.h"
#include "Combat/GBProjectile.h"
#include "Core/GBPlaceholder.h"
#include "Core/GBStatNames.h"
#include "Data/GBDataSubsystem.h"
#include "Player/GBPlayerCharacter.h"
#include "Stats/GBAttributeComponent.h"

namespace GBStaffLocal
{
	// Weapons.json staff stats for the impact splash (Ember and Storm).
	const FName SplashRadius(TEXT("SplashRadius"));                 // cm; 0 = no splash
	const FName SplashDamageFraction(TEXT("SplashDamageFraction")); // fraction of the bolt's hit dealt to the others around
}

bool AGBStaff::PerformAttack()
{
	AGBPlayerCharacter* Player = GetPlayerOwner();
	UWorld* World = GetWorld();
	if (!Player || !World)
	{
		return false;
	}

	const float ManaCost = GetStat(GBStat::ManaCost, 0.f);
	if (ManaCost > 0.f)
	{
		UGBAttributeComponent* Attributes = Player->GetAttributes();
		if (!Attributes || !Attributes->SpendMana(ManaCost))
		{
			return false;
		}
	}

	const float Range = GetStat(GBStat::Range, 5000.f);
	const FVector Muzzle = Player->GetMuzzleLocation();
	FVector Direction = FindAimPoint(Range) - Muzzle;
	if (Direction.IsNearlyZero())
	{
		Direction = Player->GetAimDirection();
	}

	FGBProjectileConfig Config;
	Config.Damage = MakeDamageInfo();
	Config.HeadshotMult = GetStat(GBStat::HeadshotMult, 1.f);
	Config.Speed = GetStat(GBStat::ProjectileSpeed, 3000.f);
	Config.Radius = GetStat(GBStat::ProjectileRadius, 10.f);
	Config.GravityScale = GetStat(GBStat::ProjectileGravity, 0.f);
	Config.MaxRange = Range;
	if (const UGBDataSubsystem* Data = UGBDataSubsystem::Get(this))
	{
		Config.MaxLifetime = Data->GetGameValue(GBProjectileKey::ProjectileMaxLifetime, Config.MaxLifetime);
	}
	Config.bPixelArt = true;
	Config.Color = GBPlaceholder::ElementColor(static_cast<uint8>(Config.Damage.Element));
	if (const FGBWeaponDefinition* Def = GetDefinition())
	{
		if (const FString* Sprite = Def->Strings.Find(GBString::ProjectileSprite))
		{
			Config.SpriteId = FName(**Sprite);
		}
	}

	AGBProjectile* Projectile = AGBProjectile::SpawnProjectile(World, Config, Muzzle, Direction, Player);
	if (!Projectile)
	{
		return false;
	}

	// Splash (Weapons.json SplashRadius > 0, SplashDamageFraction): a fraction of the hit dealt around the
	// impact to the other hostiles (never the player: DealRadialDamage only hurts hostiles, and he is ignored).
	const float SplashRadius = GetStat(GBStaffLocal::SplashRadius, 0.f);
	const float SplashFraction = GetStat(GBStaffLocal::SplashDamageFraction, 0.f);
	if (SplashRadius > 0.f && SplashFraction > 0.f)
	{
		FGBDamageInfo SplashInfo = Config.Damage;
		SplashInfo.Amount *= SplashFraction;
		SplashInfo.BuildUp *= SplashFraction;
		SplashInfo.OnHitEffect = NAME_None;
		const FLinearColor SplashColor = Config.Color;
		const TWeakObjectPtr<AGBPlayerCharacter> WeakPlayer(Player);
		Projectile->OnImpact.AddLambda([SplashInfo, SplashRadius, SplashColor, WeakPlayer](AGBProjectile* Shot, AActor* HitActor, const FVector& Location)
		{
			// Null = the bolt reached the end of its range in the air: no impact, no splash.
			if (!HitActor || !Shot)
			{
				return;
			}
			TArray<AActor*, TInlineAllocator<2>> Ignore;
			if (AGBPlayerCharacter* Owner = WeakPlayer.Get())
			{
				Ignore.Add(Owner);
			}
			// The directly hit target already took the full hit; a wall is not ignored, so it still blocks the splash.
			if (UGBCombatLibrary::CanDamage(WeakPlayer.Get(), HitActor))
			{
				Ignore.Add(HitActor);
			}
			FGBDamageInfo Info = SplashInfo;
			Info.Causer = Shot;
			UGBCombatLibrary::DealRadialDamage(Shot, Location, SplashRadius, Info, Ignore, /*bLinearFalloff*/ false);
			AGBExplosionEffect::Spawn(Shot->GetWorld(), Location, SplashRadius, SplashColor, 0.25f);
		});
	}
	return true;
}
