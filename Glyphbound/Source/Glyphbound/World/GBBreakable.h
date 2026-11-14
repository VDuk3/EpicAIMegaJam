// A piece of the level that breaks after a few hits (a weak wall, a grate) and stays broken.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GBBreakable.generated.h"

class UBoxComponent;
class UGBAttributeComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UNiagaraSystem;
class UStaticMesh;
class UStaticMeshComponent;
struct FGBDamageInfo;

/**
 * One static mesh (set per placed instance) with a blocking box around it. It is a neutral thing with attributes,
 * so shots stop on it and anything that deals damage counts. It breaks after HitsToBreak hits, whatever each hit
 * deals; hits closer together than HitCooldown count as one (a shotgun blast, a flame). Cracks spread over it with
 * each hit through CrackMaterial, an overlay with a scalar "Stage" (0 = whole, 1 = about to break).
 * With BrokenFlag set, breaking sets that progress flag, and an instance whose flag exists is gone from the start.
 * With ShardMeshes set it shatters: pieces fly the way the last hit went, fall, lie on the floor for a while and go.
 * They are moved by hand (no physics bodies, no collision): they are only seen.
 */
UCLASS()
class GLYPHBOUND_API AGBBreakable : public AActor
{
	GENERATED_BODY()

public:
	AGBBreakable();

	UStaticMeshComponent* GetMesh() const { return Mesh; }
	int32 GetHits() const { return Hits; }
	/** Broken: out of sight and out of the way; the actor itself stays a few seconds while its pieces lie there. */
	bool IsBroken() const { return bBroken; }

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

	/**
	 * The mesh's own shape blocks instead of the box round it: for a hollow thing with something standing inside it
	 * (the glass of the Valkyrie's tube). The mesh asset must use its triangles as collision (complex as simple).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glyphbound|Breakable")
	bool bMeshCollision = false;

	/** Overlay drawn over the mesh from the first hit on. Its scalar "Stage" goes from 0 to 1 as the hits add up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glyphbound|Breakable")
	TObjectPtr<UMaterialInterface> CrackMaterial;

	/** Pieces thrown when it breaks, each a random one of these. Empty = the plain burst. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glyphbound|Breakable|Shatter")
	TArray<TObjectPtr<UStaticMesh>> ShardMeshes;

	/** Material of the pieces (none = each mesh keeps its own). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glyphbound|Breakable|Shatter")
	TObjectPtr<UMaterialInterface> ShardMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glyphbound|Breakable|Shatter", meta = (ClampMin = "0", ClampMax = "200"))
	int32 ShardCount = 48;

	/** Smallest and largest scale of a piece. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glyphbound|Breakable|Shatter")
	FVector2D ShardScale = FVector2D(1.0, 3.0);

	/** cm/s of the fastest pieces, along the last hit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glyphbound|Breakable|Shatter")
	float ShardSpeed = 520.f;

	/** Seconds the pieces stay. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glyphbound|Breakable|Shatter")
	float ShardSeconds = 8.f;

	/** Effect played at a few points of the face when it breaks. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glyphbound|Breakable|Shatter")
	TObjectPtr<UNiagaraSystem> BreakEffect;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glyphbound|Breakable|Shatter", meta = (ClampMin = "0", ClampMax = "24"))
	int32 BreakEffectCount = 6;

	int32 GetShardCountAlive() const { return ShardComps.Num(); }

	virtual void Tick(float DeltaSeconds) override;

	/** Pure rule, for tests: how far the cracks have spread (0..1) after Hits of HitsToBreak. */
	static float ComputeCrackStage(int32 InHits, int32 InHitsToBreak);

	/** Pure rule, for tests: whether a hit counts, given its damage and the seconds since the last counted hit. */
	static bool CountsAsHit(float Damage, float MinDamage, float SecondsSinceLastHit, float Cooldown);

protected:
	virtual void BeginPlay() override;

	void HandleDamaged(const FGBDamageInfo& Info, float ActualDamage);
	void Break();
	/** Throws the pieces. Returns how many. */
	int32 Shatter();

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

	struct FShardMotion
	{
		FVector Velocity = FVector::ZeroVector;
		FRotator Spin = FRotator::ZeroRotator;
		float FloorZ = 0.f;
		bool bResting = false;
	};

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> ShardComps;
	TArray<FShardMotion> ShardMotion;
	FVector LastHitDirection = FVector::ZeroVector;

	int32 Hits = 0;
	double LastHitSeconds = -1000.0;
	bool bBroken = false;
};
