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
 *  - floating damage numbers over whatever the player hits (UGBCombatLibrary::OnDamageDealt);
 *  - the summoned shield: indicator near the vitals, and the shield sprite raised in first person while blocking;
 *  - menus: pause, inventory (use, equip armor, combine, owned weapons, loadout, enchant element), skill trees,
 *    death screen; keyboard, gamepad and mouse (hover moves the shared cursor, click = confirm).
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
	/** Keyboard / gamepad shortcut for the skill reset button (R / top face button). */
	void MenuReset();
	/** Refunds every owned skill node (see UGBSkillTreeComponent::ResetAll). The cursor stays on its node. */
	void ResetSkills(AGBPlayerCharacter* Player);
	/** Test option (owner, GDD 18 provisional): gives one free skill point. Not a refund: it adds to the pool. */
	void AddTestSkillPoint(AGBPlayerCharacter* Player);
	/**
	 * Mouse in menus: the cursor is visible while a menu is open, the entry under it is highlighted (the HUD
	 * reads the mouse position itself while drawing), and a left click on an entry does what confirm does there
	 * (tabs and buttons included). ScreenPosition is in viewport pixels. Returns true if something was clicked.
	 */
	bool MenuClick(const FVector2D& ScreenPosition);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

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

	// ---- Summoned shield in first person (GBHUD.cpp) ----
	/** 0 = lowered (off screen), 1 = fully raised. Slides towards IsBlockRaised() every frame. */
	float ShieldRaise01 = 0.f;
	double LastShieldUpdateTime = -1.0;

	// ---- Floating damage numbers (GBHUD_DamageNumbers.cpp) ----
	struct FDamageNumber
	{
		TWeakObjectPtr<AActor> Target;
		FVector WorldPosition = FVector::ZeroVector;
		/** Sideways screen drift, -1..1. */
		float Drift = 0.f;
		float Amount = 0.f;
		/** Target max health when the number was made (0 = unknown), for the big-hit rule after merging. */
		float TargetMaxHealth = 0.f;
		EGBElement Element = EGBElement::Physical;
		bool bBig = false;
		/** World time (slows down with the QTE time dilation). */
		double StartTime = 0.0;
	};
	TArray<FDamageNumber> DamageNumbers;
	FDelegateHandle DamageDealtHandle;
	void HandleDamageDealt(AActor* Target, const FGBDamageInfo& Info, float DamageDealt);
	void DrawDamageNumbers();
	double GetWorldTime() const;

	// ---- Mouse in menus (GBHUD_Menus.cpp) ----
	enum class EMenuHitKind : uint8
	{
		PauseEntry,
		Tab,
		ItemRow,
		WeaponRow,
		SpellSlot,
		SpellChoice,
		SkillNode,
		SkillReset,
		SkillAddPoint,
		DeathScreen
	};
	/** One clickable thing drawn this frame, in canvas coordinates. */
	struct FMenuHit
	{
		EMenuHitKind Kind = EMenuHitKind::PauseEntry;
		FVector2D Min = FVector2D::ZeroVector;
		FVector2D Max = FVector2D::ZeroVector;
		int32 Index = 0;
		FName Id = NAME_None;
	};
	/** Clickables of the last drawn menu frame, and which menu / page they belong to. */
	TArray<FMenuHit> MenuHits;
	EGBMenu MenuHitsMenu = EGBMenu::None;
	int32 MenuHitsPage = -1;
	bool bMenuHitsChoosing = false;
	/** Canvas pixels per viewport pixel (the canvas is divided by the DPI scale). Updated every frame. */
	float MouseToCanvas = 1.f;
	/** Last mouse position seen while a menu was open (canvas space); hover only acts when it moves. */
	FVector2D LastMenuMouse = FVector2D::ZeroVector;
	bool bHasMenuMouse = false;
	void AddMenuHit(EMenuHitKind Kind, float X, float Y, float W, float H, int32 Index = 0, FName Id = NAME_None);
	const FMenuHit* FindMenuHit(const FVector2D& CanvasPosition) const;
	/** Reads the mouse; if it moved over a clickable, moves the keyboard cursor there. Before drawing a menu. */
	void UpdateMenuMouse();
	void ApplyMenuHover(const FMenuHit& Hit);
	void ActivateMenuHit(const FMenuHit& Hit);
	void ResetMenuMouse();

	// ---- Menu state (GBHUD_Menus.cpp) ----
	static constexpr int32 PageItems = 0;
	static constexpr int32 PageWeapons = 1;
	static constexpr int32 PageSpells = 2;
	static constexpr int32 PageSkills = 3;
	static constexpr int32 PageCount = 4;
	struct FMenuState
	{
		/** Inventory page (PageItems, PageWeapons, PageSpells, PageSkills). */
		int32 Page = 0;
		/** Row in the current list (pause entries, items, weapons, spell slots). */
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
	/** Summoned shield: health bar, seconds left and the Block key, when the spellbook has a shield. */
	void DrawShieldIndicator(AGBPlayerCharacter* Player, const FGBHudSkinStyle& Style, float X, float Y);
	/** The raised ice shield sprite, large, from the lower left. Returns how raised it is (0..1). */
	float DrawRaisedShield(AGBPlayerCharacter* Player, float PixelSize, const FVector2D& Bob);
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
	void DrawWeaponsPage(AGBPlayerCharacter* Player, const FGBHudSkinStyle& Style, float X, float Y, float W, float H);
	void DrawSpellsPage(AGBPlayerCharacter* Player, const FGBHudSkinStyle& Style, float X, float Y, float W, float H);
	void DrawSkillTrees(AGBPlayerCharacter* Player, float Top);
	/** Owned items in Items.json order. */
	TArray<FName> GetInventoryList(AGBPlayerCharacter* Player) const;
	/** Owned weapons grouped by kind (staffs, guns, sword), each group in the order obtained. */
	TArray<FName> GetWeaponList(AGBPlayerCharacter* Player) const;
	void SetInventoryPage(int32 NewPage);
	/** Known spells that can go in a loadout slot, with NAME_None first (= empty the slot). */
	TArray<FName> GetAssignableSpells(AGBPlayerCharacter* Player) const;
	TArray<FSkillNodeRef> GetSkillNodes() const;
	void NavigateSkills(FIntPoint Direction);
	void ConfirmItem(AGBPlayerCharacter* Player);
	void ConfirmWeapon(AGBPlayerCharacter* Player);
	void ConfirmSpellSlot(AGBPlayerCharacter* Player);
	void ConfirmSkill(AGBPlayerCharacter* Player);
};
