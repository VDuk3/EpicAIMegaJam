#include "Spells/GBSpell_Wall.h"
#include "Components/BillboardComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/GBPlaceholder.h"
#include "Core/GBStatNames.h"
#include "Data/GBDataSubsystem.h"
#include "Engine/CollisionProfile.h"
#include "Stats/GBAttributeComponent.h"
#include "Stats/GBStatsComponent.h"
#include "TimerManager.h"

namespace
{
	// Visual and geometric limits only.
	constexpr int32 MaxWallSprites = 8;
	constexpr float WallGroundSearchExtraCm = 2000.f;
	// A wall that cannot stand at its full distance stops this far from what is in its way, and never closer to
	// its caster's body than this.
	constexpr float WallObstacleGapCm = 5.f;
	constexpr float WallCasterGapCm = 10.f;
}

AGBSpell_Wall::AGBSpell_Wall()
{
	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	Box->SetupAttachment(Root);
	Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Box->SetGenerateOverlapEvents(false);

	Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));
	Visual->SetupAttachment(Root);
	Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Visual->SetGenerateOverlapEvents(false);
	Visual->SetCastShadow(false);
}

void AGBSpell_Wall::StartSpell()
{
	const float Distance = ReadParam(GBSpellParam::Distance, 300.f);
	const float Width = FMath::Max(1.f, ReadParam(GBStat::Width, 300.f));
	const float Height = FMath::Max(1.f, ReadParam(GBStat::Height, 200.f));
	const float Thickness = FMath::Max(1.f, ReadParam(GBStat::Thickness, 50.f));
	const float Duration = ReadParam(GBStat::Duration, 5.f);
	const bool bBlocking = SpellCast.Get(GBSpellParam::Blocking, 0.f) > 0.5f;

	// Stand on the ground in front of the caster, facing the caster's yaw. The wall always appears, whatever its size
	// and whatever the room (owner, 2026-11-14): it stops short of anything solid in its way, and its ground is
	// searched from the caster's own height. (It used to be searched from the wall's height above the caster: with a
	// wall taller than the room that started over the ceiling, and the wall rose on the roof, out of sight.)
	AActor* Caster = GetCaster();
	const FVector CasterLocation = Caster ? Caster->GetActorLocation() : SpellCast.Origin;
	const float CasterHalfHeight = Caster ? Caster->GetSimpleCollisionHalfHeight() : 0.f;
	const float CasterRadius = Caster ? Caster->GetSimpleCollisionRadius() : 0.f;
	const FRotator WallRotation(0.f, SpellCast.Direction.Rotation().Yaw, 0.f);
	const FVector Forward = WallRotation.Vector();
	float Reach = Distance;
	bool bOnCaster = false;
	if (UWorld* World = GetWorld())
	{
		FCollisionQueryParams Params(SCENE_QUERY_STAT(GBSpellWallReach), false, this);
		if (Caster)
		{
			Params.AddIgnoredActor(Caster);
		}
		FHitResult Blocked;
		const FVector End = CasterLocation + Forward * (Distance + Thickness * 0.5f);
		if (World->LineTraceSingleByObjectType(Blocked, CasterLocation, End, FCollisionObjectQueryParams(ECC_WorldStatic), Params))
		{
			// In front of what is in the way. With the caster's face against it there is no room in front: the wall
			// then stands where the caster is, in sight, and the two do not block each other. Keeping it clear of
			// the caster instead would push it into the obstacle, out of sight (code review, 2026-11-14).
			const float Fits = Blocked.Distance - Thickness * 0.5f - WallObstacleGapCm;
			Reach = FMath::Clamp(Fits, FMath::Min(Thickness * 0.5f, Distance), Distance);
			bOnCaster = Reach < FMath::Min(CasterRadius + Thickness * 0.5f + WallCasterGapCm, Distance);
		}
	}
	const FVector Point = CasterLocation + Forward * Reach;
	FVector Ground;
	if (!FindGroundUnder(Point, CasterHalfHeight, Ground, CasterHalfHeight + WallGroundSearchExtraCm))
	{
		Ground = Point;
		Ground.Z = CasterLocation.Z - CasterHalfHeight;
	}
	SetActorLocationAndRotation(Ground + FVector::UpVector * (Height * 0.5f), WallRotation, false, nullptr, ETeleportType::TeleportPhysics);

	const FVector Size(Thickness, Width, Height);
	Box->SetBoxExtent(Size * 0.5f);

	const FString TextureId = SpellCast.GetString(GBSpellString::WallTexture);
	if (!TextureId.IsEmpty())
	{
		// A structure (ice wall): one solid pixel-textured box, no camera-facing sprites (GDD 3).
		const float TileSize = ReadParam(GBSpellParam::WallTileSize, 100.f);
		const bool bGlow = SpellCast.Get(GBSpellParam::WallGlow, 0.f) > 0.5f;
		const FString TintStr = SpellCast.GetString(FName(TEXT("Tint")));
		const FLinearColor Tinted = TintStr.IsEmpty() ? FLinearColor::White : UGBDataSubsystem::ParseColor(TintStr, FLinearColor::White);
		GBPlaceholder::SetupTexturedShape(Visual, EGBShape::Cube, Size, FName(*TextureId), TileSize, Tinted, bGlow);
		Visual->SetCastShadow(true);
	}
	else
	{
		// An effect (fire wall): a glowing box plus a row of camera-facing sprites (pixel-art flames).
		GBPlaceholder::SetupShape(Visual, EGBShape::Cube, Size, GetSpellColor(), true);
	}

	if (TextureId.IsEmpty() && !GetSpriteId().IsNone())
	{
		const float SpriteHeight = FMath::Max(1.f, SpellCast.Get(GBSpellParam::SpriteSize, Height));
		const int32 Count = FMath::Clamp(FMath::RoundToInt(Width / SpriteHeight), 1, MaxWallSprites);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			UBillboardComponent* Sprite = NewObject<UBillboardComponent>(this);
			Sprite->SetupAttachment(Root);
			Sprite->RegisterComponent();
			AddInstanceComponent(Sprite);
			Sprite->SetRelativeLocation(FVector(0.f, -Width * 0.5f + (Index + 0.5f) * Width / Count, 0.f));
			SetupSpellSprite(Sprite, Height);
			Sprites.Add(Sprite);
		}
	}

	if (bBlocking)
	{
		Box->SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);
		if (bOnCaster && Caster)
		{
			// Raised on its caster for lack of room: it never traps the one who cast it.
			Box->IgnoreActorWhenMoving(Caster, true);
			if (UPrimitiveComponent* CasterBody = Cast<UPrimitiveComponent>(Caster->GetRootComponent()))
			{
				CasterBody->IgnoreActorWhenMoving(this, true);
			}
		}
		// Projectiles only overlap WorldDynamic: without this they pass through the wall untouched.
		Box->SetGenerateOverlapEvents(true);
		SetupHealth(ReadParam(GBStat::Health, 100.f));
	}

	const float DamagePerSecond = SpellCast.Get(GBStat::DamagePerSecond, 0.f);
	if (DamagePerSecond > 0.f)
	{
		const float Interval = SafeInterval(ReadParam(GBStat::TickInterval, 0.5f));
		DamagePerTick = DamagePerSecond * Interval;
		GetWorldTimerManager().SetTimer(TickTimer, this, &AGBSpell_Wall::DamageTick, Interval, true);
	}

	EndSpellAfter(Duration);
}

