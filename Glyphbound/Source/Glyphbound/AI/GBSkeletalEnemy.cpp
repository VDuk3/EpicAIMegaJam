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
		TEXT("Anim.WalkBack"), TEXT("Anim.Attack"), TEXT("Anim.Hit"), TEXT("Anim.Death") };
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
		// Ranged shooters face the player while they strafe; melee ones turn the way they run.
		Move->bOrientRotationToMovement = !IsRanged();
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
	return FString::Printf(TEXT("%s look=%d mesh=%s armor=%d weapon=%d muzzleSocket=%d anim[%s] aware=%d shots=%d bursts=%d lunges=%d ragdoll=%d strafe[picks=%d fails=%d goal=%d intent=%d]"),
		*EnemyId.ToString(), bLookBuilt ? 1 : 0, MeshName.IsEmpty() ? TEXT("-") : *MeshName, ArmorParts.Num(), WeaponComp ? 1 : 0, bHasMuzzleSocket ? 1 : 0,
		AnimInst ? *AnimInst->DescribeClips() : TEXT("none"), bAware ? 1 : 0, ShotCount, BurstCount, LungeCount, bRagdoll ? 1 : 0, StrafePicks, StrafePickFails, bHasStrafeGoal ? 1 : 0, static_cast<int32>(Intent));
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

void AGBSkeletalEnemy::SetIntent(EMoveIntent NewIntent)
{
	if (NewIntent == Intent)
	{
		return;
	}
	// A new kind of move cancels the old path (a MoveToActor must not keep running when it should strafe).
	Intent = NewIntent;
	RepathTimer = 0.f;
	bPointPathing = false;
	bHasStrafeGoal = false;
	if (AController* C = GetController())
	{
		C->StopMovement();
	}
}

void AGBSkeletalEnemy::MoveToPoint(const FVector& Point, const AActor* Goal, bool bRequestPath)
{
	bWantsToMove = true;   // the stuck check of the parent runs after this behaviour
	AAIController* AI = Cast<AAIController>(GetController());
	const UNavigationSystemV1* Nav = UNavigationSystemV1::GetCurrent(GetWorld());
	if (AI && Nav && Nav->GetDefaultNavDataInstance() != nullptr)
	{
		if (bRequestPath)
		{
			const EPathFollowingRequestResult::Type Result = AI->MoveToLocation(Point, GBSkeletalEnemyLocal::StrafeReachRadius * 0.4f, true, true, false, true);
			bPointPathing = Result != EPathFollowingRequestResult::Failed;
		}
		if (bPointPathing)
		{
			bSteering = false;
			return;
		}
	}
	// No navmesh or no path: walk straight at it while nothing blocks the way (the parent's sweep also checks ledges).
	// Fed every tick: a single AddMovementInput is consumed in one frame.
	bSteering = true;
	const FVector Direction = (Point - GetActorLocation()).GetSafeNormal2D();
	if (!Direction.IsNearlyZero() && !IsDirectionBlocked(Direction, GetGameValue(GBEnemyKey::SteerFeelerLength, 150.f), Goal))
	{
		AddMovementInput(Direction, 1.f);
	}
}

bool AGBSkeletalEnemy::PickStrafeGoal(const AActor* Goal, float RangeMin, float RangeMax)
{
	const FVector Loc = GetActorLocation();
	const FVector ToGoal = (Goal->GetActorLocation() - Loc).GetSafeNormal2D();
	if (ToGoal.IsNearlyZero())
	{
		return false;
	}
	const float DistMin = S(TEXT("StrafeDistMin"), 300.f);
	const float DistMax = FMath::Max(S(TEXT("StrafeDistMax"), 600.f), DistMin);
	const float SideJitter = S(TEXT("StrafeJitterAngle"), 35.f);
	const UNavigationSystemV1* Nav = UNavigationSystemV1::GetCurrent(GetWorld());
	for (int32 Try = 0; Try < GBSkeletalEnemyLocal::StrafeTries; ++Try)
	{
		const float Side = Random.FRand() < 0.5f ? 1.f : -1.f;
		const FVector Direction = ToGoal.RotateAngleAxis(Side * (90.f + Random.FRandRange(-SideJitter, SideJitter)), FVector::UpVector);
		const float Distance = Random.FRandRange(DistMin, DistMax);
		FVector Candidate = Loc + Direction * Distance;
		if (Nav && Nav->GetDefaultNavDataInstance() != nullptr)
		{
			FNavLocation Projected;
			if (!Nav->ProjectPointToNavigation(Candidate, Projected, FVector(GBSkeletalEnemyLocal::StrafeNavExtentXY, GBSkeletalEnemyLocal::StrafeNavExtentXY, GBSkeletalEnemyLocal::StrafeNavExtentZ)))
			{
				continue;
			}
			Candidate = Projected.Location;
		}
		const float FromPlayer = FVector::Dist2D(Candidate, Goal->GetActorLocation());
		if (FromPlayer < RangeMin * 0.7f || FromPlayer > RangeMax * 1.1f)
		{
			continue;
		}
		if (IsDirectionBlocked(Direction, Distance, Goal))
		{
			continue;
		}
		StrafeGoal = Candidate;
		bHasStrafeGoal = true;
		return true;
	}
	return false;
}

