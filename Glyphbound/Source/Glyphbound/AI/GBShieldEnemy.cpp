#include "AI/GBShieldEnemy.h"

#include "Components/BoxComponent.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"

#include "Combat/GBCombatLibrary.h"
#include "Core/GBPlaceholder.h"
#include "Core/GBStatNames.h"
#include "Data/GBDataSubsystem.h"
#include "Stats/GBAttributeComponent.h"
#include "Stats/GBStatsComponent.h"

namespace GBShieldLocal
{
	constexpr float BlockedWindowSeconds = 3.f;      // blocked damage counts towards a stagger within this time
	constexpr float BoxAheadOfHandle = 18.f;         // cm at shield scale 1: the hit box stands this far in front of the shield's handle (about its face)
	constexpr float MeshCentreAboveOrigin = 4.f;     // cm at shield scale 1: the shield mesh's middle over its handle
	constexpr float BashRecoverSeconds = 0.35f;      // the shield comes back after the strike
	constexpr float AimBlendSeconds = 0.25f;         // the pistol arm rises to aim (and drops) in this time
	constexpr float MaxArmYaw = 70.f;                // degrees: the pistol arm aims at most this far off the way he faces
	constexpr float RecoilDegrees = 22.f;            // the revolver's kick on the arm
	const FName RightShoulder(TEXT("upperarm_r"));
	const TCHAR* const ShieldColor = TEXT("#5A6A7A");
}

AGBShieldEnemy::AGBShieldEnemy()
{
	ShieldMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShieldMesh"));
	ShieldMesh->SetupAttachment(GetCapsuleComponent());
	ShieldMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ShieldMesh->SetCanEverAffectNavigation(false);
	ShieldMesh->SetGenerateOverlapEvents(false);
	ShieldMesh->SetVisibility(false);

	// The hit box is on the capsule, in front of him: it blocks the player's shot traces (Camera) and sight (Visibility), nothing else.
	ShieldBox = CreateDefaultSubobject<UBoxComponent>(TEXT("ShieldBox"));
	ShieldBox->SetupAttachment(GetCapsuleComponent());
	ShieldBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ShieldBox->SetCollisionObjectType(ECC_WorldDynamic);
	ShieldBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	ShieldBox->SetCollisionResponseToChannel(ECC_Camera, ECR_Block);
	ShieldBox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	ShieldBox->SetCanEverAffectNavigation(false);
	ShieldBox->SetGenerateOverlapEvents(false);
}

void AGBShieldEnemy::BeginPlay()
{
	Super::BeginPlay();
	BuildShield();
	if (Attributes)
	{
		Attributes->OnModifyIncomingDamage.AddUObject(this, &AGBShieldEnemy::HandleIncomingDamage);
	}
}

