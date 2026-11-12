#include "World/GBTestArena.h"

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
#include "AI/GBEncounter.h"
#include "AI/GBEnemyCharacter.h"
#include "World/GBCheckpointVolume.h"
#include "World/GBChest.h"
#include "World/GBEraComponent.h"
#include "World/GBEraSubsystem.h"
#include "World/GBPickup.h"
#include "World/GBPortal.h"

// =====================================================================================================================
// LAYOUT (level design, not gameplay tuning). Units: cm, degrees, lux / candelas. Floor top is Z = 0.
// The player starts on the west side looking east (+X) at the central portal.
//   North (+Y): movement course, west to east: steps that need the double jump, then a gap that needs the dash.
//   North-east: raised platform with a chest, reached only by a FUTURE-only ramp that appears once the PAST
//               encounter Test_PastWave is cleared (cause in the past, effect in the future).
//   South-east: era-gated room: its doorway is blocked by a PAST-only wall (open in the future). Shotgun inside.
//   East:       encounter Test_PastWave (past only).   South-west: encounter Test_FutureWave (future only).
// Assumed player numbers (Player.json may change them): single jump ~100 cm high, double jump ~250 cm, so steps of
// 100 are single jumps, steps of 200 need the double jump, and the 600 cm platform needs the ramp.
// =====================================================================================================================
namespace GBArena
{
	// Shell
	constexpr float HalfSize = 3000.f;                // inner half width of the square arena (60 m x 60 m)
	constexpr float WallHeight = 600.f;
	constexpr float WallThickness = 100.f;
	constexpr float FloorThickness = 50.f;

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

	// Movement course (north)
	constexpr float CourseY = 2600.f;
	constexpr float StepSize = 300.f;
	const FVector2D Steps[] = { FVector2D(-1800.f, 80.f), FVector2D(-1400.f, 180.f), FVector2D(-1000.f, 380.f), FVector2D(-600.f, 560.f) };  // (x, top)
	constexpr float DashGap = 900.f;                  // edge to edge, at the height of the last step
	// The landing block sits DashGap after the last step.

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

	// Colors
	const FLinearColor Floor(0.33f, 0.33f, 0.31f);
	const FLinearColor Wall(0.28f, 0.28f, 0.3f);
	const FLinearColor Course(0.3f, 0.32f, 0.38f);
	const FLinearColor Platform(0.32f, 0.3f, 0.28f);
	const FLinearColor Stone(0.55f, 0.45f, 0.32f);
	const FLinearColor Crate(0.45f, 0.27f, 0.11f);
	const FLinearColor TorchWood(0.25f, 0.15f, 0.06f);
	const FLinearColor Flame(1.f, 0.5f, 0.12f);
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
}

namespace GBArenaIds
{
	// Names of things defined in the other data files (Weapons, Items, Enemies, Encounters, Spells).
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

AGBTestArena::AGBTestArena()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
}

// ---------------------------------------------------------------------------------------------------------------------
// Helpers

