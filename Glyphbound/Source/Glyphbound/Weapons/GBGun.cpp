#include "Weapons/GBGun.h"

#include "AI/GBEnemyCharacter.h"
#include "Camera/CameraComponent.h"
#include "Combat/GBCombatLibrary.h"
#include "Engine/Canvas.h"
#include "Engine/CanvasRenderTarget2D.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Misc/App.h"
#include "Combat/GBProjectile.h"
#include "Combat/GBStatusEffectComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/GBStatNames.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Player/GBPlayerCharacter.h"
#include "Stats/GBAttributeComponent.h"

namespace
{
	// Art tuning from Glyphbound/SourceArt/Weapons/Pistol/export_report.json, made by Tools/Blender/pistol_export_fbx.py.
	// Viewmodel downscale of the real-size (~1 m) source model.
	constexpr float PistolViewScale = 0.3f;
	// Trigger hinge offset, in cm in the body's unscaled frame (the body's scale applies through attachment).
	const FVector PistolTriggerOffset(28.84f, 0.f, 0.f);
	// Magazine centre at rest, in cm in the body's unscaled frame.
	const FVector PistolMagazineRest(0.f, 0.f, -29.31f);
	// Magazine slide-out vector, in cm in the body's frame (back and down along the grip).
	const FVector PistolMagazineOut(-12.15f, 0.f, -24.11f);
	// Trigger pull animation total time in seconds (linear in, linear out).
	constexpr float TriggerPullSeconds = 0.12f;
	// Trigger pitch at full pull in degrees (the bottom of the trigger moves back towards the grip).
	constexpr float TriggerPullPitch = -8.f;
	// The model's original green glow, shown when no enchant is active.
	const FLinearColor PistolIdleGlow(0.02f, 1.f, 0.07f);
	// Material parameters set by the game on every pistol slot.
	const FName ElementColorParam(TEXT("ElementColor"));
	const FName ImbueRemainingParam(TEXT("ImbueRemaining"));
	const FName FlashParam(TEXT("Flash"));
	const FName DisplayTextureParam(TEXT("DisplayTexture"));
	// Weak-point light: full glow for WeakPointFlashHold seconds after a weak-point hit, then a linear fade over
	// WeakPointFlashFade seconds. Provisional, owner's note 3.
	constexpr float WeakPointFlashHold = 0.12f;
	constexpr float WeakPointFlashFade = 0.3f;
	// Holographic ammo display: render target size in pixels and the share of its height the digits fill.
	// Provisional, owner's note 1.
	constexpr int32 AmmoDisplayWidth = 256;
	constexpr int32 AmmoDisplayHeight = 128;
	// The digits sit in the free window of the rear screen, measured on the model (pistol_bar_layout.py), as shares
	// of the display from its top: the top frame line ends at 0.155, the row of dots starts at 0.724, and the two
	// side status bars leave 0.164 .. 0.836 across. Owner: smaller, and up, so the digits never touch the dots.
	constexpr float AmmoDisplayTextHeightShare = 0.42f;
	constexpr float AmmoDisplayTextWidthShare = 0.6f;
	// The side bars span 0.259 .. 0.603; the font's line box carries descender space, so the box centre sits a bit
	// above the bars' centre (0.43) for the digits to look centred between them.
	constexpr float AmmoDisplayTextCenterY = 0.39f;
	const TCHAR* const AmmoDisplayReloadText = TEXT("--");

	float Smoothstep01(float T)
	{
		T = FMath::Clamp(T, 0.f, 1.f);
		return T * T * (3.f - 2.f * T);
	}
}

void AGBGun::InitFromDefinition(const FGBWeaponDefinition& Definition, AGBPlayerCharacter* InOwner)
{
	Super::InitFromDefinition(Definition, InOwner);

	const FName Required[] = { GBStat::Damage, GBStat::FireInterval, GBStat::MagazineSize, GBStat::SpareMagazines, GBStat::ReloadTime, GBStat::Range };
	for (const FName& Stat : Required)
	{
		if (!Definition.Stats.Contains(Stat))
		{
			UE_LOG(LogGB, Warning, TEXT("Weapons.json '%s': missing stat '%s', using a default"), *Definition.Id.ToString(), *Stat.ToString());
		}
	}

	BuildViewModel(Definition);
}

