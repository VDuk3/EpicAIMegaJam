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
#include "Data/GBDataSubsystem.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Player/GBPlayerCharacter.h"
#include "Stats/GBAttributeComponent.h"

// Art description of one real gun model (GDD 8). One constant per model; the code path is the same for all.
// Every vector is in cm in the body's unscaled frame (source in metres, 1 unit = 1 cm); the view scale applies to the body.
struct FGBGunModelArt
{
	const TCHAR* Name;            // Definition.ViewModel value
	const TCHAR* Folder;          // asset folder, no trailing slash
	const TCHAR* Prefix;          // mesh name prefix: <Prefix>Body, <Prefix>Trigger...
	bool bEnergyBarMesh;          // the pistol has a separate energy bar mesh; the rifle's bar is inside the body material
	float ViewScale;              // viewmodel downscale of the real-size source model
	FVector RootOffset;           // cm, relative to the held-item anchor
	float RootYaw;                // degrees: angles the whole gun towards the view centre so the barrel points near the crosshair
	FVector TriggerOffset;        // trigger hinge
	FVector MagazineRest;         // magazine position at rest
	FVector MagazineOut;          // magazine slide-out vector on reload
	FLinearColor IdleGlow;        // the model's own neon colour, shown when no enchant is active
	FVector MuzzleTip;            // where the muzzle flash appears
	float EnergyCells;            // cells of the energy bar (asset geometry)
	float TextHeightShare;        // ammo display layout, shares of the display (0 = top)
	float TextWidthShare;
	float TextCenterY;
	float KickBackCm;             // shot kick: how far back the viewmodel root moves
	float KickPitchDeg;           // and how far it pitches up
};

namespace
{
	// Pistol art tuning from Glyphbound/SourceArt/Weapons/Pistol/export_report.json, made by Tools/Blender/pistol_export_fbx.py.
	// The digits sit in the free window of the rear screen, measured on the model (pistol_bar_layout.py), as shares
	// of the display from its top: the top frame line ends at 0.155, the row of dots starts at 0.724, and the two
	// side status bars leave 0.164 .. 0.836 across. Owner: smaller, and up, so the digits never touch the dots.
	// The side bars span 0.259 .. 0.603; the font's line box carries descender space, so the box centre sits a bit
	// above the bars' centre (0.43) for the digits to look centred between them.
	// Muzzle tip measured in Blender; kick values provisional.
	const FGBGunModelArt PistolArt = {
		TEXT("Pistol"), TEXT("/Game/Glyphbound/Weapons/Pistol"), TEXT("SM_Pistol_"), true,
		0.3f, FVector(0.f, 0.f, 0.f), -6.f,
		FVector(28.84f, 0.f, 0.f), FVector(0.f, 0.f, -29.31f), FVector(-12.15f, 0.f, -24.11f),
		FLinearColor(0.02f, 1.f, 0.07f), FVector(82.5f, 0.f, 20.2f), 13.f,
		0.42f, 0.6f, 0.39f,
		1.6f, 3.f };

	// Rifle art tuning from Glyphbound/SourceArt/Weapons/Rifle/export_report.json, made by Tools/Blender/rifle_export_fbx.py.
	// All provisional. The rifle's pane has no frame line and no dots, so its text fills more of the display. View scale
	// and root offset were tuned from the smoke screenshots.
	const FGBGunModelArt RifleArt = {
		TEXT("Rifle"), TEXT("/Game/Glyphbound/Weapons/Rifle"), TEXT("SM_Rifle_"), false,
		0.62f, FVector(-12.f, -6.f, 2.f), -0.4f,
		FVector(6.5f, 0.f, 0.3f), FVector(15.8f, 0.f, -2.8f), FVector(2.f, 0.f, -17.f),
		FLinearColor(0.05f, 0.9f, 0.6f), FVector(69.f, 0.f, 6.6f), 13.f,
		0.75f, 0.85f, 0.47f,
		1.2f, 1.5f };

