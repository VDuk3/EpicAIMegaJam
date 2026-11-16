// The title screen of AGBHUD (owner, Nov 15): New Game (then the starting staff), Continue, Options, Quit, in two
// looks the player switches with a button (Tab / R / the top face button): the 90s pixel-art RPG (default) and the
// cyberpunk HUD. Same layout rules and pieces as the other menus (GBHUD_Menus.cpp); keyboard, gamepad and mouse.
#include "UI/GBHUD.h"

#include "UI/GBHUDShared.h"
#include "Player/GBPlayerCharacter.h"
#include "Player/GBPlayerController.h"
#include "Weapons/GBEquipmentComponent.h"
#include "Data/GBDataSubsystem.h"
#include "Core/GBStatNames.h"
#include "World/GBProgressSubsystem.h"

#include "AudioDevice.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "GameFramework/GameUserSettings.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Parse.h"
namespace GBAudio { bool Play(const UObject* Ctx, FName Key, const FVector* Loc, float PitchMul, float VolMul); void SpawnLoop(const UObject* Ctx, FName Key, const FVector& Loc); bool IsFutureMap(const UObject* Ctx); void MenuPing(const UObject* Ctx); void UpdateMusic(UWorld* World); }

static TAutoConsoleVariable<float> CVarMouseSensitivityScale(TEXT("gb.MouseSensitivityScale"), 1.f,
	TEXT("Multiplier of the mouse look sensitivity (the title screen's Options)."));

namespace GBMainMenuLocal
{
	const FName FlagMenuShown(TEXT("UI.MainMenuShown"));
	const FName FlagRunStarted(TEXT("UI.RunStarted"));
	const TCHAR* StaffFlagPrefix = TEXT("StartStaff.");
	const TCHAR* OptionsSection = TEXT("GlyphboundOptions");

	constexpr int32 EntryNewGame = 0;
	constexpr int32 EntryContinue = 1;
	constexpr int32 EntryOptions = 2;
	constexpr int32 EntryQuit = 3;
	constexpr int32 NumEntries = 4;
	const TCHAR* EntryNames[NumEntries] = { TEXT("New Game"), TEXT("Continue"), TEXT("Options"), TEXT("Quit") };

	enum EOptionRow : int32 { RowLook, RowDisplay, RowVSync, RowQuality, RowVolume, RowSensitivity, RowBack, RowCount };

	int32 Wrap(int32 Value, int32 Num)
	{
		return Num > 0 ? ((Value % Num) + Num) % Num : 0;
	}

	float ReadOption(const TCHAR* Key, float Default)
	{
		float Value = Default;
		if (GConfig)
		{
			GConfig->GetFloat(OptionsSection, Key, Value, GGameUserSettingsIni);
		}
		return Value;
	}

	void WriteOption(const TCHAR* Key, float Value)
	{
		if (GConfig)
		{
			GConfig->SetFloat(OptionsSection, Key, Value, GGameUserSettingsIni);
			GConfig->Flush(false, GGameUserSettingsIni);
		}
	}

	void ApplyVolume(float Volume01)
	{
		if (GEngine)
		{
			if (FAudioDevice* Device = GEngine->GetMainAudioDeviceRaw())
			{
				Device->SetTransientPrimaryVolume(FMath::Clamp(Volume01, 0.f, 1.f));
			}
		}
	}

	/** One short line per element, for the staff cards. */
	FString ElementBlurb(EGBElement Element)
	{
		switch (Element)
		{
		case EGBElement::Fire: return TEXT("Fire burns what it hits.");
		case EGBElement::Ice: return TEXT("Ice chills and slows.");
		case EGBElement::Lightning: return TEXT("Lightning strikes in a flash.");
		case EGBElement::Arcane: return TEXT("A steady arcane bolt.");
		default: return FString();
		}
	}

	/** The menu's look, loaded once per HUD from the options (the player's last choice). */
	TWeakObjectPtr<const AGBHUD> LookLoadedFor;
}

// ======================================================================== run state (static helpers)

bool AGBHUD::ConsumeMainMenuOpening(const UObject* WorldContextObject)
{
	if (FParse::Param(FCommandLine::Get(), TEXT("NoMenu")))
	{
		return false;
	}
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	UGBProgressSubsystem* Progress = UGBProgressSubsystem::Get(WorldContextObject);
	if (!World || !World->IsGameWorld() || !Progress || Progress->HasFlag(GBMainMenuLocal::FlagMenuShown))
	{
		return false;
	}
	// The flags live in the game instance: a level travel (the seal) or a map restart does not show it again.
	Progress->SetFlag(GBMainMenuLocal::FlagMenuShown);
	return true;
}

FName AGBHUD::GetChosenStartStaff(const UObject* WorldContextObject)
{
	const UGBProgressSubsystem* Progress = UGBProgressSubsystem::Get(WorldContextObject);
	if (!Progress)
	{
		return NAME_None;
	}
	for (const FName& Flag : Progress->GetFlags())
	{
		const FString Text = Flag.ToString();
		if (Text.StartsWith(GBMainMenuLocal::StaffFlagPrefix))
		{
			return FName(*Text.Mid(FCString::Strlen(GBMainMenuLocal::StaffFlagPrefix)));
		}
	}
	return NAME_None;
}

