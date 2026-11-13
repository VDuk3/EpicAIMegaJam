#include "Weapons/GBGun.h"

#include "AI/GBEnemyCharacter.h"
#include "Camera/CameraComponent.h"
#include "Combat/GBCombatLibrary.h"
#include "CanvasItem.h"
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
	bool bDrawDisplayFrame;       // the game draws the display's frame lines and round pips (the pistol's are in its model)
	FVector ReloadOffset;         // reload pose: cm added to the root (camera frame: x forward, y right, z up) at full pose
	FRotator ReloadRot;           // reload pose: degrees added to the root; roll cants the magazine well towards the camera
	float ReloadFallCm;           // how far (cm, camera down) the old magazine falls before it is below the view; the new one rises from there
	float ReloadDipCm;            // the gun dips this much as the magazine leaves
	float ReloadBumpCm;           // the gun jumps up this much as the new magazine seats
	FVector2D ReloadHandReach;    // HUD: sprite pixels (x right, y down) the left hand moves from its place to meet the magazine
	FVector2D ReloadRightHandFollow; // HUD: sprite pixels the right hand moves at full pose, to stay on this gun's grip
	// Optional pump (the shotgun's forend). A model without one leaves all of these at zero and behaves as before.
	bool bPump;                   // the model has a SM_<Prefix>Pump part
	FVector PumpRest;             // pump position at rest
	FVector PumpSlide;            // vector the pump slides along to the back of its travel
	FVector PumpRackOffset;       // the gun moves this much (cm, camera frame: x forward, y right, z up) while the pump is back
	FRotator PumpRackRot;         // and turns this much: the muzzle tips up and a roll bares the forend to the camera
	FVector2D PumpHandTravel;     // HUD: sprite pixels (x right, y down) the left hand rides with the pump at full travel
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
		1.6f, 3.f, false,
		FVector(0.f, 3.5f, 19.f), FRotator(15.f, 4.f, 50.f), 30.f, 1.2f, 0.6f, FVector2D(-9.f, -10.f), FVector2D(7.5f, -37.f) };

	// Rifle art tuning from Glyphbound/SourceArt/Weapons/Rifle/export_report.json, made by Tools/Blender/rifle_export_fbx.py.
	// All provisional. The rifle's pane is plain, so the game draws the pistol-style frame on it (owner: "like the
	// pistol's: the count, and the row below shows the rounds left") and the digits use the pistol's layout. View scale
	// and root offset were tuned from the smoke screenshots.
	const FGBGunModelArt RifleArt = {
		TEXT("Rifle"), TEXT("/Game/Glyphbound/Weapons/Rifle"), TEXT("SM_Rifle_"), false,
		0.62f, FVector(-12.f, -6.f, 2.f), -0.4f,
		FVector(6.5f, 0.f, 0.3f), FVector(15.8f, 0.f, -2.8f), FVector(2.f, 0.f, -17.f),
		FLinearColor(0.05f, 0.9f, 0.6f), FVector(69.f, 0.f, 6.6f), 13.f,
		0.42f, 0.6f, 0.39f,
		1.2f, 1.5f, true,
		FVector(4.f, 1.7f, 11.f), FRotator(16.f, 0.f, 40.f), 32.f, 1.f, 0.5f, FVector2D(0.f, -2.f), FVector2D(10.f, -19.f) };

	// Shotgun art tuning from Glyphbound/SourceArt/Weapons/Shotgun/export_report.json (offsets in cm in the body's frame).
	// The shell is the "Magazine" part: at rest it sits inside the loading port on the underside, and the pump is a sleeve
	// around the forend. The pane is plain (the game draws the frame and one pip per shell). Framing, kick and the reload
	// pose are provisional, tuned from the smoke screenshots.
	const FGBGunModelArt ShotgunArt = {
		TEXT("Shotgun"), TEXT("/Game/Glyphbound/Weapons/Shotgun"), TEXT("SM_Shotgun_"), false,
		0.55f, FVector(-6.f, -5.f, -1.5f), -0.4f,
		FVector(5.8f, 0.f, 0.6f), FVector(18.47f, 0.f, -5.04f), FVector(-5.f, 0.f, -1.81f),
		FLinearColor(0.10f, 0.85f, 1.00f), FVector(65.48f, 0.f, -1.24f), 13.f,
		0.42f, 0.6f, 0.39f,
		2.2f, 2.5f, true,
		FVector(8.f, 3.f, 5.f), FRotator(15.f, 0.f, 130.f), 32.f, 0.f, 0.5f, FVector2D(12.f, -24.f), FVector2D(10.f, -19.f),
		true, FVector(49.9f, 0.f, -10.45f), FVector(-4.11f, 0.f, 0.88f), FVector(-2.f, -1.5f, -1.f), FRotator(7.f, -10.f, 12.f), FVector2D(-4.f, 6.f) };

	const FGBGunModelArt* FindModelArt(const FString& Name)
	{
		for (const FGBGunModelArt* Art : { &PistolArt, &RifleArt, &ShotgunArt })
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
	// Energy bar: it blinks while the enchant has GunEnchantBlinkSeconds or less left, GunEnchantBlinkHz times per
	// second (Game.json; owner: "the light that shows the enchant's time starts to blink when 3 seconds are left").
	const FName EnchantBlinkSecondsKey(TEXT("GunEnchantBlinkSeconds"));
	const FName EnchantBlinkHzKey(TEXT("GunEnchantBlinkHz"));
	// Holographic ammo display: render target size in pixels. Provisional, owner's note 1.
	constexpr int32 AmmoDisplayWidth = 256;
	constexpr int32 AmmoDisplayHeight = 128;
	const TCHAR* const AmmoDisplayReloadText = TEXT("--");
	// Frame drawn by the game on a plain display (FGBGunModelArt::bDrawDisplayFrame), as shares of the display from its
	// top left. Copied from the look of the pistol's rear screen: a top and a bottom line, a status bar on each side,
	// and a row of pips under the digits, one per round, lit while that round is in the magazine. Provisional.
	constexpr int32 AmmoDisplayMaxPips = 30;        // larger magazines share pips (2 rounds per pip, 3...)
	constexpr float AmmoDisplaySpentPip = 0.16f;    // brightness of a spent round's pip
	constexpr float AmmoDisplayPipFill = 0.68f;     // lit share of each pip's slot; the rest is the gap
	struct FGBDisplayBox { float X0, Y0, X1, Y1; };
	const FGBDisplayBox AmmoDisplayFrame[] = {
		{ 0.10f, 0.095f, 0.78f, 0.125f },   // top line
		{ 0.08f, 0.885f, 0.60f, 0.915f },   // bottom line
		{ 0.075f, 0.26f, 0.10f, 0.60f },    // left status bar
		{ 0.90f, 0.26f, 0.925f, 0.60f } };  // right status bar
	const FGBDisplayBox AmmoDisplayPipRow = { 0.075f, 0.70f, 0.925f, 0.80f };
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

	// Reload animation: shares of the reload (GetReloadAnimProgress, 0 to 1). Provisional art values.
	constexpr float ReloadTiltEnd = 0.15f;       // the gun leaves rest for the reload pose
	constexpr float ReloadDipEnd = 0.35f;        // the dip as the magazine leaves is over
	constexpr float ReloadSlideOutEnd = 0.25f;   // the old magazine has slid out along MagazineOut, then it falls
	constexpr float ReloadOutEnd = 0.40f;        // the old magazine is below the view and hidden
	constexpr float ReloadEmptyEnd = 0.50f;      // empty well; a fresh magazine appears below the view
	constexpr float ReloadRiseEnd = 0.62f;       // the fresh magazine has risen to the end of the slide path
	constexpr float ReloadInEnd = 0.80f;         // the fresh magazine is seated
	constexpr float ReloadSeatEnd = 0.86f;       // the seat bump is over; the gun eases back to rest
	constexpr float ReloadSlideInEase = 1.6f;    // exponent of the slide-in (above 1: fastest at the end)
	// A cancelled reload (or a released preview) blends the gun back to rest over this time.
	constexpr float ReloadBlendSeconds = 0.12f;
	// Round-by-round reload (look): the gun leaves rest during this share of the FIRST round's cycle, then holds the pose.
	// Each round's cycle plays the magazine guns' "fresh magazine" beats (rise, slide in, seat bump) from ReloadEmptyEnd to ReloadSeatEnd.
	constexpr float PerRoundPoseInShare = 0.35f;
	// After the last round the gun eases back to rest over this time (it may run past the gameplay end of the reload).
	constexpr float PerRoundExitSeconds = 0.22f;
	// Pump: shares of the gun's FireInterval. Provisional.
	constexpr float PumpStartShare = 0.22f;      // the pump starts to slide back
	constexpr float PumpBackShare = 0.45f;       // fully back
	constexpr float PumpHomeShare = 0.72f;       // home again, well before the next shot is allowed

	float Smoothstep01(float T)
	{
		T = FMath::Clamp(T, 0.f, 1.f);
		return T * T * (3.f - 2.f * T);
	}

	// Share of [From, To] that Progress has covered, clamped to 0..1.
	float Share(float Progress, float From, float To)
	{
		return FMath::Clamp((Progress - From) / (To - From), 0.f, 1.f);
	}

	// 0..1: dip as the magazine leaves, and bump as the new one seats (half sine over their beats).
	float ReloadDip(float Progress)
	{
		return (Progress > ReloadTiltEnd && Progress < ReloadDipEnd) ? FMath::Sin(PI * Share(Progress, ReloadTiltEnd, ReloadDipEnd)) : 0.f;
	}
	float ReloadBump(float Progress)
	{
		return (Progress > ReloadInEnd && Progress < ReloadSeatEnd) ? FMath::Sin(PI * Share(Progress, ReloadInEnd, ReloadSeatEnd)) : 0.f;
	}
	// 0..1 how far down the magazine (and the left hand) is: falls with t^2 after the slide, rises with (1-t)^2 after the empty beat.
	float ReloadDrop(float Progress)
	{
		if (Progress < ReloadSlideOutEnd)
		{
			return 0.f;
		}
		if (Progress < ReloadOutEnd)
		{
			const float Fall = Share(Progress, ReloadSlideOutEnd, ReloadOutEnd);
			return Fall * Fall;
		}
		if (Progress < ReloadEmptyEnd)
		{
			return 1.f;
		}
		const float Rise = 1.f - Share(Progress, ReloadEmptyEnd, ReloadRiseEnd);
		return Rise * Rise;
	}
}