// ---- Looks ----

UStaticMeshComponent* AGBGun::AddPart(FName Name, EGBShape Shape, const FVector& Location, const FVector& Size, const FLinearColor& Color, bool bGlow)
{
	UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(this, Name);
	Part->SetupAttachment(Root);
	Part->SetRelativeLocation(Location);
	Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Part->SetGenerateOverlapEvents(false);
	Part->SetCastShadow(false);
	Part->RegisterComponent();
	GBPlaceholder::SetupShape(Part, Shape, Size, Color, bGlow);
	Parts.Add(Part);
	return Part;
}

void AGBGun::BuildViewModel(const FGBWeaponDefinition& Definition)
{
	if (Definition.ViewModel == TEXT("Pistol"))
	{
		if (TryBuildPistolViewModel())
		{
			return;
		}
		UE_LOG(LogGB, Warning, TEXT("Pistol viewmodel meshes are missing; using the placeholder shapes instead."));
	}
	BuildPlaceholderViewModel(Definition);
}

bool AGBGun::TryBuildPistolViewModel()
{
	auto LoadPistolMesh = [](const TCHAR* Path) -> UStaticMesh*
	{
		return LoadObject<UStaticMesh>(nullptr, Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
	};
	UStaticMesh* BodyMesh = LoadPistolMesh(TEXT("/Game/Glyphbound/Weapons/Pistol/SM_Pistol_Body.SM_Pistol_Body"));
	UStaticMesh* TriggerMesh = LoadPistolMesh(TEXT("/Game/Glyphbound/Weapons/Pistol/SM_Pistol_Trigger.SM_Pistol_Trigger"));
	UStaticMesh* MagazineMesh = LoadPistolMesh(TEXT("/Game/Glyphbound/Weapons/Pistol/SM_Pistol_Magazine.SM_Pistol_Magazine"));
	UStaticMesh* EnergyBarMesh = LoadPistolMesh(TEXT("/Game/Glyphbound/Weapons/Pistol/SM_Pistol_EnergyBar.SM_Pistol_EnergyBar"));
	UStaticMesh* WeakPointLightMesh = LoadPistolMesh(TEXT("/Game/Glyphbound/Weapons/Pistol/SM_Pistol_WeakPointLight.SM_Pistol_WeakPointLight"));
	UStaticMesh* AmmoDisplayMesh = LoadPistolMesh(TEXT("/Game/Glyphbound/Weapons/Pistol/SM_Pistol_AmmoDisplay.SM_Pistol_AmmoDisplay"));
	if (!BodyMesh || !TriggerMesh || !MagazineMesh || !EnergyBarMesh || !WeakPointLightMesh || !AmmoDisplayMesh)
	{
		return false;
	}

	auto NewViewPart = [this](FName Name, UStaticMesh* Mesh, USceneComponent* Parent, const FVector& Location) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(this, Name);
		Part->SetupAttachment(Parent);
		Part->SetRelativeLocation(Location);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetGenerateOverlapEvents(false);
		Part->SetCastShadow(false);
		Part->SetStaticMesh(Mesh);
		Part->RegisterComponent();
		return Part;
	};
	PistolBody = NewViewPart(TEXT("Body"), BodyMesh, Root, FVector::ZeroVector);
	PistolBody->SetRelativeScale3D(FVector(PistolViewScale));
	PistolEnergyBar = NewViewPart(TEXT("EnergyBar"), EnergyBarMesh, PistolBody, FVector::ZeroVector);
	PistolWeakPointLight = NewViewPart(TEXT("WeakPointLight"), WeakPointLightMesh, PistolBody, FVector::ZeroVector);
	PistolAmmoDisplay = NewViewPart(TEXT("AmmoDisplay"), AmmoDisplayMesh, PistolBody, FVector::ZeroVector);
	PistolTrigger = NewViewPart(TEXT("Trigger"), TriggerMesh, PistolBody, PistolTriggerOffset);
	PistolMagazine = NewViewPart(TEXT("Magazine"), MagazineMesh, PistolBody, PistolMagazineRest);
	// Angle the whole viewmodel slightly toward the view center so the barrel points near the crosshair.
	Root->SetRelativeRotation(FRotator(0.f, -6.f, 0.f));

	PistolMIDs.Reset();
	PistolIndicatorMID = nullptr;
	PistolDisplayMID = nullptr;
	UStaticMeshComponent* PistolParts[6] = { PistolBody, PistolTrigger, PistolMagazine, PistolEnergyBar, PistolWeakPointLight, PistolAmmoDisplay };
	for (UStaticMeshComponent* Part : PistolParts)
	{
		for (int32 MaterialSlot = 0; MaterialSlot < Part->GetNumMaterials(); ++MaterialSlot)
		{
			if (UMaterialInstanceDynamic* MID = Part->CreateDynamicMaterialInstance(MaterialSlot))
			{
				PistolMIDs.Add(MID);
				if (Part == PistolWeakPointLight && !PistolIndicatorMID)
				{
					PistolIndicatorMID = MID;
				}
				else if (Part == PistolAmmoDisplay && !PistolDisplayMID)
				{
					PistolDisplayMID = MID;
				}
			}
		}
	}

	// Holographic ammo display: the text is drawn into a small render target, only when it changes (UpdatePistolViewModel).
	PistolDisplayTarget = nullptr;
	ShownDisplayText.Reset();
	if (PistolDisplayMID && FApp::CanEverRender())
	{
		PistolDisplayTarget = UCanvasRenderTarget2D::CreateCanvasRenderTarget2D(this, UCanvasRenderTarget2D::StaticClass(), AmmoDisplayWidth, AmmoDisplayHeight);
		if (PistolDisplayTarget)
		{
			PistolDisplayTarget->ClearColor = FLinearColor::Black;
			PistolDisplayTarget->OnCanvasRenderTargetUpdate.AddDynamic(this, &AGBGun::DrawPistolDisplay);
			PistolDisplayMID->SetTextureParameterValue(DisplayTextureParam, PistolDisplayTarget);
		}
	}

	// The weak-point light listens to the global damage event (hitscan and projectile shots alike).
	WeakPointFlashElapsed = -1.f;
	LastFlashSent = -1.f;
	DamageDealtHandle = UGBCombatLibrary::OnDamageDealt().AddUObject(this, &AGBGun::HandleDamageDealt);

	bPistolViewModel = true;
	TriggerPullElapsed = -1.f;
	bHasLastGlow = false;
	LastImbueSent = 0.f;
	return true;
}

