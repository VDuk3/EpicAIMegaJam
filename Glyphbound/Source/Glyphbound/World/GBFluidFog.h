// Smoke in a tunnel: a Niagara Fluids gas simulation (Grid 3D) that parts around the player, the enemies, shots and blasts.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GBFluidFog.generated.h"

class UNiagaraComponent;
class UBoxComponent;
class UNiagaraDataChannelAsset;

namespace GBFluidFog
{
	/**
	 * Pure rule: the part of the segment Start..End inside Box, as the two fractions (0..1) of the segment.
	 * False when the segment misses the box (a segment that only touches a face counts as a miss).
	 */
	GLYPHBOUND_API bool ClipSegment(const FVector& Start, const FVector& End, const FBox& Box, float& OutT0, float& OutT1);

	/**
	 * Pure rule: points along the part of the segment inside Box, the first where it enters (or at Start when it
	 * starts inside), then one every Spacing cm. When that would be more than MaxPoints, the spacing grows so the
	 * row ends at the far end of the clipped part with exactly MaxPoints. Returns the number added to OutPoints.
	 */
	GLYPHBOUND_API int32 ClipSegmentPoints(const FVector& Start, const FVector& End, const FBox& Box, float Spacing, int32 MaxPoints, TArray<FVector>& OutPoints);
}

/**
 * A box (BoxExtent is the tunnel's interior) holding a Niagara Fluids gas system (data Game.json FluidFog.System).
 * The simulation runs only while the player is within FluidFog.ActiveRange of the box. While it runs, the actor
 * writes what moves through the box into the two global Niagara data channels of the NiagaraFluids plugin: moving
 * collision spheres (player, enemies, hitscan shots, projectiles) and velocity sources (impact puffs, blasts).
 * If the system asset does not exist the actor logs one warning and does nothing.
 */
UCLASS()
class GLYPHBOUND_API AGBFluidFog : public AActor
{
	GENERATED_BODY()

public:
	AGBFluidFog();

	virtual void Tick(float DeltaSeconds) override;

	/** Half size of the tunnel's interior around the actor's location, cm. Set it before BeginPlay (or call ApplyBox after). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glyphbound|Fog")
	FVector BoxExtent = FVector(750.f, 160.f, 150.f);

	/** Re-reads BoxExtent into the box and the system's size (call after changing BoxExtent at run time). */
	void ApplyBox();

	/** True while the simulation is running (the player is near and the system loaded). */
	bool IsSimulating() const { return bSimulating; }
	bool HasSystem() const { return bHasSystem; }
	UNiagaraComponent* GetFluid() const { return Fluid; }
	FBox GetBox() const;

	/** Entries written to the data channels on the last frame (for the smoke test and debugging). */
	int32 GetLastCollisionEntries() const { return LastCollisionEntries; }
	int32 GetLastPlayerEntries() const { return LastPlayerEntries; }
	int32 GetLastShotEntries() const { return LastShotEntries; }
	int32 GetLastSourceEntries() const { return LastSourceEntries; }
	/** Total of collision entries written since the actor began. */
	int32 GetTotalShotEntriesWritten() const { return TotalShotEntries; }

	/** Feeds one hitscan shot as if the event had fired (the event handler calls it; the smoke test too). */
	void AddShot(const FVector& Start, const FVector& End);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	struct FEntry
	{
		FVector Position = FVector::ZeroVector;
		FVector Velocity = FVector::ZeroVector;
		float Radius = 10.f;
		/** Frames the entry is still written (pending entries only). */
		int32 FramesLeft = 1;
	};
	struct FTrackedProjectile
	{
		TWeakObjectPtr<AActor> Projectile;
		FVector LastPosition = FVector::ZeroVector;
	};

	void SetSimulating(bool bOn);
	void OnShotTraced(const FVector& Start, const FVector& End, float Speed);
	void AddBlast(const FVector& Where);
	void GatherProjectiles(TArray<FEntry>& Collision);
	void GatherEnemies(const FVector& PlayerLocation, TArray<FEntry>& Collision);
	/** Writes them to the gas channel; returns how many entries were really written (0 if the channel or the write failed). */
	int32 WriteChannels(const TArray<FEntry>& Collision, const TArray<FEntry>& Sources);
	void LoadSettings();

	UPROPERTY(VisibleAnywhere, Category = "Glyphbound|Fog")
	TObjectPtr<UBoxComponent> Box;

	UPROPERTY(VisibleAnywhere, Category = "Glyphbound|Fog")
	TObjectPtr<UNiagaraComponent> Fluid;

	/** The plugin's two global data channels (collision spheres, velocity sources). */
	UPROPERTY(Transient)
	TObjectPtr<UNiagaraDataChannelAsset> CollisionChannel;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraDataChannelAsset> SourceChannel;

	FDelegateHandle ShotHandle;
	TArray<FEntry> PendingCollision;
	TArray<FEntry> PendingSources;
	TArray<FTrackedProjectile> Tracked;

	bool bHasSystem = false;
	bool bSimulating = false;
	bool bWarned = false;
	int32 LastCollisionEntries = 0;
	int32 LastPlayerEntries = 0;
	int32 LastShotEntries = 0;
	int32 LastSourceEntries = 0;
	int32 TotalShotEntries = 0;

	// Settings, read from Game.json in BeginPlay (FluidFog.*).
	float ActiveRange = 3000.f;
	float WarmupSeconds = 2.f;
	int32 Resolution = 192;
	float PlayerRadius = 45.f;
	float ShotRadius = 12.f;
	float ShotPush = 1500.f;
	float ShotSpacing = 40.f;
	int32 ShotFrames = 3;
	float PuffPush = 500.f;
	float PuffRadius = 40.f;
	float ProjectileRadius = 25.f;
	float BlastPush = 1200.f;
	float BlastRadius = 90.f;
	float EnemyRange = 1500.f;
	int32 MaxEntries = 64;
	bool bGridCornered = false;
	/** The fill: density the low ellipsoid source adds (system User.FillDensity), sized from the box in ApplyBox. */
	float FillDensity = 2.f;
	/** How hard a moving body (player, enemy, shot, projectile) pushes the gas out of its sphere (source divergence). */
	float PushDivergence = 1.f;
};
