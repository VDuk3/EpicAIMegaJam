#include "UI/GBHUD.h"

#include "UI/GBHUDShared.h"
#include "UI/GBPixelFont.h"
#include "UI/GBPixelSpriteLibrary.h"
#include "UI/GBStaffView.h"
#include "Player/GBPlayerCharacter.h"
#include "Weapons/GBEquipmentComponent.h"
#include "Weapons/GBHeldItem.h"
#include "Weapons/GBGun.h"
#include "Spells/GBSpellbookComponent.h"
#include "Spells/GBSpellActor.h"
#include "QTE/GBQTEComponent.h"
#include "Items/GBInventoryComponent.h"
#include "Progression/GBSkillTreeComponent.h"
#include "Combat/GBStatusEffectComponent.h"
#include "Combat/GBCombatLibrary.h"
#include "Stats/GBStatsComponent.h"
#include "Stats/GBAttributeComponent.h"
#include "World/GBEraSubsystem.h"
#include "World/GBCutsceneSubsystem.h"
#include "World/GBInteractable.h"
#include "Data/GBDataSubsystem.h"
#include "Core/GBStatNames.h"
#include "Core/GBPlaceholder.h"

#include "Camera/PlayerCameraManager.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "HAL/IConsoleManager.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/IConsoleManager.h"

#include "AI/GBEnemyCharacter.h"
#include "Combat/GBElementalStatusComponent.h"
#include "World/GBEraComponent.h"

static TAutoConsoleVariable<int32> CVarShowElemental(
	TEXT("gb.ShowElemental"),
	0,
	TEXT("Debug view of elemental build-up bars over enemies. 1 = on."),
	ECVF_Default);

// Presentation-only timings and sizes (not gameplay numbers).
namespace GBHudLayout
{
	constexpr float DamageFlashTime = 0.3f;
	constexpr float CastHandTime = 0.45f;
	constexpr float AttackKickTime = 0.15f;
	constexpr float ToastFadeTime = 0.5f;
	constexpr float ToastInTime = 0.18f;
	constexpr int32 MaxToasts = 5;
	/** Canvas height / this = size of one sprite pixel for the hands (9 at 1080p, 6 at 720p). */
	constexpr float HandPixelDivisor = 120.f;
	/** The raised shield's sprite pixels are this many times the hands' pixels. */
	constexpr float ShieldPixelMult = 2.f;
	/** Seconds of the raise / lower slide of the shield (visual only; blocking follows the input at once). */
	constexpr float ShieldSlideTime = 0.12f;
	/** Both hands jump up this many sprite pixels with the seat bump. */
	constexpr float ReloadSeatPush = 2.f;
	/** The held staff leans this many degrees from vertical, its head to the left (owner's sketch, 2026-11-15). */
	constexpr float StaffTiltDeg = 15.f;
	/** The held staff is drawn this much larger than its sprite pixels (owner, 2026-11-15: 20 to 30% bigger). */
	constexpr float StaffSizeMult = 1.25f;
	/** Fraction of the staff (from its head) that shows above the bottom of the view: the grip point it pivots on. */
	constexpr float StaffGripFrac = 0.88f;
	/** Where the grip sits at the bottom of the view, as a fraction of the screen width (owner's sketch). */
	constexpr float StaffGripX = 0.79f;
	/** The pale "health lost" trail behind the health bar drains at this fraction per second. */
	constexpr float TrailDrainPerSecond = 0.45f;
	/** ...after holding still for this long. */
	constexpr float TrailHoldTime = 0.35f;
	/** Fraction of the staff sprite (from its top) where the middle of its head is: the shots leave from there. */
	constexpr float StaffHeadFrac = 0.1f;
}

namespace GBStaffView
{
	static FVector HeadAimDir = FVector::ForwardVector;
	static double HeadAimDirTime = -1.0;

	void SetHeadAimDir(const FVector& AimSpaceDir)
	{
		HeadAimDir = AimSpaceDir;
		HeadAimDirTime = FPlatformTime::Seconds();
	}

	bool GetHeadAimDir(FVector& OutAimSpaceDir)
	{
		if (HeadAimDirTime < 0.0 || FPlatformTime::Seconds() - HeadAimDirTime > 0.25)
		{
			return false;
		}
		OutAimSpaceDir = HeadAimDir;
		return true;
	}

	static double HoldSpellTime = -1.0;

	void NoteHoldSpell()
	{
		HoldSpellTime = FPlatformTime::Seconds();
	}

	bool ShouldShowHoldHint()
	{
		return HoldSpellTime >= 0.0 && FPlatformTime::Seconds() - HoldSpellTime < 1.5;
	}
}

namespace GBHudText
{
	FString ElementName(EGBElement Element)
	{
		switch (Element)
		{
		case EGBElement::Physical: return TEXT("Physical");
		case EGBElement::Arcane: return TEXT("Arcane");
		case EGBElement::Fire: return TEXT("Fire");
		case EGBElement::Lightning: return TEXT("Lightning");
		case EGBElement::Ice: return TEXT("Ice");
		case EGBElement::Air: return TEXT("Air");
		}
		return FString();
	}

	FLinearColor ElementColor(EGBElement Element)
	{
		return GBPlaceholder::ElementColor((uint8)Element);
	}

	FString Abbreviate(const FString& Name, int32 MaxLen)
	{
		if (Name.Len() <= MaxLen)
		{
			return Name;
		}
		TArray<FString> Words;
		Name.ParseIntoArrayWS(Words);
		if (Words.Num() > 1)
		{
			FString Result;
			for (int32 i = 0; i < Words.Num() - 1; ++i)
			{
				Result += Words[i].Left(1).ToUpper() + TEXT(".");
			}
			Result += Words.Last();
			return Result.Left(MaxLen);
		}
		return Name.Left(MaxLen - 1) + TEXT(".");
	}

	FString SpellName(const UObject* Context, FName SpellId)
	{
		const UGBDataSubsystem* Data = UGBDataSubsystem::Get(Context);
		const FGBSpellDefinition* Def = Data ? Data->FindSpell(SpellId) : nullptr;
		return (Def && !Def->DisplayName.IsEmpty()) ? Def->DisplayName : SpellId.ToString();
	}

	FName SpellIconId(FName SpellId)
	{
		return SpellId == NAME_None ? NAME_None : FName(*(FString(TEXT("icon_")) + SpellId.ToString().ToLower()));
	}
}

namespace GBHudLocal
{
	FString QTEKeyLetter(EGBQTEKey Key)
	{
		switch (Key)
		{
		case EGBQTEKey::Up: return TEXT("W");
		case EGBQTEKey::Left: return TEXT("A");
		case EGBQTEKey::Down: return TEXT("S");
		case EGBQTEKey::Right: return TEXT("D");
		}
		return TEXT("?");
	}

	FString GestureText(EGBGesture Gesture)
	{
		switch (Gesture)
		{
		case EGBGesture::LineRight: return TEXT("Draw a line to the right");
		case EGBGesture::LineLeft: return TEXT("Draw a line to the left");
		case EGBGesture::LineDown: return TEXT("Draw a line downward");
		case EGBGesture::LineUp: return TEXT("Draw a line upward");
		case EGBGesture::CircleClockwise: return TEXT("Draw a clockwise circle");
		case EGBGesture::CircleCounterClockwise: return TEXT("Draw a counter-clockwise circle");
		}
		return FString();
	}

	const FLinearColor White(1.f, 1.f, 1.f, 1.f);
}

// ======================================================================== basics

AGBHUD::AGBHUD()
{
	// A hard reference so the engine font is always cooked with the game.
	static ConstructorHelpers::FObjectFinder<UFont> RobotoFinder(TEXT("/Engine/EngineFonts/Roboto"));
	RuntimeFont = RobotoFinder.Object;
}

AGBHUD* AGBHUD::Get(const UObject* WorldContextObject)
{
	const UWorld* World = (GEngine && WorldContextObject) ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	return PC ? Cast<AGBHUD>(PC->GetHUD()) : nullptr;
}

void AGBHUD::BeginPlay()
{
	Super::BeginPlay();
	EnsureArt();
	BindToPlayer(GetPlayer());
	DamageDealtHandle = UGBCombatLibrary::OnDamageDealt().AddUObject(this, &AGBHUD::HandleDamageDealt);
}

void AGBHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// The damage delegate is global: it must never call into a dead HUD.
	UGBCombatLibrary::OnDamageDealt().Remove(DamageDealtHandle);
	DamageDealtHandle.Reset();
	BindToPlayer(nullptr);
	Super::EndPlay(EndPlayReason);
}

AGBPlayerCharacter* AGBHUD::GetPlayer() const
{
	return Cast<AGBPlayerCharacter>(GetOwningPawn());
}

EGBHudSkin AGBHUD::GetCurrentSkin() const
{
	const AGBPlayerCharacter* Player = GetPlayer();
	const UGBEquipmentComponent* Equipment = Player ? Player->GetEquipment() : nullptr;
	return Equipment ? Equipment->GetHudSkin() : EGBHudSkin::Rpg90s;
}

EGBHudSkin AGBHUD::GetMenuSkin() const
{
	// The skill pages (the Skill Trees screen and the inventory's Skills page) take the shown tree's look for the whole
	// screen (owner, Nov 13): spells = 90s RPG, weapons = cyberpunk, whatever the era.
	const EGBMenu Menu = GetOpenMenu();
	// The title screen has its own look, switched by the player (owner, Nov 15).
	if (Menu == EGBMenu::MainMenu)
	{
		return MainMenuSkin;
	}
	if (Menu == EGBMenu::SkillTrees || (Menu == EGBMenu::Inventory && MenuState.Page == PageSkills))
	{
		return GetShownSkillTree() == 1 ? EGBHudSkin::Cyberpunk : EGBHudSkin::Rpg90s;
	}
	// Menus both eras share (inventory, pause, death) take the look of the era the player is in
	// (owner, Nov 13): the past is the 90s RPG, the future is cyberpunk. The in-game HUD keeps following the held item.
	if (const UGBEraSubsystem* Era = UGBEraSubsystem::Get(this))
	{
		return Era->GetCurrentEra() == EGBEra::Future ? EGBHudSkin::Cyberpunk : EGBHudSkin::Rpg90s;
	}
	return GetCurrentSkin();
}