void AGBGun::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (DamageDealtHandle.IsValid())
	{
		UGBCombatLibrary::OnDamageDealt().Remove(DamageDealtHandle);
		DamageDealtHandle.Reset();
	}
	if (PistolDisplayTarget)
	{
		PistolDisplayTarget->OnCanvasRenderTargetUpdate.RemoveAll(this);
	}
	Super::EndPlay(EndPlayReason);
}

int32 AGBGun::GetPistolMeshCount() const
{
	const UStaticMeshComponent* PistolParts[6] = { PistolBody, PistolTrigger, PistolMagazine, PistolEnergyBar, PistolWeakPointLight, PistolAmmoDisplay };
	int32 Count = 0;
	for (const UStaticMeshComponent* Part : PistolParts)
	{
		const UStaticMesh* Mesh = Part ? Part->GetStaticMesh() : nullptr;
		if (Mesh && Mesh->GetName().StartsWith(TEXT("SM_Pistol_")))
		{
			++Count;
		}
	}
	return Count;
}

float AGBGun::GetPistolMagazineOffsetCm() const
{
	if (!PistolMagazine)
	{
		return 0.f;
	}
	return (PistolMagazine->GetRelativeLocation() - PistolMagazineRest).Size();
}

void AGBGun::UpdatePistolViewModel(float DeltaSeconds)
{
	// Trigger pull: 0 to full pitch and back over TriggerPullSeconds (linear in, linear out). A new shot restarts it.
	if (PistolTrigger && TriggerPullElapsed >= 0.f)
	{
		TriggerPullElapsed += DeltaSeconds;
		if (TriggerPullElapsed >= TriggerPullSeconds)
		{
			TriggerPullElapsed = -1.f;
			PistolTrigger->SetRelativeRotation(FRotator::ZeroRotator);
		}
		else
		{
			const float Half = TriggerPullSeconds * 0.5f;
			const float Frac = TriggerPullElapsed < Half
				? TriggerPullElapsed / Half
				: 1.f - (TriggerPullElapsed - Half) / Half;
			PistolTrigger->SetRelativeRotation(FRotator(TriggerPullPitch * Frac, 0.f, 0.f));
		}
	}

	// Magazine slide during reload, driven by GetReloadProgress: out (smoothstep), held, back in (smoothstep).
	if (PistolMagazine)
	{
		FVector Offset = FVector::ZeroVector;
		if (bReloading)
		{
			const float Progress = GetReloadProgress();
			if (Progress < 1.f / 3.f)
			{
				Offset = PistolMagazineOut * Smoothstep01(Progress * 3.f);
			}
			else if (Progress < 2.f / 3.f)
			{
				Offset = PistolMagazineOut;
			}
			else
			{
				Offset = PistolMagazineOut * (1.f - Smoothstep01((Progress - 2.f / 3.f) * 3.f));
			}
		}
		PistolMagazine->SetRelativeLocation(PistolMagazineRest + Offset);
	}

	// Glow color and energy bar (GDD 6, weapon enchant): enchanted = element color + imbue left, else idle green + dark bar.
	EGBElement EnchantElement = EGBElement::Physical;
	FName EnchantEffect = NAME_None;
	FLinearColor GlowColor = PistolIdleGlow;
	float Imbue = 0.f;
	bool bEnchanted = false;
	if (GetActiveEnchant(EnchantElement, EnchantEffect))
	{
		bEnchanted = true;
		GlowColor = GBPlaceholder::ElementColor(static_cast<uint8>(EnchantElement));
		Imbue = 1.f;
		if (const AGBPlayerCharacter* Player = GetPlayerOwner())
		{
			if (const UGBStatusEffectComponent* Effects = Player->GetStatusEffects())
			{
				for (const FGBActiveStatusEffect& Active : Effects->GetActiveEffects())
				{
					if (Active.EffectId.ToString().StartsWith(TEXT("Enchant_")))
					{
						Imbue = Active.TotalDuration > KINDA_SMALL_NUMBER
							? FMath::Clamp(Active.TimeRemaining / Active.TotalDuration, 0.f, 1.f)
							: 1.f;
						break;
					}
				}
			}
		}
	}
	if (!bHasLastGlow || !LastGlowColor.Equals(GlowColor) || !FMath::IsNearlyEqual(LastImbueSent, Imbue))
	{
		for (UMaterialInstanceDynamic* MID : PistolMIDs)
		{
			if (MID)
			{
				MID->SetVectorParameterValue(ElementColorParam, GlowColor);
				MID->SetScalarParameterValue(ImbueRemainingParam, Imbue);
			}
		}
		LastGlowColor = GlowColor;
		LastImbueSent = Imbue;
		bHasLastGlow = true;
	}
	LastGlowElement = EnchantElement;
	bLastGlowEnchanted = bEnchanted;

	// Weak-point light: full for WeakPointFlashHold, then a linear fade over WeakPointFlashFade; off while not equipped.
	float Flash = 0.f;
	if (WeakPointFlashElapsed >= 0.f)
	{
		WeakPointFlashElapsed += DeltaSeconds;
		if (!bEquipped || WeakPointFlashElapsed >= WeakPointFlashHold + WeakPointFlashFade)
		{
			WeakPointFlashElapsed = -1.f;
		}
		else if (WeakPointFlashElapsed < WeakPointFlashHold)
		{
			Flash = 1.f;
		}
		else
		{
			Flash = 1.f - (WeakPointFlashElapsed - WeakPointFlashHold) / WeakPointFlashFade;
		}
	}
	SetWeakPointFlash(Flash);

	// Holographic ammo count: redraw only when the wanted text differs from the one on the render target.
	if (PistolDisplayTarget)
	{
		const FString Wanted = bReloading ? FString(AmmoDisplayReloadText) : FString::FromInt(Ammo.InMagazine);
		if (Wanted != ShownDisplayText)
		{
			ShownDisplayText = Wanted;
			PistolDisplayTarget->UpdateResource();
		}
	}
}

