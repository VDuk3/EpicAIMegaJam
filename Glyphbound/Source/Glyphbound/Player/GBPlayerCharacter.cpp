#include "Player/GBPlayerCharacter.h"

#include "Player/GBInputSet.h"
#include "Player/GBPlayerController.h"
#include "Combat/GBCombatLibrary.h"
#include "Core/GBStatNames.h"
#include "Core/GBGameMode.h"
#include "Data/GBDataSubsystem.h"
#include "Stats/GBStatsComponent.h"
#include "Stats/GBAttributeComponent.h"
#include "Combat/GBStatusEffectComponent.h"
#include "UI/GBHUD.h"
#include "Weapons/GBEquipmentComponent.h"
#include "Weapons/GBHeldItem.h"
#include "Spells/GBSpellbookComponent.h"
#include "QTE/GBQTEComponent.h"
#include "Items/GBInventoryComponent.h"
#include "Progression/GBSkillTreeComponent.h"
#include "World/GBInteractable.h"
#include "World/GBProgressSubsystem.h"
#include "World/GBEraSubsystem.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedPlayerInput.h"
#include "InputActionValue.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "CollisionQueryParams.h"
#include "CoreGlobals.h"
#include "Misc/App.h"

namespace
{
	// Feel and visual constants (not gameplay balance). Mouse2D values come scaled by the engine's
	// default axis sensitivity (0.07 per pixel, BaseInput.ini), hence the conversions below.
	constexpr float MouseUnitsPerPixel = 0.07f;
	/** Ignore axis noise on wheel / D-pad style 1D actions. */
	constexpr float AxisStepThreshold = 0.1f;
	/** Held item anchor and muzzle offsets from the camera (forward, right, up), cm. */
	const FVector HeldItemOffset(30.f, 15.f, -15.f);
	const FVector MuzzleOffset(40.f, 15.f, -12.f);
	// First-person climb (mantle) feel and visual constants (not gameplay balance, like the offsets above).
	/** The view pitches down by this (degrees) at the top of the climb. */
	constexpr float MantlePitchDipDeg = 10.f;
	/** ...and rolls by this (degrees). */
	constexpr float MantleRollDeg = 3.f;
	/** ...while the camera drops by this (cm), the pull on the arms. */
	constexpr float MantlePullDownCm = 6.f;
	/** The held item drops out of view by this (cm) while the hands grip the ledge. */
	const FVector MantleItemLowerOffset(-10.f, 0.f, -35.f);

	/** Not a balance number: a hit far above any health pool (KillZ death). */
	constexpr float FallOutOfWorldDamage = 1.0e9f;
	const FName FellOutOfWorldSourceId(TEXT("FellOutOfWorld"));

	TSet<FName> GWarnedMissingPlayerStats;
}

AGBPlayerCharacter::AGBPlayerCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(GetCapsuleComponent());
	Camera->SetRelativeLocation(FVector(0.f, 0.f, BaseEyeHeight)); // replaced by the EyeHeight stat on BeginPlay
	Camera->bUsePawnControlRotation = true;

	HeldItemAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("HeldItemAnchor"));
	HeldItemAnchor->SetupAttachment(Camera);
	HeldItemAnchor->SetRelativeLocation(HeldItemOffset);

	Stats = CreateDefaultSubobject<UGBStatsComponent>(TEXT("Stats"));
	Attributes = CreateDefaultSubobject<UGBAttributeComponent>(TEXT("Attributes"));
	StatusEffects = CreateDefaultSubobject<UGBStatusEffectComponent>(TEXT("StatusEffects"));
	Equipment = CreateDefaultSubobject<UGBEquipmentComponent>(TEXT("Equipment"));
	Spellbook = CreateDefaultSubobject<UGBSpellbookComponent>(TEXT("Spellbook"));
	QTE = CreateDefaultSubobject<UGBQTEComponent>(TEXT("QTE"));
	Inventory = CreateDefaultSubobject<UGBInventoryComponent>(TEXT("Inventory"));
	SkillTree = CreateDefaultSubobject<UGBSkillTreeComponent>(TEXT("SkillTree"));

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->bOrientRotationToMovement = false;
		// GroundFriction comes from Player.json (UpdateWalkSpeed on BeginPlay); until then the engine default (8.0) applies.
	}
}

AGBPlayerCharacter* AGBPlayerCharacter::Get(const UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World)
	{
		return nullptr;
	}
	APlayerController* PC = World->GetFirstPlayerController();
	return PC ? Cast<AGBPlayerCharacter>(PC->GetPawn()) : nullptr;
}

// ---------------------------------------------------------------------------------------------------------------
// Lifecycle

void AGBPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	UGBDataSubsystem* Data = UGBDataSubsystem::Get(this);
	if (!Data)
	{
		UE_LOG(LogGB, Warning, TEXT("GBPlayerCharacter: no data subsystem, the player starts with default stats."));
	}

	if (Data)
	{
		Stats->SetBaseStats(Data->GetPlayer().Stats);
	}
	Attributes->Team = EGBTeam::Player;
	Attributes->InitializeFromStats();

	if (Data)
	{
		// Copy: GiveWeapon reads the data tables, never keep a reference into them across calls.
		const TArray<FName> StartingWeapons = Data->GetPlayer().StartingWeapons;
		for (int32 Index = 0; Index < StartingWeapons.Num(); ++Index)
		{
			if (!Data->FindWeapon(StartingWeapons[Index]))
			{
				UE_LOG(LogGB, Warning, TEXT("GBPlayerCharacter: starting weapon '%s' is not in Weapons.json."), *StartingWeapons[Index].ToString());
				continue;
			}
			// Same rule as pickups: the first of each kind goes to its slot, the others are stored.
			Equipment->GiveWeapon(StartingWeapons[Index], Index == 0);
		}
	}

	Spellbook->InitializeFromData();
	Inventory->InitializeFromData();
	SkillTree->InitializeFromData();

	Stats->OnStatsChanged.AddUObject(this, &AGBPlayerCharacter::ApplyMovementStats);
	Attributes->OnDeath.AddUObject(this, &AGBPlayerCharacter::HandleDeath);
	DamageDealtHandle = UGBCombatLibrary::OnDamageDealt().AddUObject(this, &AGBPlayerCharacter::HandlePrecisionRecovery);
	if (Data)
	{
		DataReloadedHandle = Data->OnDataReloaded.AddUObject(this, &AGBPlayerCharacter::HandleDataReloaded);
	}

	// Passives learned above changed stats before the binding existed.
	ApplyMovementStats();

	if (UGBProgressSubsystem* Progress = UGBProgressSubsystem::Get(this))
	{
		if (!Progress->HasCheckpoint())
		{
			Progress->SaveCheckpoint(this);
		}
	}
}

void AGBPlayerCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CancelMantle();
	UGBCombatLibrary::OnDamageDealt().Remove(DamageDealtHandle);
	DamageDealtHandle.Reset();
	if (UGBDataSubsystem* Data = UGBDataSubsystem::Get(this))
	{
		Data->OnDataReloaded.Remove(DataReloadedHandle);
	}
	DataReloadedHandle.Reset();
	Super::EndPlay(EndPlayReason);
}

void AGBPlayerCharacter::HandleDataReloaded()
{
	if (UGBDataSubsystem* Data = UGBDataSubsystem::Get(this))
	{
		Stats->SetBaseStats(Data->GetPlayer().Stats);
	}
}

void AGBPlayerCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateFocusedInteractable();

	if (IsQTEActive())
	{
		// GDD 7: the movement keys are busy with the QTE, so the last movement input is held.
		if (CanUseGameplayInput() && !bDashing && !LastMoveInput.IsNearlyZero())
		{
			ApplyMoveInput(LastMoveInput);
		}
	}
	else if (GFrameCounter > LastMoveInputFrame + 1)
	{
		// No Move event for a frame: the stick / keys were released (also covers releases during a QTE).
		LastMoveInput = FVector2D::ZeroVector;
	}

	if (bDashing)
	{
		DashTimeLeft -= DeltaSeconds;
		if (DashTimeLeft <= 0.f)
		{
			EndDash();
		}
		else if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
		{
			MoveComp->Velocity = DashVelocity;
		}
	}

	if (bMantling)
	{
		TickMantle(DeltaSeconds);
	}
	else if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		// In the air (rising or falling): grab a ledge in front instead of sailing past it.
		if (CanMantleNow() && MoveComp->MovementMode == MOVE_Falling)
		{
			FGBMantleLedge AirLedge;
			if (FindMantleLedge(AirLedge, /*bFromGround*/ false))
			{
				const FVector Velocity = MoveComp->Velocity;
				const float Approach = -(Velocity.X * AirLedge.WallNormal2D.X + Velocity.Y * AirLedge.WallNormal2D.Y);
				if (Approach >= -50.f)
				{
					StartMantle(AirLedge);
				}
			}
		}
	}

	TickRecoil(DeltaSeconds);
	TickViewShake(DeltaSeconds);
}

// ---------------------------------------------------------------------------------------------------------------
// Stats and movement

float AGBPlayerCharacter::ReadStat(FName Stat, float DefaultValue) const
{
	if (!Stats)
	{
		return DefaultValue;
	}
	if (!Stats->HasBaseStat(Stat) && !GWarnedMissingPlayerStats.Contains(Stat))
	{
		GWarnedMissingPlayerStats.Add(Stat);
		UE_LOG(LogGB, Warning, TEXT("GBPlayerCharacter: stat '%s' is missing from Player.json, using %.2f."), *Stat.ToString(), DefaultValue);
	}
	return Stats->GetStat(Stat, DefaultValue);
}

void AGBPlayerCharacter::ApplyMovementStats()
{
	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	if (!MoveComp)
	{
		return;
	}

	UpdateWalkSpeed();
	MoveComp->JumpZVelocity = FMath::Max(0.f, ReadStat(GBStat::JumpVelocity, 620.f));
	MoveComp->AirControl = FMath::Max(0.f, ReadStat(GBStat::AirControl, 0.f));
	MoveComp->GravityScale = ReadStat(GBStat::GravityScale, 1.6f);
	// Committed jumps (GDD 18): no air steering, no air braking or lateral friction, so the takeoff keeps its speed.
	MoveComp->BrakingDecelerationFalling = 0.f;
	MoveComp->FallingLateralFriction = 0.f;
	JumpMaxCount = FMath::Max(1, FMath::RoundToInt(ReadStat(GBStat::MaxJumps, 1.f)));

	UCapsuleComponent* Capsule = GetCapsuleComponent();
	const float Radius = FMath::Max(1.f, ReadStat(GBStat::CapsuleRadius, 38.f));
	const float HalfHeight = FMath::Max(Radius, ReadStat(GBStat::CapsuleHalfHeight, 90.f));
	const float OldHalfHeight = Capsule->GetUnscaledCapsuleHalfHeight();
	if (!FMath::IsNearlyEqual(OldHalfHeight, HalfHeight) || !FMath::IsNearlyEqual(Capsule->GetUnscaledCapsuleRadius(), Radius))
	{
		const bool bWasOnGround = MoveComp->IsMovingOnGround();
		Capsule->SetCapsuleSize(Radius, HalfHeight, true);
		if (bWasOnGround && HalfHeight > OldHalfHeight)
		{
			// Keep the feet on the floor instead of sinking into it.
			AddActorWorldOffset(FVector(0.f, 0.f, HalfHeight - OldHalfHeight));
		}
	}

	BaseEyeHeight = ReadStat(GBStat::EyeHeight, 70.f);
	ApplyCameraPose();
}

void AGBPlayerCharacter::UpdateWalkSpeed()
{
	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	if (!MoveComp)
	{
		return;
	}
	const float Speed = FMath::Max(0.f, ReadStat(GBStat::MoveSpeed, 750.f) * MoveSpeedScale);
	MoveComp->MaxWalkSpeed = Speed;
	MoveComp->MaxAcceleration = FMath::Max(1.f, Speed * ReadStat(GBStat::AccelerationPerSpeed, 10.f));
	MoveComp->BrakingDecelerationWalking = Speed * ReadStat(GBStat::BrakingPerSpeed, 8.f);
	MoveComp->GroundFriction = ReadStat(GBStat::GroundFriction, 8.f);
}

void AGBPlayerCharacter::SetMoveSpeedScale(float Scale)
{
	MoveSpeedScale = FMath::Max(0.f, Scale);
	UpdateWalkSpeed();
}

void AGBPlayerCharacter::ApplyMoveInput(const FVector2D& Input)
{
	if (bMantling)
	{
		return;
	}
	const FRotator YawRotation(0.f, GetControlRotation().Yaw, 0.f);
	const FRotationMatrix YawMatrix(YawRotation);
	AddMovementInput(YawMatrix.GetUnitAxis(EAxis::X), Input.Y);
	AddMovementInput(YawMatrix.GetUnitAxis(EAxis::Y), Input.X);
}

FVector AGBPlayerCharacter::GetMoveInputWorldDirection(const FVector2D& Input) const
{
	if (Input.IsNearlyZero())
	{
		return FVector::ZeroVector;
	}
	const FRotator YawRotation(0.f, GetControlRotation().Yaw, 0.f);
	const FRotationMatrix YawMatrix(YawRotation);
	FVector Direction = YawMatrix.GetUnitAxis(EAxis::X) * Input.Y + YawMatrix.GetUnitAxis(EAxis::Y) * Input.X;
	Direction.Z = 0.f;
	if (!Direction.Normalize())
	{
		return FVector::ZeroVector;
	}
	return Direction;
}

FVector2D AGBPlayerCharacter::ComputeJumpHorizontalVelocity(const FVector2D& CurrentVelocity, const FVector2D& InputDirection, float MaxWalkSpeed, float MinSpeedMult)
{
	if (InputDirection.IsNearlyZero())
	{
		return CurrentVelocity;
	}
	const FVector2D Direction = InputDirection.GetSafeNormal();
	const float CurrentSpeed = CurrentVelocity.Size();
	const float MinSpeed = FMath::Max(0.f, MaxWalkSpeed) * FMath::Max(0.f, MinSpeedMult);
	return Direction * FMath::Max(CurrentSpeed, MinSpeed);
}

void AGBPlayerCharacter::OnJumped_Implementation()
{
	// JumpCurrentCount is 1 for a jump from the ground, 2+ for a jump that starts in the air
	// (double jump, or the first jump after walking off a ledge: CheckJumpInput pre-increments while falling).
	const bool bGroundJump = JumpCurrentCount <= 1;
	const float MinSpeedMult = ReadStat(bGroundJump ? GBStat::JumpTakeoffMinSpeedMult : GBStat::AirJumpMinSpeedMult,
		bGroundJump ? 0.6f : 1.0f);
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		const FVector2D CurrentVelocity(MoveComp->Velocity.X, MoveComp->Velocity.Y);
		const FVector WorldDir = GetMoveInputWorldDirection(LastMoveInput);
		const FVector2D NewHorizontal = ComputeJumpHorizontalVelocity(CurrentVelocity,
			FVector2D(WorldDir.X, WorldDir.Y), MoveComp->MaxWalkSpeed, MinSpeedMult);
		// The engine already set the vertical velocity; only the horizontal direction commits here.
		MoveComp->Velocity.X = NewHorizontal.X;
		MoveComp->Velocity.Y = NewHorizontal.Y;
	}
}

void AGBPlayerCharacter::AddLookDelta(float YawDegrees, float PitchDegrees)
{
	if (!Controller)
	{
		return;
	}
	FRotator Rotation = Controller->GetControlRotation();
	Rotation.Yaw = FRotator::NormalizeAxis(Rotation.Yaw + YawDegrees);
	const double PitchLimit = ReadStat(GBStat::PitchLimit, 89.f);
	Rotation.Pitch = FMath::Clamp(FRotator::NormalizeAxis(Rotation.Pitch + PitchDegrees), -PitchLimit, PitchLimit);
	Rotation.Roll = 0.f;
	Controller->SetControlRotation(Rotation);
}