	const FGBGunModelArt* FindModelArt(const FString& Name)
	{
		for (const FGBGunModelArt* Art : { &PistolArt, &RifleArt })
		{
			if (Name == Art->Name)
			{
				return Art;
			}
		}
		return nullptr;
	}

	// Trigger pull animation total time in seconds (linear in, linear out).
	constexpr float TriggerPullSeconds = 0.12f;
	// Trigger pitch at full pull in degrees (the bottom of the trigger moves back towards the grip).
	constexpr float TriggerPullPitch = -8.f;
	// Material parameters set by the game on every model slot.
	const FName ElementColorParam(TEXT("ElementColor"));
	const FName ImbueRemainingParam(TEXT("ImbueRemaining"));
	const FName FlashParam(TEXT("Flash"));
	const FName DisplayTextureParam(TEXT("DisplayTexture"));
	// Weak-point light: full glow for PistolWeakPointFlashHold seconds after a weak-point hit, then a linear fade over
	// PistolWeakPointFlashFade seconds (Game.json; halved by the owner, GDD v0.30). The key names stay, both guns use them.
	const FName WeakPointFlashHoldKey(TEXT("PistolWeakPointFlashHold"));
	const FName WeakPointFlashFadeKey(TEXT("PistolWeakPointFlashFade"));
	// Holographic ammo display: render target size in pixels. Provisional, owner's note 1.
	constexpr int32 AmmoDisplayWidth = 256;
	constexpr int32 AmmoDisplayHeight = 128;
	const TCHAR* const AmmoDisplayReloadText = TEXT("--");
	// Shot kick: the root eases back to rest over this time (ease out). Provisional.
	constexpr float KickSeconds = 0.09f;
	// Muzzle flash: visible this long, two crossed thin slabs of this size (cm), a random roll and 85-120 % scale.
	constexpr float MuzzleFlashSeconds = 0.05f;
	constexpr float MuzzleFlashSizeCm = 7.f;
	constexpr float MuzzleFlashThicknessCm = 0.3f;
	// Brightness multiplier of the flash colour (the glow material is additive).
	constexpr float MuzzleFlashBrightness = 2.f;
	// Visual timers never advance by more than this per frame, so a hitch cannot skip a kick or a flash entirely.
	constexpr float MaxEffectStep = 1.f / 30.f;

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
	if (!Definition.ViewModel.IsEmpty())
	{
		if (const FGBGunModelArt* Art = FindModelArt(Definition.ViewModel))
		{
			if (TryBuildViewModel(*Art))
			{
				return;
			}
			UE_LOG(LogGB, Warning, TEXT("%s viewmodel meshes are missing; using the placeholder shapes instead."), *Definition.ViewModel);
		}
		else
		{
			UE_LOG(LogGB, Warning, TEXT("Weapons.json '%s': unknown ViewModel '%s' (Pistol or Rifle); using the placeholder shapes instead."),
				*Definition.Id.ToString(), *Definition.ViewModel);
		}
	}
	BuildPlaceholderViewModel(Definition);
}

