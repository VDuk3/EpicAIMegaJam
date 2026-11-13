#include "World/GBTestArena.h"

#include "Components/BrushComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PointLight.h"
#include "Engine/Scene.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "GameFramework/PlayerStart.h"
#include "NavigationData.h"
#include "NavigationSystem.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "PhysicsEngine/BodySetup.h"
#include "AI/GBEncounter.h"
#include "AI/GBEnemyCharacter.h"
#include "Core/GBStatNames.h"
#include "Data/GBDataSubsystem.h"
#include "World/GBCheckpointVolume.h"
#include "World/GBChest.h"
#include "World/GBEraComponent.h"
#include "World/GBEraSubsystem.h"
#include "World/GBPickup.h"
#include "World/GBPortal.h"

// =====================================================================================================================
// LAYOUT (level design, not gameplay tuning). Units: cm, degrees, lux / candelas. Floor top is Z = 0.
// The player starts on the west side looking east (+X) at the central portal.
//   North (+Y): movement course, west to east: a one-jump step, another, a step that needs the double jump, then a
//               gap that needs the dash. Heights and gap come from Player.json at build time (see GBArenaCourse).
//   North-east: raised platform with a chest, reached only by a FUTURE-only ramp that appears once the PAST
//               encounter Test_PastWave is cleared (cause in the past, effect in the future).
//   South-east: era-gated room: its doorway is blocked by a PAST-only wall (open in the future). Shotgun inside.
//   East:       encounter Test_PastWave (past only).   South-west: encounter Test_FutureWave (future only).
//   Cover of both eras (low walls, pillars) stands between each encounter's spawn points and the player, on the open
//   floor, and south of the start (the wall the smoke run's pathing check uses).
// =====================================================================================================================
namespace GBArena
{
	// Shell. Walls are taller than the top of the course plus a double jump and a climb, so nobody jumps out of the arena.
	constexpr float HalfSize = 3000.f;                // inner half width of the square arena (60 m x 60 m)
	constexpr float WallHeight = 1400.f;
	constexpr float WallThickness = 100.f;
	constexpr float FloorThickness = 50.f;

	// Pixel-art tile sizes of the past look (one texture tile covers this many cm).
	constexpr float FloorTile = 200.f;
	constexpr float WallTile = 200.f;
	constexpr float WoodTile = 100.f;

	// Player start and the start area
	const FVector PlayerStart(-2500.f, 0.f, 100.f);
	constexpr float PlayerStartYaw = 0.f;
	const FVector StartCheckpointExtent(350.f, 350.f, 200.f);
	const FVector StartChest(-2700.f, -800.f, 0.f);
	constexpr float StartChestYaw = 90.f;
	const FVector Dummies[] = { FVector(-1500.f, -350.f, 100.f), FVector(-1500.f, 350.f, 100.f) };
	constexpr float DummyYaw = 180.f;
	constexpr float PickupRowX = -2150.f;
	const float PickupRowY[] = { -500.f, -350.f, -250.f, -100.f, 50.f, 200.f, 350.f };   // pistol, ammo, ammo, hp, mp, grenade, injection
	constexpr float SpellRowY = 800.f;
	const float SpellRowX[] = { -2650.f, -2500.f, -2350.f, -2200.f };                    // Fire, Lightning, Ice, Air

	// Central portal (both eras)
	const FVector Portal(0.f, 0.f, 0.f);

	// Movement course (north). Step tops = cumulative fractions of one jump's rise: two steps of OneJumpStep each
	// (one jump), then one of DoubleJumpStep (more than one jump, comfortably less than the double jump's 2.0).
	constexpr float CourseY = 2600.f;
	constexpr float StepSize = 300.f;
	constexpr int32 NumSteps = 3;
	const float StepX[NumSteps] = { -1800.f, -1400.f, -1000.f };
	constexpr float OneJumpStep = 0.6f;
	constexpr float DoubleJumpStep = 1.5f;
	// Dash gap (edge to edge, at the height of the last step) = the farthest double jump without a dash, plus this
	// share of the dash distance: impossible without the dash, with room to spare with it.
	constexpr float DashGapDashShare = 0.4f;
	constexpr float MinDashGap = 300.f;
	constexpr float MaxDashGap = 2000.f;              // keeps the landing block clear of the raised platform
	constexpr float WallClearance = 100.f;            // course top + a double jump must stay this far under the wall top

	// Raised platform with the future-only ramp (north-east)
	const FVector PlatformCenter(2300.f, 2300.f, 0.f);
	const FVector2D PlatformSize(1000.f, 1000.f);
	constexpr float PlatformTop = 600.f;
	constexpr float RampY = 2200.f;
	constexpr float RampStartX = 300.f;               // ramp foot on the floor; it rises east to the platform's west edge
	constexpr float RampWidth = 300.f;
	constexpr float RampThickness = 40.f;
	const FVector PlatformChest(2400.f, 2400.f, PlatformTop);
	constexpr float PlatformChestYaw = 180.f;

	// Era-gated room (south-east)
	constexpr float RoomWestX = 1600.f;
	constexpr float RoomNorthY = -1800.f;
	constexpr float DoorCenterX = 2200.f;
	constexpr float DoorWidth = 200.f;
	const FVector RoomShotgun(2200.f, -2500.f, 0.f);
	const FVector RoomAmmo[] = { FVector(2050.f, -2600.f, 0.f), FVector(2350.f, -2600.f, 0.f) };

	// Encounters
	const FVector PastWave(2000.f, -500.f, 0.f);
	const FVector FutureWave(-1600.f, -2200.f, 0.f);
	const FVector EncounterTrigger(700.f, 700.f, 300.f);
	const FVector EncounterSpawns[] = { FVector(450.f, 450.f, 100.f), FVector(-450.f, 450.f, 100.f), FVector(450.f, -450.f, 100.f), FVector(-450.f, -450.f, 100.f) };