float AGBPlayerCharacter::GetDashCooldownRemaining() const
{
	const UWorld* World = GetWorld();
	return World ? FMath::Max(0.f, static_cast<float>(DashReadyTime - World->GetTimeSeconds())) : 0.f;
}

void AGBPlayerCharacter::EndDash()
{
	if (!bDashing)
	{
		return;
	}
	bDashing = false;
	DashTimeLeft = 0.f;

	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	if (MoveComp && MoveComp->MovementMode == MOVE_Flying)
	{
		// Changing movement mode may reset the jump count: keep it, so a dash never refills jumps.
		const int32 SavedJumpCount = JumpCurrentCount;
		MoveComp->SetMovementMode(MOVE_Falling);
		JumpCurrentCount = SavedJumpCount;
		MoveComp->Velocity = DashVelocity.GetSafeNormal() * MoveComp->MaxWalkSpeed * ReadStat(GBStat::DashExitSpeedMult, 1.f);
	}
}

// ---------------------------------------------------------------------------------------------------------------
// Ledge climb (mantle)

GBMantle::FMantleParams AGBPlayerCharacter::GetMantleParams() const
{
	GBMantle::FMantleParams Params;
	Params.MaxReach = ReadStat(GBStat::MantleMaxReach, 160.f);
	Params.MinHeight = ReadStat(GBStat::MantleMinHeight, 50.f);
	Params.AirMinHeight = ReadStat(GBStat::MantleAirMinHeight, 15.f);
	Params.BaseDuration = ReadStat(GBStat::MantleBaseDuration, 0.22f);
	Params.DurationPerMeter = ReadStat(GBStat::MantleDurationPerMeter, 0.18f);
	Params.RiseShare = ReadStat(GBStat::MantleRiseShare, 0.6f);
	return Params;
}

bool AGBPlayerCharacter::CanMantleNow() const
{
	return bControlEnabled && !IsDead() && !IsQTEActive() && !bDashing && !bMovementFrozen && !bMantling;
}

bool AGBPlayerCharacter::FindMantleLedge(FGBMantleLedge& Out, bool bFromGround) const
{
	UWorld* World = GetWorld();
	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	const UCapsuleComponent* Capsule = GetCapsuleComponent();
	if (!World || !MoveComp || !Capsule)
	{
		return false;
	}

	const float Radius = Capsule->GetScaledCapsuleRadius();
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const FVector ActorLocation = GetActorLocation();
	const float Feet = ActorLocation.Z - HalfHeight;
	const float Yaw = Controller ? Controller->GetControlRotation().Yaw : GetActorRotation().Yaw;
	const FVector Forward = FRotator(0.f, Yaw, 0.f).Vector();
	const FVector2D Forward2D = FVector2D(Forward.X, Forward.Y).GetSafeNormal();

	const GBMantle::FMantleParams Params = GetMantleParams();
	const float ProbeDistance = ReadStat(GBStat::MantleProbeDistance, 40.f);
	const float FacingMinDot = ReadStat(GBStat::MantleFacingMinDot, 0.5f);
	const float TopInset = ReadStat(GBStat::MantleTopInset, 10.f);

	float JumpRiseCm = 0.f;
	if (bFromGround)
	{
		const float Gravity = FMath::Abs(World->GetGravityZ()) * MoveComp->GravityScale;
		JumpRiseCm = GBMantle::JumpRise(MoveComp->JumpZVelocity, Gravity);
	}
	const float MinHeight = bFromGround ? Params.MinHeight : Params.AirMinHeight;
	const float MaxHeight = bFromGround ? JumpRiseCm + Params.MaxReach : Params.MaxReach;
	if (MaxHeight < MinHeight)
	{
		return false;
	}

	// The sweep answers as the capsule would: with its own collision responses, ignoring this pawn.
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(GBMantleWall), false, this);
	FCollisionResponseParams ResponseParams(Capsule->GetCollisionResponseToChannels());

	// ---- Wall: the shrunk capsule swept forward. Lifted a little: from the actor location the capsule
	// bottom touches the floor it stands on, which would block (or start-penetrate) the sweep itself.
	// The lift only moves the hit up the same wall face; the edge XY used below does not change.
	constexpr float WallSweepLiftCm = 5.f;
	const FVector WallBase = ActorLocation + FVector(0.f, 0.f, WallSweepLiftCm);
	const float ShrunkRadius = FMath::Max(1.f, Radius - 2.f);
	const FCollisionShape WallShape = FCollisionShape::MakeCapsule(ShrunkRadius, HalfHeight);
	FHitResult WallHit;
	if (!World->SweepSingleByChannel(WallHit, WallBase, WallBase + Forward * ProbeDistance,
		FQuat::Identity, ECC_Pawn, WallShape, QueryParams, ResponseParams)
		|| !WallHit.bBlockingHit || WallHit.bStartPenetrating)
	{
		return false;
	}
	AActor* WallActor = WallHit.GetActor();
	if (!WallActor || WallActor->IsA<APawn>())
	{
		return false;
	}
	if (FMath::Abs(WallHit.ImpactNormal.Z) >= 0.3f)
	{
		return false;
	}
	FVector2D WallNormal2D(WallHit.ImpactNormal.X, WallHit.ImpactNormal.Y);
	if (!WallNormal2D.Normalize())
	{
		return false;
	}
	if (FVector2D::DotProduct(Forward2D, -WallNormal2D) < FacingMinDot)
	{
		return false;
	}

	// ---- Top: a small sphere swept down behind the wall face. ----
	const FVector2D Edge2D(WallHit.ImpactPoint.X, WallHit.ImpactPoint.Y);
	const FVector2D TopXY = Edge2D - WallNormal2D * (TopInset + 8.f);
	const FVector TopStart(TopXY.X, TopXY.Y, Feet + MaxHeight + 20.f);
	const FVector TopEnd(TopXY.X, TopXY.Y, Feet + MinHeight - 5.f);
	FHitResult TopHit;
	if (!World->SweepSingleByChannel(TopHit, TopStart, TopEnd, FQuat::Identity, ECC_Pawn,
		FCollisionShape::MakeSphere(8.f), QueryParams, ResponseParams)
		|| !TopHit.bBlockingHit || TopHit.bStartPenetrating)
	{
		return false;
	}
	AActor* TopActor = TopHit.GetActor();
	if (!TopActor || TopActor->IsA<APawn>())
	{
		return false;
	}
	if (TopHit.ImpactNormal.Z < MoveComp->GetWalkableFloorZ())
	{
		return false;
	}
	const float LedgeTopZ = TopHit.ImpactPoint.Z;
	const float LedgeHeight = LedgeTopZ - Feet;
	const bool bHeightOk = bFromGround
		? GBMantle::CanMantleFromGround(LedgeHeight, JumpRiseCm, Params)
		: GBMantle::CanMantleFromAir(LedgeHeight, Params);
	if (!bHeightOk)
	{
		return false;
	}

	// ---- Room: the capsule must fit at the end, and the whole path (straight up, then forward) must be clear,
	// so a thin ceiling or beam between the checked poses is never passed through. The path sweeps use the shrunk
	// capsule, lifted like the wall sweep, so the wall it slides along and the floor it leaves do not count.
	const FVector2D EndXY = Edge2D - WallNormal2D * (Radius + TopInset);
	const FVector End(EndXY.X, EndXY.Y, LedgeTopZ + HalfHeight + 2.f);
	const FVector Mid(ActorLocation.X, ActorLocation.Y, End.Z);
	const FCollisionShape FitShape = FCollisionShape::MakeCapsule(Radius, HalfHeight);
	if (World->OverlapBlockingTestByChannel(End, FQuat::Identity, ECC_Pawn, FitShape, QueryParams, ResponseParams))
	{
		return false;
	}
	FHitResult PathHit;
	if (Mid.Z > WallBase.Z && World->SweepSingleByChannel(PathHit, WallBase, Mid, FQuat::Identity, ECC_Pawn, WallShape, QueryParams, ResponseParams))
	{
		return false;
	}
	if (World->SweepSingleByChannel(PathHit, Mid, End, FQuat::Identity, ECC_Pawn, WallShape, QueryParams, ResponseParams))
	{
		return false;
	}
	// ---- Support: walkable floor under the end (a thin beam can give a top hit with nothing under the capsule centre).
	FHitResult FloorHit;
	const FVector FloorEnd = End - FVector(0.f, 0.f, HalfHeight + 2.f + MoveComp->MaxStepHeight);
	if (!World->LineTraceSingleByChannel(FloorHit, End, FloorEnd, ECC_Pawn, QueryParams, ResponseParams)
		|| FloorHit.bStartPenetrating || FloorHit.ImpactNormal.Z < MoveComp->GetWalkableFloorZ()
		|| (FloorHit.GetActor() && FloorHit.GetActor()->IsA<APawn>()))
	{
		return false;
	}

	Out.Start = ActorLocation;
	Out.End = End;
	Out.LedgePoint = FVector(Edge2D.X, Edge2D.Y, LedgeTopZ);
	Out.RiseCm = End.Z - ActorLocation.Z;
	Out.WallNormal2D = WallNormal2D;
	return true;
}

