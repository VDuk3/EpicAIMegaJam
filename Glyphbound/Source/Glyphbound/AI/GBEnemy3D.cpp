#include "AI/GBEnemy3D.h"

#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

#include "AI/GBEnemyCharacter.h"
#include "Combat/GBCombatLibrary.h"
#include "Stats/GBAttributeComponent.h"
#include "Stats/GBStatsComponent.h"

namespace GBEnemy3DLocal
{
	int32 ShotsFired = 0;
	int32 ShotsOnTarget = 0;
	TSet<FString> LoggedMissing;
	constexpr float ChestHeightFraction = 0.25f;   // of the target capsule's half height, above its centre
	constexpr int32 MaxIgnoredAllies = 8;
}

FString GBEnemy3D::NormalizeAssetPath(const FString& Path)
{
	if (Path.IsEmpty() || Path.Contains(TEXT(".")))
	{
		return Path;
	}
	return Path + TEXT(".") + FPackageName::GetShortName(Path);
}

UObject* GBEnemy3D::LoadAssetQuiet(UClass* Class, const FString& Path, FName OwnerId)
{
	if (Path.IsEmpty())
	{
		return nullptr;
	}
	UObject* Asset = StaticLoadObject(Class, nullptr, *NormalizeAssetPath(Path), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!Asset && !GBEnemy3DLocal::LoggedMissing.Contains(Path))
	{
		GBEnemy3DLocal::LoggedMissing.Add(Path);
		// Display, not Warning: a missing asset is expected until the art is imported, and the smoke run fails on warnings.
		UE_LOG(LogGB, Display, TEXT("ENEMIES3D asset missing for %s, using the fallback: %s"), *OwnerId.ToString(), *Path);
	}
	return Asset;
}

float GBEnemy3D::Stat(const AGBEnemyCharacter* Enemy, const TCHAR* Key, float Default)
{
	const UGBStatsComponent* Stats = Enemy ? Enemy->GetStats() : nullptr;
	return Stats ? Stats->GetStat(FName(Key), Default) : Default;
}

FString GBEnemy3D::Str(const FGBEnemyDefinition& Def, FName Key, const TCHAR* Default)
{
	const FString* Found = Def.Strings.Find(Key);
	return (Found && !Found->IsEmpty()) ? *Found : FString(Default);
}

void GBEnemy3D::LoadShotFx(FGBEnemyShotFx& OutFx, const FGBEnemyDefinition& Def, FName OwnerId, const TCHAR* DefaultMuzzle, const TCHAR* DefaultTracer, const TCHAR* DefaultImpact)
{
	OutFx.Muzzle = LoadAsset<UNiagaraSystem>(Str(Def, GBEnemy3DKey::FxMuzzle, DefaultMuzzle), OwnerId);
	OutFx.Tracer = LoadAsset<UNiagaraSystem>(Str(Def, GBEnemy3DKey::FxTracer, DefaultTracer), OwnerId);
	OutFx.Impact = LoadAsset<UNiagaraSystem>(Str(Def, GBEnemy3DKey::FxImpact, DefaultImpact), OwnerId);
	OutFx.HitImpact = LoadAsset<UNiagaraSystem>(Str(Def, GBEnemy3DKey::FxHitImpact), OwnerId);
	const FString EndParam = Str(Def, GBEnemy3DKey::FxTracerEndParam);
	const FString LengthParam = Str(Def, GBEnemy3DKey::FxTracerLengthParam);
	OutFx.TracerEndParam = EndParam.IsEmpty() ? NAME_None : FName(*EndParam);
	OutFx.TracerLengthParam = LengthParam.IsEmpty() ? NAME_None : FName(*LengthParam);
}

UNiagaraComponent* GBEnemy3D::SpawnFx(const UObject* WorldContext, UNiagaraSystem* System, const FVector& Location, const FRotator& Rotation)
{
	if (!System || !WorldContext)
	{
		return nullptr;
	}
	return UNiagaraFunctionLibrary::SpawnSystemAtLocation(WorldContext, System, Location, Rotation, FVector::OneVector, true, true);
}

FVector GBEnemy3D::TargetChest(const AActor* Target)
{
	if (!Target)
	{
		return FVector::ZeroVector;
	}
	float HalfHeight = 0.f;
	if (const ACharacter* Character = Cast<ACharacter>(Target))
	{
		HalfHeight = Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	}
	return Target->GetActorLocation() + FVector::UpVector * HalfHeight * GBEnemy3DLocal::ChestHeightFraction;
}

