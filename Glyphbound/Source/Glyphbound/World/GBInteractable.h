// Things the player uses with the Interact key (G / Triangle): portals, chests, pickups, the seal.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UObject/Interface.h"
#include "GBInteractable.generated.h"

class AGBPlayerCharacter;

UINTERFACE()
class UGBInteractable : public UInterface
{
	GENERATED_BODY()
};

/**
 * The player looks for the nearest interactable in front of it every frame (within the InteractRange stat) and the
 * HUD shows GetInteractPrompt(). Implemented by AGBInteractableActor subclasses, and by anything else that wants to.
 */
class GLYPHBOUND_API IGBInteractable
{
	GENERATED_BODY()

public:
	virtual bool CanInteract(const AGBPlayerCharacter* Player) const { return true; }
	/** Not pure virtual: Unreal must be able to build a default object of every class that implements this. */
	virtual void Interact(AGBPlayerCharacter* Player) {}
	/** Short English text for the HUD, e.g. "Travel to the future". The HUD adds the key name. */
	virtual FString GetInteractPrompt(const AGBPlayerCharacter* Player) const { return TEXT("Interact"); }
};

/**
 * Convenience base: a root, an optional placeholder look (sprite or colored cube, chosen by bPixelArt per the
 * art rule), and a UGBEraComponent so it can exist in one era only.
 */
UCLASS(Abstract)
class GLYPHBOUND_API AGBInteractableActor : public AActor, public IGBInteractable
{
	GENERATED_BODY()

public:
	AGBInteractableActor();

	/** Placeholder look. Call after spawn (or from BeginPlay). SpriteId is used when bPixelArt, Color otherwise (and as a tint). */
	void SetPlaceholderLook(bool bPixelArt, FName SpriteId, const FLinearColor& Color, const FVector& Size);

	class UGBEraComponent* GetEraComponent() const { return EraComponent; }

protected:
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere) TObjectPtr<class UStaticMeshComponent> Mesh;
	UPROPERTY(VisibleAnywhere) TObjectPtr<class UBillboardComponent> Sprite;
	/** Blocks nothing; it is what the player's interact query finds. */
	UPROPERTY(VisibleAnywhere) TObjectPtr<class USphereComponent> InteractVolume;
	UPROPERTY(VisibleAnywhere) TObjectPtr<class UGBEraComponent> EraComponent;
};