void AGBHUD::ShowMessage(const FString& Text, float Duration)
{
	if (Text.IsEmpty())
	{
		return;
	}
	const double Now = GetRealTime();
	const double EndTime = Now + FMath::Max(0.1f, Duration);
	for (FToast& Toast : Toasts)
	{
		if (Toast.Text == Text)
		{
			Toast.EndTime = EndTime;
			return;
		}
	}
	FToast Toast;
	Toast.Text = Text;
	Toast.StartTime = Now;
	Toast.EndTime = EndTime;
	Toasts.Add(Toast);
	while (Toasts.Num() > GBHudLayout::MaxToasts)
	{
		Toasts.RemoveAt(0);
	}
}

void AGBHUD::UpdateTrails(AGBPlayerCharacter* Player)
{
	const double Now = GetRealTime();
	const float Dt = LastTrailUpdateTime < 0.0 ? 0.f : FMath::Clamp((float)(Now - LastTrailUpdateTime), 0.f, 0.1f);
	LastTrailUpdateTime = Now;
	const UGBAttributeComponent* Attributes = Player ? Player->GetAttributes() : nullptr;
	if (!Attributes || Attributes->GetMaxHealth() <= 0.f)
	{
		return;
	}
	HealthShown01 = FMath::Clamp(Attributes->GetHealth() / Attributes->GetMaxHealth(), 0.f, 1.f);
	if (HealthShown01 >= HealthTrail01)
	{
		HealthTrail01 = HealthShown01;
	}
	else if (Now - LastDamageTime > GBHudLayout::TrailHoldTime)
	{
		HealthTrail01 = FMath::Max(HealthShown01, HealthTrail01 - GBHudLayout::TrailDrainPerSecond * Dt);
	}
}

// ======================================================================== player events

void AGBHUD::BindToPlayer(AGBPlayerCharacter* Player)
{
	if (BoundPlayer.Get() == Player)
	{
		return;
	}
	if (AGBPlayerCharacter* Old = BoundPlayer.Get())
	{
		if (UGBAttributeComponent* Attributes = Old->GetAttributes())
		{
			Attributes->OnDamaged.Remove(DamagedHandle);
		}
		if (UGBSpellbookComponent* Spellbook = Old->GetSpellbook())
		{
			Spellbook->OnCastFailed.Remove(CastFailedHandle);
			Spellbook->OnSpellCast.Remove(SpellCastHandle);
		}
	}
	DamagedHandle.Reset();
	CastFailedHandle.Reset();
	SpellCastHandle.Reset();
	BoundPlayer = Player;
	if (!Player)
	{
		return;
	}
	if (UGBAttributeComponent* Attributes = Player->GetAttributes())
	{
		DamagedHandle = Attributes->OnDamaged.AddUObject(this, &AGBHUD::HandleDamaged);
	}
	if (UGBSpellbookComponent* Spellbook = Player->GetSpellbook())
	{
		CastFailedHandle = Spellbook->OnCastFailed.AddUObject(this, &AGBHUD::HandleCastFailed);
		SpellCastHandle = Spellbook->OnSpellCast.AddUObject(this, &AGBHUD::HandleSpellCast);
	}
}

void AGBHUD::HandleDamaged(const FGBDamageInfo& Info, float ActualDamage)
{
	if (ActualDamage > 0.f)
	{
		LastDamageTime = GetRealTime();
	}
}

void AGBHUD::HandleSpellCast(const FGBSpellCast& SpellCast)
{
	LastCastTime = GetRealTime();
}

void AGBHUD::HandleCastFailed(FName SpellId, EGBCastFailReason Reason)
{
	FString Text;
	switch (Reason)
	{
	case EGBCastFailReason::NotEnoughMana: Text = TEXT("Not enough mana"); break;
	case EGBCastFailReason::OnCooldown: Text = TEXT("On cooldown"); break;
	case EGBCastFailReason::QTEFailed: Text = TEXT("Spell fizzled"); break;
	case EGBCastFailReason::EmptySlot: Text = TEXT("No spell in that slot"); break;
	case EGBCastFailReason::NotKnown: Text = TEXT("You have not learned that spell"); break;
	case EGBCastFailReason::WrongHeldItem:
	{
		const UGBDataSubsystem* Data = UGBDataSubsystem::Get(this);
		const FGBSpellDefinition* Def = Data ? Data->FindSpell(SpellId) : nullptr;
		Text = (Def && Def->bRequiresGun) ? TEXT("Need a gun in hand") : TEXT("Need the staff in hand");
		break;
	}
	default:
		break;
	}
	ShowMessage(Text, 1.2f);
}

// ======================================================================== frame

void AGBHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas || Canvas->ClipX <= 0.f || Canvas->ClipY <= 0.f)
	{
		return;
	}
	EnsureArt();

	// Drop expired toasts here, once per frame.
	const double Now = GetRealTime();
	Toasts.RemoveAll([Now](const FToast& Toast) { return Toast.EndTime <= Now; });

	AGBPlayerCharacter* Player = GetPlayer();
	BindToPlayer(Player);
	const EGBHudSkin Skin = GetCurrentSkin();
	if (!bHasLastSkin || Skin != LastSkin)
	{
		// The first frame does not play the intro (nothing was switched).
		SkinChangeTime = bHasLastSkin ? Now : -1000.0;
		LastSkin = Skin;
		bHasLastSkin = true;
	}
	UpdateTrails(Player);
	const EGBMenu OpenMenu = GetOpenMenu();
	const bool bMenuOpen = OpenMenu != EGBMenu::None;
	// The canvas is the viewport divided by the DPI scale; the mouse comes in viewport pixels.
	MouseToCanvas = Canvas->SizeX > 0 ? Canvas->ClipX / (float)Canvas->SizeX : 1.f;

	// World-space numbers first: hands and HUD panels stay on top of them.
	DrawDamageNumbers(Skin);
	if (CVarShowElemental.GetValueOnGameThread() == 1 && Player)
	{
		UWorld* HudWorld = GetWorld();
		APlayerCameraManager* CamMgr = PlayerOwner ? PlayerOwner->PlayerCameraManager.Get() : nullptr;
		if (HudWorld && CamMgr)
		{
			const FVector CamLoc = CamMgr->GetCameraLocation();
			const FVector CamFwd = CamMgr->GetCameraRotation().Vector();
			const FVector PlayerLoc = Player->GetActorLocation();
			for (TActorIterator<AGBEnemyCharacter> It(HudWorld); It; ++It)
			{
				AGBEnemyCharacter* Enemy = *It;
				if (!Enemy || Enemy->IsDead())
				{
					continue;
				}
				if (Enemy->GetEraComponent() && !Enemy->GetEraComponent()->IsPresent())
				{
					continue;
				}
				UGBElementalStatusComponent* Elemental = Enemy->GetElementalStatus();
				if (!Elemental)
				{
					continue;
				}
				const float Dist = FVector::Dist(PlayerLoc, Enemy->GetActorLocation());
				if (Dist > 4000.f)
				{
					continue;
				}
				FVector Head = Enemy->GetActorLocation();
				if (const UCapsuleComponent* Capsule = Enemy->FindComponentByClass<UCapsuleComponent>())
				{
					Head.Z += Capsule->GetScaledCapsuleHalfHeight() + 30.f;
				}
				else
				{
					Head.Z += 120.f;
				}
				if (FVector::DotProduct(Head - CamLoc, CamFwd) <= 0.f)
				{
					continue;
				}
				const FVector Screen = Canvas->Project(Head, true);
				if (Screen.Z <= 0.f)
				{
					continue;
				}
				TArray<FString> Lines;
				const EGBElement Elements[3] = { EGBElement::Fire, EGBElement::Lightning, EGBElement::Ice };
				const TCHAR* Names[3] = { TEXT("Fire"), TEXT("Lightning"), TEXT("Ice") };
				for (int32 i = 0; i < 3; ++i)
				{
					const EGBBuildUpPhase Phase = Elemental->GetPhase(Elements[i]);
					if (Phase == EGBBuildUpPhase::Building)
					{
						const float Value = Elemental->GetBuildUp(Elements[i]);
						if (Value <= 0.f)
						{
							continue;
						}
						const float Threshold = Elemental->GetThreshold(Elements[i]);
						Lines.Add(FString::Printf(TEXT("%s %d/%d"), Names[i], FMath::RoundToInt(Value), FMath::RoundToInt(Threshold)));
					}
					else if (Phase == EGBBuildUpPhase::Active)
					{
						const FName Reaction = Elemental->GetReaction(Elements[i]);
						Lines.Add(FString::Printf(TEXT("%s %s %.1fs"), Names[i], *Reaction.ToString(), Elemental->GetPhaseTimeRemaining(Elements[i])));
					}
					else
					{
						Lines.Add(FString::Printf(TEXT("%s IMMUNE %.1fs"), Names[i], Elemental->GetPhaseTimeRemaining(Elements[i])));
					}
				}
				if (Enemy->GetControlLockTime() > 0.f)
				{
					Lines.Add(FString::Printf(TEXT("LOCK %.1fs"), Enemy->GetControlLockTime()));
				}
				float Y = Screen.Y;
				for (const FString& Line : Lines)
				{
					DrawLabel(Line, FLinearColor::White, Screen.X, Y, 14.f, 0.5f, true, false);
					Y += 16.f;
				}
			}
		}
	}
	if (Player)
	{
		DrawHands(Player);
		// Menus cover the game: the status HUD only shows while playing (and under the QTE).
		if (!bMenuOpen && !Player->IsDead())
		{
			if (Skin == EGBHudSkin::Cyberpunk)
			{
				DrawCyberHud(Player);
			}
			else
			{
				DrawRpgHud(Player);
			}
			DrawCrosshair(Player, Skin);
			DrawInteractPrompt(Player, Skin);
		}
	}
	if (!bMenuOpen)
	{
		DrawToasts(Skin);
	}
	if (Player && Player->GetQTE() && Player->GetQTE()->IsActive())
	{
		DrawQTE(Player, Skin);
	}
	DrawDamageFlash(Skin);
	DrawCutscene(Skin);
	DrawEraFade();
	if (bMenuOpen)
	{
		UpdateMenuMouse();
		DrawMenus(Player);
		// Menu actions answer with toasts (skill unlocks, combines): above the menu, low, clear of titles and tabs.
		DrawToasts(GetMenuSkin(), Canvas->ClipY * 0.80f);
	}
}

