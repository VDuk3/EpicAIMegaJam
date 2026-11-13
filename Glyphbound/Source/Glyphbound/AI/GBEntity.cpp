#include "AI/GBEntity.h"

#include "Components/PoseableMeshComponent.h"
#include "Components/BillboardComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

#include "AI/GBTentacleRigComponent.h"
#include "Combat/GBCombatLibrary.h"
#include "Combat/GBProjectile.h"
#include "Core/GBPlaceholder.h"
#include "Core/GBStatNames.h"
#include "Data/GBDataSubsystem.h"
#include "Stats/GBAttributeComponent.h"
#include "Stats/GBStatsComponent.h"

// Named (not anonymous) so unity builds cannot collide with another file's helpers.
namespace GBEntityLocal
{
	// Visual frame (export_report.json, SourceArt/Enemies/Entity): origin at the shell centre,
	// eye looking to +X, up +Z, 1 unit = 1 cm. Provisional, from the art drop.
	const FVector EyeCentreCm(66.4f, 0.1f, -13.5f);
	constexpr float EyeRadiusCm = 27.f;
	constexpr float ShellTopCm = 83.f;          // visual-frame Z of the top of the shell
	constexpr float TentacleTipDepthCm = 224.f; // how far the tentacles hang below the origin
	constexpr float TipClearanceCm = 20.f;      // floor clearance of the lowest tip

	const TCHAR* BodyPath = TEXT("/Game/Glyphbound/Enemies/Entity/SM_Entity_Body.SM_Entity_Body");
	const TCHAR* EyePath = TEXT("/Game/Glyphbound/Enemies/Entity/SM_Entity_Eye.SM_Entity_Eye");
	const TCHAR* TentaclesPath = TEXT("/Game/Glyphbound/Enemies/Entity/SK_Entity_Tentacles.SK_Entity_Tentacles");
	const TCHAR* TentaclesMaterialPath = TEXT("/Game/Glyphbound/Enemies/Entity/MI_EntityTentacles.MI_EntityTentacles");

	const FName FlashParam(TEXT("Flash"));
	const FName SwayParam(TEXT("SwayAmount"));

	// Weak-point flash: full for an instant, then a linear fade over this long. Visual timing.
	constexpr float FlashFadeTime = 0.3f;
	// Hover bob of the visual (the capsule stays on the ground): slow and small, purely visual.
	constexpr float BobFrequencyHz = 0.4f;
	constexpr float BobAmplitudeCm = 4.f;
	// Whip-back ripple after the shot, as a share of the tuning ripple strength.
	constexpr float ShotRippleShare = 0.5f;
	// Smallest ripple on any hit, as a share of max health, so chip damage still shivers the hologram.
	constexpr float MinRippleShare = 0.05f;
}

AGBEntity::AGBEntity()
{
	VisualRoot = CreateDefaultSubobject<USceneComponent>(TEXT("EntityVisual"));
	VisualRoot->SetupAttachment(GetCapsuleComponent());

	EntityBody = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EntityBody"));
	EntityBody->SetupAttachment(VisualRoot);

	EntityEye = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EntityEye"));
	EntityEye->SetupAttachment(VisualRoot);

	EntityTentacles = CreateDefaultSubobject<UPoseableMeshComponent>(TEXT("EntityTentacles"));
	EntityTentacles->SetupAttachment(VisualRoot);

	TentacleRig = CreateDefaultSubobject<UGBTentacleRigComponent>(TEXT("EntityTentacleRig"));

	for (UStaticMeshComponent* Part : { EntityBody.Get(), EntityEye.Get() })
	{
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		Part->SetCanEverAffectNavigation(false);
		Part->SetGenerateOverlapEvents(false);
	}
	EntityTentacles->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	EntityTentacles->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	EntityTentacles->SetCanEverAffectNavigation(false);
	EntityTentacles->SetGenerateOverlapEvents(false);
}

void AGBEntity::BeginPlay()
{
	Super::BeginPlay();
}

void AGBEntity::InitFromDefinition(const FGBEnemyDefinition& Definition)
{
	Super::InitFromDefinition(Definition);
	BuildVisual();
}

