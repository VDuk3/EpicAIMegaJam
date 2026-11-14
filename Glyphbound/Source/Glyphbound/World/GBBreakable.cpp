#include "World/GBBreakable.h"

#include "Combat/GBExplosionEffect.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/GBStatNames.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Stats/GBAttributeComponent.h"
#include "Stats/GBStatsComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "World/GBProgressSubsystem.h"

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
}

AGBBreakable::AGBBreakable()
{
	PrimaryActorTick.bCanEverTick = false;

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
	if (!BrokenFlag.IsNone())
	{
		if (UGBProgressSubsystem* Progress = UGBProgressSubsystem::Get(this))
		{
			Progress->SetFlag(BrokenFlag, true);
		}
	}
	if (UWorld* World = GetWorld())
	{
		const FVector Centre = Box->GetComponentLocation();
		const float Radius = static_cast<float>(Box->GetScaledBoxExtent().GetMax());
		AGBExplosionEffect::Spawn(World, Centre, Radius, FLinearColor(0.75f, 0.72f, 0.68f), BreakBurstSeconds);
	}
	// The hit that broke it is still being dealt by whoever called: gone from sight and out of the way now, removed after.
	Mesh->SetVisibility(false);
	Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetLifeSpan(BreakRemoveSeconds);
}