void AGBSkeletalEnemy::AlertOthers()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const float Radius = S(TEXT("AlertRadius"), 1800.f);
	for (TActorIterator<AGBEnemyCharacter> It(World); It; ++It)
	{
		AGBEnemyCharacter* Other = *It;
		if (!Other || Other == this || Other->IsDead() || !Other->GetEraComponent()->IsPresent())
		{
			continue;
		}
		const float Distance = FVector::Dist(Other->GetActorLocation(), GetActorLocation());
		if (Distance > Radius)
		{
			continue;
		}
		bool bReaches = Distance <= Radius * 0.5f;
		if (!bReaches)
		{
			FCollisionQueryParams Params(SCENE_QUERY_STAT(GBEnemyAlert), false, this);
			Params.AddIgnoredActor(Other);
			bReaches = !World->LineTraceTestByChannel(GetChestLocation(), Other->GetActorLocation(), ECC_Visibility, Params);
		}
		if (bReaches)
		{
			Other->Alert();
		}
	}
}

bool AGBSkeletalEnemy::TickRanged(AActor* Goal, float DeltaSeconds)
{
	using namespace GBSkeletalEnemyLocal;
	const FVector ToGoal = Goal->GetActorLocation() - GetActorLocation();
	const float Distance = ToGoal.Size();

	SightTimer -= DeltaSeconds;
	if (SightTimer <= 0.f)
	{
		bSight = HasLineOfSight(Goal);
		SightTimer = SightInterval;
	}

	// Notice: inside the sight cone with line of sight, or already hit / alerted.
	if (!bAware)
	{
		const FVector Forward = GetActorForwardVector().GetSafeNormal2D();
		const float Angle = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Forward, ToGoal.GetSafeNormal2D()), -1.f, 1.f)));
		if (GBEngage::Notices(Distance, Angle, bSight, Stats->GetStat(GBStat::AggroRange, 0.f), S(TEXT("SightAngle"), 140.f), bProvoked))
		{
			bAware = true;
			BurstPhase = EBurstPhase::Cooldown;
			BurstTimer = S(TEXT("FirstShotDelay"), 0.4f);
			AlertOthers();
			SetState(EGBEnemyState::Chase);
		}
		else
		{
			if (State != EGBEnemyState::Idle)
			{
				StopMoving();
				SetState(EGBEnemyState::Idle);
			}
			UpdateAnimAim(Goal, false);
			return true;
		}
	}

	TurnTowards(Goal, DeltaSeconds);
	UpdateAnimAim(Goal, true);

	// The distance band.
	const float RangeMin = S(TEXT("PreferredRangeMin"), 600.f);
	const float RangeMax = FMath::Max(S(TEXT("PreferredRangeMax"), 1400.f), RangeMin);
	bool bCanRetreat = false;
	if (Distance < RangeMin)
	{
		const FVector Away = -ToGoal.GetSafeNormal2D();
		bCanRetreat = !Away.IsNearlyZero() && !IsDirectionBlocked(Away, RetreatProbe, Goal);
	}
	// Hysteresis: while strafing, drifting a little past the band is not a reason to start advancing again (it would flip every tick).
	const float BandDistance = (Intent == EMoveIntent::Strafe && Distance <= RangeMax + S(TEXT("BandHysteresis"), 150.f)) ? FMath::Min(Distance, RangeMax) : Distance;
	const EGBEngageAction Action = GBEngage::Decide(BandDistance, bSight, RangeMin, RangeMax, bCanRetreat);

	RepathTimer -= DeltaSeconds;
	switch (Action)
	{
	case EGBEngageAction::Advance:
		SetIntent(EMoveIntent::Advance);
		MoveTowards(Goal, DeltaSeconds);
		break;

	case EGBEngageAction::Retreat:
		SetIntent(EMoveIntent::Retreat);
		{
			const bool bRequest = RepathTimer <= 0.f;
			if (bRequest)
			{
				RepathTimer = S(TEXT("RepathInterval"), 0.4f);
			}
			MoveToPoint(GetActorLocation() - ToGoal.GetSafeNormal2D() * RetreatStep, Goal, bRequest);
		}
		break;

	case EGBEngageAction::Hold:
		SetIntent(EMoveIntent::Strafe);
		StrafeTimer -= DeltaSeconds;
		if (bHasStrafeGoal && FVector::Dist2D(GetActorLocation(), StrafeGoal) <= StrafeReachRadius)
		{
			bHasStrafeGoal = false;
		}
		if (StrafeTimer <= 0.f)
		{
			StrafeTimer = Random.FRandRange(S(TEXT("StrafeTimeMin"), 2.f), FMath::Max(S(TEXT("StrafeTimeMax"), 4.f), S(TEXT("StrafeTimeMin"), 2.f)));
			++StrafePicks;
			if (PickStrafeGoal(Goal, RangeMin, RangeMax))
			{
				RepathTimer = 0.f;
			}
			else
			{
				++StrafePickFails;
			}
		}
		if (bHasStrafeGoal)
		{
			const bool bRequest = RepathTimer <= 0.f;
			if (bRequest)
			{
				RepathTimer = S(TEXT("RepathInterval"), 0.4f);
			}
			MoveToPoint(StrafeGoal, Goal, bRequest);
		}
		break;
	}

	TickBurst(Goal, DeltaSeconds, bSight, Distance);
	return true;
}

