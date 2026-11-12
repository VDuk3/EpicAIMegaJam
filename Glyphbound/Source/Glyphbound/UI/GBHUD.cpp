#include "UI/GBHUD.h"

#include "UI/GBHUDShared.h"
#include "UI/GBPixelSpriteLibrary.h"
#include "Player/GBPlayerCharacter.h"
#include "Weapons/GBEquipmentComponent.h"
#include "Weapons/GBHeldItem.h"
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
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"

// Presentation-only timings and sizes (not gameplay numbers).
namespace GBHudLayout
{
	constexpr float DamageFlashTime = 0.3f;
	constexpr float CastHandTime = 0.45f;
	constexpr float AttackKickTime = 0.15f;
	constexpr float ToastFadeTime = 0.5f;
	constexpr int32 MaxToasts = 5;
	/** Canvas height / this = size of one sprite pixel for the hands (9 at 1080p, 6 at 720p). */
	constexpr float HandPixelDivisor = 120.f;
	/** The raised shield's sprite pixels are this many times the hands' pixels. */
	constexpr float ShieldPixelMult = 2.f;
	/** Seconds of the raise / lower slide of the shield (visual only; blocking follows the input at once). */
	constexpr float ShieldSlideTime = 0.12f;
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
}

namespace GBHudLocal
{
	FGBHudSkinStyle MakeRpgStyle()
	{
		FGBHudSkinStyle Style;
		Style.Panel = FLinearColor(0.60f, 0.45f, 0.26f, 0.95f);
		Style.Border = FLinearColor(0.06f, 0.03f, 0.01f, 1.f);
		Style.Text = FLinearColor(0.04f, 0.02f, 0.01f, 1.f);
		Style.Health = FLinearColor(0.75f, 0.06f, 0.04f, 1.f);
		Style.Mana = FLinearColor(0.10f, 0.22f, 0.85f, 1.f);
		Style.Accent = FLinearColor(1.f, 0.72f, 0.18f, 1.f);
		Style.bChunky = true;
		return Style;
	}

	FGBHudSkinStyle MakeCyberStyle()
	{
		FGBHudSkinStyle Style;
		Style.Panel = FLinearColor(0.f, 0.01f, 0.03f, 0.6f);
		Style.Border = FLinearColor(0.f, 0.85f, 1.f, 1.f);
		Style.Text = FLinearColor(0.75f, 1.f, 1.f, 1.f);
		Style.Health = FLinearColor(1.f, 0.05f, 0.4f, 1.f);
		Style.Mana = FLinearColor(0.05f, 0.75f, 1.f, 1.f);
		Style.Accent = FLinearColor(1.f, 0.1f, 0.75f, 1.f);
		Style.bChunky = false;
		return Style;
	}

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
	const FLinearColor Dark(0.f, 0.f, 0.f, 0.6f);
	const FLinearColor Good(0.1f, 0.75f, 0.2f, 0.9f);
	const FLinearColor Bad(0.85f, 0.08f, 0.06f, 0.9f);
}

// ======================================================================== basics

AGBHUD::AGBHUD()
{
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

const FGBHudSkinStyle& AGBHUD::GetSkinStyle(EGBHudSkin Skin)
{
	static const FGBHudSkinStyle Rpg = GBHudLocal::MakeRpgStyle();
	static const FGBHudSkinStyle Cyber = GBHudLocal::MakeCyberStyle();
	return Skin == EGBHudSkin::Cyberpunk ? Cyber : Rpg;
}

double AGBHUD::GetRealTime() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetRealTimeSeconds() : 0.0;
}

float AGBHUD::UIScale() const
{
	return Canvas ? FMath::Max(0.1f, Canvas->ClipY / 1080.f) : 1.f;
}

