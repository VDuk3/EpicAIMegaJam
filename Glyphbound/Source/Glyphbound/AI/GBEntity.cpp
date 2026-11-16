#include "AI/GBEntity.h"

#include "Components/PoseableMeshComponent.h"
#include "Components/BillboardComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "CollisionQueryParams.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
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
#include "World/GBEraComponent.h"

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
	const FName FlashColorParam(TEXT("FlashColor"));

	// Weak-point flash: full for an instant, then a linear fade over this long. Visual timing.
	constexpr float FlashFadeTime = 0.3f;
	// Whip-back ripple after the shot, as a share of the tuning ripple strength.
	constexpr float ShotRippleShare = 0.5f;
	// Smallest ripple on any hit, as a share of max health, so chip damage still shivers the hologram.
	constexpr float MinRippleShare = 0.05f;

	// Longest spring sub-step, s (a long frame cannot blow a spring up).
	constexpr float MaxSpringStep = 1.f / 60.f;
	// How fast the windup flag eases in and out (1/s); it drives the rise and the eye flare.
	constexpr float WindupAlphaRate = 6.f;
	// On death the shell tips forward and rolls as it drops to this height (cm above the floor).
	constexpr float DeathPitchKick = -140.f;
	constexpr float DeathRollKick = 70.f;
	constexpr float DeathAltitudeCm = 150.f;
	constexpr float DeathAltOmega = 2.f;
	// The eye light sits this far in front of the lens, cm.
	// The eye spot sits on the lens surface (visual frame X of the front of the eye mesh), cm.
	constexpr float EyeLensFrontCm = 92.f;
	constexpr int32 MaxTentacleLights = 6;
	// Where on a chain a tentacle light rides, as a joint index share.
	constexpr float TentacleLightAlong = 0.5f;
	// Shell centre must stay at least this many shell radii above the floor, even under a low ceiling.
	constexpr float MinShellFloorShare = 1.1f;
}

void FGBEntitySpring::Step(float Target, float Dt, float Omega, float Zeta)
{
	float Remaining = FMath::Max(Dt, 0.f);
	while (Remaining > KINDA_SMALL_NUMBER)
	{
		const float H = FMath::Min(Remaining, GBEntityLocal::MaxSpringStep);
		V += (Omega * Omega * (Target - X) - 2.f * Zeta * Omega * V) * H;
		X += V * H;
		Remaining -= H;
	}
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

	// The shell is what weapons hit: guns trace the camera channel, projectiles overlap pawns.
	ShellHit = CreateDefaultSubobject<USphereComponent>(TEXT("EntityShellHit"));
	ShellHit->SetupAttachment(VisualRoot);
	ShellHit->InitSphereRadius(FGBEntityTuning().ShellRadius);
	// Off until the art is validated (BuildVisual): with the placeholder look the capsule stays the collision.
	ShellHit->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ShellHit->SetCollisionObjectType(ECC_Pawn);
	ShellHit->SetCollisionResponseToAllChannels(ECR_Ignore);
	ShellHit->SetCollisionResponseToChannel(ECC_Camera, ECR_Block);
	ShellHit->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	ShellHit->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	ShellHit->SetGenerateOverlapEvents(true);
	ShellHit->SetCanEverAffectNavigation(false);

	EyeLight = CreateDefaultSubobject<USpotLightComponent>(TEXT("EntityEyeLight"));
	EyeLight->SetupAttachment(EntityEye);
	EyeLight->SetMobility(EComponentMobility::Movable);
	EyeLight->CastShadows = false;
	EyeLight->IntensityUnits = ELightUnits::Candelas;
	EyeLight->Intensity = 0.f;

	MuzzleLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("EntityMuzzleLight"));
	MuzzleLight->SetupAttachment(VisualRoot);
	MuzzleLight->SetMobility(EComponentMobility::Movable);
	MuzzleLight->CastShadows = false;
	MuzzleLight->IntensityUnits = ELightUnits::Candelas;
	MuzzleLight->Intensity = 0.f;
	MuzzleLight->SetVisibility(false);

	for (int32 Index = 0; Index < GBEntityLocal::MaxTentacleLights; ++Index)
	{
		UPointLightComponent* Light = CreateDefaultSubobject<UPointLightComponent>(*FString::Printf(TEXT("EntityTentacleLight%d"), Index));
		Light->SetupAttachment(VisualRoot);
		Light->SetMobility(EComponentMobility::Movable);
		Light->CastShadows = false;
		Light->IntensityUnits = ELightUnits::Candelas;
		Light->Intensity = 0.f;
		Light->SetVisibility(false);
		TentacleLights.Add(Light);
	}

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

	// The turn is ours (rate-limited in Tick), not the movement component's.
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->bOrientRotationToMovement = false;
	}
}

// The tentacle boss (owner, 2026-11-15: the machine at the end of the base's deep wing). Opt-in by the stat TentacleBoss = 1:
// it alternates hunting the player (TB_ChaseTime s, closing to TB_ChaseKeep cm) with keeping away (TB_RangeTime s, out to
// TB_RangeKeep cm); its attack is a tentacle lash when the player is within TB_WhipReach of the shell (TB_WhipDamage), else a
// fan of TB_FanCount bolts over TB_FanDeg from several tentacle tips (chance TB_FanChance, cooldown TB_FanCooldown), else one
// bolt. File-local state: no header change.
namespace GBTentacleBoss
{
	struct FState
	{
		bool bChasing = true;
		float ModeLeft = 0.f;
		float FanCooldown = 0.f;
		// the eye laser (owner): a solid glowing rod straight out of the eye to the first thing it meets, and a narrow
		// spot light along it that lights the hall's smoke
		TWeakObjectPtr<UStaticMeshComponent> Beam;
		TWeakObjectPtr<USpotLightComponent> BeamLight;
	};
	static TMap<TWeakObjectPtr<const AGBEntity>, FState> States;
}

void AGBEntity::BeginPlay()
{
	Super::BeginPlay();
	StrafeSign = FMath::RandBool() ? 1.f : -1.f;
	StrafeTimer = 1.f;
}

