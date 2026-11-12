// Everything on screen, drawn with the HUD canvas (no widget assets): HUD in two skins, hands, QTE, fade, menus.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Core/GBTypes.h"
#include "Player/GBPlayerController.h"
#include "Spells/GBSpellTypes.h"
#include "GBHUD.generated.h"

class AGBPlayerCharacter;
class AGBHeldItem;
class UFont;
class UTexture2D;

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

	// ---- Player events (bound lazily: the pawn may not exist yet, or may be replaced) ----
	void BindToPlayer(AGBPlayerCharacter* Player);
	void HandleDamaged(const FGBDamageInfo& Info, float ActualDamage);
	void HandleCastFailed(FName SpellId, EGBCastFailReason Reason);
	void HandleSpellCast(const FGBSpellCast& SpellCast);

	TWeakObjectPtr<AGBPlayerCharacter> BoundPlayer;
	FDelegateHandle DamagedHandle;
	FDelegateHandle CastFailedHandle;
	FDelegateHandle SpellCastHandle;
	/** Real time of the last successful cast, to raise the casting hand. */
	double LastCastTime = -1000.0;

	// ---- Menu state (GBHUD_Menus.cpp) ----
	static constexpr int32 PageItems = 0;
	static constexpr int32 PageSpells = 1;
	static constexpr int32 PageSkills = 2;
	static constexpr int32 PageCount = 3;
	struct FMenuState
	{
		/** Inventory page (PageItems, PageSpells, PageSkills). */
		int32 Page = 0;
		/** Row in the current list (pause entries, items, spell slots). */
		int32 Cursor = 0;
		/** Spells page: picking the spell for slot Cursor. */
		bool bChoosingSpell = false;
		int32 SubCursor = 0;
		/** Items page: item marked to be combined with the next confirmed one. */
		FName PendingCombine = NAME_None;
		/** Skill trees: the selected node (kept by id, data can be reloaded). */
		FName SkillCursor = NAME_None;
	};
	FMenuState MenuState;

	struct FSkillNodeRef
	{
		FName Id = NAME_None;
		/** 0 = spell tree (left), 1 = weapon tree (right). */
		int32 Tree = 0;
		int32 Column = 0;
		int32 Row = 0;
	};

	// ---- Drawing helpers (GBHUD.cpp) ----
	double GetRealTime() const;
	/** Canvas height / 1080: every layout size is multiplied by it. */
	float UIScale() const;
	static const FGBHudSkinStyle& GetSkinStyle(EGBHudSkin Skin);
	/** Text with its top-left (AlignX 0), top-center (0.5) or top-right (1) at X, Y. SizePx = line height at 1080p. */
	void DrawLabel(const FString& Text, const FLinearColor& Color, float X, float Y, float SizePx, float AlignX = 0.f, bool bShadow = true, bool bLarge = false);
	FVector2D MeasureLabel(const FString& Text, float SizePx, bool bLarge = false) const;
	/** Word-wrapped text inside MaxWidth pixels. Returns the height used. */
	float DrawWrapped(const FString& Text, const FLinearColor& Color, float X, float Y, float MaxWidth, float SizePx, bool bShadow);
	void DrawOutline(float X, float Y, float W, float H, const FLinearColor& Color, float Thickness);
	void DrawPanel(const FGBHudSkinStyle& Style, float X, float Y, float W, float H);
	void DrawBar(const FGBHudSkinStyle& Style, float X, float Y, float W, float H, float Fraction, const FLinearColor& Fill, const FString& Label);
	void DrawSprite(UTexture2D* Texture, float X, float Y, float PixelScale, const FLinearColor& Tint = FLinearColor::White);

	// ---- HUD layers (GBHUD.cpp) ----
	void DrawHands(AGBPlayerCharacter* Player);
	void DrawStatusHud(AGBPlayerCharacter* Player, const FGBHudSkinStyle& Style);
	void DrawVitals(AGBPlayerCharacter* Player, const FGBHudSkinStyle& Style, float X, float Y, float W);
	void DrawSpellSlots(AGBPlayerCharacter* Player, const FGBHudSkinStyle& Style, float CenterX, float Y);
	void DrawGunPanel(AGBPlayerCharacter* Player, AGBHeldItem* Gun, const FGBHudSkinStyle& Style, float X, float Y, float W);
	void DrawStatusEffects(AGBPlayerCharacter* Player, const FGBHudSkinStyle& Style, float X, float Y, bool bVertical);
	void DrawCrosshair(AGBPlayerCharacter* Player, const FGBHudSkinStyle& Style);
	void DrawInteractPrompt(AGBPlayerCharacter* Player);
	void DrawToasts();
	void DrawQTE(AGBPlayerCharacter* Player, const FGBHudSkinStyle& Style);
	void DrawGestureHint(uint8 Gesture, float CenterX, float CenterY, float Radius, const FLinearColor& Color);
	void DrawDamageFlash();
	void DrawEraFade();

	// ---- Menus (GBHUD_Menus.cpp) ----
	AGBPlayerController* GetGBController() const;
	EGBMenu GetOpenMenu() const;
	void DrawMenus(AGBPlayerCharacter* Player);
	void DrawPauseMenu(const FGBHudSkinStyle& Style);
	void DrawDeathScreen();
	void DrawInventory(AGBPlayerCharacter* Player, const FGBHudSkinStyle& Style);
	void DrawItemsPage(AGBPlayerCharacter* Player, const FGBHudSkinStyle& Style, float X, float Y, float W, float H);
	void DrawSpellsPage(AGBPlayerCharacter* Player, const FGBHudSkinStyle& Style, float X, float Y, float W, float H);
	void DrawSkillTrees(AGBPlayerCharacter* Player, float Top);
	/** Owned items in Items.json order. */
	TArray<FName> GetInventoryList(AGBPlayerCharacter* Player) const;
	/** Known spells that can go in a loadout slot, with NAME_None first (= empty the slot). */
	TArray<FName> GetAssignableSpells(AGBPlayerCharacter* Player) const;
	TArray<FSkillNodeRef> GetSkillNodes() const;
	void NavigateSkills(FIntPoint Direction);
	void ConfirmItem(AGBPlayerCharacter* Player);
	void ConfirmSpellSlot(AGBPlayerCharacter* Player);
	void ConfirmSkill(AGBPlayerCharacter* Player);
};