// ======================================================================== hands (GDD 3: always pixel art)

// Dev only: hides every first-person hand (and the staff/sword sprites) to capture clean gun templates for drawing the hands.
static TAutoConsoleVariable<int32> CVarHideHands(TEXT("gb.HideHands"), 0, TEXT("1 hides the first-person hands layer (template screenshots)."));

void AGBHUD::DrawHands(AGBPlayerCharacter* Player)
{
	if (CVarHideHands.GetValueOnGameThread() != 0 || !Player->IsControlEnabled() || Player->IsDead())
	{
		return;
	}
	UGBPixelSpriteLibrary* Sprites = UGBPixelSpriteLibrary::Get(this);
	if (!Sprites)
	{
		return;
	}

	const float W = Canvas->ClipX;
	// The RPG status bar covers the bottom of the view: the hands stand on it (as in the 90s games). Menus hide the bar.
	const bool bOverBar = GetCurrentSkin() == EGBHudSkin::Rpg90s && GetOpenMenu() == EGBMenu::None;
	// While mantling, the normal pose slides down off screen so the climbing hands take over.
	const bool bMantling = Player->IsMantling();
	const GBMantle::FMantlePose MantlePose = Player->GetMantlePose();
	const float H = Canvas->ClipY - (bOverBar ? 22.f * ArtPixel() : 0.f)
		+ (bMantling ? MantlePose.ItemLower01 * Canvas->ClipY * 0.7f : 0.f);
	const float P = FMath::Max(1.f, (float)FMath::RoundToInt(Canvas->ClipY / GBHudLayout::HandPixelDivisor));

	// Walk bob from the horizontal speed (world time, so it slows down with the QTE): the one shared motion
	// of the 2D hands and the 3D gun (GBViewBob). Art pixels (unrounded); the screen shift snaps them to the
	// active grid (hand or CENTRAL overlay) via GBViewBob::MotionToScreenPx, which the gun uses too.
	const UWorld* World = GetWorld();
	const float Time = World ? (float)World->GetTimeSeconds() : 0.f;
	float Speed01 = 0.f;
	if (const UCharacterMovementComponent* Move = Player->GetCharacterMovement())
	{
		if (Move->IsMovingOnGround())
		{
			Speed01 = FMath::Clamp(Player->GetVelocity().Size2D() / FMath::Max(1.f, Move->GetMaxSpeed()), 0.f, 1.f);
		}
	}
	const FVector2D BobArt = GBViewBob::bFrozen ? FVector2D::ZeroVector : GBViewBob::CalcBob(Time, Speed01);
	const FVector2D Bob = GBViewBob::MotionToScreenPx(BobArt, Canvas->ClipY, HasHandOverlay());

	UGBEquipmentComponent* Equipment = Player->GetEquipment();
	AGBHeldItem* Item = Equipment ? Equipment->GetActiveItem() : nullptr;
	const FGBWeaponDefinition* WeaponDef = Item ? Item->GetDefinition() : nullptr;

	const float SinceAttack = Item ? Item->GetTimeSinceLastAttack() : 1000.f;
	const float Kick01 = (SinceAttack >= 0.f && SinceAttack < GBHudLayout::AttackKickTime) ? 1.f - SinceAttack / GBHudLayout::AttackKickTime : 0.f;

	const UGBQTEComponent* QTE = Player->GetQTE();
	const bool bCastPose = (QTE && QTE->IsActive()) || (GetRealTime() - LastCastTime) < GBHudLayout::CastHandTime;

	FName RightHandId(TEXT("hand_right"));
	if (WeaponDef)
	{
		if (const FString* HandSprite = WeaponDef->Strings.Find(GBString::HandSprite))
		{
			RightHandId = FName(**HandSprite);
		}
	}
	UTexture2D* HandRight = Sprites->GetSprite(RightHandId);
	UTexture2D* HandLeft = Sprites->GetSprite(TEXT("hand_left"));
	UTexture2D* HandCast = Sprites->GetSprite(TEXT("hand_cast"));
	auto SizeOf = [P](UTexture2D* Texture) { return Texture ? FVector2D(Texture->GetSizeX() * P, Texture->GetSizeY() * P) : FVector2D::ZeroVector; };
	const FVector2D RightSize = SizeOf(HandRight);
	const FVector2D LeftSize = SizeOf(HandLeft);
	const FVector2D CastSize = SizeOf(HandCast);

	const EGBHeldSlot Slot = Item ? Item->GetSlot() : EGBHeldSlot::Staff;
	if (HasHandOverlay())
	{
		// The CENTRAL art replaces the hands layer: same motion, full screen, over the held gun.
		DrawHandOverlay(Player, BobArt, Kick01);
	}
	else if (Item && Slot == EGBHeldSlot::Staff)
	{
		FName StaffId(TEXT("staff_apprentice"));
		if (WeaponDef)
		{
			if (const FString* SpriteId = WeaponDef->Strings.Find(GBString::Sprite))
			{
				StaffId = FName(**SpriteId);
			}
		}
		UTexture2D* Staff = Sprites->GetSprite(StaffId);
		// The hand-drawn staffs (2026-11-15) are drawn at twice the old staffs' resolution: SpritePixelScale 0.5 keeps
		// them the same size on screen.
		const float SP = FMath::Max(1.f, (float)FMath::RoundToInt(P * Item->GetStat(TEXT("SpritePixelScale"), 1.f)));
		const float StaffPx = SP * GBHudLayout::StaffSizeMult;
		const FVector2D StaffSize = Staff ? FVector2D(Staff->GetSizeX() * StaffPx, Staff->GetSizeY() * StaffPx) : FVector2D::ZeroVector;
		// Rising from the lower right and leaning a little left (owner's sketch, 2026-11-15): it pivots on its grip at the
		// bottom of the view. The basic attack thrusts it a little along its own length, in the direction it leans. No hand on it (owner, 2026-11-15: no hands at all,
		// only the weapons and their own motion).
		if (Staff)
		{
			const float Tilt = FMath::DegreesToRadians(GBHudLayout::StaffTiltDeg);
			const FVector2D Along(-FMath::Sin(Tilt), -FMath::Cos(Tilt)); // grip -> head, on screen
			const FVector2D Grip = FVector2D(W * GBHudLayout::StaffGripX, H) + Bob + Along * (Kick01 * 8.f * P);
			const FVector2D Pivot01(0.5f, GBHudLayout::StaffGripFrac);
			FCanvasTileItem Tile(Grip - StaffSize * Pivot01, Staff->GetResource(), StaffSize, FLinearColor::White);
			Tile.Rotation = FRotator(0.f, -GBHudLayout::StaffTiltDeg, 0.f);
			Tile.PivotPoint = Pivot01;
			Tile.BlendMode = SE_BLEND_Translucent;
			Canvas->DrawItem(Tile);

			// Record where the head is, so staff shots and spells leave from it (owner, 2026-11-15).
			const FVector2D Head = Grip + Along * ((GBHudLayout::StaffGripFrac - GBHudLayout::StaffHeadFrac) * StaffSize.Y);
			FVector RayOrigin, RayDir;
			Canvas->Deproject(Head, RayOrigin, RayDir);
			const FVector AimDir = Player->GetAimDirection().Rotation().UnrotateVector(RayDir);
			if (AimDir.X > 0.1f)
			{
				GBStaffView::SetHeadAimDir(AimDir);
			}
		}
	}
	else if (Item && Slot == EGBHeldSlot::Gun)
	{
		// Guns are shown alone, without hands (owner, 2026-11-14): the 3D model and its own animations (kick, reload,
		// pump) are drawn by the world; nothing is drawn here.
	}
	else if (Item && Slot == EGBHeldSlot::Sword)
	{
		FName BladeId(TEXT("sword_blade"));
		if (WeaponDef)
		{
			if (const FString* SpriteId = WeaponDef->Strings.Find(GBString::Sprite))
			{
				BladeId = FName(**SpriteId);
			}
		}
		UTexture2D* Blade = Sprites->GetSprite(BladeId);
		const FVector2D BladeSize = SizeOf(Blade);
		const FLinearColor Tint = FMath::Lerp(GBHudLocal::White, GBHudText::ElementColor(Item->GetElement()), 0.5f);
		// The swing moves the sword to the left.
		const float CenterX = W * 0.70f + Bob.X - Kick01 * 12.f * P;
		// Where the (no longer drawn) hand held the hilt: the blade rises from the same place.
		const float HandY = H - RightSize.Y * 0.75f + Bob.Y;
		DrawSprite(Blade, CenterX - BladeSize.X * 0.5f, HandY - BladeSize.Y + 4.f * P, P, Tint);
	}
	// Empty-handed: nothing (no hands).

	// The summoned shield is held by the left hand: while it is up (or sliding) the left hand is hidden.
	const UGBSpellbookComponent* Spellbook = Player->GetSpellbook();
	const bool bBlockRaised = Spellbook && Spellbook->HasShield() && Spellbook->IsBlockRaised();
	const double Now = GetRealTime();
	const float Dt = LastShieldUpdateTime < 0.0 ? 0.f : FMath::Clamp((float)(Now - LastShieldUpdateTime), 0.f, 0.1f);
	LastShieldUpdateTime = Now;
	const float Step = Dt / GBHudLayout::ShieldSlideTime;
	ShieldRaise01 = bBlockRaised ? FMath::Min(1.f, ShieldRaise01 + Step) : FMath::Max(0.f, ShieldRaise01 - Step);

	// No left hand, casting hand or climbing hands (owner, 2026-11-15: no hands at all). The raised shield stays down
	// while mantling.
	(void)bCastPose;
	(void)LeftSize;
	(void)CastSize;
	if (!bMantling)
	{
		DrawRaisedShield(Player, P, Bob);
	}

	// Lower-left reminder while the flamethrower (a hold-to-sustain spell) burns (owner, 2026-11-15).
	if (GBStaffView::ShouldShowHoldHint() && GetOpenMenu() == EGBMenu::None)
	{
		const FString HoldText(TEXT("Hold to keep casting"));
		if (GetCurrentSkin() == EGBHudSkin::Cyberpunk)
		{
			const float S = UIScale();
			FCyTextStyle Style;
			Style.Face = ECyFace::Condensed;
			Style.Px = 22.f;
			Style.Spacing = 100;
			Style.bUpper = true;
			Style.Color = GBCy::Pale;
			const FVector2D Size = MeasureCyText(HoldText, Style);
			const float X = 40.f * S;
			const float Y = FMath::RoundToFloat(Canvas->ClipY * 0.80f);
			DrawFade(X - 16.f * S, Y - 8.f * S, Size.X + 32.f * S, 42.f * S, FLinearColor(0.f, 0.f, 0.f, 0.45f), false);
			DrawCyText(HoldText, X, Y, Style, 0.f);
		}
		else
		{
			const float AP = ArtPixel();
			const int32 Scale = FMath::Max(2, FMath::RoundToInt(AP * 0.75f));
			const FVector2D Size = MeasurePixelText(HoldText, Scale);
			const float PW = FMath::CeilToFloat((Size.X + 8.f * AP) / AP) * AP;
			const float PH = 12.f * AP;
			const float X = 6.f * AP;
			const float Y = FMath::RoundToFloat((Canvas->ClipY - (bOverBar ? 22.f * AP : 0.f) - PH - 4.f * AP) / AP) * AP;
			FillRect(X, Y, PW, PH, GBAlpha(GBRpg::StoneDeep, 0.9f));
			RpgFrame(X, Y, PW, PH, ERpgFrame::Bone);
			FPixelTextStyle Style;
			Style.Scale = Scale;
			Style.Ink = &GBRpg::InkBone;
			Style.Shadow = FLinearColor(0.f, 0.f, 0.f, 0.9f);
			DrawPixelText(HoldText, X + 4.f * AP, Y + FMath::RoundToFloat((PH - Size.Y) * 0.5f), Style, 0.f);
		}
	}
}