	// Cover present in both eras. Boxes are (center, size) with the center at half height.
	const FVector PathTestWallCenter(-2500.f, -1150.f, 100.f);   // south of the start: the smoke run's pathing check
	const FVector PathTestWallSize(600.f, 100.f, 200.f);
	const FVector PathTestEnemySpawn(-2500.f, -1550.f, 0.f);      // behind that wall, seen from the player start
	struct FCoverBox { FVector Center; FVector Size; };
	const FCoverBox CoverWalls[] = {
		{ PathTestWallCenter, PathTestWallSize },
		{ FVector(2100.f, -500.f, 80.f), FVector(100.f, 600.f, 160.f) },     // Test_PastWave: between the east spawns and the west side
		{ FVector(-1600.f, -2350.f, 80.f), FVector(700.f, 100.f, 160.f) },   // Test_FutureWave: hides the south spawns from the north side
		{ FVector(900.f, -500.f, 80.f), FVector(100.f, 500.f, 160.f) } };    // open floor, between the portal and the east encounter
	const FVector2D CoverPillars[] = { FVector2D(1650.f, -300.f), FVector2D(1650.f, -700.f),   // Test_PastWave, west side
		FVector2D(-1900.f, -1950.f), FVector2D(-1300.f, -1950.f),                             // Test_FutureWave, north side
		FVector2D(-700.f, -1800.f) };                                                         // open floor, south
	constexpr float CoverPillarDiameter = 120.f;
	constexpr float CoverPillarHeight = 400.f;

	// Decoration anchors shared by both eras (past pillars / future columns stand in the same places)
	const FVector2D Columns[] = { FVector2D(-900.f, -1100.f), FVector2D(-900.f, 1100.f), FVector2D(900.f, -1100.f), FVector2D(900.f, 1100.f) };
	constexpr float ColumnHeight = 500.f;
	const FVector Torches[] = { FVector(-2950.f, -1500.f, 250.f), FVector(-2950.f, 1500.f, 250.f), FVector(2950.f, 600.f, 250.f), FVector(-200.f, -2950.f, 250.f) };
	const FVector Crates[] = { FVector(-1200.f, -1000.f, 50.f), FVector(-1090.f, -1000.f, 50.f), FVector(-1145.f, -1000.f, 150.f),
		FVector(1100.f, 1000.f, 50.f), FVector(1100.f, 1110.f, 50.f), FVector(-300.f, 1500.f, 50.f) };
	const FVector Containers[] = { FVector(-1150.f, -1000.f, 65.f), FVector(1100.f, 1050.f, 65.f), FVector(-300.f, 1500.f, 65.f) };
	const FVector LowWallCenter(-500.f, -1500.f, 75.f);
	const FVector LowWallSize(600.f, 120.f, 150.f);
	const FVector LowWall2Center(700.f, 1600.f, 75.f);
	const FVector LowWall2Size(120.f, 500.f, 150.f);
	constexpr float NeonLaneY = 250.f;                // two neon lines on the floor from the start to the portal
	constexpr float NeonLaneStartX = -1700.f;
	constexpr float NeonLaneEndX = -150.f;
	constexpr float WallNeonZ = 300.f;                // future: a neon line along the inner face of each perimeter wall

	// Colors (future look and plain placeholders)
	const FLinearColor Floor(0.33f, 0.33f, 0.31f);
	const FLinearColor Wall(0.28f, 0.28f, 0.3f);
	const FLinearColor Course(0.3f, 0.32f, 0.38f);
	const FLinearColor Platform(0.32f, 0.3f, 0.28f);
	const FLinearColor Cover(0.18f, 0.2f, 0.24f);
	const FLinearColor DoorStoneTint(0.85f, 0.75f, 0.65f);       // the walled-up doorway reads as newer masonry
	const FLinearColor Flame(1.f, 0.5f, 0.12f);
	const FLinearColor TorchWood(0.25f, 0.15f, 0.06f);
	const FLinearColor Metal(0.07f, 0.08f, 0.1f);
	const FLinearColor NeonCyan(0.1f, 0.9f, 1.f);
	const FLinearColor NeonMagenta(1.f, 0.15f, 0.8f);

	// Lights
	const FRotator SunRotation(-40.f, 35.f, 0.f);
	const FLinearColor PastSun(1.f, 0.82f, 0.6f);
	constexpr float PastSunLux = 7.f;
	const FLinearColor FutureSun(0.5f, 0.6f, 1.f);
	constexpr float FutureSunLux = 1.5f;
	constexpr float PastSkyIntensity = 1.f;
	constexpr float FutureSkyIntensity = 0.6f;
	const FLinearColor PastFog(0.9f, 0.7f, 0.5f);
	constexpr float PastFogDensity = 0.015f;
	const FLinearColor FutureFog(0.15f, 0.2f, 0.35f);
	constexpr float FutureFogDensity = 0.03f;
	const FLinearColor TorchLight(1.f, 0.55f, 0.2f);
	constexpr float TorchCandelas = 60.f;
	constexpr float TorchRadius = 900.f;
	constexpr float NeonCandelas = 40.f;
	constexpr float NeonRadius = 800.f;

	// Navigation bounds: the whole arena, from under the floor to above the course and the platform.
	constexpr float NavBoundsBottom = -200.f;
	constexpr float NavBoundsTop = 1200.f;
}

namespace GBArenaIds
{
	// Names of things defined in the other data files (Weapons, Items, Enemies, Encounters, Spells, Sprites).
	const FName PistolId(TEXT("Pistol"));
	const FName ShotgunId(TEXT("Shotgun"));
	const FName RifleId(TEXT("Rifle"));
	const FName StaffEmberId(TEXT("Staff_Ember"));
	const FName AmmoId(TEXT("Ammo_Magazine"));
	const FName HealthPotionId(TEXT("HealthPotion"));
	const FName ManaPotionId(TEXT("ManaPotion"));
	const FName GrenadeId(TEXT("ElectricGrenade"));
	const FName InjectionId(TEXT("HealthInjection"));
	const FName SwordHiltId(TEXT("SwordHilt"));
	const FName GemFireId(TEXT("Gem_Fire"));
	const FName GemIceId(TEXT("Gem_Ice"));
	const FName ArmorId(TEXT("Armor_Chronoweave"));
	const FName DummyId(TEXT("TrainingDummy"));
	const FName PastWaveId(TEXT("Test_PastWave"));
	const FName FutureWaveId(TEXT("Test_FutureWave"));
	const FName SpellGroups[] = { FName(TEXT("Fire")), FName(TEXT("Lightning")), FName(TEXT("Ice")), FName(TEXT("Air")) };
	const FName StoneTile(TEXT("tile_stone"));
	const FName StoneFloorTile(TEXT("tile_stone_floor"));
	const FName WoodTile(TEXT("tile_wood"));