void AGBHUD::ShowMessage(const FString& Text, float Duration)
{
	if (Text.IsEmpty())
	{
		return;
	}
	const double EndTime = GetRealTime() + FMath::Max(0.1f, Duration);
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
	Toast.EndTime = EndTime;
	Toasts.Add(Toast);
	while (Toasts.Num() > GBHudLayout::MaxToasts)
	{
		Toasts.RemoveAt(0);
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

// ======================================================================== draw helpers

void AGBHUD::DrawLabel(const FString& Text, const FLinearColor& Color, float X, float Y, float SizePx, float AlignX, bool bShadow, bool bLarge)
{
	if (!Canvas || !GEngine || Text.IsEmpty())
	{
		return;
	}
	UFont* Font = bLarge ? GEngine->GetLargeFont() : GEngine->GetMediumFont();
	if (!Font)
	{
		return;
	}
	const float Scale = SizePx * UIScale() / FMath::Max(1.f, Font->GetMaxCharHeight());
	float W = 0.f;
	float H = 0.f;
	GetTextSize(Text, W, H, Font, Scale);
	const float DrawX = FMath::RoundToFloat(X - W * AlignX);
	const float DrawY = FMath::RoundToFloat(Y);
	if (bShadow)
	{
		const float Off = FMath::Max(1.f, 2.f * UIScale());
		DrawText(Text, FLinearColor(0.f, 0.f, 0.f, 0.8f * Color.A), DrawX + Off, DrawY + Off, Font, Scale);
	}
	DrawText(Text, Color, DrawX, DrawY, Font, Scale);
}

FVector2D AGBHUD::MeasureLabel(const FString& Text, float SizePx, bool bLarge) const
{
	if (!Canvas || !GEngine || Text.IsEmpty())
	{
		return FVector2D::ZeroVector;
	}
	UFont* Font = bLarge ? GEngine->GetLargeFont() : GEngine->GetMediumFont();
	if (!Font)
	{
		return FVector2D::ZeroVector;
	}
	const float Scale = SizePx * UIScale() / FMath::Max(1.f, Font->GetMaxCharHeight());
	float W = 0.f;
	float H = 0.f;
	GetTextSize(Text, W, H, Font, Scale);
	return FVector2D(W, H);
}

void AGBHUD::DrawOutline(float X, float Y, float W, float H, const FLinearColor& Color, float Thickness)
{
	const float T = FMath::Max(1.f, Thickness);
	DrawRect(Color, X, Y, W, T);
	DrawRect(Color, X, Y + H - T, W, T);
	DrawRect(Color, X, Y + T, T, H - 2.f * T);
	DrawRect(Color, X + W - T, Y + T, T, H - 2.f * T);
}

void AGBHUD::DrawPanel(const FGBHudSkinStyle& Style, float X, float Y, float W, float H)
{
	const float S = UIScale();
	if (Style.bChunky)
	{
		// 90s RPG: thick dark frame, parchment fill, a light bevel line on top.
		const float B = FMath::Max(2.f, 4.f * S);
		DrawRect(Style.Border, X, Y, W, H);
		DrawRect(Style.Panel, X + B, Y + B, W - 2.f * B, H - 2.f * B);
		DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.2f), X + B, Y + B, W - 2.f * B, FMath::Max(1.f, 2.f * S));
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.25f), X + B, Y + H - B - FMath::Max(1.f, 2.f * S), W - 2.f * B, FMath::Max(1.f, 2.f * S));
	}
	else
	{
		// Cyberpunk: translucent fill, thin neon top line, a short side tick and a magenta corner.
		const float T = FMath::Max(1.f, 1.5f * S);
		DrawRect(Style.Panel, X, Y, W, H);
		DrawRect(Style.Border, X, Y, W, T);
		DrawRect(Style.Border, X, Y, T, H * 0.4f);
		DrawRect(Style.Accent, X + W - 24.f * S, Y + H - 2.f * T, 24.f * S, 2.f * T);
	}
}

void AGBHUD::DrawBar(const FGBHudSkinStyle& Style, float X, float Y, float W, float H, float Fraction, const FLinearColor& Fill, const FString& Label)
{
	const float S = UIScale();
	DrawRect(GBHudLocal::Dark, X, Y, W, H);
	DrawRect(Fill, X, Y, W * FMath::Clamp(Fraction, 0.f, 1.f), H);
	if (Style.bChunky)
	{
		DrawOutline(X, Y, W, H, Style.Border, 2.f * S);
	}
	else
	{
		DrawRect(Style.Border, X, Y + H - FMath::Max(1.f, S), W, FMath::Max(1.f, S));
	}
	if (!Label.IsEmpty())
	{
		const float SizePx = FMath::Clamp(H / S * 0.8f, 10.f, 22.f);
		const FVector2D Size = MeasureLabel(Label, SizePx);
		DrawLabel(Label, GBHudLocal::White, X + W * 0.5f, Y + (H - Size.Y) * 0.5f, SizePx, 0.5f);
	}
}

void AGBHUD::DrawSprite(UTexture2D* Texture, float X, float Y, float PixelScale, const FLinearColor& Tint)
{
	if (!Texture)
	{
		return;
	}
	const float W = Texture->GetSizeX() * PixelScale;
	const float H = Texture->GetSizeY() * PixelScale;
	DrawTexture(Texture, FMath::RoundToFloat(X), FMath::RoundToFloat(Y), W, H, 0.f, 0.f, 1.f, 1.f, Tint, BLEND_Translucent);
}

// ======================================================================== frame

void AGBHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas || Canvas->ClipX <= 0.f || Canvas->ClipY <= 0.f)
	{
		return;
	}

	// Drop expired toasts here, once per frame.
	const double Now = GetRealTime();
	Toasts.RemoveAll([Now](const FToast& Toast) { return Toast.EndTime <= Now; });

	AGBPlayerCharacter* Player = GetPlayer();
	BindToPlayer(Player);
	const FGBHudSkinStyle& Style = GetSkinStyle(GetCurrentSkin());
	const bool bMenuOpen = GetOpenMenu() != EGBMenu::None;
	// The canvas is the viewport divided by the DPI scale; the mouse comes in viewport pixels.
	MouseToCanvas = Canvas->SizeX > 0 ? Canvas->ClipX / (float)Canvas->SizeX : 1.f;

	// World-space numbers first: hands and HUD panels stay on top of them.
	DrawDamageNumbers();
	if (Player)
	{
		DrawHands(Player);
		DrawStatusHud(Player, Style);
		if (!bMenuOpen)
		{
			DrawCrosshair(Player, Style);
			DrawInteractPrompt(Player);
		}
	}
	if (!bMenuOpen)
	{
		DrawToasts();
	}
	if (Player && Player->GetQTE() && Player->GetQTE()->IsActive())
	{
		DrawQTE(Player, Style);
	}
	DrawDamageFlash();
	DrawEraFade();
	if (bMenuOpen)
	{
		UpdateMenuMouse();
		DrawMenus(Player);
		// Menu actions answer with toasts (skill unlocks, combines): keep them readable above the menu.
		DrawToasts();
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
	const float H = Canvas->ClipY;
	const float P = FMath::Max(1.f, (float)FMath::RoundToInt(H / GBHudLayout::HandPixelDivisor));

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
		DrawSprite(HandRight, W * 0.66f - RightSize.X * 0.5f + Bob.X, H - RightSize.Y * 0.7f + Bob.Y + KickY, P);
		if (!bCastPose)
		{
			DrawSprite(HandLeft, W * 0.54f - LeftSize.X * 0.5f + Bob.X, H - LeftSize.Y * 0.6f + Bob.Y + KickY, P);
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

// ======================================================================== status HUD

void AGBHUD::DrawStatusHud(AGBPlayerCharacter* Player, const FGBHudSkinStyle& Style)
{
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	const float S = UIScale();
	const bool bShadow = !Style.bChunky;

	UGBEquipmentComponent* Equipment = Player->GetEquipment();
	AGBHeldItem* Item = Equipment ? Equipment->GetActiveItem() : nullptr;
	const bool bStaff = Item && Item->GetSlot() == EGBHeldSlot::Staff;
	const bool bGun = Item && Item->GetSlot() == EGBHeldSlot::Gun;

	// Right-side info lines (quick-use item, skill points), anchored by their right edge.
	TArray<FString> InfoLines;
	if (const UGBInventoryComponent* Inventory = Player->GetInventory())
	{
		const FName QuickId = Inventory->GetQuickUseItem();
		if (QuickId != NAME_None)
		{
			const UGBDataSubsystem* Data = UGBDataSubsystem::Get(this);
			const FGBItemDefinition* ItemDef = Data ? Data->FindItem(QuickId) : nullptr;
			const FString Name = (ItemDef && !ItemDef->DisplayName.IsEmpty()) ? ItemDef->DisplayName : QuickId.ToString();
			InfoLines.Add(FString::Printf(TEXT("[F] %s x%d"), *Name, Inventory->GetCount(QuickId)));
		}
		else
		{
			InfoLines.Add(TEXT("[F] -"));
		}
	}
	if (const UGBSkillTreeComponent* SkillTree = Player->GetSkillTree())
	{
		if (SkillTree->GetPoints() > 0)
		{
			InfoLines.Add(FString::Printf(TEXT("Skill points: %d [K]"), SkillTree->GetPoints()));
		}
	}

	float InfoX = 0.f;
	float InfoY = 0.f;
	if (Style.bChunky)
	{
		// 90s RPG: one chunky status bar along the bottom.
		const float BarH = 112.f * S;
		const float Top = H - BarH;
		DrawPanel(Style, 0.f, Top, W, BarH);
		DrawVitals(Player, Style, 20.f * S, Top + 14.f * S, 340.f * S);
		if (bStaff)
		{
			DrawSpellSlots(Player, Style, W * 0.5f, Top + 14.f * S);
		}
		else if (bGun)
		{
			DrawGunPanel(Player, Item, Style, W * 0.5f - 230.f * S, Top + 12.f * S, 460.f * S);
		}
		else if (Item)
		{
			DrawLabel(Item->GetDisplayName(), Style.Text, W * 0.5f, Top + 30.f * S, 26.f, 0.5f, bShadow);
			DrawLabel(GBHudText::ElementName(Item->GetElement()), GBHudText::ElementColor(Item->GetElement()), W * 0.5f, Top + 62.f * S, 18.f, 0.5f, true);
		}
		DrawStatusEffects(Player, Style, 20.f * S, Top - 40.f * S, false);
		DrawShieldIndicator(Player, Style, 20.f * S, Top - 90.f * S);
		InfoX = W - 24.f * S;
		InfoY = Top + 18.f * S;
	}
	else
	{
		// Cyberpunk: thin panels in the corners.
		DrawPanel(Style, 24.f * S, H - 124.f * S, 380.f * S, 100.f * S);
		DrawVitals(Player, Style, 40.f * S, H - 110.f * S, 348.f * S);
		if (bStaff)
		{
			DrawSpellSlots(Player, Style, W * 0.5f, H - 100.f * S);
		}
		else if (bGun)
		{
			DrawPanel(Style, W - 404.f * S, H - 154.f * S, 380.f * S, 130.f * S);
			DrawGunPanel(Player, Item, Style, W - 392.f * S, H - 144.f * S, 356.f * S);
		}
		else if (Item)
		{
			DrawLabel(Item->GetDisplayName(), Style.Text, W - 32.f * S, H - 70.f * S, 24.f, 1.f, bShadow);
		}
		DrawStatusEffects(Player, Style, 24.f * S, 24.f * S, true);
		DrawShieldIndicator(Player, Style, 24.f * S, H - 172.f * S);
		InfoX = W - 24.f * S;
		InfoY = 24.f * S;
	}

	for (const FString& Line : InfoLines)
	{
		DrawLabel(Line, Style.Text, InfoX, InfoY, 18.f, 1.f, bShadow);
		InfoY += 26.f * S;
	}
}

void AGBHUD::DrawVitals(AGBPlayerCharacter* Player, const FGBHudSkinStyle& Style, float X, float Y, float W)
{
	const float S = UIScale();
	const bool bShadow = !Style.bChunky;
	const float BarH = (Style.bChunky ? 24.f : 16.f) * S;
	const float Gap = 8.f * S;
	const float LabelW = 44.f * S;
	const float BarW = W - LabelW;

	if (const UGBAttributeComponent* Attributes = Player->GetAttributes())
	{
		const float MaxHealth = Attributes->GetMaxHealth();
		const float MaxMana = Attributes->GetMaxMana();
		const float HealthFrac = MaxHealth > 0.f ? Attributes->GetHealth() / MaxHealth : 0.f;
		const float ManaFrac = MaxMana > 0.f ? Attributes->GetMana() / MaxMana : 0.f;
		DrawLabel(TEXT("HP"), Style.Text, X, Y + BarH * 0.5f - 9.f * S, 18.f, 0.f, bShadow);
		DrawBar(Style, X + LabelW, Y, BarW, BarH, HealthFrac, Style.Health,
			FString::Printf(TEXT("%d / %d"), FMath::CeilToInt(Attributes->GetHealth()), FMath::RoundToInt(MaxHealth)));
		const float ManaY = Y + BarH + Gap;
		DrawLabel(TEXT("MP"), Style.Text, X, ManaY + BarH * 0.5f - 9.f * S, 18.f, 0.f, bShadow);
		DrawBar(Style, X + LabelW, ManaY, BarW, BarH, ManaFrac, Style.Mana,
			FString::Printf(TEXT("%d / %d"), FMath::FloorToInt(Attributes->GetMana()), FMath::RoundToInt(MaxMana)));
	}

	// Dash cooldown: a small bar that fills up; "READY" when full.
	const float DashY = Y + 2.f * (BarH + Gap);
	const float Remaining = Player->GetDashCooldownRemaining();
	const UGBStatsComponent* Stats = Player->GetStats();
	const float Total = Stats ? Stats->GetStat(GBStat::DashCooldown, 0.f) : 0.f;
	const float DashFrac = (Remaining <= 0.f || Total <= 0.f) ? 1.f : 1.f - Remaining / Total;
	DrawLabel(TEXT("DASH"), Style.Text, X, DashY - 3.f * S, 14.f, 0.f, bShadow);
	DrawBar(Style, X + LabelW, DashY, BarW * 0.5f, 8.f * S, DashFrac, DashFrac >= 1.f ? Style.Accent : FLinearColor(0.4f, 0.4f, 0.4f, 1.f), FString());
}

void AGBHUD::DrawShieldIndicator(AGBPlayerCharacter* Player, const FGBHudSkinStyle& Style, float X, float Y)
{
	const UGBSpellbookComponent* Spellbook = Player->GetSpellbook();
	if (!Spellbook || !Spellbook->HasShield())
	{
		return;
	}
	const float S = UIScale();
	const bool bShadow = !Style.bChunky;
	const float PW = 340.f * S;
	const float PH = 40.f * S;
	const FLinearColor Ice = GBHudText::ElementColor(EGBElement::Ice);
	const bool bRaised = Spellbook->IsBlockRaised();
	DrawPanel(Style, X, Y, PW, PH);
	DrawLabel(TEXT("SHIELD"), bRaised ? Ice : Style.Text, X + 10.f * S, Y + 11.f * S, 16.f, 0.f, bShadow);
	const float BarX = X + 86.f * S;
	const float BarW = 140.f * S;
	DrawBar(Style, BarX, Y + 11.f * S, BarW, 18.f * S, Spellbook->GetShieldHealthFraction(), Ice,
		FString::Printf(TEXT("%.1fs"), FMath::Max(0.f, Spellbook->GetShieldTimeRemaining())));
	DrawLabel(bRaised ? TEXT("BLOCKING") : TEXT("[Q] Block"), bRaised ? Ice : Style.Accent, X + PW - 10.f * S, Y + 11.f * S, 16.f, 1.f, true);
}

void AGBHUD::DrawSpellSlots(AGBPlayerCharacter* Player, const FGBHudSkinStyle& Style, float CenterX, float Y)
{
	UGBSpellbookComponent* Spellbook = Player->GetSpellbook();
	if (!Spellbook)
	{
		return;
	}
	const UGBDataSubsystem* Data = UGBDataSubsystem::Get(this);
	const float S = UIScale();
	const float Box = (Style.bChunky ? 84.f : 66.f) * S;
	const float Gap = 10.f * S;
	const int32 Num = UGBSpellbookComponent::NumSlots;
	const float X0 = CenterX - (Num * Box + (Num - 1) * Gap) * 0.5f;
	static const TCHAR* Keys[] = { TEXT("RMB"), TEXT("1"), TEXT("2"), TEXT("3"), TEXT("4") };

	for (int32 Slot = 0; Slot < Num; ++Slot)
	{
		const float X = X0 + Slot * (Box + Gap);
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.65f), X, Y, Box, Box);
		DrawOutline(X, Y, Box, Box, Style.bChunky ? Style.Border : Style.Border * FLinearColor(1.f, 1.f, 1.f, 0.7f), 2.f * S);

		const FName SpellId = Spellbook->GetSlotSpell(Slot);
		const FGBSpellDefinition* Def = (Data && SpellId != NAME_None) ? Data->FindSpell(SpellId) : nullptr;
		if (Def)
		{
			DrawRect(GBHudText::ElementColor(Def->Element), X + 2.f * S, Y + 2.f * S, Box - 4.f * S, 5.f * S);
			const FString Name = GBHudText::Abbreviate(Def->DisplayName.IsEmpty() ? SpellId.ToString() : Def->DisplayName, 8);
			DrawLabel(Name, GBHudLocal::White, X + Box * 0.5f, Y + Box * 0.36f, 15.f, 0.5f);
			DrawLabel(FString::Printf(TEXT("%d"), FMath::RoundToInt(Spellbook->GetManaCost(SpellId))), Style.Mana + FLinearColor(0.3f, 0.3f, 0.3f, 0.f),
				X + Box - 5.f * S, Y + Box - 20.f * S, 14.f, 1.f);

			const EGBCastFailReason Reason = Spellbook->CanCast(SpellId);
			const float Remaining = Spellbook->GetCooldownRemaining(SpellId);
			if (Remaining > 0.f)
			{
				const float Duration = Spellbook->GetCooldownDuration(SpellId);
				const float Frac = Duration > 0.f ? FMath::Clamp(Remaining / Duration, 0.f, 1.f) : 1.f;
				DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.65f), X, Y + Box * (1.f - Frac), Box, Box * Frac);
				DrawLabel(FString::Printf(TEXT("%.1f"), Remaining), GBHudLocal::White, X + Box * 0.5f, Y + Box * 0.5f - 12.f * S, 22.f, 0.5f);
			}
			else if (Reason != EGBCastFailReason::None && Reason != EGBCastFailReason::OnCooldown)
			{
				DrawRect(FLinearColor(0.25f, 0.25f, 0.25f, 0.6f), X, Y, Box, Box);
			}
		}
		else
		{
			DrawLabel(TEXT("-"), FLinearColor(1.f, 1.f, 1.f, 0.4f), X + Box * 0.5f, Y + Box * 0.35f, 18.f, 0.5f);
		}
		DrawLabel(Keys[Slot], Style.Accent, X + 5.f * S, Y + 8.f * S, 13.f, 0.f);
	}
}