void AGBHUD::DrawMantleHands(AGBPlayerCharacter* Player, float PixelSize)
{
	UGBPixelSpriteLibrary* Sprites = UGBPixelSpriteLibrary::Get(this);
	if (!Sprites || !Canvas)
	{
		return;
	}
	const GBMantle::FMantlePose Pose = Player->GetMantlePose();
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;

	// The ledge edge on screen: where the knuckles sit when the grip is full. Clamped, with a fallback mid-screen.
	UTexture2D* HandTextures[2] = { Sprites->GetSprite(TEXT("hand_left")), Sprites->GetSprite(TEXT("hand_right")) };
	// Lowest grip line: the whole hand and a bit of sleeve stay above the RPG status bar (drawn over the hands)
	// while gripping. Once the body is over the edge the ledge is below the view, so the hands rest there, pushing down.
	const bool bOverBar = GetCurrentSkin() == EGBHudSkin::Rpg90s && GetOpenMenu() == EGBMenu::None;
	const float ViewBottom = H - (bOverBar ? 22.f * ArtPixel() : 0.f);
	const float HandH = HandTextures[0] ? HandTextures[0]->GetSizeY() * PixelSize : 0.f;
	const float MaxGripY = FMath::Max(0.12f * H, FMath::Min(0.85f * H, ViewBottom - HandH * 1.4f));
	const FVector Projected = Canvas->Project(Player->GetMantleLedgePoint(), true);
	float GripY = FMath::Min(0.45f * H, MaxGripY);
	if (Projected.Z > 0.f)
	{
		GripY = FMath::Clamp(Projected.Y, 0.12f * H, MaxGripY);
	}

	// Wide apart, clear of the status bar's centre crest.
	const float CentersX[2] = { 0.27f * W, 0.73f * W };
	for (int32 Index = 0; Index < 2; ++Index)
	{
		UTexture2D* Hand = HandTextures[Index];
		if (!Hand)
		{
			continue;
		}
		const float SpriteW = Hand->GetSizeX() * PixelSize;
		const float SpriteH = Hand->GetSizeY() * PixelSize;
		// The hands come up from below the screen and go back down (fully hidden at Grip01 = 0).
		const float HandX = CentersX[Index] - SpriteW * 0.5f;
		const float HandY = GripY + (1.f - Pose.Grip01) * (H - GripY + SpriteH);
		DrawSprite(Hand, HandX, HandY, PixelSize);
		// Sleeves: the cuff rows (the bottom 8 of the 20-row sprite) tiled from the wrist down to the screen bottom.
		const float CuffH = SpriteH * 8.f / 20.f;
		if (CuffH <= 0.f)
		{
			continue;
		}
		const float CuffV = 1.f - 8.f / 20.f;
		for (float Y0 = HandY + SpriteH; Y0 < H; Y0 += CuffH)
		{
			const float TileH = FMath::Min(CuffH, H - Y0);
			DrawSpriteTile(Hand, HandX, Y0, SpriteW, TileH, 0.f, CuffV, 1.f, (1.f - CuffV) * (TileH / CuffH));
		}
	}
}

float AGBHUD::DrawRaisedShield(AGBPlayerCharacter* Player, float PixelSize, const FVector2D& Bob)
{
	if (ShieldRaise01 <= 0.f)
	{
		return 0.f;
	}
	UGBPixelSpriteLibrary* Sprites = UGBPixelSpriteLibrary::Get(this);
	const UGBSpellbookComponent* Spellbook = Player ? Player->GetSpellbook() : nullptr;
	const AGBSpellActor* ShieldActor = Spellbook ? Spellbook->GetShieldActor() : nullptr;
	FString SpriteId = ShieldActor ? ShieldActor->GetSpellCast().GetString(GBString::Sprite, TEXT("ice_shield")) : FString(TEXT("ice_shield"));
	if (SpriteId.IsEmpty())
	{
		SpriteId = TEXT("ice_shield");
	}
	UTexture2D* Shield = Sprites ? Sprites->GetSprite(FName(*SpriteId)) : nullptr;
	if (!Shield)
	{
		return ShieldRaise01;
	}
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	// Whole sprite pixels, like the hands, only bigger: the shield is close to the eye.
	const float Scale = FMath::Max(1.f, (float)FMath::RoundToInt(PixelSize * GBHudLayout::ShieldPixelMult));
	const FVector2D Size(Shield->GetSizeX() * Scale, Shield->GetSizeY() * Scale);
	const float Ease = FMath::InterpEaseOut(0.f, 1.f, ShieldRaise01, 2.f);
	// Center X and top Y: from below the lower left corner up towards the middle, leaving the crosshair visible.
	const FVector2D Lowered(W * 0.10f, H + Size.Y * 0.05f);
	const FVector2D Raised(W * 0.36f, H * 0.40f);
	const FVector2D Pos = FMath::Lerp(Lowered, Raised, Ease);
	const float Health01 = Spellbook ? FMath::Clamp(Spellbook->GetShieldHealthFraction(), 0.f, 1.f) : 0.f;
	// A worn shield is more see-through. HudTint optionally tints it (default white, so Color never changes the look here).
	FLinearColor BaseTint(FLinearColor::White);
	if (ShieldActor)
	{
		const FString HudTintStr = ShieldActor->GetSpellCast().GetString(FName(TEXT("HudTint")));
		if (!HudTintStr.IsEmpty())
		{
			BaseTint = UGBDataSubsystem::ParseColor(HudTintStr, FLinearColor::White);
		}
	}
	const FLinearColor Tint(BaseTint.R, BaseTint.G, BaseTint.B, FMath::Lerp(0.6f, 0.95f, Health01));
	DrawSprite(Shield, Pos.X - Size.X * 0.5f + Bob.X * 0.5f, Pos.Y + Bob.Y * 0.5f, Scale, Tint);
	return ShieldRaise01;
}

void AGBHUD::SetHandOverlay(UTexture2D* Texture)
{
	HandOverlayTexture = Texture;
	GBViewBob::bOverlayShown = (Texture != nullptr);
}

void AGBHUD::ClearHandOverlay()
{
	HandOverlayTexture = nullptr;
	GBViewBob::bOverlayShown = false;
}

void AGBHUD::DrawHandOverlay(AGBPlayerCharacter* Player, const FVector2D& BobArt, float Kick01)
{
	if (!HandOverlayTexture || !Canvas)
	{
		return;
	}
	// The same motion the hands get, in old hand art pixels: bob (shared with the gun), kick, reload follow,
	// seat push and pump shift. A reload parks the pump, so reload follow and pump shift never stack.
	// One motion grid with the gun: art * ClipY/120, snapped to whole CENTRAL overlay art pixels.
	FVector2D MotionArt = BobArt + FVector2D(0.f, Kick01 * 3.f);
	if (const UGBEquipmentComponent* Equipment = Player->GetEquipment())
	{
		if (const AGBGun* Gun = Cast<AGBGun>(Equipment->GetActiveItem()))
		{
			MotionArt += Gun->GetReloadRightHandFollow() * Gun->GetReloadPoseAmount();
			MotionArt += Gun->GetPumpHandShift();
			MotionArt.Y -= Gun->GetReloadHandBeat().Push * GBHudLayout::ReloadSeatPush;
		}
	}
	const FVector2D Motion = GBViewBob::MotionToScreenPx(MotionArt, Canvas->ClipY, true);
	// A 480x270 file, scaled to the screen height, centred horizontally, bottom-aligned. One CENTRAL art pixel
	// is 3 file pixels (ClipY/90 screen pixels, 12 at 1080p); nearest filtering keeps whole file pixels whole.
	const float Q = FMath::Max(1.f, FMath::RoundToFloat(Canvas->ClipY / 270.f));
	const float W = 480.f * Q;
	const float H = 270.f * Q;
	DrawTexture(HandOverlayTexture, FMath::RoundToFloat((Canvas->ClipX - W) * 0.5f + Motion.X),
		FMath::RoundToFloat(Canvas->ClipY - H + Motion.Y), W, H,
		0.f, 0.f, 1.f, 1.f, FLinearColor::White, BLEND_Translucent);
}