void AGBGun::SetWeakPointFlash(float Value)
{
	if (!PistolIndicatorMID || FMath::IsNearlyEqual(Value, LastFlashSent, 0.001f))
	{
		return;
	}
	PistolIndicatorMID->SetScalarParameterValue(FlashParam, Value);
	LastFlashSent = Value;
}

void AGBGun::HandleDamageDealt(AActor* Target, const FGBDamageInfo& Info, float DamageDealt)
{
	(void)Target;
	(void)DamageDealt;
	if (!Info.bHeadshot || !bEquipped || !bPistolViewModel)
	{
		return;
	}
	const AGBPlayerCharacter* Player = GetPlayerOwner();
	if (!Player || Info.Instigator.Get() != Player)
	{
		return;
	}
	// A new weak-point hit restarts the flash; show it right away, not on the next tick.
	WeakPointFlashElapsed = 0.f;
	SetWeakPointFlash(1.f);
}

void AGBGun::DrawPistolDisplay(UCanvas* Canvas, int32 Width, int32 Height)
{
	UFont* Font = GEngine ? GEngine->GetLargeFont() : nullptr;
	if (!Canvas || !Font || ShownDisplayText.IsEmpty())
	{
		return;
	}
	// AmmoDisplayTextHeightShare of the height, narrowed if wider than the window between the side bars, centred on
	// AmmoDisplayTextCenterY so the digits stay clear of the frame line above and the dots below.
	float Scale = AmmoDisplayTextHeightShare * Height / FMath::Max(1.f, Font->GetMaxCharHeight());
	const FVector2D Size = Canvas->K2_TextSize(Font, ShownDisplayText, FVector2D(Scale, Scale));
	if (Size.X > AmmoDisplayTextWidthShare * Width && Size.X > KINDA_SMALL_NUMBER)
	{
		Scale *= AmmoDisplayTextWidthShare * Width / Size.X;
	}
	Canvas->K2_DrawText(Font, ShownDisplayText, FVector2D(Width * 0.5f, Height * AmmoDisplayTextCenterY), FVector2D(Scale, Scale), FLinearColor::White,
		0.f, FLinearColor(0.f, 0.f, 0.f, 0.f), FVector2D::ZeroVector, true, true, false, FLinearColor::Black);
}