void AGBEntity::BuildVisual()
{
	if (bVisualBuilt)
	{
		return;
	}
	bVisualBuilt = true;

	UCapsuleComponent* Capsule = GetCapsuleComponent();
	const float HalfHeight = Capsule ? Capsule->GetUnscaledCapsuleHalfHeight() : 135.f;
	VisualBaseZ = GBEntityLocal::TentacleTipDepthCm + GBEntityLocal::TipClearanceCm - HalfHeight;
	VisualRoot->SetRelativeLocation(FVector(0.f, 0.f, VisualBaseZ));

	UStaticMesh* BodySM = LoadObject<UStaticMesh>(nullptr, GBEntityLocal::BodyPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	UStaticMesh* EyeSM = LoadObject<UStaticMesh>(nullptr, GBEntityLocal::EyePath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	USkeletalMesh* TentacleSK = LoadObject<USkeletalMesh>(nullptr, GBEntityLocal::TentaclesPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	UMaterialInterface* TentacleMI = LoadObject<UMaterialInterface>(nullptr, GBEntityLocal::TentaclesMaterialPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!BodySM || !EyeSM || !TentacleSK || !TentacleMI)
	{
		UE_LOG(LogGB, Warning, TEXT("Entity art missing (body=%d eye=%d tentacles=%d material=%d), keeping the placeholder look"),
			BodySM ? 1 : 0, EyeSM ? 1 : 0, TentacleSK ? 1 : 0, TentacleMI ? 1 : 0);
		return;
	}

	EntityBody->SetStaticMesh(BodySM);
	EntityEye->SetStaticMesh(EyeSM);
	EntityTentacles->SetSkinnedAssetAndUpdate(TentacleSK, true);
	// The skeletal mesh ships without a material on its slot: use the authored hologram instance.
	for (int32 Slot = 0; Slot < EntityTentacles->GetNumMaterials(); ++Slot)
	{
		if (EntityTentacles->GetMaterial(Slot) == nullptr)
		{
			EntityTentacles->SetMaterial(Slot, TentacleMI);
		}
	}

	// Dynamic instances: Flash starts at 0, GlowColor stays as authored. SwayAmount is 0:
	// the bones now do the motion, the material sway would fight them.
	for (int32 Slot = 0; Slot < EntityBody->GetNumMaterials(); ++Slot)
	{
		if (UMaterialInstanceDynamic* MID = EntityBody->CreateDynamicMaterialInstance(Slot))
		{
			MID->SetScalarParameterValue(GBEntityLocal::FlashParam, 0.f);
			BodyMIDs.Add(MID);
		}
	}
	for (int32 Slot = 0; Slot < EntityEye->GetNumMaterials(); ++Slot)
	{
		if (UMaterialInstanceDynamic* MID = EntityEye->CreateDynamicMaterialInstance(Slot))
		{
			MID->SetScalarParameterValue(GBEntityLocal::FlashParam, 0.f);
			EyeMIDs.Add(MID);
		}
	}
	for (int32 Slot = 0; Slot < EntityTentacles->GetNumMaterials(); ++Slot)
	{
		if (UMaterialInstanceDynamic* MID = EntityTentacles->CreateDynamicMaterialInstance(Slot))
		{
			MID->SetScalarParameterValue(GBEntityLocal::SwayParam, 0.f);
			if (!TentacleMID)
			{
				TentacleMID = MID;
			}
		}
	}

	// The real look is up: hide the base class placeholder.
	Sprite->SetVisibility(false);
	BodyMesh->SetVisibility(false);

	TentacleRig->Setup(EntityTentacles);
}

FVector AGBEntity::GetEyeCentreWorld() const
{
	if (VisualRoot)
	{
		return VisualRoot->GetComponentTransform().TransformPosition(GBEntityLocal::EyeCentreCm);
	}
	return GetActorLocation();
}

FVector AGBEntity::GetShellTopWorld() const
{
	if (VisualRoot)
	{
		return VisualRoot->GetComponentTransform().TransformPosition(FVector(0.f, 0.f, GBEntityLocal::ShellTopCm));
	}
	return GetActorLocation();
}

bool AGBEntity::IsHeadshotLocation(const FVector& HitLocation) const
{
	return FVector::Dist(HitLocation, GetEyeCentreWorld()) <= GBEntityLocal::EyeRadiusCm;
}

FVector AGBEntity::GetAttackOrigin(AActor* Goal) const
{
	if (TentacleRig && TentacleRig->IsReady() && Goal)
	{
		const FVector GoalPos = Goal->GetActorLocation();
		int32 Best = INDEX_NONE;
		float BestDist = FLT_MAX;
		for (int32 Chain = 0; Chain < TentacleRig->GetChainCount(); ++Chain)
		{
			const float Dist = FVector::Dist(TentacleRig->GetChainTipWorldPosition(Chain), GoalPos);
			if (Dist < BestDist)
			{
				BestDist = Dist;
				Best = Chain;
			}
		}
		if (Best != INDEX_NONE)
		{
			return TentacleRig->GetChainTipWorldPosition(Best);
		}
	}
	return GetChestLocation();
}

void AGBEntity::PerformAttack()
{
	AActor* Goal = FindTarget();
	const FGBEnemyDefinition* Def = GetDefinition();
	if (!Goal || !Def || !GetWorld())
	{
		return;
	}
	if (bAttackSuppressed)
	{
		return;
	}
	if (!IsRanged())
	{
		Super::PerformAttack();
		return;
	}

	// Same shot as the base ranged attack, but from the tentacle tip closest to the target.
	FGBDamageInfo Info;
	Info.Amount = Stats->GetStat(GBStat::AttackDamage, 0.f);
	Info.Element = Def->Element;
	Info.Instigator = this;
	Info.Causer = this;
	Info.SourceId = EnemyId;
	Info.OnHitEffect = FName(*Def->Strings.FindRef(GBString::OnHitEffect));

	FGBProjectileConfig Config;
	Config.Damage = Info;
	Config.Damage.Causer = nullptr;
	Config.Speed = Stats->GetStat(GBStat::ProjectileSpeed, Config.Speed);
	Config.Radius = Stats->GetStat(GBStat::ProjectileRadius, Config.Radius);
	Config.GravityScale = Stats->GetStat(GBStat::ProjectileGravity, 0.f);
	Config.MaxRange = Stats->GetStat(GBStat::AggroRange, 0.f) * GetGameValue(GBEnemyKey::ProjectileRangeMult, 1.5f);
	Config.MaxLifetime = GetGameValue(GBProjectileKey::ProjectileMaxLifetime, Config.MaxLifetime);
	Config.bPixelArt = false;
	Config.Color = UGBDataSubsystem::ParseColor(Def->Strings.FindRef(GBEnemyKey::ProjectileColor),
		GBPlaceholder::ElementColor(static_cast<uint8>(Def->Element)));

	const FVector Origin = GetAttackOrigin(Goal);
	FVector Direction = Goal->GetActorLocation() - Origin;
	if (Direction.IsNearlyZero())
	{
		Direction = GetActorForwardVector();
	}
	AGBProjectile::SpawnProjectile(GetWorld(), Config, Origin, Direction, this);

	// The tentacle whips back: release the reach and ripple it.
	if (TentacleRig)
	{
		TentacleRig->ClearReach();
		const float Strength = GetGameValue(GBEntityTentacle::RippleStrength, GBEntityTentacle::RippleStrengthDefault);
		TentacleRig->AddRipple(Strength * GBEntityLocal::ShotRippleShare);
	}
}

void AGBEntity::OnDamagedVisual(const FGBDamageInfo& Info, float ActualDamage)
{
	if (Info.bHeadshot || IsHeadshotLocation(Info.HitLocation))
	{
		FlashValue = 1.f;
	}
	if (TentacleRig && TentacleRig->IsReady())
	{
		const float MaxHealth = Attributes ? Attributes->GetMaxHealth() : 0.f;
		const float Share = MaxHealth > 0.f
			? FMath::Clamp(ActualDamage / MaxHealth, GBEntityLocal::MinRippleShare, 1.f)
			: GBEntityLocal::MinRippleShare;
		const float Strength = GetGameValue(GBEntityTentacle::RippleStrength, GBEntityTentacle::RippleStrengthDefault);
		TentacleRig->AddRipple(Strength * Share);
	}
}

void AGBEntity::HandleDeath(const FGBDamageInfo& KillingBlow)
{
	Super::HandleDeath(KillingBlow);
	if (TentacleRig)
	{
		TentacleRig->ClearReach();
		TentacleRig->SetLimp(true);
	}
}

FString AGBEntity::DescribeMaterials(UMeshComponent* InMesh) const
{
	FString Out;
	if (!InMesh)
	{
		return Out;
	}
	for (int32 Slot = 0; Slot < InMesh->GetNumMaterials(); ++Slot)
	{
		if (Slot > 0)
		{
			Out += TEXT("|");
		}
		if (UMaterialInterface* Material = InMesh->GetMaterial(Slot))
		{
			Out += Material->GetName();
		}
		else
		{
			Out += TEXT("null");
		}
	}
	return Out;
}

FString AGBEntity::DescribeVisual() const
{
	const FString BodyState = EntityBody
		? FString::Printf(TEXT("mesh=%s vis=%d"), EntityBody->GetStaticMesh() ? *EntityBody->GetStaticMesh()->GetName() : TEXT("null"), EntityBody->IsVisible() ? 1 : 0)
		: TEXT("null");
	const FString EyeState = EntityEye
		? FString::Printf(TEXT("mesh=%s vis=%d"), EntityEye->GetStaticMesh() ? *EntityEye->GetStaticMesh()->GetName() : TEXT("null"), EntityEye->IsVisible() ? 1 : 0)
		: TEXT("null");
	const FString TentState = EntityTentacles
		? FString::Printf(TEXT("mesh=%s vis=%d slots=%d mats=[%s] bounds=%s"), EntityTentacles->GetSkinnedAsset() ? *EntityTentacles->GetSkinnedAsset()->GetName() : TEXT("null"),
			EntityTentacles->IsVisible() ? 1 : 0, EntityTentacles->GetNumMaterials(), *DescribeMaterials(EntityTentacles),
			*EntityTentacles->Bounds.BoxExtent.ToCompactString())
		: TEXT("null");
	const FString RootState = VisualRoot
		? FString::Printf(TEXT("rel=%s"), *VisualRoot->GetRelativeLocation().ToCompactString())
		: TEXT("null");
	const FString RigState = TentacleRig && TentacleRig->IsReady()
		? FString::Printf(TEXT("chains=%d tip0=%s"), TentacleRig->GetChainCount(), *TentacleRig->GetChainTipWorldPosition(0).ToCompactString())
		: TEXT("notready");
	return FString::Printf(TEXT("built=%d loc=%s %s body=[%s] eye=[%s] tent=[%s] rig=[%s]"),
		bVisualBuilt ? 1 : 0, *GetActorLocation().ToCompactString(), *RootState, *BodyState, *EyeState, *TentState, *RigState);
}

void AGBEntity::ApplyFlash()
{
	for (UMaterialInstanceDynamic* MID : BodyMIDs)
	{
		if (MID)
		{
			MID->SetScalarParameterValue(GBEntityLocal::FlashParam, FlashValue);
		}
	}
	for (UMaterialInstanceDynamic* MID : EyeMIDs)
	{
		if (MID)
		{
			MID->SetScalarParameterValue(GBEntityLocal::FlashParam, FlashValue);
		}
	}
}

void AGBEntity::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (FlashValue > 0.f)
	{
		FlashValue = FMath::Max(0.f, FlashValue - DeltaSeconds / GBEntityLocal::FlashFadeTime);
		ApplyFlash();
	}

	if (VisualRoot && bVisualBuilt)
	{
		// Hover bob: the visual floats, the capsule walks like every other enemy.
		BobTime += DeltaSeconds;
		VisualRoot->SetRelativeLocation(FVector(0.f, 0.f,
			VisualBaseZ + FMath::Sin(BobTime * 2.f * PI * GBEntityLocal::BobFrequencyHz) * GBEntityLocal::BobAmplitudeCm));
	}

	if (bAttackSuppressed)
	{
		StopMoving();
		return;
	}

	// During the windup the nearest tentacle reaches towards the player.
	if (GetState() == EGBEnemyState::Windup && TentacleRig && TentacleRig->IsReady())
	{
		if (AActor* Goal = FindTarget())
		{
			TentacleRig->SetReachTarget(Goal->GetActorLocation(), 1.f);
		}
	}
}