void AGBSkeletalEnemy::TickBurst(AActor* Goal, float DeltaSeconds, bool bHasSight, float Distance)
{
	const float AttackRange = Stats->GetStat(GBStat::AttackRange, 0.f);
	const bool bCanShoot = Stats->GetStat(GBStat::AttackDamage, 0.f) > 0.f && AttackRange > 0.f;

	// A lightning interrupt (the parent's InterruptAttack) moved the state out of Windup: the telegraph is cancelled.
	if (BurstPhase == EBurstPhase::Telegraph && State != EGBEnemyState::Windup)
	{
		BurstPhase = EBurstPhase::Cooldown;
		BurstTimer = Stats->GetStat(GBStat::AttackInterval, 1.f);
	}

	switch (BurstPhase)
	{
	case EBurstPhase::Cooldown:
		BurstTimer -= DeltaSeconds;
		if (State != EGBEnemyState::Chase)
		{
			SetState(EGBEnemyState::Chase);
		}
		if (bCanShoot && BurstTimer <= 0.f && bHasSight && Distance <= AttackRange)
		{
			BurstPhase = EBurstPhase::Telegraph;
			BurstTimer = Stats->GetStat(GBStat::AttackWindup, 0.4f);
			SetState(EGBEnemyState::Windup);
		}
		break;

	case EBurstPhase::Telegraph:
		BurstTimer -= DeltaSeconds;
		if (!bHasSight)
		{
			// Lost the line during the aim: nothing to shoot at.
			BurstPhase = EBurstPhase::Cooldown;
			BurstTimer = 0.2f;
			SetState(EGBEnemyState::Chase);
		}
		else if (BurstTimer <= 0.f)
		{
			BurstPhase = EBurstPhase::Firing;
			ShotsLeft = FMath::Max(1, FMath::RoundToInt(S(TEXT("BurstCount"), 3.f)));
			ShotTimer = 0.f;
			++BurstCount;
			SetState(EGBEnemyState::Recover);
		}
		break;

	case EBurstPhase::Firing:
		ShotTimer -= DeltaSeconds;
		if (!bHasSight)
		{
			ShotsLeft = 0;
		}
		else if (ShotTimer <= 0.f && ShotsLeft > 0)
		{
			FireShot(Goal);
			--ShotsLeft;
			ShotTimer = S(TEXT("BurstShotInterval"), 0.12f);
		}
		if (ShotsLeft <= 0)
		{
			BurstPhase = EBurstPhase::Cooldown;
			BurstTimer = Stats->GetStat(GBStat::AttackInterval, 1.f);
			SetState(EGBEnemyState::Chase);
		}
		break;
	}
}

void AGBSkeletalEnemy::FireShot(AActor* Goal)
{
	const FVector Muzzle = GetMuzzleLocation();
	FVector Direction = (GBEnemy3D::TargetChest(Goal) - Muzzle).GetSafeNormal();
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
	const float Spread = UGBCombatLibrary::ComputeMovementSpread(S(TEXT("SpreadDegrees"), 4.f), S(TEXT("MoveSpreadDegrees"), 3.f), PlayerSpeed, PlayerFullSpeed);
	Direction = UGBCombatLibrary::ApplySpread(Direction, Spread, Random);

	const float Range = Stats->GetStat(GBStat::AggroRange, 3000.f) * GetGameValue(GBEnemyKey::ProjectileRangeMult, 1.5f);
	GBEnemy3D::FireHitscan(this, Muzzle, Direction, Range, MakeShotInfo(), ShotFx, Goal);
	++ShotCount;
	if (AnimInst)
	{
		AnimInst->NotifyShot();
	}
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
	if (State == EGBEnemyState::Chase && LungeCooldown <= 0.f && Distance <= LungeRange && Distance > AttackRange && HasLineOfSight(Goal))
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
	const FVector Flat = (Goal->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
	const FVector Velocity = Flat * S(TEXT("LungeSpeed"), 1400.f) + FVector::UpVector * S(TEXT("LungeUpSpeed"), 350.f);
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