bool AGBPlayerCharacter::TryStartMantle(bool bFromGround)
{
	if (!CanMantleNow())
	{
		return false;
	}
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		if (bFromGround)
		{
			if (!MoveComp->IsMovingOnGround())
			{
				return false;
			}
		}
		else if (MoveComp->MovementMode != MOVE_Falling)
		{
			return false;
		}
	}
	FGBMantleLedge Ledge;
	if (!FindMantleLedge(Ledge, bFromGround))
	{
		return false;
	}
	StartMantle(Ledge);
	return true;
}

void AGBPlayerCharacter::StartMantle(const FGBMantleLedge& Ledge)
{
	ReleaseHeldInputs();
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->StopMovementImmediately();
		// Flying = no gravity and no ground friction while the path drives the actor; the jump count is kept.
		MantleSavedJumpCount = JumpCurrentCount;
		MoveComp->SetMovementMode(MOVE_Flying);
		JumpCurrentCount = MantleSavedJumpCount;
		MoveComp->Velocity = FVector::ZeroVector;
	}
	const GBMantle::FMantleParams Params = GetMantleParams();
	MantleStart = Ledge.Start;
	MantleEnd = Ledge.End;
	MantleLedgePoint = Ledge.LedgePoint;
	MantleRiseShare = Params.RiseShare;
	MantleDuration = GBMantle::Duration(Ledge.RiseCm, Params);
	MantleElapsed = 0.f;
	bMantling = true;
	ApplyCameraPose();
}

void AGBPlayerCharacter::TickMantle(float DeltaSeconds)
{
	if (!bMantling)
	{
		return;
	}
	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	MantleElapsed += DeltaSeconds;
	const float Alpha = GetMantleAlpha();
	SetActorLocation(GBMantle::EvaluatePath(MantleStart, MantleEnd, Alpha, MantleRiseShare),
		false, nullptr, ETeleportType::TeleportPhysics);
	if (MoveComp)
	{
		MoveComp->Velocity = FVector::ZeroVector;
	}
	if (Alpha >= 1.f)
	{
		SetActorLocation(MantleEnd, false, nullptr, ETeleportType::TeleportPhysics);
		if (MoveComp)
		{
			MoveComp->SetMovementMode(MOVE_Walking);
			MoveComp->Velocity = FVector::ZeroVector;
		}
		bMantling = false;
		MantleElapsed = 0.f;
	}
}

void AGBPlayerCharacter::CancelMantle()
{
	if (!bMantling)
	{
		return;
	}
	bMantling = false;
	MantleElapsed = 0.f;
	MantleDuration = 0.f;
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		if (!bMovementFrozen)
		{
			MoveComp->SetMovementMode(MOVE_Falling);
		}
		MoveComp->Velocity = FVector::ZeroVector;
	}
	// A cancelled climb never refills jumps (releasing jump while Flying resets the count).
	JumpCurrentCount = MantleSavedJumpCount;
	ApplyCameraPose();
}

float AGBPlayerCharacter::GetMantleAlpha() const
{
	if (!bMantling || MantleDuration <= 0.f)
	{
		return 0.f;
	}
	return FMath::Clamp(MantleElapsed / MantleDuration, 0.f, 1.f);
}

GBMantle::FMantlePose AGBPlayerCharacter::GetMantlePose() const
{
	if (!bMantling)
	{
		return GBMantle::FMantlePose();
	}
	return GBMantle::EvaluatePose(GetMantleAlpha(), MantleRiseShare);
}

void AGBPlayerCharacter::ApplyCameraPose()
{
	if (!Camera)
	{
		return;
	}
	// One place sets the view: the shake offset plus the climb dip, so neither leaves a residue behind.
	// The camera follows the control rotation (bUsePawnControlRotation overwrites its rotation every view), so
	// the view-only offsets go through the camera's additive offset, applied after it. The aim never moves.
	const GBMantle::FMantlePose Pose = GetMantlePose();
	const FRotator MantleRot(-Pose.CameraDip01 * MantlePitchDipDeg, 0.f, Pose.CameraDip01 * MantleRollDeg);
	const FRotator ViewRot = ViewShakeRot + MantleRot;
	const FVector ViewOffset(0.f, 0.f, -Pose.Grip01 * MantlePullDownCm);
	Camera->SetRelativeLocation(FVector(0.f, 0.f, BaseEyeHeight));
	Camera->ClearAdditiveOffset();
	if (!ViewRot.IsNearlyZero(1.e-4f) || !ViewOffset.IsNearlyZero(1.e-4f))
	{
		Camera->AddAdditiveOffset(FTransform(ViewRot, ViewOffset), 0.f);
	}
	if (HeldItemAnchor)
	{
		HeldItemAnchor->SetRelativeLocation(HeldItemOffset + MantleItemLowerOffset * Pose.ItemLower01);
	}
}

// ---------------------------------------------------------------------------------------------------------------
// Aiming

FVector AGBPlayerCharacter::GetAimOrigin() const
{
	return Camera ? Camera->GetComponentLocation() : GetActorLocation();
}

FVector AGBPlayerCharacter::GetAimDirection() const
{
	// Control rotation is current this frame; the camera component only catches up when the view is computed.
	if (Controller)
	{
		return Controller->GetControlRotation().Vector();
	}
	return Camera ? Camera->GetForwardVector() : GetActorForwardVector();
}

FVector AGBPlayerCharacter::GetMuzzleLocation() const
{
	const FRotator AimRotation = GetAimDirection().Rotation();
	return GetAimOrigin() + AimRotation.RotateVector(MuzzleOffset);
}

void AGBPlayerCharacter::AddRecoil(float Pitch, float Yaw, float RecoverPerSecond)
{
	AddLookDelta(Yaw, Pitch);
	PendingRecoil.Pitch += Pitch;
	PendingRecoil.Yaw += Yaw;
	RecoilRecoverSpeed = FMath::Max(0.f, RecoverPerSecond);
}

void AGBPlayerCharacter::TickRecoil(float DeltaSeconds)
{
	if (IsQTEActive() || RecoilRecoverSpeed <= 0.f)
	{
		return; // the camera is locked during the QTE
	}
	const FVector2D Remaining(PendingRecoil.Pitch, PendingRecoil.Yaw);
	const float Length = static_cast<float>(Remaining.Size());
	if (Length <= KINDA_SMALL_NUMBER)
	{
		PendingRecoil = FRotator::ZeroRotator;
		return;
	}
	const float Step = FMath::Min(Length, RecoilRecoverSpeed * DeltaSeconds);
	const float BackPitch = static_cast<float>(Remaining.X) / Length * Step;
	const float BackYaw = static_cast<float>(Remaining.Y) / Length * Step;
	PendingRecoil.Pitch -= BackPitch;
	PendingRecoil.Yaw -= BackYaw;
	AddLookDelta(-BackYaw, -BackPitch);
}

