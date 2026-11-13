// Plays one baked pixel VFX clip: a camera-facing quad plus a real point light on a baked track.
#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "GBPixelVfxTypes.h"
#include "GBPixelVfxComponent.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;
class UMaterialInstanceDynamic;

UCLASS(ClassGroup = "Glyphbound", meta = (BlueprintSpawnableComponent))
class GBPIXELVFX_API UGBPixelVfxComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UGBPixelVfxComponent();

	// Starts playing a clip (builds the quad and the light on first use). False when unknown.
	UFUNCTION(BlueprintCallable, Category = "PixelVfx")
	bool Play(FName ClipId, bool bRestart = true);

	UFUNCTION(BlueprintCallable, Category = "PixelVfx")
	void Stop();

	UFUNCTION(BlueprintCallable, Category = "PixelVfx")
	bool IsPlaying() const { return bPlaying && CurrentClip.IsValid(); }

	// Destroy the owner actor when a one-shot clip finishes.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PixelVfx")
	bool bAutoDestroyOwnerWhenFinished = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PixelVfx")
	float PlayRate = 1.f;

	// Multiplier on the clip world size and on the light radius.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PixelVfx")
	float SizeScale = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PixelVfx")
	float LightIntensityScale = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PixelVfx")
	bool bEnableLight = true;

	// Defaults to the clip's value once Play loads it.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PixelVfx")
	bool bLightCastsShadows = true;

	bool bLightCastsShadowsSet = false;

	// When set, replaces the clip facing. "None" (the default) uses the clip value.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PixelVfx")
	FName FacingOverride;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PixelVfx")
	float StartTimeOffset = 0.f;

	// Quad world origin/extent for diagnostics (bake showcase logging).
	void GetQuadWorldBounds(FVector& OutOrigin, FVector& OutExtent) const;

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	void EnsureBuilt();
	void RebuildLight();
	void UpdateFrame(bool bForce);
	void UpdateQuadTransform();
	FVector PivotWorldOffset() const;

	TSharedPtr<FGBPixelVfxClip> CurrentClip;
	FName CurrentClipId = NAME_None;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Quad = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> QuadMaterial = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UPointLightComponent> ClipLight = nullptr;

	bool bPlaying = false;
	float PlayTime = 0.f;
	int32 CurrentFrame = -1;
	bool bQuadMissingMaterialWarned = false;
};
