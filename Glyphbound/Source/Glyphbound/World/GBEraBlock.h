// A placed level piece that exists only in one era and / or depends on progress flags: wards, plugs, door leaves, pylons.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/GBTypes.h"
#include "GBEraBlock.generated.h"

class UStaticMeshComponent;
class UGBEraComponent;

/**
 * One static mesh (set per instance in the Editor; default the engine cube) with collision BlockAll and a
 * UGBEraComponent whose Presence, RequiredFlag and RemovedByFlag are edited on each placed instance.
 * Not present (wrong era, missing required flag, removed-by flag set) = hidden, no collision and out of the
 * navmesh, so enemies path through where it was. No tick. Also works spawned in code (Configure).
 */
UCLASS()
class GLYPHBOUND_API AGBEraBlock : public AActor
{
	GENERATED_BODY()

public:
	AGBEraBlock();

	UStaticMeshComponent* GetMesh() const { return Mesh; }
	UGBEraComponent* GetEraComponent() const { return EraComponent; }
	bool IsPresent() const;

	/** For blocks spawned at runtime: sets the era rule and applies it. */
	void Configure(EGBEraPresence InPresence, FName InRequiredFlag = NAME_None, FName InRemovedByFlag = NAME_None);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Tells the dynamic navmesh that this block now blocks (or no longer blocks) paths. */
	void HandlePresenceChanged();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Glyphbound|Era Block", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Glyphbound|Era Block", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UGBEraComponent> EraComponent;
};
