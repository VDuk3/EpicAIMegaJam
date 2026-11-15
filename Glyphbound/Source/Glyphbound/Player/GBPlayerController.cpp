#include "Player/GBPlayerController.h"
#include "World/GBCutsceneSubsystem.h"

#include "Player/GBInputSet.h"
#include "Player/GBPlayerCharacter.h"
#include "Core/GBTypes.h"
#include "Core/GBGameMode.h"
#include "UI/GBHUD.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "Engine/LocalPlayer.h"

namespace
{
	// The menu context sits above the gameplay one, so during the death screen (both may be active) menu keys win.
	constexpr int32 GameplayContextPriority = 0;
	constexpr int32 MenuContextPriority = 1;
	constexpr float MenuAxisStepThreshold = 0.1f;

	bool IsPausingMenu(EGBMenu Menu)
	{
		return Menu == EGBMenu::Pause || Menu == EGBMenu::Inventory || Menu == EGBMenu::SkillTrees || Menu == EGBMenu::MainMenu;
	}
}

AGBPlayerController::AGBPlayerController()
{
	// Hidden while playing; shown while a menu is open (ApplyMouseMode).
	bShowMouseCursor = false;
}

void AGBPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (IsLocalController())
	{
		ApplyMouseMode(OpenMenu != EGBMenu::None);
	}
}

void AGBPlayerController::TickActor(float DeltaTime, ELevelTick TickType, FActorTickFunction& ThisTickFunction)
{
	Super::TickActor(DeltaTime, TickType, ThisTickFunction);
	// The title screen opens on the first tick with a HUD (the HUD may not exist yet at BeginPlay), once per run.
	if (!bMainMenuChecked && IsLocalController() && GetHUD())
	{
		bMainMenuChecked = true;
		AGBHUD::ApplySavedOptions(this);
		if (OpenMenu == EGBMenu::None && AGBHUD::ConsumeMainMenuOpening(this))
		{
			SetMenu(EGBMenu::MainMenu);
		}
	}
	if (bCenterCursorPending)
	{
		bCenterCursorPending = false;
		if (OpenMenu != EGBMenu::None)
		{
			CenterMouseCursor();
		}
	}
}

void AGBPlayerController::ApplyMouseMode(bool bMenuOpen)
{
	if (!IsLocalController())
	{
		return;
	}
	if (bMenuOpen)
	{
		bShowMouseCursor = true;
		FInputModeGameAndUI Mode;
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Mode.SetHideCursorDuringCapture(false);
		SetInputMode(Mode);
		return;
	}
	bShowMouseCursor = false;
	// The click that gives the mouse back to the game is swallowed, so it never fires the weapon.
	FInputModeGameOnly Mode;
	Mode.SetConsumeCaptureMouseDown(true);
	SetInputMode(Mode);
}

void AGBPlayerController::CenterMouseCursor()
{
	int32 SizeX = 0;
	int32 SizeY = 0;
	GetViewportSize(SizeX, SizeY);
	if (SizeX > 0 && SizeY > 0)
	{
		SetMouseLocation(SizeX / 2, SizeY / 2);
	}
}

