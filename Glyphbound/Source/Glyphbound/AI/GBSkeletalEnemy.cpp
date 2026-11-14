#include "AI/GBSkeletalEnemy.h"

#include "AIController.h"
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"
#include "Animation/AnimSequence.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/PhysicsAsset.h"

#include "AI/GBEngage.h"
#include "Combat/GBCombatLibrary.h"
#include "Core/GBStatNames.h"
#include "Stats/GBAttributeComponent.h"
#include "Data/GBDataSubsystem.h"
#include "Stats/GBStatsComponent.h"
#include "World/GBEraComponent.h"

namespace GBSkeletalEnemyLocal
{
	// Technical defaults for stats that Enemies.json normally sets (the numbers live in the JSON).
	const TCHAR* const DefaultMuzzleFx = TEXT("/Game/MuzzleFlash3D/FX/Futuristic/NS_MuzzleFlash_Single_Futuristic");
	const TCHAR* const DefaultTracerFx = TEXT("/Game/SciFi_Impacts/FX/NS_Tracer_Red");
	const TCHAR* const DefaultImpactFx = TEXT("/Game/GunFX/NiagaraSystem/ImpactFX/Concrete/NS_Impact_Concrete_Low");
	const FName MuzzleSocket(TEXT("Muzzle"));
	const FName TipSocket(TEXT("Tip"));
	const FName RagdollProfile(TEXT("Ragdoll"));
	const FName PhysicsActorProfile(TEXT("PhysicsActor"));
	const TCHAR* const ClipKeys[GBEnemyAnim::NumClips] = { TEXT("Anim.Idle"), TEXT("Anim.Walk"), TEXT("Anim.Run"), TEXT("Anim.StrafeLeft"), TEXT("Anim.StrafeRight"),
		TEXT("Anim.WalkBack"), TEXT("Anim.Attack"), TEXT("Anim.Hit"), TEXT("Anim.Death"),
		TEXT("Anim.RelaxedIdle"), TEXT("Anim.PatrolWalk"), TEXT("Anim.Sprint"), TEXT("Anim.CrouchIdle"), TEXT("Anim.CrouchWalk") };
	constexpr float SightInterval = 0.1f;          // s between line-of-sight traces
	constexpr float StrafeReachRadius = 80.f;      // cm: a strafe goal this close counts as reached
	constexpr float StrafeNavExtentXY = 150.f;
	constexpr float StrafeNavExtentZ = 300.f;
	constexpr int32 StrafeTries = 6;
	constexpr float RetreatProbe = 250.f;          // cm checked behind a too-close enemy before it backs off
	constexpr float RetreatStep = 500.f;           // cm of the retreat goal
	constexpr float MaxAimPitch = 50.f;            // degrees
}

AGBSkeletalEnemy::AGBSkeletalEnemy()
{
	// The default skeletal mesh component of ACharacter is the body; the parent's placeholder shape stays until the art loads.
	if (USkeletalMeshComponent* Skeletal = GetMesh())
	{
		Skeletal->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Skeletal->SetCanEverAffectNavigation(false);
		Skeletal->SetGenerateOverlapEvents(false);
	}
}

void AGBSkeletalEnemy::InitFromDefinition(const FGBEnemyDefinition& Definition)
{
	Super::InitFromDefinition(Definition);
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		// Ranged shooters turn by their own rules (they face the player, or the way they run a long move); melee ones turn the way they run.
		Move->bOrientRotationToMovement = !IsRanged();
		Move->RotationRate = FRotator(0.f, FMath::Max(StatOr("TurnRate", 540.f), 1.f), 0.f);
	}
}

void AGBSkeletalEnemy::BeginPlay()
{
	Super::BeginPlay();
	Random.Initialize(static_cast<int32>(GetUniqueID()) ^ FMath::Rand());
	BuildLook();
}