void AGBHUD::ApplySavedOptions(const UObject* WorldContextObject)
{
	using namespace GBMainMenuLocal;
	ApplyVolume(ReadOption(TEXT("MasterVolume"), 1.f));
	CVarMouseSensitivityScale->Set(FMath::Clamp(ReadOption(TEXT("MouseSensitivity"), 1.f), 0.25f, 2.f), ECVF_SetByGameSetting);
}

bool AGBHUD::IsRunInProgress() const
{
	const UGBProgressSubsystem* Progress = UGBProgressSubsystem::Get(this);
	return Progress && Progress->HasFlag(GBMainMenuLocal::FlagRunStarted);
}

TArray<FName> AGBHUD::GetStaffChoices() const
{
	TArray<FName> Staffs;
	if (const UGBDataSubsystem* Data = UGBDataSubsystem::Get(this))
	{
		for (const FGBWeaponDefinition& Weapon : Data->GetWeapons())
		{
			if (Weapon.Slot == EGBHeldSlot::Staff)
			{
				Staffs.Add(Weapon.Id);
			}
		}
	}
	return Staffs;
}

void AGBHUD::StartNewGame(FName StaffId)
{
	using namespace GBMainMenuLocal;
	UGBProgressSubsystem* Progress = UGBProgressSubsystem::Get(this);
	AGBPlayerController* PC = GetGBController();
	if (!Progress || !PC)
	{
		return;
	}
	const bool bFreshRestart = IsRunInProgress();
	if (bFreshRestart)
	{
		// A run was going on: a new game starts it over on the start map, with nothing kept.
		Progress->ResetProgress();
	}
	else
	{
		const FName Old = GetChosenStartStaff(this);
		if (Old != NAME_None)
		{
			Progress->SetFlag(FName(*(FString(StaffFlagPrefix) + Old.ToString())), false);
		}
	}
	Progress->SetFlag(FlagMenuShown);
	Progress->SetFlag(FlagRunStarted);
	Progress->SetFlag(FName(*(FString(StaffFlagPrefix) + StaffId.ToString())));

	if (bFreshRestart)
	{
		FString Map;
		if (GConfig)
		{
			GConfig->GetString(TEXT("/Script/EngineSettings.GameMapsSettings"), TEXT("GameDefaultMap"), Map, GEngineIni);
		}
		if (Map.IsEmpty())
		{
			Map = UGameplayStatics::GetCurrentLevelName(this, true);
		}
		PC->SetMenu(EGBMenu::None);
		// The new player's BeginPlay takes the chosen staff instead of the starting one (AGBPlayerCharacter::BeginPlay).
		UGameplayStatics::OpenLevel(this, FName(*Map));
		return;
	}

	// At boot the player already exists with Player.json's starting staff: swap it for the chosen one in place.
	AGBPlayerCharacter* Player = GetPlayer();
	UGBEquipmentComponent* Equipment = Player ? Player->GetEquipment() : nullptr;
	const UGBDataSubsystem* Data = UGBDataSubsystem::Get(this);
	if (Equipment && Data && Data->FindWeapon(StaffId))
	{
		FName Original = NAME_None;
		for (const FName& WeaponId : Data->GetPlayer().StartingWeapons)
		{
			const FGBWeaponDefinition* Def = Data->FindWeapon(WeaponId);
			if (Def && Def->Slot == EGBHeldSlot::Staff)
			{
				Original = WeaponId;
				break;
			}
		}
		if (Original != StaffId)
		{
			FGBPlayerSnapshot Snapshot;
			Equipment->SaveToSnapshot(Snapshot);
			auto Swap = [Original, StaffId](TArray<FName>& Ids)
			{
				const int32 At = Original != NAME_None ? Ids.IndexOfByKey(Original) : INDEX_NONE;
				if (At != INDEX_NONE)
				{
					Ids.RemoveAt(At);
					if (!Ids.Contains(StaffId))
					{
						Ids.Insert(StaffId, At);
					}
				}
				else
				{
					Ids.AddUnique(StaffId);
				}
			};
			Swap(Snapshot.Weapons);
			// The chosen staff takes the staff slot whatever was there.
			Snapshot.EquippedWeapons.Remove(Original);
			Snapshot.EquippedWeapons.RemoveAll([Data](FName Id)
			{
				const FGBWeaponDefinition* Def = Data->FindWeapon(Id);
				return Def && Def->Slot == EGBHeldSlot::Staff;
			});
			Snapshot.EquippedWeapons.Insert(StaffId, 0);
			Snapshot.ActiveSlot = EGBHeldSlot::Staff;
			Equipment->LoadFromSnapshot(Snapshot);
		}
	}
	PC->SetMenu(EGBMenu::None);
}

// ======================================================================== options

int32 AGBHUD::NumOptionRows() const
{
	return GBMainMenuLocal::RowCount;
}

FString AGBHUD::OptionLabel(int32 Row) const
{
	using namespace GBMainMenuLocal;
	switch (Row)
	{
	case RowLook: return TEXT("Menu look");
	case RowDisplay: return TEXT("Display");
	case RowVSync: return TEXT("V-Sync");
	case RowQuality: return TEXT("Graphics");
	case RowVolume: return TEXT("Master volume");
	case RowSensitivity: return TEXT("Mouse sensitivity");
	case RowBack: return TEXT("Back");
	default: return FString();
	}
}