void AGBShieldEnemy::BuildShield()
{
	const FGBEnemyDefinition* Def = GetDefinition();
	if (!Def)
	{
		return;
	}
	// Sizes are given at scale 1; the shield mesh has its own scale (ShieldScale, default the chief's MeshScale) and the hit box follows it.
	const float Scale = FMath::Max(S(TEXT("MeshScale"), 1.f), 0.1f);
	ShieldScale = FMath::Max(S(TEXT("ShieldScale"), Scale), 0.1f);
	ShieldWidthCm = S(TEXT("ShieldWidth"), 100.f) * ShieldScale;
	ShieldHeightCm = S(TEXT("ShieldHeight"), 125.f) * ShieldScale;
	const float Thickness = S(TEXT("ShieldThickness"), 12.f);
	PoseBlend = S(TEXT("ShieldPoseBlend"), 0.3f);

	// The visual: the asset when it exists, otherwise a slab of the same size.
	UStaticMesh* Asset = GBEnemy3D::LoadAsset<UStaticMesh>(GBEnemy3D::Str(*Def, FName(TEXT("Shield"))), EnemyId);
	if (Asset)
	{
		ShieldMesh->SetStaticMesh(Asset);
	}
	else
	{
		GBPlaceholder::SetupShape(ShieldMesh, EGBShape::Cube, FVector(Thickness, ShieldWidthCm / ShieldScale, ShieldHeightCm / ShieldScale), UGBDataSubsystem::ParseColor(GBShieldLocal::ShieldColor, FLinearColor::Gray));
	}
	ShieldMesh->SetVisibility(true);

	// The shield's two places on the capsule (its mesh: face to +X, origin at the handle on its back, a little under its middle).
	// Raised: upright in front of his chest, the handle where his left fist reaches. Lowered: turned to his left side at the hip.
	const FVector MeshScale3D(ShieldScale);
	const FVector RaisedAt(S(TEXT("ShieldHandleForward"), 33.f) * Scale, S(TEXT("ShieldHandleSide"), -9.f) * Scale, S(TEXT("ShieldHandleHeight"), 22.f) * Scale);
	const FVector LoweredAt(S(TEXT("ShieldLoweredForward"), 3.f) * Scale, S(TEXT("ShieldLoweredSide"), -30.f) * Scale, S(TEXT("ShieldLoweredHeight"), 2.f) * Scale);
	ShieldRaisedPlace = FTransform(FRotator::ZeroRotator, RaisedAt, MeshScale3D);
	ShieldLoweredPlace = FTransform(FRotator(0.f, S(TEXT("ShieldLoweredYaw"), -80.f), 0.f), LoweredAt, MeshScale3D);
	ShieldPlace = ShieldRaisedPlace;
	ShieldAlpha = 1.f;
	ShieldMesh->SetRelativeTransform(ShieldRaisedPlace);

	// The hit box: as big as the shield, on its raised place (it only blocks while raised).
	ShieldBox->SetBoxExtent(FVector(Thickness * 0.5f + 4.f, ShieldWidthCm * 0.5f, ShieldHeightCm * 0.5f), false);
	ShieldBox->SetRelativeLocation(RaisedAt + FVector(GBShieldLocal::BoxAheadOfHandle, 0.f, GBShieldLocal::MeshCentreAboveOrigin) * ShieldScale);

	if (AnimInst)
	{
		// The left arm is the IK's now: the old pose layer stays empty unless the data still names one.
		AnimInst->SetPoseLayer(GBEnemy3D::Str(*Def, FName(TEXT("Pose.ShieldRaised"))));
		AnimInst->SetPoseTarget(1.f, 0.01f);
	}
	bShieldUp = true;
	UpdateShieldVisuals();
}