bool AGBGun::TryBuildViewModel(const FGBGunModelArt& Art)
{
	auto LoadModelMesh = [&Art](const TCHAR* Part) -> UStaticMesh*
	{
		const FString Path = FString::Printf(TEXT("%s/%s%s.%s%s"), Art.Folder, Art.Prefix, Part, Art.Prefix, Part);
		return LoadObject<UStaticMesh>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
	};
	UStaticMesh* BodyMesh = LoadModelMesh(TEXT("Body"));
	UStaticMesh* TriggerMesh = LoadModelMesh(TEXT("Trigger"));
	UStaticMesh* MagazineMesh = LoadModelMesh(TEXT("Magazine"));
	UStaticMesh* EnergyBarMesh = Art.bEnergyBarMesh ? LoadModelMesh(TEXT("EnergyBar")) : nullptr;
	UStaticMesh* WeakPointLightMesh = LoadModelMesh(TEXT("WeakPointLight"));
	UStaticMesh* AmmoDisplayMesh = LoadModelMesh(TEXT("AmmoDisplay"));
	if (!BodyMesh || !TriggerMesh || !MagazineMesh || (Art.bEnergyBarMesh && !EnergyBarMesh) || !WeakPointLightMesh || !AmmoDisplayMesh)
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
		// Self-shadow only: the gun shades itself (panels, sight, magazine well), which gives it volume; without it the
		// rifle looked soft, "made of jelly" (owner). It casts no shadow on the world, where a floating gun would look wrong.
		Part->SetCastShadow(true);
		Part->bSelfShadowOnly = true;
		Part->SetStaticMesh(Mesh);
		Part->RegisterComponent();
		return Part;
	};
	ModelArt = &Art;
	VMBody = NewViewPart(TEXT("Body"), BodyMesh, Root, FVector::ZeroVector);
	VMBody->SetRelativeScale3D(FVector(Art.ViewScale));
	VMEnergyBar = Art.bEnergyBarMesh ? NewViewPart(TEXT("EnergyBar"), EnergyBarMesh, VMBody, FVector::ZeroVector) : nullptr;
	VMWeakPointLight = NewViewPart(TEXT("WeakPointLight"), WeakPointLightMesh, VMBody, FVector::ZeroVector);
	VMAmmoDisplay = NewViewPart(TEXT("AmmoDisplay"), AmmoDisplayMesh, VMBody, FVector::ZeroVector);
	VMTrigger = NewViewPart(TEXT("Trigger"), TriggerMesh, VMBody, Art.TriggerOffset);
	VMMagazine = NewViewPart(TEXT("Magazine"), MagazineMesh, VMBody, Art.MagazineRest);
	// The base pose of the whole viewmodel (offset and yaw towards the view centre); the shot kick moves it from here.
	CurrentKickBackCm = 0.f;
	KickElapsed = -1.f;
	ApplyKickPose();

	ViewModelMIDs.Reset();
	ViewModelIndicatorMID = nullptr;
	ViewModelDisplayMID = nullptr;
	UStaticMeshComponent* ModelParts[6] = { VMBody, VMTrigger, VMMagazine, VMEnergyBar, VMWeakPointLight, VMAmmoDisplay };
	for (UStaticMeshComponent* Part : ModelParts)
	{
		if (!Part)
		{
			continue;
		}
		for (int32 MaterialSlot = 0; MaterialSlot < Part->GetNumMaterials(); ++MaterialSlot)
		{
			if (UMaterialInstanceDynamic* MID = Part->CreateDynamicMaterialInstance(MaterialSlot))
			{
				ViewModelMIDs.Add(MID);
				if (Part == VMWeakPointLight && !ViewModelIndicatorMID)
				{
					ViewModelIndicatorMID = MID;
				}
				else if (Part == VMAmmoDisplay && !ViewModelDisplayMID)
				{
					ViewModelDisplayMID = MID;
				}
			}
		}
	}

	// Holographic ammo display: the text is drawn into a small render target, only when it changes (UpdateViewModel).
	ViewModelDisplayTarget = nullptr;
	ShownDisplayText.Reset();
	if (ViewModelDisplayMID && FApp::CanEverRender())
	{
		ViewModelDisplayTarget = UCanvasRenderTarget2D::CreateCanvasRenderTarget2D(this, UCanvasRenderTarget2D::StaticClass(), AmmoDisplayWidth, AmmoDisplayHeight);
		if (ViewModelDisplayTarget)
		{
			ViewModelDisplayTarget->ClearColor = FLinearColor::Black;
			ViewModelDisplayTarget->OnCanvasRenderTargetUpdate.AddDynamic(this, &AGBGun::DrawViewModelDisplay);
			ViewModelDisplayMID->SetTextureParameterValue(DisplayTextureParam, ViewModelDisplayTarget);
		}
	}

	// Muzzle flash: two thin glowing slabs, crossed, at the muzzle tip (placeholder shapes, no assets). Hidden until a shot.
	auto NewFlashSlab = [this, &Art](const TCHAR* Name, TObjectPtr<UMaterialInstanceDynamic>& OutMID) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* Slab = NewObject<UStaticMeshComponent>(this, Name);
		Slab->SetupAttachment(Root);
		Slab->SetRelativeLocation(Art.MuzzleTip * Art.ViewScale);
		Slab->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Slab->SetGenerateOverlapEvents(false);
		Slab->SetCastShadow(false);
		Slab->RegisterComponent();
		OutMID = GBPlaceholder::SetupShape(Slab, EGBShape::Cube, FVector(MuzzleFlashThicknessCm, MuzzleFlashSizeCm, MuzzleFlashSizeCm), Art.IdleGlow, true);
		Slab->SetVisibility(false);
		return Slab;
	};
	MuzzleFlash = NewFlashSlab(TEXT("MuzzleFlash"), MuzzleFlashMID);
	MuzzleFlashCross = NewFlashSlab(TEXT("MuzzleFlashCross"), MuzzleFlashCrossMID);
	MuzzleFlashElapsed = -1.f;

	// The weak-point light listens to the global damage event (hitscan and projectile shots alike).
	WeakPointFlashElapsed = -1.f;
	LastFlashSent = -1.f;
	DamageDealtHandle = UGBCombatLibrary::OnDamageDealt().AddUObject(this, &AGBGun::HandleDamageDealt);

	bViewModel = true;
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
	if (ViewModelDisplayTarget)
	{
		ViewModelDisplayTarget->OnCanvasRenderTargetUpdate.RemoveAll(this);
	}
	Super::EndPlay(EndPlayReason);
}