// ======================================================================== crosshair, prompt, toasts

void AGBHUD::DrawCrosshair(AGBPlayerCharacter* Player, EGBHudSkin Skin)
{
	if (!Player->IsControlEnabled() || Player->IsDead())
	{
		return;
	}
	const float S = UIScale();
	const float CX = FMath::RoundToFloat(Canvas->ClipX * 0.5f);
	const float CY = FMath::RoundToFloat(Canvas->ClipY * 0.5f);
	const UGBEquipmentComponent* Equipment = Player->GetEquipment();
	AGBHeldItem* Item = Equipment ? Equipment->GetActiveItem() : nullptr;
	const bool bGun = Item && Item->GetSlot() == EGBHeldSlot::Gun;

	if (Skin == EGBHudSkin::Cyberpunk || bGun)
	{
		// Center dot and four ticks at the current spread cone (degrees -> pixels with the vertical camera FOV), with a dark keyline.
		const float T = FMath::Max(1.f, FMath::RoundToFloat(2.f * S));
		const FLinearColor Ink = GBCy::Pale;
		const FLinearColor Key(0.f, 0.f, 0.f, 0.55f);
		float Offset = 6.f * S;
		if (bGun)
		{
			const AGBGun* Gun = Cast<AGBGun>(Item);
			const float SpreadDeg = Gun ? Gun->GetCurrentSpread() : Item->GetStat(Item->IsAiming() ? GBStat::AimSpread : GBStat::Spread, 0.f);
			const float FOV = (PlayerOwner && PlayerOwner->PlayerCameraManager) ? PlayerOwner->PlayerCameraManager->GetFOVAngle() : 58.72f;
			const float HalfFOVTan = FMath::Tan(FMath::DegreesToRadians(FMath::Clamp(FOV, 5.f, 170.f) * 0.5f));
			Offset = FMath::Max(FMath::Tan(FMath::DegreesToRadians(FMath::Clamp(SpreadDeg, 0.f, 80.f))) / HalfFOVTan * Canvas->ClipY * 0.5f, 6.f * S);
		}
		Offset = FMath::RoundToFloat(Offset);
		const float L = FMath::RoundToFloat(9.f * S);
		auto Tick = [this, &Key, &Ink](float X, float Y, float W, float H)
		{
			FillRect(X - 1.f, Y - 1.f, W + 2.f, H + 2.f, Key);
			FillRect(X, Y, W, H, Ink);
		};
		Tick(CX - T * 0.5f, CY - T * 0.5f, T, T);
		Tick(CX - Offset - L, CY - T * 0.5f, L, T);
		Tick(CX + Offset, CY - T * 0.5f, L, T);
		Tick(CX - T * 0.5f, CY + Offset, T, L);
		// Four equal ticks (owner, Nov 13: the red bar that capped the top tick read as a stray line).
		Tick(CX - T * 0.5f, CY - Offset - L, T, L);
		return;
	}

	// RPG: a small pixel rune, gold with a dark outline, on a whole-pixel grid.
	const float Px = FMath::Max(2.f, FMath::RoundToFloat(ArtPixel() * 0.5f));
	const bool bSword = Item && Item->GetSlot() == EGBHeldSlot::Sword;
	auto Dot = [this, Px, CX, CY](int32 GX, int32 GY, const FLinearColor& C)
	{
		FillRect(CX + (GX - 0.5f) * Px, CY + (GY - 0.5f) * Px, Px, Px, C);
	};
	const FLinearColor Gold = GBRpg::Gold3;
	const FLinearColor Out = GBAlpha(GBRpg::Outline, 0.85f);
	if (bSword)
	{
		// A diamond.
		const int32 Pts[][2] = { {0,-2}, {-1,-1}, {1,-1}, {-2,0}, {2,0}, {-1,1}, {1,1}, {0,2} };
		for (const auto& Pt : Pts) { for (int32 DY = -1; DY <= 1; ++DY) for (int32 DX = -1; DX <= 1; ++DX) Dot(Pt[0] + DX, Pt[1] + DY, Out); }
		for (const auto& Pt : Pts) { Dot(Pt[0], Pt[1], Gold); }
		return;
	}
	// Four arms of two pixels around a gap, plus the center pixel.
	const int32 Arms[][2] = { {0,-3}, {0,-4}, {0,3}, {0,4}, {-3,0}, {-4,0}, {3,0}, {4,0}, {0,0} };
	for (const auto& Pt : Arms) { for (int32 DY = -1; DY <= 1; ++DY) for (int32 DX = -1; DX <= 1; ++DX) Dot(Pt[0] + DX, Pt[1] + DY, Out); }
	for (const auto& Pt : Arms) { Dot(Pt[0], Pt[1], (Pt[0] == 0 && Pt[1] == 0) ? GBRpg::Gold4 : Gold); }
}

void AGBHUD::DrawInteractPrompt(AGBPlayerCharacter* Player, EGBHudSkin Skin)
{
	IGBInteractable* Interactable = Player->GetFocusedInteractable();
	if (!Interactable)
	{
		return;
	}
	const FString Text = Interactable->GetInteractPrompt(Player);
	const float CX = Canvas->ClipX * 0.5f;
	const float Y = FMath::RoundToFloat(Canvas->ClipY * 0.6f);
	if (Skin == EGBHudSkin::Cyberpunk)
	{
		const float S = UIScale();
		FCyTextStyle Style;
		Style.Face = ECyFace::Condensed;
		Style.Px = 22.f;
		Style.Spacing = 100;
		Style.bUpper = true;
		Style.Color = GBCy::Pale;
		const FVector2D Size = MeasureCyText(Text, Style);
		const float KeyW = 30.f * S;
		const float Total = KeyW + 12.f * S + Size.X;
		const float X = CX - Total * 0.5f;
		DrawFade(X - 40.f * S, Y - 8.f * S, Total + 80.f * S, 46.f * S, FLinearColor(0.f, 0.f, 0.f, 0.45f), false);
		CyKey(X, Y, TEXT("G"), GBCy::Cyan, 30.f);
		DrawCyText(Text, X + KeyW + 12.f * S, Y + 1.f * S, Style, 0.f);
		FillRect(X + KeyW + 12.f * S, Y + 32.f * S, Size.X, 1.f, GBAlpha(GBCy::Red, 0.7f));
		return;
	}
	const float P = ArtPixel();
	const int32 Scale = FMath::Max(2, FMath::RoundToInt(P * 0.75f));
	const FVector2D Size = MeasurePixelText(Text, Scale);
	float KeyW = 0.f;
	const float PW = FMath::CeilToFloat((Size.X + 22.f * P) / P) * P;
	const float PH = 14.f * P;
	const float X = FMath::RoundToFloat((CX - PW * 0.5f) / P) * P;
	FillRect(X, Y, PW, PH, GBAlpha(GBRpg::StoneDeep, 0.9f));
	RpgFrame(X, Y, PW, PH, ERpgFrame::Bone);
	RpgKeyCap(X + 3.f * P, Y + 2.f * P, TEXT("G"), &KeyW);
	FPixelTextStyle Style;
	Style.Scale = Scale;
	Style.Ink = &GBRpg::InkBone;
	Style.Shadow = FLinearColor(0.f, 0.f, 0.f, 0.9f);
	DrawPixelText(Text, X + 3.f * P + KeyW + 4.f * P, Y + FMath::RoundToFloat((PH - Size.Y) * 0.5f), Style, 0.f);
}

