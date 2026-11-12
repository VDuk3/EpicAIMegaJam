#include "Weapons/GBGun.h"

#include "AI/GBEnemyCharacter.h"
#include "Camera/CameraComponent.h"
#include "Combat/GBCombatLibrary.h"
#include "Combat/GBProjectile.h"
#include "Components/StaticMeshComponent.h"
#include "Core/GBStatNames.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Player/GBPlayerCharacter.h"
#include "Stats/GBAttributeComponent.h"

void AGBGun::InitFromDefinition(const FGBWeaponDefinition& Definition, AGBPlayerCharacter* InOwner)
{
	Super::InitFromDefinition(Definition, InOwner);

	const FName Required[] = { GBStat::Damage, GBStat::FireInterval, GBStat::MagazineSize, GBStat::SpareMagazines, GBStat::ReloadTime, GBStat::Range };
	for (const FName& Stat : Required)
	{
		if (!Definition.Stats.Contains(Stat))
		{
			UE_LOG(LogGB, Warning, TEXT("Weapons.json '%s': missing stat '%s', using a default"), *Definition.Id.ToString(), *Stat.ToString());
		}
	}

	BuildViewModel(Definition);
}

// ---- Looks ----

UStaticMeshComponent* AGBGun::AddPart(FName Name, EGBShape Shape, const FVector& Location, const FVector& Size, const FLinearColor& Color, bool bGlow)
{
	UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(this, Name);
	Part->SetupAttachment(Root);
	Part->SetRelativeLocation(Location);
	Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Part->SetGenerateOverlapEvents(false);
	Part->SetCastShadow(false);
	Part->RegisterComponent();
	GBPlaceholder::SetupShape(Part, Shape, Size, Color, bGlow);
	Parts.Add(Part);
	return Part;
}

void AGBGun::BuildViewModel(const FGBWeaponDefinition& Definition)
{
	// Placeholder viewmodel (art rule: guns are 3D). Sizes in cm are purely visual; X = forward, Z = up.
	// The parts sit forward of the held-item anchor so the whole gun reads as one small pistol in the
	// lower right of the view, roughly 25-35 cm long, pointing forward.
	const float* PelletStat = Definition.Stats.Find(GBStat::Pellets);
	const bool bShotgun = PelletStat && *PelletStat > 1.f;
	const bool bAutomaticLook = Definition.bAutomatic;

	const float BodyLength = bShotgun ? 24.f : (bAutomaticLook ? 26.f : 20.f);
	const float BarrelLength = bShotgun ? 16.f : (bAutomaticLook ? 14.f : 9.f);
	const float BarrelThickness = bShotgun ? 4.f : 3.f;
	constexpr float BodyForward = 8.f;

	const FLinearColor BodyColor(0.08f, 0.08f, 0.10f);
	const FLinearColor BarrelColor(0.03f, 0.03f, 0.035f);
	const FLinearColor GripColor(0.12f, 0.10f, 0.10f);
	const FLinearColor NeonColor(0.0f, 0.9f, 1.0f);

	AddPart(TEXT("Body"), EGBShape::Cube, FVector(BodyForward, 0.f, 0.f), FVector(BodyLength, 5.f, 7.5f), BodyColor, false);
	AddPart(TEXT("Barrel"), EGBShape::Cube, FVector(BodyForward + BodyLength * 0.5f + BarrelLength * 0.5f, 0.f, 1.5f),
		FVector(BarrelLength, BarrelThickness, BarrelThickness), BarrelColor, false);
	AddPart(TEXT("Grip"), EGBShape::Cube, FVector(BodyForward - BodyLength * 0.5f + 2.5f, 0.f, -7.f), FVector(4.f, 4.f, 9.f), GripColor, false);
	if (bAutomaticLook)
	{
		AddPart(TEXT("Magazine"), EGBShape::Cube, FVector(BodyForward + 2.f, 0.f, -7.f), FVector(3.f, 2.5f, 8.f), BodyColor, false);
	}
	// Neon on the camera-facing side (the anchor sits right of the view, so +Y faces away).
	AddPart(TEXT("Neon"), EGBShape::Cube, FVector(BodyForward, -2.6f, 1.f), FVector(BodyLength * 0.8f, 0.35f, 0.7f), NeonColor, true);
	// Angle the whole viewmodel slightly toward the view center so the barrel points near the crosshair.
	Root->SetRelativeRotation(FRotator(0.f, -6.f, 0.f));
}

// ---- Ammo ----

int32 AGBGun::GetMagazineSize() const
{
	return FMath::Max(0, FMath::RoundToInt(GetStat(GBStat::MagazineSize, 1.f)));
}

int32 AGBGun::GetMaxSpareMagazines() const
{
	return FMath::Max(0, FMath::RoundToInt(GetStat(GBStat::SpareMagazines, 0.f)));
}