void AGBPlayerCharacter::StartViewShake(float Duration, float AmplitudeDeg, float Frequency)
{
	if (!Camera)
	{
		return;
	}
	if (Duration <= 0.f || AmplitudeDeg <= 0.f)
	{
		ViewShakeTimeLeft = 0.f;
		ViewShakeDuration = 0.f;
		ViewShakeAmplitudeDeg = 0.f;
		ViewShakeRot = FRotator::ZeroRotator;
		ApplyCameraPose();
		return;
	}
	if (ViewShakeTimeLeft > 0.f)
	{
		ViewShakeAmplitudeDeg = FMath::Max(ViewShakeAmplitudeDeg, AmplitudeDeg);
	}
	else
	{
		ViewShakeAmplitudeDeg = AmplitudeDeg;
	}
	ViewShakeDuration = Duration;
	ViewShakeTimeLeft = Duration;
	ViewShakeFrequency = FMath::Max(0.f, Frequency);
	ApplyCameraPose();
}

void AGBPlayerCharacter::TickViewShake(float DeltaSeconds)
{
	if (!Camera)
	{
		return;
	}
	if (ViewShakeTimeLeft > 0.f)
	{
		ViewShakeTimeLeft -= DeltaSeconds;
		if (ViewShakeTimeLeft <= 0.f || ViewShakeDuration <= 0.f)
		{
			ViewShakeTimeLeft = 0.f;
			ViewShakeDuration = 0.f;
			ViewShakeAmplitudeDeg = 0.f;
			ViewShakeRot = FRotator::ZeroRotator;
		}
		else
		{
			const float Elapsed = ViewShakeDuration - ViewShakeTimeLeft;
			const float Fade = FMath::Clamp(ViewShakeTimeLeft / ViewShakeDuration, 0.f, 1.f);
			const float Omega = ViewShakeFrequency * 2.f * UE_PI;
			const float Pitch = ViewShakeAmplitudeDeg * Fade * FMath::Sin(Omega * Elapsed);
			const float Yaw = ViewShakeAmplitudeDeg * Fade * FMath::Sin(Omega * 1.13f * Elapsed + 1.3f);
			ViewShakeRot = FRotator(Pitch, Yaw, 0.f);
		}
	}
	else
	{
		ViewShakeRot = FRotator::ZeroRotator;
	}
	ApplyCameraPose();
}

// ---------------------------------------------------------------------------------------------------------------
// Interaction

IGBInteractable* AGBPlayerCharacter::GetFocusedInteractable() const
{
	return Cast<IGBInteractable>(FocusedInteractable.Get());
}

void AGBPlayerCharacter::UpdateFocusedInteractable()
{
	FocusedInteractable = nullptr;

	UWorld* World = GetWorld();
	if (!World || !CanUseGameplayInput())
	{
		return;
	}
	const float Range = ReadStat(GBStat::InteractRange, 250.f);
	if (Range <= 0.f)
	{
		return;
	}

	const FVector Origin = GetAimOrigin();
	const FVector ViewDir = GetAimDirection();

	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);
	ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(GBInteract), false, this);

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps, Origin, FQuat::Identity, ObjectParams, FCollisionShape::MakeSphere(Range), QueryParams);

	AActor* Best = nullptr;
	float BestDot = ReadStat(GBStat::InteractFacingMinDot, 0.3f);
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Actor = Overlap.GetActor();
		if (!Actor || Actor == this || Actor == Best)
		{
			continue;
		}
		IGBInteractable* Interactable = Cast<IGBInteractable>(Actor);
		if (!Interactable || !Interactable->CanInteract(this))
		{
			continue;
		}
		const float Dot = FVector::DotProduct((Actor->GetActorLocation() - Origin).GetSafeNormal(), ViewDir);
		if (Dot > BestDot)
		{
			BestDot = Dot;
			Best = Actor;
		}
	}
	FocusedInteractable = Best;
}

// ---------------------------------------------------------------------------------------------------------------
// State

bool AGBPlayerCharacter::IsDead() const
{
	return Attributes && Attributes->IsDead();
}

bool AGBPlayerCharacter::CanUseGameplayInput() const
{
	return bControlEnabled && !IsDead();
}

bool AGBPlayerCharacter::IsQTEActive() const
{
	return QTE && QTE->IsActive();
}

void AGBPlayerCharacter::RestoreAll(float HealthFraction, float ManaFraction, bool bRefillAmmo, bool bOnlyRaise)
{
	Attributes->RestoreAll(HealthFraction, ManaFraction, bOnlyRaise);
	if (bRefillAmmo)
	{
		Equipment->RefillAllAmmo();
	}
}

void AGBPlayerCharacter::ReleaseHeldInputs()
{
	if (AGBHeldItem* Item = Equipment ? Equipment->GetActiveItem() : nullptr)
	{
		Item->StopPrimary();
		Item->StopSecondary();
	}
	if (Spellbook)
	{
		Spellbook->SetBlockHeld(false);
	}
	bSecondaryHeld = false;
	LastMoveInput = FVector2D::ZeroVector;
	StopJumping();
}

void AGBPlayerCharacter::SetControlEnabled(bool bEnabled)
{
	bControlEnabled = bEnabled;
	if (!bEnabled)
	{
		CancelMantle();
		ReleaseHeldInputs();
		EndDash();
	}
	if (AGBHeldItem* Item = Equipment ? Equipment->GetActiveItem() : nullptr)
	{
		Item->SetActorHiddenInGame(!bEnabled);
	}
}

void AGBPlayerCharacter::HandleDeath(const FGBDamageInfo& KillingBlow)
{
	CancelMantle();
	SetControlEnabled(false);
	Spellbook->CancelSustained();
	QTE->CancelQTE();
	if (AGBGameMode* GameMode = AGBGameMode::Get(this))
	{
		GameMode->HandlePlayerDeath(this);
	}
	else
	{
		UE_LOG(LogGB, Warning, TEXT("GBPlayerCharacter: died without an AGBGameMode, nothing will respawn the player."));
	}
}

void AGBPlayerCharacter::HandlePrecisionRecovery(AActor* Target, const FGBDamageInfo& Info, float DamageDealt)
{
	(void)DamageDealt;
	if (!IsValid(Target) || Target == this || !Info.bHeadshot || Info.Instigator.Get() != this)
	{
		return;
	}
	if (!Stats || !Attributes || IsDead())
	{
		return;
	}
	// Only a blow that killed counts: a head hit that does not kill never heals.
	const UGBAttributeComponent* TargetAttributes = UGBAttributeComponent::Find(Target);
	if (!TargetAttributes || !TargetAttributes->IsDead())
	{
		return;
	}
	const int32 Rank = FMath::Max(0, FMath::RoundToInt(Stats->GetStat(GBStat::PrecisionRecovery, 0.f)));
	if (Rank <= 0)
	{
		return;
	}
	const UGBDataSubsystem* Data = UGBDataSubsystem::Get(this);
	const float HealFraction = Data ? Data->GetGameValue(GBRecovery::HealFraction, 0.03f) : 0.03f;
	const float Cooldown = UGBCombatLibrary::ComputePrecisionRecoveryCooldown(Rank,
		Data ? Data->GetGameValue(GBRecovery::CooldownBase, 7.f) : 7.f,
		Data ? Data->GetGameValue(GBRecovery::CooldownPerRank, 1.f) : 1.f,
		Data ? Data->GetGameValue(GBRecovery::CooldownMin, 1.f) : 1.f);
	const UWorld* World = GetWorld();
	const double Now = World ? World->GetTimeSeconds() : 0.0;
	if (!UGBCombatLibrary::ShouldPrecisionRecover(true, true, Rank, static_cast<float>(Now - LastPrecisionRecoveryTime), Cooldown))
	{
		return;
	}
	LastPrecisionRecoveryTime = Now;
	const float Before = Attributes->GetHealth();
	Attributes->Heal(HealFraction * Attributes->GetMaxHealth());
	const float Healed = Attributes->GetHealth() - Before;
	if (Healed >= 0.5f)
	{
		if (AGBHUD* Hud = AGBHUD::Get(this))
		{
			Hud->ShowMessage(FString::Printf(TEXT("+%d health"), FMath::RoundToInt(Healed)));
		}
	}
}