void AGBHUD::DrawToasts(EGBHudSkin Skin, float TopY)
{
	const double Now = GetRealTime();
	const float CX = Canvas->ClipX * 0.5f;
	float Y = FMath::RoundToFloat(TopY >= 0.f ? TopY : Canvas->ClipY * 0.13f);
	if (TopY >= 0.f && Toasts.Num() > 0)
	{
		// A stack anchored low (menus) moves up as it grows, so the newest toast never leaves the screen.
		const float Step = Skin == EGBHudSkin::Cyberpunk ? 46.f * UIScale() : 16.f * ArtPixel();
		const float Margin = Canvas->ClipY * 0.04f;
		Y = FMath::RoundToFloat(FMath::Min(Y, Canvas->ClipY - Margin - Toasts.Num() * Step));
	}
	for (const FToast& Toast : Toasts)
	{
		const float Alpha = FMath::Clamp((float)(Toast.EndTime - Now) / GBHudLayout::ToastFadeTime, 0.f, 1.f);
		const float In = FMath::Clamp((float)(Now - Toast.StartTime) / GBHudLayout::ToastInTime, 0.f, 1.f);
		if (Skin == EGBHudSkin::Cyberpunk)
		{
			const float S = UIScale();
			FCyTextStyle Style;
			Style.Face = ECyFace::Condensed;
			Style.Px = 21.f;
			Style.Spacing = 90;
			Style.bUpper = true;
			Style.Color = GBAlpha(GBCy::Pale, Alpha);
			const FVector2D Size = MeasureCyText(Toast.Text, Style);
			const float W = Size.X + 44.f * S;
			const float H = 38.f * S;
			const float X = CX - W * 0.5f;
			CyBox(X, Y, W, H, 10.f * S, GBAlpha(GBCy::Ink, 0.8f * Alpha), GBAlpha(GBCy::RedDim, Alpha), 0x2);
			FillRect(X, Y, 4.f * S, H, GBAlpha(GBCy::Red, Alpha));
			// Typed on: the letters appear left to right while the box opens.
			const int32 Shown = FMath::Clamp(FMath::CeilToInt(Toast.Text.Len() * FMath::Clamp((float)(Now - Toast.StartTime) / 0.3f, 0.f, 1.f)), 0, Toast.Text.Len());
			Style.Split = (1.f - In) * 4.f * S;
			DrawCyText(Toast.Text.Left(Shown), X + 24.f * S, Y + (H - Size.Y) * 0.5f, Style, 0.f);
			Y += H + 8.f * S;
			continue;
		}
		const float P = ArtPixel();
		const int32 Scale = FMath::Max(2, FMath::RoundToInt(P * 0.75f));
		const FVector2D Size = MeasurePixelText(Toast.Text, Scale);
		const float W = FMath::CeilToFloat((Size.X + 16.f * P) / P) * P;
		const float H = 13.f * P;
		const float X = FMath::RoundToFloat((CX - W * 0.5f) / P) * P;
		const float Slide = FMath::RoundToFloat((1.f - In) * 6.f) * P;
		const float TY = Y - Slide;
		FillRect(X, TY, W, H, GBAlpha(GBRpg::StoneDeep, 0.92f * Alpha));
		RpgStone(X + P, TY + P, W - 2.f * P, H - 2.f * P, FLinearColor(1.f, 1.f, 1.f, Alpha));
		if (Alpha >= 1.f)
		{
			RpgFrame(X, TY, W, H, ERpgFrame::Gold);
		}
		else
		{
			DrawOutline(X, TY, W, H, GBAlpha(GBRpg::Gold2, Alpha), P);
		}
		FPixelTextStyle Style;
		Style.Scale = Scale;
		Style.Ink = &GBRpg::InkBone;
		Style.Color = FLinearColor(1.f, 1.f, 1.f, Alpha);
		Style.Shadow = FLinearColor(0.f, 0.f, 0.f, 0.9f);
		DrawPixelText(Toast.Text, CX, TY + FMath::RoundToFloat((H - Size.Y) * 0.5f), Style, 0.5f);
		Y += H + 3.f * P;
	}
}

// ======================================================================== QTE (GDD 7)

void AGBHUD::DrawQTEElementChoice(AGBPlayerCharacter* Player, const FGBQTESession& Session, EGBHudSkin Skin)
{
	const float S = UIScale();
	const float P = ArtPixel();
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	const float CX = W * 0.5f;
	const bool bCyber = Skin == EGBHudSkin::Cyberpunk;
	const UGBSpellbookComponent* Spellbook = Player->GetSpellbook();
	const EGBElement Current = Spellbook ? Spellbook->GetEnchantElement() : EGBElement::Fire;
	const int32 Count = Session.Config.ChoiceKeys.Num();
	const float TileW = bCyber ? FMath::RoundToFloat(150.f * S) : FMath::RoundToFloat(40.f * P / P) * P;
	const float TileH = bCyber ? FMath::RoundToFloat(120.f * S) : 32.f * P;
	const float Gap = bCyber ? FMath::RoundToFloat(24.f * S) : 6.f * P;
	const float RowY = FMath::RoundToFloat(H * 0.255f / P) * P;
	const float X0 = FMath::RoundToFloat((CX - (Count * TileW + FMath::Max(0, Count - 1) * Gap) * 0.5f) / P) * P;
	for (int32 i = 0; i < Count; ++i)
	{
		const EGBElement Element = UGBSpellbookComponent::GetEnchantChoiceElement(i);
		const FLinearColor EC = GBHudText::ElementColor(Element);
		const bool bCurrent = Element == Current;
		// The key that picks it sits where it is on the keyboard: W (the middle option) a little higher than A and D.
		const float X = X0 + i * (TileW + Gap);
		const float Y = RowY + (Session.Config.ChoiceKeys[i] == EGBQTEKey::Up ? 0.f : (bCyber ? 18.f * S : 4.f * P));
		const FString Letter = GBHudLocal::QTEKeyLetter(Session.Config.ChoiceKeys[i]);
		const FString Name = GBHudText::ElementName(Element).ToUpper();
		if (bCyber)
		{
			DrawGlow(X + TileW * 0.5f, Y + TileH * 0.5f, TileW * 0.8f, TileH * 0.8f, GBAlpha(EC, 0.18f));
			CyBox(X, Y, TileW, TileH, 14.f * S, GBAlpha(GBCy::Ink, 0.85f), bCurrent ? GBCy::Pale : EC, 0x5, bCurrent ? 2.f : 1.f);
			FillRect(X + 12.f * S, Y + TileH - 8.f * S, TileW - 24.f * S, 3.f * S, EC);
			FCyTextStyle Key;
			Key.Face = ECyFace::Condensed;
			Key.Px = 44.f;
			Key.Color = EC;
			DrawCyText(Letter, X + TileW * 0.5f, Y + 10.f * S, Key, 0.5f);
			FCyTextStyle Label;
			Label.Face = ECyFace::Condensed;
			Label.Px = 20.f;
			Label.Spacing = 140;
			Label.bUpper = true;
			Label.Color = GBCy::Pale;
			DrawCyText(Name, X + TileW * 0.5f, Y + 68.f * S, Label, 0.5f);
			if (bCurrent)
			{
				FCyTextStyle Tag;
				Tag.Face = ECyFace::Mono;
				Tag.Px = 11.f;
				Tag.Color = GBCy::Pale;
				DrawCyText(TEXT("CURRENT"), X + TileW * 0.5f, Y - 18.f * S, Tag, 0.5f);
			}
		}
		else
		{
			DrawGlow(X + TileW * 0.5f, Y + TileH * 0.5f, TileW * 0.8f, TileH * 0.8f, GBAlpha(EC, 0.2f));
			FillRect(X, Y, TileW, TileH, GBRpg::StoneDeep);
			RpgStone(X, Y, TileW, TileH, FLinearColor(0.8f, 0.8f, 0.9f, 0.6f));
			RpgFrame(X, Y, TileW, TileH, bCurrent ? ERpgFrame::Gold : ERpgFrame::Iron);
			RpgGem(X + TileW * 0.5f, Y + 9.f * P, EC, true);
			FPixelTextStyle Key;
			Key.Scale = FMath::Max(2, FMath::RoundToInt(P));
			Key.Ink = &GBRpg::InkBone;
			Key.Outline = GBRpg::Outline;
			DrawPixelText(Letter, X + TileW * 0.5f, Y + 14.f * P, Key, 0.5f);
			FPixelTextStyle Label;
			Label.Scale = FMath::Max(2, FMath::RoundToInt(P * 0.5f));
			Label.Color = EC;
			Label.Shadow = FLinearColor(0.f, 0.f, 0.f, 0.9f);
			DrawPixelText(Name, X + TileW * 0.5f, Y + TileH - 7.f * P, Label, 0.5f);
			if (bCurrent)
			{
				FPixelTextStyle Tag;
				Tag.Scale = FMath::Max(2, FMath::RoundToInt(P * 0.5f));
				Tag.Ink = &GBRpg::InkGold;
				Tag.Shadow = FLinearColor(0.f, 0.f, 0.f, 0.9f);
				DrawPixelText(TEXT("CURRENT"), X + TileW * 0.5f, Y - 6.f * P, Tag, 0.5f);
			}
		}
	}
	UiText(Skin, EGBTextSize::Body, EGBTextTone::Normal, TEXT("Choose the element"), CX, RowY + TileH + (bCyber ? 34.f * S : 9.f * P), 0.5f);
}