namespace GBReloadAnim
{
	float PoseAmount(float Progress)
	{
		if (Progress < 0.f)
		{
			return 0.f;
		}
		if (Progress < ReloadSeatEnd)
		{
			return Smoothstep01(Progress / ReloadTiltEnd);
		}
		return 1.f - Smoothstep01(Share(Progress, ReloadSeatEnd, 1.f));
	}

	FHandBeat HandBeat(float Progress)
	{
		FHandBeat Beat;
		if (Progress >= 0.f)
		{
			Beat.Reach = PoseAmount(Progress);
			// The hand starts to fall only once the magazine has slid out, but it already reaches in during the tilt.
			Beat.Drop = ReloadDrop(Progress);
			Beat.Push = ReloadBump(Progress);
		}
		return Beat;
	}
}

// ---- Round-by-round reload stepping (pure) ----

void FGBPerRoundReload::Begin(int32 Rounds, int32 Reserve, int32 Capacity, float TotalSeconds)
{
	CycleSeconds = Capacity > 0 ? FMath::Max(0.f, TotalSeconds) / Capacity : 0.f;
	CycleElapsed = 0.f;
	Moved = 0;
	Planned = FMath::Max(0, FMath::Min(Capacity - Rounds, Reserve));
	bRunning = Planned > 0;
}