FString AGBHUD::OptionValue(int32 Row) const
{
	using namespace GBMainMenuLocal;
	const UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	switch (Row)
	{
	case RowLook:
		return MainMenuSkin == EGBHudSkin::Cyberpunk ? TEXT("Cyberpunk") : TEXT("Pixel art");
	case RowDisplay:
		return Settings && Settings->GetFullscreenMode() == EWindowMode::Windowed ? TEXT("Windowed") : TEXT("Fullscreen");
	case RowVSync:
		return Settings && Settings->IsVSyncEnabled() ? TEXT("On") : TEXT("Off");
	case RowQuality:
	{
		static const TCHAR* Names[] = { TEXT("Low"), TEXT("Medium"), TEXT("High"), TEXT("Epic"), TEXT("Cinematic") };
		const int32 Level = Settings ? Settings->GetOverallScalabilityLevel() : 3;
		return Level >= 0 && Level < 5 ? Names[Level] : TEXT("Custom");
	}
	case RowVolume:
		return FString::Printf(TEXT("%d%%"), FMath::RoundToInt(ReadOption(TEXT("MasterVolume"), 1.f) * 100.f));
	case RowSensitivity:
		return FString::Printf(TEXT("%d%%"), FMath::RoundToInt(CVarMouseSensitivityScale.GetValueOnGameThread() * 100.f));
	default:
		return FString();
	}
}

void AGBHUD::AdjustOption(int32 Row, int32 Direction)
{
	using namespace GBMainMenuLocal;
	if (Direction == 0)
	{
		return;
	}
	UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	// In the Editor (PIE) the display settings are only saved: applying them would resize or rescale the Editor itself.
	const bool bApply = !GIsEditor;
	switch (Row)
	{
	case RowLook:
		ToggleMainMenuSkin();
		break;
	case RowDisplay:
		if (Settings)
		{
			Settings->SetFullscreenMode(Settings->GetFullscreenMode() == EWindowMode::Windowed ? EWindowMode::WindowedFullscreen : EWindowMode::Windowed);
			if (bApply)
			{
				Settings->ApplyResolutionSettings(false);
			}
			Settings->SaveSettings();
		}
		break;
	case RowVSync:
		if (Settings)
		{
			Settings->SetVSyncEnabled(!Settings->IsVSyncEnabled());
			if (bApply)
			{
				Settings->ApplyNonResolutionSettings();
			}
			Settings->SaveSettings();
		}
		break;
	case RowQuality:
		if (Settings)
		{
			const int32 Level = FMath::Clamp(Settings->GetOverallScalabilityLevel(), 0, 3);
			Settings->SetOverallScalabilityLevel(Wrap(Level + Direction, 4));
			if (bApply)
			{
				Settings->ApplyNonResolutionSettings();
			}
			Settings->SaveSettings();
		}
		break;
	case RowVolume:
	{
		const float Volume = FMath::Clamp(FMath::RoundToFloat(ReadOption(TEXT("MasterVolume"), 1.f) * 10.f + Direction) / 10.f, 0.f, 1.f);
		WriteOption(TEXT("MasterVolume"), Volume);
		ApplyVolume(Volume);
		break;
	}
	case RowSensitivity:
	{
		const float Scale = FMath::Clamp(FMath::RoundToFloat(CVarMouseSensitivityScale.GetValueOnGameThread() * 20.f + Direction) / 20.f, 0.25f, 2.f);
		CVarMouseSensitivityScale->Set(Scale, ECVF_SetByGameSetting);
		WriteOption(TEXT("MouseSensitivity"), Scale);
		break;
	}
	default:
		break;
	}
}

// ======================================================================== input

void AGBHUD::ToggleMainMenuSkin()
{
	MainMenuSkin = MainMenuSkin == EGBHudSkin::Cyberpunk ? EGBHudSkin::Rpg90s : EGBHudSkin::Cyberpunk;
	GBMainMenuLocal::WriteOption(TEXT("MenuLook"), MainMenuSkin == EGBHudSkin::Cyberpunk ? 1.f : 0.f);
	// The clickables were laid out in the other look: rebuilt on the next frame.
	MenuHits.Reset();
}

void AGBHUD::MainMenuNavigate(FIntPoint Direction)
{
	GBAudio::Play(this, FName(TEXT("ui_move")), nullptr, 1.f, 1.f);
	using namespace GBMainMenuLocal;
	switch (MenuState.Page)
	{
	case MainPageRoot:
		if (Direction.Y != 0)
		{
			int32 Next = Wrap(MenuState.Cursor + Direction.Y, NumEntries);
			if (Next == EntryContinue && !IsRunInProgress())
			{
				Next = Wrap(Next + Direction.Y, NumEntries);
			}
			MenuState.Cursor = Next;
		}
		break;
	case MainPageStaff:
	{
		const int32 Num = GetStaffChoices().Num();
		const int32 Step = Direction.X != 0 ? Direction.X : Direction.Y;
		MenuState.Cursor = Wrap(MenuState.Cursor + Step, Num);
		break;
	}
	case MainPageOptions:
		if (Direction.Y != 0)
		{
			MenuState.Cursor = Wrap(MenuState.Cursor + Direction.Y, RowCount);
		}
		else if (Direction.X != 0 && MenuState.Cursor != RowBack)
		{
			AdjustOption(MenuState.Cursor, Direction.X);
		}
		break;
	default:
		break;
	}
}