void AGBEntity::ReadTuning()
{
	FGBEntityTuning& T = Tuning;
	auto V = [this](const TCHAR* Name, float Default) { return GetGameValue(FName(Name), Default); };
	T.TurnMaxRate = V(TEXT("EntityTurnMaxRate"), 90.f);
	T.TurnGain = V(TEXT("EntityTurnGain"), 3.f);
	T.TurnAccel = V(TEXT("EntityTurnAccel"), 5.f);
	T.PitchMax = V(TEXT("EntityPitchMax"), 30.f);
	T.AimOmega = V(TEXT("EntityAimOmega"), 6.f);
	T.EyeTrackMax = V(TEXT("EntityEyeTrackMax"), 12.f);
	T.EyeTrackSpeed = V(TEXT("EntityEyeTrackSpeed"), 14.f);
	T.AltMin = V(TEXT("EntityAltMin"), 250.f);
	T.AltMax = FMath::Max(T.AltMin, V(TEXT("EntityAltMax"), 400.f));
	T.AltDriftAmp = V(TEXT("EntityAltDriftAmp"), 70.f);
	T.AltDriftHz = V(TEXT("EntityAltDriftHz"), 0.22f);
	T.AltRise = V(TEXT("EntityAltRise"), 60.f);
	T.AltDip = V(TEXT("EntityAltDip"), 45.f);
	T.AltDipTime = FMath::Max(V(TEXT("EntityAltDipTime"), 0.7f), 0.05f);
	T.AltOmega = V(TEXT("EntityAltOmega"), 3.f);
	T.BankMax = V(TEXT("EntityBankMax"), 20.f);
	T.BankPerYawRate = V(TEXT("EntityBankPerYawRate"), 0.07f);
	T.LeanDeg = V(TEXT("EntityLeanDeg"), 10.f);
	T.AccelPitchDeg = V(TEXT("EntityAccelPitchDeg"), 8.f);
	T.TiltOmega = V(TEXT("EntityTiltOmega"), 7.f);
	T.TiltZeta = V(TEXT("EntityTiltZeta"), 0.75f);
	T.Acceleration = V(TEXT("EntityAcceleration"), 700.f);
	T.Braking = V(TEXT("EntityBraking"), 600.f);
	T.GroundFriction = V(TEXT("EntityGroundFriction"), 2.f);
	T.StrafeSpeed = V(TEXT("EntityStrafeSpeed"), 0.7f);
	T.StrafeFlipMin = V(TEXT("EntityStrafeFlipMin"), 1.8f);
	T.StrafeFlipMax = FMath::Max(T.StrafeFlipMin, V(TEXT("EntityStrafeFlipMax"), 4.f));
	T.StrafeKeep = V(TEXT("EntityStrafeKeep"), 0.5f);
	T.StrafeProbe = V(TEXT("EntityStrafeProbe"), 220.f);
	T.HitTiltKick = V(TEXT("EntityHitTiltKick"), 90.f);
	T.HitPushKick = V(TEXT("EntityHitPushKick"), 220.f);
	T.HitRefDamage = FMath::Max(V(TEXT("EntityHitRefDamage"), 100.f), 1.f);
	T.HitMinShare = V(TEXT("EntityHitMinShare"), 0.15f);
	T.HitWeakMult = V(TEXT("EntityHitWeakMult"), 2.f);
	T.HitOmega = V(TEXT("EntityHitOmega"), 8.f);
	T.HitZeta = V(TEXT("EntityHitZeta"), 0.3f);
	T.ShellRadius = V(TEXT("EntityShellRadius"), 88.f);
	T.EyeWeakRadius = V(TEXT("EntityEyeWeakRadius"), 40.f);
	T.EyeLightIntensity = V(TEXT("EntityEyeLightIntensity"), 3000.f);
	T.EyeLightRadius = V(TEXT("EntityEyeLightRadius"), 1600.f);
	T.EyeLightPulseHz = V(TEXT("EntityEyeLightPulseHz"), 0.7f);
	T.EyeLightPulse = V(TEXT("EntityEyeLightPulse"), 0.15f);
	T.EyeLightWindupFlare = V(TEXT("EntityEyeLightWindupFlare"), 0.8f);
	T.EyeLightHitFlare = V(TEXT("EntityEyeLightHitFlare"), 2.f);
	T.EyeLightFade = FMath::Max(V(TEXT("EntityEyeLightFade"), 0.8f), 0.05f);
	T.BoltLength = V(TEXT("EntityBoltLength"), 80.f);
	T.BoltThickness = V(TEXT("EntityBoltThickness"), 12.f);
	T.BoltTrail = V(TEXT("EntityBoltTrail"), 240.f);
	T.BoltLightIntensity = V(TEXT("EntityBoltLightIntensity"), 4000.f);
	T.BoltLightRadius = V(TEXT("EntityBoltLightRadius"), 700.f);
	T.Scale = FMath::Max(V(TEXT("EntityScale"), 2.5f), 0.1f);
	T.EyeLightInner = V(TEXT("EntityEyeLightInner"), 10.f);
	T.EyeLightOuter = FMath::Max(T.EyeLightInner + 1.f, V(TEXT("EntityEyeLightOuter"), 28.f));
	T.TentacleLightCount = V(TEXT("EntityTentacleLightCount"), 4.f);
	T.TentacleLightIntensity = V(TEXT("EntityTentacleLightIntensity"), 90.f);
	T.TentacleLightRadius = V(TEXT("EntityTentacleLightRadius"), 600.f);
	T.TentacleFlashShot = V(TEXT("EntityTentacleFlashShot"), 1.f);
	T.TentacleFlashHit = V(TEXT("EntityTentacleFlashHit"), 0.6f);
	T.TentacleFlashFade = FMath::Max(V(TEXT("EntityTentacleFlashFade"), 0.3f), 0.05f);
	T.CeilingMargin = V(TEXT("EntityCeilingMargin"), 40.f);
	T.CeilingProbeInterval = FMath::Max(V(TEXT("EntityCeilingProbeInterval"), 0.15f), 0.02f);
	T.MuzzleIntensity = V(TEXT("EntityMuzzleIntensity"), 15000.f);
	T.MuzzleRadius = V(TEXT("EntityMuzzleRadius"), 700.f);
	T.MuzzleTime = FMath::Max(V(TEXT("EntityMuzzleTime"), 0.12f), 0.02f);
}

void AGBEntity::FaceTowards(const AActor* Goal)
{
	(void)Goal; // UpdateTurn turns the Entity every tick, with weight.
}

void AGBEntity::InitFromDefinition(const FGBEnemyDefinition& Definition)
{
	Super::InitFromDefinition(Definition);
	ReadTuning();
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->MaxAcceleration = Tuning.Acceleration;
		Move->BrakingDecelerationWalking = Tuning.Braking;
		Move->GroundFriction = Tuning.GroundFriction;
	}
	if (ShellHit)
	{
		ShellHit->SetSphereRadius(Tuning.ShellRadius);
	}
	EyeLightColor = UGBDataSubsystem::ParseColor(Definition.Strings.FindRef(GBEnemyKey::ProjectileColor), EyeLightColor);
	TentacleColor = EyeLightColor;
	BuildVisual();
}