FGBEnemyShotResult GBEnemy3D::FireHitscan(AGBEnemyCharacter* Shooter, const FVector& Muzzle, const FVector& Direction, float Range,
	const FGBDamageInfo& Info, const FGBEnemyShotFx& Fx, const AActor* Target)
{
	FGBEnemyShotResult Result;
	UWorld* World = Shooter ? Shooter->GetWorld() : nullptr;
	const FVector Dir = Direction.GetSafeNormal();
	if (!World || Dir.IsNearlyZero())
	{
		return Result;
	}
	++GBEnemy3DLocal::ShotsFired;

	const FVector End = Muzzle + Dir * FMath::Max(Range, 1.f);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GBEnemyHitscan), false, Shooter);
	FHitResult Hit;
	bool bHit = false;
	// Other enemies do not stop a shot (friendly fire is off anyway): the ray goes through them.
	for (int32 Attempt = 0; Attempt < GBEnemy3DLocal::MaxIgnoredAllies; ++Attempt)
	{
		bHit = World->LineTraceSingleByChannel(Hit, Muzzle, End, ECC_Visibility, Params);
		AGBEnemyCharacter* Ally = bHit ? Cast<AGBEnemyCharacter>(Hit.GetActor()) : nullptr;
		if (!Ally)
		{
			break;
		}
		Params.AddIgnoredActor(Ally);
	}

	Result.End = bHit ? FVector(Hit.ImpactPoint) : End;
	UGBCombatLibrary::OnShotTraced().Broadcast(Muzzle, Result.End, 0.f);
	AActor* HitActor = bHit ? Hit.GetActor() : nullptr;

	// The player's capsule may not block the Visibility channel (the line of sight test passes through it), so the
	// target is also tested against the ray directly: a hit if the ray passes within its capsule before the world hit.
	if (Target && HitActor != Target)
	{
		if (const ACharacter* TargetChar = Cast<ACharacter>(Target))
		{
			const UCapsuleComponent* Capsule = TargetChar->GetCapsuleComponent();
			const float HalfSegment = FMath::Max(Capsule->GetScaledCapsuleHalfHeight() - Capsule->GetScaledCapsuleRadius(), 0.f);
			const FVector Centre = Capsule->GetComponentLocation();
			FVector OnRay, OnAxis;
			FMath::SegmentDistToSegmentSafe(Muzzle, Result.End, Centre - FVector::UpVector * HalfSegment, Centre + FVector::UpVector * HalfSegment, OnRay, OnAxis);
			if (FVector::Dist(OnRay, OnAxis) <= Capsule->GetScaledCapsuleRadius())
			{
				Hit = FHitResult();
				Hit.ImpactPoint = OnRay;
				Hit.ImpactNormal = (Muzzle - OnRay).GetSafeNormal();
				HitActor = const_cast<AActor*>(Target);
				bHit = true;
				Result.End = OnRay;
			}
		}
	}
	if (HitActor && UGBAttributeComponent::Find(HitActor))
	{
		FGBDamageInfo Shot = Info;
		Shot.HitLocation = Hit.ImpactPoint;
		Shot.HitDirection = Dir;
		Result.DamageDealt = UGBCombatLibrary::DealDamage(HitActor, Shot);
		Result.bHitTarget = HitActor == Target;
		if (Result.bHitTarget)
		{
			++GBEnemy3DLocal::ShotsOnTarget;
		}
	}
	else if (bHit)
	{
		Result.bHitWorld = true;
		SpawnFx(World, Fx.Impact, Hit.ImpactPoint, Hit.ImpactNormal.Rotation());
	}

	SpawnFx(World, Fx.Muzzle, Muzzle, Dir.Rotation());
	if (UNiagaraComponent* Tracer = SpawnFx(World, Fx.Tracer, Muzzle, Dir.Rotation()))
	{
		// The user parameter names of the tracer systems are filled in Enemies.json once the assets are known.
		if (!Fx.TracerEndParam.IsNone())
		{
			Tracer->SetVariableVec3(Fx.TracerEndParam, Result.End);
		}
		if (!Fx.TracerLengthParam.IsNone())
		{
			Tracer->SetVariableFloat(Fx.TracerLengthParam, FVector::Dist(Muzzle, Result.End));
		}
	}
	return Result;
}

int32 GBEnemy3D::GetShotsFired() { return GBEnemy3DLocal::ShotsFired; }
int32 GBEnemy3D::GetShotsOnTarget() { return GBEnemy3DLocal::ShotsOnTarget; }
void GBEnemy3D::ResetShotCounters()
{
	GBEnemy3DLocal::ShotsFired = 0;
	GBEnemy3DLocal::ShotsOnTarget = 0;
}

FTransform GBEnemy3D::ParseOffset(const FString& Text)
{
	TArray<FString> Parts;
	Text.ParseIntoArrayWS(Parts);
	auto Number = [&Parts](int32 Index, float Default) { return Parts.IsValidIndex(Index) ? FCString::Atof(*Parts[Index]) : Default; };
	return FTransform(FRotator(Number(3, 0.f), Number(4, 0.f), Number(5, 0.f)), FVector(Number(0, 0.f), Number(1, 0.f), Number(2, 0.f)), FVector(Number(6, 1.f)));
}
