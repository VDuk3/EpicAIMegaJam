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
	Death
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

protected:
	virtual void BeginPlay() override;

	void InputInventory();
	void InputSkillTrees();
	void InputPause();
	void InputMenuNavigate(const FInputActionValue& Value);
	void InputMenuConfirm();
	void InputMenuBack();
	void InputMenuTab(const FInputActionValue& Value);

	UPROPERTY() TObjectPtr<UGBInputSet> InputSet;
	EGBMenu OpenMenu = EGBMenu::None;
};