	// Progress flags owned by this level.
	const FName GotFirstGunFlag(TEXT("GotFirstGun"));
	const FName StartChestFlag(TEXT("Arena.Chest.Start"));
	const FName PlatformChestFlag(TEXT("Arena.Chest.Platform"));

	FActorSpawnParameters AlwaysSpawnParams()
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		return Params;
	}
}

namespace GBArenaCourse
{
	// Used only when Player.json lacks the stat (a warning is logged).
	constexpr float DefaultJumpVelocity = 800.f;
	constexpr float DefaultGravityScale = 1.6f;
	constexpr float DefaultMoveSpeed = 750.f;
	constexpr float DefaultDashDistance = 700.f;
	constexpr float DefaultGravity = 980.f;

	struct FLayout
	{
		float StepTops[GBArena::NumSteps] = {};
		float DashGap = 0.f;
		float JumpRise = 0.f;
	};

	float ReadPlayerStat(const UGBDataSubsystem* Data, FName Stat, float DefaultValue)
	{
		const float* Value = Data ? Data->GetPlayer().Stats.Find(Stat) : nullptr;
		if (!Value)
		{
			UE_LOG(LogGB, Warning, TEXT("Test arena: Player.json has no '%s'; the movement course assumes %.2f."), *Stat.ToString(), DefaultValue);
			return DefaultValue;
		}
		return *Value;
	}

	/**
	 * One jump rises v^2 / 2g. The double jump (a starting spell) jumps again at the apex: twice that. The farthest
	 * double jump back down to the same height takes (2 + sqrt 2) * v / g at full run speed; the dash adds its distance.
	 */
	FLayout Compute(const UWorld* World, const UGBDataSubsystem* Data)
	{
		const float JumpVelocity = FMath::Max(ReadPlayerStat(Data, GBStat::JumpVelocity, DefaultJumpVelocity), 1.f);
		const float GravityScale = FMath::Max(ReadPlayerStat(Data, GBStat::GravityScale, DefaultGravityScale), 0.1f);
		const float MoveSpeed = FMath::Max(ReadPlayerStat(Data, GBStat::MoveSpeed, DefaultMoveSpeed), 0.f);
		const float DashDistance = FMath::Max(ReadPlayerStat(Data, GBStat::DashDistance, DefaultDashDistance), 0.f);
		const float MantleMaxReach = FMath::Max(ReadPlayerStat(Data, GBStat::MantleMaxReach, 160.f), 0.f);
		const float WorldGravity = World ? FMath::Abs(World->GetGravityZ()) : 0.f;
		const float Gravity = (WorldGravity > 1.f ? WorldGravity : DefaultGravity) * GravityScale;

		FLayout Layout;
		Layout.JumpRise = JumpVelocity * JumpVelocity / (2.f * Gravity);
		float Top = 0.f;
		for (int32 Index = 0; Index < GBArena::NumSteps; ++Index)
		{
			// The last step must need the double jump even with a climb: halfway between one jump + climb
			// and double jump + climb.
			float StepRise = Layout.JumpRise * (Index == GBArena::NumSteps - 1 ? GBArena::DoubleJumpStep : GBArena::OneJumpStep);
			if (Index == GBArena::NumSteps - 1)
			{
				StepRise += MantleMaxReach;
			}
			Top += StepRise;
			Layout.StepTops[Index] = Top;
		}
		const float AirTime = (2.f + UE_SQRT_2) * JumpVelocity / Gravity;
		Layout.DashGap = FMath::Clamp(MoveSpeed * AirTime + DashDistance * GBArena::DashGapDashShare, GBArena::MinDashGap, GBArena::MaxDashGap);

		UE_LOG(LogGB, Display, TEXT("Test arena course: one jump %.0f cm, double jump %.0f cm, step tops %.0f / %.0f / %.0f cm, dash gap %.0f cm, climb reach %.0f cm."),
			Layout.JumpRise, 2.f * Layout.JumpRise, Layout.StepTops[0], Layout.StepTops[1], Layout.StepTops[2], Layout.DashGap, MantleMaxReach);
		if (Top + 2.f * Layout.JumpRise + MantleMaxReach > GBArena::WallHeight - GBArena::WallClearance)
		{
			UE_LOG(LogGB, Warning, TEXT("Test arena: with these jump numbers the player can jump over the arena walls from the course (raise GBArena::WallHeight)."));
		}
		if (2.f * Layout.JumpRise + MantleMaxReach > GBArena::PlatformTop - 20.f)
		{
			UE_LOG(LogGB, Warning, TEXT("Test arena: the past-era raised platform (future-only ramp) can be reached by climbing."));
		}
		return Layout;
	}
}

AGBTestArena::AGBTestArena()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
}

void AGBTestArena::GetPathTestPoints(FVector& OutEnemySpawn, FVector& OutPlayerSpot)
{
	OutEnemySpawn = GBArena::PathTestEnemySpawn;
	OutPlayerSpot = GBArena::PlayerStart;
}

// ---------------------------------------------------------------------------------------------------------------------
// Helpers

namespace GBArenaSpawn
{
	/** A movable static mesh actor: a static component cannot get a mesh or a scale once the actor exists at runtime. */
	AStaticMeshActor* SpawnMeshActor(UWorld* World, const FVector& Center, const FRotator& Rotation)
	{
		AStaticMeshActor* Actor = World ? World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Center, Rotation, GBArenaIds::AlwaysSpawnParams()) : nullptr;
		if (Actor)
		{
			Actor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
		}
		return Actor;
	}

	void ApplyCollision(UStaticMeshComponent* MeshComponent, bool bCollision)
	{
		MeshComponent->SetCollisionProfileName(bCollision ? UCollisionProfile::BlockAll_ProfileName : UCollisionProfile::NoCollision_ProfileName);
		// Only solid shapes shape the navmesh; looks and glows stay out of the navigation octree.
		MeshComponent->SetCanEverAffectNavigation(bCollision);
	}
}