int32 AGBGun::GetViewModelMeshCount() const
{
	if (!ModelArt)
	{
		return 0;
	}
	const UStaticMeshComponent* ModelParts[6] = { VMBody, VMTrigger, VMMagazine, VMEnergyBar, VMWeakPointLight, VMAmmoDisplay };
	int32 Count = 0;
	for (const UStaticMeshComponent* Part : ModelParts)
	{
		const UStaticMesh* Mesh = Part ? Part->GetStaticMesh() : nullptr;
		if (Mesh && Mesh->GetName().StartsWith(ModelArt->Prefix))
		{
			++Count;
		}
	}
	return Count;
}

float AGBGun::GetViewModelMagazineOffsetCm() const
{
	if (!VMMagazine || !ModelArt)
	{
		return 0.f;
	}
	return (VMMagazine->GetRelativeLocation() - ModelArt->MagazineRest).Size();
}

bool AGBGun::IsMuzzleFlashVisible() const
{
	return MuzzleFlash && MuzzleFlash->IsVisible();
}

void AGBGun::ApplyKickPose()
{
	if (!ModelArt)
	{
		return;
	}
	// Ease out: the push is full at the shot and shrinks as (1 - t)^2 over KickSeconds.
	float Strength = 0.f;
	if (KickElapsed >= 0.f)
	{
		const float Remaining = 1.f - FMath::Clamp(KickElapsed / KickSeconds, 0.f, 1.f);
		Strength = Remaining * Remaining;
	}
	CurrentKickBackCm = ModelArt->KickBackCm * Strength;
	Root->SetRelativeLocation(ModelArt->RootOffset + FVector(-CurrentKickBackCm, 0.f, 0.f));
	Root->SetRelativeRotation(FRotator(ModelArt->KickPitchDeg * Strength, ModelArt->RootYaw, 0.f));
}

void AGBGun::HideMuzzleFlash()
{
	MuzzleFlashElapsed = -1.f;
	if (MuzzleFlash)
	{
		MuzzleFlash->SetVisibility(false);
	}
	if (MuzzleFlashCross)
	{
		MuzzleFlashCross->SetVisibility(false);
	}
}