void AGBHUD::DrawGunPanel(AGBPlayerCharacter* Player, AGBHeldItem* Gun, const FGBHudSkinStyle& Style, float X, float Y, float W)
{
	const float S = UIScale();
	const bool bShadow = !Style.bChunky;

	DrawLabel(Gun->GetDisplayName(), Style.Text, X, Y, 18.f, 0.f, bShadow);
	const FGBAmmoState Ammo = Gun->GetAmmo();
	const int32 MagSize = FMath::RoundToInt(Gun->GetStat(GBStat::MagazineSize, 0.f));
	const FString AmmoText = MagSize > 0 ? FString::Printf(TEXT("%d / %d"), Ammo.InMagazine, MagSize) : FString::Printf(TEXT("%d"), Ammo.InMagazine);
	const FLinearColor AmmoColor = Ammo.InMagazine > 0 ? Style.Text : GBHudLocal::Bad;
	DrawLabel(AmmoText, AmmoColor, X, Y + 22.f * S, 32.f, 0.f, bShadow);
	const FVector2D AmmoSize = MeasureLabel(AmmoText, 32.f);
	DrawLabel(FString::Printf(TEXT("x%d mags"), Ammo.SpareMagazines), Style.Text, X + AmmoSize.X + 12.f * S, Y + 34.f * S, 16.f, 0.f, bShadow);

	if (Gun->IsReloading())
	{
		DrawBar(Style, X, Y + 64.f * S, W * 0.5f, 10.f * S, Gun->GetReloadProgress(), Style.Accent, FString());
		DrawLabel(TEXT("RELOADING"), Style.Text, X + W * 0.5f + 8.f * S, Y + 60.f * S, 14.f, 0.f, bShadow);
	}
	else if (Gun->UsesAmmo() && Ammo.InMagazine <= 0)
	{
		DrawLabel(Ammo.SpareMagazines > 0 ? TEXT("[R] Reload") : TEXT("No ammo"), GBHudLocal::Bad, X, Y + 60.f * S, 16.f, 0.f, true);
	}

	// Enchant slot (T): element, cost, cooldown; greyed when it cannot be cast.
	UGBSpellbookComponent* Spellbook = Player->GetSpellbook();
	if (!Spellbook)
	{
		return;
	}
	const FName EnchantId = Spellbook->GetEnchantSpell();
	const EGBElement Element = Spellbook->GetEnchantElement();
	const FLinearColor ElementColor = GBHudText::ElementColor(Element);
	const float Box = 64.f * S;
	const float BX = X + W - Box;
	const float BY = Y;
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.65f), BX, BY, Box, Box);
	DrawOutline(BX, BY, Box, Box, ElementColor, 2.f * S);
	DrawLabel(GBHudText::Abbreviate(GBHudText::ElementName(Element), 6), ElementColor, BX + Box * 0.5f, BY + Box * 0.38f, 15.f, 0.5f);
	if (EnchantId != NAME_None)
	{
		DrawLabel(FString::Printf(TEXT("%d"), FMath::RoundToInt(Spellbook->GetManaCost(EnchantId))), Style.Mana + FLinearColor(0.3f, 0.3f, 0.3f, 0.f),
			BX + Box - 5.f * S, BY + Box - 20.f * S, 14.f, 1.f);
		const float Remaining = Spellbook->GetCooldownRemaining(EnchantId);
		const EGBCastFailReason Reason = Spellbook->CanCast(EnchantId);
		if (Remaining > 0.f)
		{
			const float Duration = Spellbook->GetCooldownDuration(EnchantId);
			const float Frac = Duration > 0.f ? FMath::Clamp(Remaining / Duration, 0.f, 1.f) : 1.f;
			DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.65f), BX, BY + Box * (1.f - Frac), Box, Box * Frac);
			DrawLabel(FString::Printf(TEXT("%.1f"), Remaining), GBHudLocal::White, BX + Box * 0.5f, BY + Box * 0.5f - 12.f * S, 22.f, 0.5f);
		}
		else if (Reason != EGBCastFailReason::None && Reason != EGBCastFailReason::OnCooldown)
		{
			DrawRect(FLinearColor(0.25f, 0.25f, 0.25f, 0.6f), BX, BY, Box, Box);
		}
	}
	DrawLabel(TEXT("T"), Style.Accent, BX + 5.f * S, BY + 6.f * S, 14.f, 0.f);

	// "ENCHANTED" while an Enchant_* status effect runs.
	if (const UGBStatusEffectComponent* Effects = Player->GetStatusEffects())
	{
		for (const FGBActiveStatusEffect& Effect : Effects->GetActiveEffects())
		{
			if (Effect.EffectId.ToString().StartsWith(TEXT("Enchant_")))
			{
				DrawLabel(FString::Printf(TEXT("ENCHANTED %.1fs"), Effect.TimeRemaining), ElementColor, BX + Box, BY + Box + 6.f * S, 16.f, 1.f, true);
				break;
			}
		}
	}
}