void AGBSpell_Wall::SetupHealth(float Health)
{
	UGBStatsComponent* WallStats = NewObject<UGBStatsComponent>(this, TEXT("WallStats"));
	WallStats->RegisterComponent();
	AddInstanceComponent(WallStats);
	WallStats->SetBaseStat(GBStat::MaxHealth, FMath::Max(1.f, Health));
	WallStats->SetBaseStat(GBStat::DamageTaken, SpellCast.Get(GBStat::DamageTaken, 1.f));
	WallStats->SetBaseStat(GBStat::HealthRegen, SpellCast.Get(GBStat::HealthRegen, 0.f));
	WallStats->SetBaseStat(GBStat::MaxMana, 0.f);

	UGBAttributeComponent* WallAttributes = NewObject<UGBAttributeComponent>(this, TEXT("WallAttributes"));
	// Neutral: nobody targets the wall on purpose, but shots and enemies that hit it wear it down.
	WallAttributes->Team = EGBTeam::Neutral;
	WallAttributes->RegisterComponent();
	AddInstanceComponent(WallAttributes);
	WallAttributes->InitializeFromStats();
	WallAttributes->OnDeath.AddUObject(this, &AGBSpell_Wall::HandleWallDeath);
}

void AGBSpell_Wall::HandleWallDeath(const FGBDamageInfo& KillingBlow)
{
	// Not inside the damage call that killed it.
	EndSpellAfter(0.f);
}

void AGBSpell_Wall::DamageTick()
{
	TArray<AActor*> Targets;
	// Pawns pressed against a blocking wall still count as touching it for damage ticks.
	const float TouchMargin = ReadParam(GBSpellParam::TouchMargin, 15.f);
	const FVector Extent = Box->GetUnscaledBoxExtent() + FVector(TouchMargin);
	GatherHostilesInBox(GetActorLocation(), GetActorQuat(), Extent, Targets);
	for (AActor* Target : Targets)
	{
		DamageTarget(Target, DamagePerTick, GetActorLocation());
	}
}
