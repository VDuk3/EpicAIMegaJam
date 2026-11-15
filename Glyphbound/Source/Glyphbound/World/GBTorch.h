// A wall torch of the castle (PAST_CASTLE_PLAN.md: every light has its fixture). Its holder is a mesh (the pixel twin
// of the castle pack's torch, set by the map's builder); its flame is a clip baked from a Niagara Fluids fire by the
// GBPixelVfx plugin (a pixel-art flipbook on a camera-facing quad), and its light is the clip's own point light, which
// follows the flame's baked brightness.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GBTorch.generated.h"

class UStaticMeshComponent;
class UGBPixelVfxComponent;

UCLASS()
class GLYPHBOUND_API AGBTorch : public AActor
{
	GENERATED_BODY()

public:
	AGBTorch();

	/** The torch itself: stick and iron bracket. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Glyphbound|Torch")
	TObjectPtr<UStaticMeshComponent> Holder;

	/** The flame and its light (placed at the holder's head by the builder). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Glyphbound|Torch")
	TObjectPtr<UGBPixelVfxComponent> Flame;

	/** Baked clip in Content/Data/Vfx. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glyphbound|Torch")
	FName ClipId = TEXT("TorchFire");

	/** Flame size (and light radius) as a multiple of the clip's. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glyphbound|Torch")
	float FlameSize = 1.f;

	/** Light intensity as a multiple of the clip's. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glyphbound|Torch")
	float LightScale = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glyphbound|Torch")
	bool bLightCastsShadows = true;

	/** The light's place relative to the flame, in the torch's frame (+Y leans out of the wall). A light a hand's
	 *  breadth from the wall drew hard dark wedges down the stone under each torch (owner); 1.5 m out it lights the
	 *  passage evenly. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glyphbound|Torch")
	FVector LightOffset = FVector(0.f, 150.f, 0.f);

protected:
	virtual void BeginPlay() override;
};