void AGBPlayerController::SetupInput(UGBInputSet* InInputSet)
{
	if (!InInputSet)
	{
		UE_LOG(LogGB, Warning, TEXT("GBPlayerController: SetupInput called without an input set."));
		return;
	}
	const bool bNewSet = InputSet != InInputSet;
	InputSet = InInputSet;

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		if (OpenMenu == EGBMenu::None)
		{
			Subsystem->AddMappingContext(InputSet->GameplayContext, GameplayContextPriority);
		}
		else
		{
			Subsystem->AddMappingContext(InputSet->MenuContext, MenuContextPriority);
		}
	}
	else
	{
		UE_LOG(LogGB, Warning, TEXT("GBPlayerController: no Enhanced Input subsystem, input will not work."));
	}

	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent);
	if (!EIC)
	{
		UE_LOG(LogGB, Error, TEXT("GBPlayerController: the controller input component is not an UEnhancedInputComponent."));
		return;
	}
	if (!bNewSet && EIC->GetActionEventBindings().Num() > 0)
	{
		return; // already bound to this set
	}
	EIC->ClearActionBindings();

	EIC->BindAction(InputSet->Inventory, ETriggerEvent::Started, this, &ThisClass::InputInventory);
	EIC->BindAction(InputSet->SkillTrees, ETriggerEvent::Started, this, &ThisClass::InputSkillTrees);
	EIC->BindAction(InputSet->Pause, ETriggerEvent::Started, this, &ThisClass::InputPause);
	EIC->BindAction(InputSet->MenuNavigate, ETriggerEvent::Started, this, &ThisClass::InputMenuNavigate);
	EIC->BindAction(InputSet->MenuConfirm, ETriggerEvent::Started, this, &ThisClass::InputMenuConfirm);
	EIC->BindAction(InputSet->MenuBack, ETriggerEvent::Started, this, &ThisClass::InputMenuBack);
	EIC->BindAction(InputSet->MenuTabKey, ETriggerEvent::Started, this, &ThisClass::InputMenuTabKey);
	EIC->BindAction(InputSet->MenuTab, ETriggerEvent::Started, this, &ThisClass::InputMenuTab);
	EIC->BindAction(InputSet->MenuReset, ETriggerEvent::Started, this, &ThisClass::InputMenuReset);
	EIC->BindAction(InputSet->MenuClick, ETriggerEvent::Started, this, &ThisClass::InputMenuClick);
}

void AGBPlayerController::SetMenu(EGBMenu Menu)
{
	if (Menu == OpenMenu)
	{
		return;
	}
	// A cutscene owns the screen: no menu opens over it (the death screen still can).
	if (Menu != EGBMenu::None && Menu != EGBMenu::Death && Menu != EGBMenu::MainMenu)
	{
		const UGBCutsceneSubsystem* Cutscene = UGBCutsceneSubsystem::Get(this);
		if (Cutscene && Cutscene->IsPlaying())
		{
			return;
		}
	}
	const EGBMenu OldMenu = OpenMenu;
	AGBHUD* HUD = Cast<AGBHUD>(GetHUD());

	if (OldMenu != EGBMenu::None && HUD)
	{
		HUD->OnMenuClosed(OldMenu);
	}
	OpenMenu = Menu;

	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	if (Menu == EGBMenu::None)
	{
		SetPause(false);
		if (Subsystem && InputSet)
		{
			Subsystem->RemoveMappingContext(InputSet->MenuContext);
			// A key or button still held from the menu (the click that closed it) does nothing until released.
			FModifyContextOptions Options;
			Options.bIgnoreAllPressedKeysUntilRelease = true;
			Subsystem->AddMappingContext(InputSet->GameplayContext, GameplayContextPriority, Options);
		}
		bCenterCursorPending = false;
		ApplyMouseMode(false);
		return;
	}

	if (OldMenu == EGBMenu::None)
	{
		if (AGBPlayerCharacter* PlayerCharacter = Cast<AGBPlayerCharacter>(GetPawn()))
		{
			PlayerCharacter->ReleaseHeldInputs();
		}
	}
	if (Subsystem && InputSet)
	{
		// The death screen also drops the gameplay keys: nothing to do in the world while dead.
		Subsystem->RemoveMappingContext(InputSet->GameplayContext);
		Subsystem->AddMappingContext(InputSet->MenuContext, MenuContextPriority);
	}
	SetPause(IsPausingMenu(Menu));

	ApplyMouseMode(true);
	if (OldMenu == EGBMenu::None)
	{
		CenterMouseCursor();
		bCenterCursorPending = true;
	}

	if (HUD)
	{
		HUD->OnMenuOpened(Menu);
	}
}

void AGBPlayerController::ToggleMenu(EGBMenu Menu)
{
	SetMenu(OpenMenu == Menu ? EGBMenu::None : Menu);
}

void AGBPlayerController::InputInventory()
{
	if (OpenMenu != EGBMenu::Death && OpenMenu != EGBMenu::MainMenu)
	{
		ToggleMenu(EGBMenu::Inventory);
	}
}

void AGBPlayerController::InputSkillTrees()
{
	if (OpenMenu != EGBMenu::Death && OpenMenu != EGBMenu::MainMenu)
	{
		ToggleMenu(EGBMenu::SkillTrees);
	}
}

