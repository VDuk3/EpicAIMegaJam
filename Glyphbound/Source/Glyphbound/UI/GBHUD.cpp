#include "UI/GBHUD.h"

#include "UI/GBHUDShared.h"
#include "UI/GBPixelFont.h"
#include "UI/GBPixelSpriteLibrary.h"
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
#include "World/GBInteractable.h"
#include "Data/GBDataSubsystem.h"
#include "Core/GBStatNames.h"
#include "Core/GBPlaceholder.h"

#include "Camera/PlayerCameraManager.h"
#include "Engine/Canvas.h"
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
	/** The pale "health lost" trail behind the health bar drains at this fraction per second. */
	constexpr float TrailDrainPerSecond = 0.45f;
	/** ...after holding still for this long. */
	constexpr float TrailHoldTime = 0.35f;
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

void AGBHUD::DrawHands(AGBPlayerCharacter* Player)
{
	if (!Player->IsControlEnabled() || Player->IsDead())
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
	const float H = Canvas->ClipY - (bOverBar ? 22.f * ArtPixel() : 0.f);
	const float P = FMath::Max(1.f, (float)FMath::RoundToInt(Canvas->ClipY / GBHudLayout::HandPixelDivisor));

	// Walk bob from the horizontal speed (world time, so it slows down with the QTE).
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
	const float Phase = Time * 9.f;
	const FVector2D Bob(FMath::Cos(Phase) * 3.f * P * Speed01, FMath::Abs(FMath::Sin(Phase)) * 2.f * P * Speed01);

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
	if (Item && Slot == EGBHeldSlot::Staff)
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
		const FVector2D StaffSize = SizeOf(Staff);
		// The basic attack thrusts the staff up a little.
		const float CenterX = W * 0.72f + Bob.X;
		const float StaffY = H - StaffSize.Y * 0.8f + Bob.Y - Kick01 * 5.f * P;
		DrawSprite(Staff, CenterX - StaffSize.X * 0.5f, StaffY, P);
		DrawSprite(HandRight, CenterX - RightSize.X * 0.5f, StaffY + StaffSize.Y * 0.45f, P);
	}
	else if (Item && Slot == EGBHeldSlot::Gun)
	{
		// The gun is a 3D model drawn by the world; only the hands are drawn here. Recoil pushes them down.
		const float KickY = Kick01 * 3.f * P;
		// Reload (both guns): the right hand follows the gun's pose, the left hand goes down with the old magazine and
		// comes back up with the new one. Driven by the same progress as the model, so a pinned preview shows too.
		const AGBGun* GunItem = Cast<AGBGun>(Item);
		const bool bModelGun = GunItem && GunItem->HasViewModel();
		const GBReloadAnim::FHandBeat Beat = bModelGun ? GunItem->GetReloadHandBeat() : GBReloadAnim::FHandBeat();
		const float RightFollow = bModelGun ? GunItem->GetReloadPoseAmount() : 0.f;
		const float PushY = -Beat.Push * GBHudLayout::ReloadSeatPush * P;
		const FVector2D RightShift = (GunItem ? GunItem->GetReloadRightHandFollow() : FVector2D::ZeroVector) * (RightFollow * P);
		DrawSprite(HandRight, W * 0.66f - RightSize.X * 0.5f + Bob.X + RightShift.X, H - RightSize.Y * 0.7f + Bob.Y + KickY + RightShift.Y + PushY, P);
		if (!bCastPose)
		{
			const FVector2D Reach = (GunItem ? GunItem->GetReloadHandReach() : FVector2D::ZeroVector) * (Beat.Reach * P);
			const float DropY = Beat.Drop * (LeftSize.Y + FMath::Abs(Reach.Y));
			// A gun with a pump: the left hand rides the forend back and forward with it.
			const FVector2D PumpShift = (bModelGun ? GunItem->GetPumpHandShift() : FVector2D::ZeroVector) * P;
			DrawSprite(HandLeft, W * 0.54f - LeftSize.X * 0.5f + Bob.X + Reach.X + PumpShift.X, H - LeftSize.Y * 0.6f + Bob.Y + KickY + Reach.Y + DropY + PushY + PumpShift.Y, P);
		}
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
		const float HandY = H - RightSize.Y * 0.75f + Bob.Y;
		DrawSprite(Blade, CenterX - BladeSize.X * 0.5f, HandY - BladeSize.Y + 4.f * P, P, Tint);
		DrawSprite(HandRight, CenterX - RightSize.X * 0.5f, HandY, P);
	}
	else
	{
		DrawSprite(HandRight, W * 0.72f - RightSize.X * 0.5f + Bob.X, H - RightSize.Y * 0.45f + Bob.Y, P);
	}

	// The summoned shield is held by the left hand: while it is up (or sliding) the left hand is hidden.
	const UGBSpellbookComponent* Spellbook = Player->GetSpellbook();
	const bool bBlockRaised = Spellbook && Spellbook->HasShield() && Spellbook->IsBlockRaised();
	const double Now = GetRealTime();
	const float Dt = LastShieldUpdateTime < 0.0 ? 0.f : FMath::Clamp((float)(Now - LastShieldUpdateTime), 0.f, 0.1f);
	LastShieldUpdateTime = Now;
	const float Step = Dt / GBHudLayout::ShieldSlideTime;
	ShieldRaise01 = bBlockRaised ? FMath::Min(1.f, ShieldRaise01 + Step) : FMath::Max(0.f, ShieldRaise01 - Step);

	if (bCastPose && ShieldRaise01 < 0.5f)
	{
		// The left hand draws the glyph: over the gun with a gun, raised at the left otherwise.
		const float CastX = (Item && Slot == EGBHeldSlot::Gun) ? W * 0.56f : W * 0.30f;
		DrawSprite(HandCast, CastX - CastSize.X * 0.5f + Bob.X * 0.5f, H - CastSize.Y * 1.1f + Bob.Y * 0.5f, P);
	}
	else if (!bCastPose && !(Item && Slot == EGBHeldSlot::Gun) && ShieldRaise01 <= 0.f)
	{
		DrawSprite(HandLeft, W * 0.22f - LeftSize.X * 0.5f + Bob.X, H - LeftSize.Y * 0.45f + Bob.Y, P);
	}
	DrawRaisedShield(Player, P, Bob);
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
		// Center dot and four ticks at the current spread cone (degrees -> pixels with the camera FOV), with a dark keyline.
		const float T = FMath::Max(1.f, FMath::RoundToFloat(2.f * S));
		const FLinearColor Ink = GBCy::Pale;
		const FLinearColor Key(0.f, 0.f, 0.f, 0.55f);
		float Offset = 6.f * S;
		if (bGun)
		{
			const AGBGun* Gun = Cast<AGBGun>(Item);
			const float SpreadDeg = Gun ? Gun->GetCurrentSpread() : Item->GetStat(Item->IsAiming() ? GBStat::AimSpread : GBStat::Spread, 0.f);
			const float FOV = (PlayerOwner && PlayerOwner->PlayerCameraManager) ? PlayerOwner->PlayerCameraManager->GetFOVAngle() : 90.f;
			const float HalfFOVTan = FMath::Tan(FMath::DegreesToRadians(FMath::Clamp(FOV, 5.f, 170.f) * 0.5f));
			Offset = FMath::Max(FMath::Tan(FMath::DegreesToRadians(FMath::Clamp(SpreadDeg, 0.f, 80.f))) / HalfFOVTan * Canvas->ClipX * 0.5f, 6.f * S);
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
	const float Limit = Session.Config.TimeLimit;
	const float TimeFrac = Limit > 0.f ? FMath::Clamp(Session.GetTimeRemaining() / Limit, 0.f, 1.f) : 0.f;
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
		DrawCyText(FString::Printf(TEXT("%05.2f"), Session.GetTimeRemaining()), CX + BarW * 0.5f + 12.f * S, TimeY - 4.f * S, T, 0.f);
	}
	else
	{
		const int32 TitleScale = FMath::Max(3, FMath::RoundToInt(5.f * S));
		const FBox2D Plaque = RpgPlaque(Title.ToUpper(), CX, FMath::RoundToFloat(H * 0.08f), TitleScale);
		TimeY = FMath::RoundToFloat(Plaque.Max.Y + 5.f * P);
		RpgTube(FMath::RoundToFloat((CX - BarW * 0.5f) / P) * P, TimeY, FMath::RoundToFloat(BarW / P) * P, 6.f * P, TimeFrac, TimeFrac > 0.3f ? GBRpg::StaminaRamp : GBRpg::HealthRamp);
	}

	// Part 1: the key sequence (power).
	const int32 NumKeys = Session.Sequence.Num();
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