void AGBShieldEnemy::UpdateShieldVisuals(float DeltaSeconds)
{
	// The shield moves between its two places in the same time the arm takes; it blocks (hit box and damage hook) only once it has got
	// there (alpha 0.8), and stops blocking the moment it starts to lower.
	const bool bWantsUp = WantsShieldUp();
	ShieldAlpha = DeltaSeconds > 0.f ? FMath::FInterpConstantTo(ShieldAlpha, bWantsUp ? 1.f : 0.f, DeltaSeconds, 1.f / FMath::Max(PoseBlend, 0.05f)) : (bWantsUp ? 1.f : 0.f);
	const bool bBlocking = IsShieldBlocking();
	ShieldBox->SetCollisionEnabled(bBlocking ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	if (State != EGBEnemyState::Dead)
	{
		ShieldPlace.Blend(ShieldLoweredPlace, ShieldRaisedPlace, FMath::SmoothStep(0.f, 1.f, ShieldAlpha));
		const float Push = ComputeBashPush();
		if (Push > 0.f)
		{
			// The bash never pushes the handle out of the left arm's reach: the fist stays on it.
			const USkeletalMeshComponent* Body = GetMesh();
			const FName Upper(TEXT("upperarm_l")), Lower(TEXT("lowerarm_l")), Hand(TEXT("hand_l"));
			float Allowed = Push;
			if (Body && Body->DoesSocketExist(Upper) && Body->DoesSocketExist(Lower) && Body->DoesSocketExist(Hand))
			{
				const float Reach = 0.95f * (FVector::Dist(Body->GetSocketLocation(Upper), Body->GetSocketLocation(Lower)) + FVector::Dist(Body->GetSocketLocation(Lower), Body->GetSocketLocation(Hand)));
				const FVector Shoulder = GetActorTransform().InverseTransformPosition(Body->GetSocketLocation(Upper));
				const FVector Grip = ShieldPlace.TransformPosition(FVector(-S(TEXT("ShieldGripBack"), 10.f), 0.f, 0.f));
				const float Rest2 = FMath::Square(Grip.Y - Shoulder.Y) + FMath::Square(Grip.Z - Shoulder.Z);
				Allowed = Rest2 < Reach * Reach ? FMath::Clamp(Shoulder.X + FMath::Sqrt(Reach * Reach - Rest2) - Grip.X, 0.f, Push) : 0.f;
			}
			ShieldPlace.AddToTranslation(FVector(Allowed, 0.f, 0.f));
		}
		else
		{
			ShieldPlace.AddToTranslation(FVector(Push, 0.f, 0.f));
		}
		ShieldMesh->SetRelativeTransform(ShieldPlace);
		// The hit box goes with the shield (it only blocks while raised, but the bash pushes the raised shield forward).
		ShieldBox->SetRelativeLocationAndRotation(ShieldPlace.TransformPosition(FVector(GBShieldLocal::BoxAheadOfHandle, 0.f, GBShieldLocal::MeshCentreAboveOrigin)), ShieldPlace.GetRotation());
	}
	if (AnimInst)
	{
		AnimInst->SetPoseTarget(bWantsUp ? 1.f : 0.f, PoseBlend);
	}
	UpdateArms(DeltaSeconds);
}

float AGBShieldEnemy::ComputeBashPush() const
{
	const float Scale = FMath::Max(S(TEXT("MeshScale"), 1.f), 0.1f);
	const float Pull = S(TEXT("BashPull"), 12.f) * Scale;
	const float Push = S(TEXT("BashPush"), 40.f) * Scale;
	if (DebugHoldMode == 3)
	{
		return Push;
	}
	if (Phase == EChiefPhase::Bash && State != EGBEnemyState::Dead)
	{
		// The windup: he draws the shield back for 60 % of it, then drives it forward; the strike lands at its end.
		const float Windup = FMath::Max(S(TEXT("BashWindup"), 0.45f), 0.05f);
		const float T = FMath::Clamp(1.f - PhaseTimer / Windup, 0.f, 1.f);
		return T < 0.6f ? -Pull * FMath::SmoothStep(0.f, 1.f, T / 0.6f) : FMath::Lerp(-Pull, Push, FMath::InterpEaseIn(0.f, 1.f, (T - 0.6f) / 0.4f, 2.f));
	}
	// The follow-through: the shield comes back from the strike.
	return BashRecover > 0.f ? Push * FMath::SmoothStep(0.f, 1.f, BashRecover / GBShieldLocal::BashRecoverSeconds) : 0.f;
}

void AGBShieldEnemy::UpdateArms(float DeltaSeconds)
{
	USkeletalMeshComponent* Body = GetMesh();
	if (!AnimInst || !Body || State == EGBEnemyState::Dead)
	{
		return;
	}
	BashRecover = FMath::Max(0.f, BashRecover - DeltaSeconds);
	const FTransform MeshRel = Body->GetRelativeTransform();   // capsule space -> the mesh component's space
	const float Scale = FMath::Max(S(TEXT("MeshScale"), 1.f), 0.1f);
	auto ToMeshDir = [&MeshRel](const FVector& ActorDir) { return MeshRel.InverseTransformVectorNoScale(ActorDir.GetSafeNormal()); };

	// Left: the wrist a little behind the handle (the fist closes on it), the elbow out to the left and down.
	FGBArmIK Left;
	Left.Alpha = 1.f;
	Left.Location = MeshRel.InverseTransformPosition(ShieldPlace.TransformPosition(FVector(-S(TEXT("ShieldGripBack"), 10.f), 0.f, 0.f)));
	Left.Pole = ToMeshDir(FVector(-0.3f, -1.f, -0.8f));
	AnimInst->SetArmIK(0, Left);

	// Right: the pistol in one hand, low at his side, or stretched out at the player (aim and the moment after the shot).
	const bool bAimWanted = !IsStaggered() && (DebugHoldMode == 2 || (DebugHoldMode == 0 && (Phase == EChiefPhase::Aim || Phase == EChiefPhase::Open)));
	AimAlpha = DeltaSeconds > 0.f ? FMath::FInterpConstantTo(AimAlpha, bAimWanted ? 1.f : 0.f, DeltaSeconds, 1.f / GBShieldLocal::AimBlendSeconds) : (bAimWanted ? 1.f : 0.f);
	const FTransform ActorTM = GetActorTransform();
	FVector AimActor = FVector::ForwardVector;
	if (bHasAimPoint)
	{
		const FVector Shoulder = Body->DoesSocketExist(GBShieldLocal::RightShoulder) ? Body->GetSocketLocation(GBShieldLocal::RightShoulder) : GetActorLocation();
		AimActor = ActorTM.InverseTransformVectorNoScale(AimPoint - Shoulder).GetSafeNormal();
		FRotator Clamped = AimActor.Rotation();
		Clamped.Yaw = FMath::Clamp(FRotator::NormalizeAxis(Clamped.Yaw), -GBShieldLocal::MaxArmYaw, GBShieldLocal::MaxArmYaw);
		Clamped.Pitch = FMath::Clamp(Clamped.Pitch, -50.f, 50.f);
		AimActor = Clamped.Vector();
	}
	const float Blend = FMath::SmoothStep(0.f, 1.f, AimAlpha);
	const FVector LowActor = FVector(0.3f, 0.18f, -0.93f).GetSafeNormal();
	const FVector ArmActor = FMath::Lerp(LowActor, AimActor, Blend).GetSafeNormal();
	FGBArmIK Right;
	Right.Alpha = 1.f;
	Right.bFromShoulder = true;
	Right.Location = ToMeshDir(ArmActor) * FMath::Lerp(0.9f, 0.98f, Blend);
	Right.Pole = ToMeshDir(FVector(-0.3f, 1.f, -0.8f));
	Right.RecoilDegrees = GBShieldLocal::RecoilDegrees;
	if (WeaponComp)
	{
		// The pistol along the arm's aim (pointing down and forward while low), upright; the hand is turned so the grip gives that.
		const FQuat LowWeapon = FRotator(-40.f, 0.f, 0.f).Quaternion();
		const FQuat AimWeapon = FRotationMatrix::MakeFromXZ(AimActor, FVector::UpVector).ToQuat();
		const FQuat WeaponActor = FQuat::Slerp(LowWeapon, AimWeapon, Blend);
		const FQuat WeaponMesh = MeshRel.InverseTransformRotation(WeaponActor);
		Right.bRotation = true;
		Right.Rotation = (WeaponMesh * WeaponComp->GetRelativeRotation().Quaternion().Inverse()).GetNormalized();
	}
	AnimInst->SetArmIK(1, Right);

	// The bash leans him into the shield.
	const float Push = ComputeBashPush();
	if (Push > 0.f)
	{
		AnimInst->SetAim(-12.f * Push / FMath::Max(S(TEXT("BashPush"), 40.f) * Scale, 1.f), 0.f);
	}
}

void AGBShieldEnemy::Tick(float DeltaSeconds)
{
	// Real time, also while he is stunned (the parent's tick returns early then).
	StaggerTimer = FMath::Max(0.f, StaggerTimer - DeltaSeconds);
	Super::Tick(DeltaSeconds);
	if (State != EGBEnemyState::Dead)
	{
		UpdateShieldVisuals(DeltaSeconds);
	}
}

void AGBShieldEnemy::DebugSetPose(const FString& Spec)
{
	if (AnimInst)
	{
		AnimInst->SetPoseLayer(Spec);
	}
}

void AGBShieldEnemy::DebugSetShield(bool bUp)
{
	bShieldUp = bUp;
	StaggerTimer = 0.f;
	UpdateShieldVisuals(0.f);   // the hit box follows at once (the smoke traces right after)
}

void AGBShieldEnemy::ApplyControlLock(float Seconds)
{
	Super::ApplyControlLock(Seconds);
	if (State != EGBEnemyState::Dead)
	{
		// A stun or a freeze knocks the shield aside: the opening the player earns.
		StaggerTimer = FMath::Max(StaggerTimer, FMath::Max(Seconds, S(TEXT("StaggerTime"), 2.5f)));
		Phase = EChiefPhase::Approach;
		UpdateShieldVisuals();
	}
}

void AGBShieldEnemy::HandleIncomingDamage(FGBDamageInfo& Info)
{
	if (!IsShieldBlocking() || Info.Amount <= 0.f)
	{
		return;
	}
	FVector Direction = Info.HitDirection;
	if (Direction.IsNearlyZero() && Info.Instigator)
	{
		Direction = GetActorLocation() - Info.Instigator->GetActorLocation();
	}
	// Where the damage comes from, flat: inside the frontal arc of the shield it is blocked.
	const FVector From = (-Direction).GetSafeNormal2D();
	if (From.IsNearlyZero())
	{
		return;
	}
	const float HalfArc = FMath::DegreesToRadians(S(TEXT("ShieldArc"), 130.f) * 0.5f);
	if (FVector::DotProduct(GetActorForwardVector().GetSafeNormal2D(), From) < FMath::Cos(HalfArc))
	{
		return;
	}

	++BlockedHits;
	if (ShotFx.HitImpact)
	{
		const FVector At = Info.HitLocation.IsNearlyZero() ? GetActorLocation() + GetActorForwardVector() * 100.f : Info.HitLocation;
		GBEnemy3D::SpawnFx(this, ShotFx.HitImpact, At, (-Direction).Rotation());
	}
	const float BlockedRecently = GBTactics::AddBlockedHit(BlockedLog, GetWorld()->GetTimeSeconds(), Info.Amount, GBShieldLocal::BlockedWindowSeconds);
	Info.Amount *= FMath::Max(0.f, S(TEXT("ShieldDamageTaken"), 0.f));
	if (BlockedRecently >= S(TEXT("ShieldStaggerDamage"), 160.f))
	{
		BlockedLog.Reset();
		StaggerTimer = S(TEXT("StaggerTime"), 2.5f);
		Phase = EChiefPhase::Approach;
		UpdateShieldVisuals();
	}
}

bool AGBShieldEnemy::TickRanged(AActor* Goal, float DeltaSeconds)
{
	if (DebugHoldMode != 0)
	{
		// A held pose for the closeups: no behaviour, he stands and faces the player.
		bShieldUp = DebugHoldMode == 1 || DebugHoldMode == 3;
		AimPoint = GBEnemy3D::TargetChest(Goal);
		bHasAimPoint = true;
		StaggerTimer = 0.f;
		StopMoving();
		// He keeps the way he faces (the closeups walk round him).
		UpdateAnimAim(Goal, DebugHoldMode == 2);
		return true;
	}
	UGBSquadSubsystem* Squad = UGBSquadSubsystem::Get(this);
	bSight = bSightCached;
	const FVector Me = GetActorLocation();
	const FVector GoalPos = Goal->GetActorLocation();
	const float Distance = FVector::Dist(Me, GoalPos);
	FVector Known = GoalPos;
	if (!bSight)
	{
		float Age = 0.f;
		if (!(Squad && Squad->GetLastKnown(this, Known, Age)))
		{
			Known = GoalPos;
		}
	}
	if (!bChiefAware)
	{
		bChiefAware = true;
		AttackCooldown = S(TEXT("FirstShotDelay"), 1.0f);
		Phase = EChiefPhase::Approach;
		bShieldUp = true;
	}
	AimPoint = bSight ? GBEnemy3D::TargetChest(Goal) : Known + FVector(0.f, 0.f, 60.f);
	bHasAimPoint = true;
	AttackCooldown = FMath::Max(0.f, AttackCooldown - DeltaSeconds);
	BashCooldown = FMath::Max(0.f, BashCooldown - DeltaSeconds);

	// Always facing him, but slowly: a player who runs round him gets his side.
	const FVector ToTarget = ((bSight ? GoalPos : Known) - Me).GetSafeNormal2D();
	if (!ToTarget.IsNearlyZero())
	{
		TurnYawTowards(ToTarget.Rotation().Yaw, DeltaSeconds, S(TEXT("TurnRate"), 110.f));
	}

	const float AttackRange = Stats->GetStat(GBStat::AttackRange, 0.f);
	const float RangeMin = S(TEXT("PreferredRangeMin"), 500.f);
	const float RangeMax = FMath::Max(S(TEXT("PreferredRangeMax"), 1800.f), RangeMin);
	const float MeleeRange = S(TEXT("MeleeRange"), 300.f);

	// A lightning interrupt cancels the aim.
	if (Phase == EChiefPhase::Aim && State != EGBEnemyState::Windup)
	{
		Phase = EChiefPhase::Approach;
		bShieldUp = true;
		AttackCooldown = Stats->GetStat(GBStat::AttackInterval, 3.5f) * 0.5f;
	}

	switch (Phase)
	{
	case EChiefPhase::Approach:
	{
		bShieldUp = true;
		SetGait(EGBGait::Combat);
		if (Distance <= MeleeRange && bSight && BashCooldown <= 0.f)
		{
			Phase = EChiefPhase::Bash;
			PhaseTimer = S(TEXT("BashWindup"), 0.45f);
			StopMoving();
		}
		else if (Distance <= FMath::Min(AttackRange, RangeMax) && Distance > MeleeRange && bSight && AttackCooldown <= 0.f && Stats->GetStat(GBStat::AttackDamage, 0.f) > 0.f)
		{
			// Lowers the shield to aim: the long telegraph.
			Phase = EChiefPhase::Aim;
			PhaseTimer = Stats->GetStat(GBStat::AttackWindup, 1.3f);
			bShieldUp = false;
			StopMoving();
			SetState(EGBEnemyState::Windup);
		}
		else if (Distance > RangeMin || !bSight)
		{
			MoveToPoint(bSight ? GoalPos : Known, Goal, DeltaSeconds);
		}
		else if (GetVelocity().Size2D() > 10.f)
		{
			StopMoving();
		}
		UpdateAnimAim(Goal, false);
		break;
	}

	case EChiefPhase::Aim:
		bShieldUp = false;
		PhaseTimer -= DeltaSeconds;
		StopMoving();
		UpdateAnimAim(Goal, true);
		if (!bSight && PhaseTimer > 0.3f)
		{
			Phase = EChiefPhase::Approach;
			bShieldUp = true;
			AttackCooldown = 0.5f;
			SetState(EGBEnemyState::Chase);
		}
		else if (PhaseTimer <= 0.f)
		{
			ShotTimes.Add(GetWorld()->GetTimeSeconds());
			ShotsWithShieldUp += bShieldUp ? 1 : 0;
			FireShot(Goal, GBEnemy3D::TargetChest(Goal), 0.f);
			++BurstCount;
			Phase = EChiefPhase::Open;
			PhaseTimer = S(TEXT("ShieldRecover"), 0.8f);
			SetState(EGBEnemyState::Recover);
		}
		break;

	case EChiefPhase::Open:
		bShieldUp = false;
		PhaseTimer -= DeltaSeconds;
		UpdateAnimAim(Goal, true);
		if (PhaseTimer <= 0.f)
		{
			Phase = EChiefPhase::Approach;
			bShieldUp = true;
			AttackCooldown = Stats->GetStat(GBStat::AttackInterval, 3.5f);
			SetState(EGBEnemyState::Chase);
		}
		break;

	case EChiefPhase::Bash:
		PhaseTimer -= DeltaSeconds;
		StopMoving();
		UpdateAnimAim(Goal, false);
		if (PhaseTimer <= 0.f)
		{
			if (Distance <= MeleeRange * 1.2f)
			{
				FGBDamageInfo Info = MakeShotInfo();
				Info.Amount *= S(TEXT("MeleeDamageMult"), 1.5f);
				Info.HitLocation = GoalPos;
				Info.HitDirection = (GoalPos - Me).GetSafeNormal();
				UGBCombatLibrary::DealDamage(Goal, Info);
				// A short shove: the player is launched away from him (stat BashKnockback, cm/s).
				const float Knockback = S(TEXT("BashKnockback"), 0.f);
				if (Knockback > 0.f)
				{
					if (ACharacter* PlayerChar = Cast<ACharacter>(Goal))
					{
						PlayerChar->LaunchCharacter(Info.HitDirection.GetSafeNormal2D() * Knockback + FVector(0.f, 0.f, Knockback * 0.2f), true, false);
					}
				}
			}
			BashCooldown = S(TEXT("BashInterval"), 2.f);
			BashRecover = GBShieldLocal::BashRecoverSeconds;
			Phase = EChiefPhase::Approach;
		}
		break;
	}
	return true;
}

void AGBShieldEnemy::HandleDeath(const FGBDamageInfo& KillingBlow)
{
	Super::HandleDeath(KillingBlow);
	ShieldBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	bShieldUp = false;
	if (AnimInst)
	{
		AnimInst->SetArmIK(0, FGBArmIK());
		AnimInst->SetArmIK(1, FGBArmIK());
	}
	// The body falls as a ragdoll: the shield goes with the hand that held it instead of staying in the air.
	if (USkeletalMeshComponent* Body = GetMesh())
	{
		const FGBEnemyDefinition* Def = GetDefinition();
		const FName Socket(Def ? *GBEnemy3D::Str(*Def, FName(TEXT("ShieldSocket")), TEXT("hand_l")) : TEXT("hand_l"));
		ShieldMesh->AttachToComponent(Body, FAttachmentTransformRules::KeepWorldTransform, Socket);
	}
}

FString AGBShieldEnemy::DescribeTactics() const
{
	static const TCHAR* const PhaseNames[] = { TEXT("Approach"), TEXT("Aim"), TEXT("Open"), TEXT("Bash") };
	return FString::Printf(TEXT("Anchor %s shield=%d stagger=%.1f blocked=%d shots=%d shotsWithShieldUp=%d"), PhaseNames[static_cast<int32>(Phase)], bShieldUp ? 1 : 0, StaggerTimer, BlockedHits, ShotCount, ShotsWithShieldUp);
}