void AGBHUD::DrawQTE(AGBPlayerCharacter* Player, EGBHudSkin Skin)
{
	const UGBQTEComponent* QTE = Player->GetQTE();
	const FGBQTESession& Session = QTE->GetSession();
	const float S = UIScale();
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	const float CX = W * 0.5f;
	const bool bCyber = Skin == EGBHudSkin::Cyberpunk;
	const float P = ArtPixel();

	// Backdrop.
	FillRect(0.f, 0.f, W, H, FLinearColor(0.f, 0.f, 0.f, bCyber ? 0.5f : 0.38f));
	if (bCyber)
	{
		DrawGlow(CX, H * 0.4f, W * 0.42f, H * 0.5f, FLinearColor(0.f, 0.f, 0.f, 0.55f));
		CyScanlines(0.f, 0.f, W, H, 0.12f);
		CyBrackets(W * 0.22f, H * 0.06f, W * 0.56f, H * 0.74f, 26.f * S, GBAlpha(GBCy::Red, 0.7f), 2.f * S);
	}
	else
	{
		DrawGlow(CX, H * 0.48f, W * 0.42f, H * 0.5f, GBAlpha(GBRpg::Arcane, 0.18f));
	}

	// Title and time.
	const FString Title = GBHudText::SpellName(this, QTE->GetSpellId());
	// The element choice has its own clock: the bar shows it while choosing, then the QTE's own.
	const bool bChoosing = Session.Phase == EGBQTEPhase::Choice;
	const float Limit = bChoosing ? Session.Config.ChoiceTimeLimit : Session.Config.TimeLimit;
	const float Remaining = bChoosing ? Session.GetChoiceTimeRemaining() : Session.GetTimeRemaining();
	const float TimeFrac = Limit > 0.f ? FMath::Clamp(Remaining / Limit, 0.f, 1.f) : 0.f;
	const float BarW = FMath::RoundToFloat(560.f * S);
	float TimeY = 0.f;
	if (bCyber)
	{
		FCyTextStyle Sub;
		Sub.Face = ECyFace::Mono;
		Sub.Px = 16.f;
		Sub.Color = GBCy::RedBright;
		DrawCyText(TEXT("// SYNC SEQUENCE"), CX, H * 0.08f, Sub, 0.5f);
		FCyTextStyle Big;
		Big.Face = ECyFace::Condensed;
		Big.Px = 50.f;
		Big.Spacing = 120;
		Big.bUpper = true;
		Big.Color = GBCy::Red;
		Big.Split = 2.f * S;
		DrawCyText(Title, CX, H * 0.105f, Big, 0.5f);
		TimeY = H * 0.19f;
		CySlantBar(CX - BarW * 0.5f, TimeY, BarW, 10.f * S, 5.f * S, TimeFrac, 30, TimeFrac > 0.3f ? GBCy::Cyan : GBCy::Yellow, GBAlpha(GBCy::CyanDeep, 0.8f));
		FCyTextStyle T;
		T.Face = ECyFace::Mono;
		T.Px = 14.f;
		T.Color = TimeFrac > 0.3f ? GBCy::Cyan : GBCy::Yellow;
		DrawCyText(FString::Printf(TEXT("%05.2f"), Remaining), CX + BarW * 0.5f + 12.f * S, TimeY - 4.f * S, T, 0.f);
	}
	else
	{
		const int32 TitleScale = FMath::Max(3, FMath::RoundToInt(5.f * S));
		const FBox2D Plaque = RpgPlaque(Title.ToUpper(), CX, FMath::RoundToFloat(H * 0.08f), TitleScale);
		TimeY = FMath::RoundToFloat(Plaque.Max.Y + 5.f * P);
		RpgTube(FMath::RoundToFloat((CX - BarW * 0.5f) / P) * P, TimeY, FMath::RoundToFloat(BarW / P) * P, 6.f * P, TimeFrac, TimeFrac > 0.3f ? GBRpg::StaminaRamp : GBRpg::HealthRamp);
	}

	// The weapon enchant's first step: pick the element, always in the same places (owner, Nov 13).
	if (bChoosing)
	{
		DrawQTEElementChoice(Player, Session, Skin);
	}

	// Part 1: the key sequence (power). Hidden while the element is being chosen.
	const int32 NumKeys = bChoosing ? 0 : Session.Sequence.Num();
	const int32 Done = Session.SequenceResults.Num();
	const float Box = bCyber ? FMath::RoundToFloat(60.f * S) : 18.f * P;
	const float Gap = bCyber ? FMath::RoundToFloat(14.f * S) : 4.f * P;
	const float RowY = FMath::RoundToFloat(H * 0.255f);
	const float X0 = FMath::RoundToFloat(CX - (NumKeys * Box + FMath::Max(0, NumKeys - 1) * Gap) * 0.5f);
	int32 RightKeys = 0;
	for (int32 i = 0; i < NumKeys; ++i)
	{
		const bool bConsumed = i < Done;
		const bool bCurrent = Session.Phase == EGBQTEPhase::Sequence && i == Done;
		const bool bRight = bConsumed && Session.SequenceResults[i];
		if (bRight)
		{
			++RightKeys;
		}
		const float X = X0 + i * (Box + Gap);
		const FString Letter = GBHudLocal::QTEKeyLetter(Session.Sequence[i]);
		if (bCyber)
		{
			const FLinearColor Fill = bConsumed ? (bRight ? GBAlpha(GBCy::Green, 0.25f) : GBAlpha(GBCy::Red, 0.35f)) : GBAlpha(GBCy::Ink, 0.75f);
			const FLinearColor Line = bConsumed ? (bRight ? GBCy::Green : GBCy::Red) : (bCurrent ? GBCy::Cyan : GBCy::RedDim);
			CyBox(X, RowY, Box, Box, 10.f * S, Fill, Line, 0x5, bCurrent ? 2.f : 1.f);
			if (bCurrent)
			{
				CyBrackets(X - 6.f * S, RowY - 6.f * S, Box + 12.f * S, Box + 12.f * S, 10.f * S, GBCy::Cyan, 2.f * S);
				DrawGlow(X + Box * 0.5f, RowY + Box * 0.5f, Box, Box, GBAlpha(GBCy::Cyan, 0.18f));
			}
			FCyTextStyle KeyStyle;
			KeyStyle.Face = ECyFace::Condensed;
			KeyStyle.Px = 34.f;
			KeyStyle.Color = bConsumed ? (bRight ? GBCy::Green : GBCy::Red) : (bCurrent ? GBCy::Pale : GBCy::RedBright);
			const FVector2D KS = MeasureCyText(Letter, KeyStyle);
			DrawCyText(Letter, X + Box * 0.5f, RowY + (Box - KS.Y) * 0.5f, KeyStyle, 0.5f);
		}
		else
		{
			if (bCurrent)
			{
				DrawGlow(X + Box * 0.5f, RowY + Box * 0.5f, Box * 1.1f, Box * 1.1f, GBAlpha(GBRpg::EmberLight, 0.35f));
			}
			RpgFrame(X, RowY, Box, Box, bCurrent ? ERpgFrame::Gold : ERpgFrame::Iron);
			const float In = bCurrent ? 3.f * P : 4.f * P;
			RpgStone(X + In, RowY + In, Box - 2.f * In, Box - 2.f * In, bConsumed ? (bRight ? FLinearColor(0.5f, 1.f, 0.5f, 1.f) : FLinearColor(1.f, 0.4f, 0.35f, 1.f)) : FLinearColor::White);
			FPixelTextStyle KeyStyle;
			KeyStyle.Scale = FMath::Max(2, FMath::RoundToInt(P));
			KeyStyle.Ink = bConsumed ? (bRight ? &GBRpg::InkIce : &GBRpg::InkBlood) : (bCurrent ? &GBRpg::InkGold : &GBRpg::InkBone);
			KeyStyle.Outline = GBRpg::Outline;
			DrawPixelText(Letter, X + Box * 0.5f, RowY + FMath::RoundToFloat((Box - 7.f * KeyStyle.Scale) * 0.5f), KeyStyle, 0.5f);
		}
	}
	const float BelowKeys = RowY + Box + (bCyber ? 18.f * S : 5.f * P);
	if (Session.Phase == EGBQTEPhase::Sequence)
	{
		UiText(Skin, EGBTextSize::Body, EGBTextTone::Normal, TEXT("Press the keys in order"), CX, BelowKeys, 0.5f);
	}

	// Part 2: the gesture (reach).
	const int32 NumGestures = Session.Gestures.Num();
	const int32 Judged = Session.GestureScores.Num();
	float ScoreSum = 0.f;
	for (const float Score : Session.GestureScores)
	{
		ScoreSum += Score;
	}
	if (Session.Phase == EGBQTEPhase::Gesture && Judged < NumGestures)
	{
		const EGBGesture Gesture = Session.Gestures[Judged];
		FString GestureTitle = GBHudLocal::GestureText(Gesture);
		if (NumGestures > 1)
		{
			GestureTitle += FString::Printf(TEXT(" (%d/%d)"), Judged + 1, NumGestures);
		}
		UiText(Skin, EGBTextSize::Heading, EGBTextTone::Accent, GestureTitle, CX, H * 0.34f, 0.5f);
		const float CY = H * 0.52f;
		const FLinearColor HintColor = bCyber ? GBAlpha(GBCy::Cyan, 0.35f) : GBAlpha(GBRpg::Gold3, 0.4f);
		DrawGestureHint((uint8)Gesture, CX, CY, 110.f * S, HintColor, (bCyber ? 2.f : 5.f) * S);

		// The stroke is in accumulated screen units from (0,0): draw it centered on screen, shrunk if it is too big.
		const TArray<FVector2D>& Stroke = Session.CurrentStroke;
		if (Stroke.Num() >= 2)
		{
			FVector2D Min = Stroke[0];
			FVector2D Max = Stroke[0];
			for (const FVector2D& Point : Stroke)
			{
				Min = FVector2D(FMath::Min(Min.X, Point.X), FMath::Min(Min.Y, Point.Y));
				Max = FVector2D(FMath::Max(Max.X, Point.X), FMath::Max(Max.Y, Point.Y));
			}
			const FVector2D Center = (Min + Max) * 0.5f;
			const float Extent = FMath::Max3((float)(Max.X - Min.X), (float)(Max.Y - Min.Y), 1.f);
			const float Fit = FMath::Min(1.f, 320.f * S / Extent);
			const FLinearColor Glow = bCyber ? GBAlpha(GBCy::Cyan, 0.3f) : GBAlpha(GBRpg::ArcaneLight, 0.3f);
			const FLinearColor Core = bCyber ? GBCy::Pale : GBRpg::Gold4;
			for (int32 i = 1; i < Stroke.Num(); ++i)
			{
				const FVector2D A = (Stroke[i - 1] - Center) * Fit;
				const FVector2D B = (Stroke[i] - Center) * Fit;
				DrawLine(CX + A.X, CY + A.Y, CX + B.X, CY + B.Y, Glow, 10.f * S);
			}
			for (int32 i = 1; i < Stroke.Num(); ++i)
			{
				const FVector2D A = (Stroke[i - 1] - Center) * Fit;
				const FVector2D B = (Stroke[i] - Center) * Fit;
				DrawLine(CX + A.X, CY + A.Y, CX + B.X, CY + B.Y, Core, 3.f * S);
			}
			const FVector2D Tip = (Stroke.Last() - Center) * Fit;
			DrawGlow(CX + Tip.X, CY + Tip.Y, 22.f * S, 22.f * S, bCyber ? GBAlpha(GBCy::Cyan, 0.6f) : GBAlpha(GBRpg::EmberLight, 0.6f));
		}
	}

	// Running scores.
	const float Power = NumKeys > 0 ? (float)RightKeys / NumKeys : 0.f;
	const float Reach = NumGestures > 0 ? ScoreSum / NumGestures : 0.f;
	const float ScoreY = H * 0.72f;
	for (int32 i = 0; i < 2; ++i)
	{
		const float SX = CX + (i == 0 ? -220.f : 220.f) * S;
		const FString Label = i == 0 ? TEXT("POWER") : TEXT("REACH");
		const FString Value = FString::Printf(TEXT("%d%%"), FMath::RoundToInt((i == 0 ? Power : Reach) * 100.f));
		if (bCyber)
		{
			FCyTextStyle L;
			L.Face = ECyFace::Mono;
			L.Px = 16.f;
			L.Color = GBCy::RedBright;
			CyBox(SX - 90.f * S, ScoreY - 10.f * S, 180.f * S, 96.f * S, 12.f * S, GBAlpha(GBCy::Ink, 0.7f), GBAlpha(GBCy::RedDim, 0.9f), 0x5);
			DrawCyText(Label, SX, ScoreY, L, 0.5f);
			FCyTextStyle V;
			V.Face = ECyFace::Condensed;
			V.Px = 44.f;
			V.Color = GBCy::Cyan;
			DrawCyText(Value, SX, ScoreY + 18.f * S, V, 0.5f);
			FillRect(SX - 60.f * S, ScoreY + 74.f * S, 120.f * S, 1.f, GBCy::RedDim);
		}
		else
		{
			const float PW = 44.f * P;
			const float PH = 26.f * P;
			const float PX = FMath::RoundToFloat((SX - PW * 0.5f) / P) * P;
			const float PY = FMath::RoundToFloat((ScoreY - 4.f * P) / P) * P;
			FillRect(PX, PY, PW, PH, GBRpg::Stone1);
			RpgStone(PX, PY, PW, PH);
			RpgFrame(PX, PY, PW, PH, ERpgFrame::Gold);
			UiText(Skin, EGBTextSize::Small, EGBTextTone::Normal, Label, SX, PY + 5.f * P, 0.5f);
			UiText(Skin, EGBTextSize::Number, EGBTextTone::Accent, Value, SX, PY + 11.f * P, 0.5f);
		}
	}
}