void AGBPlayerCharacter::FellOutOfWorld(const UDamageType& /*DmgType*/)
{
	// Never Super: the engine would destroy the pawn. The checkpoint respawn brings the player back.
	if (IsDead())
	{
		FreezeMovement(); // already dead (or killed by the first call): just keep it from falling forever
		return;
	}
	if (!Attributes)
	{
		return;
	}

	EndDash();
	FreezeMovement();

	// Friendly fire and invulnerability do not matter here: the hit goes straight to the attributes.
	const bool bWasInvulnerable = Attributes->IsInvulnerable();
	Attributes->SetInvulnerable(false);
	FGBDamageInfo Fall;
	Fall.Amount = FallOutOfWorldDamage;
	Fall.Element = EGBElement::Physical;
	Fall.SourceId = FellOutOfWorldSourceId;
	Attributes->ApplyDamage(Fall); // HitDirection is zero, so a raised shield cannot block it
	Attributes->SetInvulnerable(bWasInvulnerable);

	if (!IsDead())
	{
		// Only a data setup with DamageTaken 0 gets here: respawn directly rather than fall forever.
		UE_LOG(LogGB, Warning, TEXT("GBPlayerCharacter: fell out of the world but could not be killed (DamageTaken 0?); respawning at the checkpoint."));
		if (UGBProgressSubsystem* Progress = UGBProgressSubsystem::Get(this))
		{
			if (Progress->RestoreCheckpoint(this))
			{
				return;
			}
		}
		UnfreezeMovement();
	}
}

void AGBPlayerCharacter::FreezeMovement()
{
	CancelMantle();
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->StopMovementImmediately();
		if (!bMovementFrozen)
		{
			MoveComp->DisableMovement();
		}
	}
	bMovementFrozen = true;
}

void AGBPlayerCharacter::UnfreezeMovement()
{
	if (!bMovementFrozen)
	{
		return;
	}
	bMovementFrozen = false;
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		// Falling settles into walking on the first floor check.
		MoveComp->SetMovementMode(MOVE_Falling);
	}
}

void AGBPlayerCharacter::SaveToSnapshot(FGBPlayerSnapshot& Snapshot) const
{
	Snapshot.bValid = true;
	Snapshot.Transform = GetActorTransform();
	Snapshot.ControlRotation = Controller ? Controller->GetControlRotation() : GetActorRotation();
	if (const UGBEraSubsystem* Era = UGBEraSubsystem::Get(this))
	{
		Snapshot.Era = Era->GetCurrentEra();
	}
	Snapshot.Health = Attributes->GetHealth();
	Snapshot.Mana = Attributes->GetMana();

	Equipment->SaveToSnapshot(Snapshot);
	Spellbook->SaveToSnapshot(Snapshot);
	Inventory->SaveToSnapshot(Snapshot);
	SkillTree->SaveToSnapshot(Snapshot);
}

void AGBPlayerCharacter::LoadFromSnapshot(const FGBPlayerSnapshot& Snapshot)
{	if (QTE->IsActive())
	{
		QTE->CancelQTE();
	}
	CancelMantle();
	EndDash();

	SetActorTransform(Snapshot.Transform, false, nullptr, ETeleportType::TeleportPhysics);
	if (Controller)
	{
		Controller->SetControlRotation(Snapshot.ControlRotation);
	}
	if (UGBEraSubsystem* Era = UGBEraSubsystem::Get(this))
	{
		Era->SetEraImmediate(Snapshot.Era);
	}

	// Components first: armor and skill nodes change MaxHealth / MaxMana, which clamp the values set below.
	Equipment->LoadFromSnapshot(Snapshot);
	Spellbook->LoadFromSnapshot(Snapshot);
	Inventory->LoadFromSnapshot(Snapshot);
	SkillTree->LoadFromSnapshot(Snapshot);

	Attributes->RestoreAll(); // revives
	const float Health = Snapshot.Health > 0.f ? Snapshot.Health : Attributes->GetMaxHealth();
	Attributes->SetHealthAndMana(Health, Snapshot.Mana);

	UnfreezeMovement();
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->StopMovementImmediately();
	}
	PendingRecoil = FRotator::ZeroRotator;
	LastMoveInput = FVector2D::ZeroVector;
	SetControlEnabled(true);
	ApplyMovementStats();
	ViewShakeTimeLeft = 0.f;
	ViewShakeDuration = 0.f;
	ViewShakeAmplitudeDeg = 0.f;
	ViewShakeRot = FRotator::ZeroRotator;
	ApplyCameraPose();

	if (AGBPlayerController* PC = Cast<AGBPlayerController>(Controller))
	{
		if (PC->GetOpenMenu() == EGBMenu::Death)
		{
			PC->SetMenu(EGBMenu::None);
		}
	}
}

void AGBPlayerCharacter::RespawnAt(const FGBPlayerSnapshot& Snapshot)
{
	// Death never rolls back what the player owns: only place, era and resources are restored.
	if (QTE && QTE->IsActive())
	{
		QTE->CancelQTE();
	}
	if (Spellbook)
	{
		Spellbook->CancelSustained();
	}
	CancelMantle();
	EndDash();

	SetActorTransform(Snapshot.Transform, false, nullptr, ETeleportType::TeleportPhysics);
	if (Controller)
	{
		Controller->SetControlRotation(Snapshot.ControlRotation);
	}
	if (UGBEraSubsystem* Era = UGBEraSubsystem::Get(this))
	{
		Era->SetEraImmediate(Snapshot.Era);
	}

	if (StatusEffects)
	{
		StatusEffects->RemoveAllEffects();
	}
	// Checkpoint respawn always starts from the restore fractions (health and mana are set, not raised).
	const UGBDataSubsystem* RespawnData = UGBDataSubsystem::Get(this);
	const float RespawnHealth = RespawnData ? RespawnData->GetGameValue(GBGameValue::RespawnHealthFraction, 1.f) : 1.f;
	const float RespawnMana = RespawnData ? RespawnData->GetGameValue(GBGameValue::RespawnManaFraction, 1.f) : 1.f;
	const bool bRespawnRefill = RespawnData ? RespawnData->GetGameValue(GBGameValue::RespawnRefillAmmo, 1.f) > 0.5f : true;
	RestoreAll(RespawnHealth, RespawnMana, bRespawnRefill, /*bOnlyRaise*/ false);

	UnfreezeMovement();
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->StopMovementImmediately();
	}
	PendingRecoil = FRotator::ZeroRotator;
	LastMoveInput = FVector2D::ZeroVector;
	SetControlEnabled(true);
	ViewShakeTimeLeft = 0.f;
	ViewShakeDuration = 0.f;
	ViewShakeAmplitudeDeg = 0.f;
	ViewShakeRot = FRotator::ZeroRotator;
	ApplyCameraPose();

	if (AGBPlayerController* PC = Cast<AGBPlayerController>(Controller))
	{
		if (PC->GetOpenMenu() == EGBMenu::Death)
		{
			PC->SetMenu(EGBMenu::None);
		}
	}
}

// ---------------------------------------------------------------------------------------------------------------
// Input

void AGBPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (!InputSet)
	{
		InputSet = NewObject<UGBInputSet>(this);
		InputSet->Build();
	}

	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EIC)
	{
		UE_LOG(LogGB, Error, TEXT("GBPlayerCharacter: the input component is not an UEnhancedInputComponent (check DefaultInput.ini)."));
		return;
	}

	const UGBInputSet* S = InputSet;

	EIC->BindAction(S->Move, ETriggerEvent::Triggered, this, &ThisClass::InputMove);
	EIC->BindAction(S->Move, ETriggerEvent::Completed, this, &ThisClass::InputMoveCompleted);
	EIC->BindAction(S->Look, ETriggerEvent::Triggered, this, &ThisClass::InputLook);
	EIC->BindAction(S->LookStick, ETriggerEvent::Triggered, this, &ThisClass::InputLookStick);

	EIC->BindAction(S->Jump, ETriggerEvent::Started, this, &ThisClass::InputJump);
	EIC->BindAction(S->Jump, ETriggerEvent::Completed, this, &ThisClass::InputJumpReleased);
	EIC->BindAction(S->Dash, ETriggerEvent::Started, this, &ThisClass::InputDash);

	EIC->BindAction(S->Primary, ETriggerEvent::Started, this, &ThisClass::InputPrimaryPressed);
	EIC->BindAction(S->Primary, ETriggerEvent::Completed, this, &ThisClass::InputPrimaryReleased);
	EIC->BindAction(S->Secondary, ETriggerEvent::Started, this, &ThisClass::InputSecondaryPressed);
	EIC->BindAction(S->Secondary, ETriggerEvent::Completed, this, &ThisClass::InputSecondaryReleased);

	EIC->BindAction(S->SpellSlot1, ETriggerEvent::Started, this, &ThisClass::InputSpellSlot, 0);
	EIC->BindAction(S->SpellSlot2, ETriggerEvent::Started, this, &ThisClass::InputSpellSlot, 1);
	EIC->BindAction(S->SpellSlot3, ETriggerEvent::Started, this, &ThisClass::InputSpellSlot, 2);
	EIC->BindAction(S->SpellSlot4, ETriggerEvent::Started, this, &ThisClass::InputSpellSlot, 3);
	EIC->BindAction(S->SpellSlot5, ETriggerEvent::Started, this, &ThisClass::InputSpellSlot, 4);
	EIC->BindAction(S->Enchant, ETriggerEvent::Started, this, &ThisClass::InputEnchant);
	EIC->BindAction(S->Block, ETriggerEvent::Started, this, &ThisClass::InputBlockPressed);
	EIC->BindAction(S->Block, ETriggerEvent::Completed, this, &ThisClass::InputBlockReleased);

	EIC->BindAction(S->Reload, ETriggerEvent::Started, this, &ThisClass::InputReload);
	EIC->BindAction(S->Interact, ETriggerEvent::Started, this, &ThisClass::InputInteract);
	EIC->BindAction(S->QuickUse, ETriggerEvent::Started, this, &ThisClass::InputQuickUse);
	EIC->BindAction(S->CycleQuickUse, ETriggerEvent::Started, this, &ThisClass::InputCycleQuickUse);
	EIC->BindAction(S->SwitchWeapon, ETriggerEvent::Started, this, &ThisClass::InputSwitchWeapon);

	EIC->BindAction(S->QTEUp, ETriggerEvent::Started, this, &ThisClass::InputQTEKey, static_cast<int32>(EGBQTEKey::Up));
	EIC->BindAction(S->QTELeft, ETriggerEvent::Started, this, &ThisClass::InputQTEKey, static_cast<int32>(EGBQTEKey::Left));
	EIC->BindAction(S->QTEDown, ETriggerEvent::Started, this, &ThisClass::InputQTEKey, static_cast<int32>(EGBQTEKey::Down));
	EIC->BindAction(S->QTERight, ETriggerEvent::Started, this, &ThisClass::InputQTEKey, static_cast<int32>(EGBQTEKey::Right));

	EIC->BindAction(S->FaceUp, ETriggerEvent::Started, this, &ThisClass::InputFace, EGBQTEKey::Up);
	EIC->BindAction(S->FaceLeft, ETriggerEvent::Started, this, &ThisClass::InputFace, EGBQTEKey::Left);
	EIC->BindAction(S->FaceDown, ETriggerEvent::Started, this, &ThisClass::InputFace, EGBQTEKey::Down);
	EIC->BindAction(S->FaceRight, ETriggerEvent::Started, this, &ThisClass::InputFace, EGBQTEKey::Right);
	EIC->BindAction(S->FaceDown, ETriggerEvent::Completed, this, &ThisClass::InputJumpReleased);

	if (AGBPlayerController* PC = Cast<AGBPlayerController>(Controller))
	{
		PC->SetupInput(InputSet);
	}
	else
	{
		UE_LOG(LogGB, Warning, TEXT("GBPlayerCharacter: not possessed by an AGBPlayerController, menus and mapping contexts are not set up."));
	}
}

void AGBPlayerCharacter::InputMove(const FInputActionValue& Value)
{
	if (bMantling)
	{
		LastMoveInput = FVector2D::ZeroVector;
		return;
	}
	if (IsQTEActive())
	{
		return; // W A S D belong to the QTE; LastMoveInput stays as it was
	}
	if (!CanUseGameplayInput())
	{
		LastMoveInput = FVector2D::ZeroVector;
		return;
	}
	LastMoveInput = Value.Get<FVector2D>();
	LastMoveInputFrame = GFrameCounter;
	if (!bDashing)
	{
		ApplyMoveInput(LastMoveInput);
	}
}

void AGBPlayerCharacter::InputMoveCompleted(const FInputActionValue& Value)
{
	if (!IsQTEActive())
	{
		LastMoveInput = FVector2D::ZeroVector;
	}
}

void AGBPlayerCharacter::InputLook(const FInputActionValue& Value)
{
	if (!CanUseGameplayInput())
	{
		return;
	}
	const FVector2D Delta = Value.Get<FVector2D>(); // mouse: x right, y up
	if (IsQTEActive())
	{
		// Camera locked; the mouse draws the gesture in screen space (y down).
		QTE->HandleGestureInput(FVector2D(Delta.X, -Delta.Y) / MouseUnitsPerPixel);
		return;
	}
	AddLookDelta(static_cast<float>(Delta.X) * ReadStat(GBStat::LookSensitivity, 2.5f), static_cast<float>(Delta.Y) * ReadStat(GBStat::LookSensitivity, 2.5f));
}

void AGBPlayerCharacter::InputLookStick(const FInputActionValue& Value)
{
	if (!CanUseGameplayInput())
	{
		return;
	}
	const FVector2D Stick = Value.Get<FVector2D>(); // x right, y up
	if (IsQTEActive())
	{
		// Undilated: the world runs at 0.1 speed during the QTE, the gesture runs in real time.
		const float Scale = QTE->GetStickGestureScale();
		const float RealDelta = static_cast<float>(FApp::GetDeltaTime());
		QTE->HandleGestureInput(FVector2D(Stick.X, -Stick.Y) * Scale * RealDelta);
		return;
	}
	const UWorld* World = GetWorld();
	// Real time: the QTE runs in real time while the world is slowed.
	const float RealDelta = World ? World->DeltaRealTimeSeconds : 0.f;
	AddLookDelta(static_cast<float>(Stick.X) * ReadStat(GBStat::StickYawRate, 200.f) * RealDelta, static_cast<float>(Stick.Y) * ReadStat(GBStat::StickPitchRate, 140.f) * RealDelta);
}

void AGBPlayerCharacter::InputFace(EGBQTEKey Key)
{
	if (!CanUseGameplayInput())
	{
		return;
	}
	if (IsQTEActive())
	{
		QTE->HandleKey(Key);
		return;
	}
	// GDD 9: hold L2 + Cross, Circle, Square, Triangle = spell slots 2-5 with the staff.
	if (bSecondaryHeld && Equipment->IsHoldingStaff())
	{
		// L2 also raises a summoned shield (Block). A face button while it is up means "cast": lower it first,
		// otherwise slots 2-5 would be unusable on gamepad for the whole life of the shield.
		if (IsBlocking())
		{
			Spellbook->SetBlockHeld(false);
		}
		int32 Slot = 1;
		switch (Key)
		{
		case EGBQTEKey::Down:  Slot = 1; break;
		case EGBQTEKey::Right: Slot = 2; break;
		case EGBQTEKey::Left:  Slot = 3; break;
		case EGBQTEKey::Up:    Slot = 4; break;
		}
		Spellbook->TryCastSlot(Slot);
		return;
	}
	switch (Key)
	{
	case EGBQTEKey::Down:  InputJump(); break;
	case EGBQTEKey::Right: InputDash(); break;
	case EGBQTEKey::Left:  InputReload(); break;
	case EGBQTEKey::Up:    InputInteract(); break;
	}
}

void AGBPlayerCharacter::PressJump()
{
	if (!CanUseGameplayInput() || IsQTEActive() || bDashing || bMantling)
	{
		return;
	}
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		if (MoveComp->IsMovingOnGround() && TryStartMantle(/*bFromGround*/ true))
		{
			return;
		}
	}
	Jump();
}