void AGBGun::StartShotEffects()
{
	if (!ModelArt)
	{
		return;
	}
	KickElapsed = 0.f;
	ApplyKickPose();

	// Visual randomness uses the global stream, never the gun's own, so the spread sequence is not disturbed.
	const float Roll = FMath::FRandRange(0.f, 90.f);
	const float Size = MuzzleFlashSizeCm * FMath::FRandRange(0.85f, 1.2f);
	const FVector Scale(MuzzleFlashThicknessCm / 100.f, Size / 100.f, Size / 100.f);
	const FLinearColor Color = (bHasLastGlow ? LastGlowColor : ModelArt->IdleGlow) * MuzzleFlashBrightness;
	UStaticMeshComponent* Slabs[2] = { MuzzleFlash, MuzzleFlashCross };
	UMaterialInstanceDynamic* SlabMIDs[2] = { MuzzleFlashMID, MuzzleFlashCrossMID };
	for (int32 Index = 0; Index < 2; ++Index)
	{
		if (!Slabs[Index])
		{
			continue;
		}
		Slabs[Index]->SetRelativeRotation(FRotator(0.f, 0.f, Roll + 45.f * Index));
		Slabs[Index]->SetRelativeScale3D(Scale);
		if (SlabMIDs[Index])
		{
			SlabMIDs[Index]->SetVectorParameterValue(FName(TEXT("Color")), Color);
		}
		Slabs[Index]->SetVisibility(true);
	}
	MuzzleFlashElapsed = 0.f;
}

void AGBGun::UpdateViewModel(float DeltaSeconds)
{
	const FGBGunModelArt& Art = *ModelArt;

	// Trigger pull: 0 to full pitch and back over TriggerPullSeconds (linear in, linear out). A new shot restarts it.
	if (VMTrigger && TriggerPullElapsed >= 0.f)
	{
		TriggerPullElapsed += DeltaSeconds;
		if (TriggerPullElapsed >= TriggerPullSeconds)
		{
			TriggerPullElapsed = -1.f;
			VMTrigger->SetRelativeRotation(FRotator::ZeroRotator);
		}
		else
		{
			const float Half = TriggerPullSeconds * 0.5f;
			const float Frac = TriggerPullElapsed < Half
				? TriggerPullElapsed / Half
				: 1.f - (TriggerPullElapsed - Half) / Half;
			VMTrigger->SetRelativeRotation(FRotator(TriggerPullPitch * Frac, 0.f, 0.f));
		}
	}

	// Shot kick and muzzle flash. The step is capped so a long frame still shows each one for at least one frame.
	const float EffectStep = FMath::Min(DeltaSeconds, MaxEffectStep);
	if (KickElapsed >= 0.f)
	{
		KickElapsed += EffectStep;
		if (KickElapsed >= KickSeconds)
		{
			KickElapsed = -1.f;
		}
		ApplyKickPose();
	}
	if (MuzzleFlashElapsed >= 0.f)
	{
		MuzzleFlashElapsed += EffectStep;
		if (MuzzleFlashElapsed >= MuzzleFlashSeconds)
		{
			HideMuzzleFlash();
		}
	}

	// Magazine slide during reload, driven by GetReloadProgress: out (smoothstep), held, back in (smoothstep).
	if (VMMagazine)
	{
		FVector Offset = FVector::ZeroVector;
		if (bReloading)
		{
			const float Progress = GetReloadProgress();
			if (Progress < 1.f / 3.f)
			{
				Offset = Art.MagazineOut * Smoothstep01(Progress * 3.f);
			}
			else if (Progress < 2.f / 3.f)
			{
				Offset = Art.MagazineOut;
			}
			else
			{
				Offset = Art.MagazineOut * (1.f - Smoothstep01((Progress - 2.f / 3.f) * 3.f));
			}
		}
		VMMagazine->SetRelativeLocation(Art.MagazineRest + Offset);
	}

	// Glow color and energy bar (GDD 6, weapon enchant): enchanted = element color + imbue left, else idle glow + dark bar.
	EGBElement EnchantElement = EGBElement::Physical;
	FName EnchantEffect = NAME_None;
	FLinearColor GlowColor = Art.IdleGlow;
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
						const float Left = Active.TotalDuration > KINDA_SMALL_NUMBER
							? FMath::Clamp(Active.TimeRemaining / Active.TotalDuration, 0.f, 1.f)
							: 1.f;
						// The material lights a cell while 1 - ImbueU < ImbueRemaining, with ImbueU at the cell
						// centres ((i + 0.5) / cells). Adding half a cell lights ceil(Left x cells) cells, so the
						// last cell goes dark exactly when the enchant ends (owner, GDD v0.30), not half a cell early.
						Imbue = Left > 0.f ? FMath::Min(1.f, Left + 0.5f / Art.EnergyCells) : 0.f;
						break;
					}
				}
			}
		}
	}
	if (!bHasLastGlow || !LastGlowColor.Equals(GlowColor) || !FMath::IsNearlyEqual(LastImbueSent, Imbue))
	{
		for (UMaterialInstanceDynamic* MID : ViewModelMIDs)
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
		const UGBDataSubsystem* FlashData = UGBDataSubsystem::Get(this);
		const float WeakPointFlashHold = FMath::Max(0.f, FlashData ? FlashData->GetGameValue(WeakPointFlashHoldKey, 0.06f) : 0.06f);
		const float WeakPointFlashFade = FMath::Max(0.01f, FlashData ? FlashData->GetGameValue(WeakPointFlashFadeKey, 0.15f) : 0.15f);
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
	if (ViewModelDisplayTarget)
	{
		const FString Wanted = bReloading ? FString(AmmoDisplayReloadText) : FString::FromInt(Ammo.InMagazine);
		if (Wanted != ShownDisplayText)
		{
			ShownDisplayText = Wanted;
			ViewModelDisplayTarget->UpdateResource();
		}
	}
}