void AGBHUD::DrawGestureHint(uint8 GestureByte, float CenterX, float CenterY, float Radius, const FLinearColor& Color, float Thick)
{
	const EGBGesture Gesture = (EGBGesture)GestureByte;
	const float S = UIScale();
	const FVector2D C(CenterX, CenterY);
	auto Line = [this, &Color, Thick](const FVector2D& A, const FVector2D& B)
	{
		DrawLine(A.X, A.Y, B.X, B.Y, Color, Thick);
	};
	auto Arrowhead = [&Line, S](const FVector2D& Tip, const FVector2D& InDir)
	{
		const FVector2D Dir = InDir.GetSafeNormal();
		const FVector2D Normal(-Dir.Y, Dir.X);
		const float Len = 24.f * S;
		Line(Tip, Tip - Dir * Len + Normal * Len * 0.6f);
		Line(Tip, Tip - Dir * Len - Normal * Len * 0.6f);
	};

	FVector2D Dir = FVector2D::ZeroVector;
	switch (Gesture)
	{
	case EGBGesture::LineRight: Dir = FVector2D(1.f, 0.f); break;
	case EGBGesture::LineLeft: Dir = FVector2D(-1.f, 0.f); break;
	case EGBGesture::LineDown: Dir = FVector2D(0.f, 1.f); break;
	case EGBGesture::LineUp: Dir = FVector2D(0.f, -1.f); break;
	default: break;
	}
	if (!Dir.IsZero())
	{
		Line(C - Dir * Radius, C + Dir * Radius);
		Arrowhead(C + Dir * Radius, Dir);
		return;
	}

	// Circles: screen +Y is down, so a growing angle turns clockwise on screen. 300 degrees, starting at the top.
	const float Sign = Gesture == EGBGesture::CircleClockwise ? 1.f : -1.f;
	const int32 Segments = 28;
	const float Start = -UE_HALF_PI;
	const float Sweep = FMath::DegreesToRadians(300.f);
	FVector2D Prev = C + FVector2D(FMath::Cos(Start), FMath::Sin(Start)) * Radius;
	float Angle = Start;
	for (int32 i = 1; i <= Segments; ++i)
	{
		Angle = Start + Sign * Sweep * i / Segments;
		const FVector2D Next = C + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius;
		Line(Prev, Next);
		Prev = Next;
	}
	Arrowhead(Prev, FVector2D(-FMath::Sin(Angle), FMath::Cos(Angle)) * Sign);
}

// ======================================================================== full-screen effects

void AGBHUD::DrawDamageFlash(EGBHudSkin Skin)
{
	const float Elapsed = (float)(GetRealTime() - LastDamageTime);
	if (Elapsed < 0.f || Elapsed >= GBHudLayout::DamageFlashTime)
	{
		return;
	}
	const float Strength = 1.f - Elapsed / GBHudLayout::DamageFlashTime;
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	if (Skin == EGBHudSkin::Cyberpunk)
	{
		const float S = UIScale();
		const FLinearColor Edge = GBAlpha(GBCy::Red, 0.45f * Strength);
		DrawFade(0.f, 0.f, W * 0.2f, H, Edge, true);
		DrawFade(W * 0.8f, 0.f, W * 0.2f, H, Edge, false);
		// A few torn scan bands: the "signal hit" look.
		const int32 Seed = FMath::FloorToInt((float)GetRealTime() * 30.f);
		for (int32 i = 0; i < 4; ++i)
		{
			const float R1 = FMath::Frac(FMath::Sin((Seed + i * 17) * 12.9898f) * 43758.5453f);
			const float R2 = FMath::Frac(FMath::Sin((Seed + i * 31) * 78.233f) * 43758.5453f);
			const float BY = R1 * H;
			const float BH = (2.f + R2 * 8.f) * S;
			FillRect(0.f, BY, W, BH, GBAlpha(i % 2 ? GBCy::Cyan : GBCy::Red, 0.12f * Strength));
		}
		return;
	}
	// RPG: a stepped blood vignette in whole art pixels.
	const float P = ArtPixel();
	constexpr int32 FlashLayers = 5;
	for (int32 k = 0; k < FlashLayers; ++k)
	{
		const float T = 3.f * P;
		const float Inset = k * T;
		const FLinearColor Color = GBAlpha(GBRpg::Blood, 0.42f * Strength * (1.f - (float)k / FlashLayers));
		FillRect(Inset, Inset, W - 2.f * Inset, T, Color);
		FillRect(Inset, H - Inset - T, W - 2.f * Inset, T, Color);
		FillRect(Inset, Inset + T, T, H - 2.f * (Inset + T), Color);
		FillRect(W - Inset - T, Inset + T, T, H - 2.f * (Inset + T), Color);
	}
}

void AGBHUD::DrawCutscene(EGBHudSkin Skin)
{
	const UGBCutsceneSubsystem* Cutscene = UGBCutsceneSubsystem::Get(this);
	if (!Cutscene || !Cutscene->IsPlaying())
	{
		return;
	}
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;

	// Dim the world; the dimming comes in over the first moments.
	const float DimIn = FMath::Clamp(Cutscene->GetTotalElapsed() / 0.4f, 0.f, 1.f);
	FillRect(0.f, 0.f, W, H, FLinearColor(0.f, 0.f, 0.02f, 0.72f * DimIn));

	// The line fades in and out around its own time.
	const float LineSeconds = Cutscene->GetLineSeconds();
	const float LineT = Cutscene->GetLineElapsed();
	const float EdgeFade = FMath::Min(0.3f, LineSeconds * 0.4f);
	const float Alpha = EdgeFade > 0.f ? FMath::Clamp(FMath::Min(LineT, LineSeconds - LineT) / EdgeFade, 0.f, 1.f) : 1.f;
	const FString& Text = Cutscene->GetLineText();

	const EGBTextSize Size = EGBTextSize::Body;
	const float LineH = UiLineHeight(Skin, Size);
	const float PanelW = FMath::Min(W * 0.7f, 1100.f * FMath::Max(1.f, H / 1080.f));
	const float Pad = Skin == EGBHudSkin::Cyberpunk ? 28.f * UIScale() : 8.f * ArtPixel();
	// Room for three wrapped lines: the longest caption of the data fits in two at 720p.
	const float PanelH = LineH * 3.f + Pad * 2.f;
	const float PanelX = FMath::RoundToFloat((W - PanelW) * 0.5f);
	const float PanelY = FMath::RoundToFloat(H * 0.66f);

	if (Skin == EGBHudSkin::Cyberpunk)
	{
		const float S = UIScale();
		CyBox(PanelX, PanelY, PanelW, PanelH, 14.f * S, GBAlpha(GBCy::Ink, 0.85f * DimIn), GBAlpha(GBCy::RedDim, DimIn), 0x5);
		FillRect(PanelX, PanelY, 4.f * S, PanelH, GBAlpha(GBCy::Red, DimIn));
	}
	else
	{
		const float P = ArtPixel();
		FillRect(PanelX, PanelY, PanelW, PanelH, GBAlpha(GBRpg::StoneDeep, 0.94f * DimIn));
		RpgStone(PanelX + P, PanelY + P, PanelW - 2.f * P, PanelH - 2.f * P, FLinearColor(1.f, 1.f, 1.f, DimIn));
		RpgFrame(PanelX, PanelY, PanelW, PanelH, ERpgFrame::Gold);
	}
	FLinearColor TextColor = ToneColor(Skin, EGBTextTone::Normal);
	TextColor.A *= Alpha;
	UiWrapped(Skin, Size, EGBTextTone::Normal, Text, PanelX + Pad, PanelY + Pad, PanelW - Pad * 2.f, &TextColor);

	if (Cutscene->CanSkip())
	{
		FLinearColor HintColor = ToneColor(Skin, EGBTextTone::Muted);
		HintColor.A *= 0.9f;
		const float Margin = Canvas->ClipY * 0.04f;
		UiText(Skin, EGBTextSize::Small, EGBTextTone::Muted, TEXT("G / Esc: skip"), W - Margin, H - Margin - UiLineHeight(Skin, EGBTextSize::Small), 1.f, &HintColor);
	}
}

void AGBHUD::DrawEraFade()
{
	const UGBEraSubsystem* Era = UGBEraSubsystem::Get(this);
	const float Alpha = Era ? FMath::Clamp(Era->GetFadeAlpha(), 0.f, 1.f) : 0.f;
	if (Alpha <= 0.f)
	{
		return;
	}
	FillRect(0.f, 0.f, Canvas->ClipX, Canvas->ClipY, FLinearColor(0.82f, 0.9f, 1.f, Alpha));
}