void AGBHUD::MainMenuConfirm()
{
	GBAudio::Play(this, FName(TEXT("ui_select")), nullptr, 1.f, 1.f);
	using namespace GBMainMenuLocal;
	switch (MenuState.Page)
	{
	case MainPageRoot:
		switch (MenuState.Cursor)
		{
		case EntryNewGame:
		{
			// Open on the staff the player picked last time, or the first one.
			const TArray<FName> Staffs = GetStaffChoices();
			const int32 Last = Staffs.IndexOfByKey(GetChosenStartStaff(this));
			MenuState = FMenuState();
			MenuState.Page = MainPageStaff;
			MenuState.Cursor = FMath::Max(0, Last);
			MenuOpenTime = GetRealTime();
			break;
		}
		case EntryContinue:
			if (IsRunInProgress())
			{
				if (AGBPlayerController* PC = GetGBController())
				{
					PC->SetMenu(EGBMenu::None);
				}
			}
			break;
		case EntryOptions:
			MenuState = FMenuState();
			MenuState.Page = MainPageOptions;
			MenuOpenTime = GetRealTime();
			break;
		case EntryQuit:
			UKismetSystemLibrary::QuitGame(this, GetOwningPlayerController(), EQuitPreference::Quit, false);
			break;
		default:
			break;
		}
		break;
	case MainPageStaff:
	{
		const TArray<FName> Staffs = GetStaffChoices();
		if (Staffs.IsValidIndex(MenuState.Cursor))
		{
			StartNewGame(Staffs[MenuState.Cursor]);
		}
		break;
	}
	case MainPageOptions:
		if (MenuState.Cursor == RowBack)
		{
			MainMenuBack();
		}
		else
		{
			AdjustOption(MenuState.Cursor, 1);
		}
		break;
	default:
		break;
	}
}

bool AGBHUD::MainMenuBack()
{
	GBAudio::Play(this, FName(TEXT("ui_back")), nullptr, 1.f, 1.f);
	using namespace GBMainMenuLocal;
	if (MenuState.Page != MainPageRoot)
	{
		const int32 Back = MenuState.Page == MainPageOptions ? EntryOptions : EntryNewGame;
		MenuState = FMenuState();
		MenuState.Page = MainPageRoot;
		MenuState.Cursor = Back;
		MenuOpenTime = GetRealTime();
	}
	// The title screen never closes by Back.
	return true;
}

// ======================================================================== drawing

void AGBHUD::DrawMainMenu(EGBHudSkin Skin)
{
	GBAudio::MenuPing(this);
	using namespace GBMainMenuLocal;
	if (LookLoadedFor.Get() != this)
	{
		// First frame of this HUD: the look the player chose last time (the transition must not play for it).
		LookLoadedFor = this;
		const EGBHudSkin Saved = ReadOption(TEXT("MenuLook"), 0.f) > 0.5f ? EGBHudSkin::Cyberpunk : EGBHudSkin::Rpg90s;
		if (Saved != MainMenuSkin)
		{
			MainMenuSkin = Saved;
			MenuSkinShown = Saved;
			Skin = Saved;
		}
	}
	switch (MenuState.Page)
	{
	case MainPageStaff:
		DrawMainStaffSelect(Skin);
		break;
	case MainPageOptions:
		DrawMainOptions(Skin);
		break;
	default:
		DrawMainRoot(Skin);
		break;
	}
	DrawMainStyleButton(Skin);
}

