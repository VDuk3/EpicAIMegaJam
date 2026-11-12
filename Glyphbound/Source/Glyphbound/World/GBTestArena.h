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

/**
 * Spawned by AGBGameMode::InitGame when the map has no PlayerStart.
 *  - BuildStatic() (called from InitGame, before the player exists): lights, sky, fog, floor, walls, the movement
 *    course (double jump steps and a dash gap), the era-gated room, the raised platform, and the APlayerStart.
 *  - BeginPlay(): everything with gameplay: era decorations (past = warm stone, crates, pillars, torches;
 *    future = dark metal, neon strips, cold lights), the central portal, the era-gated door, the future-only ramp
 *    unlocked by the past encounter, encounters, training dummies, pickups, chests and the start checkpoint.
 * The layout numbers are level design and live in one block at the top of GBTestArena.cpp.
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

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void BuildGameplay();
	void BuildPastDecor();
	void BuildFutureDecor();

	/** Sun color and intensity, sky light and fog follow the era. Bound to UGBEraSubsystem::OnEraChanged. */
	void ApplyEraLighting(EGBEra Era);

	/** A placeholder shape as a movable static mesh actor. bCollision = blocks everything. */
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

	FTransform PlayerStartTransform;
	bool bStaticBuilt = false;
	bool bGameplayBuilt = false;
	FDelegateHandle EraChangedHandle;
};
