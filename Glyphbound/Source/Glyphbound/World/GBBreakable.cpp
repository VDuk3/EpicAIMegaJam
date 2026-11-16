#include "World/GBBreakable.h"

#include "Combat/GBExplosionEffect.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/GBStatNames.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Stats/GBAttributeComponent.h"
#include "Stats/GBStatsComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "World/GBProgressSubsystem.h"
namespace GBAudio { bool Play(const UObject* Ctx, FName Key, const FVector* Loc = nullptr, float PitchMul = 1.f, float VolMul = 1.f); void SpawnLoop(const UObject* Ctx, FName Key, const FVector& Loc); bool IsFutureMap(const UObject* Ctx); void MenuPing(const UObject* Ctx); void UpdateMusic(UWorld* World); }

namespace
{
	// It never dies of damage: only the count of hits breaks it. Its health is topped up after every hit.
	constexpr float BreakableHealth = 1000000.f;
	// Broken, it is hidden at once and removed a moment later: never inside the damage call that broke it.
	constexpr float BreakRemoveSeconds = 0.05f;
	// A flat mesh (a grate, a sheet) still gets a box this thick, so shots and bodies meet it from both sides.
	constexpr float BreakableMinHalfThicknessCm = 12.f;
	// The burst when it gives way: visual only.
	constexpr float BreakBurstSeconds = 0.6f;
	const FName CrackStageParam(TEXT("Stage"));
	// Pieces: how far below its start a piece looks for a floor, how high it lies over it, how fast it tumbles.
	constexpr float ShardFloorProbeCm = 1500.f;
	constexpr float ShardRestLiftCm = 2.f;
	constexpr float ShardSpinDegrees = 420.f;
}

AGBBreakable::AGBBreakable()
{
	PrimaryActorTick.bCanEverTick = true;          // only while pieces are in the air
	PrimaryActorTick.bStartWithTickEnabled = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Root);
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetGenerateOverlapEvents(false);
	Mesh->SetCanEverAffectNavigation(false);

	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	Box->SetupAttachment(Root);
	Box->SetMobility(EComponentMobility::Movable);
	Box->SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);
	// Projectiles only overlap WorldDynamic: without this they pass through untouched (same as the ice wall).
	Box->SetGenerateOverlapEvents(true);
	Box->SetCanEverAffectNavigation(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		Mesh->SetStaticMesh(CubeMesh.Object);
	}
}

float AGBBreakable::ComputeCrackStage(int32 InHits, int32 InHitsToBreak)
{
	if (InHitsToBreak <= 0)
	{
		return 1.f;
	}
	return FMath::Clamp(static_cast<float>(InHits) / static_cast<float>(InHitsToBreak), 0.f, 1.f);
}

bool AGBBreakable::CountsAsHit(float Damage, float MinDamage, float SecondsSinceLastHit, float Cooldown)
{
	return Damage >= MinDamage && SecondsSinceLastHit >= Cooldown;
}

void AGBBreakable::BeginPlay()
{
	Super::BeginPlay();

	if (!BrokenFlag.IsNone())
	{
		const UGBProgressSubsystem* Progress = UGBProgressSubsystem::Get(this);
		if (Progress && Progress->HasFlag(BrokenFlag))
		{
			Destroy();
			return;
		}
	}

	// The box follows the mesh as placed: its bounds in the actor's own axes, never thinner than the minimum.
	const FBoxSphereBounds Local = Mesh->CalcBounds(Mesh->GetRelativeTransform());
	FVector Extent = Local.BoxExtent;
	Extent.X = FMath::Max(Extent.X, BreakableMinHalfThicknessCm);
	Extent.Y = FMath::Max(Extent.Y, BreakableMinHalfThicknessCm);
	Extent.Z = FMath::Max(Extent.Z, BreakableMinHalfThicknessCm);
	Box->SetRelativeLocation(Local.Origin);
	Box->SetBoxExtent(Extent);
	if (bMeshCollision)
	{
		// The box stays only as the measure of where the pieces start; the mesh takes the hits and stops bodies.
		Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Box->SetGenerateOverlapEvents(false);
		Mesh->SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);
		Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Mesh->SetGenerateOverlapEvents(true);
	}

	UGBStatsComponent* Stats = NewObject<UGBStatsComponent>(this, TEXT("BreakableStats"));
	Stats->RegisterComponent();
	AddInstanceComponent(Stats);
	Stats->SetBaseStat(GBStat::MaxHealth, BreakableHealth);
	Stats->SetBaseStat(GBStat::DamageTaken, 1.f);
	Stats->SetBaseStat(GBStat::HealthRegen, 0.f);
	Stats->SetBaseStat(GBStat::MaxMana, 0.f);

	Attributes = NewObject<UGBAttributeComponent>(this, TEXT("BreakableAttributes"));
	// Neutral: nobody targets it on purpose, but shots stop on it and damage reaches it.
	Attributes->Team = EGBTeam::Neutral;
	Attributes->RegisterComponent();
	AddInstanceComponent(Attributes);
	Attributes->InitializeFromStats();
	Attributes->OnDamaged.AddUObject(this, &AGBBreakable::HandleDamaged);
}

