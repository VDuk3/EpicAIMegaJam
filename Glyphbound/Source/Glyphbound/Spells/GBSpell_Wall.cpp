#include "Spells/GBSpell_Wall.h"
#include "Components/BillboardComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/GBPlaceholder.h"
#include "Core/GBStatNames.h"
#include "Engine/CollisionProfile.h"
#include "Stats/GBAttributeComponent.h"
#include "Stats/GBStatsComponent.h"
#include "TimerManager.h"

namespace
{
	// Visual and geometric limits only.
	constexpr int32 MaxWallSprites = 8;
	/** Pawns pressed against a blocking wall still count as touching it for damage ticks. */
	constexpr float WallTouchMarginCm = 15.f;
	constexpr float WallGroundSearchExtraCm = 2000.f;
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

	// Stand on the ground in front of the caster, facing the caster's yaw.
	AActor* Caster = GetCaster();
	const FVector CasterLocation = Caster ? Caster->GetActorLocation() : SpellCast.Origin;
	const FRotator WallRotation(0.f, SpellCast.Direction.Rotation().Yaw, 0.f);
	const FVector Point = CasterLocation + WallRotation.Vector() * Distance;
	FVector Ground;
	if (!FindGround(Point + FVector::UpVector * Height, Ground, Height + WallGroundSearchExtraCm))
	{
		Ground = Point;
		Ground.Z = CasterLocation.Z - (Caster ? Caster->GetSimpleCollisionHalfHeight() : 0.f);
	}
	SetActorLocationAndRotation(Ground + FVector::UpVector * (Height * 0.5f), WallRotation, false, nullptr, ETeleportType::TeleportPhysics);

	const FVector Size(Thickness, Width, Height);
	Box->SetBoxExtent(Size * 0.5f);
	GBPlaceholder::SetupShape(Visual, EGBShape::Cube, Size, GetSpellColor(), true);

	// A row of camera-facing sprites along the wall (pixel-art flames / ice).
	if (!GetSpriteId().IsNone())
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
	const FVector Extent = Box->GetUnscaledBoxExtent() + FVector(WallTouchMarginCm);
	GatherHostilesInBox(GetActorLocation(), GetActorQuat(), Extent, Targets);
	for (AActor* Target : Targets)
	{
		DamageTarget(Target, DamagePerTick, GetActorLocation());
	}
}