void AGBSkeletalEnemy::BuildLook()
{
	const FGBEnemyDefinition* Def = GetDefinition();
	USkeletalMeshComponent* Body = GetMesh();
	if (!Def || !Body || bLookBuilt)
	{
		return;
	}
	GBEnemy3D::LoadShotFx(ShotFx, *Def, EnemyId, GBSkeletalEnemyLocal::DefaultMuzzleFx, GBSkeletalEnemyLocal::DefaultTracerFx, GBSkeletalEnemyLocal::DefaultImpactFx);

	USkeletalMesh* SK = GBEnemy3D::LoadAsset<USkeletalMesh>(GBEnemy3D::Str(*Def, GBEnemy3DKey::Mesh), EnemyId);
	if (!SK)
	{
		Body->SetVisibility(false);
		return;
	}

	const float HalfHeight = GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
	const float MeshScale = S(TEXT("MeshScale"), 1.f);
	const float MeshYaw = S(TEXT("MeshYaw"), -90.f);

	Body->SetSkeletalMeshAsset(SK);
	Body->SetRelativeLocation(FVector(0.f, 0.f, -HalfHeight));
	Body->SetRelativeRotation(FRotator(0.f, MeshYaw, 0.f));
	Body->SetRelativeScale3D(FVector(MeshScale));
	// The animation must run even when the camera does not see the enemy (the shots leave from an animated hand).
	Body->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	Body->bEnableUpdateRateOptimizations = false;
	Body->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	Body->SetAnimInstanceClass(UGBEnemyAnimInstance::StaticClass());
	Body->SetVisibility(true);
	BodyMesh->SetVisibility(false);
	MeshName = SK->GetName();

	// Armor: more skeletal meshes on the same skeleton, following the body's pose.
	const FString ArmorList = GBEnemy3D::Str(*Def, GBEnemy3DKey::Armor);
	TArray<FString> ArmorPaths;
	ArmorList.ParseIntoArray(ArmorPaths, TEXT(";"), true);
	for (FString& Path : ArmorPaths)
	{
		Path.TrimStartAndEndInline();
		USkeletalMesh* Piece = GBEnemy3D::LoadAsset<USkeletalMesh>(Path, EnemyId);
		if (!Piece)
		{
			continue;
		}
		USkeletalMeshComponent* Part = NewObject<USkeletalMeshComponent>(this);
		Part->SetSkeletalMeshAsset(Piece);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetCanEverAffectNavigation(false);
		Part->SetGenerateOverlapEvents(false);
		Part->AttachToComponent(Body, FAttachmentTransformRules::SnapToTargetIncludingScale);
		Part->SetLeaderPoseComponent(Body);
		Part->RegisterComponent();
		ArmorParts.Add(Part);
	}
	if (S(TEXT("ArmorHideBody"), 0.f) > 0.5f && ArmorParts.Num() > 0)
	{
		Body->SetVisibility(false, false);
	}

	// Weapon on a hand socket.
	if (UStaticMesh* WeaponMesh = GBEnemy3D::LoadAsset<UStaticMesh>(GBEnemy3D::Str(*Def, GBEnemy3DKey::Weapon), EnemyId))
	{
		WeaponComp = NewObject<UStaticMeshComponent>(this);
		WeaponComp->SetStaticMesh(WeaponMesh);
		WeaponComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		WeaponComp->SetCanEverAffectNavigation(false);
		WeaponComp->SetGenerateOverlapEvents(false);
		const FName Socket(*GBEnemy3D::Str(*Def, GBEnemy3DKey::WeaponSocket, TEXT("hand_r")));
		WeaponComp->AttachToComponent(Body, FAttachmentTransformRules::SnapToTargetNotIncludingScale, Socket);
		WeaponComp->SetRelativeTransform(GBEnemy3D::ParseOffset(GBEnemy3D::Str(*Def, GBEnemy3DKey::WeaponOffset)));
		WeaponComp->RegisterComponent();
		// The shot leaves from the weapon's Muzzle socket, or its Tip socket (a sword).
		for (const FName& Candidate : { GBSkeletalEnemyLocal::MuzzleSocket, GBSkeletalEnemyLocal::TipSocket })
		{
			if (!bHasMuzzleSocket && WeaponComp->DoesSocketExist(Candidate))
			{
				bHasMuzzleSocket = true;
				MuzzleSocketName = Candidate;
			}
		}
	}

	// Animation clips and tuning.
	AnimInst = Cast<UGBEnemyAnimInstance>(Body->GetAnimInstance());
	if (AnimInst)
	{
		TArray<UAnimSequence*> Clips;
		for (int32 Index = 0; Index < GBEnemyAnim::NumClips; ++Index)
		{
			Clips.Add(GBEnemy3D::LoadAsset<UAnimSequence>(GBEnemy3D::Str(*Def, FName(GBSkeletalEnemyLocal::ClipKeys[Index])), EnemyId));
		}
		FGBEnemyAnimTuning Tuning;
		Tuning.WalkAnimSpeed = S(TEXT("WalkAnimSpeed"), Tuning.WalkAnimSpeed);
		Tuning.RunAnimSpeed = S(TEXT("RunAnimSpeed"), Tuning.RunAnimSpeed);
		Tuning.BlendTime = S(TEXT("AnimBlendTime"), Tuning.BlendTime);
		Tuning.OverlayBlend = S(TEXT("AnimOverlayBlend"), Tuning.OverlayBlend);
		Tuning.AimSmooth = S(TEXT("AnimAimSmooth"), Tuning.AimSmooth);
		Tuning.RecoilPitch = S(TEXT("AnimRecoilPitch"), Tuning.RecoilPitch);
		Tuning.RecoilDecay = S(TEXT("AnimRecoilDecay"), Tuning.RecoilDecay);
		Tuning.FlinchPitch = S(TEXT("AnimFlinchPitch"), Tuning.FlinchPitch);
		Tuning.FlinchYaw = S(TEXT("AnimFlinchYaw"), Tuning.FlinchYaw);
		Tuning.FlinchDecay = S(TEXT("AnimFlinchDecay"), Tuning.FlinchDecay);
		Tuning.MeshYaw = MeshYaw;
		Tuning.MeshScale = MeshScale;
		Tuning.AnimRateMin = S(TEXT("AnimRateMin"), Tuning.AnimRateMin);
		Tuning.AnimRateMax = S(TEXT("AnimRateMax"), Tuning.AnimRateMax);
		Tuning.MoveBlendSpeed = S(TEXT("AnimMoveBlendSpeed"), Tuning.MoveBlendSpeed);
		Tuning.TravelBone = FName(*GBEnemy3D::Str(*Def, FName(TEXT("Anim.TravelBone")), TEXT("pelvis")));
		AnimInst->Configure(Clips, Tuning);
	}
	else
	{
		UE_LOG(LogGB, Display, TEXT("ENEMIES3D %s: the native animation instance did not start (the mesh stays in its reference pose)"), *EnemyId.ToString());
	}
	bLookBuilt = true;
}

FVector AGBSkeletalEnemy::GetMuzzleLocation() const
{
	if (WeaponComp && WeaponComp->GetStaticMesh())
	{
		if (bHasMuzzleSocket)
		{
			return WeaponComp->GetSocketLocation(MuzzleSocketName);
		}
		const FBoxSphereBounds Bounds = WeaponComp->GetStaticMesh()->GetBounds();
		return WeaponComp->GetComponentTransform().TransformPosition(Bounds.Origin + FVector(Bounds.BoxExtent.X, 0.f, 0.f));
	}
	return GetChestLocation() + GetActorForwardVector() * (GetCapsuleComponent()->GetScaledCapsuleRadius() + 20.f);
}