int32 FGBPerRoundReload::Advance(float DeltaSeconds, int32& Rounds, int32& Reserve, int32 Capacity, int32 MaxRounds)
{
	if (!bRunning)
	{
		return 0;
	}
	int32 MovedNow = 0;
	CycleElapsed += FMath::Max(0.f, DeltaSeconds);
	// The small tolerance keeps six cycles of 1/3 s from falling a float rounding error short of 2 s.
	while (bRunning && MovedNow < MaxRounds && CycleElapsed + 0.0001f >= CycleSeconds)
	{
		CycleElapsed = FMath::Max(0.f, CycleElapsed - CycleSeconds);
		if (Rounds < Capacity && Reserve > 0)
		{
			++Rounds;
			--Reserve;
			++Moved;
			++MovedNow;
		}
		if (Rounds >= Capacity || Reserve <= 0)
		{
			bRunning = false;
			CycleElapsed = 0.f;
		}
	}
	return MovedNow;
}

void FGBPerRoundReload::Stop()
{
	bRunning = false;
	CycleElapsed = 0.f;
}

float FGBPerRoundReload::Progress() const
{
	if (Planned <= 0)
	{
		return 1.f;
	}
	return FMath::Clamp((Moved + CycleProgress()) / Planned, 0.f, 1.f);
}

