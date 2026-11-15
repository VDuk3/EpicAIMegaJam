#include "Spells/GBSpell_Wall.h"
#include "Components/BillboardComponent.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/GBPlaceholder.h"
#include "Core/GBStatNames.h"
#include "Core/GBTypes.h"
#include "Data/GBDataSubsystem.h"
#include "Engine/CollisionProfile.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GBPixelVfxComponent.h"
#include "GBPixelVfxSubsystem.h"
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

// The fire wall's flames (owner, 2026-11-15: animated pixel art like the flamethrower, not one picture repeated).
// Tall looping flame columns (GBPixelVfx clips, Strings "FlameVfx" + A/B/C, drawn by Tools/Jam/make_fire_wall.js)
// stand side by side in two rows across the wall, each at its own phase and size. They rise out of the ground when
// the wall appears and sink in its last moments; one unshadowed light flickers with them.
// The same numbers are in make_fire_wall.js (WALL), which renders the preview.
namespace GBWallFlames
{
	const TCHAR* const VariantSuffixes[] = { TEXT("A"), TEXT("B"), TEXT("C") };
	const FName LightName(TEXT("FireWallLight"));
	/** A column is this much taller than the wall, so the tongues lick over its top. */
	constexpr float HeightShare = 1.15f;
	/** Distance between columns of one row, as a share of a column's width (they overlap). */
	constexpr float SpacingShare = 0.5f;
	constexpr int32 MaxColumnsPerRow = 10;
	/** The row away from the caster is a little lower. */
	constexpr float BackRowScale = 0.85f;
	constexpr float RiseSeconds = 0.35f;
	constexpr float SinkSeconds = 0.4f;
	constexpr float MinScale = 0.12f;
	constexpr float LightCandelas = 40.f;
	constexpr float LightRadius = 1000.f;

	float Hash01(int32 A, int32 B)
	{
		uint32 H = uint32(A) * 73856093u ^ uint32(B) * 19349663u;
		H ^= H >> 16; H *= 0x85ebca6bu; H ^= H >> 13; H *= 0xc2b2ae35u; H ^= H >> 16;
		return float(H >> 8) / 16777215.f;
	}

	const FGBPixelVfxClip* FindClip(const UObject* WorldContext, const FString& ClipBase, int32 Variant)
	{
		if (ClipBase.IsEmpty())
		{
			return nullptr;
		}
		const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
		const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		const UGBPixelVfxSubsystem* Vfx = GameInstance ? GameInstance->GetSubsystem<UGBPixelVfxSubsystem>() : nullptr;
		return Vfx ? Vfx->FindClip(FName(*(ClipBase + VariantSuffixes[Variant % UE_ARRAY_COUNT(VariantSuffixes)]))) : nullptr;
	}

	/** Rise at the start, full while it burns, sink at the end. */
	float ScaleAt(float Elapsed, float Duration)
	{
		const float Rise = FMath::Clamp(Elapsed / RiseSeconds, 0.f, 1.f);
		const float Up = 1.f - FMath::Square(1.f - Rise);
		const float Left = Duration - Elapsed;
		const float Down = (Duration > 0.f && Left < SinkSeconds) ? FMath::Max(0.f, Left / SinkSeconds) : 1.f;
		return FMath::Max(MinScale, Up * Down);
	}