FString AGBSkeletalEnemy::DescribeLook() const
{
	FString Speeds;
	if (AnimInst)
	{
		Speeds = FString::Printf(TEXT(" ref[walk=%.0f%s run=%.0f%s]"), AnimInst->GetClipRefSpeed(EGBEnemyClip::Walk), AnimInst->ClipHasDrift(EGBEnemyClip::Walk) ? TEXT("d") : TEXT(""),
			AnimInst->GetClipRefSpeed(EGBEnemyClip::Run), AnimInst->ClipHasDrift(EGBEnemyClip::Run) ? TEXT("d") : TEXT(""));
	}
	return FString::Printf(TEXT("%s look=%d mesh=%s armor=%d weapon=%d muzzleSocket=%d anim[%s]%s aware=%d shots=%d bursts=%d lunges=%d ragdoll=%d maxPelvisDist=%.0f | %s"),
		*EnemyId.ToString(), bLookBuilt ? 1 : 0, MeshName.IsEmpty() ? TEXT("-") : *MeshName, ArmorParts.Num(), WeaponComp ? 1 : 0, bHasMuzzleSocket ? 1 : 0,
		AnimInst ? *AnimInst->DescribeClips() : TEXT("none"), *Speeds, bAware ? 1 : 0, ShotCount, BurstCount, LungeCount, bRagdoll ? 1 : 0, MaxPelvisDistance, *DescribeTactics());
}

FGBDamageInfo AGBSkeletalEnemy::MakeShotInfo() const
{
	FGBDamageInfo Info;
	if (const FGBEnemyDefinition* Def = GetDefinition())
	{
		Info.Element = Def->Element;
		Info.OnHitEffect = FName(*Def->Strings.FindRef(GBString::OnHitEffect));
	}
	Info.Amount = Stats->GetStat(GBStat::AttackDamage, 0.f);
	Info.Instigator = const_cast<AGBSkeletalEnemy*>(this);
	Info.Causer = const_cast<AGBSkeletalEnemy*>(this);
	Info.SourceId = EnemyId;
	return Info;
}

void AGBSkeletalEnemy::SetTelegraph(bool bOn)
{
	// With the real mesh the aim pose is the telegraph; the tinted placeholder only exists without the art.
	if (!bLookBuilt)
	{
		Super::SetTelegraph(bOn);
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Behaviour
// ---------------------------------------------------------------------------------------------------------------------

bool AGBSkeletalEnemy::TickCustomBehavior(AActor* Goal, float DeltaSeconds)
{
	if (IsRanged())
	{
		return TickRanged(Goal, DeltaSeconds);
	}
	TickLunge(Goal, DeltaSeconds);
	return false;
}

void AGBSkeletalEnemy::UpdateAnimAim(const AActor* Goal, bool bEngaged)
{
	if (!AnimInst)
	{
		return;
	}
	float Pitch = 0.f;
	if (bEngaged && Goal)
	{
		const FVector From = GetMuzzleLocation();
		const FVector To = GBEnemy3D::TargetChest(Goal) - From;
		Pitch = FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan2(To.Z, To.Size2D())), -GBSkeletalEnemyLocal::MaxAimPitch, GBSkeletalEnemyLocal::MaxAimPitch);
	}
	AnimInst->SetAim(Pitch, 0.f);
}