float FGBPerRoundReload::CycleProgress() const
{
	return (bRunning && CycleSeconds > KINDA_SMALL_NUMBER) ? FMath::Clamp(CycleElapsed / CycleSeconds, 0.f, 1.f) : 0.f;
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
			UE_LOG(LogGB, Warning, TEXT("Weapons.json '%s': unknown ViewModel '%s' (Pistol, Rifle or Shotgun); using the placeholder shapes instead."),
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
	UStaticMesh* PumpMesh = Art.bPump ? LoadModelMesh(TEXT("Pump")) : nullptr;
	if (!BodyMesh || !TriggerMesh || !MagazineMesh || (Art.bEnergyBarMesh && !EnergyBarMesh) || !WeakPointLightMesh || !AmmoDisplayMesh || (Art.bPump && !PumpMesh))
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
	VMPump = Art.bPump ? NewViewPart(TEXT("Pump"), PumpMesh, VMBody, Art.PumpRest) : nullptr;
	PumpElapsed = -1.f;
	PumpPreview = -1.f;
	CurrentPumpAmount = 0.f;
	CurrentPoseAmount = 0.f;
	// The base pose of the whole viewmodel (offset and yaw towards the view centre); the shot kick moves it from here.
	CurrentKickBackCm = 0.f;
	KickElapsed = -1.f;
	ApplyViewModelPose();

	ViewModelMIDs.Reset();
	ViewModelIndicatorMID = nullptr;
	ViewModelDisplayMID = nullptr;
	UStaticMeshComponent* ModelParts[7] = { VMBody, VMTrigger, VMMagazine, VMEnergyBar, VMWeakPointLight, VMAmmoDisplay, VMPump };
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
	const UStaticMeshComponent* ModelParts[7] = { VMBody, VMTrigger, VMMagazine, VMEnergyBar, VMWeakPointLight, VMAmmoDisplay, VMPump };
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

bool AGBGun::IsViewModelMagazineVisible() const
{
	return VMMagazine && VMMagazine->IsVisible();
}

float AGBGun::GetViewModelRootOffsetCm() const
{
	return ModelArt ? (Root->GetRelativeLocation() - ModelArt->RootOffset).Size() : 0.f;
}

float AGBGun::GetViewModelRootAngleDeg() const
{
	if (!ModelArt)
	{
		return 0.f;
	}
	const FQuat Rest = FRotator(0.f, ModelArt->RootYaw, 0.f).Quaternion();
	return FMath::RadiansToDegrees(Root->GetRelativeRotation().Quaternion().AngularDistance(Rest));
}

FVector2D AGBGun::GetReloadRightHandFollow() const
{
	return ModelArt ? ModelArt->ReloadRightHandFollow : FVector2D::ZeroVector;
}

FVector2D AGBGun::GetReloadHandReach() const
{
	return ModelArt ? ModelArt->ReloadHandReach : FVector2D::ZeroVector;
}

bool AGBGun::ReloadsPerRound() const
{
	return GetStat(GBStat::ReloadPerRound, 0.f) > 0.5f;
}

bool AGBGun::HasPump() const
{
	return VMPump != nullptr;
}

float AGBGun::GetViewModelPumpOffsetCm() const
{
	if (!VMPump || !ModelArt)
	{
		return 0.f;
	}
	return (VMPump->GetRelativeLocation() - ModelArt->PumpRest).Size();
}

FVector2D AGBGun::GetPumpHandShift() const
{
	return ModelArt ? ModelArt->PumpHandTravel * CurrentPumpAmount : FVector2D::ZeroVector;
}

void AGBGun::SetPumpPreview(float Amount)
{
	PumpPreview = FMath::Clamp(Amount, 0.f, 1.f);
	if (bViewModel)
	{
		ApplyViewModelPose();
	}
}

void AGBGun::ClearPumpPreview()
{
	PumpPreview = -1.f;
	PumpElapsed = -1.f;
	if (bViewModel)
	{
		ApplyViewModelPose();
	}
}

void AGBGun::StartPump()
{
	if (ModelArt && ModelArt->bPump && !bReloading)
	{
		PumpElapsed = 0.f;
	}
}

void AGBGun::ResetPump()
{
	PumpElapsed = -1.f;
	PumpPreview = -1.f;
	if (bViewModel)
	{
		ApplyPump();
	}
}

void AGBGun::ApplyPump()
{
	float Amount = 0.f;
	if (ModelArt && ModelArt->bPump)
	{
		if (PumpPreview >= 0.f)
		{
			Amount = PumpPreview;
		}
		else if (PumpElapsed >= 0.f)
		{
			// Shares of the current FireInterval: still, back, fully back, home, still.
			const float T = PumpElapsed / FMath::Max(0.05f, GetStat(GBStat::FireInterval, 0.5f));
			if (T >= PumpStartShare && T < PumpBackShare)
			{
				Amount = Smoothstep01(Share(T, PumpStartShare, PumpBackShare));
			}
			else if (T >= PumpBackShare && T < PumpHomeShare)
			{
				Amount = 1.f - Smoothstep01(Share(T, PumpBackShare, PumpHomeShare));
			}
		}
	}
	CurrentPumpAmount = Amount;
	if (VMPump && ModelArt)
	{
		VMPump->SetRelativeLocation(ModelArt->PumpRest + ModelArt->PumpSlide * Amount);
	}
}

float AGBGun::GetReloadAnimProgress() const
{
	if (ReloadPreview >= 0.f)
	{
		return ReloadPreview;
	}
	if (!bReloading)
	{
		return -1.f;
	}
	return ReloadsPerRound() ? PerRound.CycleProgress() : GetReloadProgress();
}

FGBReloadLook AGBGun::ComputeReloadLook() const
{
	FGBReloadLook Look;
	const float Progress = GetReloadAnimProgress();
	if (Progress < 0.f)
	{
		return Look;
	}
	Look.bActive = true;
	if (!ReloadsPerRound())
	{
		// Magazine reload: slide out along MagazineOut, (fall), (rise), slide in; the slide share is 1 while the magazine is away.
		Look.Pose = GBReloadAnim::PoseAmount(Progress);
		Look.Dip = ReloadDip(Progress);
		Look.Bump = ReloadBump(Progress);
		Look.Drop = ReloadDrop(Progress);
		Look.Slide = 1.f;
		if (Progress < ReloadSlideOutEnd)
		{
			Look.Slide = Smoothstep01(Share(Progress, ReloadTiltEnd, ReloadSlideOutEnd));
		}
		else if (Progress >= ReloadRiseEnd)
		{
			Look.Slide = 1.f - FMath::Pow(Share(Progress, ReloadRiseEnd, ReloadInEnd), ReloadSlideInEase);
		}
		Look.bShowMag = Progress < ReloadOutEnd || Progress >= ReloadEmptyEnd;
		return Look;
	}
	// One round of a round-by-round reload: the "fresh magazine" beats of the magazine guns, once per round.
	// The pose eases in during the first round only (a pinned preview holds it fully); the last ease-out is the blend after the end.
	const bool bFirstRound = ReloadPreview < 0.f && PerRound.Moved == 0;
	Look.Pose = bFirstRound ? Smoothstep01(Progress / PerRoundPoseInShare) : 1.f;
	const float Beat = FMath::Lerp(ReloadEmptyEnd, ReloadSeatEnd, Progress);
	Look.Bump = ReloadBump(Beat);
	Look.Drop = ReloadDrop(Beat);
	Look.Slide = Beat < ReloadRiseEnd ? 1.f : 1.f - FMath::Pow(Share(Beat, ReloadRiseEnd, ReloadInEnd), ReloadSlideInEase);
	Look.bShowMag = Progress < 0.98f; // seated: inside the port, the next round starts below the view
	return Look;
}

GBReloadAnim::FHandBeat AGBGun::GetReloadHandBeat() const
{
	GBReloadAnim::FHandBeat Beat;
	if (!bViewModel)
	{
		return Beat;
	}
	const FGBReloadLook Look = ComputeReloadLook();
	if (Look.bActive)
	{
		Beat.Reach = Look.Pose;
		Beat.Drop = Look.Drop;
		Beat.Push = Look.Bump;
	}
	else if (ReloadsPerRound())
	{
		Beat.Reach = CurrentPoseAmount;
	}
	return Beat;
}

float AGBGun::GetReloadPoseAmount() const
{
	if (!bViewModel)
	{
		return 0.f;
	}
	const FGBReloadLook Look = ComputeReloadLook();
	return Look.bActive ? Look.Pose : (ReloadsPerRound() ? CurrentPoseAmount : 0.f);
}

void AGBGun::SetReloadPreview(float Progress)
{
	ReloadPreview = FMath::Clamp(Progress, 0.f, 1.f);
	ReloadBlendElapsed = -1.f;
	if (bViewModel)
	{
		ApplyViewModelPose();
		ApplyReloadMagazine();
	}
}

void AGBGun::ClearReloadPreview()
{
	if (ReloadPreview < 0.f)
	{
		return;
	}
	if (bViewModel && !bReloading)
	{
		BeginReloadBlend(ReloadBlendSeconds);
	}
	ReloadPreview = -1.f;
	if (bViewModel)
	{
		ApplyViewModelPose();
		ApplyReloadMagazine();
	}
}

void AGBGun::BeginReloadBlend(float Seconds)
{
	if (!bViewModel)
	{
		return;
	}
	ReloadBlendLength = FMath::Max(0.01f, Seconds);
	ReloadBlendLoc = ReloadPoseLoc;
	ReloadBlendPoseAmount = CurrentPoseAmount;
	ReloadBlendRot = ReloadPoseRot;
	ReloadBlendElapsed = 0.f;
}

void AGBGun::ResetReloadVisuals()
{
	ReloadPreview = -1.f;
	ReloadBlendElapsed = -1.f;
	ReloadPoseLoc = FVector::ZeroVector;
	ReloadPoseRot = FRotator::ZeroRotator;
	CurrentPoseAmount = 0.f;
	ResetPump();
	if (bViewModel)
	{
		ApplyViewModelPose();
		ApplyReloadMagazine();
	}
}

void AGBGun::ApplyViewModelPose()
{
	if (!ModelArt)
	{
		return;
	}
	const FGBGunModelArt& Art = *ModelArt;
	// Shot kick, ease out: the push is full at the shot and shrinks as (1 - t)^2 over KickSeconds.
	float Strength = 0.f;
	if (KickElapsed >= 0.f)
	{
		const float Remaining = 1.f - FMath::Clamp(KickElapsed / KickSeconds, 0.f, 1.f);
		Strength = Remaining * Remaining;
	}
	CurrentKickBackCm = Art.KickBackCm * Strength;

	// Pump first: its amount drives the small rack motion of the root.
	ApplyPump();

	// Reload pose: rest -> reload pose -> rest, with a dip as the magazine leaves and a bump as the new one seats.
	// After a cancel the pose it had blends to rest instead of snapping.
	const FGBReloadLook Look = ComputeReloadLook();
	if (Look.bActive)
	{
		ReloadPoseLoc = Art.ReloadOffset * Look.Pose + FVector(0.f, 0.f, Art.ReloadBumpCm * Look.Bump - Art.ReloadDipCm * Look.Dip);
		ReloadPoseRot = Art.ReloadRot * Look.Pose;
		CurrentPoseAmount = Look.Pose;
	}
	else if (ReloadBlendElapsed >= 0.f)
	{
		const float Left = 1.f - Smoothstep01(ReloadBlendElapsed / ReloadBlendLength);
		ReloadPoseLoc = ReloadBlendLoc * Left;
		ReloadPoseRot = ReloadBlendRot * Left;
		CurrentPoseAmount = ReloadBlendPoseAmount * Left;
	}
	else
	{
		ReloadPoseLoc = FVector::ZeroVector;
		ReloadPoseRot = FRotator::ZeroRotator;
		CurrentPoseAmount = 0.f;
	}

	// The pump's rack: the gun is pulled in, tips up and rolls while the pump is back. It adds to the shot kick.
	const FVector RackLoc = Art.PumpRackOffset * CurrentPumpAmount;
	const FRotator RackRot = Art.PumpRackRot * CurrentPumpAmount;
	Root->SetRelativeLocation(Art.RootOffset + FVector(-CurrentKickBackCm, 0.f, 0.f) + ReloadPoseLoc + RackLoc);
	Root->SetRelativeRotation(FRotator(Art.KickPitchDeg * Strength, Art.RootYaw, 0.f) + ReloadPoseRot + RackRot);
}

void AGBGun::ApplyReloadMagazine()
{
	if (!VMMagazine || !ModelArt)
	{
		return;
	}
	const FGBGunModelArt& Art = *ModelArt;
	FVector Offset = FVector::ZeroVector;
	const FGBReloadLook Look = ComputeReloadLook();
	// A round-by-round gun keeps its shell inside the port (hidden) outside a reload; a magazine gun shows its magazine.
	bool bShow = Look.bActive ? Look.bShowMag : !ReloadsPerRound();
	if (Look.bActive)
	{
		Offset = Art.MagazineOut * Look.Slide;
		if (Look.Drop > 0.f)
		{
			// Straight down on screen whatever the gun's tilt: camera down, brought into the body's own (scaled) frame.
			const FVector CameraDown(0.f, 0.f, -Art.ReloadFallCm * Look.Drop);
			Offset += Root->GetRelativeRotation().UnrotateVector(CameraDown) / FMath::Max(KINDA_SMALL_NUMBER, Art.ViewScale);
		}
	}
	VMMagazine->SetRelativeLocation(Art.MagazineRest + Offset);
	if (VMMagazine->IsVisible() != bShow)
	{
		VMMagazine->SetVisibility(bShow);
	}
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
	ApplyViewModelPose();

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
	}
	if (ReloadBlendElapsed >= 0.f)
	{
		ReloadBlendElapsed += DeltaSeconds;
		if (ReloadBlendElapsed >= ReloadBlendLength)
		{
			ReloadBlendElapsed = -1.f;
		}
	}
	// Pump timer: runs for PumpHomeShare of the fire interval, then the pump rests. No pump while a reload runs.
	// Real time on purpose (not the clamped EffectStep): at a low frame rate a clamped pump would still be back when
	// the next shot is allowed (Sol 6.1 review).
	if (PumpElapsed >= 0.f)
	{
		if (bReloading)
		{
			PumpElapsed = -1.f;
		}
		else
		{
			PumpElapsed += DeltaSeconds;
			if (PumpElapsed >= PumpHomeShare * FMath::Max(0.05f, GetStat(GBStat::FireInterval, 0.5f)))
			{
				PumpElapsed = -1.f;
			}
		}
	}
	// The root pose (rest + reload + kick) and the magazine follow the reload progress every frame.
	ApplyViewModelPose();
	ApplyReloadMagazine();
	if (MuzzleFlashElapsed >= 0.f)
	{
		MuzzleFlashElapsed += EffectStep;
		if (MuzzleFlashElapsed >= MuzzleFlashSeconds)
		{
			HideMuzzleFlash();
		}
	}

	// Glow color and energy bar (GDD 6, weapon enchant): enchanted = element color + imbue left, else idle glow + dark bar.
	EGBElement EnchantElement = EGBElement::Physical;
	FName EnchantEffect = NAME_None;
	FLinearColor GlowColor = Art.IdleGlow;
	float Imbue = 0.f;
	bool bEnchanted = false;
	bool bBarBlinking = false;
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
						// Last seconds: the lit cells blink. The phase comes from the time left, so the bar is lit
						// as the blinking starts, each blink is half lit and half dark, and it ends dark.
						const UGBDataSubsystem* BlinkData = UGBDataSubsystem::Get(this);
						const float BlinkSeconds = BlinkData ? BlinkData->GetGameValue(EnchantBlinkSecondsKey, 3.f) : 3.f;
						const float BlinkHz = FMath::Max(0.1f, BlinkData ? BlinkData->GetGameValue(EnchantBlinkHzKey, 4.f) : 4.f);
						if (Active.TimeRemaining > 0.f && Active.TimeRemaining <= BlinkSeconds)
						{
							bBarBlinking = true;
							if (FMath::Frac(Active.TimeRemaining * BlinkHz) < 0.5f)
							{
								Imbue = 0.f;
							}
						}
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
	bEnergyBarBlinking = bBarBlinking;

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
		// A round-by-round gun shows the live count while it loads; a magazine gun shows -- until the new magazine is in.
		const FString Wanted = (bReloading && !ReloadsPerRound()) ? FString(AmmoDisplayReloadText) : FString::FromInt(Ammo.InMagazine);
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
	// digits stay clear of the frame line and the dots: the pistol's are in its model, the rifle's are drawn below.
	float Scale = ModelArt->TextHeightShare * Height / FMath::Max(1.f, Font->GetMaxCharHeight());
	const FVector2D Size = Canvas->K2_TextSize(Font, ShownDisplayText, FVector2D(Scale, Scale));
	if (Size.X > ModelArt->TextWidthShare * Width && Size.X > KINDA_SMALL_NUMBER)
	{
		Scale *= ModelArt->TextWidthShare * Width / Size.X;
	}
	Canvas->K2_DrawText(Font, ShownDisplayText, FVector2D(Width * 0.5f, Height * ModelArt->TextCenterY), FVector2D(Scale, Scale), FLinearColor::White,
		0.f, FLinearColor(0.f, 0.f, 0.f, 0.f), FVector2D::ZeroVector, true, true, false, FLinearColor::Black);
	if (!ModelArt->bDrawDisplayFrame)
	{
		return;
	}
	auto DrawBox = [Canvas, Width, Height](float X0, float Y0, float X1, float Y1, float Brightness)
	{
		FCanvasTileItem Tile(FVector2D(X0 * Width, Y0 * Height), FVector2D((X1 - X0) * Width, (Y1 - Y0) * Height),
			FLinearColor(Brightness, Brightness, Brightness, 1.f));
		Tile.BlendMode = SE_BLEND_Opaque;
		Canvas->DrawItem(Tile);
	};
	for (const FGBDisplayBox& Box : AmmoDisplayFrame)
	{
		DrawBox(Box.X0, Box.Y0, Box.X1, Box.Y1, 1.f);
	}
	// One pip per round (several rounds per pip above AmmoDisplayMaxPips); none lit while reloading.
	const int32 Capacity = GetMagazineSize();
	if (Capacity <= 0)
	{
		return;
	}
	const int32 RoundsPerPip = FMath::DivideAndRoundUp(Capacity, AmmoDisplayMaxPips);
	const int32 Pips = FMath::DivideAndRoundUp(Capacity, RoundsPerPip);
	const int32 Lit = (bReloading && !ReloadsPerRound()) ? 0 : FMath::DivideAndRoundUp(Ammo.InMagazine, RoundsPerPip);
	const float PipSlot = (AmmoDisplayPipRow.X1 - AmmoDisplayPipRow.X0) / Pips;
	for (int32 Index = 0; Index < Pips; ++Index)
	{
		const float X0 = AmmoDisplayPipRow.X0 + Index * PipSlot;
		DrawBox(X0, AmmoDisplayPipRow.Y0, X0 + PipSlot * AmmoDisplayPipFill, AmmoDisplayPipRow.Y1, Index < Lit ? 1.f : AmmoDisplaySpentPip);
	}
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
	const int32 Magazines = FMath::Max(0, FMath::RoundToInt(GetStat(GBStat::SpareMagazines, 0.f)));
	// A round-by-round gun keeps its reserve in rounds: the same total the magazines would have held.
	return ReloadsPerRound() ? Magazines * GetMagazineSize() : Magazines;
}

void AGBGun::SetAmmo(const FGBAmmoState& InAmmo)
{
	CancelReload();
	bFireWhenLoaded = false;
	Ammo.InMagazine = FMath::Clamp(InAmmo.InMagazine, 0, GetMagazineSize());
	Ammo.SpareMagazines = FMath::Clamp(InAmmo.SpareMagazines, 0, GetMaxSpareMagazines());
	NotifyEquipmentChanged();
}

void AGBGun::RefillAmmo()
{
	CancelReload();
	bFireWhenLoaded = false;
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
	if (ReloadsPerRound())
	{
		// Count magazines' worth of rounds; the result is how many magazines' worth went in, rounded up.
		const int32 PerMagazine = FMath::Max(1, GetMagazineSize());
		const int32 AddedRounds = FMath::Clamp(GetMaxSpareMagazines() - Ammo.SpareMagazines, 0, Count * PerMagazine);
		if (AddedRounds > 0)
		{
			Ammo.SpareMagazines += AddedRounds;
			NotifyEquipmentChanged();
		}
		return FMath::DivideAndRoundUp(AddedRounds, PerMagazine);
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
	ResetPump();
	if (ReloadsPerRound())
	{
		PerRound.Begin(Ammo.InMagazine, Ammo.SpareMagazines, GetMagazineSize(), ReloadDuration);
	}
	NotifyEquipmentChanged();
}

void AGBGun::CancelReload()
{
	if (!bReloading)
	{
		return;
	}
	// Blend from the pose the gun is in now (the pinned preview, if any, keeps owning the pose until it is released).
	if (ReloadPreview < 0.f)
	{
		BeginReloadBlend(ReloadBlendSeconds);
	}
	bReloading = false;
	ReloadElapsed = 0.f;
	PerRound.Stop(); // rounds already loaded stay loaded; the round in flight was never taken from the reserve
	bFireWhenLoaded = false;
	NotifyEquipmentChanged();
}

void AGBGun::FinishPerRoundReload()
{
	bReloading = false;
	ReloadElapsed = 0.f;
	// The gun eases back to rest a little slower than a cancel and may still be moving when it is allowed to fire.
	if (ReloadPreview < 0.f)
	{
		BeginReloadBlend(PerRoundExitSeconds);
	}
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
	if (ReloadsPerRound())
	{
		return PerRound.Progress();
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
	bFireWhenLoaded = false;
	SetAiming(false);
	UpdateAimFOV(0.f, true);
	if (bViewModel)
	{
		KickElapsed = -1.f;
		ResetReloadVisuals();
		HideMuzzleFlash();
	}
	Super::OnUnequipped();
}

void AGBGun::StartPrimary()
{
	// Round-by-round gun mid-reload: the press itself interrupts, whatever the fire cooldown says.
	if (bEquipped && bReloading && ReloadsPerRound())
	{
		if (Ammo.InMagazine > 0)
		{
			CancelReload(); // the shot then follows through PressTrigger as soon as the fire rules allow
		}
		else
		{
			bFireWhenLoaded = true; // empty: wait for the first shell
		}
	}
	PressTrigger();
}

void AGBGun::Tick(float DeltaSeconds)
{
	if (bReloading && ReloadsPerRound())
	{
		ReloadElapsed += DeltaSeconds;
		const AGBPlayerCharacter* PlayerRef = GetPlayerOwner();
		if (bFireWhenLoaded && (!PlayerRef || PlayerRef->IsDead()))
		{
			bFireWhenLoaded = false; // a pending shot does not survive death
		}
		// With a shot pending the stepping yields after the first round, so a long frame cannot load more shells first.
		const int32 MaxRounds = bFireWhenLoaded ? 1 : MAX_int32;
		const int32 Moved = PerRound.Advance(DeltaSeconds, Ammo.InMagazine, Ammo.SpareMagazines, GetMagazineSize(), MaxRounds);
		if (Moved > 0)
		{
			NotifyEquipmentChanged();
		}
		if (Moved > 0 && bFireWhenLoaded && Ammo.InMagazine > 0)
		{
			// Fire was pressed on an empty gun: the first shell is in, so the reload stops now and the queued shot follows
			// through the normal trigger path as soon as the fire rules allow.
			CancelReload();
			bAttackQueued = true;
		}
		else if (!PerRound.bRunning)
		{
			FinishPerRoundReload();
		}
	}
	else if (bReloading)
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
	if (!Player || !GetWorld())
	{
		return false;
	}
	if (bReloading)
	{
		if (!ReloadsPerRound())
		{
			return false;
		}
		if (Ammo.InMagazine <= 0)
		{
			// Empty gun: the shot waits for the first shell to land (Tick), then the reload stops and the gun shoots.
			bFireWhenLoaded = true;
			return false;
		}
		CancelReload(); // fire stops a round-by-round reload; the shells already in stay in
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
		StartPump();
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