void AGBHUD::DrawStatusEffects(AGBPlayerCharacter* Player, const FGBHudSkinStyle& Style, float X, float Y, bool bVertical)
{
	const UGBStatusEffectComponent* Effects = Player->GetStatusEffects();
	if (!Effects)
	{
		return;
	}
	const UGBDataSubsystem* Data = UGBDataSubsystem::Get(this);
	const float S = UIScale();
	const float EntryW = 170.f * S;
	const float EntryH = 28.f * S;
	for (const FGBActiveStatusEffect& Effect : Effects->GetActiveEffects())
	{
		const FGBStatusEffectDefinition* Def = Data ? Data->FindStatusEffect(Effect.EffectId) : nullptr;
		FString Name = (Def && !Def->DisplayName.IsEmpty()) ? Def->DisplayName : Effect.EffectId.ToString();
		if (Effect.Stacks > 1)
		{
			Name += FString::Printf(TEXT(" x%d"), Effect.Stacks);
		}
		const FString Text = Effect.TotalDuration > 0.f ? FString::Printf(TEXT("%s %.1fs"), *Name, Effect.TimeRemaining) : Name;
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.6f), X, Y, EntryW, EntryH);
		if (Effect.TotalDuration > 0.f)
		{
			DrawRect(Style.Accent, X, Y + EntryH - 3.f * S, EntryW * FMath::Clamp(Effect.TimeRemaining / Effect.TotalDuration, 0.f, 1.f), 3.f * S);
		}
		DrawLabel(GBHudText::Abbreviate(Text, 20), GBHudLocal::White, X + 6.f * S, Y + 4.f * S, 15.f, 0.f);
		if (bVertical)
		{
			Y += EntryH + 6.f * S;
		}
		else
		{
			X += EntryW + 8.f * S;
		}
	}
}