void AGBSkeletalEnemy::TurnTowards(const AActor* Goal, float DeltaSeconds)
{
	const FVector To = (Goal->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
	if (To.IsNearlyZero())
	{
		return;
	}
	const FRotator Current = GetActorRotation();
	const FRotator Wanted(0.f, To.Rotation().Yaw, 0.f);
	SetActorRotation(FMath::RInterpConstantTo(Current, Wanted, DeltaSeconds, S(TEXT("TurnRate"), 420.f)));
}

void AGBSkeletalEnemy::MoveToPoint(const FVector& Point, const AActor* Goal, float DeltaSeconds)
{
	MoveToZonePoint(Point, 40.f, Goal, DeltaSeconds);
}

void AGBSkeletalEnemy::AbortBurst(float Cooldown)
{
	BurstPhase = EBurstPhase::Cooldown;
	ShotsLeft = 0;
	BurstCooldown = FMath::Max(BurstCooldown, Cooldown);
	if (State == EGBEnemyState::Windup || State == EGBEnemyState::Recover)
	{
		SetState(EGBEnemyState::Chase);
	}
}

void AGBSkeletalEnemy::StartBurst()
{
	BurstPhase = EBurstPhase::Telegraph;
	BurstTimer = Stats->GetStat(GBStat::AttackWindup, 0.4f);
	SetState(EGBEnemyState::Windup);
}

bool AGBSkeletalEnemy::TickMeleeSwing(AActor* Goal, float DeltaSeconds, float Distance)
{
	const float MeleeRange = S(TEXT("MeleeRange"), 0.f);
	if (MeleeRange <= 0.f)
	{
		return false;
	}
	SwingCooldown = FMath::Max(0.f, SwingCooldown - DeltaSeconds);
	if (SwingTimer >= 0.f)
	{
		SwingTimer -= DeltaSeconds;
		StopMoving();
		FaceTowards(Goal);
		if (SwingTimer <= 0.f)
		{
			SwingTimer = -1.f;
			SwingCooldown = Stats->GetStat(GBStat::AttackInterval, 1.5f);
			if (Distance <= MeleeRange * 1.2f && bSight)
			{
				FGBDamageInfo Info = MakeShotInfo();
				Info.Amount *= S(TEXT("MeleeDamageMult"), 1.5f);
				Info.HitLocation = Goal->GetActorLocation();
				Info.HitDirection = (Goal->GetActorLocation() - GetActorLocation()).GetSafeNormal();
				UGBCombatLibrary::DealDamage(Goal, Info);
			}
		}
		return true;
	}
	if (Distance <= MeleeRange && bSight && SwingCooldown <= 0.f)
	{
		if (BurstPhase != EBurstPhase::Cooldown)
		{
			AbortBurst(0.5f);
		}
		SwingTimer = S(TEXT("MeleeWindup"), 0.45f);
		if (AnimInst)
		{
			AnimInst->PlayOneShot(EGBEnemyClip::Attack, false);
		}
		return true;
	}
	return false;
}

bool AGBSkeletalEnemy::TickRanged(AActor* Goal, float DeltaSeconds)
{
	using namespace GBSkeletalEnemyLocal;
	UGBSquadSubsystem* Squad = UGBSquadSubsystem::Get(this);
	bSight = bSightCached;
	const FVector Me = GetActorLocation();
	const FVector GoalPos = Goal->GetActorLocation();
	const float Distance = FVector::Dist(Me, GoalPos);

	// What it knows: the player while somebody sees him, otherwise the squad's last known position.
	FVector Known = GoalPos;
	float KnownAge = 0.f;
	if (!bSight)
	{
		if (!(Squad && Squad->GetLastKnown(this, Known, KnownAge)))
		{
			Known = GoalPos;
			KnownAge = 99.f;
		}
	}

	if (!bWasAware)
	{
		bWasAware = true;
		BurstPhase = EBurstPhase::Cooldown;
		BurstCooldown = S(TEXT("FirstShotDelay"), 0.4f);
		HealthWindowStart = -1.f;
		bHasPosition = false;
		bAwaitingPosition = false;
		RelocateCooldown = 0.f;
		CState = ECombatState::Hold;
	}
	BurstCooldown = FMath::Max(0.f, BurstCooldown - DeltaSeconds);
	RelocateCooldown = FMath::Max(0.f, RelocateCooldown - DeltaSeconds);

	if (TickMeleeSwing(Goal, DeltaSeconds, Distance))
	{
		UpdateAnimAim(Goal, true);
		return true;
	}

	const EGBSquadRole SquadRole = Squad ? Squad->GetRole(this) : EGBSquadRole::Suppress;
	const float AttackRange = Stats->GetStat(GBStat::AttackRange, 0.f);
	const bool bCanShoot = Stats->GetStat(GBStat::AttackDamage, 0.f) > 0.f && AttackRange > 0.f;
	const float RangeMin = S(TEXT("PreferredRangeMin"), 600.f);
	const float SuppressWindow = S(TEXT("SuppressWindow"), 8.f);

	// Hurt: 30 percent of the health in 2 seconds.
	const float Health = Attributes->GetHealth();
	const float MaxHealth = Attributes->GetMaxHealth();
	if (HealthWindowStart < 0.f)
	{
		HealthWindowStart = Health;
		HealthWindowTime = 0.f;
	}
	HealthWindowTime += DeltaSeconds;
	const bool bHurt = MaxHealth > 0.f && (HealthWindowStart - Health) >= 0.3f * MaxHealth;
	if (HealthWindowTime >= 2.f)
	{
		HealthWindowStart = Health;
		HealthWindowTime = 0.f;
	}

	FGBTacticalPosition TempPosition;
	TempPosition.Cover = Me;
	TempPosition.FiringSpot = Me;
	TempPosition.bValid = true;
	const FGBTacticalPosition& Pos = bHasPosition ? Position : TempPosition;
	const FVector CoverPoint = Pos.Cover;
	const FVector FiringPoint = Pos.FiringSpot;
	const float DistCover = FVector::Dist2D(Me, CoverPoint);
	const bool bAtCover = DistCover < 90.f;

	// ---- When to ask for a new position ----
	bool bRelocate = !bHasPosition;
	if (Squad && Squad->TakeRoleChanged(this))
	{
		bRelocate = true;
	}
	if (bHasPosition && CState != ECombatState::MoveToPosition)
	{
		StayTimer -= DeltaSeconds;
		bRelocate |= StayTimer <= 0.f;
		bRelocate |= bHurt;
		// The cover is compromised when he has a clear line to the cover point itself for too long.
		if (Pos.bCovered && CState == ECombatState::Hold && bAtCover && bSight)
		{
			CoverCompromisedTimer += DeltaSeconds;
		}
		else
		{
			CoverCompromisedTimer = 0.f;
		}
		bRelocate |= CoverCompromisedTimer >= S(TEXT("CoverCompromisedTime"), 1.2f);
		bRelocate |= Distance < RangeMin * 0.9f && CState == ECombatState::Hold;
		if (CState == ECombatState::Peek && !bSight && KnownAge < 3.f)
		{
			NoLineTimer += DeltaSeconds;
		}
		else if (bSight)
		{
			NoLineTimer = 0.f;
		}
		bRelocate |= NoLineTimer >= S(TEXT("NoLineTime"), 3.f);
		// Nobody has seen him for a while: everyone but the base of fire advances in bounds to see the last known position.
		if (!bSight && KnownAge > S(TEXT("SearchDelay"), 4.f) && SquadRole != EGBSquadRole::Suppress && SquadRole != EGBSquadRole::Anchor)
		{
			SearchTimer += DeltaSeconds;
			bRelocate |= SearchTimer >= S(TEXT("SearchRelocateInterval"), 4.f);
		}
		else
		{
			SearchTimer = 0.f;
		}
	}
	if (bRelocate && !bAwaitingPosition && RelocateCooldown <= 0.f && BurstPhase == EBurstPhase::Cooldown && Squad)
	{
		Squad->ReleaseToken(this);
		Squad->RequestPosition(this);
		bAwaitingPosition = true;
		RelocateCooldown = S(TEXT("RelocateCooldown"), 1.5f);
		CoverCompromisedTimer = 0.f;
		NoLineTimer = 0.f;
		SearchTimer = 0.f;
		++Relocations;
	}
	if (bAwaitingPosition && Squad)
	{
		FGBTacticalPosition Assigned;
		if (Squad->TakeAssignedPosition(this, Assigned))
		{
			bAwaitingPosition = false;
			// A burst that started while the answer was on its way does not go on in the open while it moves.
			if (BurstPhase != EBurstPhase::Cooldown)
			{
				AbortBurst(0.5f);
			}
			Squad->ReleaseToken(this);
			if (Assigned.bValid)
			{
				Position = Assigned;
			}
			else
			{
				// Nothing passed: hold in place and fire from the open.
				Position = TempPosition;
				Position.Cover = GetActorLocation();
				Position.FiringSpot = Position.Cover;
			}
			bHasPosition = true;
			CState = ECombatState::MoveToPosition;
			MoveTimer = 0.f;
			BurstsDone = 0;
			const float StayMin = S(TEXT("CoverStayMin"), 6.f);
			StayTimer = Random.FRandRange(StayMin, FMath::Max(S(TEXT("CoverStayMax"), 12.f), StayMin));
			return true;
		}
	}
	if (!Squad)
	{
		bHasPosition = true;
		Position = TempPosition;
		StayTimer = 1.0e6f;
	}

	// Which way it faces while it moves: where it runs on a long move, the player (or where it thinks he is) on a short one.
	auto FaceWhileMoving = [&](bool bRunning)
	{
		const FVector Velocity = GetVelocity();
		if ((bRunning || !bSight) && Velocity.Size2D() > 25.f)
		{
			TurnYawTowards(Velocity.Rotation().Yaw, DeltaSeconds, S(TEXT("TurnRate"), 420.f));
		}
		else
		{
			const FVector To = (Known - GetActorLocation()).GetSafeNormal2D();
			if (!To.IsNearlyZero())
			{
				TurnYawTowards(To.Rotation().Yaw, DeltaSeconds, S(TEXT("TurnRate"), 420.f));
			}
		}
	};

	const FVector AimPoint = bSight ? GBEnemy3D::TargetChest(Goal) : Known + FVector(0.f, 0.f, 60.f);
	const bool bCanAim = bSight || (SquadRole == EGBSquadRole::Suppress && KnownAge < SuppressWindow);
	const float BurstsPerPeek = FMath::Clamp(FMath::RoundToFloat(S(TEXT("BurstsPerPeek"), 1.f)), 1.f, 2.f);

	switch (CState)
	{
	case ECombatState::MoveToPosition:
	{
		MoveTimer += DeltaSeconds;
		if (DistCover < 70.f || MoveTimer > S(TEXT("MoveTimeout"), 14.f))
		{
			StopMoving();
			CState = ECombatState::Hold;
			break;
		}
		const bool bFar = DistCover > S(TEXT("RunDistance"), 700.f);
		SetGait(bFar ? EGBGait::Run : EGBGait::Combat);
		MoveToPoint(CoverPoint, Goal, DeltaSeconds);
		FaceWhileMoving(bFar);
		UpdateAnimAim(Goal, !bFar && bSight);
		break;
	}

	case ECombatState::Hold:
	{
		SetGait(EGBGait::Combat);
		bCrouchPose = bAtCover && Pos.bCovered && CanCrouchInCover();
		if (!bAtCover)
		{
			MoveToPoint(CoverPoint, Goal, DeltaSeconds);
			FaceWhileMoving(false);
		}
		else
		{
			if (GetVelocity().Size2D() > 10.f)
			{
				StopMoving();
			}
			FaceWhileMoving(false);
			if (Pos.bCovered)
			{
				CoverTime += DeltaSeconds;
			}
		}
		UpdateAnimAim(Goal, bAtCover && bSight);
		const bool bHasTarget = bSight || KnownAge < SuppressWindow;
		if (bAtCover && bCanShoot && !bAwaitingPosition && BurstCooldown <= 0.f && bHasTarget && Distance <= AttackRange * 1.05f)
		{
			if (!Squad || Squad->TryAcquireToken(this, 1.f))
			{
				CState = ECombatState::Peek;
				PeekTimer = 0.f;
				BurstsDone = 0;
			}
		}
		break;
	}

	case ECombatState::Peek:
	{
		PeekTimer += DeltaSeconds;
		const float DistFire = FVector::Dist2D(Me, FiringPoint);
		const bool bSameSpot = FVector::Dist2D(CoverPoint, FiringPoint) < 30.f;
		const bool bAtFire = DistFire < 45.f || (bSameSpot && bAtCover);
		SetGait(EGBGait::Combat);
		if (BurstPhase == EBurstPhase::Cooldown && !bAtFire)
		{
			MoveToPoint(FiringPoint, Goal, DeltaSeconds);
			FaceWhileMoving(false);
		}
		else
		{
			if (GetVelocity().Size2D() > 10.f)
			{
				StopMoving();
			}
			TurnTowards(Goal, DeltaSeconds);
		}
		UpdateAnimAim(Goal, true);

		if (bAtFire)
		{
			if (BurstPhase == EBurstPhase::Cooldown && BurstsDone < static_cast<int32>(BurstsPerPeek) && bCanAim && bCanShoot && Distance <= AttackRange * 1.05f)
			{
				StartBurst();
			}
			TickBurst(Goal, DeltaSeconds, bCanAim, AimPoint);
		}
		else if (BurstPhase != EBurstPhase::Cooldown)
		{
			AbortBurst(0.5f);
		}
		if (BurstPhase == EBurstPhase::Cooldown && (BurstsDone >= static_cast<int32>(BurstsPerPeek) || PeekTimer > S(TEXT("PeekTimeout"), 3.5f)))
		{
			if (Squad)
			{
				Squad->ReleaseToken(this);
			}
			const float RoleMult = SquadRole == EGBSquadRole::Suppress ? 0.8f : ((SquadRole == EGBSquadRole::FlankLeft || SquadRole == EGBSquadRole::FlankRight) ? 1.2f : 1.f);
			BurstCooldown = BurstsDone > 0 ? Stats->GetStat(GBStat::AttackInterval, 1.f) * RoleMult : 1.f;
			CState = ECombatState::Hold;
		}
		break;
	}
	}
	return true;
}

void AGBSkeletalEnemy::TickBurst(AActor* Goal, float DeltaSeconds, bool bCanAim, const FVector& AimPoint)
{
	// A lightning interrupt (the parent's InterruptAttack) moved the state out of Windup: the telegraph is cancelled.
	if (BurstPhase == EBurstPhase::Telegraph && State != EGBEnemyState::Windup)
	{
		AbortBurst(Stats->GetStat(GBStat::AttackInterval, 1.f));
		return;
	}

	switch (BurstPhase)
	{
	case EBurstPhase::Cooldown:
		break;

	case EBurstPhase::Telegraph:
		BurstTimer -= DeltaSeconds;
		if (!bCanAim)
		{
			AbortBurst(0.3f);
		}
		else if (BurstTimer <= 0.f)
		{
			BurstPhase = EBurstPhase::Firing;
			ShotsLeft = FMath::Max(1, FMath::RoundToInt(S(TEXT("BurstCount"), 3.f)));
			ShotTimer = 0.f;
			++BurstCount;
			bBurstBlind = !bSight;
			if (CState == ECombatState::Peek)
			{
				++PeekBursts;
			}
			SetState(EGBEnemyState::Recover);
		}
		break;

	case EBurstPhase::Firing:
		ShotTimer -= DeltaSeconds;
		if (!bCanAim)
		{
			ShotsLeft = 0;
		}
		else if (ShotTimer <= 0.f && ShotsLeft > 0)
		{
			FireShot(Goal, AimPoint, bBurstBlind ? S(TEXT("BlindSpreadDegrees"), 4.f) : 0.f);
			--ShotsLeft;
			ShotTimer = S(TEXT("BurstShotInterval"), 0.12f);
		}
		if (ShotsLeft <= 0)
		{
			BurstPhase = EBurstPhase::Cooldown;
			++BurstsDone;
			SetState(EGBEnemyState::Chase);
		}
		break;
	}
}

void AGBSkeletalEnemy::FireShot(AActor* Goal, const FVector& AimPoint, float ExtraSpread)
{
	const FVector Muzzle = GetMuzzleLocation();
	FVector Direction = (AimPoint - Muzzle).GetSafeNormal();
	if (Direction.IsNearlyZero())
	{
		Direction = GetActorForwardVector();
	}
	// Spread: a base cone, wider while the player runs (he is harder to lead).
	float PlayerSpeed = 0.f;
	float PlayerFullSpeed = 0.f;
	if (const ACharacter* PlayerChar = Cast<ACharacter>(Goal))
	{
		PlayerSpeed = PlayerChar->GetCharacterMovement()->Velocity.Size2D();
		PlayerFullSpeed = PlayerChar->GetCharacterMovement()->MaxWalkSpeed;
	}
	const float Spread = UGBCombatLibrary::ComputeMovementSpread(S(TEXT("SpreadDegrees"), 4.f), S(TEXT("MoveSpreadDegrees"), 3.f), PlayerSpeed, PlayerFullSpeed) + ExtraSpread;
	Direction = UGBCombatLibrary::ApplySpread(Direction, Spread, Random);

	const float Range = Stats->GetStat(GBStat::AggroRange, 3000.f) * GetGameValue(GBEnemyKey::ProjectileRangeMult, 1.5f);
	GBEnemy3D::FireHitscan(this, Muzzle, Direction, Range, MakeShotInfo(), ShotFx, Goal);
	++ShotCount;
	if (AnimInst)
	{
		AnimInst->NotifyShot();
	}
}

float AGBSkeletalEnemy::GetPelvisDistance() const
{
	const USkeletalMeshComponent* Body = GetMesh();
	const FName Pelvis(TEXT("pelvis"));
	if (!Body || !Body->GetSkeletalMeshAsset() || Body->GetBoneIndex(Pelvis) == INDEX_NONE)
	{
		return -1.f;
	}
	return FVector::Dist2D(Body->GetBoneLocation(Pelvis, EBoneSpaces::WorldSpace), GetActorLocation());
}

bool AGBSkeletalEnemy::bTrackPelvis = false;

void AGBSkeletalEnemy::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bHoldsMeleeSlot && !IsMeleeCommitted())
	{
		bHoldsMeleeSlot = false;   // the windup and the lunge are over (or it died / lost him): the place goes back
		if (UGBSquadSubsystem* Squad = UGBSquadSubsystem::Get(this))
		{
			Squad->EndMeleeAttack(this);
		}
	}
	if (State == EGBEnemyState::Dead || !EraComponent->IsPresent())
	{
		return;
	}
	if (AnimInst)
	{
		AnimInst->SetPosture(ForcedPosture == 1 ? true : (ForcedPosture == 2 ? false : !bAware), ForcedPosture == 2 ? true : (bCrouchPose && bAware));
		if (!bAware)
		{
			AnimInst->SetAim(0.f, 0.f);
		}
	}
	if (bTrackPelvis && bLookBuilt && !bRagdoll)
	{
		MaxPelvisDistance = FMath::Max(MaxPelvisDistance, GetPelvisDistance());
	}
	// The crouched pose: in covered hold only (the cover logic sets it each tick it applies).
	bCrouchPose = bCrouchPose && CState == ECombatState::Hold;
}