void AGBGun::BuildPlaceholderViewModel(const FGBWeaponDefinition& Definition)
{
	// Placeholder viewmodel (art rule: guns are 3D). Sizes in cm are purely visual; X = forward, Z = up.
	// The parts sit forward of the held-item anchor so the whole gun reads as one small pistol in the
	// lower right of the view, roughly 25-35 cm long, pointing forward.
	const float* PelletStat = Definition.Stats.Find(GBStat::Pellets);
	const bool bShotgun = PelletStat && *PelletStat > 1.f;
	const bool bAutomaticLook = Definition.bAutomatic;

	const float BodyLength = bShotgun ? 24.f : (bAutomaticLook ? 26.f : 20.f);
	const float BarrelLength = bShotgun ? 16.f : (bAutomaticLook ? 14.f : 9.f);
	const float BarrelThickness = bShotgun ? 4.f : 3.f;
	constexpr float BodyForward = 8.f;

	const FLinearColor BodyColor(0.08f, 0.08f, 0.10f);
	const FLinearColor BarrelColor(0.03f, 0.03f, 0.035f);
	const FLinearColor GripColor(0.12f, 0.10f, 0.10f);
	const FLinearColor NeonColor(0.0f, 0.9f, 1.0f);

	AddPart(TEXT("Body"), EGBShape::Cube, FVector(BodyForward, 0.f, 0.f), FVector(BodyLength, 5.f, 7.5f), BodyColor, false);
	AddPart(TEXT("Barrel"), EGBShape::Cube, FVector(BodyForward + BodyLength * 0.5f + BarrelLength * 0.5f, 0.f, 1.5f),
		FVector(BarrelLength, BarrelThickness, BarrelThickness), BarrelColor, false);
	AddPart(TEXT("Grip"), EGBShape::Cube, FVector(BodyForward - BodyLength * 0.5f + 2.5f, 0.f, -7.f), FVector(4.f, 4.f, 9.f), GripColor, false);
	if (bAutomaticLook)
	{
		AddPart(TEXT("Magazine"), EGBShape::Cube, FVector(BodyForward + 2.f, 0.f, -7.f), FVector(3.f, 2.5f, 8.f), BodyColor, false);
	}
	// Neon on the camera-facing side (the anchor sits right of the view, so +Y faces away).
	AddPart(TEXT("Neon"), EGBShape::Cube, FVector(BodyForward, -2.6f, 1.f), FVector(BodyLength * 0.8f, 0.35f, 0.7f), NeonColor, true);
	// Angle the whole viewmodel slightly toward the view center so the barrel points near the crosshair.
	Root->SetRelativeRotation(FRotator(0.f, -6.f, 0.f));
}