void AGBGun::SetAmmo(const FGBAmmoState& InAmmo)
{
	CancelReload();
	Ammo.InMagazine = FMath::Clamp(InAmmo.InMagazine, 0, GetMagazineSize());
	Ammo.SpareMagazines = FMath::Clamp(InAmmo.SpareMagazines, 0, GetMaxSpareMagazines());
	NotifyEquipmentChanged();
}

void AGBGun::RefillAmmo()
{
	CancelReload();
	Ammo.InMagazine = GetMagazineSize();
	Ammo.SpareMagazines = GetMaxSpareMagazines();
	NotifyEquipmentChanged();
}

int32 AGBGun::AddSpareMagazines(int32 Count)
{
	if (Count <= 0)
	{
		return 0;
	}
	const int32 Added = FMath::Clamp(GetMaxSpareMagazines() - Ammo.SpareMagazines, 0, Count);
	if (Added > 0)
	{
		Ammo.SpareMagazines += Added;
		NotifyEquipmentChanged();
	}
	return Added;
}

// ---- Reload ----

void AGBGun::Reload()
{
	if (bReloading || !bEquipped)
	{
		return;
	}
	if (Ammo.InMagazine >= GetMagazineSize() || Ammo.SpareMagazines <= 0)
	{
		return;
	}
	bReloading = true;
	ReloadElapsed = 0.f;
	ReloadDuration = FMath::Max(0.f, GetStat(GBStat::ReloadTime, 1.f));
	NotifyEquipmentChanged();
}

void AGBGun::CancelReload()
{
	if (!bReloading)
	{
		return;
	}
	bReloading = false;
	ReloadElapsed = 0.f;
	NotifyEquipmentChanged();
}

void AGBGun::FinishReload()
{
	bReloading = false;
	ReloadElapsed = 0.f;
	if (Ammo.SpareMagazines > 0)
	{
		// The rounds left in the old magazine are lost (provisional, classic tactical style).
		--Ammo.SpareMagazines;
		Ammo.InMagazine = GetMagazineSize();
	}
	NotifyEquipmentChanged();
}

float AGBGun::GetReloadProgress() const
{
	if (!bReloading)
	{
		return 0.f;
	}
	return ReloadDuration > KINDA_SMALL_NUMBER ? FMath::Clamp(ReloadElapsed / ReloadDuration, 0.f, 1.f) : 1.f;
}

// ---- Aim ----

void AGBGun::StartSecondary()
{
	if (bEquipped)
	{
		SetAiming(true);
	}
}

void AGBGun::StopSecondary()
{
	SetAiming(false);
}

void AGBGun::SetAiming(bool bInAiming)
{
	if (bAiming == bInAiming)
	{
		return;
	}
	bAiming = bInAiming;
	if (AGBPlayerCharacter* Player = GetPlayerOwner())
	{
		Player->SetMoveSpeedScale(bAiming ? GetStat(GBStat::AimMoveSpeedMult, 1.f) : 1.f);
	}
}

void AGBGun::UpdateAimFOV(float DeltaSeconds, bool bInstant)
{
	const AGBPlayerCharacter* Player = GetPlayerOwner();
	UCameraComponent* Camera = Player ? Player->GetCamera() : nullptr;
	if (!Camera)
	{
		return;
	}

	const float Target = bAiming ? 1.f : 0.f;
	if (AimAlpha == Target)
	{
		return;
	}
	if (AimAlpha <= 0.f)
	{
		HipFOV = Camera->FieldOfView;
	}

	if (bInstant)
	{
		AimAlpha = Target;
	}
	else
	{
		const float AimTime = GetStat(GBWeaponStat::AimTime, 0.15f);
		const float Step = AimTime > KINDA_SMALL_NUMBER ? DeltaSeconds / AimTime : 1.f;
		AimAlpha = FMath::Clamp(Target > AimAlpha ? AimAlpha + Step : AimAlpha - Step, 0.f, 1.f);
	}
	Camera->SetFieldOfView(FMath::Lerp(HipFOV, GetStat(GBStat::AimFOV, HipFOV), AimAlpha));
}

void AGBGun::OnUnequipped()
{
	CancelReload();
	SetAiming(false);
	UpdateAimFOV(0.f, true);
	Super::OnUnequipped();
}

void AGBGun::Tick(float DeltaSeconds)
{
	if (bReloading)
	{
		ReloadElapsed += DeltaSeconds;
		if (ReloadElapsed >= ReloadDuration)
		{
			FinishReload();
		}
	}
	if (bEquipped)
	{
		UpdateAimFOV(DeltaSeconds, false);
	}
	Super::Tick(DeltaSeconds);
}

// ---- Shooting ----