bool AGBSkeletalEnemy::TryStartMelee()
{
	if (IsRanged() || AlwaysMeleeAttacks())
	{
		return true;
	}
	UGBSquadSubsystem* Squad = UGBSquadSubsystem::Get(this);
	if (!Squad)
	{
		return true;
	}
	// Asked at the moment of every windup / lunge start; the place is held until the windup and the lunge are over.
	if (Squad->TryBeginMeleeAttack(this))
	{
		bHoldsMeleeSlot = true;
		return true;
	}
	return false;
}

bool AGBSkeletalEnemy::GetMoveGoalOverride(FVector& OutPoint)
{
	if (IsRanged() || !bAware || !IsReadyToFight())
	{
		return false;
	}
	UGBSquadSubsystem* Squad = UGBSquadSubsystem::Get(this);
	bool bMayAttack = false;
	FVector Slot;
	if (Squad && Squad->GetMeleeSlot(this, Slot, bMayAttack))
	{
		(void)bMayAttack;   // the attack permission itself is asked at the windup / lunge start (TryStartMelee)
		OutPoint = Slot;
		return true;
	}
	return false;
}

void AGBSkeletalEnemy::OnReturnedToUnaware()
{
	bWasAware = false;
	CState = ECombatState::Hold;
	bHasPosition = false;
	bAwaitingPosition = false;
	BurstPhase = EBurstPhase::Cooldown;
	bCrouchPose = false;
	if (UGBSquadSubsystem* Squad = UGBSquadSubsystem::Get(this))
	{
		Squad->ReleaseToken(this);
	}
}