// ======================================================================== crosshair, prompt, toasts

void AGBHUD::DrawCrosshair(AGBPlayerCharacter* Player, const FGBHudSkinStyle& Style)
{
	if (!Player->IsControlEnabled() || Player->IsDead())
	{
		return;
	}
	const float S = UIScale();
	const float CX = Canvas->ClipX * 0.5f;
	const float CY = Canvas->ClipY * 0.5f;
	const FLinearColor Color = Style.bChunky ? Style.Accent : Style.Border;
	const float T = FMath::Max(1.f, 2.f * S);

	const UGBEquipmentComponent* Equipment = Player->GetEquipment();
	AGBHeldItem* Item = Equipment ? Equipment->GetActiveItem() : nullptr;
	if (Item && Item->GetSlot() == EGBHeldSlot::Gun)
	{
		// Dot + four ticks at the current spread cone (degrees -> pixels with the camera FOV).
		DrawRect(Color, CX - T, CY - T, 2.f * T, 2.f * T);
		const float SpreadDeg = Item->GetStat(Item->IsAiming() ? GBStat::AimSpread : GBStat::Spread, 0.f);
		const float FOV = (PlayerOwner && PlayerOwner->PlayerCameraManager) ? PlayerOwner->PlayerCameraManager->GetFOVAngle() : 90.f;
		const float HalfFOVTan = FMath::Tan(FMath::DegreesToRadians(FMath::Clamp(FOV, 5.f, 170.f) * 0.5f));
		float Offset = FMath::Tan(FMath::DegreesToRadians(FMath::Clamp(SpreadDeg, 0.f, 80.f))) / HalfFOVTan * Canvas->ClipX * 0.5f;
		Offset = FMath::Max(Offset, 6.f * S);
		const float L = 8.f * S;
		DrawRect(Color, CX - Offset - L, CY - T * 0.5f, L, T);
		DrawRect(Color, CX + Offset, CY - T * 0.5f, L, T);
		DrawRect(Color, CX - T * 0.5f, CY - Offset - L, T, L);
		DrawRect(Color, CX - T * 0.5f, CY + Offset, T, L);
	}
	else if (Item && Item->GetSlot() == EGBHeldSlot::Sword)
	{
		DrawRect(Color, CX - T, CY - T, 2.f * T, 2.f * T);
	}
	else
	{
		const float Gap = 3.f * S;
		const float L = 8.f * S;
		DrawRect(Color, CX - Gap - L, CY - T * 0.5f, L, T);
		DrawRect(Color, CX + Gap, CY - T * 0.5f, L, T);
		DrawRect(Color, CX - T * 0.5f, CY - Gap - L, T, L);
		DrawRect(Color, CX - T * 0.5f, CY + Gap, T, L);
	}
}

