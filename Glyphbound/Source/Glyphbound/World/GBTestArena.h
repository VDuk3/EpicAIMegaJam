// The day-1 playable test level (GDD 15), built entirely in code from placeholder shapes: no map asset needed.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/GBTypes.h"
#include "Core/GBPlaceholder.h"
#include "Data/GBDataTypes.h"
#include "GBTestArena.generated.h"

class AStaticMeshActor;
class APointLight;
class ADirectionalLight;
class ASkyLight;
class AExponentialHeightFog;
class APlayerStart;
class ANavMeshBoundsVolume;

/**
 * Spawned by AGBGameMode::InitGame when the map has no PlayerStart.
 *  - BuildStatic() (called from InitGame, before the player exists): lights, sky, fog, the navigation bounds, the
 *    shell (floor, walls, the movement course, the raised platform, the era-gated room) and the APlayerStart.
 *    Structures exist in both eras with one collision body; what changes with the era is the look (GDD 3):
 *    pixel-art stone and wood in the past, plain dark 3D with neon in the future.
 *  - BeginPlay(): everything with gameplay: era decorations, cover between the encounters' spawn points and the
 *    player, the central portal, the era-gated door, the future-only ramp unlocked by the past encounter,
 *    encounters, training dummies, pickups, chests and the start checkpoint; then it builds the navmesh.
 * The layout numbers are level design and live in one block at the top of GBTestArena.cpp. The movement course
 * heights and the dash gap are derived from Player.json (jump velocity, gravity scale, speed, dash distance).
 */
UCLASS()
class GLYPHBOUND_API AGBTestArena : public AActor
{
	GENERATED_BODY()

public:
	AGBTestArena();

	/** Geometry, lighting and the PlayerStart. Safe to call once; BeginPlay calls it if nobody did. */
	void BuildStatic();

	const FTransform& GetPlayerStartTransform() const { return PlayerStartTransform; }
	/** The APlayerStart created by BuildStatic (null until built). */
	APlayerStart* GetPlayerStartActor() const { return PlayerStartActor; }

	/**
	 * For the smoke run's pathing check: an enemy standing at OutEnemySpawn has a solid wall (present in both eras)
	 * on the straight line to OutPlayerSpot (the player start), so it can only reach the player by going around.
	 */
	static void GetPathTestPoints(FVector& OutEnemySpawn, FVector& OutPlayerSpot);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void BuildGameplay();
	void BuildPastDecor();
	void BuildFutureDecor();
	/** Cover in both eras between spawn points and the player: makes enemies path around things in normal play. */
	void BuildCover();

	/** A navmesh bounds volume over the whole arena (a volume spawned at runtime has no brush, so it gets a box body). */
	void SpawnNavigationBounds();
	/** Builds the navmesh now, once everything is spawned, so the first enemies already have paths. */
	void BuildNavigation();

	/** Sun color and intensity, sky light and fog follow the era. Bound to UGBEraSubsystem::OnEraChanged. */
	void ApplyEraLighting(EGBEra Era);

	/** A placeholder shape as a movable static mesh actor. bCollision = blocks everything (and shapes the navmesh). */
	AStaticMeshActor* SpawnShape(EGBShape Shape, const FVector& Center, const FVector& Size, const FLinearColor& Color,
		bool bGlow = false, bool bCollision = true, const FRotator& Rotation = FRotator::ZeroRotator);
	AStaticMeshActor* SpawnBlock(const FVector& Center, const FVector& Size, const FLinearColor& Color, bool bCollision = true)
	{
		return SpawnShape(EGBShape::Cube, Center, Size, Color, false, bCollision);
	}
	AStaticMeshActor* SpawnGlow(const FVector& Center, const FVector& Size, const FLinearColor& Color, const FRotator& Rotation = FRotator::ZeroRotator)
	{
		return SpawnShape(EGBShape::Cube, Center, Size, Color, true, false, Rotation);
	}
	/** A shape covered with a pixel-art tile texture (medieval structures, GDD 3). */
	AStaticMeshActor* SpawnTexturedShape(EGBShape Shape, const FVector& Center, const FVector& Size, FName TextureId, float TileSize,
		bool bCollision, const FRotator& Rotation = FRotator::ZeroRotator, const FLinearColor& Tint = FLinearColor::White);
	/**
	 * A structure of both eras: one hidden collision body present in both, a PastOnly pixel-art look (PastTexture)
	 * and a FutureOnly plain 3D look (FutureColor), neither of which collides. Era travel never changes footing or
	 * the navmesh under it. Returns the collision actor.
	 */
	AStaticMeshActor* SpawnStructure(EGBShape Shape, const FVector& Center, const FVector& Size, FName PastTexture, float PastTileSize,
		const FLinearColor& FutureColor, const FRotator& Rotation = FRotator::ZeroRotator);
	APointLight* SpawnPointLight(const FVector& Location, const FLinearColor& Color, float Candelas, float Radius);

	/** Adds a UGBEraComponent to a spawned actor so it exists in one era only (and optionally needs a flag). */
	void SetPresence(AActor* Actor, EGBEraPresence Presence, FName RequiredFlag = NAME_None);

	static FGBDropDefinition MakeItemDrop(FName ItemId, int32 Count = 1);
	static FGBDropDefinition MakeWeaponDrop(FName WeaponId);
	static FGBDropDefinition MakeSpellDrop(FName SpellGroup);

	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;

	UPROPERTY() TObjectPtr<ADirectionalLight> Sun;
	UPROPERTY() TObjectPtr<ASkyLight> SkyLight;
	UPROPERTY() TObjectPtr<AExponentialHeightFog> Fog;
	UPROPERTY() TObjectPtr<ANavMeshBoundsVolume> NavBounds;

	FTransform PlayerStartTransform;
	TObjectPtr<APlayerStart> PlayerStartActor = nullptr;
	/** Where the movement course ends (computed from Player.json by BuildStatic): the reward goes on top. */
	FVector CourseLandingTop = FVector::ZeroVector;
	bool bStaticBuilt = false;
	bool bGameplayBuilt = false;
	FDelegateHandle EraChangedHandle;
};