void AGBSkeletalEnemy::OnBecameAware(bool bSource)
{
	Super::OnBecameAware(bSource);
	bCrouchPose = false;
}

FString AGBSkeletalEnemy::DescribeTactics() const
{
	static const TCHAR* const StateNames[] = { TEXT("Hold"), TEXT("Move"), TEXT("Peek") };
	return FString::Printf(TEXT("%s%s cover=%d atPos=%d coverTime=%.0f peekBursts=%d reloc=%d"), StateNames[static_cast<int32>(CState)], BurstPhase != EBurstPhase::Cooldown ? TEXT("+burst") : TEXT(""),
		(bHasPosition && Position.bCovered) ? 1 : 0, static_cast<int32>(bHasPosition ? FMath::RoundToInt(FVector::Dist2D(GetActorLocation(), Position.Cover)) : -1), CoverTime, PeekBursts, Relocations);
}

// ---------------------------------------------------------------------------------------------------------------------
// Melee lunge (the spider, the heavy orcs when they have a LungeRange)
// ---------------------------------------------------------------------------------------------------------------------

void AGBSkeletalEnemy::TickLunge(AActor* Goal, float DeltaSeconds)
{
	const float LungeRange = S(TEXT("LungeRange"), 0.f);
	if (LungeRange <= 0.f)
	{
		return;
	}
	LungeCooldown = FMath::Max(0.f, LungeCooldown - DeltaSeconds);
	const float Distance = FVector::Dist(GetActorLocation(), Goal->GetActorLocation());

	if (bLunging)
	{
		LungeTime += DeltaSeconds;
		const float Reach = Stats->GetStat(GBStat::AttackRange, 0.f) * GetGameValue(GBEnemyKey::MeleeReachTolerance, 1.2f);
		if (!bLungeHit && Distance <= Reach && HasLineOfSight(Goal))
		{
			bLungeHit = true;
			FGBDamageInfo Info = MakeShotInfo();
			Info.HitLocation = Goal->GetActorLocation();
			Info.HitDirection = (Goal->GetActorLocation() - GetActorLocation()).GetSafeNormal();
			UGBCombatLibrary::DealDamage(Goal, Info);
		}
		const bool bLanded = LungeTime > 0.2f && GetCharacterMovement()->IsMovingOnGround();
		if (bLanded || LungeTime > S(TEXT("LungeMaxTime"), 1.2f))
		{
			bLunging = false;
		}
		return;
	}

	// Windup of a lunge: close enough to jump, too far to bite, with a clear line, and rested.
	const float AttackRange = Stats->GetStat(GBStat::AttackRange, 0.f);
	// No lunge towards a landing point outside the zone, and none from the wait radius of a slot.
	if (State == EGBEnemyState::Chase && LungeCooldown <= 0.f && Distance <= LungeRange && Distance > AttackRange && IsInsideZone(Goal->GetActorLocation()) && HasLineOfSight(Goal) && TryStartMelee())
	{
		bLungeQueued = true;
		StopMoving();
		FaceTowards(Goal);
		SetState(EGBEnemyState::Windup);
	}
	else if (State != EGBEnemyState::Windup)
	{
		bLungeQueued = false;
	}
}

