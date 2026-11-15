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

// The fire wall's flames (owner, 2026-11-15: animated pixel art like the flamethrower; then, from his reference, ONE
// curtain of fire along a line on the ground, not a row of columns). GBPixelVfx clips drawn by
// Tools/Jam/make_fire_wall.js, Strings "FlameVfx" = their common name:
//   <name>A, <name>B    the sheet: two quads along the wall line, one behind the other, each with its own shapes
//   <name>Rib           narrow pieces of the same sheet stood across the wall, so it never vanishes seen end-on
//   <name>Seam          the white-hot line, flat on the floor
//   <name>Sparks        sparks spitting from the seam (camera-facing)
// The sheets rise out of the floor when the wall appears and sink back into it in its last moments; one unshadowed
// light flickers with them. The same layout is in make_fire_wall.js (WALL), which renders the previews.
namespace GBWallFlames
{
	const FName LightName(TEXT("FireWallLight"));
	/** The sheet is drawn for a wall this wide (cm); everything scales with the wall's real width. */
	constexpr float DrawnForWidth = 400.f;
	/** The two sheets stand this share of the wall's thickness in front of and behind its middle. */
	constexpr float SheetOffsetShare = 0.18f;
	constexpr int32 RibCount = 3;
	constexpr int32 SparkCount = 8;
	constexpr float RiseSeconds = 0.4f;
	constexpr float SinkSeconds = 0.45f;
	constexpr float LightCandelas = 40.f;
	constexpr float LightRadius = 1000.f;

	float Hash01(int32 A, int32 B)
	{
		uint32 H = uint32(A) * 73856093u ^ uint32(B) * 19349663u;
		H ^= H >> 16; H *= 0x85ebca6bu; H ^= H >> 13; H *= 0xc2b2ae35u; H ^= H >> 16;
		return float(H >> 8) / 16777215.f;
	}

	const FGBPixelVfxClip* FindClip(const UObject* WorldContext, const FString& ClipName)
	{
		const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
		const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		const UGBPixelVfxSubsystem* Vfx = GameInstance ? GameInstance->GetSubsystem<UGBPixelVfxSubsystem>() : nullptr;
		return Vfx ? Vfx->FindClip(FName(*ClipName)) : nullptr;
	}

	/** How much of the fire is out of the floor: rises at the start, 1 while it burns, sinks at the end. */
	float ShownAt(float Elapsed, float Duration)
	{
		const float Rise = FMath::Clamp(Elapsed / RiseSeconds, 0.f, 1.f);
		const float Up = 1.f - FMath::Square(1.f - Rise);
		const float Left = Duration - Elapsed;
		const float Down = (Duration > 0.f && Left < SinkSeconds) ? FMath::Max(0.f, Left / SinkSeconds) : 1.f;
		return Up * Down;
	}

	/** One piece of the fire. Risers move up out of the floor; the others (seam, sparks) grow in place. */
	struct FPiece
	{
		TWeakObjectPtr<UGBPixelVfxComponent> Clip;
		FVector Location = FVector::ZeroVector;
		bool bRiser = false;
	};

