// A piece of the level that breaks after a few hits (a weak wall, a grate) and stays broken.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GBBreakable.generated.h"

class UBoxComponent;
class UGBAttributeComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMeshComponent;
struct FGBDamageInfo;

/**
 * One static mesh (set per placed instance) with a blocking box around it. It is a neutral thing with attributes,
 * so shots stop on it and anything that deals damage counts. It breaks after HitsToBreak hits, whatever each hit
 * deals; hits closer together than HitCooldown count as one (a shotgun blast, a flame). Cracks spread over it with
 * each hit through CrackMaterial, an overlay with a scalar "Stage" (0 = whole, 1 = about to break).
 * With BrokenFlag set, breaking sets that progress flag, and an instance whose flag exists is gone from the start.
 */
UCLASS()
class GLYPHBOUND_API AGBBreakable : public AActor
{
	GENERATED_BODY()

public:
	AGBBreakable();

	UStaticMeshComponent* GetMesh() const { return Mesh; }
	int32 GetHits() const { return Hits; }

	/** Hits that break it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glyphbound|Breakable", meta = (ClampMin = "1"))
	int32 HitsToBreak = 4;

	/** A hit that deals less than this does not count (a status tick, a graze). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glyphbound|Breakable")
	float MinHitDamage = 1.f;

	/** Seconds after a counted hit during which further hits do not count. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glyphbound|Breakable")
	float HitCooldown = 0.25f;

	/** Progress flag set when it breaks; an instance whose flag already exists removes itself. None = not remembered. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glyphbound|Breakable")
	FName BrokenFlag = NAME_None;

	/** Overlay drawn over the mesh from the first hit on. Its scalar "Stage" goes from 0 to 1 as the hits add up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glyphbound|Breakable")
	TObjectPtr<UMaterialInterface> CrackMaterial;

	/** Pure rule, for tests: how far the cracks have spread (0..1) after Hits of HitsToBreak. */
	static float ComputeCrackStage(int32 InHits, int32 InHitsToBreak);

	/** Pure rule, for tests: whether a hit counts, given its damage and the seconds since the last counted hit. */
	static bool CountsAsHit(float Damage, float MinDamage, float SecondsSinceLastHit, float Cooldown);

protected:
	virtual void BeginPlay() override;

	void HandleDamaged(const FGBDamageInfo& Info, float ActualDamage);
	void Break();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Glyphbound|Breakable", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Glyphbound|Breakable", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Glyphbound|Breakable", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> Box;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> CrackInstance;

	UPROPERTY(Transient)
	TObjectPtr<UGBAttributeComponent> Attributes;

	int32 Hits = 0;
	double LastHitSeconds = -1000.0;
	bool bBroken = false;
};