// ---- Ammo ----

int32 AGBGun::GetMagazineSize() const
{
	return FMath::Max(0, FMath::RoundToInt(GetStat(GBStat::MagazineSize, 1.f)));
}

int32 AGBGun::GetMaxSpareMagazines() const
{
	return FMath::Max(0, FMath::RoundToInt(GetStat(GBStat::SpareMagazines, 0.f)));
}

void AGBGun::SetAmmo(const FGBAmmoState& InAmmo)
{
	CancelReload();
	Ammo.InMagazine = FMath::Clamp(InAmmo.InMagazine, 0, GetMagazineSize());
	Ammo.SpareMagazines = FMath::Clamp(InAmmo.SpareMagazines, 0, GetMaxSpareMagazines());
	NotifyEquipmentChanged();
}

void AGBGun::RefillAmmo()
{
	CancelReload();
	Ammo.InMagazine = GetMagazineSize();
	Ammo.SpareMagazines = GetMaxSpareMagazines();
	NotifyEquipmentChanged();
}

int32 AGBGun::AddSpareMagazines(int32 Count)
{
	if (Count <= 0)
	{
		return 0;
	}
	const int32 Added = FMath::Clamp(GetMaxSpareMagazines() - Ammo.SpareMagazines, 0, Count);
	if (Added > 0)
	{
		Ammo.SpareMagazines += Added;
		NotifyEquipmentChanged();
	}
	return Added;
}

// ---- Reload ----

void AGBGun::Reload()
{
	if (bReloading || !bEquipped)
	{
		return;
	}
	if (Ammo.InMagazine >= GetMagazineSize() || Ammo.SpareMagazines <= 0)
	{
		return;
	}
	bReloading = true;
	ReloadElapsed = 0.f;
	ReloadDuration = FMath::Max(0.f, GetStat(GBStat::ReloadTime, 1.f));
	NotifyEquipmentChanged();
}

void AGBGun::CancelReload()
{
	if (!bReloading)
	{
		return;
	}
	bReloading = false;
	ReloadElapsed = 0.f;
	NotifyEquipmentChanged();
}

void AGBGun::FinishReload()
{
	bReloading = false;
	ReloadElapsed = 0.f;
	if (Ammo.SpareMagazines > 0)
	{
		// The rounds left in the old magazine are lost (provisional, classic tactical style).
		--Ammo.SpareMagazines;
		Ammo.InMagazine = GetMagazineSize();
	}
	NotifyEquipmentChanged();
}

float AGBGun::GetReloadProgress() const
{
	if (!bReloading)
	{
		return 0.f;
	}
	return ReloadDuration > KINDA_SMALL_NUMBER ? FMath::Clamp(ReloadElapsed / ReloadDuration, 0.f, 1.f) : 1.f;
}

// ---- Aim ----

void AGBGun::StartSecondary()
{
	if (bEquipped)
	{
		SetAiming(true);
	}
}

void AGBGun::StopSecondary()
{
	SetAiming(false);
}

