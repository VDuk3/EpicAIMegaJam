// A short shockwave shown by every explosion (fireball, grenade...), whether or not it damaged anything.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GBExplosionEffect.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;
class UMaterialInstanceDynamic;

/**
 * Purely visual, no collision: a glowing sphere that grows from a small size to the full blast radius while it
 * fades out, plus a point light flash that fades with it. Destroys itself after Duration (game time, so it
 * follows the QTE slow motion like everything else).
 */
UCLASS()
class GLYPHBOUND_API AGBExplosionEffect : public AActor
{
	GENERATED_BODY()

public:
	AGBExplosionEffect();

	/** Radius in cm (the blast radius), Duration in seconds (from data). Returns null if the spawn fails. */
	static AGBExplosionEffect* Spawn(UWorld* World, const FVector& Location, float Radius, const FLinearColor& Color, float Duration);

	virtual void Tick(float DeltaSeconds) override;

protected:
	void InitEffect(float InRadius, const FLinearColor& InColor, float InDuration);
	/** Applies the look at a point of the animation, Alpha 0..1. */
	void ApplyAlpha(float Alpha);

	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Sphere;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UPointLightComponent> Flash;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> SphereMaterial;

	float Radius = 100.f;
	float Duration = 0.35f;
	float Age = 0.f;
	FLinearColor Color = FLinearColor::White;
	/** Relative scale of the sphere at the full blast radius. */
	FVector FullScale = FVector::OneVector;
};