float AGBHUD::DrawMainTitle(EGBHudSkin Skin, const FString& PageTitle)
{
	const float S = UIScale();
	const float P = ArtPixel();
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	const float Since = (float)(GetRealTime() - MenuOpenTime);
	const bool bRoot = PageTitle.IsEmpty();

	if (Skin == EGBHudSkin::Cyberpunk)
	{
		// Near-black screen with the menus' grid, glows and scanlines; a big glitching title at the top left.
		FillRect(0.f, 0.f, W, H, GBCy::Ink);
		MenuBackdrop(Skin);
		const float X = 140.f * S;
		FCyTextStyle Sub;
		Sub.Face = ECyFace::Mono;
		Sub.Px = 15.f;
		Sub.Color = GBCy::RedDim;
		const float TopY = bRoot ? H * 0.16f : 56.f * S;
		DrawCyText(bRoot ? TEXT("// BOOT SEQUENCE COMPLETE  //  DIRECTOR'S CUT") : TEXT("// GLYPHBOUND  //  DIRECTOR'S CUT"), X, TopY, Sub, 0.f);
		FCyTextStyle Title;
		Title.Face = ECyFace::Condensed;
		Title.Px = bRoot ? 150.f : 84.f;
		Title.Spacing = 60;
		Title.bUpper = true;
		Title.Color = GBCy::Red;
		Title.Split = (Since < 0.35f ? (1.f - Since / 0.35f) * 10.f : 1.5f * FMath::Abs(FMath::Sin(Since * 1.7f))) * S;
		const FVector2D TS = DrawCyText(bRoot ? TEXT("Glyphbound") : PageTitle, X, TopY + 18.f * S, Title, 0.f);
		const float RulerY = TopY + 18.f * S + TS.Y + 8.f * S;
		CyRuler(X, RulerY, bRoot ? 720.f * S : W - 2.f * X, GBAlpha(GBCy::RedDim, 0.8f));
		return RulerY + 30.f * S;
	}

	// RPG: dark carved stone everywhere, a warm light in the middle, the name in big gilt letters over a gold rule.
	FillRect(0.f, 0.f, W, H, GBRpg::StoneDeep);
	RpgStone(0.f, 0.f, W, H, FLinearColor(0.55f, 0.52f, 0.62f, 1.f));
	DrawGlow(W * 0.5f, H * 0.42f, W * 0.55f, H * 0.6f, FLinearColor(0.35f, 0.17f, 0.06f, 0.22f));
	for (int32 k = 0; k < 6; ++k)
	{
		const float T = 4.f * P;
		const float In = k * T;
		const FLinearColor C = GBAlpha(GBRpg::Outline, 0.55f * (1.f - k / 6.f));
		FillRect(In, In, W - 2.f * In, T, C);
		FillRect(In, H - In - T, W - 2.f * In, T, C);
		FillRect(In, In + T, T, H - 2.f * (In + T), C);
		FillRect(W - In - T, In + T, T, H - 2.f * (In + T), C);
	}
	const int32 Scale = bRoot ? FMath::Max(5, FMath::RoundToInt(14.f * S)) : FMath::Max(3, FMath::RoundToInt(7.f * S));
	FPixelTextStyle Title;
	Title.Scale = Scale;
	Title.Ink = &GBRpg::InkGold;
	Title.Outline = GBRpg::Outline;
	Title.Shadow = FLinearColor(0.f, 0.f, 0.f, 0.85f);
	Title.Color = FLinearColor(1.f, 1.f, 1.f, FMath::Clamp(Since / 0.4f, 0.f, 1.f));
	const float TitleY = FMath::RoundToFloat((bRoot ? H * 0.12f : H * 0.05f) / P) * P;
	DrawPixelText(TEXT("GLYPHBOUND"), W * 0.5f, TitleY, Title, 0.5f);
	float Y = TitleY + 7.f * Scale + 5.f * P;
	if (bRoot)
	{
		FPixelTextStyle Sub;
		Sub.Scale = FMath::Max(2, FMath::RoundToInt(4.f * S));
		Sub.Ink = &GBRpg::InkBone;
		Sub.Shadow = FLinearColor(0.f, 0.f, 0.f, 0.85f);
		DrawPixelText(TEXT("- THE DIRECTOR'S CUT -"), W * 0.5f, Y, Sub, 0.5f);
		Y += 7.f * Sub.Scale + 5.f * P;
	}
	const float DW = FMath::RoundToFloat(W * (bRoot ? 0.34f : 0.24f) / P) * P;
	RpgDivider(FMath::RoundToFloat((W - DW) * 0.5f / P) * P, Y, DW);
	return Y + 12.f * P;
}

void AGBHUD::DrawMainStyleButton(EGBHudSkin Skin)
{
	const float S = UIScale();
	const float P = ArtPixel();
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	const FString Label = Skin == EGBHudSkin::Cyberpunk ? TEXT("Look: Cyberpunk") : TEXT("LOOK: PIXEL ART");
	float BW, BH, BX, BY;
	if (Skin == EGBHudSkin::Cyberpunk)
	{
		BW = 300.f * S;
		BH = 44.f * S;
		BX = W - BW - 64.f * S;
		BY = H - BH - 48.f * S;
	}
	else
	{
		BW = 92.f * P;
		BH = 16.f * P;
		BX = FMath::RoundToFloat((W - BW - 12.f * P) / P) * P;
		BY = FMath::RoundToFloat((H - BH - 10.f * P) / P) * P;
	}
	const bool bHot = IsMouseOver(BX, BY, BW, BH);
	MenuButton(Skin, BX, BY, BW, BH, Label, true, bHot);
	AddMenuHit(EMenuHitKind::MainStyle, BX, BY, BW, BH);
	// The key that does the same, just left of the button.
	if (Skin == EGBHudSkin::Cyberpunk)
	{
		FCyTextStyle K;
		K.Face = ECyFace::Condensed;
		K.Px = 22.f * 0.72f;
		K.bUpper = true;
		const float KW = FMath::Max(FMath::RoundToFloat(22.f * S), FMath::RoundToFloat(MeasureCyText(TEXT("Tab"), K).X + 10.f * S));
		CyKey(BX - KW - 10.f * S, BY + (BH - 22.f * S) * 0.5f, TEXT("Tab"), GBCy::Cyan, 22.f);
	}
	else
	{
		float KW = 0.f;
		const int32 Small = FMath::Max(2, FMath::RoundToInt(P * 0.5f));
		KW = FMath::Max(FMath::CeilToFloat((MeasurePixelText(TEXT("Tab"), Small).X + 6 * P) / P) * P, 9 * P);
		RpgKeyCap(BX - KW - 3.f * P, BY + 3.f * P, TEXT("Tab"));
	}
}

