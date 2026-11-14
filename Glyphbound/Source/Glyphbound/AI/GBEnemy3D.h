// Shared helpers of the 3D enemies of the future (skeletal shooters, drone, sentinel, spider): every asset is read from
// Enemies.json "Strings" and may be missing (the enemy then keeps its placeholder look); every shot is a hitscan with
// Niagara effects (muzzle flash, tracer, impact) whose systems come from "Strings" too.
#pragma once

#include "CoreMinimal.h"
#include "Core/GBTypes.h"
#include "Data/GBDataTypes.h"
#include "GBEnemy3D.generated.h"

class AGBEnemyCharacter;
class UNiagaraSystem;
class UNiagaraComponent;
class UMeshComponent;

/** Enemies.json "Strings" keys of the 3D enemies. */
namespace GBEnemy3DKey
{
	inline const FName Mesh(TEXT("Mesh"));
	inline const FName Armor(TEXT("Armor"));                      // ';'-separated USkeletalMesh paths on the same skeleton
	inline const FName Weapon(TEXT("Weapon"));
	inline const FName WeaponSocket(TEXT("WeaponSocket"));
	inline const FName WeaponOffset(TEXT("WeaponOffset"));        // "x y z pitch yaw roll scale"
	inline const FName FxMuzzle(TEXT("Fx.Muzzle"));
	inline const FName FxTracer(TEXT("Fx.Tracer"));
	inline const FName FxImpact(TEXT("Fx.Impact"));
	inline const FName FxHitImpact(TEXT("Fx.HitImpact"));
	inline const FName FxTracerEndParam(TEXT("Fx.TracerEndParam"));
	inline const FName FxTracerLengthParam(TEXT("Fx.TracerLengthParam"));
	inline const FName FxGlowParam(TEXT("Fx.GlowParam"));
	inline const FName Body(TEXT("Body"));
	inline const FName Eye(TEXT("Eye"));
	inline const FName Shell(TEXT("Shell"));
	inline const FName Core(TEXT("Core"));
}

/** Niagara systems of an enemy's shots (kept alive by the owner's UPROPERTY). */
USTRUCT()
struct FGBEnemyShotFx
{
	GENERATED_BODY()

	UPROPERTY() TObjectPtr<UNiagaraSystem> Muzzle;
	UPROPERTY() TObjectPtr<UNiagaraSystem> Tracer;
	UPROPERTY() TObjectPtr<UNiagaraSystem> Impact;
	UPROPERTY() TObjectPtr<UNiagaraSystem> HitImpact;
	FName TracerEndParam = NAME_None;
	FName TracerLengthParam = NAME_None;
};

/** What a hitscan shot did. */
struct FGBEnemyShotResult
{
	FVector End = FVector::ZeroVector;
	bool bHitWorld = false;
	bool bHitTarget = false;
	float DamageDealt = 0.f;
};

namespace GBEnemy3D
{
	/** Normalizes "/Game/A/B" to "/Game/A/B.B" so LoadObject finds it. */
	GLYPHBOUND_API FString NormalizeAssetPath(const FString& Path);

	/** Loads an asset by path; null for an empty path or a missing asset (logged once per path at Display level, not as a warning). */
	GLYPHBOUND_API UObject* LoadAssetQuiet(UClass* Class, const FString& Path, FName OwnerId);
	template<class T>
	T* LoadAsset(const FString& Path, FName OwnerId)
	{
		return Cast<T>(LoadAssetQuiet(T::StaticClass(), Path, OwnerId));
	}

	GLYPHBOUND_API float Stat(const AGBEnemyCharacter* Enemy, const TCHAR* Key, float Default);
	GLYPHBOUND_API FString Str(const FGBEnemyDefinition& Def, FName Key, const TCHAR* Default = TEXT(""));

	/** Loads the shot effects: the systems named in "Strings", or the given default paths. */
	GLYPHBOUND_API void LoadShotFx(FGBEnemyShotFx& OutFx, const FGBEnemyDefinition& Def, FName OwnerId, const TCHAR* DefaultMuzzle, const TCHAR* DefaultTracer, const TCHAR* DefaultImpact);

	GLYPHBOUND_API UNiagaraComponent* SpawnFx(const UObject* WorldContext, UNiagaraSystem* System, const FVector& Location, const FRotator& Rotation);

	/** A point on the target to aim at (a bit above its centre). */
	GLYPHBOUND_API FVector TargetChest(const AActor* Target);

	/**
	 * One hitscan shot: a ray from Muzzle along Direction (other enemies and the shooter are ignored), damage through
	 * UGBCombatLibrary::DealDamage when it hits something with attributes, a world impact effect otherwise, and the muzzle
	 * flash and the tracer. Info is the finished damage (PerformAttack's fields).
	 */
	GLYPHBOUND_API FGBEnemyShotResult FireHitscan(AGBEnemyCharacter* Shooter, const FVector& Muzzle, const FVector& Direction, float Range,
		const FGBDamageInfo& Info, const FGBEnemyShotFx& Fx, const AActor* Target);

	/** Smoke counters: how many hitscan shots enemies fired, and how many reached their target. Reset by the smoke steps. */
	GLYPHBOUND_API int32 GetShotsFired();
	GLYPHBOUND_API int32 GetShotsOnTarget();
	GLYPHBOUND_API void ResetShotCounters();

	/** Parses "x y z pitch yaw roll scale" (missing numbers default to identity). */
	GLYPHBOUND_API FTransform ParseOffset(const FString& Text);
}
