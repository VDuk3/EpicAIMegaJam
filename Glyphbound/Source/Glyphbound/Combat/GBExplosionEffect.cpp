#include "Combat/GBExplosionEffect.h"

#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/GBPlaceholder.h"
#include "Core/GBTypes.h"
#include "Engine/Scene.h"
#include "Engine/World.h"

namespace
{
	// Look of the shockwave only (the size and the duration come from the caller's data).
	/** The sphere starts at this fraction of the blast radius. */
	constexpr float ExplosionStartScale = 0.15f;
	/** Brightness of the glow sphere relative to the explosion color. */
	constexpr float ExplosionGlowBoost = 1.5f;
	constexpr float ExplosionFlashCandelas = 800.f;
	/** The flash lights the surroundings beyond the blast itself. */
	constexpr float ExplosionFlashRadiusScale = 2.5f;
	/** Shortest animation, so a 0 in the data cannot divide by zero. */
	constexpr float ExplosionMinDuration = 0.05f;
	/** Extra seconds before the safety life span ends the actor, in case Tick never reaches the end. */
	constexpr float ExplosionLifeSpanMargin = 1.f;
}

AGBExplosionEffect::AGBExplosionEffect()
{
	PrimaryActorTick.bCanEverTick = true;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Movable);
	RootComponent = Root;

	Sphere = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Sphere"));
	Sphere->SetupAttachment(Root);
	Sphere->SetMobility(EComponentMobility::Movable);
	Sphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Sphere->SetGenerateOverlapEvents(false);
	Sphere->SetCastShadow(false);
	Sphere->SetCanEverAffectNavigation(false);

	Flash = CreateDefaultSubobject<UPointLightComponent>(TEXT("Flash"));
	Flash->SetupAttachment(Root);
	Flash->SetMobility(EComponentMobility::Movable);
	// Plain defaults here (no render state yet); InitEffect / ApplyAlpha drive the light at runtime.
	Flash->CastShadows = false;
	Flash->IntensityUnits = ELightUnits::Candelas;
	Flash->Intensity = 0.f;
}

AGBExplosionEffect* AGBExplosionEffect::Spawn(UWorld* World, const FVector& Location, float InRadius, const FLinearColor& InColor, float InDuration)
{
	if (!World || InRadius <= 0.f)
	{
		return nullptr;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AGBExplosionEffect* Effect = World->SpawnActor<AGBExplosionEffect>(AGBExplosionEffect::StaticClass(), Location, FRotator::ZeroRotator, Params);
	if (!Effect)
	{
		UE_LOG(LogGB, Warning, TEXT("Explosion effect: spawn failed"));
		return nullptr;
	}
	Effect->InitEffect(InRadius, InColor, InDuration);
	return Effect;
}

void AGBExplosionEffect::InitEffect(float InRadius, const FLinearColor& InColor, float InDuration)
{
	Radius = FMath::Max(InRadius, 1.f);
	Color = InColor;
	Duration = FMath::Max(InDuration, ExplosionMinDuration);
	Age = 0.f;

	SphereMaterial = GBPlaceholder::SetupShape(Sphere, EGBShape::Sphere, FVector(Radius * 2.f), Color, /*bGlow*/ true);
	// SetupShape sized the sphere to the full blast; the animation scales down from there.
	FullScale = Sphere->GetRelativeScale3D();
	Flash->SetLightColor(Color);
	Flash->SetAttenuationRadius(Radius * ExplosionFlashRadiusScale);

	SetLifeSpan(Duration + ExplosionLifeSpanMargin);
	ApplyAlpha(0.f);
}

void AGBExplosionEffect::ApplyAlpha(float Alpha)
{
	const float T = FMath::Clamp(Alpha, 0.f, 1.f);
	const float Remaining = 1.f - T;
	// Fast at first, slowing down at the edge, like a shockwave.
	const float Grow = ExplosionStartScale + (1.f - ExplosionStartScale) * (1.f - Remaining * Remaining);

	Sphere->SetRelativeScale3D(FullScale * Grow);
	// The glow material is additive: fading the color to black fades the sphere out.
	GBPlaceholder::SetShapeColor(Sphere, Color * (ExplosionGlowBoost * Remaining));
	Flash->SetIntensity(ExplosionFlashCandelas * Remaining * Remaining);
}

void AGBExplosionEffect::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	Age += DeltaSeconds;
	if (Age >= Duration)
	{
		Destroy();
		return;
	}
	ApplyAlpha(Age / Duration);
}
