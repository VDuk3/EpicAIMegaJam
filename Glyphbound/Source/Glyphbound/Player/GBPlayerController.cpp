#include "Player/GBPlayerController.h"

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
		return Menu == EGBMenu::Pause || Menu == EGBMenu::Inventory || Menu == EGBMenu::SkillTrees;
	}
}

AGBPlayerController::AGBPlayerController()
{
	// Menus are driven by keyboard / gamepad only.
	bShowMouseCursor = false;
}

void AGBPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (IsLocalController())
	{
		SetInputMode(FInputModeGameOnly());
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
	EIC->BindAction(InputSet->MenuTab, ETriggerEvent::Started, this, &ThisClass::InputMenuTab);
}

void AGBPlayerController::SetMenu(EGBMenu Menu)
{
	if (Menu == OpenMenu)
	{
		return;
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
			Subsystem->AddMappingContext(InputSet->GameplayContext, GameplayContextPriority);
		}
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
	if (OpenMenu != EGBMenu::Death)
	{
		ToggleMenu(EGBMenu::Inventory);
	}
}

void AGBPlayerController::InputSkillTrees()
{
	if (OpenMenu != EGBMenu::Death)
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