void AGBHUD::DrawMainRoot(EGBHudSkin Skin)
{
	using namespace GBMainMenuLocal;
	const float S = UIScale();
	const float P = ArtPixel();
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	const float Top = DrawMainTitle(Skin, FString());
	const bool bCanContinue = IsRunInProgress();
	TArray<TPair<FString, FString>> Hints;
	Hints.Add(TPair<FString, FString>(TEXT("Arrows"), TEXT("Move")));
	Hints.Add(TPair<FString, FString>(TEXT("Enter"), TEXT("Select")));
	Hints.Add(TPair<FString, FString>(TEXT("Tab"), TEXT("Switch look")));

	if (Skin == EGBHudSkin::Cyberpunk)
	{
		const float X = 140.f * S;
		for (int32 i = 0; i < NumEntries; ++i)
		{
			const float RowY = Top + i * 66.f * S;
			const float RowW = 460.f * S;
			const float RowH = 54.f * S;
			const bool bEnabled = i != EntryContinue || bCanContinue;
			const bool bSelected = MenuState.Cursor == i && bEnabled;
			if (bSelected)
			{
				CyBox(X, RowY, RowW, RowH, 12.f * S, GBCy::Red, FLinearColor::Transparent, 0x4);
				DrawGlow(X + RowW * 0.3f, RowY + RowH * 0.5f, RowW * 0.6f, RowH, GBAlpha(GBCy::Red, 0.25f));
			}
			else
			{
				CyBox(X, RowY, RowW, RowH, 12.f * S, GBAlpha(GBCy::Ink, 0.4f), GBAlpha(bEnabled ? GBCy::RedDim : GBCy::Grey, 0.7f), 0x4);
			}
			if (bEnabled)
			{
				AddMenuHit(EMenuHitKind::MainEntry, X, RowY, RowW, RowH, i);
			}
			FCyTextStyle Entry;
			Entry.Face = ECyFace::Condensed;
			Entry.Px = 30.f;
			Entry.Spacing = 120;
			Entry.bUpper = true;
			Entry.Color = !bEnabled ? GBCy::Grey : (bSelected ? GBCy::Ink : GBCy::RedBright);
			const FVector2D ES = MeasureCyText(EntryNames[i], Entry);
			DrawCyText(EntryNames[i], X + 26.f * S, RowY + (RowH - ES.Y) * 0.5f, Entry, 0.f);
			if (i == EntryContinue && !bEnabled)
			{
				FCyTextStyle Note;
				Note.Face = ECyFace::Mono;
				Note.Px = 13.f;
				Note.Color = GBCy::Grey;
				DrawCyText(TEXT("NO SESSION IN MEMORY"), X + RowW - 16.f * S, RowY + (RowH - 13.f * S * 1.2f) * 0.5f, Note, 1.f);
			}
		}
		MenuHints(Skin, Hints, X, H - 62.f * S, 0.f);
		return;
	}

	// RPG: an iron-framed stone window under the title with four carved entries and the gold arrow.
	const float PW = 150.f * P;
	const float PH = 104.f * P;
	const float PX = FMath::RoundToFloat((W - PW) * 0.5f / P) * P;
	const float PY = FMath::RoundToFloat(FMath::Max(Top, H * 0.40f) / P) * P;
	RpgWindow(PX, PY, PW, PH);
	for (int32 i = 0; i < NumEntries; ++i)
	{
		const float RowX = PX + 20.f * P;
		const float RowW = PW - 40.f * P;
		const float RowH = 16.f * P;
		const float RowY = PY + 12.f * P + i * 20.f * P;
		const bool bEnabled = i != EntryContinue || bCanContinue;
		const bool bSelected = MenuState.Cursor == i && bEnabled;
		MenuRow(Skin, RowX, RowY, RowW, RowH, bSelected);
		if (bEnabled)
		{
			AddMenuHit(EMenuHitKind::MainEntry, RowX, RowY, RowW, RowH, i);
		}
		const float LH = UiMeasure(Skin, EGBTextSize::Heading, EntryNames[i]).Y;
		UiText(Skin, EGBTextSize::Heading, !bEnabled ? EGBTextTone::Disabled : (bSelected ? EGBTextTone::Selected : EGBTextTone::Muted), EntryNames[i],
			RowX + RowW * 0.5f, RowY + FMath::RoundToFloat((RowH - LH) * 0.5f), 0.5f, (bEnabled && !bSelected) ? &GBRpg::BoneDim : (bEnabled ? nullptr : &GBRpg::BoneDark));
	}
	MenuHints(Skin, Hints, PX + PW * 0.5f, PY + PH - 16.f * P + 2.f * P, 0.5f);
}