bool AGBGun::PerformAttack()
{
	AGBPlayerCharacter* Player = GetPlayerOwner();
	if (!Player || !GetWorld() || bReloading)
	{
		return false;
	}
	if (Ammo.InMagazine <= 0)
	{
		Reload();
		return false;
	}

	const FGBDamageInfo BaseInfo = MakeDamageInfo();
	const float SpreadDegrees = bAiming ? GetStat(GBStat::AimSpread, 0.f) : GetStat(GBStat::Spread, 0.f);

	--Ammo.InMagazine;

	const FGBWeaponDefinition* Def = GetDefinition();
	if (Def && Def->FireMode.Equals(TEXT("Projectile"), ESearchCase::IgnoreCase))
	{
		FireProjectile(BaseInfo, SpreadDegrees);
	}
	else
	{
		FireHitscan(BaseInfo, SpreadDegrees);
	}

	const float RecoilYaw = GetStat(GBStat::RecoilYaw, 0.f);
	Player->AddRecoil(GetStat(GBStat::RecoilPitch, 0.f), Random.FRandRange(-RecoilYaw, RecoilYaw), GetStat(GBStat::RecoilRecovery, 0.f));

	NotifyEquipmentChanged();

	if (Ammo.InMagazine <= 0)
	{
		Reload();
	}
	return true;
}

void AGBGun::FireHitscan(const FGBDamageInfo& BaseInfo, float SpreadDegrees)
{
	AGBPlayerCharacter* Player = GetPlayerOwner();
	UWorld* World = GetWorld();

	const FVector Origin = Player->GetAimOrigin();
	const FVector AimDirection = Player->GetAimDirection();
	const FVector Muzzle = Player->GetMuzzleLocation();
	const float Range = GetStat(GBStat::Range, 5000.f);
	const int32 Pellets = FMath::Max(1, FMath::RoundToInt(GetStat(GBStat::Pellets, 1.f)));
	const float FalloffStart = GetStat(GBStat::FalloffStart, 0.f);
	const float FalloffEnd = GetStat(GBStat::FalloffEnd, 0.f);
	const float NearMult = GetStat(GBStat::FalloffNearMult, 1.f);
	const float FarMult = GetStat(GBStat::FalloffFarMult, 1.f);
	const float HeadshotMult = GetStat(GBStat::HeadshotMult, 1.f);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(GBGunTrace), false, Player);
	Params.AddIgnoredActor(this);

	for (int32 Index = 0; Index < Pellets; ++Index)
	{
		const FVector Direction = UGBCombatLibrary::ApplySpread(AimDirection, SpreadDegrees, Random);
		const FVector End = Origin + Direction * Range;

		FHitResult Hit;
		const bool bHit = World->LineTraceSingleByChannel(Hit, Origin, End, GBWeapon::AimTraceChannel, Params);
		AActor* Target = bHit ? Hit.GetActor() : nullptr;

		if (Target && UGBAttributeComponent::Find(Target))
		{
			FGBDamageInfo Info = BaseInfo;
			Info.HitLocation = Hit.ImpactPoint;
			Info.HitDirection = Direction;
			Info.Amount *= UGBCombatLibrary::DistanceFalloffMultiplier(Hit.Distance, FalloffStart, FalloffEnd, NearMult, FarMult);
			UGBCombatLibrary::ApplyWeakPointBonus(Target, Info, HeadshotMult);
			UGBCombatLibrary::DealDamage(Target, Info);
		}

#if !UE_BUILD_SHIPPING
		// Placeholder tracer.
		DrawDebugLine(World, Muzzle, bHit ? Hit.ImpactPoint : End, FColor(255, 220, 120), false, 0.05f, 0, 0.6f);
#endif
	}
}

void AGBGun::FireProjectile(const FGBDamageInfo& BaseInfo, float SpreadDegrees)
{
	AGBPlayerCharacter* Player = GetPlayerOwner();
	UWorld* World = GetWorld();

	const float Range = GetStat(GBStat::Range, 5000.f);
	const FVector Muzzle = Player->GetMuzzleLocation();
	FVector AimDirection = (FindAimPoint(Range) - Muzzle).GetSafeNormal();
	if (AimDirection.IsNearlyZero())
	{
		AimDirection = Player->GetAimDirection();
	}

	FGBProjectileConfig Config;
	Config.Damage = BaseInfo;
	Config.Speed = GetStat(GBStat::ProjectileSpeed, 3000.f);
	Config.Radius = GetStat(GBStat::ProjectileRadius, 5.f);
	Config.GravityScale = GetStat(GBStat::ProjectileGravity, 0.f);
	Config.MaxRange = Range;
	Config.bPixelArt = false; // art rule: gun shots are 3D
	Config.Color = GBPlaceholder::ElementColor(static_cast<uint8>(BaseInfo.Element));

	const int32 Pellets = FMath::Max(1, FMath::RoundToInt(GetStat(GBStat::Pellets, 1.f)));
	for (int32 Index = 0; Index < Pellets; ++Index)
	{
		const FVector Direction = UGBCombatLibrary::ApplySpread(AimDirection, SpreadDegrees, Random);
		AGBProjectile::SpawnProjectile(World, Config, Muzzle, Direction, Player);
	}
}
