// Everything on screen, drawn with the HUD canvas (no widget assets): HUD in two skins, hands, QTE, fade, menus.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Core/GBTypes.h"
#include "Player/GBPlayerController.h"
#include "GBHUD.generated.h"

class AGBPlayerCharacter;

/** Colors and layout switches of one HUD skin (GDD 13). Values live in GBHUD.cpp; fonts and art come later. */
struct FGBHudSkinStyle
{
	FLinearColor Panel = FLinearColor(0.f, 0.f, 0.f, 0.6f);
	FLinearColor Border = FLinearColor::White;
	FLinearColor Text = FLinearColor::White;
	FLinearColor Health = FLinearColor::Red;
	FLinearColor Mana = FLinearColor::Blue;
	FLinearColor Accent = FLinearColor::Yellow;
	/** Rpg90s: chunky bordered panels at the bottom. Cyberpunk: thin angled bars at the corners. */
	bool bChunky = true;
};

/**
 * Draws, every frame, from the player's components (it owns no gameplay state):
 *  - health / mana bars, ammo or spell slots with cooldowns, quick-use item, skill points, active effects;
 *  - the first-person pixel-art hands and staff (sprites from UGBPixelSpriteLibrary), art rule: hands are
 *    always pixel art, even when holding a 3D gun;
 *  - crosshair, interact prompt, toast messages, damage flash;
 *  - the QTE overlay (key sequence with right / wrong marks, gesture to draw with its trail, time bar);
 *  - the era-travel fade (UGBEraSubsystem::GetFadeAlpha);
 *  - menus: pause, inventory (use, equip armor, combine, loadout, enchant element), skill trees, death screen.
 * The skin follows the held item: staff and sword = Rpg90s, gun = Cyberpunk.
 */
UCLASS()
class GLYPHBOUND_API AGBHUD : public AHUD
{
	GENERATED_BODY()

public:
	AGBHUD();

	/** The local player's HUD, or null. */
	static AGBHUD* Get(const UObject* WorldContextObject);

	virtual void DrawHUD() override;

	/** A short message in the middle-top of the screen ("Checkpoint", "Picked up Pistol", "Not enough mana"). */
	void ShowMessage(const FString& Text, float Duration = 2.5f);

	EGBHudSkin GetCurrentSkin() const;

	// ---- Menu input, forwarded by AGBPlayerController while a menu is open ----
	void OnMenuOpened(EGBMenu Menu);
	void OnMenuClosed(EGBMenu Menu);
	/** Direction: x = -1 left / +1 right, y = -1 up / +1 down. */
	void MenuNavigate(FIntPoint Direction);
	void MenuConfirm();
	/** Returns true if the menu handled it (e.g. left a sub-page); false = the controller should close the menu. */
	bool MenuBack();
	/** Switches page inside a menu (inventory tabs). */
	void MenuTab(int32 Direction);

protected:
	virtual void BeginPlay() override;

	AGBPlayerCharacter* GetPlayer() const;

	struct FToast
	{
		FString Text;
		double EndTime = 0.0;
	};
	TArray<FToast> Toasts;

	/** Real time of the last damage taken, for the red flash. */
	double LastDamageTime = -1000.0;
};