AStaticMeshActor* AGBTestArena::SpawnShape(EGBShape Shape, const FVector& Center, const FVector& Size, const FLinearColor& Color,
	bool bGlow, bool bCollision, const FRotator& Rotation)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Center, Rotation, GBArenaIds::AlwaysSpawnParams());
	if (!Actor)
	{
		return nullptr;
	}
	UStaticMeshComponent* MeshComponent = Actor->GetStaticMeshComponent();
	// Movable: a static component cannot get a mesh or a scale once the actor exists at runtime.
	MeshComponent->SetMobility(EComponentMobility::Movable);
	GBPlaceholder::SetupShape(MeshComponent, Shape, Size, Color, bGlow);
	MeshComponent->SetCollisionProfileName(bCollision ? UCollisionProfile::BlockAll_ProfileName : UCollisionProfile::NoCollision_ProfileName);
	MeshComponent->SetCastShadow(!bGlow);
	return Actor;
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

	// ---- Floor and perimeter walls ----
	const float Outer = GBArena::HalfSize + GBArena::WallThickness * 0.5f;
	const float FullSpan = GBArena::HalfSize * 2.f + GBArena::WallThickness * 2.f;
	SpawnBlock(FVector(0.f, 0.f, -GBArena::FloorThickness * 0.5f), FVector(FullSpan, FullSpan, GBArena::FloorThickness), GBArena::Floor);
	SpawnBlock(FVector(0.f, Outer, GBArena::WallHeight * 0.5f), FVector(FullSpan, GBArena::WallThickness, GBArena::WallHeight), GBArena::Wall);
	SpawnBlock(FVector(0.f, -Outer, GBArena::WallHeight * 0.5f), FVector(FullSpan, GBArena::WallThickness, GBArena::WallHeight), GBArena::Wall);
	SpawnBlock(FVector(Outer, 0.f, GBArena::WallHeight * 0.5f), FVector(GBArena::WallThickness, FullSpan, GBArena::WallHeight), GBArena::Wall);
	SpawnBlock(FVector(-Outer, 0.f, GBArena::WallHeight * 0.5f), FVector(GBArena::WallThickness, FullSpan, GBArena::WallHeight), GBArena::Wall);

	// ---- Movement course: double-jump steps, then a dash gap to the landing block ----
	for (const FVector2D& Step : GBArena::Steps)
	{
		SpawnBlock(FVector(Step.X, GBArena::CourseY, Step.Y * 0.5f), FVector(GBArena::StepSize, GBArena::StepSize, Step.Y), GBArena::Course);
	}
	const FVector2D& LastStep = GBArena::Steps[UE_ARRAY_COUNT(GBArena::Steps) - 1];
	const float LandingX = LastStep.X + GBArena::StepSize + GBArena::DashGap;
	SpawnBlock(FVector(LandingX, GBArena::CourseY, LastStep.Y * 0.5f), FVector(GBArena::StepSize, GBArena::StepSize, LastStep.Y), GBArena::Course);

	// ---- Raised platform (the ramp is gameplay: it depends on era and a flag) ----
	SpawnBlock(FVector(GBArena::PlatformCenter.X, GBArena::PlatformCenter.Y, GBArena::PlatformTop * 0.5f), FVector(GBArena::PlatformSize.X, GBArena::PlatformSize.Y, GBArena::PlatformTop), GBArena::Platform);

	// ---- Era-gated room walls (the doorway blocker is gameplay) ----
	const float RoomSouthY = -GBArena::HalfSize;
	const float RoomEastX = GBArena::HalfSize;
	const float RoomDepth = GBArena::RoomNorthY - RoomSouthY + GBArena::WallThickness * 0.5f;
	SpawnBlock(FVector(GBArena::RoomWestX, RoomSouthY + RoomDepth * 0.5f, GBArena::WallHeight * 0.5f), FVector(GBArena::WallThickness, RoomDepth, GBArena::WallHeight), GBArena::Wall);
	const float DoorWestX = GBArena::DoorCenterX - GBArena::DoorWidth * 0.5f;
	const float DoorEastX = GBArena::DoorCenterX + GBArena::DoorWidth * 0.5f;
	const float WestSegment = DoorWestX - GBArena::RoomWestX;
	const float EastSegment = RoomEastX - DoorEastX;
	SpawnBlock(FVector(GBArena::RoomWestX + WestSegment * 0.5f, GBArena::RoomNorthY, GBArena::WallHeight * 0.5f), FVector(WestSegment, GBArena::WallThickness, GBArena::WallHeight), GBArena::Wall);
	SpawnBlock(FVector(DoorEastX + EastSegment * 0.5f, GBArena::RoomNorthY, GBArena::WallHeight * 0.5f), FVector(EastSegment, GBArena::WallThickness, GBArena::WallHeight), GBArena::Wall);

	// ---- Player start ----
	PlayerStartTransform = FTransform(FRotator(0.f, GBArena::PlayerStartYaw, 0.f), GBArena::PlayerStart);
	World->SpawnActor<APlayerStart>(APlayerStart::StaticClass(), GBArena::PlayerStart, FRotator(0.f, GBArena::PlayerStartYaw, 0.f), GBArenaIds::AlwaysSpawnParams());
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
	{
		const FVector2D& LastStep = GBArena::Steps[UE_ARRAY_COUNT(GBArena::Steps) - 1];
		const float LandingX = LastStep.X + GBArena::StepSize + GBArena::DashGap;
		AGBPickup::SpawnPickup(World, MakeItemDrop(GBArenaIds::AmmoId), FVector(LandingX, GBArena::CourseY, LastStep.Y + 10.f));
	}

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

	// ---- Era-gated room: the doorway is walled up in the past, open in the future ----
	AStaticMeshActor* DoorBlock = SpawnBlock(FVector(GBArena::DoorCenterX, GBArena::RoomNorthY, GBArena::WallHeight * 0.5f), FVector(GBArena::DoorWidth, GBArena::WallThickness, GBArena::WallHeight), GBArena::Stone);
	SetPresence(DoorBlock, EGBEraPresence::PastOnly);
	AGBPickup::SpawnPickup(World, MakeWeaponDrop(GBArenaIds::ShotgunId), GBArena::RoomShotgun);
	for (const FVector& AmmoLocation : GBArena::RoomAmmo)
	{
		AGBPickup::SpawnPickup(World, MakeItemDrop(GBArenaIds::AmmoId), AmmoLocation);
	}
}

void AGBTestArena::BuildPastDecor()
{
	const EGBEraPresence Past = EGBEraPresence::PastOnly;

	// Stone pillars with a brazier on top.
	for (const FVector2D& Column : GBArena::Columns)
	{
		SetPresence(SpawnShape(EGBShape::Cylinder, FVector(Column.X, Column.Y, GBArena::ColumnHeight * 0.5f), FVector(100.f, 100.f, GBArena::ColumnHeight), GBArena::Stone), Past);
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

	// Crates and low stone walls.
	for (const FVector& CrateLocation : GBArena::Crates)
	{
		SetPresence(SpawnBlock(CrateLocation, FVector(100.f, 100.f, 100.f), GBArena::Crate), Past);
	}
	SetPresence(SpawnBlock(GBArena::LowWallCenter, GBArena::LowWallSize, GBArena::Stone), Past);
	SetPresence(SpawnBlock(GBArena::LowWall2Center, GBArena::LowWall2Size, GBArena::Stone), Past);
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