void AGBGun::SetAiming(bool bInAiming)
{
	if (bAiming == bInAiming)
	{
		return;
	}
	bAiming = bInAiming;
	if (AGBPlayerCharacter* Player = GetPlayerOwner())
	{
		Player->SetMoveSpeedScale(bAiming ? GetStat(GBStat::AimMoveSpeedMult, 1.f) : 1.f);
	}
}

void AGBGun::UpdateAimFOV(float DeltaSeconds, bool bInstant)
{
	const AGBPlayerCharacter* Player = GetPlayerOwner();
	UCameraComponent* Camera = Player ? Player->GetCamera() : nullptr;
	if (!Camera)
	{
		return;
	}

	const float Target = bAiming ? 1.f : 0.f;
	if (AimAlpha == Target)
	{
		return;
	}
	if (AimAlpha <= 0.f)
	{
		HipFOV = Camera->FieldOfView;
	}

	if (bInstant)
	{
		AimAlpha = Target;
	}
	else
	{
		const float AimTime = GetStat(GBWeaponStat::AimTime, 0.15f);
		const float Step = AimTime > KINDA_SMALL_NUMBER ? DeltaSeconds / AimTime : 1.f;
		AimAlpha = FMath::Clamp(Target > AimAlpha ? AimAlpha + Step : AimAlpha - Step, 0.f, 1.f);
	}
	Camera->SetFieldOfView(FMath::Lerp(HipFOV, GetStat(GBStat::AimFOV, HipFOV), AimAlpha));
}

void AGBGun::OnUnequipped()
{
	CancelReload();
	SetAiming(false);
	UpdateAimFOV(0.f, true);
	Super::OnUnequipped();
}

void AGBGun::Tick(float DeltaSeconds)
{
	if (bReloading)
	{
		ReloadElapsed += DeltaSeconds;
		if (ReloadElapsed >= ReloadDuration)
		{
			FinishReload();
		}
	}
	if (bEquipped)
	{
		UpdateAimFOV(DeltaSeconds, false);
	}
	if (bPistolViewModel)
	{
		UpdatePistolViewModel(DeltaSeconds);
	}
	Super::Tick(DeltaSeconds);
}

// ---- Shooting ----

bool AGBGun::PerformAttack()
{
	AGBPlayerCharacter* Player = GetPlayerOwner();
	if (!Player || !GetWorld() || bReloading)
	{
		return false;
	}
	if (Ammo.InMagazine <= 0)
	{
		Reload();
		return false;
	}

	const FGBDamageInfo BaseInfo = MakeDamageInfo();
	const float SpreadDegrees = GetCurrentSpread();

	--Ammo.InMagazine;

	const FGBWeaponDefinition* Def = GetDefinition();
	if (Def && Def->FireMode.Equals(TEXT("Projectile"), ESearchCase::IgnoreCase))
	{
		FireProjectile(BaseInfo, SpreadDegrees);
	}
	else
	{
		FireHitscan(BaseInfo, SpreadDegrees);
	}

	const float RecoilYaw = GetStat(GBStat::RecoilYaw, 0.f);
	Player->AddRecoil(GetStat(GBStat::RecoilPitch, 0.f), Random.FRandRange(-RecoilYaw, RecoilYaw), GetStat(GBStat::RecoilRecovery, 0.f));

	if (bPistolViewModel)
	{
		TriggerPullElapsed = 0.f;
	}
	NotifyEquipmentChanged();

	if (Ammo.InMagazine <= 0)
	{
		Reload();
	}
	return true;
}

float AGBGun::GetCurrentSpread() const
{
	const float Base = bAiming ? GetStat(GBStat::AimSpread, 0.f) : GetStat(GBStat::Spread, 0.f);
	const float Move = bAiming ? GetStat(GBStat::AimMoveSpread, 0.f) : GetStat(GBStat::MoveSpread, 0.f);
	const AGBPlayerCharacter* Player = GetPlayerOwner();
	const UCharacterMovementComponent* MoveComp = Player ? Player->GetCharacterMovement() : nullptr;
	if (!MoveComp)
	{
		return FMath::Max(0.f, Base);
	}
	// Full bonus at the current max walk speed (already lowered while aiming), so slow aimed steps still count.
	return UGBCombatLibrary::ComputeMovementSpread(Base, Move, MoveComp->Velocity.Size2D(), MoveComp->MaxWalkSpeed);
}