void AGBHUD::DrawMainStaffSelect(EGBHudSkin Skin)
{
	const float S = UIScale();
	const float P = ArtPixel();
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	const bool bCyber = Skin == EGBHudSkin::Cyberpunk;
	const float Top = DrawMainTitle(Skin, TEXT("Choose your staff"));
	const UGBDataSubsystem* Data = UGBDataSubsystem::Get(this);
	const TArray<FName> Staffs = GetStaffChoices();
	const int32 Num = FMath::Max(1, Staffs.Num());
	const float Now = (float)GetRealTime();

	// Content area: the same frame as the other menus' pages.
	float AX, AY, AW, AH, FooterY;
	if (bCyber)
	{
		AX = 140.f * S;
		AW = W - 2.f * AX;
		AY = Top;
		FooterY = H - 62.f * S;
		AH = FooterY - 30.f * S - AY;
	}
	else
	{
		const float WinX = FMath::RoundToFloat(W * 0.08f / P) * P;
		const float WinW = FMath::RoundToFloat((W - 2.f * WinX) / P) * P;
		const float WinY = FMath::RoundToFloat(Top / P) * P;
		const float WinH = FMath::RoundToFloat((H - WinY - 36.f * P) / P) * P;
		RpgPlaque(TEXT("CHOOSE YOUR STAFF"), WinX + WinW * 0.5f, WinY - 7.f * P, FMath::Max(3, FMath::RoundToInt(5.f * S)));
		RpgWindow(WinX, WinY + 4.f * P, WinW, WinH);
		AX = WinX + 12.f * P;
		AW = WinW - 24.f * P;
		AY = WinY + 18.f * P;
		FooterY = WinY + 4.f * P + WinH - 15.f * P;
		AH = FooterY - 6.f * P - AY;
	}
	const float Gap = bCyber ? 24.f * S : 6.f * P;
	const float CardW = bCyber ? (AW - Gap * (Num - 1)) / Num : FMath::FloorToFloat((AW - Gap * (Num - 1)) / Num / P) * P;
	const float CardH = AH;
	for (int32 i = 0; i < Staffs.Num(); ++i)
	{
		const FGBWeaponDefinition* Def = Data ? Data->FindWeapon(Staffs[i]) : nullptr;
		const float CX = AX + i * (CardW + Gap);
		const float CY = AY;
		const bool bSelected = MenuState.Cursor == i;
		const FLinearColor EC = Def ? GBHudText::ElementColor(Def->Element) : FLinearColor::White;
		AddMenuHit(EMenuHitKind::StaffCard, CX, CY, CardW, CardH, i);
		// Card body.
		if (bCyber)
		{
			CyBox(CX, CY, CardW, CardH, 16.f * S, bSelected ? GBAlpha(GBCy::Red, 0.14f) : GBAlpha(GBCy::Ink, 0.6f),
				bSelected ? GBCy::Red : GBAlpha(GBCy::RedDim, 0.7f), 0x5, bSelected ? 2.f : 1.f);
			if (bSelected)
			{
				CyBrackets(CX - 6.f * S, CY - 6.f * S, CardW + 12.f * S, CardH + 12.f * S, 22.f * S, GBCy::Cyan, 2.f);
			}
			FCyTextStyle Index;
			Index.Face = ECyFace::Mono;
			Index.Px = 13.f;
			Index.Color = bSelected ? GBCy::Cyan : GBCy::RedDim;
			DrawCyText(FString::Printf(TEXT("UNIT 0%d"), i + 1), CX + 18.f * S, CY + 14.f * S, Index, 0.f);
		}
		else
		{
			RpgWell(CX, CY, CardW, CardH);
			if (bSelected)
			{
				FillRect(CX, CY, CardW, CardH, GBAlpha(GBRpg::Ember, 0.06f));
				RpgFrame(CX, CY, CardW, CardH, ERpgFrame::Gold);
			}
		}
		// The staff itself, standing, with a glow of its element behind it; the picked one floats.
		const float SpriteTop = CY + CardH * 0.06f;
		const float SpriteH = CardH * 0.50f;
		const float Bob = bSelected ? FMath::RoundToFloat(FMath::Sin(Now * 2.6f) * 2.f) * (bCyber ? S * 2.f : P) : 0.f;
		DrawGlow(CX + CardW * 0.5f, SpriteTop + SpriteH * 0.5f, CardW * 0.45f, SpriteH * 0.55f, GBAlpha(EC, bSelected ? 0.30f : 0.10f));
		const FName Sprite = Def ? FName(*Def->Strings.FindRef(TEXT("Sprite"))) : NAME_None;
		if (Sprite == NAME_None || !DrawSpriteFit(Sprite, CX + CardW * 0.12f, SpriteTop + Bob, CardW * 0.76f, SpriteH, bSelected ? FLinearColor::White : FLinearColor(0.7f, 0.7f, 0.75f, 1.f)))
		{
			FillRect(CX + CardW * 0.5f - 2.f * P, SpriteTop + Bob, 4.f * P, SpriteH, GBAlpha(EC, 0.8f));
		}
		// Name, element, numbers, one line about it.
		const float Pad = bCyber ? 18.f * S : 6.f * P;
		float TY = CY + CardH * 0.60f;
		const FString Name = Def && !Def->DisplayName.IsEmpty() ? Def->DisplayName : Staffs[i].ToString();
		UiText(Skin, EGBTextSize::Body, bSelected ? EGBTextTone::Selected : EGBTextTone::Accent, bCyber ? Name.ToUpper() : Name.ToUpper(), CX + CardW * 0.5f, TY, 0.5f);
		TY += UiLineHeight(Skin, EGBTextSize::Body) + (bCyber ? 4.f * S : P);
		if (Def)
		{
			UiText(Skin, EGBTextSize::Small, EGBTextTone::Normal, GBHudText::ElementName(Def->Element), CX + CardW * 0.5f, TY, 0.5f, &EC);
			TY += UiLineHeight(Skin, EGBTextSize::Small) + (bCyber ? 8.f * S : 2.f * P);
			const int32 Damage = FMath::RoundToInt(Def->Stats.FindRef(GBStat::Damage));
			const float Interval = Def->Stats.FindRef(GBStat::FireInterval);
			UiText(Skin, EGBTextSize::Small, EGBTextTone::Value, FString::Printf(TEXT("Damage %d"), Damage), CX + CardW * 0.5f, TY, 0.5f);
			TY += UiLineHeight(Skin, EGBTextSize::Small);
			if (Interval > 0.f)
			{
				UiText(Skin, EGBTextSize::Small, EGBTextTone::Value, FString::Printf(TEXT("%.1f shots/s"), 1.f / Interval), CX + CardW * 0.5f, TY, 0.5f);
				TY += UiLineHeight(Skin, EGBTextSize::Small);
			}
			TY += bCyber ? 8.f * S : 2.f * P;
			UiWrapped(Skin, EGBTextSize::Small, EGBTextTone::Muted, GBMainMenuLocal::ElementBlurb(Def->Element), CX + Pad, TY, CardW - 2.f * Pad);
		}
	}
	TArray<TPair<FString, FString>> Hints;
	Hints.Add(TPair<FString, FString>(TEXT("Left Right"), TEXT("Choose")));
	Hints.Add(TPair<FString, FString>(TEXT("Enter"), TEXT("Begin")));
	Hints.Add(TPair<FString, FString>(TEXT("Esc"), TEXT("Back")));
	MenuHints(Skin, Hints, bCyber ? AX : W * 0.5f, FooterY, bCyber ? 0.f : 0.5f);
}