void AGBEntity::BuildVisual()
{
	if (bVisualBuilt || bVisualFailed)
	{
		return;
	}

	// Validate the art first: nothing about the flight, the hit sphere or the capsule changes unless every part loads.
	UStaticMesh* BodySM = LoadObject<UStaticMesh>(nullptr, GBEntityLocal::BodyPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	UStaticMesh* EyeSM = LoadObject<UStaticMesh>(nullptr, GBEntityLocal::EyePath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	USkeletalMesh* TentacleSK = LoadObject<USkeletalMesh>(nullptr, GBEntityLocal::TentaclesPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	UMaterialInterface* TentacleMI = LoadObject<UMaterialInterface>(nullptr, GBEntityLocal::TentaclesMaterialPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!BodySM || !EyeSM || !TentacleSK || !TentacleMI)
	{
		bVisualFailed = true;
		UE_LOG(LogGB, Warning, TEXT("Entity art missing (body=%d eye=%d tentacles=%d material=%d), keeping the placeholder look"),
			BodySM ? 1 : 0, EyeSM ? 1 : 0, TentacleSK ? 1 : 0, TentacleMI ? 1 : 0);
		return;
	}

	UCapsuleComponent* Capsule = GetCapsuleComponent();
	const float HalfHeight = Capsule ? Capsule->GetUnscaledCapsuleHalfHeight() : 135.f;
	CapsuleHalfHeightCached = HalfHeight;
	const float Scale = Tuning.Scale;
	VisualRoot->SetRelativeScale3D(FVector(Scale));
	// First altitude: the lowest the tentacle tips allow. The flight takes over from here.
	VisualBaseZ = (GBEntityLocal::TentacleTipDepthCm + GBEntityLocal::TipClearanceCm) * Scale - HalfHeight;
	VisualRoot->SetRelativeLocation(FVector(0.f, 0.f, VisualBaseZ));

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
			MID->SetScalarParameterValue(GBEntityLocal::FlashParam, 0.f);
			MID->SetVectorParameterValue(GBEntityLocal::FlashColorParam, TentacleColor);
			TentacleMIDs.Add(MID);
			if (!TentacleMID)
			{
				TentacleMID = MID;
			}
		}
	}

	// The eye light takes the eye material glow colour when it has one (else the projectile colour).
	for (UMaterialInstanceDynamic* MID : EyeMIDs)
	{
		if (MID)
		{
			const FLinearColor Glow = MID->K2_GetVectorParameterValue(FName(TEXT("GlowColor")));
			if (Glow.R + Glow.G + Glow.B > 0.05f)
			{
				EyeLightColor = FLinearColor(Glow.R, Glow.G, Glow.B);
				break;
			}
		}
	}
	// A forward spot that starts on the lens surface and points out along the eye: the shell around the
	// lens is outside its cone, so the metal is not burnt by it. (Local units: the eye component is scaled.)
	EyeLight->SetRelativeLocation(FVector(GBEntityLocal::EyeLensFrontCm, GBEntityLocal::EyeCentreCm.Y, GBEntityLocal::EyeCentreCm.Z));
	EyeLight->SetRelativeRotation(FRotator::ZeroRotator);
	EyeLight->SetLightColor(EyeLightColor);
	EyeLight->SetAttenuationRadius(Tuning.EyeLightRadius * Scale);
	if (USpotLightComponent* Spot = Cast<USpotLightComponent>(EyeLight))
	{
		Spot->SetInnerConeAngle(Tuning.EyeLightInner);
		Spot->SetOuterConeAngle(Tuning.EyeLightOuter);
	}
	MuzzleLight->SetLightColor(EyeLightColor);
	MuzzleLight->SetAttenuationRadius(Tuning.MuzzleRadius * Scale);
	for (UPointLightComponent* Light : TentacleLights)
	{
		Light->SetLightColor(TentacleColor);
		Light->SetAttenuationRadius(Tuning.TentacleLightRadius * Scale);
	}

	// The real look is up: hide the base class placeholder, and only now move the collision to the shell.
	Sprite->SetVisibility(false);
	BodyMesh->SetVisibility(false);
	ShellHit->SetSphereRadius(Tuning.ShellRadius);
	ShellHit->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SetCapsuleMovementOnly(true);

	TentacleRig->Setup(EntityTentacles);
	bVisualBuilt = true;
}

void AGBEntity::SetCapsuleMovementOnly(bool bMovementOnly)
{
	UCapsuleComponent* Capsule = GetCapsuleComponent();
	if (!Capsule)
	{
		return;
	}
	if (bMovementOnly)
	{
		// The capsule only walks and blocks pawns. It leaves the Pawn object type, so melee, area spells and blasts
		// (object queries on pawns) find the shell sphere, not the empty capsule on the ground, and it ignores
		// every channel a weapon or a sight line uses.
		Capsule->SetCollisionObjectType(ECC_Vehicle);
		Capsule->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
		Capsule->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
		Capsule->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Ignore);
	}
	else
	{
		Capsule->SetCollisionObjectType(ECC_Pawn);
		Capsule->SetCollisionResponseToChannel(ECC_Camera, ECR_Block);
		Capsule->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		Capsule->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	}
}