	/** Builds the columns and the light, and starts their rise. False (nothing built) when the clips are missing. */
	bool Build(AActor* Wall, USceneComponent* Root, const FString& ClipBase, float Width, float Height, float Thickness, float Duration)
	{
		const FGBPixelVfxClip* First = FindClip(Wall, ClipBase, 0);
		UWorld* World = Wall ? Wall->GetWorld() : nullptr;
		if (!World || !First || First->WorldHeightCm <= 0.f || First->WorldWidthCm <= 0.f || First->Fps <= 0 || First->FrameCount <= 0)
		{
			return false;
		}
		const FName FirstId = First->Id;
		const float BaseScale = Height * HeightShare / First->WorldHeightCm;
		const float ColumnWidth = First->WorldWidthCm * BaseScale;
		const float ClipSeconds = float(First->FrameCount) / float(First->Fps);
		const int32 Count = FMath::Clamp(FMath::RoundToInt(Width / (ColumnWidth * SpacingShare)), 2, MaxColumnsPerRow);
		TArray<TPair<TWeakObjectPtr<UGBPixelVfxComponent>, float>> Columns;
		for (int32 Row = 0; Row < 2; ++Row)
		{
			const int32 InRow = Row == 0 ? Count : Count - 1;
			for (int32 Index = 0; Index < InRow; ++Index)
			{
				const int32 Serial = Columns.Num();
				const FGBPixelVfxClip* Variant = FindClip(Wall, ClipBase, Serial);
				const float Scale = BaseScale * (Row == 0 ? 1.f : BackRowScale) * (0.92f + 0.16f * Hash01(Serial, 7));
				UGBPixelVfxComponent* Column = NewObject<UGBPixelVfxComponent>(Wall, NAME_None, RF_Transient);
				Column->bEnableLight = false;
				Column->SizeScale = Scale * MinScale;
				Column->StartTimeOffset = Hash01(Serial, 3) * ClipSeconds;
				Column->AttachToComponent(Root, FAttachmentTransformRules::KeepRelativeTransform);
				// On the ground (the actor stands at half the wall's height); -X is the caster's side.
				Column->SetRelativeLocation(FVector((Row == 0 ? -0.25f : 0.25f) * Thickness,
					-Width * 0.5f + (Index + 0.5f + Row * 0.5f) * Width / Count, -Height * 0.5f));
				Column->RegisterComponent();
				if (Column->Play(Variant ? Variant->Id : FirstId))
				{
					Columns.Emplace(Column, Scale);
				}
				else
				{
					Column->DestroyComponent();
				}
			}
		}
		if (Columns.Num() == 0)
		{
			return false;
		}
		UPointLightComponent* Light = NewObject<UPointLightComponent>(Wall, LightName, RF_Transient);
		Light->SetMobility(EComponentMobility::Movable);
		Light->SetIntensityUnits(ELightUnits::Candelas);
		Light->SetLightColor(FLinearColor(FColor(0xF3, 0x61, 0x0B)));
		Light->SetAttenuationRadius(LightRadius);
		Light->SetCastShadows(false);
		Light->SetIntensity(0.f);
		Light->AttachToComponent(Root, FAttachmentTransformRules::KeepRelativeTransform);
		Light->RegisterComponent();

		// The wall actor does not tick: a timer drives the rise, the sink and the light's flicker (in steps, like the
		// frames). It dies with the wall.
		const double StartTime = World->GetTimeSeconds();
		const TWeakObjectPtr<UPointLightComponent> WeakLight(Light);
		FTimerHandle Handle;
		World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(Wall, [Wall, Columns, WeakLight, StartTime, Duration]()
		{
			const UWorld* TimerWorld = Wall->GetWorld();
			if (!TimerWorld)
			{
				return;
			}
			const float Elapsed = static_cast<float>(TimerWorld->GetTimeSeconds() - StartTime);
			const float Scale = ScaleAt(Elapsed, Duration);
			for (const TPair<TWeakObjectPtr<UGBPixelVfxComponent>, float>& Column : Columns)
			{
				if (UGBPixelVfxComponent* Flame = Column.Key.Get())
				{
					Flame->SizeScale = Column.Value * Scale;
				}
			}
			if (UPointLightComponent* FlameLight = WeakLight.Get())
			{
				FlameLight->SetIntensity(LightCandelas * Scale * (0.75f + 0.25f * Hash01(FMath::FloorToInt(Elapsed * 12.f), 11)));
			}
		}), 1.f / 30.f, true, 0.f);
		UE_LOG(LogGB, Display, TEXT("Wall: flames '%s' with %d columns"), *ClipBase, Columns.Num());
		return true;
	}
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

	// The fire wall's flames (Strings "FlameVfx"); without its clips, the old look: the sprite in a row.
	const FString FlameClips = TextureId.IsEmpty() ? SpellCast.GetString(TEXT("FlameVfx")) : FString();
	const bool bFlames = GBWallFlames::Build(this, Root, FlameClips, Width, Height, Thickness, Duration);
	if (bFlames)
	{
		// The flames are the wall: the plain glowing box under them would only flatten them.
		Visual->SetVisibility(false);
	}
	else if (!FlameClips.IsEmpty())
	{
		UE_LOG(LogGB, Warning, TEXT("Wall: the flame clips '%s' (A/B/C in Content/Data/Vfx) did not load; the sprite row is drawn instead"), *FlameClips);
	}

	if (!bFlames && TextureId.IsEmpty() && !GetSpriteId().IsNone())
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