void AGBHUD::DrawMainOptions(EGBHudSkin Skin)
{
	using namespace GBMainMenuLocal;
	const float S = UIScale();
	const float P = ArtPixel();
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	const bool bCyber = Skin == EGBHudSkin::Cyberpunk;
	const float Top = DrawMainTitle(Skin, TEXT("Options"));

	float LX, LW, RowH, RowGap, Y0, FooterY;
	if (bCyber)
	{
		LX = 140.f * S;
		LW = FMath::Min(900.f * S, W - 2.f * LX);
		RowH = 50.f * S;
		RowGap = 10.f * S;
		Y0 = Top;
		FooterY = H - 62.f * S;
	}
	else
	{
		const float PW = 220.f * P;
		const float PH = (16.f + static_cast<int32>(RowCount) * 20.f + 18.f) * P;
		const float PX = FMath::RoundToFloat((W - PW) * 0.5f / P) * P;
		const float PY = FMath::RoundToFloat(Top / P) * P;
		RpgPlaque(TEXT("OPTIONS"), PX + PW * 0.5f, PY - 7.f * P, FMath::Max(3, FMath::RoundToInt(5.f * S)));
		RpgWindow(PX, PY + 4.f * P, PW, PH);
		LX = PX + 16.f * P;
		LW = PW - 32.f * P;
		RowH = 16.f * P;
		RowGap = 4.f * P;
		Y0 = PY + 18.f * P;
		FooterY = PY + 4.f * P + PH - 15.f * P;
	}
	for (int32 Row = 0; Row < RowCount; ++Row)
	{
		const float RY = Y0 + Row * (RowH + RowGap);
		const bool bSelected = MenuState.Cursor == Row;
		const float Inner = MenuRow(Skin, LX, RY, LW, RowH, bSelected);
		AddMenuHit(EMenuHitKind::OptionRow, LX, RY, LW, RowH, Row);
		const EGBTextSize Size = EGBTextSize::Body;
		const float LH = UiMeasure(Skin, Size, TEXT("A")).Y;
		const float TY = RY + FMath::RoundToFloat((RowH - LH) * 0.5f);
		if (Row == RowBack)
		{
			UiText(Skin, Size, bSelected ? EGBTextTone::Selected : EGBTextTone::Muted, OptionLabel(Row), LX + LW * 0.5f, TY, 0.5f);
			continue;
		}
		UiText(Skin, Size, bSelected ? EGBTextTone::Selected : EGBTextTone::Normal, OptionLabel(Row), LX + Inner, TY, 0.f);
		// "<  value  >" at the right; each arrow is its own click.
		const float ArrowW = bCyber ? 34.f * S : 10.f * P;
		const float ValueW = bCyber ? 220.f * S : 64.f * P;
		const float RightX = LX + LW - Inner;
		const float LeftArrowX = RightX - ArrowW * 2.f - ValueW;
		UiText(Skin, Size, bSelected ? EGBTextTone::Accent : EGBTextTone::Muted, TEXT("<"), LeftArrowX + ArrowW * 0.5f, TY, 0.5f);
		UiText(Skin, Size, EGBTextTone::Value, OptionValue(Row), LeftArrowX + ArrowW + ValueW * 0.5f, TY, 0.5f);
		UiText(Skin, Size, bSelected ? EGBTextTone::Accent : EGBTextTone::Muted, TEXT(">"), RightX - ArrowW * 0.5f, TY, 0.5f);
		AddMenuHit(EMenuHitKind::OptionRow, LeftArrowX, RY, ArrowW, RowH, Row, FName(TEXT("L")));
		AddMenuHit(EMenuHitKind::OptionRow, RightX - ArrowW, RY, ArrowW, RowH, Row, FName(TEXT("R")));
	}
	if (GIsEditor)
	{
		const float NoteY = Y0 + static_cast<int32>(RowCount) * (RowH + RowGap) + (bCyber ? 6.f * S : 0.f);
		if (bCyber)
		{
			UiText(Skin, EGBTextSize::Small, EGBTextTone::Muted, TEXT("Display and graphics apply in the packaged game."), LX, NoteY, 0.f);
		}
	}
	TArray<TPair<FString, FString>> Hints;
	Hints.Add(TPair<FString, FString>(TEXT("Up Down"), TEXT("Move")));
	Hints.Add(TPair<FString, FString>(TEXT("Left Right"), TEXT("Change")));
	Hints.Add(TPair<FString, FString>(TEXT("Esc"), TEXT("Back")));
	MenuHints(Skin, Hints, bCyber ? LX : W * 0.5f, FooterY, bCyber ? 0.f : 0.5f);
}