	/** Builds the curtain and the light, and starts their rise. False (nothing built) when the sheet clip is missing. */
	bool Build(AActor* Wall, USceneComponent* Root, const FString& ClipBase, float Width, float Height, float Thickness, float Duration)
	{
		UWorld* World = Wall ? Wall->GetWorld() : nullptr;
		const FGBPixelVfxClip* Sheet = ClipBase.IsEmpty() ? nullptr : FindClip(Wall, ClipBase + TEXT("A"));
		if (!World || !Sheet || Sheet->WorldHeightCm <= 0.f || Sheet->Fps <= 0 || Sheet->FrameCount <= 0)
		{
			return false;
		}
		const float Scale = Width / DrawnForWidth;
		const float SheetHeight = Sheet->WorldHeightCm * Scale;
		const float ClipSeconds = float(Sheet->FrameCount) / float(Sheet->Fps);
		// The actor stands at half the wall's height: the floor is half a height below it.
		const float Floor = -Height * 0.5f;
		TArray<FPiece> Pieces;
		// Quad axes in the wall's frame (X = away from the caster, Y = along the wall, Z = up): the clip's right and up.
		auto Add = [&](const TCHAR* Suffix, const FVector& Location, const FVector& Right, const FVector& Up, float TimeOffset, bool bRiser)
		{
			const FString ClipName = ClipBase + Suffix;
			if (!FindClip(Wall, ClipName))
			{
				return;
			}
			UGBPixelVfxComponent* Clip = NewObject<UGBPixelVfxComponent>(Wall, NAME_None, RF_Transient);
			Clip->bEnableLight = false;
			Clip->SizeScale = bRiser ? Scale : Scale * 0.02f;
			Clip->StartTimeOffset = TimeOffset;
			Clip->AttachToComponent(Root, FAttachmentTransformRules::KeepRelativeTransform);
			Clip->SetRelativeLocationAndRotation(Location + (bRiser ? FVector(0.f, 0.f, -SheetHeight) : FVector::ZeroVector),
				FMatrix(Right, Up, FVector::CrossProduct(Right, Up), FVector::ZeroVector).Rotator());
			Clip->RegisterComponent();
			if (Clip->Play(FName(*ClipName)))
			{
				FPiece& Piece = Pieces.AddDefaulted_GetRef();
				Piece.Clip = Clip;
				Piece.Location = Location;
				Piece.bRiser = bRiser;
			}
			else
			{
				Clip->DestroyComponent();
			}
		};
		const FVector Along = FVector::RightVector;
		const FVector Across = FVector::ForwardVector;
		const FVector Upward = FVector::UpVector;
		const float SheetX = SheetOffsetShare * Thickness;
		Add(TEXT("A"), FVector(-SheetX, 0.f, Floor), Along, Upward, 0.f, true);
		Add(TEXT("B"), FVector(SheetX, 0.f, Floor), -Along, Upward, 0.55f, true);
		for (int32 Index = 0; Index < RibCount; ++Index)
		{
			Add(TEXT("Rib"), FVector(0.f, -Width * 0.5f + (Index + 0.5f) * Width / RibCount, Floor), Across, Upward, 0.3f + Index * 0.37f, true);
		}
		// The seam lies on the floor (its clip faces the ground and follows this yaw), a little above it.
		Add(TEXT("Seam"), FVector(0.f, 0.f, Floor + 2.f), Along, -Across, 0.2f, false);
		for (int32 Index = 0; Index < SparkCount; ++Index)
		{
			Add(TEXT("Sparks"), FVector((Index % 2 ? 1.f : -1.f) * (SheetX + 12.f), -Width * 0.5f + (Index + 0.5f) * Width / SparkCount, Floor),
				Along, Upward, Hash01(Index, 5) * ClipSeconds, false);
		}
		if (Pieces.Num() == 0)
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
		Light->SetRelativeLocation(FVector(0.f, 0.f, Floor + SheetHeight * 0.3f));
		Light->RegisterComponent();

		// The wall actor does not tick: a timer drives the rise, the sink and the light's flicker (in steps, like the
		// frames). It dies with the wall.
		const double StartTime = World->GetTimeSeconds();
		const TWeakObjectPtr<UPointLightComponent> WeakLight(Light);
		FTimerHandle Handle;
		World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(Wall, [Wall, Pieces, WeakLight, StartTime, Duration, Scale, SheetHeight]()
		{
			const UWorld* TimerWorld = Wall->GetWorld();
			if (!TimerWorld)
			{
				return;
			}
			const float Elapsed = static_cast<float>(TimerWorld->GetTimeSeconds() - StartTime);
			const float Shown = ShownAt(Elapsed, Duration);
			for (const FPiece& Piece : Pieces)
			{
				UGBPixelVfxComponent* Clip = Piece.Clip.Get();
				if (!Clip)
				{
					continue;
				}
				if (Piece.bRiser)
				{
					Clip->SetRelativeLocation(Piece.Location - FVector(0.f, 0.f, (1.f - Shown) * SheetHeight));
				}
				else
				{
					Clip->SizeScale = Scale * FMath::Max(0.02f, Shown);
				}
			}
			if (UPointLightComponent* FlameLight = WeakLight.Get())
			{
				FlameLight->SetIntensity(LightCandelas * Shown * (0.75f + 0.25f * Hash01(FMath::FloorToInt(Elapsed * 12.f), 11)));
			}
		}), 1.f / 30.f, true, 0.f);
		UE_LOG(LogGB, Display, TEXT("Wall: fire curtain '%s' with %d pieces"), *ClipBase, Pieces.Num());
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
		UE_LOG(LogGB, Warning, TEXT("Wall: the flame clips '%s' did not load (the sheet is <name>A in Content/Data/Vfx); the sprite row is drawn instead"), *FlameClips);
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
