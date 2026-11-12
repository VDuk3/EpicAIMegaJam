#include "Player/GBPlayerCharacter.h"

#include "Player/GBInputSet.h"
#include "Player/GBPlayerController.h"
#include "Core/GBStatNames.h"
#include "Core/GBGameMode.h"
#include "Data/GBDataSubsystem.h"
#include "Stats/GBStatsComponent.h"
#include "Stats/GBAttributeComponent.h"
#include "Combat/GBStatusEffectComponent.h"
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
	constexpr float MouseDegreesPerUnit = 2.5f;
	constexpr float MouseUnitsPerPixel = 0.07f;
	constexpr float StickYawDegreesPerSecond = 200.f;
	constexpr float StickPitchDegreesPerSecond = 140.f;
	constexpr float MaxLookPitch = 89.f;
	/** Ignore axis noise on wheel / D-pad style 1D actions. */
	constexpr float AxisStepThreshold = 0.1f;
	/** Interactables must be roughly in front of the view. */
	constexpr float InteractFacingMinDot = 0.3f;
	/** Snappy shooter movement: acceleration and braking as multiples of the walk speed (per second). */
	constexpr float AccelerationPerSpeed = 10.f;
	constexpr float BrakingPerSpeed = 8.f;
	constexpr float GroundFrictionValue = 8.f;
	/** Held item anchor and muzzle offsets from the camera (forward, right, up), cm. */
	const FVector HeldItemOffset(30.f, 15.f, -15.f);
	const FVector MuzzleOffset(40.f, 15.f, -12.f);

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
		MoveComp->GroundFriction = GroundFrictionValue;
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
		const TArray<FName>& StartingWeapons = Data->GetPlayer().StartingWeapons;
		for (int32 Index = 0; Index < StartingWeapons.Num(); ++Index)
		{
			if (!Equipment->GiveWeapon(StartingWeapons[Index], Index == 0, false))
			{
				UE_LOG(LogGB, Warning, TEXT("GBPlayerCharacter: starting weapon '%s' is not in Weapons.json."), *StartingWeapons[Index].ToString());
			}
		}
	}

	Spellbook->InitializeFromData();
	Inventory->InitializeFromData();
	SkillTree->InitializeFromData();

	Stats->OnStatsChanged.AddUObject(this, &AGBPlayerCharacter::ApplyMovementStats);
	Attributes->OnDeath.AddUObject(this, &AGBPlayerCharacter::HandleDeath);
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

	TickRecoil(DeltaSeconds);
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
	MoveComp->AirControl = FMath::Max(0.f, ReadStat(GBStat::AirControl, 0.6f));
	MoveComp->GravityScale = ReadStat(GBStat::GravityScale, 1.6f);
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
	Camera->SetRelativeLocation(FVector(0.f, 0.f, BaseEyeHeight));
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
	MoveComp->MaxAcceleration = FMath::Max(1.f, Speed * AccelerationPerSpeed);
	MoveComp->BrakingDecelerationWalking = Speed * BrakingPerSpeed;
	MoveComp->GroundFriction = GroundFrictionValue;
}

void AGBPlayerCharacter::SetMoveSpeedScale(float Scale)
{
	MoveSpeedScale = FMath::Max(0.f, Scale);
	UpdateWalkSpeed();
}

void AGBPlayerCharacter::ApplyMoveInput(const FVector2D& Input)
{
	const FRotator YawRotation(0.f, GetControlRotation().Yaw, 0.f);
	const FRotationMatrix YawMatrix(YawRotation);
	AddMovementInput(YawMatrix.GetUnitAxis(EAxis::X), Input.Y);
	AddMovementInput(YawMatrix.GetUnitAxis(EAxis::Y), Input.X);
}

void AGBPlayerCharacter::AddLookDelta(float YawDegrees, float PitchDegrees)
{
	if (!Controller)
	{
		return;
	}
	FRotator Rotation = Controller->GetControlRotation();
	Rotation.Yaw = FRotator::NormalizeAxis(Rotation.Yaw + YawDegrees);
	Rotation.Pitch = FMath::Clamp(FRotator::NormalizeAxis(Rotation.Pitch + PitchDegrees), -static_cast<double>(MaxLookPitch), static_cast<double>(MaxLookPitch));
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
		MoveComp->Velocity = DashVelocity.GetSafeNormal() * MoveComp->MaxWalkSpeed;
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
	float BestDot = InteractFacingMinDot;
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

void AGBPlayerCharacter::RestoreAll()
{
	Attributes->RestoreAll();
	Equipment->RefillAllAmmo();
}

void AGBPlayerCharacter::ReleaseHeldInputs()
{
	if (AGBHeldItem* Item = Equipment ? Equipment->GetActiveItem() : nullptr)
	{
		Item->StopPrimary();
		Item->StopSecondary();
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

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->StopMovementImmediately();
	}
	PendingRecoil = FRotator::ZeroRotator;
	LastMoveInput = FVector2D::ZeroVector;
	SetControlEnabled(true);
	ApplyMovementStats();

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
	RestoreAll();

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->StopMovementImmediately();
	}
	PendingRecoil = FRotator::ZeroRotator;
	LastMoveInput = FVector2D::ZeroVector;
	SetControlEnabled(true);

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
	AddLookDelta(static_cast<float>(Delta.X) * MouseDegreesPerUnit, static_cast<float>(Delta.Y) * MouseDegreesPerUnit);
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
	AddLookDelta(static_cast<float>(Stick.X) * StickYawDegreesPerSecond * RealDelta, static_cast<float>(Stick.Y) * StickPitchDegreesPerSecond * RealDelta);
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

void AGBPlayerCharacter::InputJump()
{
	if (!CanUseGameplayInput() || IsQTEActive() || bDashing)
	{
		return;
	}
	Jump();
}

void AGBPlayerCharacter::InputJumpReleased()
{
	StopJumping();
}

void AGBPlayerCharacter::InputDash()
{
	if (!CanUseGameplayInput() || IsQTEActive() || bDashing || GetDashCooldownRemaining() > 0.f)
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
	FVector Direction = Forward * LastMoveInput.Y + YawMatrix.GetUnitAxis(EAxis::Y) * LastMoveInput.X;
	Direction.Z = 0.f;
	if (!Direction.Normalize())
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
	if (!CanUseGameplayInput() || IsQTEActive())
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
	if (!CanUseGameplayInput() || IsQTEActive())
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
	if (!CanUseGameplayInput() || IsQTEActive())
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
	if (!CanUseGameplayInput() || IsQTEActive() || !Equipment->IsHoldingStaff())
	{
		return;
	}
	Spellbook->TryCastSlot(Slot);
}

void AGBPlayerCharacter::InputEnchant()
{
	if (!CanUseGameplayInput() || IsQTEActive() || !Equipment->IsHoldingGun())
	{
		return;
	}
	Spellbook->TryCastEnchant();
}

void AGBPlayerCharacter::InputInteract()
{
	if (!CanUseGameplayInput() || IsQTEActive())
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
	if (!CanUseGameplayInput() || IsQTEActive())
	{
		return;
	}
	Inventory->UseQuickItem();
}

void AGBPlayerCharacter::InputCycleQuickUse(const FInputActionValue& Value)
{
	if (!CanUseGameplayInput() || IsQTEActive())
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