void AGBPlayerController::InputPause()
{
	if (OpenMenu == EGBMenu::Death)
	{
		return;
	}
	if (OpenMenu == EGBMenu::MainMenu)
	{
		// The title screen never closes into the game by itself: Start / Esc go back a page.
		if (AGBHUD* HUD = Cast<AGBHUD>(GetHUD()))
		{
			HUD->MenuBack();
		}
		return;
	}
	// Esc during a cutscene skips it (once it can be skipped) instead of opening the pause menu.
	if (UGBCutsceneSubsystem* Cutscene = UGBCutsceneSubsystem::Get(this))
	{
		if (Cutscene->RequestSkip())
		{
			return;
		}
	}
	SetMenu(OpenMenu == EGBMenu::None ? EGBMenu::Pause : EGBMenu::None);
}

void AGBPlayerController::InputMenuNavigate(const FInputActionValue& Value)
{
	if (OpenMenu == EGBMenu::None)
	{
		return;
	}
	// Value: x right, y up. One step per press along the dominant axis; the HUD wants y down.
	const FVector2D Axis = Value.Get<FVector2D>();
	if (Axis.IsNearlyZero())
	{
		return;
	}
	FIntPoint Step(0, 0);
	if (FMath::Abs(Axis.X) >= FMath::Abs(Axis.Y))
	{
		Step.X = Axis.X > 0.f ? 1 : -1;
	}
	else
	{
		Step.Y = Axis.Y > 0.f ? -1 : 1;
	}
	if (AGBHUD* HUD = Cast<AGBHUD>(GetHUD()))
	{
		HUD->MenuNavigate(Step);
	}
}

void AGBPlayerController::InputMenuConfirm()
{
	if (OpenMenu == EGBMenu::None)
	{
		return;
	}
	if (OpenMenu == EGBMenu::Death)
	{
		if (AGBGameMode* GameMode = AGBGameMode::Get(this))
		{
			GameMode->RespawnPlayer();
		}
		return;
	}
	if (AGBHUD* HUD = Cast<AGBHUD>(GetHUD()))
	{
		HUD->MenuConfirm();
	}
}

void AGBPlayerController::InputMenuBack()
{
	if (OpenMenu == EGBMenu::None || OpenMenu == EGBMenu::Death)
	{
		return;
	}
	AGBHUD* HUD = Cast<AGBHUD>(GetHUD());
	if (!HUD || !HUD->MenuBack())
	{
		SetMenu(EGBMenu::None);
	}
}

void AGBPlayerController::InputMenuTabKey()
{
	if (OpenMenu == EGBMenu::MainMenu)
	{
		// Tab on the title screen switches its look (like the on-screen button).
		if (AGBHUD* HUD = Cast<AGBHUD>(GetHUD()))
		{
			HUD->MenuReset();
		}
		return;
	}
	if (OpenMenu == EGBMenu::Inventory)
	{
		SetMenu(EGBMenu::None);
		return;
	}
	InputMenuBack();
}

void AGBPlayerController::InputMenuTab(const FInputActionValue& Value)
{
	if (OpenMenu == EGBMenu::None || OpenMenu == EGBMenu::Death)
	{
		return;
	}
	const float Axis = Value.Get<float>();
	if (FMath::Abs(Axis) < MenuAxisStepThreshold)
	{
		return;
	}
	if (AGBHUD* HUD = Cast<AGBHUD>(GetHUD()))
	{
		HUD->MenuTab(Axis > 0.f ? 1 : -1);
	}
}

void AGBPlayerController::InputMenuReset()
{
	if (OpenMenu == EGBMenu::None || OpenMenu == EGBMenu::Death)
	{
		return;
	}
	if (AGBHUD* HUD = Cast<AGBHUD>(GetHUD()))
	{
		HUD->MenuReset();
	}
}

void AGBPlayerController::InputMenuClick()
{
	if (OpenMenu == EGBMenu::None)
	{
		return;
	}
	if (OpenMenu == EGBMenu::Death)
	{
		// Like confirm: a click anywhere respawns.
		if (AGBGameMode* GameMode = AGBGameMode::Get(this))
		{
			GameMode->RespawnPlayer();
		}
		return;
	}
	float MouseX = 0.f;
	float MouseY = 0.f;
	if (!GetMousePosition(MouseX, MouseY))
	{
		return;
	}
	if (AGBHUD* HUD = Cast<AGBHUD>(GetHUD()))
	{
		HUD->MenuClick(FVector2D(MouseX, MouseY));
	}
}