void AGBSkeletalEnemy::StartLunge(AActor* Goal)
{
	if (!IsInsideZone(Goal->GetActorLocation()))
	{
		return;   // the landing point would be outside the zone
	}
	const FVector Flat = (Goal->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
	// The lunge is limited by its own landing: start + horizontal speed x air time must be inside the zone and not carry it far past him.
	const float UpSpeed = S(TEXT("LungeUpSpeed"), 350.f);
	const float Gravity = FMath::Max(FMath::Abs(GetCharacterMovement()->GetGravityZ()), 1.f);
	const float AirTime = 2.f * UpSpeed / Gravity;
	float Horizontal = S(TEXT("LungeSpeed"), 1400.f);
	const float ToPlayer = FVector::Dist2D(GetActorLocation(), Goal->GetActorLocation());
	Horizontal = FMath::Min(Horizontal, (ToPlayer + 100.f) / FMath::Max(AirTime, 0.1f));
	if (bHasHomeZone)
	{
		const FVector Landing = GetActorLocation() + Flat * Horizontal * AirTime;
		if (!IsInsideZone(Landing))
		{
			Horizontal = FVector::Dist2D(GetActorLocation(), ClampToZone(Landing)) / FMath::Max(AirTime, 0.1f);
		}
		if (Horizontal < 150.f)
		{
			return;   // no room to land inside the zone
		}
	}
	const FVector Velocity = Flat * Horizontal + FVector::UpVector * UpSpeed;
	LaunchCharacter(Velocity, true, true);
	bLunging = true;
	bLungeHit = false;
	LungeTime = 0.f;
	LungeCooldown = S(TEXT("LungeCooldown"), 2.f);
	++LungeCount;
}

void AGBSkeletalEnemy::PerformAttack()
{
	if (AnimInst)
	{
		AnimInst->PlayOneShot(EGBEnemyClip::Attack, false);
	}
	if (bLungeQueued)
	{
		bLungeQueued = false;
		if (AActor* Goal = Target.Get())
		{
			StartLunge(Goal);
		}
		return;
	}
	Super::PerformAttack();
}

// ---------------------------------------------------------------------------------------------------------------------
// Hits and death
// ---------------------------------------------------------------------------------------------------------------------

void AGBSkeletalEnemy::OnDamagedVisual(const FGBDamageInfo& Info, float ActualDamage)
{
	if (ActualDamage <= 0.f || State == EGBEnemyState::Dead)
	{
		return;
	}
	// Sparks (robots) or whatever Fx.HitImpact names, where the hit landed.
	if (ShotFx.HitImpact && !Info.HitLocation.IsNearlyZero())
	{
		GBEnemy3D::SpawnFx(this, ShotFx.HitImpact, Info.HitLocation, (-Info.HitDirection).Rotation());
	}
	if (AnimInst && GetWorld() && GetWorld()->GetTimeSeconds() - LastHitReactTime >= S(TEXT("HitReactInterval"), 0.4f))
	{
		LastHitReactTime = GetWorld()->GetTimeSeconds();
		AnimInst->NotifyHit(FVector::DotProduct(Info.HitDirection, GetActorRightVector()));
	}
}

void AGBSkeletalEnemy::HandleDeath(const FGBDamageInfo& KillingBlow)
{
	const bool bWasDead = State == EGBEnemyState::Dead;
	Super::HandleDeath(KillingBlow);
	if (bWasDead || !bLookBuilt)
	{
		return;
	}
	bLunging = false;
	// The ragdoll or the death clip needs longer than the default corpse time.
	if (S(TEXT("CorpseTime"), 0.f) > 0.f)
	{
		SetLifeSpan(S(TEXT("CorpseTime"), 0.f));
	}
	USkeletalMeshComponent* Body = GetMesh();
	if (Body && Body->GetPhysicsAsset())
	{
		StartRagdoll(KillingBlow);
	}
	else if (AnimInst)
	{
		// No physics asset (the spider's skeleton, a mesh without one): the Death clip if there is one; otherwise the corpse stays as the parent leaves it.
		AnimInst->SetAim(0.f, 0.f);
		AnimInst->PlayDeath();
	}
}

void AGBSkeletalEnemy::StartRagdoll(const FGBDamageInfo& KillingBlow)
{
	USkeletalMeshComponent* Body = GetMesh();
	bRagdoll = true;

	// The parent switched every collision off; only the mesh (and the weapon) should collide now.
	SetActorEnableCollision(true);
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	for (USkeletalMeshComponent* Part : ArmorParts)
	{
		if (Part)
		{
			Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}
	if (AnimInst)
	{
		AnimInst->SetAim(0.f, 0.f);
	}

	Body->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	Body->SetCollisionProfileName(GBSkeletalEnemyLocal::RagdollProfile);
	Body->SetAllBodiesSimulatePhysics(true);
	Body->SetSimulatePhysics(true);
	Body->WakeAllRigidBodies();
	Body->bBlendPhysics = true;
	FVector Impulse = KillingBlow.HitDirection.GetSafeNormal() * S(TEXT("RagdollImpulse"), 450.f);
	Impulse.Z += S(TEXT("RagdollLift"), 120.f);
	Body->AddImpulse(Impulse, NAME_None, true);

	// The weapon falls off (only when its mesh has collision to fall with).
	if (WeaponComp && WeaponComp->GetStaticMesh() && WeaponComp->GetStaticMesh()->GetBodySetup()
		&& WeaponComp->GetStaticMesh()->GetBodySetup()->AggGeom.GetElementCount() > 0)
	{
		WeaponComp->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		WeaponComp->SetCollisionProfileName(GBSkeletalEnemyLocal::PhysicsActorProfile);
		WeaponComp->SetSimulatePhysics(true);
		WeaponComp->AddImpulse(Impulse * 0.5f + FVector(Random.FRandRange(-60.f, 60.f), Random.FRandRange(-60.f, 60.f), 80.f), NAME_None, true);
	}
}