void AGBGun::SetWeakPointFlash(float Value)
{
	if (!ViewModelIndicatorMID || FMath::IsNearlyEqual(Value, LastFlashSent, 0.001f))
	{
		return;
	}
	ViewModelIndicatorMID->SetScalarParameterValue(FlashParam, Value);
	LastFlashSent = Value;
}

void AGBGun::HandleDamageDealt(AActor* Target, const FGBDamageInfo& Info, float DamageDealt)
{
	(void)Target;
	(void)DamageDealt;
	if (!Info.bHeadshot || !bEquipped || !bViewModel)
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

void AGBGun::DrawViewModelDisplay(UCanvas* Canvas, int32 Width, int32 Height)
{
	UFont* Font = GEngine ? GEngine->GetLargeFont() : nullptr;
	if (!Canvas || !Font || ShownDisplayText.IsEmpty() || !ModelArt)
	{
		return;
	}
	// TextHeightShare of the height, narrowed if wider than TextWidthShare of the width, centred on TextCenterY so the
	// digits stay clear of the model's frame line and dots (pistol) or fill the pane (rifle).
	float Scale = ModelArt->TextHeightShare * Height / FMath::Max(1.f, Font->GetMaxCharHeight());
	const FVector2D Size = Canvas->K2_TextSize(Font, ShownDisplayText, FVector2D(Scale, Scale));
	if (Size.X > ModelArt->TextWidthShare * Width && Size.X > KINDA_SMALL_NUMBER)
	{
		Scale *= ModelArt->TextWidthShare * Width / Size.X;
	}
	Canvas->K2_DrawText(Font, ShownDisplayText, FVector2D(Width * 0.5f, Height * ModelArt->TextCenterY), FVector2D(Scale, Scale), FLinearColor::White,
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
	if (bViewModel)
	{
		KickElapsed = -1.f;
		ApplyKickPose();
		HideMuzzleFlash();
	}
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
	if (bViewModel)
	{
		UpdateViewModel(DeltaSeconds);
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

	if (bViewModel)
	{
		TriggerPullElapsed = 0.f;
		StartShotEffects();
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