void AGBBreakable::HandleDamaged(const FGBDamageInfo& Info, float ActualDamage)
{
	UWorld* World = GetWorld();
	if (bBroken || !World)
	{
		return;
	}
	if (Attributes)
	{
		Attributes->RestoreAll();          // however hard the hit, the next one still lands
	}
	const double Now = World->GetTimeSeconds();
	if (!CountsAsHit(ActualDamage, MinHitDamage, static_cast<float>(Now - LastHitSeconds), HitCooldown))
	{
		return;
	}
	LastHitSeconds = Now;
	LastHitDirection = Info.HitDirection;
	++Hits;
	if (Hits >= HitsToBreak)
	{
		Break();
		return;
	}
	if (CrackMaterial)
	{
		if (!CrackInstance)
		{
			CrackInstance = UMaterialInstanceDynamic::Create(CrackMaterial, this);
			Mesh->SetOverlayMaterial(CrackInstance);
		}
		CrackInstance->SetScalarParameterValue(CrackStageParam, ComputeCrackStage(Hits, HitsToBreak));
	}
}

void AGBBreakable::Break()
{
	bBroken = true;
	{
		const FVector SoundAt = GetActorLocation();
		GBAudio::Play(this, FName(TEXT("breakable")), &SoundAt, 1.f, 1.f);
	}
	if (!BrokenFlag.IsNone())
	{
		if (UGBProgressSubsystem* Progress = UGBProgressSubsystem::Get(this))
		{
			Progress->SetFlag(BrokenFlag, true);
		}
	}
	const int32 Thrown = Shatter();
	if (Thrown == 0)
	{
		if (UWorld* World = GetWorld())
		{
			const FVector Centre = Box->GetComponentLocation();
			const float Radius = static_cast<float>(Box->GetScaledBoxExtent().GetMax());
			AGBExplosionEffect::Spawn(World, Centre, Radius, FLinearColor(0.75f, 0.72f, 0.68f), BreakBurstSeconds);
		}
	}
	// The hit that broke it is still being dealt by whoever called: gone from sight and out of the way now, removed after.
	Mesh->SetVisibility(false);
	Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetLifeSpan(Thrown > 0 ? FMath::Max(ShardSeconds, BreakRemoveSeconds) : BreakRemoveSeconds);
}