FVector AGBEntity::GetEyeCentreWorld() const
{
	if (EntityEye)
	{
		return EntityEye->GetComponentTransform().TransformPosition(GBEntityLocal::EyeCentreCm);
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

FVector AGBEntity::GetChestLocation() const
{
	return bVisualBuilt ? GetShellCentreWorld() : Super::GetChestLocation();
}

FVector AGBEntity::GetDamageTargetLocation(const FVector& FromPoint) const
{
	if (!bVisualBuilt || !ShellHit)
	{
		return Super::GetDamageTargetLocation(FromPoint);
	}
	// The nearest point of the shell sphere (the point itself when it is inside): a blast at the shell hurts it.
	const FVector Centre = GetShellCentreWorld();
	const float Radius = ShellHit->GetScaledSphereRadius();
	const FVector Out = FromPoint - Centre;
	const float Distance = Out.Size();
	if (Distance <= Radius)
	{
		return FromPoint;
	}
	return Centre + Out / Distance * Radius;
}

float AGBEntity::GetVisualBoundsHeight() const
{
	FBox Box(ForceInit);
	const UPrimitiveComponent* Parts[3] = { EntityBody.Get(), EntityEye.Get(), EntityTentacles.Get() };
	for (const UPrimitiveComponent* Part : Parts)
	{
		if (Part && Part->IsVisible())
		{
			Box += Part->Bounds.GetBox();
		}
	}
	return Box.IsValid ? Box.GetSize().Z : 0.f;
}

bool AGBEntity::IsHeadshotLocation(const FVector& HitLocation) const
{
	// The weak point is the drawn eye. The hit sphere stands a little proud of the lens, so the radius has some slack.
	return FVector::Dist(HitLocation, GetEyeCentreWorld()) <= FMath::Max(Tuning.EyeWeakRadius, GBEntityLocal::EyeRadiusCm) * Tuning.Scale;
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

FVector AGBEntity::GetGoalHeadLocation(const AActor* Goal) const
{
	if (const APawn* GoalPawn = Cast<APawn>(Goal))
	{
		return GoalPawn->GetPawnViewLocation();
	}
	return Goal ? Goal->GetActorLocation() : GetActorLocation();
}

AGBProjectile* AGBEntity::FireBolt(AActor* Goal, const FVector& Origin, const FVector& Direction, float SpeedOverride)
{
	const FGBEnemyDefinition* Def = GetDefinition();
	if (!Def || !GetWorld())
	{
		return nullptr;
	}

	// Same shot as the base ranged attack, but a 3D energy bolt from a tentacle tip.
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
	if (SpeedOverride > 0.f)
	{
		Config.Speed = SpeedOverride;
	}
	Config.Radius = Stats->GetStat(GBStat::ProjectileRadius, Config.Radius);
	Config.GravityScale = Stats->GetStat(GBStat::ProjectileGravity, 0.f);
	Config.MaxRange = Stats->GetStat(GBStat::AggroRange, 0.f) * GetGameValue(GBEnemyKey::ProjectileRangeMult, 1.5f);
	Config.MaxLifetime = GetGameValue(GBProjectileKey::ProjectileMaxLifetime, Config.MaxLifetime);
	Config.bPixelArt = false;
	Config.Color = UGBDataSubsystem::ParseColor(Def->Strings.FindRef(GBEnemyKey::ProjectileColor),
		GBPlaceholder::ElementColor(static_cast<uint8>(Def->Element)));
	Config.bEnergyBolt = true;
	Config.BoltLength = Tuning.BoltLength;
	Config.BoltThickness = Tuning.BoltThickness;
	Config.BoltTrailLength = Tuning.BoltTrail;
	Config.BoltLightIntensity = Tuning.BoltLightIntensity;
	Config.BoltLightRadius = Tuning.BoltLightRadius;

	AGBProjectile* Bolt = AGBProjectile::SpawnProjectile(GetWorld(), Config, Origin, Direction, this);
	LastBolt = Bolt;
	FlashTentacles(Tuning.TentacleFlashShot);

	// Muzzle flash: a brief light at the tip that fired.
	if (MuzzleLight && bVisualBuilt)
	{
		MuzzleLight->SetWorldLocation(Origin);
		MuzzleLight->SetLightColor(Config.Color);
		MuzzleLight->SetIntensity(Tuning.MuzzleIntensity);
		MuzzleLight->SetVisibility(true);
		MuzzleTimer = Tuning.MuzzleTime;
	}

	// The tentacle whips back: release the reach and ripple it.
	if (TentacleRig)
	{
		TentacleRig->ClearReach();
		const float Strength = GetGameValue(GBEntityTentacle::RippleStrength, GBEntityTentacle::RippleStrengthDefault);
		TentacleRig->AddRipple(Strength * GBEntityLocal::ShotRippleShare);
	}
	return Bolt;
}

AGBProjectile* AGBEntity::FireDebugBolt(const FVector& Direction, float Speed)
{
	AActor* Goal = FindTarget();
	if (!Goal)
	{
		return nullptr;
	}
	return FireBolt(Goal, GetAttackOrigin(Goal), Direction, Speed);
}

AGBProjectile* AGBEntity::GetLastBolt() const
{
	return LastBolt.Get();
}

void AGBEntity::PerformAttack()
{
	AActor* Goal = FindTarget();
	const FGBEnemyDefinition* Def = GetDefinition();
	if (!Goal || !Def || !GetWorld())
	{
		return;
	}
	// After every shot (suppressed ones too, for the smoke run) the shell dips.
	AltDipTimer = Tuning.AltDipTime;
	if (bAttackSuppressed)
	{
		return;
	}
	if (!IsRanged())
	{
		Super::PerformAttack();
		return;
	}

	if (StatOr(TEXT("TentacleBoss"), 0.f) > 0.5f)
	{
		GBTentacleBoss::FState& Boss = GBTentacleBoss::States.FindOrAdd(this);
		const FVector Shell = GetShellCentreWorld();
		const FVector GoalPos = Goal->GetActorLocation();
		if (FVector::Dist2D(Shell, GoalPos) <= StatOr(TEXT("TB_WhipReach"), 600.f))
		{
			// the lash: the nearest tentacle has reached for the player through the windup; now it strikes
			FGBDamageInfo Info;
			Info.Amount = StatOr(TEXT("TB_WhipDamage"), 25.f);
			Info.Element = Def->Element;
			Info.Instigator = this;
			Info.Causer = this;
			Info.SourceId = EnemyId;
			Info.HitLocation = GoalPos;
			Info.HitDirection = (GoalPos - Shell).GetSafeNormal2D();
			UGBCombatLibrary::DealDamage(Goal, Info);
			FlashTentacles(Tuning.TentacleFlashShot);
			if (TentacleRig)
			{
				TentacleRig->ClearReach();
				TentacleRig->AddRipple(GetGameValue(GBEntityTentacle::RippleStrength, GBEntityTentacle::RippleStrengthDefault));
			}
			return;
		}
		if (Boss.FanCooldown <= 0.f && FMath::FRand() < StatOr(TEXT("TB_FanChance"), 0.4f))
		{
			// the fan: several tentacle tips fire at once, spread around the line to the player
			const int32 Count = FMath::Clamp(FMath::RoundToInt(StatOr(TEXT("TB_FanCount"), 5.f)), 2, 12);
			const float Fan = StatOr(TEXT("TB_FanDeg"), 40.f);
			const int32 Chains = TentacleRig && TentacleRig->IsReady() ? TentacleRig->GetChainCount() : 0;
			const int32 First = Chains > 0 ? FMath::RandRange(0, Chains - 1) : 0;
			for (int32 Index = 0; Index < Count; ++Index)
			{
				const FVector Origin = Chains > 0 ? TentacleRig->GetChainTipWorldPosition((First + Index * 3) % Chains) : GetChestLocation();
				FVector Direction = GoalPos - Origin;
				if (Direction.IsNearlyZero())
				{
					Direction = GetActorForwardVector();
				}
				const float Yaw = FMath::Lerp(-Fan * 0.5f, Fan * 0.5f, static_cast<float>(Index) / (Count - 1));
				FireBolt(Goal, Origin, Direction.RotateAngleAxis(Yaw, FVector::UpVector), 0.f);
			}
			Boss.FanCooldown = StatOr(TEXT("TB_FanCooldown"), 4.f);
			return;
		}
	}

	const FVector Origin = GetAttackOrigin(Goal);
	FVector Direction = Goal->GetActorLocation() - Origin;
	if (Direction.IsNearlyZero())
	{
		Direction = GetActorForwardVector();
	}
	FireBolt(Goal, Origin, Direction, 0.f);
}

void AGBEntity::OnDamagedVisual(const FGBDamageInfo& Info, float ActualDamage)
{
	const bool bWeak = Info.bHeadshot || IsHeadshotLocation(Info.HitLocation);
	if (bWeak)
	{
		FlashValue = 1.f;
	}
	FlashTentacles(Tuning.TentacleFlashHit);
	if (TentacleRig && State != EGBEnemyState::Dead)
	{
		// All the tentacles flinch together along the shot, harder for a weak-point hit.
		const float RecoilShare = FMath::Clamp(ActualDamage / Tuning.HitRefDamage, Tuning.HitMinShare, 1.f) * (bWeak ? Tuning.HitWeakMult : 1.f);
		TentacleRig->AddRecoil(Info.HitDirection, FMath::Min(RecoilShare, 1.f));
	}
	if (State != EGBEnemyState::Dead && bVisualBuilt)
	{
		// Knockback: the shell is pushed along the shot and tilts on a spring; a weak-point hit staggers more.
		const float Share = FMath::Clamp(ActualDamage / Tuning.HitRefDamage, Tuning.HitMinShare, 1.f) * (bWeak ? Tuning.HitWeakMult : 1.f);
		FVector Dir = Info.HitDirection.GetSafeNormal();
		if (Dir.IsNearlyZero())
		{
			Dir = -GetActorForwardVector();
		}
		const FVector Local = GetActorQuat().UnrotateVector(Dir);
		HitPitchSpring.V += -Local.X * Tuning.HitTiltKick * Share;
		HitRollSpring.V += Local.Y * Tuning.HitTiltKick * Share;
		PushX.V += Local.X * Tuning.HitPushKick * Tuning.Scale * Share;
		PushY.V += Local.Y * Tuning.HitPushKick * Tuning.Scale * Share;
		PushZ.V += Local.Z * Tuning.HitPushKick * Tuning.Scale * Share;
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
	bDying = true;
	HitPitchSpring.V += GBEntityLocal::DeathPitchKick;
	HitRollSpring.V += GBEntityLocal::DeathRollKick;
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

void AGBEntity::FlashTentacles(float Amount)
{
	TentacleFlash = FMath::Max(TentacleFlash, Amount);
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

void AGBEntity::ResetMotionStats()
{
	PeakYawRate = 0.f;
	ShellZMin = 1e9f;
	ShellZMax = -1e9f;
	PeakRoll = 0.f;
}

void AGBEntity::UpdateTurn(float Dt, const AActor* Goal)
{
	const FVector ToGoal = (Goal->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
	if (ToGoal.IsNearlyZero())
	{
		YawRateDeg = FMath::FInterpTo(YawRateDeg, 0.f, Dt, Tuning.TurnAccel);
		return;
	}
	const float Yaw = GetActorRotation().Yaw;
	const float Error = FMath::FindDeltaAngleDegrees(Yaw, ToGoal.Rotation().Yaw);
	// Aim for a rate proportional to the error (it slows into the target), capped, and reached with inertia.
	const float Desired = FMath::Clamp(Error * Tuning.TurnGain, -Tuning.TurnMaxRate, Tuning.TurnMaxRate);
	YawRateDeg = FMath::Clamp(FMath::FInterpTo(YawRateDeg, Desired, Dt, Tuning.TurnAccel), -Tuning.TurnMaxRate, Tuning.TurnMaxRate);
	float Step = YawRateDeg * Dt;
	if (FMath::Abs(Step) > FMath::Abs(Error) && FMath::Sign(Step) == FMath::Sign(Error))
	{
		Step = Error;
	}
	SetActorRotation(FRotator(0.f, Yaw + Step, 0.f));
}

void AGBEntity::UpdateStrafe(float Dt, const AActor* Goal)
{
	StrafeTimer -= Dt;
	if (StrafeTimer <= 0.f)
	{
		StrafeSign = -StrafeSign;
		StrafeTimer = FMath::FRandRange(Tuning.StrafeFlipMin, Tuning.StrafeFlipMax);
	}

	FVector ToGoal = Goal->GetActorLocation() - GetActorLocation();
	ToGoal.Z = 0.f;
	const float Distance = ToGoal.Size();
	if (Distance < 1.f)
	{
		return;
	}
	ToGoal /= Distance;
	// Orbit the target, drifting towards a preferred range (a share of the attack range) while doing it.
	const float AttackRange = Stats->GetStat(GBStat::AttackRange, 0.f);
	const float Keep = FMath::Max(AttackRange * Tuning.StrafeKeep, 100.f);
	const float Radial = FMath::Clamp((Distance - Keep) / (Keep * 0.5f), -1.f, 1.f);
	const FVector Side(-ToGoal.Y * StrafeSign, ToGoal.X * StrafeSign, 0.f);
	const FVector Dir = (Side + ToGoal * Radial * 0.6f).GetSafeNormal();
	if (IsDirectionBlocked(Dir, Tuning.StrafeProbe * Tuning.Scale, Goal))
	{
		// A wall or a ledge that way: go the other way round for a while.
		StrafeSign = -StrafeSign;
		StrafeTimer = FMath::FRandRange(Tuning.StrafeFlipMin, Tuning.StrafeFlipMax);
		return;
	}
	AddMovementInput(Dir, Tuning.StrafeSpeed);
	bStrafingLastTick = true;
}

void AGBEntity::UpdateFlight(float Dt, const AActor* Goal)
{
	const UCharacterMovementComponent* Move = GetCharacterMovement();
	const float Speed = Move ? FMath::Max(Move->MaxWalkSpeed, 1.f) : 1.f;
	const float AccelRef = Move ? FMath::Max(Move->MaxAcceleration, 1.f) : 1.f;
	const FVector LocalVel = GetActorQuat().UnrotateVector(GetVelocity());

	// Altitude of the shell centre above the floor: a slow drift, a rise in the windup, a dip after a shot.
	const float Scale = Tuning.Scale;
	// Altitudes are authored for scale 1 and follow EntityScale; the lowest keeps the tentacle tips off the floor.
	const float AltLo = FMath::Max(Tuning.AltMin * Scale, (GBEntityLocal::TentacleTipDepthCm + GBEntityLocal::TipClearanceCm) * Scale);
	const float AltHi = FMath::Max(AltLo, Tuning.AltMax * Scale);
	const float Mid = 0.5f * (AltLo + AltHi);
	const float W = 2.f * PI * Tuning.AltDriftHz * BobTime;
	const float Drift = Tuning.AltDriftAmp * Scale * (0.65f * FMath::Sin(W + 0.4f) + 0.35f * FMath::Sin(2.1f * W + 1.9f));
	AltDipTimer = FMath::Max(0.f, AltDipTimer - Dt);
	const bool bWindup = GetState() == EGBEnemyState::Windup;
	WindupAlpha = FMath::FInterpTo(WindupAlpha, (bWindup && !bDying) ? 1.f : 0.f, Dt, GBEntityLocal::WindupAlphaRate);
	float AltTarget = FMath::Clamp(Mid + Drift + (Tuning.AltRise * WindupAlpha - Tuning.AltDip * (AltDipTimer / Tuning.AltDipTime)) * Scale,
		AltLo, AltHi);
	// Never into the geometry above: the ceiling probe limits the altitude (a low ceiling beats the tip clearance).
	AltTarget = FMath::Min(AltTarget, FMath::Max(CeilingAltCm, Tuning.ShellRadius * Scale * GBEntityLocal::MinShellFloorShare));
	if (bHoldReach)
	{
		AltTarget = AltSpring.X; // a hand-set reach aims at a fixed world point: the shell holds its height meanwhile
	}
	float AltOmega = Tuning.AltOmega;
	if (bDying)
	{
		AltTarget = GBEntityLocal::DeathAltitudeCm * Scale;
		AltOmega = GBEntityLocal::DeathAltOmega;
	}
	if (!bAltInitialised)
	{
		AltSpring.X = AltTarget;
		bAltInitialised = true;
	}
	AltSpring.Step(AltTarget, Dt, AltOmega, 0.9f);

	// Aim: the whole body pitches at the target's head (clamped, smoothed); the eye turns a little ahead of it.
	const FVector ShellCentre = VisualRoot->GetComponentLocation();
	float RawPitch = 0.f;
	float YawError = 0.f;
	if (Goal && !bDying)
	{
		const FVector ToHead = GetGoalHeadLocation(Goal) - ShellCentre;
		RawPitch = FMath::RadiansToDegrees(FMath::Atan2(ToHead.Z, FMath::Max(ToHead.Size2D(), 1.f)));
		YawError = FMath::FindDeltaAngleDegrees(GetActorRotation().Yaw, ToHead.Rotation().Yaw);
	}
	if (bHoldReach)
	{
		RawPitch = AimPitchSpring.X; // see the altitude hold: a hand-set reach needs a still shell
	}
	AimPitchSpring.Step(FMath::Clamp(RawPitch, -Tuning.PitchMax, Tuning.PitchMax), Dt, Tuning.AimOmega, 0.9f);
	EyePitch = FMath::FInterpTo(EyePitch, FMath::Clamp(RawPitch - AimPitchSpring.X, -Tuning.EyeTrackMax, Tuning.EyeTrackMax), Dt, Tuning.EyeTrackSpeed);
	EyeYaw = FMath::FInterpTo(EyeYaw, FMath::Clamp(YawError, -Tuning.EyeTrackMax, Tuning.EyeTrackMax), Dt, Tuning.EyeTrackSpeed);

	// Banking like a drone: roll into sideways speed and turns, pitch forward when speeding up, back when braking.
	const float FwdAccel = Dt > KINDA_SMALL_NUMBER ? (LocalVel.X - PrevLocalFwd) / Dt : 0.f;
	PrevLocalFwd = LocalVel.X;
	FwdAccelSmoothed = FMath::FInterpTo(FwdAccelSmoothed, FwdAccel, Dt, 6.f);
	const float RollTarget = FMath::Clamp(LocalVel.Y / Speed * Tuning.BankMax + YawRateDeg * Tuning.BankPerYawRate, -Tuning.BankMax, Tuning.BankMax);
	const float PitchTarget = FMath::Clamp(-(LocalVel.X / Speed) * Tuning.LeanDeg - (FwdAccelSmoothed / AccelRef) * Tuning.AccelPitchDeg,
		-Tuning.BankMax, Tuning.BankMax);
	BankRollSpring.Step(RollTarget, Dt, Tuning.TiltOmega, Tuning.TiltZeta);
	MotionPitchSpring.Step(PitchTarget, Dt, Tuning.TiltOmega, Tuning.TiltZeta);

	// Knockback springs (underdamped, so a hit wobbles).
	HitPitchSpring.Step(0.f, Dt, Tuning.HitOmega, Tuning.HitZeta);
	HitRollSpring.Step(0.f, Dt, Tuning.HitOmega, Tuning.HitZeta);
	PushX.Step(0.f, Dt, Tuning.HitOmega, Tuning.HitZeta);
	PushY.Step(0.f, Dt, Tuning.HitOmega, Tuning.HitZeta);
	PushZ.Step(0.f, Dt, Tuning.HitOmega, Tuning.HitZeta);

	const float Pitch = AimPitchSpring.X + MotionPitchSpring.X + HitPitchSpring.X;
	const float Roll = BankRollSpring.X + HitRollSpring.X;
	VisualRoot->SetRelativeLocation(FVector(PushX.X, PushY.X, AltSpring.X - CapsuleHalfHeightCached + PushZ.X));
	VisualRoot->SetRelativeRotation(FRotator(Pitch, 0.f, Roll));
	EntityEye->SetRelativeRotation(FRotator(EyePitch, EyeYaw, 0.f));

	const float ShellZ = VisualRoot->GetComponentLocation().Z;
	ShellZMin = FMath::Min(ShellZMin, ShellZ);
	ShellZMax = FMath::Max(ShellZMax, ShellZ);
	PeakRoll = FMath::Max(PeakRoll, FMath::Abs(Roll));
}

void AGBEntity::UpdateCeiling(float Dt)
{
	CeilingTimer -= Dt;
	if (CeilingTimer > 0.f)
	{
		return;
	}
	CeilingTimer = Tuning.CeilingProbeInterval;
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	// Sweep the shell sphere straight up from where it is, as far as it could ever fly.
	const float Radius = Tuning.ShellRadius * Tuning.Scale;
	const FVector Start = GetShellCentreWorld();
	const float Reach = FMath::Max(Tuning.AltMax * Tuning.Scale - AltSpring.X, 0.f) + Tuning.CeilingMargin * Tuning.Scale + Radius;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GBEntityCeiling), false, this);
	FHitResult Hit;
	if (World->SweepSingleByChannel(Hit, Start, Start + FVector(0.f, 0.f, Reach), FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(Radius), Params))
	{
		CeilingAltCm = Hit.bStartPenetrating
			? AltSpring.X - Radius * 0.5f                                  // already inside something: sink out of it
			: AltSpring.X + Hit.Distance - Tuning.CeilingMargin * Tuning.Scale;
	}
	else
	{
		CeilingAltCm = 1e9f;
	}
}

void AGBEntity::UpdateLights(float Dt)
{
	if (bDying)
	{
		DeathAlpha = FMath::Max(0.f, DeathAlpha - Dt / Tuning.EyeLightFade);
	}
	const float Pulse = 1.f + Tuning.EyeLightPulse * FMath::Sin(2.f * PI * Tuning.EyeLightPulseHz * BobTime);
	const float Flare = 1.f + Tuning.EyeLightWindupFlare * WindupAlpha + Tuning.EyeLightHitFlare * FlashValue;
	const float Intensity = Tuning.EyeLightIntensity * Pulse * Flare * DeathAlpha;
	EyeLight->SetIntensity(Intensity);
	EyeLight->SetAttenuationRadius(Tuning.EyeLightRadius * Tuning.Scale);
	EyeLight->SetVisibility(Intensity > 1.f);

	if (MuzzleTimer > 0.f)
	{
		MuzzleTimer = FMath::Max(0.f, MuzzleTimer - Dt);
		MuzzleLight->SetIntensity(Tuning.MuzzleIntensity * (MuzzleTimer / Tuning.MuzzleTime));
		if (MuzzleTimer <= 0.f)
		{
			MuzzleLight->SetVisibility(false);
		}
	}

	// Tentacle flash on a shot and on hits: all the hologram brightens for a moment.
	if (TentacleFlash > 0.f)
	{
		TentacleFlash = FMath::Max(0.f, TentacleFlash - Dt / Tuning.TentacleFlashFade);
	}
	for (UMaterialInstanceDynamic* MID : TentacleMIDs)
	{
		if (MID)
		{
			MID->SetScalarParameterValue(GBEntityLocal::FlashParam, TentacleFlash);
		}
	}

	// A few dim green lights riding the middle of different chains, so the floor and walls under it glow faintly.
	const int32 Chains = TentacleRig ? TentacleRig->GetChainCount() : 0;
	const int32 Count = Chains > 0 ? FMath::Clamp(FMath::RoundToInt(Tuning.TentacleLightCount), 0, TentacleLights.Num()) : 0;
	for (int32 Index = 0; Index < TentacleLights.Num(); ++Index)
	{
		UPointLightComponent* Light = TentacleLights[Index];
		if (Index >= Count)
		{
			Light->SetVisibility(false);
			continue;
		}
		const int32 Chain = Index * Chains / Count;
		const int32 Joint = FMath::Max(1, FMath::RoundToInt(TentacleRig->GetJointCount(Chain) * GBEntityLocal::TentacleLightAlong));
		Light->SetWorldLocation(TentacleRig->GetChainJointWorldPosition(Chain, Joint));
		const float LightIntensity = Tuning.TentacleLightIntensity * (1.f + TentacleFlash) * DeathAlpha;
		Light->SetIntensity(LightIntensity);
		Light->SetAttenuationRadius(Tuning.TentacleLightRadius * Tuning.Scale);
		Light->SetVisibility(LightIntensity > 0.5f);
	}
}

void AGBEntity::Tick(float DeltaSeconds)
{
	ReadTuning();
	UCharacterMovementComponent* Move = GetCharacterMovement();
	const FVector VelocityBefore = Move ? Move->Velocity : FVector::ZeroVector;
	const bool bWasStrafing = bStrafingLastTick;

	Super::Tick(DeltaSeconds);

	// The base state machine stops the body at each windup; a strafe keeps its momentum through that.
	if (bWasStrafing && Move && !IsDead() && Move->Velocity.IsNearlyZero())
	{
		Move->Velocity = VelocityBefore;
	}

	// The whole frame is used (springs and the rig sub-step inside); only a huge hitch is cut.
	const float Dt = FMath::Min(DeltaSeconds, 0.5f);
	BobTime += Dt;

	if (FlashValue > 0.f)
	{
		FlashValue = FMath::Max(0.f, FlashValue - DeltaSeconds / GBEntityLocal::FlashFadeTime);
		ApplyFlash();
	}

	AActor* Goal = FindTarget();
	const bool bPresent = !GetEraComponent() || GetEraComponent()->IsPresent();
	const bool bFrozen = GetControlLockTime() > 0.f;
	const bool bAlive = !IsDead();
	const bool bActive = bVisualBuilt && bAlive && Goal && bPresent && !bFrozen;

	if (bActive && !bHoldReach)
	{
		UpdateTurn(Dt, Goal);
	}
	else
	{
		YawRateDeg = FMath::FInterpTo(YawRateDeg, 0.f, Dt, Tuning.TurnAccel);
	}
	// The peak turn rate is measured on the actor itself, not on the controller's own variable.
	const float ActorYaw = GetActorRotation().Yaw;
	if (bHasPrevYaw && Dt > KINDA_SMALL_NUMBER)
	{
		PeakYawRate = FMath::Max(PeakYawRate, FMath::Abs(FMath::FindDeltaAngleDegrees(PrevActorYaw, ActorYaw)) / Dt);
	}
	PrevActorYaw = ActorYaw;
	bHasPrevYaw = true;

	// Strafe: in range and in sight the Entity circles the player instead of standing still.
	bStrafingLastTick = false;
	if (ForcedStrafeTime > 0.f)
	{
		ForcedStrafeTime -= DeltaSeconds;
	}
	bool bBossChasing = false;
	if (bActive && StatOr(TEXT("TentacleBoss"), 0.f) > 0.5f)
	{
		GBTentacleBoss::FState& Boss = GBTentacleBoss::States.FindOrAdd(this);
		Boss.FanCooldown = FMath::Max(0.f, Boss.FanCooldown - DeltaSeconds);
		Boss.ModeLeft -= DeltaSeconds;
		if (Boss.ModeLeft <= 0.f)
		{
			Boss.bChasing = !Boss.bChasing;
			Boss.ModeLeft = Boss.bChasing ? StatOr(TEXT("TB_ChaseTime"), 6.f) : StatOr(TEXT("TB_RangeTime"), 3.f);
		}
		FVector ToGoal = Goal->GetActorLocation() - GetActorLocation();
		ToGoal.Z = 0.f;
		const float Distance = ToGoal.Size();
		const FVector Dir = ToGoal.GetSafeNormal();
		if (Boss.bChasing)
		{
			// hunting: straight at the player, through windups and recoveries too
			bBossChasing = true;
			if (Distance > StatOr(TEXT("TB_ChaseKeep"), 350.f))
			{
				AddMovementInput(Dir, 1.f);
				bStrafingLastTick = true;
			}
		}
		else if (Distance < StatOr(TEXT("TB_RangeKeep"), 1600.f) && !IsDirectionBlocked(-Dir, Tuning.StrafeProbe * Tuning.Scale, Goal))
		{
			// keeping away for a while: it backs off and shoots (the strafe below circles meanwhile)
			AddMovementInput(-Dir, 1.f);
			bStrafingLastTick = true;
		}
	}
	if (bActive && !bBossChasing)
	{
		const float AttackRange = Stats->GetStat(GBStat::AttackRange, 0.f);
		const bool bFighting = !bAttackSuppressed && AttackRange > 0.f
			&& (GetState() == EGBEnemyState::Windup || GetState() == EGBEnemyState::Recover)
			&& FVector::Dist(GetActorLocation(), Goal->GetActorLocation()) <= AttackRange && HasLineOfSight(Goal);
		if (ForcedStrafeTime > 0.f || bFighting)
		{
			UpdateStrafe(DeltaSeconds, Goal);
		}
	}

	if (bVisualBuilt)
	{
		UpdateCeiling(Dt);
		UpdateFlight(Dt, bAlive ? Goal : nullptr);
		UpdateLights(Dt);
		if (TentacleRig)
		{
			// What the Entity is doing, for the coordinated tentacle motion: moving, hunting, winding up.
			FGBTentacleIntent Intent;
			Intent.WorldVelocity = GetVelocity();
			Intent.FullSpeed = Move ? Move->MaxWalkSpeed : 1.f;
			Intent.bHasTarget = bAlive && Goal && bPresent && GetState() != EGBEnemyState::Idle;
			Intent.WorldTarget = Goal ? GetGoalHeadLocation(Goal) : GetActorLocation();
			Intent.Windup = bAlive ? WindupAlpha : 0.f;
			TentacleRig->SetIntent(Intent);
		}
	}

	if (bVisualBuilt && StatOr(TEXT("TB_Laser"), 0.f) > 0.5f)
	{
		GBTentacleBoss::FState& Boss = GBTentacleBoss::States.FindOrAdd(this);
		const FGBEnemyDefinition* LaserDef = GetDefinition();
		const FLinearColor LaserColor = UGBDataSubsystem::ParseColor(LaserDef ? LaserDef->Strings.FindRef(TEXT("LaserColor")) : FString(),
			FLinearColor(0.04f, 1.f, 0.43f));
		if (!Boss.Beam.IsValid())
		{
			UStaticMeshComponent* Beam = NewObject<UStaticMeshComponent>(this, TEXT("TentacleBossLaser"));
			Beam->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Beam->SetCastShadow(false);
			Beam->SetCanEverAffectNavigation(false);
			// a laser lights nothing around it (owner): no Lumen bounce from its glow, only the narrow spot along it
			Beam->bAffectDynamicIndirectLighting = false;
			Beam->bAffectDistanceFieldLighting = false;
			Beam->SetVisibleInRayTracing(false);
			Beam->RegisterComponent();
			GBPlaceholder::SetupShape(Beam, EGBShape::Cylinder, FVector(10.f, 10.f, 100.f), LaserColor * StatOr(TEXT("TB_LaserGlow"), 12.f), /*bGlow*/ true);
			Boss.Beam = Beam;
			USpotLightComponent* Light = NewObject<USpotLightComponent>(this, TEXT("TentacleBossLaserLight"));
			Light->SetMobility(EComponentMobility::Movable);
			Light->CastShadows = false;
			Light->IntensityUnits = ELightUnits::Candelas;
			Light->RegisterComponent();
			Light->SetLightColor(LaserColor);
			Light->SetInnerConeAngle(0.5f);
			Light->SetOuterConeAngle(StatOr(TEXT("TB_LaserLightCone"), 2.5f));
			Light->SetIntensity(StatOr(TEXT("TB_LaserLight"), 6000.f));
			Boss.BeamLight = Light;
		}
		UStaticMeshComponent* Beam = Boss.Beam.Get();
		USpotLightComponent* Light = Boss.BeamLight.Get();
		const bool bOn = !IsDead() && !bDying && EntityEye;
		Beam->SetVisibility(bOn);
		if (Light)
		{
			Light->SetVisibility(bOn);
		}
		if (bOn)
		{
			// straight out of the eye, along where the eye looks; it ends where it meets the first wall, floor or body
			const FVector Origin = GetEyeCentreWorld();
			const FVector Dir = EntityEye->GetForwardVector();
			const float MaxLength = StatOr(TEXT("TB_LaserLength"), 6000.f);
			FHitResult Hit;
			FCollisionQueryParams Params(SCENE_QUERY_STAT(TentacleLaser), false, this);
			const float Length = GetWorld()->LineTraceSingleByChannel(Hit, Origin, Origin + Dir * MaxLength, ECC_Visibility, Params)
				? FMath::Max(Hit.Distance, 1.f) : MaxLength;
			const float Thick = StatOr(TEXT("TB_LaserThickness"), 10.f);
			Beam->SetWorldLocationAndRotation(Origin + Dir * (Length * 0.5f), FRotationMatrix::MakeFromZ(Dir).Rotator());
			Beam->SetWorldScale3D(FVector(Thick / 100.f, Thick / 100.f, Length / 100.f));
			if (Light)
			{
				Light->SetWorldLocationAndRotation(Origin, Dir.Rotation());
				Light->SetAttenuationRadius(Length + 100.f);
			}
		}
	}

	// A reach lives only through an active windup: a cancelled attack, a lost line of sight, an era change or death ends it.
	if (TentacleRig && !bHoldReach && (GetState() != EGBEnemyState::Windup || !Goal || !bPresent) && TentacleRig->IsReaching())
	{
		TentacleRig->ClearReach();
	}

	if (bAttackSuppressed)
	{
		if (ForcedStrafeTime <= 0.f)
		{
			StopMoving();
		}
		return;
	}

	// During the windup the nearest tentacle reaches towards the player.
	if (GetState() == EGBEnemyState::Windup && TentacleRig && TentacleRig->IsReady() && Goal)
	{
		TentacleRig->SetReachTarget(Goal->GetActorLocation(), 1.f);
	}
}

FVector AGBEntity::GetShellCentreWorld() const
{
	return VisualRoot ? VisualRoot->GetComponentLocation() : GetActorLocation();
}

float AGBEntity::GetShellRadiusWorld() const
{
	return ShellHit ? ShellHit->GetScaledSphereRadius() : 0.f;
}