void AGBHUD::DrawInteractPrompt(AGBPlayerCharacter* Player)
{
	IGBInteractable* Interactable = Player->GetFocusedInteractable();
	if (!Interactable)
	{
		return;
	}
	const FString Text = FString::Printf(TEXT("[G] %s"), *Interactable->GetInteractPrompt(Player));
	const float S = UIScale();
	const FVector2D Size = MeasureLabel(Text, 22.f);
	const float X = Canvas->ClipX * 0.5f;
	const float Y = Canvas->ClipY * 0.6f;
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.5f), X - Size.X * 0.5f - 10.f * S, Y - 4.f * S, Size.X + 20.f * S, Size.Y + 8.f * S);
	DrawLabel(Text, GBHudLocal::White, X, Y, 22.f, 0.5f);
}

void AGBHUD::DrawToasts()
{
	const double Now = GetRealTime();
	const float S = UIScale();
	const float X = Canvas->ClipX * 0.5f;
	float Y = Canvas->ClipY * 0.14f;
	for (const FToast& Toast : Toasts)
	{
		const float Alpha = FMath::Clamp((float)(Toast.EndTime - Now) / GBHudLayout::ToastFadeTime, 0.f, 1.f);
		const FVector2D Size = MeasureLabel(Toast.Text, 22.f);
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.5f * Alpha), X - Size.X * 0.5f - 12.f * S, Y - 4.f * S, Size.X + 24.f * S, Size.Y + 8.f * S);
		DrawLabel(Toast.Text, FLinearColor(1.f, 1.f, 1.f, Alpha), X, Y, 22.f, 0.5f);
		Y += Size.Y + 14.f * S;
	}
}

// ======================================================================== QTE (GDD 7)