int32 AGBBreakable::Shatter()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return 0;
	}
	const FTransform BoxTransform = Box->GetComponentTransform();
	const FVector Extent = Box->GetUnscaledBoxExtent();
	// The thin axis of the box is the way through the wall; the pieces go the way the last hit went.
	FVector Through = BoxTransform.GetUnitAxis(Extent.X <= Extent.Y ? EAxis::X : EAxis::Y);
	if (FVector::DotProduct(Through, LastHitDirection) < 0.f)
	{
		Through = -Through;
	}

	if (BreakEffect)
	{
		for (int32 Index = 0; Index < BreakEffectCount; ++Index)
		{
			const FVector Local(FMath::FRandRange(-Extent.X, Extent.X), FMath::FRandRange(-Extent.Y, Extent.Y) * 0.85f, FMath::FRandRange(-Extent.Z, Extent.Z) * 0.85f);
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, BreakEffect, BoxTransform.TransformPosition(Local), Through.Rotation());
		}
	}

	ShardMeshes.RemoveAll([](const TObjectPtr<UStaticMesh>& Piece) { return Piece == nullptr; });
	if (ShardMeshes.Num() == 0 || ShardCount <= 0)
	{
		return 0;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GBBreakableShardFloor), false, this);
	for (int32 Index = 0; Index < ShardCount; ++Index)
	{
		const FVector Local(FMath::FRandRange(-Extent.X, Extent.X), FMath::FRandRange(-Extent.Y, Extent.Y), FMath::FRandRange(-Extent.Z, Extent.Z));
		const FVector Start = BoxTransform.TransformPosition(Local);

		UStaticMeshComponent* Piece = NewObject<UStaticMeshComponent>(this);
		Piece->SetMobility(EComponentMobility::Movable);
		Piece->SetStaticMesh(ShardMeshes[FMath::RandRange(0, ShardMeshes.Num() - 1)]);
		if (ShardMaterial)
		{
			for (int32 Slot = 0; Slot < Piece->GetNumMaterials(); ++Slot)
			{
				Piece->SetMaterial(Slot, ShardMaterial);
			}
		}
		Piece->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Piece->SetGenerateOverlapEvents(false);
		Piece->SetCanEverAffectNavigation(false);
		Piece->SetCastShadow(false);
		Piece->SetUsingAbsoluteLocation(true);
		Piece->SetUsingAbsoluteRotation(true);
		Piece->SetUsingAbsoluteScale(true);
		Piece->SetupAttachment(Root);
		Piece->RegisterComponent();
		Piece->SetWorldLocationAndRotation(Start, FRotator(FMath::FRandRange(0.f, 360.f), FMath::FRandRange(0.f, 360.f), FMath::FRandRange(0.f, 360.f)));
		Piece->SetWorldScale3D(FVector(FMath::FRandRange(static_cast<float>(ShardScale.X), static_cast<float>(FMath::Max(ShardScale.X, ShardScale.Y)))));

		FShardMotion Motion;
		// Mostly through the wall, a little to every side, some kicked up: it falls in a heap past the frame.
		const FVector Side = FVector(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-0.2f, 0.9f)) * 0.35f;
		Motion.Velocity = (Through * FMath::FRandRange(0.15f, 1.f) + Side) * ShardSpeed;
		Motion.Spin = FRotator(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f)) * ShardSpinDegrees;
		// Where it will come down, roughly: under a point a little along its flight.
		const FVector Ahead = Start + FVector(Motion.Velocity.X, Motion.Velocity.Y, 0.f) * 0.5f;
		FHitResult Floor;
		const bool bFloor = World->LineTraceSingleByObjectType(Floor, FVector(Ahead.X, Ahead.Y, Start.Z), FVector(Ahead.X, Ahead.Y, Start.Z - ShardFloorProbeCm),
			FCollisionObjectQueryParams(ECC_WorldStatic), Params);
		Motion.FloorZ = bFloor ? static_cast<float>(Floor.ImpactPoint.Z) + ShardRestLiftCm : static_cast<float>(Start.Z) - ShardFloorProbeCm;
		ShardComps.Add(Piece);
		ShardMotion.Add(Motion);
	}
	SetActorTickEnabled(true);
	return ShardComps.Num();
}

void AGBBreakable::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const UWorld* World = GetWorld();
	const float Gravity = World ? World->GetGravityZ() : -980.f;
	bool bAnyMoving = false;
	for (int32 Index = 0; Index < ShardComps.Num() && Index < ShardMotion.Num(); ++Index)
	{
		UStaticMeshComponent* Piece = ShardComps[Index];
		FShardMotion& Motion = ShardMotion[Index];
		if (!Piece || Motion.bResting)
		{
			continue;
		}
		Motion.Velocity.Z += Gravity * DeltaSeconds;
		FVector Location = Piece->GetComponentLocation() + Motion.Velocity * DeltaSeconds;
		FRotator Rotation = Piece->GetComponentRotation() + Motion.Spin * DeltaSeconds;
		if (Location.Z <= Motion.FloorZ && Motion.Velocity.Z < 0.f)
		{
			// Landed: it lies flat where it fell.
			Location.Z = Motion.FloorZ;
			Rotation = FRotator(0.f, Rotation.Yaw, FMath::RandBool() ? 0.f : 180.f);
			Motion.bResting = true;
		}
		else
		{
			bAnyMoving = true;
		}
		Piece->SetWorldLocationAndRotation(Location, Rotation);
	}
	if (!bAnyMoving)
	{
		SetActorTickEnabled(false);
	}
}
