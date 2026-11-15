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
class UGBPixelFont;
struct FGBPixelInk;

/** Text sizes shared by both skins (each skin maps them to its own font and size). */
enum class EGBTextSize : uint8
{
	Title,
	Heading,
	Body,
	Small,
	/** Big numerals: ammo, health. */
	Number
};

/** Text colors shared by both skins (each skin maps them to its palette). */
enum class EGBTextTone : uint8
{
	Normal,
	/** Headings and highlights: gold (RPG), red (cyberpunk). */
	Accent,
	/** Secondary info: muted bone (RPG), dim red (cyberpunk). */
	Muted,
	/** Values and numbers: bright bone (RPG), cyan (cyberpunk). */
	Value,
	Good,
	Bad,
	/** Attention: ember (RPG), yellow (cyberpunk). */
	Warn,
	/** Text on a selected row. */
	Selected,
	Disabled
};

/**
 * Draws, every frame, from the player's components (it owns no gameplay state):
 *  - the status HUD in two skins (GDD 13): a 1990s first-person RPG status bar of carved stone, iron and gold in
 *    pixel art with the game's own pixel font (staff and sword), and a modern cyberpunk HUD of thin red / cyan
 *    lines, cut corners and condensed type (gun);
 *  - the first-person pixel-art hands and staff (sprites from UGBPixelSpriteLibrary), art rule: hands are
 *    always pixel art, even when holding a 3D gun;
 *  - crosshair, interact prompt, toast messages, damage flash, in the held item's skin;
 *  - the QTE overlay (key sequence with right / wrong marks, gesture to draw with its trail, time bar);
 *  - the era-travel fade (UGBEraSubsystem::GetFadeAlpha);
 *  - floating damage numbers over whatever the player hits (UGBCombatLibrary::OnDamageDealt);
 *  - the summoned shield: indicator near the vitals, and the shield sprite raised in first person while blocking;
 *  - menus in the held item's skin: pause, inventory (use, equip armor, combine, owned weapons, loadout, enchant
 *    element), skill trees (spell tree always RPG, weapon tree always cyberpunk), death screen; keyboard, gamepad
 *    and mouse (hover moves the shared cursor, click = confirm).
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
	/** Skin of the menus both eras share: the current era's look (past = RPG, future = cyberpunk). */
	EGBHudSkin GetMenuSkin() const;

	/** Full-screen hand overlay for the CENTRAL art (gb.HandOverlay): a 480x270 PNG drawn with the same motion as the hands. */
	void SetHandOverlay(UTexture2D* Texture);
	void ClearHandOverlay();
	bool HasHandOverlay() const { return HandOverlayTexture != nullptr; }

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

	/** Smoke run only: moves the menu cursor (page, row, skill node) so screenshots show every screen. */
	void DebugSetMenuCursor(int32 Page, int32 Cursor, FName SkillNode = NAME_None, bool bChoosingSpell = false);

	// ---- Title screen (GBHUD_MainMenu.cpp) ----
	/** True once per run (not after a level travel or a map restart), unless -NoMenu is on the command line. Marks it done. */
	static bool ConsumeMainMenuOpening(const UObject* WorldContextObject);
	/** The staff picked on the title screen for a fresh start (None = Player.json's starting weapons as they are). */
	static FName GetChosenStartStaff(const UObject* WorldContextObject);
	/** Applies the saved options (volume, mouse sensitivity) once at startup. */
	static void ApplySavedOptions(const UObject* WorldContextObject);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	AGBPlayerCharacter* GetPlayer() const;

	struct FToast
	{
		FString Text;
		double StartTime = 0.0;
		double EndTime = 0.0;
	};
	TArray<FToast> Toasts;

	/** Real time of the last damage taken, for the red flash. */
	double LastDamageTime = -1000.0;
	/** Real time the held item's skin last changed (the new HUD plays a short intro), and the skin then. */
	double SkinChangeTime = -1000.0;
	EGBHudSkin LastSkin = EGBHudSkin::Rpg90s;
	bool bHasLastSkin = false;
	/** Real time the open menu was opened (intro animation). */
	double MenuOpenTime = -1000.0;
	/** The look the open menu was last drawn in; a change while it stays open plays a transition (owner, Nov 13). */
	EGBHudSkin MenuSkinShown = EGBHudSkin::Rpg90s;
	bool bHasMenuSkin = false;
	double MenuSkinChangeTime = -1000.0;

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

	/** Full-screen hand overlay (gb.HandOverlay); null = draw the normal hands. Nearest filtering, set on load. */
	UPROPERTY() TObjectPtr<UTexture2D> HandOverlayTexture = nullptr;

	// ---- Smoothed bars (the "lost health" trail behind the health bar) ----
	float HealthTrail01 = 1.f;
	float HealthShown01 = 1.f;
	double LastTrailUpdateTime = -1.0;
	void UpdateTrails(AGBPlayerCharacter* Player);

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
	void DrawDamageNumbers(EGBHudSkin Skin);
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
		/** Skill trees: the tab of one tree (Index 0 = spells, 1 = weapons). */
		SkillTreeTab,
		/** Weapon tree: the Back button of a gun branch page. */
		SkillBranchBack,
		/** The skill point confirmation: spend, or cancel. */
		ConfirmYes,
		ConfirmNo,
		DeathScreen,
		/** Title screen: a root entry (Index), the look switch, a staff card (Index), an options row (Index; Id L / R = arrows). */
		MainEntry,
		MainStyle,
		StaffCard,
		OptionRow
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
	/** True if the mouse is over this rectangle (for the hover look of buttons and tabs). */
	bool IsMouseOver(float X, float Y, float W, float H) const;
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
		/** Skill trees: the node last selected in each tree, so switching back returns to it. */
		FName TreeCursor[2] = { NAME_None, NAME_None };
		/** A perk waiting for the player to confirm the skill point (owner, Nov 13). None = no dialog. */
		FName PendingUnlock = NAME_None;
	};
	FMenuState MenuState;

	struct FSkillNodeRef
	{
		FName Id = NAME_None;
		/** 0 = spell tree (left), 1 = weapon tree (right). */
		int32 Tree = 0;
		int32 Column = 0;
		int32 Row = 0;
		/** Weapon tree: the gun branch page the node is on (None = the hub page). */
		FName Branch = NAME_None;
		/** A gun emblem on the weapon hub (not a perk): confirming it opens its Branch page. Its Id is "Branch_<Branch>". */
		bool bCategory = false;
		FName OpensBranch = NAME_None;
	};

	// ======================================================================== painting (GBHUD_Paint.cpp)
	double GetRealTime() const;
	/** Canvas height / 1080: every cyberpunk layout size is multiplied by it. */
	float UIScale() const;
	/** Size in screen pixels of one RPG art pixel (4 at 1080p, 3 at 720p): the RPG skin is laid out in these. */
	float ArtPixel() const;

	/** While true (the cyberpunk HUD over the bright world), every cyberpunk text gets a soft dark shadow. */
	bool bForceTextShadow = false;
	UPROPERTY(Transient) TObjectPtr<UGBPixelFont> PixelFont;
	/** Engine Roboto (runtime font cache): the canvas only draws Slate font text when the item also has a runtime UFont. */
	UPROPERTY() TObjectPtr<UFont> RuntimeFont;
	/** Tiled dark stone blocks of the RPG panels (wrap, nearest). */
	UPROPERTY(Transient) TObjectPtr<UTexture2D> RpgStoneTexture;
	/** Soft white radial blob (bilinear): glows, vignettes, light pools. */
	UPROPERTY(Transient) TObjectPtr<UTexture2D> GlowTexture;
	/** White left-to-right alpha ramp (bilinear): fades on bars and rows. */
	UPROPERTY(Transient) TObjectPtr<UTexture2D> RampTexture;
	void EnsureArt();

	// -- generic primitives
	void FillRect(float X, float Y, float W, float H, const FLinearColor& Color);
	void DrawOutline(float X, float Y, float W, float H, const FLinearColor& Color, float Thickness);
	/** Convex polygon (fan from the first point), solid color. */
	void FillPoly(const TArray<FVector2D>& Points, const FLinearColor& Color);
	/** Closed polyline. */
	void StrokePoly(const TArray<FVector2D>& Points, const FLinearColor& Color, float Thickness);
	void DrawGlow(float CX, float CY, float RX, float RY, const FLinearColor& Color);
	/** Horizontal alpha ramp: Color at the left fading to transparent at the right (or the reverse). */
	void DrawFade(float X, float Y, float W, float H, const FLinearColor& Color, bool bFadeRight = true);
	void DrawSprite(UTexture2D* Texture, float X, float Y, float PixelScale, const FLinearColor& Tint = FLinearColor::White);
	/** A UV sub-rect of a texture (for the climb sleeves: the cuff rows tiled downward). Same pixel snapping as DrawSprite. */
	void DrawSpriteTile(UTexture2D* Texture, float X, float Y, float W, float H, float U, float V, float UL, float VL, const FLinearColor& Tint = FLinearColor::White);
	/** A sprite id from Sprites.json fitted inside a box, whole pixels, centered. Returns false if the sprite does not exist. */
	bool DrawSpriteFit(FName SpriteId, float X, float Y, float W, float H, const FLinearColor& Tint = FLinearColor::White, bool bSilhouette = false);

	// -- old engine-font text (debug overlays only)
	void DrawLabel(const FString& Text, const FLinearColor& Color, float X, float Y, float SizePx, float AlignX = 0.f, bool bShadow = true, bool bLarge = false);
	FVector2D MeasureLabel(const FString& Text, float SizePx, bool bLarge = false) const;

	// -- pixel font (RPG)
	struct FPixelTextStyle
	{
		int32 Scale = 2;
		FLinearColor Color = FLinearColor::White;
		/** If set, the glyphs carry this baked ramp (Color only sets the alpha). */
		const FGBPixelInk* Ink = nullptr;
		FLinearColor Outline = FLinearColor::Transparent;
		FLinearColor Shadow = FLinearColor::Transparent;
	};
	/** Draws one line; Y is the top of the capitals. Returns the drawn size (capital height, no descenders). */
	FVector2D DrawPixelText(const FString& Text, float X, float Y, const FPixelTextStyle& Style, float AlignX = 0.f);
	FVector2D MeasurePixelText(const FString& Text, int32 Scale) const;

	// -- condensed vector type (cyberpunk), engine Roboto / Droid faces loaded from the TTF files
	enum class ECyFace : uint8 { Condensed, Regular, Medium, Light, Black, Mono };
	struct FCyTextStyle
	{
		ECyFace Face = ECyFace::Regular;
		/** Line height in pixels at 1080p (scaled with UIScale). */
		float Px = 20.f;
		FLinearColor Color = FLinearColor::White;
		/** Extra letter spacing, thousandths of an em. */
		int32 Spacing = 0;
		bool bUpper = false;
		FLinearColor Shadow = FLinearColor::Transparent;
		/** Chromatic split (glitch): draws a cyan and a red copy this many pixels apart. */
		float Split = 0.f;
	};
	/** Draws one line; Y is the top of the line box. Returns the drawn size. */
	FVector2D DrawCyText(const FString& Text, float X, float Y, const FCyTextStyle& Style, float AlignX = 0.f);
	FVector2D MeasureCyText(const FString& Text, const FCyTextStyle& Style) const;

	// -- skin-neutral text roles (menus share their layout code between the skins)
	FVector2D UiText(EGBHudSkin Skin, EGBTextSize Size, EGBTextTone Tone, const FString& Text, float X, float Y, float AlignX = 0.f, const FLinearColor* OverrideColor = nullptr);
	FVector2D UiMeasure(EGBHudSkin Skin, EGBTextSize Size, const FString& Text) const;
	/** Height of one line of this size, including the gap to the next line. */
	float UiLineHeight(EGBHudSkin Skin, EGBTextSize Size) const;
	/** Word-wrapped text inside MaxWidth pixels. Returns the height used. */
	float UiWrapped(EGBHudSkin Skin, EGBTextSize Size, EGBTextTone Tone, const FString& Text, float X, float Y, float MaxWidth, const FLinearColor* OverrideColor = nullptr);
	FLinearColor ToneColor(EGBHudSkin Skin, EGBTextTone Tone) const;

	// -- RPG pieces (GBHUD_Rpg.cpp)
	enum class ERpgFrame : uint8 { Iron, Gold, Inset, Bone };
	/** Tiled stone, aligned to the screen so neighbouring panels line up. */
	void RpgStone(float X, float Y, float W, float H, const FLinearColor& Tint = FLinearColor::White);
	/** A beveled frame band, Thick art pixels wide, drawn inside the rectangle. */
	void RpgFrame(float X, float Y, float W, float H, ERpgFrame Kind);
	void RpgRivet(float X, float Y);
	void RpgGem(float CX, float CY, const FLinearColor& Color, bool bLit = true);
	/** Iron-framed stone window with gold trim and corner bosses. */
	void RpgWindow(float X, float Y, float W, float H);
	/** Recessed dark well (lists, details). */
	void RpgWell(float X, float Y, float W, float H);
	/** A carved plaque with a title in gold, centered at CX. Returns its rectangle. */
	FBox2D RpgPlaque(const FString& Title, float CX, float Y, int32 Scale);
	/** Gold rule with a gem in the middle. */
	void RpgDivider(float X, float Y, float W);
	/** Glass tube with a liquid level (HP, mana): Ramp = shadow, base, light, shine. */
	void RpgTube(float X, float Y, float W, float H, float Fraction, const FLinearColor Ramp[4], float Trail = -1.f);
	void RpgKeyCap(float X, float Y, const FString& Key, float* OutWidth = nullptr);
	/** Pointing cursor (a gold arrow) at the left of a row. */
	void RpgCursor(float X, float CY);

	// -- cyberpunk pieces (GBHUD_Cyber.cpp)
	/** Box with cut corners. Cut is in pixels; Corners bit mask 1 = top-left, 2 = top-right, 4 = bottom-right, 8 = bottom-left. */
	TArray<FVector2D> CyChamfer(float X, float Y, float W, float H, float Cut, uint8 Corners = 0x5) const;
	void CyBox(float X, float Y, float W, float H, float Cut, const FLinearColor& Fill, const FLinearColor& Line, uint8 Corners = 0x5, float Thick = 1.f);
	/** Four L-shaped corner brackets. */
	void CyBrackets(float X, float Y, float W, float H, float Len, const FLinearColor& Color, float Thick = 1.f);
	/** Slanted, segmented bar (health, mana). Slant = horizontal shift of the top edge in pixels. */
	void CySlantBar(float X, float Y, float W, float H, float Slant, float Fraction, int32 Segments, const FLinearColor& Fill, const FLinearColor& Empty, float Trail = -1.f);
	/** A key in a small square (or wider for words): [G]. Returns the width. */
	float CyKey(float X, float Y, const FString& Key, const FLinearColor& Color, float Height = 22.f);
	/** Faint scanlines over a rectangle. */
	void CyScanlines(float X, float Y, float W, float H, float Alpha);
	/** Short tick marks along a horizontal line (rulers under headers). */
	void CyRuler(float X, float Y, float W, const FLinearColor& Color);

	// ======================================================================== HUD layers
	void DrawHands(AGBPlayerCharacter* Player);
	/** The two climbing hands gripping the ledge while mantling, with sleeve tiles down to the screen bottom. */
	void DrawMantleHands(AGBPlayerCharacter* Player, float PixelSize);
	/** The raised ice shield sprite, large, from the lower left. Returns how raised it is (0..1). */
	float DrawRaisedShield(AGBPlayerCharacter* Player, float PixelSize, const FVector2D& Bob);
	/** Draws the hand overlay (nothing when none is set): same bob, kick, reload follow and pump shift as the hands. */
	void DrawHandOverlay(AGBPlayerCharacter* Player, const FVector2D& BobArt, float Kick01);

	// RPG (GBHUD_Rpg.cpp)
	void DrawRpgHud(AGBPlayerCharacter* Player);
	void DrawRpgVitals(AGBPlayerCharacter* Player, float X, float Y, float W);
	void DrawRpgSpellSockets(AGBPlayerCharacter* Player, float CX, float Y);
	void DrawRpgEffects(AGBPlayerCharacter* Player, float X, float Y);
	void DrawRpgShield(AGBPlayerCharacter* Player, float X, float Y);

	// Cyberpunk (GBHUD_Cyber.cpp)
	void DrawCyberHud(AGBPlayerCharacter* Player);
	void DrawCyberVitals(AGBPlayerCharacter* Player, float X, float Y);
	void DrawCyberWeapon(AGBPlayerCharacter* Player, AGBHeldItem* Item, float Right, float Bottom);
	float DrawCyberEffects(AGBPlayerCharacter* Player, float X, float Y);
	void DrawCyberShield(AGBPlayerCharacter* Player, float X, float Y);

	// Shared overlays (GBHUD.cpp)
	void DrawCrosshair(AGBPlayerCharacter* Player, EGBHudSkin Skin);
	void DrawInteractPrompt(AGBPlayerCharacter* Player, EGBHudSkin Skin);
	/** Y = top of the first toast (negative: the default spot near the top). */
	void DrawToasts(EGBHudSkin Skin, float TopY = -1.f);
	void DrawQTE(AGBPlayerCharacter* Player, EGBHudSkin Skin);
	/** The weapon enchant's first QTE step: the three elements on their fixed keys, the current one marked. */
	void DrawQTEElementChoice(AGBPlayerCharacter* Player, const struct FGBQTESession& Session, EGBHudSkin Skin);
	void DrawGestureHint(uint8 Gesture, float CenterX, float CenterY, float Radius, const FLinearColor& Color, float Thick);
	void DrawDamageFlash(EGBHudSkin Skin);
	void DrawEraFade();
	/** The caption cutscene (UGBCutsceneSubsystem): a dimmed screen with the current line in a panel of the skin's style. */
	void DrawCutscene(EGBHudSkin Skin);

	// ======================================================================== menus (GBHUD_Menus.cpp)
	AGBPlayerController* GetGBController() const;
	EGBMenu GetOpenMenu() const;
	void DrawMenus(AGBPlayerCharacter* Player);
	/** Darkens the game behind a menu, in the skin's way. */
	void MenuBackdrop(EGBHudSkin Skin);
	/** A row in a list: selected, marked (pending combine) or plain. Returns the inner left padding. */
	float MenuRow(EGBHudSkin Skin, float X, float Y, float W, float H, bool bSelected, bool bMarked = false);
	void MenuButton(EGBHudSkin Skin, float X, float Y, float W, float H, const FString& Label, bool bEnabled, bool bHot);
	/** Key + action hints along a footer line, centered at CX (or left-aligned when AlignX is 0). */
	void MenuHints(EGBHudSkin Skin, const TArray<TPair<FString, FString>>& Hints, float X, float Y, float AlignX);
	/** A titled detail panel (right side of the pages). Returns the content top. */
	float MenuDetailPanel(EGBHudSkin Skin, float X, float Y, float W, float H, const FString& Title, const FString& Subtitle, const FLinearColor* SubtitleColor);
	void DrawPauseMenu(EGBHudSkin Skin);
	void DrawDeathScreen(EGBHudSkin Skin);
	void DrawInventory(AGBPlayerCharacter* Player, EGBHudSkin Skin);
	void DrawItemsPage(AGBPlayerCharacter* Player, EGBHudSkin Skin, float X, float Y, float W, float H);
	void DrawWeaponsPage(AGBPlayerCharacter* Player, EGBHudSkin Skin, float X, float Y, float W, float H);
	void DrawSpellsPage(AGBPlayerCharacter* Player, EGBHudSkin Skin, float X, float Y, float W, float H);
	/** Both trees side by side inside the rectangle (spell tree RPG, weapon tree cyberpunk) and the node details. */
	void DrawSkillTrees(AGBPlayerCharacter* Player, EGBHudSkin Skin, float X, float Y, float W, float H);
	void DrawSkillNode(AGBPlayerCharacter* Player, int32 Tree, FName NodeId, float X, float Y, float W, float H);
	/** Owned items in Items.json order. */
	TArray<FName> GetInventoryList(AGBPlayerCharacter* Player) const;
	/** Owned weapons grouped by kind (staffs, guns, sword), each group in the order obtained. */
	TArray<FName> GetWeaponList(AGBPlayerCharacter* Player) const;
	void SetInventoryPage(int32 NewPage);
	/** Known spells that can go in a loadout slot, with NAME_None first (= empty the slot). */
	TArray<FName> GetAssignableSpells(AGBPlayerCharacter* Player) const;
	TArray<FSkillNodeRef> GetSkillNodes() const;
	void NavigateSkills(FIntPoint Direction);
	/** Shows one skill tree (0 = spells, 1 = weapons): the cursor goes back to that tree's last node, or its first. */
	void SelectSkillTree(int32 Tree);
	/** The tree shown on the skill pages: the selected node's tree. */
	int32 GetShownSkillTree() const;
	/** Weapon tree: opens a gun branch page (the cursor goes to its first perk). */
	void OpenSkillBranch(FName Branch);
	/** Weapon tree: from a branch page back to the hub, on that branch's emblem. False if no branch page is shown. */
	bool CloseSkillBranch();
	/** The dialog that asks before a skill point is spent, in the screen's look, on top of everything. */
	void DrawUnlockConfirm(AGBPlayerCharacter* Player, EGBHudSkin Skin);
	/** Covers a change of the menu's look and reveals the new one (a scan sweep into cyberpunk, a pixel dissolve into RPG). */
	void DrawMenuSkinTransition();
	/** Weapon hub: one gun emblem (title, ranks owned, its perks). */
	void DrawSkillCategory(AGBPlayerCharacter* Player, const FSkillNodeRef& Category, const TArray<FSkillNodeRef>& Nodes, float X, float Y, float W, float H);
	void ConfirmItem(AGBPlayerCharacter* Player);
	void ConfirmWeapon(AGBPlayerCharacter* Player);
	void ConfirmSpellSlot(AGBPlayerCharacter* Player);
	void ConfirmSkill(AGBPlayerCharacter* Player);

	// ======================================================================== title screen (GBHUD_MainMenu.cpp)
	static constexpr int32 MainPageRoot = 0;
	static constexpr int32 MainPageStaff = 1;
	static constexpr int32 MainPageOptions = 2;
	/** The title screen's own look, switched by the player (owner, Nov 15): pixel-art RPG by default, or cyberpunk. */
	EGBHudSkin MainMenuSkin = EGBHudSkin::Rpg90s;
	void DrawMainMenu(EGBHudSkin Skin);
	void DrawMainRoot(EGBHudSkin Skin);
	void DrawMainStaffSelect(EGBHudSkin Skin);
	void DrawMainOptions(EGBHudSkin Skin);
	/** Opaque title backdrop, the game's name and the page title, in the skin. Returns the top of the content area. */
	float DrawMainTitle(EGBHudSkin Skin, const FString& PageTitle);
	/** The look switch button (bottom right). */
	void DrawMainStyleButton(EGBHudSkin Skin);
	void MainMenuNavigate(FIntPoint Direction);
	void MainMenuConfirm();
	bool MainMenuBack();
	void ToggleMainMenuSkin();
	/** True when a run is going on (the title was opened from the pause menu): Continue goes back to it. */
	bool IsRunInProgress() const;
	/** Starts the game with this staff: in place at boot, or a fresh restart of the start map when a run was going on. */
	void StartNewGame(FName StaffId);
	TArray<FName> GetStaffChoices() const;
	int32 NumOptionRows() const;
	FString OptionLabel(int32 Row) const;
	FString OptionValue(int32 Row) const;
	void AdjustOption(int32 Row, int32 Direction);
};
