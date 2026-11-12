#include "Weapons/GBStaff.h"

#include "Combat/GBProjectile.h"
#include "Core/GBPlaceholder.h"
#include "Core/GBStatNames.h"
#include "Data/GBDataSubsystem.h"
#include "Player/GBPlayerCharacter.h"
#include "Stats/GBAttributeComponent.h"

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

	return AGBProjectile::SpawnProjectile(World, Config, Muzzle, Direction, Player) != nullptr;
}