void AGBPlayerCharacter::InputJump()
{
	PressJump();
}

void AGBPlayerCharacter::InputJumpReleased()
{
	StopJumping();
}

void AGBPlayerCharacter::InputDash()
{
	if (!CanUseGameplayInput() || IsQTEActive() || bDashing || bMantling || GetDashCooldownRemaining() > 0.f)
	{
		return;
	}
	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	UWorld* World = GetWorld();
	if (!MoveComp || !World)
	{
		return;
	}

	const float Distance = ReadStat(GBStat::DashDistance, 700.f);
	const float Duration = ReadStat(GBStat::DashDuration, 0.18f);
	const float Cooldown = ReadStat(GBStat::DashCooldown, 1.2f);
	if (Distance <= 0.f || Duration <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const FRotationMatrix YawMatrix(FRotator(0.f, GetControlRotation().Yaw, 0.f));
	const FVector Forward = YawMatrix.GetUnitAxis(EAxis::X);
	FVector Direction = GetMoveInputWorldDirection(LastMoveInput);
	if (Direction.IsNearlyZero())
	{
		Direction = Forward;
	}

	bDashing = true;
	DashTimeLeft = Duration;
	DashVelocity = Direction * (Distance / Duration);
	DashReadyTime = World->GetTimeSeconds() + FMath::Max(0.f, Cooldown);

	// Flying = no gravity and no ground friction while the velocity is forced each tick.
	const int32 SavedJumpCount = JumpCurrentCount;
	MoveComp->SetMovementMode(MOVE_Flying);
	JumpCurrentCount = SavedJumpCount;
	MoveComp->Velocity = DashVelocity;
}

void AGBPlayerCharacter::InputPrimaryPressed()
{
	if (!CanUseGameplayInput() || IsQTEActive() || IsBlocking() || bMantling)
	{
		return;
	}
	if (AGBHeldItem* Item = Equipment->GetActiveItem())
	{
		Item->StartPrimary();
	}
}

void AGBPlayerCharacter::InputPrimaryReleased()
{
	if (AGBHeldItem* Item = Equipment->GetActiveItem())
	{
		Item->StopPrimary();
	}
}

void AGBPlayerCharacter::InputSecondaryPressed()
{
	if (bMantling)
	{
		bSecondaryHeld = false;
		return;
	}
	bSecondaryHeld = true;
	if (!CanUseGameplayInput() || IsQTEActive())
	{
		return;
	}
	// With the staff, right click is spell slot 1 (its own action) and L2 is the spell modifier: nothing here.
	if (Equipment->IsHoldingGun())
	{
		if (AGBHeldItem* Item = Equipment->GetActiveItem())
		{
			Item->StartSecondary();
		}
	}
}

void AGBPlayerCharacter::InputSecondaryReleased()
{
	bSecondaryHeld = false;
	if (AGBHeldItem* Item = Equipment->GetActiveItem())
	{
		Item->StopSecondary();
	}
}

void AGBPlayerCharacter::InputReload()
{
	if (!CanUseGameplayInput() || IsQTEActive() || bMantling)
	{
		return;
	}
	if (AGBHeldItem* Item = Equipment->GetActiveItem())
	{
		Item->Reload();
	}
}

void AGBPlayerCharacter::InputSwitchWeapon(const FInputActionValue& Value)
{
	if (!CanUseGameplayInput() || IsQTEActive() || bMantling)
	{
		return;
	}
	const float Axis = Value.Get<float>();
	if (FMath::Abs(Axis) < AxisStepThreshold)
	{
		return;
	}
	Equipment->CycleSlot(Axis > 0.f ? 1 : -1);
}

void AGBPlayerCharacter::InputSpellSlot(int32 Slot)
{
	if (!CanUseGameplayInput() || IsQTEActive() || IsBlocking() || bMantling || !Equipment->IsHoldingStaff())
	{
		return;
	}
	Spellbook->TryCastSlot(Slot);
}

void AGBPlayerCharacter::InputEnchant()
{
	if (!CanUseGameplayInput() || IsQTEActive() || IsBlocking() || bMantling || !Equipment->IsHoldingGun())
	{
		return;
	}
	Spellbook->TryCastEnchant();
}

bool AGBPlayerCharacter::IsSpellInputHeld(FName SpellId) const
{
	if (!Spellbook || SpellId.IsNone() || !InputSet)
	{
		return true;
	}
	int32 Slot = INDEX_NONE;
	for (int32 Index = 0; Index < UGBSpellbookComponent::NumSlots; ++Index)
	{
		if (Spellbook->GetSlotSpell(Index) == SpellId)
		{
			Slot = Index;
			break;
		}
	}
	if (Slot == INDEX_NONE)
	{
		return true;
	}
	const UInputAction* SlotActions[UGBSpellbookComponent::NumSlots] = {
		InputSet->SpellSlot1, InputSet->SpellSlot2, InputSet->SpellSlot3, InputSet->SpellSlot4, InputSet->SpellSlot5
	};
	const UInputAction* SlotAction = SlotActions[Slot];
	const APlayerController* PC = Cast<APlayerController>(GetController());
	const UEnhancedPlayerInput* EI = PC ? Cast<UEnhancedPlayerInput>(PC->PlayerInput) : nullptr;
	if (!EI || !SlotAction)
	{
		return true;
	}
	if (EI->GetActionValue(SlotAction).Get<bool>())
	{
		return true;
	}
	// Gamepad chord for slots 2-5 (see InputFace): L2 held plus the face button of that slot.
	if (Slot >= 1 && Slot <= 4 && InputSet->Secondary)
	{
		const UInputAction* FaceActions[4] = { InputSet->FaceDown, InputSet->FaceRight, InputSet->FaceLeft, InputSet->FaceUp };
		const UInputAction* FaceAction = FaceActions[Slot - 1];
		if (FaceAction && EI->GetActionValue(InputSet->Secondary).Get<bool>() && EI->GetActionValue(FaceAction).Get<bool>())
		{
			return true;
		}
	}
	return false;
}

bool AGBPlayerCharacter::IsBlocking() const
{
	return Spellbook && Spellbook->IsBlockRaised();
}

void AGBPlayerCharacter::InputBlockPressed()
{
	if (!CanUseGameplayInput() || IsQTEActive() || bMantling)
	{
		return;
	}
	Spellbook->SetBlockHeld(true);
	if (IsBlocking())
	{
		// Raising the shield interrupts a held attack (an automatic staff keeps firing otherwise).
		if (AGBHeldItem* Item = Equipment->GetActiveItem())
		{
			Item->StopPrimary();
		}
	}
}

void AGBPlayerCharacter::InputBlockReleased()
{
	if (Spellbook)
	{
		Spellbook->SetBlockHeld(false);
	}
}

void AGBPlayerCharacter::InputInteract()
{
	if (!CanUseGameplayInput() || IsQTEActive() || bMantling)
	{
		return;
	}
	IGBInteractable* Interactable = GetFocusedInteractable();
	if (Interactable && Interactable->CanInteract(this))
	{
		Interactable->Interact(this);
	}
}

void AGBPlayerCharacter::InputQuickUse()
{
	if (!CanUseGameplayInput() || IsQTEActive() || bMantling)
	{
		return;
	}
	Inventory->UseQuickItem();
}

void AGBPlayerCharacter::InputCycleQuickUse(const FInputActionValue& Value)
{
	if (!CanUseGameplayInput() || IsQTEActive() || bMantling)
	{
		return;
	}
	const float Axis = Value.Get<float>();
	if (FMath::Abs(Axis) < AxisStepThreshold)
	{
		return;
	}
	Inventory->CycleQuickUse(Axis > 0.f ? 1 : -1);
}

void AGBPlayerCharacter::InputQTEKey(int32 KeyIndex)
{
	if (!CanUseGameplayInput() || !IsQTEActive())
	{
		return;
	}
	QTE->HandleKey(static_cast<EGBQTEKey>(FMath::Clamp(KeyIndex, 0, 3)));
}