void AGBHUD::DrawQTE(AGBPlayerCharacter* Player, const FGBHudSkinStyle& Style)
{
	const UGBQTEComponent* QTE = Player->GetQTE();
	const FGBQTESession& Session = QTE->GetSession();
	const float S = UIScale();
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	const float CX = W * 0.5f;

	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.3f), 0.f, 0.f, W, H);
	DrawLabel(GBHudText::SpellName(this, QTE->GetSpellId()), GBHudLocal::White, CX, H * 0.1f, 34.f, 0.5f, true, true);

	// Time bar for the whole QTE.
	const float Limit = Session.Config.TimeLimit;
	const float TimeFrac = Limit > 0.f ? Session.GetTimeRemaining() / Limit : 0.f;
	const float BarW = 600.f * S;
	DrawBar(Style, CX - BarW * 0.5f, H * 0.17f, BarW, 12.f * S, TimeFrac, TimeFrac > 0.3f ? Style.Accent : GBHudLocal::Bad, FString());

	// Part 1: the key sequence (power).
	const int32 NumKeys = Session.Sequence.Num();
	const int32 Done = Session.SequenceResults.Num();
	const float Box = 56.f * S;
	const float Gap = 12.f * S;
	const float RowY = H * 0.24f;
	const float X0 = CX - (NumKeys * Box + FMath::Max(0, NumKeys - 1) * Gap) * 0.5f;
	int32 RightKeys = 0;
	for (int32 i = 0; i < NumKeys; ++i)
	{
		const bool bConsumed = i < Done;
		const bool bCurrent = Session.Phase == EGBQTEPhase::Sequence && i == Done;
		if (bConsumed && Session.SequenceResults[i])
		{
			++RightKeys;
		}
		const float Grow = bCurrent ? 6.f * S : 0.f;
		const float X = X0 + i * (Box + Gap) - Grow;
		const float Y = RowY - Grow;
		const float Size = Box + 2.f * Grow;
		const FLinearColor Fill = bConsumed ? (Session.SequenceResults[i] ? GBHudLocal::Good : GBHudLocal::Bad) : FLinearColor(0.f, 0.f, 0.f, 0.65f);
		DrawRect(Fill, X, Y, Size, Size);
		DrawOutline(X, Y, Size, Size, bCurrent ? Style.Accent : FLinearColor(1.f, 1.f, 1.f, 0.5f), (bCurrent ? 3.f : 1.5f) * S);
		DrawLabel(GBHudLocal::QTEKeyLetter(Session.Sequence[i]), GBHudLocal::White, X + Size * 0.5f, Y + Size * 0.5f - 15.f * S, 28.f, 0.5f);
	}
	if (Session.Phase == EGBQTEPhase::Sequence)
	{
		DrawLabel(TEXT("Press the keys in order"), GBHudLocal::White, CX, RowY + Box + 14.f * S, 18.f, 0.5f);
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
		FString Title = GBHudLocal::GestureText(Gesture);
		if (NumGestures > 1)
		{
			Title += FString::Printf(TEXT(" (%d/%d)"), Judged + 1, NumGestures);
		}
		DrawLabel(Title, GBHudLocal::White, CX, H * 0.34f, 24.f, 0.5f);
		const float CY = H * 0.52f;
		DrawGestureHint((uint8)Gesture, CX, CY, 110.f * S, FLinearColor(1.f, 1.f, 1.f, 0.35f));

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
			for (int32 i = 1; i < Stroke.Num(); ++i)
			{
				const FVector2D A = (Stroke[i - 1] - Center) * Fit;
				const FVector2D B = (Stroke[i] - Center) * Fit;
				DrawLine(CX + A.X, CY + A.Y, CX + B.X, CY + B.Y, Style.Accent, 3.f * S);
			}
		}
	}

	// Running scores.
	const float Power = NumKeys > 0 ? (float)RightKeys / NumKeys : 0.f;
	const float Reach = NumGestures > 0 ? ScoreSum / NumGestures : 0.f;
	DrawLabel(FString::Printf(TEXT("POWER %d%%"), FMath::RoundToInt(Power * 100.f)), GBHudLocal::White, CX - 220.f * S, H * 0.72f, 24.f, 0.5f, true, true);
	DrawLabel(FString::Printf(TEXT("REACH %d%%"), FMath::RoundToInt(Reach * 100.f)), GBHudLocal::White, CX + 220.f * S, H * 0.72f, 24.f, 0.5f, true, true);
}

void AGBHUD::DrawGestureHint(uint8 GestureByte, float CenterX, float CenterY, float Radius, const FLinearColor& Color)
{
	const EGBGesture Gesture = (EGBGesture)GestureByte;
	const float S = UIScale();
	const float Thick = 4.f * S;
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

void AGBHUD::DrawDamageFlash()
{
	const float Elapsed = (float)(GetRealTime() - LastDamageTime);
	if (Elapsed < 0.f || Elapsed >= GBHudLayout::DamageFlashTime)
	{
		return;
	}
	const float Strength = 1.f - Elapsed / GBHudLayout::DamageFlashTime;
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	const float T = 18.f * UIScale();
	constexpr int32 DamageLayers = 5;
	for (int32 k = 0; k < DamageLayers; ++k)
	{
		const float Inset = k * T;
		const FLinearColor Color(0.8f, 0.f, 0.f, 0.4f * Strength * (1.f - (float)k / DamageLayers));
		DrawRect(Color, Inset, Inset, W - 2.f * Inset, T);
		DrawRect(Color, Inset, H - Inset - T, W - 2.f * Inset, T);
		DrawRect(Color, Inset, Inset + T, T, H - 2.f * (Inset + T));
		DrawRect(Color, W - Inset - T, Inset + T, T, H - 2.f * (Inset + T));
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
	DrawRect(FLinearColor(0.82f, 0.9f, 1.f, Alpha), 0.f, 0.f, Canvas->ClipX, Canvas->ClipY);
}