void AGBGun::FireHitscan(const FGBDamageInfo& BaseInfo, float SpreadDegrees)
{
	AGBPlayerCharacter* Player = GetPlayerOwner();
	UWorld* World = GetWorld();

	const FVector Origin = Player->GetAimOrigin();
	const FVector AimDirection = Player->GetAimDirection();
	const FVector Muzzle = Player->GetMuzzleLocation();
	const float Range = GetStat(GBStat::Range, 5000.f);
	const int32 Pellets = FMath::Max(1, FMath::RoundToInt(GetStat(GBStat::Pellets, 1.f)));
	const float FalloffStart = GetStat(GBStat::FalloffStart, 0.f);
	const float FalloffEnd = GetStat(GBStat::FalloffEnd, 0.f);
	const float NearMult = GetStat(GBStat::FalloffNearMult, 1.f);
	const float FarMult = GetStat(GBStat::FalloffFarMult, 1.f);
	const float HeadshotMult = GetStat(GBStat::HeadshotMult, 1.f);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(GBGunTrace), false, Player);
	Params.AddIgnoredActor(this);

	for (int32 Index = 0; Index < Pellets; ++Index)
	{
		const FVector Direction = UGBCombatLibrary::ApplySpread(AimDirection, SpreadDegrees, Random);
		const FVector End = Origin + Direction * Range;

		FHitResult Hit;
		const bool bHit = World->LineTraceSingleByChannel(Hit, Origin, End, GBWeapon::AimTraceChannel, Params);
		AActor* Target = bHit ? Hit.GetActor() : nullptr;

		if (Target && UGBAttributeComponent::Find(Target))
		{
			FGBDamageInfo Info = BaseInfo;
			Info.HitLocation = Hit.ImpactPoint;
			Info.HitDirection = Direction;
			Info.Amount *= UGBCombatLibrary::DistanceFalloffMultiplier(Hit.Distance, FalloffStart, FalloffEnd, NearMult, FarMult);
			UGBCombatLibrary::ApplyWeakPointBonus(Target, Info, HeadshotMult);
			UGBCombatLibrary::DealDamage(Target, Info);
		}

#if !UE_BUILD_SHIPPING
		// Placeholder tracer.
		DrawDebugLine(World, Muzzle, bHit ? Hit.ImpactPoint : End, FColor(255, 220, 120), false, 0.05f, 0, 0.6f);
#endif
	}
}

void AGBGun::FireProjectile(const FGBDamageInfo& BaseInfo, float SpreadDegrees)
{
	AGBPlayerCharacter* Player = GetPlayerOwner();
	UWorld* World = GetWorld();

	const float Range = GetStat(GBStat::Range, 5000.f);
	const FVector Muzzle = Player->GetMuzzleLocation();
	FVector AimDirection = (FindAimPoint(Range) - Muzzle).GetSafeNormal();
	if (AimDirection.IsNearlyZero())
	{
		AimDirection = Player->GetAimDirection();
	}

	FGBProjectileConfig Config;
	Config.Damage = BaseInfo;
	Config.Speed = GetStat(GBStat::ProjectileSpeed, 3000.f);
	Config.Radius = GetStat(GBStat::ProjectileRadius, 5.f);
	Config.GravityScale = GetStat(GBStat::ProjectileGravity, 0.f);
	Config.MaxRange = Range;
	Config.bPixelArt = false; // art rule: gun shots are 3D
	Config.Color = GBPlaceholder::ElementColor(static_cast<uint8>(BaseInfo.Element));

	const int32 Pellets = FMath::Max(1, FMath::RoundToInt(GetStat(GBStat::Pellets, 1.f)));
	for (int32 Index = 0; Index < Pellets; ++Index)
	{
		const FVector Direction = UGBCombatLibrary::ApplySpread(AimDirection, SpreadDegrees, Random);
		AGBProjectile::SpawnProjectile(World, Config, Muzzle, Direction, Player);
	}
}