AStaticMeshActor* AGBTestArena::SpawnShape(EGBShape Shape, const FVector& Center, const FVector& Size, const FLinearColor& Color,
	bool bGlow, bool bCollision, const FRotator& Rotation)
{
	AStaticMeshActor* Actor = GBArenaSpawn::SpawnMeshActor(GetWorld(), Center, Rotation);
	if (!Actor)
	{
		return nullptr;
	}
	UStaticMeshComponent* MeshComponent = Actor->GetStaticMeshComponent();
	GBPlaceholder::SetupShape(MeshComponent, Shape, Size, Color, bGlow);
	GBArenaSpawn::ApplyCollision(MeshComponent, bCollision);
	MeshComponent->SetCastShadow(!bGlow);
	return Actor;
}

AStaticMeshActor* AGBTestArena::SpawnTexturedShape(EGBShape Shape, const FVector& Center, const FVector& Size, FName TextureId, float TileSize,
	bool bCollision, const FRotator& Rotation, const FLinearColor& Tint)
{
	AStaticMeshActor* Actor = GBArenaSpawn::SpawnMeshActor(GetWorld(), Center, Rotation);
	if (!Actor)
	{
		return nullptr;
	}
	UStaticMeshComponent* MeshComponent = Actor->GetStaticMeshComponent();
	GBPlaceholder::SetupTexturedShape(MeshComponent, Shape, Size, TextureId, TileSize, Tint, false);
	GBArenaSpawn::ApplyCollision(MeshComponent, bCollision);
	MeshComponent->SetCastShadow(true);
	return Actor;
}

AStaticMeshActor* AGBTestArena::SpawnStructure(EGBShape Shape, const FVector& Center, const FVector& Size, FName PastTexture, float PastTileSize,
	const FLinearColor& FutureColor, const FRotator& Rotation)
{
	AStaticMeshActor* Solid = SpawnShape(Shape, Center, Size, FutureColor, false, true, Rotation);
	if (Solid)
	{
		Solid->SetActorHiddenInGame(true);
		Solid->GetStaticMeshComponent()->SetCastShadow(false);
	}
	SetPresence(SpawnTexturedShape(Shape, Center, Size, PastTexture, PastTileSize, false, Rotation), EGBEraPresence::PastOnly);
	SetPresence(SpawnShape(Shape, Center, Size, FutureColor, false, false, Rotation), EGBEraPresence::FutureOnly);
	return Solid;
}

APointLight* AGBTestArena::SpawnPointLight(const FVector& Location, const FLinearColor& Color, float Candelas, float Radius)
{
	UWorld* World = GetWorld();
	APointLight* Light = World ? World->SpawnActor<APointLight>(APointLight::StaticClass(), Location, FRotator::ZeroRotator, GBArenaIds::AlwaysSpawnParams()) : nullptr;
	if (!Light)
	{
		return nullptr;
	}
	if (UPointLightComponent* Component = Cast<UPointLightComponent>(Light->GetLightComponent()))
	{
		Component->SetMobility(EComponentMobility::Movable);
		Component->SetIntensityUnits(ELightUnits::Candelas);
		Component->SetIntensity(Candelas);
		Component->SetLightColor(Color);
		Component->SetAttenuationRadius(Radius);
		Component->SetCastShadows(false);
	}
	return Light;
}

void AGBTestArena::SetPresence(AActor* Actor, EGBEraPresence Presence, FName RequiredFlag)
{
	if (!Actor)
	{
		return;
	}
	UGBEraComponent* Era = NewObject<UGBEraComponent>(Actor);
	Actor->AddInstanceComponent(Era);
	Era->RegisterComponent();
	Era->Configure(Presence, RequiredFlag);
}

FGBDropDefinition AGBTestArena::MakeItemDrop(FName ItemId, int32 Count)
{
	FGBDropDefinition Drop;
	Drop.ItemId = ItemId;
	Drop.Count = Count;
	return Drop;
}

FGBDropDefinition AGBTestArena::MakeWeaponDrop(FName WeaponId)
{
	FGBDropDefinition Drop;
	Drop.WeaponId = WeaponId;
	return Drop;
}

FGBDropDefinition AGBTestArena::MakeSpellDrop(FName SpellGroup)
{
	FGBDropDefinition Drop;
	Drop.SpellGroup = SpellGroup;
	return Drop;
}

// ---------------------------------------------------------------------------------------------------------------------
// Navigation

void AGBTestArena::SpawnNavigationBounds()
{
	UWorld* World = GetWorld();
	UNavigationSystemV1* NavSys = World ? UNavigationSystemV1::GetCurrent(World) : nullptr;
	if (!NavSys)
	{
		UE_LOG(LogGB, Warning, TEXT("Test arena: this world has no navigation system; enemies will use local steering only."));
		return;
	}

	const float HalfSpan = GBArena::HalfSize + GBArena::WallThickness;
	const FVector Center(0.f, 0.f, (GBArena::NavBoundsBottom + GBArena::NavBoundsTop) * 0.5f);
	NavBounds = World->SpawnActor<ANavMeshBoundsVolume>(ANavMeshBoundsVolume::StaticClass(), Center, FRotator::ZeroRotator, GBArenaIds::AlwaysSpawnParams());
	UBrushComponent* Brush = NavBounds ? NavBounds->GetBrushComponent() : nullptr;
	if (!Brush)
	{
		UE_LOG(LogGB, Warning, TEXT("Test arena: could not spawn the navigation bounds; enemies will use local steering only."));
		return;
	}

	// A volume placed in the editor gets its box from a brush model; one spawned at runtime has none, so its
	// bounds would be a point. UBrushComponent::CalcBounds falls back to the body setup's shapes: give it a box.
	UBodySetup* BodySetup = NewObject<UBodySetup>(Brush, NAME_None, RF_Transient);
	BodySetup->AggGeom.BoxElems.Add(FKBoxElem(2.f * HalfSpan, 2.f * HalfSpan, GBArena::NavBoundsTop - GBArena::NavBoundsBottom));
	Brush->BrushBodySetup = BodySetup;
	Brush->UpdateBounds();
	// The volume already registered itself (with point bounds) while spawning: send the real box.
	NavSys->OnNavigationBoundsUpdated(NavBounds);
}

