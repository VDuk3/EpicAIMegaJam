// Player controller: mapping contexts, menus and pause.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GBPlayerController.generated.h"

class UGBInputSet;
struct FInputActionValue;

/** Screens drawn by AGBHUD. Opening any of them pauses the game (single player). */
UENUM(BlueprintType)
enum class EGBMenu : uint8
{
	None,
	Pause,
	Inventory,
	SkillTrees,
	Death,
	/** The title screen at boot (new game with the staff choice, continue, options, quit), in two looks. */
	MainMenu
};

UCLASS()
class GLYPHBOUND_API AGBPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AGBPlayerController();

	/** Called by the character once its UGBInputSet is built: adds the gameplay context and binds the menu actions. */
	void SetupInput(UGBInputSet* InInputSet);

	EGBMenu GetOpenMenu() const { return OpenMenu; }
	bool IsMenuOpen() const { return OpenMenu != EGBMenu::None; }
	/** Opens a menu (pausing and swapping mapping contexts) or, with None, closes the current one. */
	void SetMenu(EGBMenu Menu);
	/** Opens Menu, or closes it if it is the one already open. */
	void ToggleMenu(EGBMenu Menu);

	/** Ticks while paused too (menus pause the game): finishes centering the cursor after a menu opens. */
	virtual void TickActor(float DeltaTime, ELevelTick TickType, FActorTickFunction& ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;

	/**
	 * Mouse in menus: while any menu is open the cursor is visible and free over the viewport (game and UI input,
	 * so keys and clicks still reach the menu actions); with no menu it is hidden and captured again (game only).
	 */
	void ApplyMouseMode(bool bMenuOpen);
	void CenterMouseCursor();

	void InputInventory();
	void InputSkillTrees();
	void InputPause();
	void InputMenuNavigate(const FInputActionValue& Value);
	void InputMenuConfirm();
	void InputMenuBack();
	void InputMenuTabKey();
	void InputMenuTab(const FInputActionValue& Value);
	void InputMenuReset();
	/** Left click in a menu: the HUD's entry under the cursor; on the death screen a click anywhere respawns. */
	void InputMenuClick();

	UPROPERTY() TObjectPtr<UGBInputSet> InputSet;
	EGBMenu OpenMenu = EGBMenu::None;
	/** The cursor is centered again on the next tick: Slate may move it back while releasing the mouse capture. */
	bool bCenterCursorPending = false;
	/** The title screen is opened on the first tick that has a HUD (once per run, see AGBHUD::ConsumeMainMenuOpening). */
	bool bMainMenuChecked = false;
};