void AGBTestArena::BuildNavigation()
{
	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld());
	if (!NavSys || !NavBounds)
	{
		return;
	}
	// Dynamic generation would also build it tile by tile over the next frames; doing it once now (blocking, a
	// fraction of a second) means the first wave already has paths. Era travel later only rebuilds dirty tiles.
	NavSys->Build();
	const ANavigationData* NavData = NavSys->GetDefaultNavDataInstance(FNavigationSystem::DontCreate);
	if (NavData)
	{
		UE_LOG(LogGB, Display, TEXT("Test arena: navigation mesh built (%s)."), *NavData->GetName());
	}
	else
	{
		UE_LOG(LogGB, Warning, TEXT("Test arena: no navigation data after the build; enemies will use local steering only."));
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Static part: before the player exists

void AGBTestArena::BuildStatic()
{
	UWorld* World = GetWorld();
	if (bStaticBuilt || !World)
	{
		return;
	}
	bStaticBuilt = true;

	// ---- Lighting: movable sun, real-time sky light, atmosphere, height fog ----
	Sun = World->SpawnActor<ADirectionalLight>(ADirectionalLight::StaticClass(), FVector(0.f, 0.f, 1000.f), GBArena::SunRotation, GBArenaIds::AlwaysSpawnParams());
	if (Sun)
	{
		// ADirectionalLight::GetComponent() only exists in Editor builds; GetLightComponent() exists in every build.
		if (UDirectionalLightComponent* SunComponent = Cast<UDirectionalLightComponent>(Sun->GetLightComponent()))
		{
			SunComponent->SetMobility(EComponentMobility::Movable);
			SunComponent->SetAtmosphereSunLight(true);
			SunComponent->SetCastShadows(true);
		}
	}
	SkyLight = World->SpawnActor<ASkyLight>(ASkyLight::StaticClass(), FVector(0.f, 0.f, 500.f), FRotator::ZeroRotator, GBArenaIds::AlwaysSpawnParams());
	if (SkyLight)
	{
		if (USkyLightComponent* SkyComponent = SkyLight->GetLightComponent())
		{
			SkyComponent->SetMobility(EComponentMobility::Movable);
			SkyComponent->SetRealTimeCapture(true);
		}
	}
	World->SpawnActor<ASkyAtmosphere>(ASkyAtmosphere::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, GBArenaIds::AlwaysSpawnParams());
	Fog = World->SpawnActor<AExponentialHeightFog>(AExponentialHeightFog::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, GBArenaIds::AlwaysSpawnParams());
	ApplyEraLighting(EGBEra::Past);

	SpawnNavigationBounds();

	const FName Stone = GBArenaIds::StoneTile;

	// ---- Floor and perimeter walls (stone in the past, dark 3D with neon lines in the future) ----
	const float Outer = GBArena::HalfSize + GBArena::WallThickness * 0.5f;
	const float FullSpan = GBArena::HalfSize * 2.f + GBArena::WallThickness * 2.f;
	SpawnStructure(EGBShape::Cube, FVector(0.f, 0.f, -GBArena::FloorThickness * 0.5f), FVector(FullSpan, FullSpan, GBArena::FloorThickness),
		GBArenaIds::StoneFloorTile, GBArena::FloorTile, GBArena::Floor);
	SpawnStructure(EGBShape::Cube, FVector(0.f, Outer, GBArena::WallHeight * 0.5f), FVector(FullSpan, GBArena::WallThickness, GBArena::WallHeight), Stone, GBArena::WallTile, GBArena::Wall);
	SpawnStructure(EGBShape::Cube, FVector(0.f, -Outer, GBArena::WallHeight * 0.5f), FVector(FullSpan, GBArena::WallThickness, GBArena::WallHeight), Stone, GBArena::WallTile, GBArena::Wall);
	SpawnStructure(EGBShape::Cube, FVector(Outer, 0.f, GBArena::WallHeight * 0.5f), FVector(GBArena::WallThickness, FullSpan, GBArena::WallHeight), Stone, GBArena::WallTile, GBArena::Wall);
	SpawnStructure(EGBShape::Cube, FVector(-Outer, 0.f, GBArena::WallHeight * 0.5f), FVector(GBArena::WallThickness, FullSpan, GBArena::WallHeight), Stone, GBArena::WallTile, GBArena::Wall);
	{
		const float Face = GBArena::HalfSize - 3.f;
		const float Span = GBArena::HalfSize * 2.f;
		SetPresence(SpawnGlow(FVector(0.f, Face, GBArena::WallNeonZ), FVector(Span, 4.f, 10.f), GBArena::NeonCyan), EGBEraPresence::FutureOnly);
		SetPresence(SpawnGlow(FVector(0.f, -Face, GBArena::WallNeonZ), FVector(Span, 4.f, 10.f), GBArena::NeonCyan), EGBEraPresence::FutureOnly);
		SetPresence(SpawnGlow(FVector(Face, 0.f, GBArena::WallNeonZ), FVector(4.f, Span, 10.f), GBArena::NeonMagenta), EGBEraPresence::FutureOnly);
		SetPresence(SpawnGlow(FVector(-Face, 0.f, GBArena::WallNeonZ), FVector(4.f, Span, 10.f), GBArena::NeonMagenta), EGBEraPresence::FutureOnly);
	}

	// ---- Movement course: one-jump steps, a double-jump step, then a dash gap to the landing block ----
	const GBArenaCourse::FLayout Course = GBArenaCourse::Compute(World, UGBDataSubsystem::Get(this));
	for (int32 Index = 0; Index < GBArena::NumSteps; ++Index)
	{
		const float Top = Course.StepTops[Index];
		SpawnStructure(EGBShape::Cube, FVector(GBArena::StepX[Index], GBArena::CourseY, Top * 0.5f), FVector(GBArena::StepSize, GBArena::StepSize, Top),
			Stone, GBArena::WallTile, GBArena::Course);
	}
	const float LastTop = Course.StepTops[GBArena::NumSteps - 1];
	const float LandingX = GBArena::StepX[GBArena::NumSteps - 1] + GBArena::StepSize + Course.DashGap;
	SpawnStructure(EGBShape::Cube, FVector(LandingX, GBArena::CourseY, LastTop * 0.5f), FVector(GBArena::StepSize, GBArena::StepSize, LastTop),
		Stone, GBArena::WallTile, GBArena::Course);
	CourseLandingTop = FVector(LandingX, GBArena::CourseY, LastTop);

	// ---- Raised platform (the ramp is gameplay: it depends on era and a flag) ----
	SpawnStructure(EGBShape::Cube, FVector(GBArena::PlatformCenter.X, GBArena::PlatformCenter.Y, GBArena::PlatformTop * 0.5f),
		FVector(GBArena::PlatformSize.X, GBArena::PlatformSize.Y, GBArena::PlatformTop), Stone, GBArena::WallTile, GBArena::Platform);

	// ---- Era-gated room walls (the doorway blocker is gameplay) ----
	const float RoomSouthY = -GBArena::HalfSize;
	const float RoomEastX = GBArena::HalfSize;
	const float RoomDepth = GBArena::RoomNorthY - RoomSouthY + GBArena::WallThickness * 0.5f;
	SpawnStructure(EGBShape::Cube, FVector(GBArena::RoomWestX, RoomSouthY + RoomDepth * 0.5f, GBArena::WallHeight * 0.5f),
		FVector(GBArena::WallThickness, RoomDepth, GBArena::WallHeight), Stone, GBArena::WallTile, GBArena::Wall);
	const float DoorWestX = GBArena::DoorCenterX - GBArena::DoorWidth * 0.5f;
	const float DoorEastX = GBArena::DoorCenterX + GBArena::DoorWidth * 0.5f;
	const float WestSegment = DoorWestX - GBArena::RoomWestX;
	const float EastSegment = RoomEastX - DoorEastX;
	SpawnStructure(EGBShape::Cube, FVector(GBArena::RoomWestX + WestSegment * 0.5f, GBArena::RoomNorthY, GBArena::WallHeight * 0.5f),
		FVector(WestSegment, GBArena::WallThickness, GBArena::WallHeight), Stone, GBArena::WallTile, GBArena::Wall);
	SpawnStructure(EGBShape::Cube, FVector(DoorEastX + EastSegment * 0.5f, GBArena::RoomNorthY, GBArena::WallHeight * 0.5f),
		FVector(EastSegment, GBArena::WallThickness, GBArena::WallHeight), Stone, GBArena::WallTile, GBArena::Wall);

	// ---- Player start: on the floor's collision body, which exists in every era (the game starts in the past) ----
	PlayerStartTransform = FTransform(FRotator(0.f, GBArena::PlayerStartYaw, 0.f), GBArena::PlayerStart);
	PlayerStartActor = World->SpawnActor<APlayerStart>(APlayerStart::StaticClass(), GBArena::PlayerStart, FRotator(0.f, GBArena::PlayerStartYaw, 0.f), GBArenaIds::AlwaysSpawnParams());
}

void AGBTestArena::ApplyEraLighting(EGBEra Era)
{
	const bool bPast = Era == EGBEra::Past;
	if (Sun)
	{
		// ADirectionalLight::GetComponent() only exists in Editor builds; GetLightComponent() exists in every build.
		if (UDirectionalLightComponent* SunComponent = Cast<UDirectionalLightComponent>(Sun->GetLightComponent()))
		{
			SunComponent->SetLightColor(bPast ? GBArena::PastSun : GBArena::FutureSun);
			SunComponent->SetIntensity(bPast ? GBArena::PastSunLux : GBArena::FutureSunLux);
		}
	}
	if (SkyLight)
	{
		if (USkyLightComponent* SkyComponent = SkyLight->GetLightComponent())
		{
			SkyComponent->SetIntensity(bPast ? GBArena::PastSkyIntensity : GBArena::FutureSkyIntensity);
		}
	}
	if (Fog)
	{
		if (UExponentialHeightFogComponent* FogComponent = Fog->GetComponent())
		{
			FogComponent->SetFogInscatteringColor(bPast ? GBArena::PastFog : GBArena::FutureFog);
			FogComponent->SetFogDensity(bPast ? GBArena::PastFogDensity : GBArena::FutureFogDensity);
		}
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Gameplay part

void AGBTestArena::BeginPlay()
{
	Super::BeginPlay();

	BuildStatic();

	if (UGBEraSubsystem* Era = UGBEraSubsystem::Get(this))
	{
		EraChangedHandle = Era->OnEraChanged.AddUObject(this, &AGBTestArena::ApplyEraLighting);
		ApplyEraLighting(Era->GetCurrentEra());
	}

	BuildGameplay();
	BuildNavigation();
}

void AGBTestArena::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UGBEraSubsystem* Era = UGBEraSubsystem::Get(this))
	{
		Era->OnEraChanged.Remove(EraChangedHandle);
	}
	Super::EndPlay(EndPlayReason);
}

void AGBTestArena::BuildGameplay()
{
	UWorld* World = GetWorld();
	if (bGameplayBuilt || !World)
	{
		return;
	}
	bGameplayBuilt = true;

	BuildPastDecor();
	BuildFutureDecor();
	BuildCover();

	// ---- Start area: checkpoint, dummies, pickups, spell knowledge, chest ----
	if (AGBCheckpointVolume* Checkpoint = World->SpawnActor<AGBCheckpointVolume>(AGBCheckpointVolume::StaticClass(),
		GBArena::PlayerStart + FVector(0.f, 0.f, GBArena::StartCheckpointExtent.Z - GBArena::PlayerStart.Z), FRotator::ZeroRotator, GBArenaIds::AlwaysSpawnParams()))
	{
		Checkpoint->SetExtent(GBArena::StartCheckpointExtent);
	}

	for (const FVector& DummyLocation : GBArena::Dummies)
	{
		if (!AGBEnemyCharacter::SpawnEnemy(World, GBArenaIds::DummyId, DummyLocation, FRotator(0.f, GBArena::DummyYaw, 0.f), EGBEraPresence::Both))
		{
			UE_LOG(LogGB, Warning, TEXT("Test arena: could not spawn enemy %s."), *GBArenaIds::DummyId.ToString());
		}
	}

	const FGBDropDefinition StartPickups[] = {
		MakeWeaponDrop(GBArenaIds::PistolId), MakeItemDrop(GBArenaIds::AmmoId), MakeItemDrop(GBArenaIds::AmmoId), MakeItemDrop(GBArenaIds::HealthPotionId),
		MakeItemDrop(GBArenaIds::ManaPotionId), MakeItemDrop(GBArenaIds::GrenadeId), MakeItemDrop(GBArenaIds::InjectionId) };
	static_assert(UE_ARRAY_COUNT(StartPickups) == UE_ARRAY_COUNT(GBArena::PickupRowY), "One row position per start pickup.");
	for (int32 Index = 0; Index < static_cast<int32>(UE_ARRAY_COUNT(StartPickups)); ++Index)
	{
		const FName Flag = StartPickups[Index].WeaponId == GBArenaIds::PistolId ? GBArenaIds::GotFirstGunFlag : NAME_None;
		AGBPickup::SpawnPickup(World, StartPickups[Index], FVector(GBArena::PickupRowX, GBArena::PickupRowY[Index], 0.f), Flag);
	}

	static_assert(UE_ARRAY_COUNT(GBArenaIds::SpellGroups) == UE_ARRAY_COUNT(GBArena::SpellRowX), "One row position per spell group.");
	for (int32 Index = 0; Index < static_cast<int32>(UE_ARRAY_COUNT(GBArenaIds::SpellGroups)); ++Index)
	{
		AGBPickup::SpawnPickup(World, MakeSpellDrop(GBArenaIds::SpellGroups[Index]), FVector(GBArena::SpellRowX[Index], GBArena::SpellRowY, 0.f));
	}

	if (AGBChest* Chest = World->SpawnActor<AGBChest>(AGBChest::StaticClass(), GBArena::StartChest, FRotator(0.f, GBArena::StartChestYaw, 0.f), GBArenaIds::AlwaysSpawnParams()))
	{
		const FGBDropDefinition Contents[] = { MakeWeaponDrop(GBArenaIds::StaffEmberId), MakeItemDrop(GBArenaIds::SwordHiltId), MakeItemDrop(GBArenaIds::GemFireId), MakeItemDrop(GBArenaIds::GemIceId) };
		Chest->Configure(Contents, GBArenaIds::StartChestFlag);
	}

	// ---- Central portal, present in both eras; the player stays in place and changes time ----
	World->SpawnActor<AGBPortal>(AGBPortal::StaticClass(), GBArena::Portal, FRotator::ZeroRotator, GBArenaIds::AlwaysSpawnParams());

	// ---- Movement course reward on the landing block ----
	AGBPickup::SpawnPickup(World, MakeItemDrop(GBArenaIds::AmmoId), CourseLandingTop + FVector(0.f, 0.f, 10.f));

	// ---- Encounters ----
	AGBEncounter* PastEncounter = World->SpawnActor<AGBEncounter>(AGBEncounter::StaticClass(), GBArena::PastWave, FRotator::ZeroRotator, GBArenaIds::AlwaysSpawnParams());
	if (PastEncounter)
	{
		PastEncounter->Configure(GBArenaIds::PastWaveId, EGBEraPresence::PastOnly, GBArena::EncounterTrigger, GBArena::EncounterSpawns);
	}
	if (AGBEncounter* FutureEncounter = World->SpawnActor<AGBEncounter>(AGBEncounter::StaticClass(), GBArena::FutureWave, FRotator::ZeroRotator, GBArenaIds::AlwaysSpawnParams()))
	{
		FutureEncounter->Configure(GBArenaIds::FutureWaveId, EGBEraPresence::FutureOnly, GBArena::EncounterTrigger, GBArena::EncounterSpawns);
	}

	// ---- Past-to-future unlock: a future-only ramp to the raised platform, once the past wave is cleared ----
	FName PastClearedFlag = PastEncounter ? PastEncounter->GetClearedFlag() : NAME_None;
	if (PastClearedFlag.IsNone())
	{
		PastClearedFlag = FName(*FString::Printf(TEXT("Encounter.%s.Cleared"), *GBArenaIds::PastWaveId.ToString()));
	}
	{
		const float Run = GBArena::PlatformCenter.X - GBArena::PlatformSize.X * 0.5f - GBArena::RampStartX;
		const float Rise = GBArena::PlatformTop;
		const float Length = FMath::Sqrt(Run * Run + Rise * Rise) + GBArena::RampThickness;
		const float PitchRad = FMath::Atan2(Rise, Run);
		const FRotator RampRotation(FMath::RadiansToDegrees(PitchRad), 0.f, 0.f);
		// The top surface goes from the foot (RampStartX, 0) to the platform edge; the box center sits half a thickness below it.
		const FVector Up = RampRotation.RotateVector(FVector::UpVector);
		const FVector SurfaceMid(GBArena::RampStartX + Run * 0.5f, GBArena::RampY, Rise * 0.5f);
		const FVector RampCenter = SurfaceMid - Up * (GBArena::RampThickness * 0.5f);
		AStaticMeshActor* Ramp = SpawnShape(EGBShape::Cube, RampCenter, FVector(Length, GBArena::RampWidth, GBArena::RampThickness), GBArena::Metal, false, true, RampRotation);
		SetPresence(Ramp, EGBEraPresence::FutureOnly, PastClearedFlag);
		const FVector Side = RampRotation.RotateVector(FVector::RightVector) * (GBArena::RampWidth * 0.5f - 6.f);
		for (const float Sign : { -1.f, 1.f })
		{
			AStaticMeshActor* Strip = SpawnGlow(SurfaceMid + Up * 2.f + Side * Sign, FVector(Length, 8.f, 4.f), GBArena::NeonCyan, RampRotation);
			SetPresence(Strip, EGBEraPresence::FutureOnly, PastClearedFlag);
		}
	}
	if (AGBChest* PlatformChestActor = World->SpawnActor<AGBChest>(AGBChest::StaticClass(), GBArena::PlatformChest, FRotator(0.f, GBArena::PlatformChestYaw, 0.f), GBArenaIds::AlwaysSpawnParams()))
	{
		const FGBDropDefinition Contents[] = { MakeWeaponDrop(GBArenaIds::RifleId), MakeItemDrop(GBArenaIds::ArmorId) };
		PlatformChestActor->Configure(Contents, GBArenaIds::PlatformChestFlag);
	}

	// ---- Era-gated room: the doorway is walled up in the past (medieval masonry), open in the future ----
	AStaticMeshActor* DoorBlock = SpawnTexturedShape(EGBShape::Cube, FVector(GBArena::DoorCenterX, GBArena::RoomNorthY, GBArena::WallHeight * 0.5f),
		FVector(GBArena::DoorWidth, GBArena::WallThickness, GBArena::WallHeight), GBArenaIds::StoneTile, GBArena::WallTile, true, FRotator::ZeroRotator, GBArena::DoorStoneTint);
	SetPresence(DoorBlock, EGBEraPresence::PastOnly);
	AGBPickup::SpawnPickup(World, MakeWeaponDrop(GBArenaIds::ShotgunId), GBArena::RoomShotgun);
	for (const FVector& AmmoLocation : GBArena::RoomAmmo)
	{
		AGBPickup::SpawnPickup(World, MakeItemDrop(GBArenaIds::AmmoId), AmmoLocation);
	}
}

void AGBTestArena::BuildCover()
{
	for (const GBArena::FCoverBox& Box : GBArena::CoverWalls)
	{
		SpawnStructure(EGBShape::Cube, Box.Center, Box.Size, GBArenaIds::StoneTile, GBArena::WallTile, GBArena::Cover);
	}
	const FVector PillarSize(GBArena::CoverPillarDiameter, GBArena::CoverPillarDiameter, GBArena::CoverPillarHeight);
	for (const FVector2D& Pillar : GBArena::CoverPillars)
	{
		SpawnStructure(EGBShape::Cylinder, FVector(Pillar.X, Pillar.Y, GBArena::CoverPillarHeight * 0.5f), PillarSize, GBArenaIds::StoneTile, GBArena::WallTile, GBArena::Cover);
		// Future: a neon ring near the top tells the metal pillars apart from the dark walls.
		SetPresence(SpawnShape(EGBShape::Cylinder, FVector(Pillar.X, Pillar.Y, GBArena::CoverPillarHeight - 40.f),
			FVector(GBArena::CoverPillarDiameter + 6.f, GBArena::CoverPillarDiameter + 6.f, 8.f), GBArena::NeonCyan, true, false), EGBEraPresence::FutureOnly);
	}
}

void AGBTestArena::BuildPastDecor()
{
	const EGBEraPresence Past = EGBEraPresence::PastOnly;

	// Stone pillars with a brazier on top.
	for (const FVector2D& Column : GBArena::Columns)
	{
		SetPresence(SpawnTexturedShape(EGBShape::Cylinder, FVector(Column.X, Column.Y, GBArena::ColumnHeight * 0.5f), FVector(100.f, 100.f, GBArena::ColumnHeight),
			GBArenaIds::StoneTile, GBArena::WallTile, true), Past);
		SetPresence(SpawnShape(EGBShape::Sphere, FVector(Column.X, Column.Y, GBArena::ColumnHeight + 25.f), FVector(45.f, 45.f, 45.f), GBArena::Flame, true, false), Past);
		SetPresence(SpawnPointLight(FVector(Column.X, Column.Y, GBArena::ColumnHeight + 100.f), GBArena::TorchLight, GBArena::TorchCandelas, GBArena::TorchRadius), Past);
	}

	// Wall torches.
	for (const FVector& Torch : GBArena::Torches)
	{
		SetPresence(SpawnShape(EGBShape::Cylinder, Torch, FVector(14.f, 14.f, 70.f), GBArena::TorchWood, false, false), Past);
		SetPresence(SpawnGlow(Torch + FVector(0.f, 0.f, 45.f), FVector(18.f, 18.f, 26.f), GBArena::Flame), Past);
		SetPresence(SpawnPointLight(Torch + FVector(0.f, 0.f, 80.f), GBArena::TorchLight, GBArena::TorchCandelas, GBArena::TorchRadius), Past);
	}

	// Wooden crates and low stone walls.
	for (const FVector& CrateLocation : GBArena::Crates)
	{
		SetPresence(SpawnTexturedShape(EGBShape::Cube, CrateLocation, FVector(100.f, 100.f, 100.f), GBArenaIds::WoodTile, GBArena::WoodTile, true), Past);
	}
	SetPresence(SpawnTexturedShape(EGBShape::Cube, GBArena::LowWallCenter, GBArena::LowWallSize, GBArenaIds::StoneTile, GBArena::WallTile, true), Past);
	SetPresence(SpawnTexturedShape(EGBShape::Cube, GBArena::LowWall2Center, GBArena::LowWall2Size, GBArenaIds::StoneTile, GBArena::WallTile, true), Past);
}

void AGBTestArena::BuildFutureDecor()
{
	const EGBEraPresence Future = EGBEraPresence::FutureOnly;

	// Dark metal columns with neon strips where the past pillars stand.
	for (const FVector2D& Column : GBArena::Columns)
	{
		SetPresence(SpawnBlock(FVector(Column.X, Column.Y, GBArena::ColumnHeight * 0.5f), FVector(120.f, 120.f, GBArena::ColumnHeight), GBArena::Metal), Future);
		for (const float Sign : { -1.f, 1.f })
		{
			SetPresence(SpawnGlow(FVector(Column.X + Sign * 62.f, Column.Y, GBArena::ColumnHeight * 0.5f), FVector(6.f, 20.f, GBArena::ColumnHeight - 40.f), GBArena::NeonCyan), Future);
		}
		SetPresence(SpawnPointLight(FVector(Column.X, Column.Y, GBArena::ColumnHeight - 50.f), GBArena::NeonCyan, GBArena::NeonCandelas, GBArena::NeonRadius), Future);
	}

	// Containers where the crates were, with a magenta neon line on top.
	for (const FVector& Container : GBArena::Containers)
	{
		SetPresence(SpawnBlock(Container, FVector(240.f, 120.f, 130.f), GBArena::Metal), Future);
		SetPresence(SpawnGlow(Container + FVector(0.f, 0.f, 67.f), FVector(240.f, 10.f, 6.f), GBArena::NeonMagenta), Future);
		SetPresence(SpawnPointLight(Container + FVector(0.f, 0.f, 150.f), GBArena::NeonMagenta, GBArena::NeonCandelas, GBArena::NeonRadius), Future);
	}

	// Neon lanes on the floor from the start to the portal.
	const float LaneLength = GBArena::NeonLaneEndX - GBArena::NeonLaneStartX;
	for (const float Sign : { -1.f, 1.f })
	{
		SetPresence(SpawnGlow(FVector(GBArena::NeonLaneStartX + LaneLength * 0.5f, Sign * GBArena::NeonLaneY, 1.5f), FVector(LaneLength, 12.f, 3.f), GBArena::NeonCyan), Future);
	}
}
