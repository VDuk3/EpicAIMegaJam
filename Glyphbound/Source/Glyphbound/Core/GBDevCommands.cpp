// Developer console commands. "gb.Smoke" plays a scripted run of the main systems with no human input:
// it logs what it does (lines starting with "SMOKE"), saves screenshots and quits. AI agents may not run
// Play In Editor, so this is how a build is checked for crashes and obvious breakage before a person plays.
// Usage: Glyphbound.exe -ExecCmds="gb.Smoke"   (or UnrealEditor.exe <project> -game -ExecCmds="gb.Smoke")
// Screenshots: <Project>/Saved/Screenshots/Smoke/*.png. Not compiled in Shipping builds.
#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "AI/GBEnemyCharacter.h"
#include "AI/GBEntity.h"
#include "AI/GBSkeletalEnemy.h"
#include "AI/GBShieldEnemy.h"
#include "AI/GBSpriteAnimComponent.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshResources.h"
#include "AI/GBValkyrieEnemy.h"
#include "Animation/AnimSequence.h"
#include "World/GBBreakable.h"
#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Weapons/GBHeldItem.h"
#include "Weapons/GBEquipmentComponent.h"
#include "AI/GBSquad.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/PointLight.h"
#include "Components/PointLightComponent.h"
#include "AI/GBDroneEnemy.h"
#include "AI/GBSentinelEnemy.h"
#include "AI/GBEnemy3D.h"
#include "AI/GBEncounter.h"
#include "Components/SkeletalMeshComponent.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "AI/GBTentacleRigComponent.h"
#include "Combat/GBProjectile.h"
#include "Combat/GBCombatLibrary.h"
#include "Combat/GBElementalStatusComponent.h"
#include "Combat/GBStatusEffectComponent.h"
#include "Core/GBStatNames.h"
#include "Core/GBTypes.h"
#include "Data/GBDataSubsystem.h"
#include "Items/GBInventoryComponent.h"
#include "Player/GBPlayerCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Player/GBPlayerController.h"
#include "Progression/GBSkillTreeComponent.h"
#include "QTE/GBQTEComponent.h"
#include "Spells/GBSpell_Beam.h"
#include "Spells/GBSpell_Trap.h"
#include "Spells/GBSpell_Zone.h"
#include "Spells/GBSpell_Wall.h"
#include "Spells/GBSpellbookComponent.h"
#include "Stats/GBAttributeComponent.h"
#include "Stats/GBStatsComponent.h"
#include "Weapons/GBEquipmentComponent.h"
#include "Weapons/GBGun.h"
#include "Weapons/GBHeldItem.h"
#include "AI/GBEncounter.h"
#include "AI/GBEncounterStarter.h"
#include "Engine/LevelStreaming.h"
#include "Engine/Texture2D.h"
#include "World/GBBreakable.h"
#include "World/GBChest.h"
#include "World/GBEnergyGate.h"
#include "World/GBDoor.h"
#include "World/GBLadder.h"
#include "World/GBFinalPortal.h"
#include "World/GBFluidFog.h"
#include "World/GBPortal.h"
#include "World/GBCutsceneSubsystem.h"
#include "World/GBEraBlock.h"
#include "World/GBEraSubsystem.h"
#include "World/GBSeal.h"
#include "World/GBPickup.h"
#include "World/GBProgressSubsystem.h"
#include "World/GBTestArena.h"
#include "UI/GBHUD.h"

namespace GBSmokeRun
{
	struct FStep
	{
		float Time = 0.f;
		const TCHAR* Name = TEXT("");
		TFunction<void(UWorld*, AGBPlayerCharacter*)> Run;
		/** > 0: the step waits until this much simulated (game) time has passed since the last MarkSim, so ticks always run between two samples. */
		float MinSimSinceMark = 0.f;
	};

	static TArray<FStep> Steps;
	static int32 NextStep = 0;
	static double StartSeconds = 0.0;
	/** A block that runs as its own state machine (CastleVisit) holds the step clock at HoldElapsed until it is done. */
	static bool bHoldClock = false;
	static double HoldElapsed = 0.0;
	static TWeakObjectPtr<UWorld> SmokeWorld;
	static double SimMark = 0.0;
	// A gated step waits at most this long (wall seconds past its time) before it runs anyway and reports.
	constexpr double SimGateTimeoutSeconds = 20.0;
	static void MarkSim(const UWorld* World)
	{
		SimMark = World ? World->GetTimeSeconds() : 0.0;
	}
	static FTSTicker::FDelegateHandle TickerHandle;

	// Pathing check: an orc spawned behind a wall must reach the player around it.
	constexpr float PathCheckStart = 7.8f;
	constexpr float PathCheckDuration = 6.0f;          // the way around the wall is about 17 m: some 4 s at the orc's MoveSpeed
	constexpr float PathCheckShift = 6.4f;             // every later step moves this much later
	constexpr float PathCheckReachMargin = 100.f;      // cm on top of the orc's AttackRange
	constexpr float PathCheckPlayerTolerance = 200.f;  // cm: farther than this from the start, the player is put back there first
	static TWeakObjectPtr<AGBEnemyCharacter> PathOrc;
	static float PathStartDistance = 0.f;
	static bool bPathOrcSpawned = false;
	static const FName PathOrcId(TEXT("Orc_Past"));
	static FString PistolDisplayFullText;
	static TWeakObjectPtr<AGBEnemyCharacter> PistolFlashTarget;
	static FString RifleDisplayFullText;
	static TWeakObjectPtr<AGBEnemyCharacter> RifleFlashTarget;
	static FName SmokeSkillNode = NAME_None;
	static int32 SmokeSkillPointsBefore = 0;

	// Entity step (the final boss, phase 1): the spawned test actor, the component-space tips at
	// the start of the idle window, and the reach target for chain 0.
	static TWeakObjectPtr<AGBEntity> SmokeEntity;
	static TArray<FVector> SmokeEntityTipsA;
	static FVector SmokeEntityReachTarget = FVector::ZeroVector;
	static float SmokeEntityTurnStartError = 0.f;


	/** Angle in degrees between the Entity facing and the direction from it to the player, on the ground plane. */
	static float TurnErrorDegrees(const AGBEntity* Entity, const AGBPlayerCharacter* Player)
	{
		const FVector ToPlayer = (Player->GetActorLocation() - Entity->GetActorLocation()).GetSafeNormal2D();
		const FVector Facing = Entity->GetActorForwardVector().GetSafeNormal2D();
		return FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(ToPlayer, Facing), -1.f, 1.f)));
	}

	static AGBGun* RifleInHand(AGBPlayerCharacter* Player)
	{
		AGBGun* Gun = Cast<AGBGun>(Player->GetEquipment()->GetActiveItem());
		return (Gun && Gun->GetWeaponId() == FName(TEXT("Rifle"))) ? Gun : nullptr;
	}

	// Shotgun block of the smoke run (round-by-round reload and pump).
	static AGBGun* ShotgunInHand(AGBPlayerCharacter* Player)
	{
		AGBGun* Gun = Cast<AGBGun>(Player->GetEquipment()->GetActiveItem());
		return (Gun && Gun->GetWeaponId() == FName(TEXT("Shotgun"))) ? Gun : nullptr;
	}
	static TWeakObjectPtr<AGBEnemyCharacter> ShotgunFlashTarget;
	static FGBAmmoState ShotgunAmmoBefore;
	static int32 ShotgunTubePointsBefore = 0;
	static int32 UiConfirmPointsBefore = 0;
	// The reload from 3 shells: the game time it started and the time one shell takes (ReloadTime moves with the Rapid Reload perk).
	static double ShotgunReloadStartSim = 0.0;
	static float ShotgunCycleSeconds = 0.333f;
	// Everything at rest: no reload, the shell hidden inside the port, the pump home, the root at its rest pose.
	static void CheckShotgunAtRest(const TCHAR* When, AGBGun* Gun)
	{
		const bool bOk = !Gun->IsReloading() && !Gun->IsViewModelMagazineVisible() && Gun->GetViewModelMagazineOffsetCm() < 0.05f
			&& Gun->GetViewModelPumpOffsetCm() < 0.05f && Gun->GetViewModelRootOffsetCm() < 0.05f && Gun->GetViewModelRootAngleDeg() < 0.5f;
		UE_LOG(LogGB, Display, TEXT("SMOKE shotgun at rest (%s): reloading=%d shell visible=%d shell %.2f cm pump %.2f cm root %.2f cm %.2f deg"), When,
			Gun->IsReloading() ? 1 : 0, Gun->IsViewModelMagazineVisible() ? 1 : 0, Gun->GetViewModelMagazineOffsetCm(),
			Gun->GetViewModelPumpOffsetCm(), Gun->GetViewModelRootOffsetCm(), Gun->GetViewModelRootAngleDeg());
		if (!bOk)
		{
			UE_LOG(LogGB, Error, TEXT("SMOKE shotgun rest FAILED (%s): the shell, the pump or the gun is not at rest"), When);
		}
	}

	static void Shot(const TCHAR* Name)
	{
		const FString Path = FPaths::ProjectSavedDir() / TEXT("Screenshots/Smoke") / (FString(Name) + TEXT(".png"));
		FScreenshotRequest::RequestScreenshot(Path, false, false);
		UE_LOG(LogGB, Display, TEXT("SMOKE screenshot %s"), *Path);
	}

	static FGBAmmoState ReloadAmmoBefore;

	// Energy bar blink (the enchant's last seconds). SetEnchantTimeLeft moves the active weapon enchant's clock.
	static void SetEnchantTimeLeft(AGBPlayerCharacter* Player, float Seconds)
	{
		UGBStatusEffectComponent* Effects = Player->GetStatusEffects();
		if (!Effects)
		{
			return;
		}
		FName EnchantId = NAME_None;
		for (const FGBActiveStatusEffect& Active : Effects->GetActiveEffects())
		{
			if (Active.EffectId.ToString().StartsWith(TEXT("Enchant_")))
			{
				EnchantId = Active.EffectId;
				break;
			}
		}
		if (EnchantId.IsNone())
		{
			UE_LOG(LogGB, Error, TEXT("SMOKE enchant blink: no weapon enchant is active"));
			return;
		}
		Effects->SetTimeRemaining(EnchantId, Seconds);
	}

	static void CheckEnchantBlink(const TCHAR* Gun, const AGBGun* Item, bool bExpected, const TCHAR* When)
	{
		const bool bBlinking = Item->IsViewModelEnergyBarBlinking();
		UE_LOG(LogGB, Display, TEXT("SMOKE %s energy bar blinking=%d (%s)"), Gun, bBlinking ? 1 : 0, When);
		if (bBlinking != bExpected)
		{
			UE_LOG(LogGB, Error, TEXT("SMOKE %s energy bar blink FAILED: blinking=%d %s (expected %d)"), Gun, bBlinking ? 1 : 0, When, bExpected ? 1 : 0);
		}
	}

	// One pinned beat of the reload animation: the gun is away from its rest pose, the magazine is where the beat says.
	static void CheckReloadBeat(const TCHAR* Gun, AGBGun* Item, float Progress, bool bMagazineVisible)
	{
		const bool bVisible = Item->IsViewModelMagazineVisible();
		const float Magazine = Item->GetViewModelMagazineOffsetCm();
		const float RootCm = Item->GetViewModelRootOffsetCm();
		const float RootDeg = Item->GetViewModelRootAngleDeg();
		UE_LOG(LogGB, Display, TEXT("SMOKE %s reload at %.2f: magazine visible=%d offset=%.1f cm, root %.1f cm and %.1f deg from rest"),
			Gun, Progress, bVisible ? 1 : 0, Magazine, RootCm, RootDeg);
		if (!FMath::IsNearlyEqual(Item->GetReloadAnimProgress(), Progress, 0.001f))
		{
			UE_LOG(LogGB, Error, TEXT("SMOKE %s reload FAILED: pinned at %.2f but the animation shows %.2f"), Gun, Progress, Item->GetReloadAnimProgress());
		}
		if (bVisible != bMagazineVisible)
		{
			UE_LOG(LogGB, Error, TEXT("SMOKE %s reload FAILED at %.2f: magazine visible=%d (expected %d)"), Gun, Progress, bVisible ? 1 : 0, bMagazineVisible ? 1 : 0);
		}
		if (bMagazineVisible && Magazine < 1.f)
		{
			UE_LOG(LogGB, Error, TEXT("SMOKE %s reload FAILED at %.2f: magazine only %.1f cm from rest (expected away from it)"), Gun, Progress, Magazine);
		}
		if (RootCm < 1.f && RootDeg < 5.f)
		{
			UE_LOG(LogGB, Error, TEXT("SMOKE %s reload FAILED at %.2f: the gun is at its rest pose (%.1f cm, %.1f deg)"), Gun, Progress, RootCm, RootDeg);
		}
	}

	// After a real reload ran to its end: everything at rest, the display shows the full count, one spare magazine fewer.
	static void CheckReloadFinished(const TCHAR* Gun, AGBGun* Item)
	{
		const FString Text = Item->GetViewModelDisplayText();
		const FGBAmmoState Ammo = Item->GetAmmo();
		UE_LOG(LogGB, Display, TEXT("SMOKE %s reload finished: reloading=%d magazine visible=%d offset=%.2f root %.2f cm %.2f deg display=%s ammo %d/%d spare %d (was %d)"),
			Gun, Item->IsReloading() ? 1 : 0, Item->IsViewModelMagazineVisible() ? 1 : 0, Item->GetViewModelMagazineOffsetCm(),
			Item->GetViewModelRootOffsetCm(), Item->GetViewModelRootAngleDeg(), *Text, Ammo.InMagazine, Item->GetMagazineSize(),
			Ammo.SpareMagazines, ReloadAmmoBefore.SpareMagazines);
		if (Item->IsReloading() || !Item->IsViewModelMagazineVisible() || Item->GetViewModelMagazineOffsetCm() > 0.05f
			|| Item->GetViewModelRootOffsetCm() > 0.05f || Item->GetViewModelRootAngleDeg() > 0.5f)
		{
			UE_LOG(LogGB, Error, TEXT("SMOKE %s reload FAILED: after the reload the magazine or the gun is not back at rest"), Gun);
		}
		if (Text != FString::FromInt(Item->GetMagazineSize()) || Ammo.InMagazine != Item->GetMagazineSize() || Ammo.SpareMagazines != ReloadAmmoBefore.SpareMagazines - 1)
		{
			UE_LOG(LogGB, Error, TEXT("SMOKE %s reload FAILED: display '%s', %d in the magazine and %d spare after the reload (expected the full count and one spare magazine fewer than %d)"),
				Gun, *Text, Ammo.InMagazine, Ammo.SpareMagazines, ReloadAmmoBefore.SpareMagazines);
		}
	}

	// After a reload cancelled halfway: magazine and gun at rest, ammo untouched.
	static void CheckReloadCancelled(const TCHAR* Gun, AGBGun* Item)
	{
		const FGBAmmoState Ammo = Item->GetAmmo();
		UE_LOG(LogGB, Display, TEXT("SMOKE %s reload cancelled: reloading=%d magazine visible=%d offset=%.2f root %.2f cm %.2f deg ammo %d spare %d (was %d, %d)"),
			Gun, Item->IsReloading() ? 1 : 0, Item->IsViewModelMagazineVisible() ? 1 : 0, Item->GetViewModelMagazineOffsetCm(),
			Item->GetViewModelRootOffsetCm(), Item->GetViewModelRootAngleDeg(), Ammo.InMagazine, Ammo.SpareMagazines,
			ReloadAmmoBefore.InMagazine, ReloadAmmoBefore.SpareMagazines);
		if (Item->IsReloading() || !Item->IsViewModelMagazineVisible() || Item->GetViewModelMagazineOffsetCm() > 0.05f
			|| Item->GetViewModelRootOffsetCm() > 0.05f || Item->GetViewModelRootAngleDeg() > 0.5f)
		{
			UE_LOG(LogGB, Error, TEXT("SMOKE %s reload cancel FAILED: the magazine or the gun is not back at rest"), Gun);
		}
		if (Ammo.InMagazine != ReloadAmmoBefore.InMagazine || Ammo.SpareMagazines != ReloadAmmoBefore.SpareMagazines)
		{
			UE_LOG(LogGB, Error, TEXT("SMOKE %s reload cancel FAILED: ammo %d / %d spare after the cancel (expected the unchanged %d / %d)"),
				Gun, Ammo.InMagazine, Ammo.SpareMagazines, ReloadAmmoBefore.InMagazine, ReloadAmmoBefore.SpareMagazines);
		}
	}

	static void SetQTEEnabled(bool bEnabled)
	{
		if (IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(TEXT("gb.QTE")))
		{
			Var->Set(bEnabled ? 1 : 0);
		}
	}

	static void LogState(UWorld* World, AGBPlayerCharacter* Player, const TCHAR* Label)
	{
		int32 Enemies = 0;
		int32 GBActors = 0;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->GetClass()->GetName().StartsWith(TEXT("GB")))
			{
				++GBActors;
			}
			if (It->IsA<AGBEnemyCharacter>())
			{
				++Enemies;
			}
		}
		const UGBEraSubsystem* Era = UGBEraSubsystem::Get(World);
		const UGBAttributeComponent* Attr = Player ? Player->GetAttributes() : nullptr;
		const AGBHeldItem* Held = (Player && Player->GetEquipment()) ? Player->GetEquipment()->GetActiveItem() : nullptr;
		UE_LOG(LogGB, Display, TEXT("SMOKE state [%s] player=%d dead=%d health=%.0f/%.0f mana=%.0f/%.0f era=%s held=%s gbActors=%d enemies=%d loc=%s"),
			Label, Player ? 1 : 0, (Player && Player->IsDead()) ? 1 : 0,
			Attr ? Attr->GetHealth() : -1.f, Attr ? Attr->GetMaxHealth() : -1.f,
			Attr ? Attr->GetMana() : -1.f, Attr ? Attr->GetMaxMana() : -1.f,
			(Era && Era->GetCurrentEra() == EGBEra::Future) ? TEXT("Future") : TEXT("Past"),
			Held ? *Held->GetWeaponId().ToString() : TEXT("none"),
			GBActors, Enemies, Player ? *Player->GetActorLocation().ToCompactString() : TEXT("-"));
		// Things that mean the level is broken even though nothing crashed.
		if (Player && Player->GetActorLocation().Z < -5000.f)
		{
			UE_LOG(LogGB, Error, TEXT("SMOKE [%s] the player fell out of the world (no floor under the start?)"), Label);
		}
		if (Enemies == 0 && FCString::Strcmp(Label, TEXT("start")) == 0)
		{
			UE_LOG(LogGB, Error, TEXT("SMOKE [%s] there are no enemies in the level (was the test arena built?)"), Label);
		}
	}

	static void StartPathCheck(UWorld* World, AGBPlayerCharacter* Player)
	{
		UNavigationSystemV1* Nav = UNavigationSystemV1::GetCurrent(World);
		const bool bNavData = Nav && Nav->GetDefaultNavDataInstance(FNavigationSystem::DontCreate) != nullptr;
		UE_LOG(LogGB, Display, TEXT("SMOKE navigation system=%d navData=%d"), Nav ? 1 : 0, bNavData ? 1 : 0);

		bPathOrcSpawned = false;
		PathOrc.Reset();
		bool bHasArena = false;
		for (TActorIterator<AGBTestArena> It(World); It; ++It)
		{
			bHasArena = true;
			break;
		}
		if (!bHasArena)
		{
			UE_LOG(LogGB, Warning, TEXT("SMOKE path check skipped: this map has no test arena"));
			return;
		}

		FVector EnemySpawn;
		FVector PlayerSpot;
		AGBTestArena::GetPathTestPoints(EnemySpawn, PlayerSpot);
		if (FVector::Dist2D(Player->GetActorLocation(), PlayerSpot) > PathCheckPlayerTolerance)
		{
			Player->SetActorLocation(PlayerSpot, false, nullptr, ETeleportType::TeleportPhysics);
		}
		Player->GetAttributes()->SetInvulnerable(true);

		AGBEnemyCharacter* Orc = AGBEnemyCharacter::SpawnEnemy(World, PathOrcId, EnemySpawn, FRotator::ZeroRotator, EGBEraPresence::Both);
		if (!Orc)
		{
			UE_LOG(LogGB, Error, TEXT("SMOKE path check: could not spawn %s"), *PathOrcId.ToString());
			Player->GetAttributes()->SetInvulnerable(false);
			return;
		}
		PathOrc = Orc;
		bPathOrcSpawned = true;

		const FVector From = Orc->GetActorLocation();
		const FVector To = Player->GetActorLocation();
		PathStartDistance = FVector::Dist2D(From, To);

		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(GBSmokePathCheck), false, Orc);
		Params.AddIgnoredActor(Player);
		const bool bBlocked = World->LineTraceSingleByChannel(Hit, From, To, ECC_Visibility, Params);

		int32 PathValid = 0;
		int32 PathPartial = 0;
		double PathLength = 0.0;
		if (bNavData)
		{
			if (const UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(World, From, To, Orc))
			{
				PathValid = Path->IsValid() ? 1 : 0;
				PathPartial = Path->IsPartial() ? 1 : 0;
				PathLength = Path->GetPathLength();
			}
		}
		const FString BlockerName = bBlocked ? GetNameSafe(Hit.GetActor()) : FString(TEXT("nothing"));
		UE_LOG(LogGB, Display, TEXT("SMOKE path check start: orc at %s, player at %s, distance=%.0f straightLineBlocked=%d (by %s) navPath=%d partial=%d pathLength=%.0f"),
			*From.ToCompactString(), *To.ToCompactString(), PathStartDistance, bBlocked ? 1 : 0, *BlockerName, PathValid, PathPartial, PathLength);
		if (!bBlocked)
		{
			UE_LOG(LogGB, Error, TEXT("SMOKE path check: nothing blocks the straight line from the orc to the player (is the test wall missing?)"));
		}
	}

	static void EndPathCheck(UWorld* World, AGBPlayerCharacter* Player)
	{
		Player->GetAttributes()->SetInvulnerable(false);
		if (!bPathOrcSpawned)
		{
			return;
		}
		bPathOrcSpawned = false;

		AGBEnemyCharacter* Orc = PathOrc.Get();
		PathOrc.Reset();
		if (!Orc || Orc->IsDead())
		{
			UE_LOG(LogGB, Error, TEXT("SMOKE path check: the orc is gone or dead before the end of the check"));
			if (Orc)
			{
				Orc->Destroy();
			}
			return;
		}

		const float Distance = FVector::Dist2D(Orc->GetActorLocation(), Player->GetActorLocation());
		const float Reach = Orc->GetStats()->GetStat(GBStat::AttackRange, 0.f) + PathCheckReachMargin;
		UE_LOG(LogGB, Display, TEXT("SMOKE path check end: distance=%.0f (start %.0f, needed <= %.0f) orc at %s, %s"),
			Distance, PathStartDistance, Reach, *Orc->GetActorLocation().ToCompactString(), *Orc->DescribeMovement());
		if (Distance > Reach)
		{
			UE_LOG(LogGB, Error, TEXT("SMOKE path check FAILED: the orc did not get around the wall to the player (distance %.0f > %.0f)"), Distance, Reach);
		}
		else
		{
			UE_LOG(LogGB, Display, TEXT("SMOKE path check passed"));
		}
		Orc->Destroy();
	}

	// =====================================================================================================================
	// Seal / era block / caption / navigation checks (gb.Smoke seal).
	// A state machine driven by its own ticker, one phase per frame: each phase waits for a state (a caption
	// ended, a travel finished, a navmesh rebuilt) with a bounded timeout, so a frame hitch cannot fail correct code
	// and a real failure always ends the run with an error line.
	// =====================================================================================================================
	struct FPathProbe
	{
		bool bValid = false;
		bool bPartial = false;
		double Length = 0.0;
	};

	static FPathProbe ProbePath(UWorld* World, const FVector& From, const FVector& To)
	{
		FPathProbe Result;
		if (const UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(World, From, To, nullptr))
		{
			Result.bValid = Path->IsValid();
			Result.bPartial = Path->IsPartial();
			Result.Length = Path->GetPathLength();
		}
		return Result;
	}

	/** A whole path that is about as long as the straight line. */
	static bool PathIsDirect(const FPathProbe& Probe, double Straight) { return Probe.bValid && !Probe.bPartial && Probe.Length <= Straight * 1.3; }
	/** No whole path, or one that has to go a long way round. */
	static bool PathIsBlocked(const FPathProbe& Probe, double Straight) { return !Probe.bValid || Probe.bPartial || Probe.Length > Straight * 1.6; }

	static void SmokeQuitNow(const TCHAR* Who)
	{
		UE_LOG(LogGB, Display, TEXT("SMOKE %s finished, quitting"), Who);
		UE_LOG(LogGB, Display, TEXT("SMOKE DONE"));
		FPlatformMisc::RequestExit(false);
	}

	// ---- Seal block ----
	namespace SealRun
	{
		enum EPhase : int32
		{
			Setup, WaitClosed, UseSeal, WaitSkippable, WaitFuture, WaitFuturePath, WaitPast, WaitPastBlocked, WaitRemoved, CancelTest, DropTest, Finish
		};
		static FTSTicker::FDelegateHandle Ticker;
		static int32 Phase = Setup;
		static bool bEntered = false;
		static double PhaseStart = 0.0;
		static bool bFailed = false;
		static TWeakObjectPtr<AGBSeal> Seal;
		static TArray<TWeakObjectPtr<AGBEraBlock>> Walls;
		static TWeakObjectPtr<AGBEraBlock> Door;
		static FVector PointA = FVector::ZeroVector;
		static FVector PointB = FVector::ZeroVector;
		static double Straight = 0.0;
		static bool bPlainInvulnerableBefore = false;
		static const FName DoorFlag(TEXT("Smoke.SiteBlockOff"));
		static const FName GunFlag(TEXT("GotFirstGun"));

		static void Fail(const TCHAR* Message)
		{
			UE_LOG(LogGB, Error, TEXT("SMOKE seal FAILED in phase %d: %s"), Phase, Message);
			bFailed = true;
			Phase = Finish;
			bEntered = false;
		}

		static void Next(int32 To)
		{
			Phase = To;
			bEntered = false;
		}

		static bool DoorGone(const AGBEraBlock* Block)
		{
			return Block && !Block->IsPresent() && !Block->GetActorEnableCollision() && Block->IsHidden();
		}

		static bool DoorSolid(const AGBEraBlock* Block)
		{
			return Block && Block->IsPresent() && Block->GetActorEnableCollision() && !Block->IsHidden();
		}

		static bool Tick(float)
		{
			UWorld* World = SmokeWorld.Get();
			AGBPlayerCharacter* Player = World ? AGBPlayerCharacter::Get(World) : nullptr;
			UGBEraSubsystem* Era = World ? UGBEraSubsystem::Get(World) : nullptr;
			UGBProgressSubsystem* Progress = World ? UGBProgressSubsystem::Get(World) : nullptr;
			UGBCutsceneSubsystem* Cutscenes = World ? UGBCutsceneSubsystem::Get(World) : nullptr;
			if (!World || !Player || !Era || !Progress || !Cutscenes)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE seal FAILED: world, player or a subsystem is missing"));
				SmokeQuitNow(TEXT("seal"));
				return false;
			}
			const double Now = FPlatformTime::Seconds();
			if (!bEntered)
			{
				bEntered = true;
				PhaseStart = Now;
			}
			const double InPhase = Now - PhaseStart;
			const bool bTimedOut = [&](double Limit) { return InPhase > Limit; }(20.0);

			switch (Phase)
			{
			case Setup:
			{
				Era->SetEraImmediate(EGBEra::Past);
				Progress->SetFlag(GBSealName::SealUsedFlag, false);
				Progress->SetFlag(DoorFlag, false);
				bPlainInvulnerableBefore = Player->GetAttributes()->IsPlainInvulnerable();

				// Find a spot of the arena floor where a straight walk A -> B (1000 cm along X) is free, then build two walls
				// along Y with a 200 cm doorway in the middle, and put the era block in the doorway.
				const FVector Candidates[] = { FVector(800.f, -1200.f, 0.f), FVector(-400.f, -1700.f, 0.f), FVector(1000.f, 1000.f, 0.f),
					FVector(-800.f, 1500.f, 0.f), FVector(1800.f, -300.f, 0.f), FVector(300.f, -2000.f, 0.f) };
				bool bFound = false;
				FVector Center = FVector::ZeroVector;
				for (const FVector& Candidate : Candidates)
				{
					const FVector A = Candidate + FVector(-500.f, 0.f, 0.f);
					const FVector B = Candidate + FVector(500.f, 0.f, 0.f);
					const FPathProbe Probe = ProbePath(World, A, B);
					if (PathIsDirect(Probe, 1000.0))
					{
						Center = Candidate;
						PointA = A;
						PointB = B;
						bFound = true;
						break;
					}
				}
				if (!bFound)
				{
					Fail(TEXT("no free spot of the arena floor with a direct navmesh path to build the doorway on"));
					break;
				}
				Straight = 1000.0;

				auto MakeBlock = [World](const FVector& At, const FVector& Scale, EGBEraPresence Presence, FName RemovedBy) -> AGBEraBlock*
				{
					AGBEraBlock* Block = World->SpawnActor<AGBEraBlock>(AGBEraBlock::StaticClass(), FTransform(FRotator::ZeroRotator, At, Scale));
					if (Block)
					{
						Block->Configure(Presence, NAME_None, RemovedBy);
					}
					return Block;
				};
				// Cube = 100 cm. Walls: 100 thick, 400 high, 1400 long each side of a 200 cm doorway.
				Walls.Reset();
				Walls.Add(MakeBlock(Center + FVector(0.f, -800.f, 200.f), FVector(1.f, 14.f, 4.f), EGBEraPresence::Both, NAME_None));
				Walls.Add(MakeBlock(Center + FVector(0.f, 800.f, 200.f), FVector(1.f, 14.f, 4.f), EGBEraPresence::Both, NAME_None));
				Door = MakeBlock(Center + FVector(0.f, 0.f, 200.f), FVector(1.f, 2.f, 4.f), EGBEraPresence::PastOnly, DoorFlag);
				if (!Door.IsValid() || !Walls[0].IsValid() || !Walls[1].IsValid())
				{
					Fail(TEXT("could not spawn the walls or the door block"));
					break;
				}
				UE_LOG(LogGB, Display, TEXT("SMOKE seal setup: doorway at %s, walk %s -> %s, door present=%d solid=%d"),
					*Center.ToCompactString(), *PointA.ToCompactString(), *PointB.ToCompactString(), Door->IsPresent() ? 1 : 0, DoorSolid(Door.Get()) ? 1 : 0);
				Next(WaitClosed);
				break;
			}
			case WaitClosed:
			{
				const FPathProbe Probe = ProbePath(World, PointA, PointB);
				if (PathIsBlocked(Probe, Straight))
				{
					UE_LOG(LogGB, Display, TEXT("SMOKE seal nav: doorway closed by the block: valid=%d partial=%d length=%.0f (straight %.0f) after %.1f s"), Probe.bValid ? 1 : 0, Probe.bPartial ? 1 : 0, Probe.Length, Straight, InPhase);
					Next(UseSeal);
				}
				else if (bTimedOut)
				{
					Fail(TEXT("the navmesh never closed the doorway while the block is present (is the navmesh dynamic?)"));
				}
				break;
			}
			case UseSeal:
			{
				Seal = World->SpawnActor<AGBSeal>(AGBSeal::StaticClass(), Player->GetActorLocation() + FVector(0.f, 0.f, -60.f), FRotator::ZeroRotator);
				if (!Seal.IsValid())
				{
					Fail(TEXT("could not spawn the seal"));
					break;
				}
				const bool bCanBefore = Seal->CanInteract(Player);
				Seal->Interact(Player);
				const bool bUsed = Progress->HasFlag(GBSealName::SealUsedFlag);
				const bool bCanAfter = Seal->CanInteract(Player);
				const bool bLocked = Cutscenes->IsPlaying() && !Player->IsControlEnabled() && Player->GetAttributes()->IsInvulnerable() && Player->GetEquipment()->IsSwitchLocked();
				UE_LOG(LogGB, Display, TEXT("SMOKE seal used: could_before=%d flag=%d can_after=%d cutscene=%d locked=%d travelling=%d line=\"%s\""),
					bCanBefore ? 1 : 0, bUsed ? 1 : 0, bCanAfter ? 1 : 0, Cutscenes->IsPlaying() ? 1 : 0, bLocked ? 1 : 0, Era->IsTravelling() ? 1 : 0, *Cutscenes->GetLineText());
				if (!bCanBefore || !bUsed || bCanAfter || !bLocked || Era->IsTravelling())
				{
					Fail(TEXT("expected the seal usable once, SealUsed set, the caption playing with the player locked, and no travel before the caption ends"));
					break;
				}
				Cutscenes->RequestSkip();
				if (!Cutscenes->IsPlaying())
				{
					Fail(TEXT("the caption could be skipped before the skip delay"));
					break;
				}
				Next(WaitSkippable);
				break;
			}
			case WaitSkippable:
			{
				if (Cutscenes->CanSkip())
				{
					Cutscenes->RequestSkip();
					const bool bOk = !Cutscenes->IsPlaying() && (Era->IsTravelling() || Era->GetCurrentEra() == EGBEra::Future);
					UE_LOG(LogGB, Display, TEXT("SMOKE seal skipped after %.1f s: caption over=%d travel started=%d"), InPhase, !Cutscenes->IsPlaying() ? 1 : 0, Era->IsTravelling() ? 1 : 0);
					if (!bOk)
					{
						Fail(TEXT("after the skip the caption must be over and the travel to the future started"));
						break;
					}
					Next(WaitFuture);
				}
				else if (InPhase > 10.0)
				{
					Fail(TEXT("the caption never became skippable"));
				}
				break;
			}
			case WaitFuture:
			{
				if (Era->GetCurrentEra() == EGBEra::Future && !Era->IsTravelling())
				{
					const bool bControl = Player->IsControlEnabled() && !Cutscenes->IsPlaying();
					const bool bLocksBack = Player->GetAttributes()->IsInvulnerable() == bPlainInvulnerableBefore && !Player->GetEquipment()->IsSwitchLocked();
					const bool bGone = DoorGone(Door.Get());
					const bool bSealDone = Progress->HasFlag(GBSealName::SealUsedFlag) && Seal.IsValid() && !Seal->CanInteract(Player);
					UE_LOG(LogGB, Display, TEXT("SMOKE seal future after %.1f s: control restored=%d locks back=%d door gone=%d seal done=%d"), InPhase, bControl ? 1 : 0, bLocksBack ? 1 : 0, bGone ? 1 : 0, bSealDone ? 1 : 0);
					if (!bControl || !bLocksBack || !bGone || !bSealDone)
					{
						Fail(TEXT("in the future: control must be back after the caption and the travel, invulnerability and weapon lock as before, the past-only door gone, the seal unusable"));
						break;
					}
					Next(WaitFuturePath);
				}
				else if (bTimedOut)
				{
					Fail(TEXT("the travel to the future never finished"));
				}
				break;
			}
			case WaitFuturePath:
			{
				const FPathProbe Probe = ProbePath(World, PointA, PointB);
				if (PathIsDirect(Probe, Straight))
				{
					UE_LOG(LogGB, Display, TEXT("SMOKE seal nav: doorway open in the future: length=%.0f (straight %.0f) after %.1f s"), Probe.Length, Straight, InPhase);
					Era->RequestTravel(EGBEra::Past, Player, false);
					Next(WaitPast);
				}
				else if (InPhase > 30.0)
				{
					Fail(TEXT("the navmesh never opened the doorway in the future"));
				}
				break;
			}
			case WaitPast:
			{
				if (Era->GetCurrentEra() == EGBEra::Past && !Era->IsTravelling())
				{
					const bool bBack = DoorSolid(Door.Get());
					UE_LOG(LogGB, Display, TEXT("SMOKE seal past again after %.1f s: door back and solid=%d"), InPhase, bBack ? 1 : 0);
					if (!bBack)
					{
						Fail(TEXT("back in the past the door block must be present and solid again"));
						break;
					}
					Next(WaitPastBlocked);
				}
				else if (bTimedOut)
				{
					Fail(TEXT("the travel back to the past never finished"));
				}
				break;
			}
			case WaitPastBlocked:
			{
				const FPathProbe Probe = ProbePath(World, PointA, PointB);
				if (PathIsBlocked(Probe, Straight))
				{
					UE_LOG(LogGB, Display, TEXT("SMOKE seal nav: doorway closed again in the past after %.1f s"), InPhase);
					Progress->SetFlag(DoorFlag, true);
					const bool bRemoved = DoorGone(Door.Get());
					UE_LOG(LogGB, Display, TEXT("SMOKE seal door removed by its flag=%d"), bRemoved ? 1 : 0);
					if (!bRemoved)
					{
						Fail(TEXT("the door block must be gone once its RemovedByFlag is set"));
						break;
					}
					Next(WaitRemoved);
				}
				else if (InPhase > 30.0)
				{
					Fail(TEXT("the navmesh never closed the doorway again in the past"));
				}
				break;
			}
			case WaitRemoved:
			{
				const FPathProbe Probe = ProbePath(World, PointA, PointB);
				if (PathIsDirect(Probe, Straight))
				{
					UE_LOG(LogGB, Display, TEXT("SMOKE seal nav: doorway open after the flag: length=%.0f after %.1f s"), Probe.Length, InPhase);
					Next(CancelTest);
				}
				else if (InPhase > 30.0)
				{
					Fail(TEXT("the navmesh never opened the doorway after the removing flag"));
				}
				break;
			}
			case CancelTest:
			{
				// A caption cancelled (what the player's death does) gives the seal back, runs no travel, and leaves a lock someone else owns alone.
				Progress->SetFlag(GBSealName::SealUsedFlag, false);
				Player->GetEquipment()->SetSwitchLocked(true);
				AGBSeal* TheSeal = Seal.Get();
				if (!TheSeal || !TheSeal->CanInteract(Player))
				{
					Fail(TEXT("the seal should be usable again after SealUsed was cleared"));
					break;
				}
				TheSeal->Interact(Player);
				const bool bPlaying = Cutscenes->IsPlaying();
				Cutscenes->Cancel();
				const bool bReset = !Progress->HasFlag(GBSealName::SealUsedFlag) && TheSeal->CanInteract(Player);
				const bool bNoTravel = !Era->IsTravelling() && Era->GetCurrentEra() == EGBEra::Past;
				const bool bFree = !Cutscenes->IsPlaying() && Player->IsControlEnabled() && !Player->GetAttributes()->IsInvulnerable() == !bPlainInvulnerableBefore;
				const bool bOtherLockKept = Player->GetEquipment()->IsSwitchLocked();
				Player->GetEquipment()->SetSwitchLocked(false);
				const bool bLockClean = !Player->GetEquipment()->IsSwitchLocked();
				UE_LOG(LogGB, Display, TEXT("SMOKE seal cancelled: was playing=%d seal reset=%d no travel=%d player free=%d other lock kept=%d lock clean after=%d"),
					bPlaying ? 1 : 0, bReset ? 1 : 0, bNoTravel ? 1 : 0, bFree ? 1 : 0, bOtherLockKept ? 1 : 0, bLockClean ? 1 : 0);
				if (!bPlaying || !bReset || !bNoTravel || !bFree || !bOtherLockKept || !bLockClean)
				{
					Fail(TEXT("a cancelled caption must reset the seal, not travel, free the player, and leave another system's weapon lock alone"));
					break;
				}
				Next(DropTest);
				break;
			}
			case DropTest:
			{
				const UGBDataSubsystem* Data = UGBDataSubsystem::Get(World);
				const FGBEnemyDefinition* Carrier = Data ? Data->FindEnemy(FName(TEXT("Orc_Future_GunCarrier"))) : nullptr;
				if (!Carrier)
				{
					Fail(TEXT("no Orc_Future_GunCarrier data"));
					break;
				}
				Progress->SetFlag(GunFlag, false);
				TArray<AGBPickup*> Before;
				for (TActorIterator<AGBPickup> It(World); It; ++It)
				{
					Before.Add(*It);
				}
				const TArray<FGBDropDefinition> Drops = Carrier->Drops;
				const int32 Spawned = AGBPickup::SpawnDrops(World, Drops, Player->GetActorLocation() + FVector(400.f, 0.f, 0.f));
				AGBPickup* Drop = nullptr;
				for (TActorIterator<AGBPickup> It(World); It; ++It)
				{
					if (!Before.Contains(*It))
					{
						Drop = *It;
					}
				}
				const bool bFlagged = Drop && Drop->FlagOnPickup == GunFlag;
				if (Drop)
				{
					Drop->Interact(Player);
				}
				const bool bFlagSet = Progress->HasFlag(GunFlag);
				const bool bCutscene = Cutscenes->IsPlaying() && Cutscenes->GetPlayingId() == FName(TEXT("FirstGun"));
				Cutscenes->Stop();
				// The flag cleared and set again must not replay the caption (once per run).
				Progress->SetFlag(GunFlag, false);
				Progress->SetFlag(GunFlag, true);
				const bool bReplayed = Cutscenes->IsPlaying();
				UE_LOG(LogGB, Display, TEXT("SMOKE seal drop: spawned=%d flagged=%d flag set=%d first gun caption=%d replayed after clear+set=%d"), Spawned, bFlagged ? 1 : 0, bFlagSet ? 1 : 0, bCutscene ? 1 : 0, bReplayed ? 1 : 0);
				Cutscenes->Cancel();
				Progress->SetFlag(GunFlag, false);
				if (Drop && IsValid(Drop))
				{
					Drop->Destroy();
				}
				if (Spawned != 1 || !bFlagged || !bFlagSet || !bCutscene || bReplayed)
				{
					Fail(TEXT("expected one pistol pickup carrying GotFirstGun, the flag set on pickup, the FirstGun caption once, and no replay"));
					break;
				}
				Next(Finish);
				break;
			}
			case Finish:
			default:
			{
				if (AGBSeal* TheSeal = Seal.Get())
				{
					TheSeal->Destroy();
				}
				for (TWeakObjectPtr<AGBEraBlock>& Wall : Walls)
				{
					if (Wall.IsValid())
					{
						Wall->Destroy();
					}
				}
				if (Door.IsValid())
				{
					Door->Destroy();
				}
				Progress->SetFlag(DoorFlag, false);
				Progress->SetFlag(GBSealName::SealUsedFlag, false);
				Progress->SetFlag(GunFlag, false);
				Cutscenes->Cancel();
				UE_LOG(LogGB, Display, TEXT("SMOKE seal block %s"), bFailed ? TEXT("FAILED") : TEXT("passed"));
				SmokeQuitNow(TEXT("seal"));
				return false;
			}
			}
			return true;
		}

		static void Start(UWorld*, AGBPlayerCharacter*)
		{
			Phase = Setup;
			bEntered = false;
			bFailed = false;
			FTSTicker::GetCoreTicker().RemoveTicker(Ticker);
			Ticker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&Tick));
		}
	}

	// ---- Map tour block (gb.Smoke tour) ----
	// A real map marks its review spots with actors tagged "GBTour"; their second tag is the spot's name. The block
	// puts the player on each spot in name order, lets the view settle, checks that the player stands on something,
	// and takes a screenshot through the real game camera: what a person will see when playing that map.
	// A map without tour spots (the test arena) skips the block.
	namespace TourRun
	{
		static FTSTicker::FDelegateHandle Ticker;
		static TArray<FTransform> Spots;
		static TArray<FString> Names;
		static int32 Index = 0;
		static double PhaseStart = 0.0;
		static bool bPlaced = false;
		static bool bShot = false;
		static bool bCast = false;
		static int32 Failures = 0;
		static TWeakObjectPtr<AGBBreakable> BreakTarget;
		static int32 BreakHits = 0;
		static bool bBreakFound = false;
		static bool bBreakStoodThreeHits = false;
		static float LadderRise = 0.f;
		static FVector LadderOtherEnd = FVector::ZeroVector;
		static bool bLadderChecked = false;
		// The "vaulttest" spot: where the push began, the facing, whether a vault ever ran, and why FindVault refused (if it did).
		static FVector VaultStart = FVector::ZeroVector;
		static FVector VaultFacing = FVector::ForwardVector;
		static bool bVaultSeen = false;
		static bool bVaultInputCleared = false;
		static bool bVaultFound = false;
		static FString VaultWhy;

		/** The tour leaves no energy gate open behind it: the flags its socket checks set are cleared. */
		static void ClearGateFlags(UWorld* World)
		{
			UGBProgressSubsystem* Progress = UGBProgressSubsystem::Get(World);
			for (TActorIterator<AGBEnergyGate> It(World); Progress && It; ++It)
			{
				for (const FName& Flag : It->RequiredFlags)
				{
					Progress->SetFlag(Flag, false);
				}
			}
		}

		static bool Tick(float)
		{
			UWorld* World = SmokeWorld.Get();
			AGBPlayerCharacter* Player = World ? AGBPlayerCharacter::Get(World) : nullptr;
			if (!World || !Player)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE tour FAILED: no world or no player"));
				SmokeQuitNow(TEXT("tour"));
				return false;
			}
			if (Index >= Spots.Num())
			{
				ClearGateFlags(World);
				UE_LOG(LogGB, Display, TEXT("SMOKE tour block %s: %d spots, %d without floor"), Failures == 0 ? TEXT("passed") : TEXT("FAILED"), Spots.Num(), Failures);
				SmokeQuitNow(TEXT("tour"));
				return false;
			}
			const double Now = FPlatformTime::Seconds();
			UCharacterMovementComponent* Move = Player->GetCharacterMovement();
			const float HalfHeight = Player->GetSimpleCollisionHalfHeight();
			if (!bPlaced)
			{
				const FTransform& Spot = Spots[Index];
				Player->CancelMantle();          // a climb still running from the last spot would carry the player back
				Player->ClearMoveInputForTest();
				LadderRise = 0.f;
				bLadderChecked = false;
				bVaultSeen = false;
				bVaultInputCleared = false;
				bVaultFound = false;
				VaultWhy.Reset();
				Player->SetActorLocation(Spot.GetLocation() + FVector(0.f, 0.f, HalfHeight + 5.f), false, nullptr, ETeleportType::TeleportPhysics);
				if (AGBPlayerController* PC = Cast<AGBPlayerController>(Player->GetController()))
				{
					PC->SetControlRotation(Spot.Rotator());
				}
				if (Move)
				{
					Move->Velocity = FVector::ZeroVector;
				}
				bPlaced = true;
				bShot = false;
				bCast = false;
				PhaseStart = Now;
				return true;
			}
			const double InPhase = Now - PhaseStart;
			// A spot named "castwall": the ice wall at full strength (every quick-time step right) must stand on this
			// floor, in sight. At full size it used to rise on the roof of a low corridor (owner, 2026-11-14).
			// A spot named "breakwall": the nearest breakable takes four hits, one every 0.4 s, and must be gone when
			// the picture is taken. Its progress flag is taken off first, so the run leaves no broken wall behind.
			const bool bBreakWall = Names[Index].Contains(TEXT("breakwall"));
			if (bBreakWall && InPhase > 0.5 + 0.4 * BreakHits && BreakHits < 4)
			{
				if (BreakHits == 0)
				{
					BreakTarget.Reset();
					float Best = TNumericLimits<float>::Max();
					for (TActorIterator<AGBBreakable> It(World); It; ++It)
					{
						const float Dist = static_cast<float>(FVector::DistSquared(It->GetActorLocation(), Player->GetActorLocation()));
						if (Dist < Best)
						{
							Best = Dist;
							BreakTarget = *It;
						}
					}
					bBreakFound = BreakTarget.IsValid();
					bBreakStoodThreeHits = false;
					if (BreakTarget.IsValid())
					{
						BreakTarget->BrokenFlag = NAME_None;
					}
				}
				if (BreakHits == 3)
				{
					bBreakStoodThreeHits = BreakTarget.IsValid() && BreakTarget->GetHits() == 3;     // still whole, and it counted each one
				}
				++BreakHits;
				if (BreakTarget.IsValid())
				{
					FGBDamageInfo Hit;
					Hit.Amount = 20.f;
					Hit.Instigator = Player;
					Hit.HitLocation = BreakTarget->GetActorLocation();
					Hit.SourceId = TEXT("SmokeTour");
					UGBCombatLibrary::DealDamage(BreakTarget.Get(), Hit);
				}
			}
			// A spot named "laddertest" stands at one end of a ladder: the nearest end is used, and before the tour moves
			// on the player must have arrived, on its feet, at the height of the other end.
			const bool bLadderTest = Names[Index].Contains(TEXT("laddertest"));
			if (bLadderTest && !bCast && InPhase > 0.5)
			{
				bCast = true;
				AGBLadder* Nearest = nullptr;
				float Best = TNumericLimits<float>::Max();
				for (TActorIterator<AGBLadder> It(World); It; ++It)
				{
					const float Dist = static_cast<float>(FVector::DistSquared(It->GetActorLocation(), Spots[Index].GetLocation()));
					if (Dist < Best)
					{
						Best = Dist;
						Nearest = *It;
					}
				}
				if (Nearest && Best < FMath::Square(150.f))
				{
					LadderRise = Nearest->Rise;
					LadderOtherEnd = Nearest->GetOtherEnd();
					Nearest->Interact(Player);
				}
			}
			if (bLadderTest && !bLadderChecked && InPhase > 3.4)
			{
				bLadderChecked = true;
				const float Climbed = static_cast<float>(Player->GetActorLocation().Z - HalfHeight - Spots[Index].GetLocation().Z);
				const FVector Feet = Player->GetActorLocation() - FVector(0.f, 0.f, HalfHeight);
				const bool bOnItsFeet = Move && Move->IsMovingOnGround();
				const bool bAtTheOtherEnd = FVector::Dist2D(Feet, LadderOtherEnd) < 60.f;
				if (!FMath::IsNearlyZero(LadderRise) && !Player->IsMantling() && FMath::Abs(Climbed - LadderRise) < 40.f && bOnItsFeet && bAtTheOtherEnd)
				{
					UE_LOG(LogGB, Display, TEXT("SMOKE tour %s: the ladder carried the player %.0f cm and set it down"), *Names[Index], Climbed);
				}
				else
				{
					++Failures;
					UE_LOG(LogGB, Error, TEXT("SMOKE tour %s: the ladder did not work (ladder rise %.0f, player moved %.0f cm, still climbing: %d, on its feet: %d, at the other end: %d)"),
						*Names[Index], LadderRise, Climbed, Player->IsMantling() ? 1 : 0, bOnItsFeet ? 1 : 0, bAtTheOtherEnd ? 1 : 0);
				}
			}
			// A spot named "vaulttest" stands in front of a railing: forward input is held (the push, no jump press) from 0.5 s
			// to 2.0 s; the player must start a vault and end more than 100 cm beyond his start, on his feet.
			const bool bVaultTest = Names[Index].Contains(TEXT("vaulttest"));
			if (bVaultTest)
			{
				if (!bCast && InPhase > 0.5)
				{
					bCast = true;
					VaultStart = Player->GetActorLocation();
					VaultFacing = FRotator(0.f, Spots[Index].Rotator().Yaw, 0.f).Vector();
					AGBPlayerCharacter::FGBVaultPlan Plan;
					bVaultFound = Player->FindVault(Plan, true, FVector2D(VaultFacing.X, VaultFacing.Y), &VaultWhy);
					Player->SetMoveInputForTest(FVector2D(0.f, 1.f));
				}
				bVaultSeen = bVaultSeen || Player->IsVaulting();
				if (bCast && !bVaultInputCleared && InPhase > 2.0)
				{
					bVaultInputCleared = true;
					Player->ClearMoveInputForTest();
				}
			}
			// A spot named "gatetest" stands by one socket of an energy gate: the player is handed one power cell there.
			const bool bGateTest = Names[Index].Contains(TEXT("gatetest"));
			if (bGateTest && !bCast && InPhase > 0.8)
			{
				bCast = true;
				Player->GetInventory()->AddItem(TEXT("PowerCell"), 1);
			}
			const bool bCastWall = Names[Index].Contains(TEXT("castwall"));
			if (bCastWall && !bCast && InPhase > 1.0)
			{
				bCast = true;
				BreakHits = 0;
				for (TActorIterator<AGBSpell_Wall> It(World); It; ++It)
				{
					It->Destroy();          // a wall left by an earlier spot would be counted with this one
				}
				SetQTEEnabled(false);
				Player->GetEquipment()->SwitchTo(EGBHeldSlot::Staff);
				Player->GetSpellbook()->DebugCastAtFullStrength(TEXT("IceWall"));
			}
			// The vault spot waits longer: the push, the move and a fall to the floor beyond come first.
			const double ShotAt = bVaultTest ? 4.0 : 2.5;
			if (!bShot && InPhase > ShotAt) // lighting and exposure need a moment after a jump of many metres
			{
				const FVector Feet = Player->GetActorLocation() - FVector(0.f, 0.f, HalfHeight);
				FHitResult Hit;
				FCollisionQueryParams Params(SCENE_QUERY_STAT(GBSmokeTourFloor), false, Player);
				const bool bHit = World->LineTraceSingleByChannel(Hit, Feet + FVector(0.f, 0.f, 20.f), Feet - FVector(0.f, 0.f, 200.f), ECC_Visibility, Params);
				const bool bFalling = Move && Move->IsFalling();
				const float Drop = static_cast<float>(Spots[Index].GetLocation().Z - Feet.Z);
				if (bLadderTest)
				{
					// In the middle of its climb when the picture is taken: judged by the ladder check, not by the floor.
				}
				else if (bVaultTest)
				{
					Player->ClearMoveInputForTest();
					const FVector Moved = Player->GetActorLocation() - VaultStart;
					const float Beyond = static_cast<float>(Moved.X * VaultFacing.X + Moved.Y * VaultFacing.Y);
					const bool bOnItsFeet = Move && Move->IsMovingOnGround() && !Player->IsMantling();
					if (bVaultSeen && Beyond > 100.f && bOnItsFeet)
					{
						UE_LOG(LogGB, Display, TEXT("SMOKE tour %s: the push vaulted the railing, %.0f cm beyond the start, on its feet"), *Names[Index], Beyond);
					}
					else
					{
						++Failures;
						UE_LOG(LogGB, Error, TEXT("SMOKE tour %s: the vault did not work (got %.0f cm beyond the start, needs over 100; a vault ever started: %d; on its feet: %d; FindVault from the start spot: %s, %s)"),
							*Names[Index], Beyond, bVaultSeen ? 1 : 0, bOnItsFeet ? 1 : 0, bVaultFound ? TEXT("found one") : TEXT("refused"), *VaultWhy);
					}
				}
				else if (bHit && !bFalling && FMath::Abs(Drop) < 60.f)
				{
					UE_LOG(LogGB, Display, TEXT("SMOKE tour %s: standing on %s, %.0f cm from the marked height"), *Names[Index], *GetNameSafe(Hit.GetActor()), Drop);
				}
				else
				{
					++Failures;
					UE_LOG(LogGB, Error, TEXT("SMOKE tour %s: no floor where the map marks one (hit=%d falling=%d, feet %.0f cm below the mark)"), *Names[Index], bHit ? 1 : 0, bFalling ? 1 : 0, Drop);
				}
				if (bBreakWall)
				{
					if (bBreakFound && bBreakStoodThreeHits && BreakHits == 4 && (!BreakTarget.IsValid() || BreakTarget->IsBroken()))
					{
						UE_LOG(LogGB, Display, TEXT("SMOKE tour %s: the breakable stood three hits and broke at the fourth (%d pieces thrown)"), *Names[Index],
							BreakTarget.IsValid() ? BreakTarget->GetShardCountAlive() : 0);
					}
					else
					{
						++Failures;
						UE_LOG(LogGB, Error, TEXT("SMOKE tour %s: the breakable did not break as it should (found: %d, whole after three hits: %d, hits dealt: %d, still there: %d)"),
							*Names[Index], bBreakFound ? 1 : 0, bBreakStoodThreeHits ? 1 : 0, BreakHits, BreakTarget.IsValid() ? 1 : 0);
					}
				}
				if (bGateTest)
				{
					const AGBKeySocket* Nearest = nullptr;
					float Best = TNumericLimits<float>::Max();
					for (TActorIterator<AGBKeySocket> It(World); It; ++It)
					{
						const float Dist = static_cast<float>(FVector::DistSquared(It->GetActorLocation(), Player->GetActorLocation()));
						if (Dist < Best)
						{
							Best = Dist;
							Nearest = *It;
						}
					}
					const UGBProgressSubsystem* Progress = UGBProgressSubsystem::Get(World);
					const bool bFlagSet = Nearest && Progress && Progress->HasFlag(Nearest->Flag);
					if (!Player->GetInventory()->HasItem(TEXT("PowerCell")) && Nearest && Nearest->IsFilled() && bFlagSet)
					{
						UE_LOG(LogGB, Display, TEXT("SMOKE tour %s: the socket took the power cell and set its flag"), *Names[Index]);
					}
					else
					{
						++Failures;
						UE_LOG(LogGB, Error, TEXT("SMOKE tour %s: the socket did not take the power cell"), *Names[Index]);
						Player->GetInventory()->RemoveItem(TEXT("PowerCell"), 1);
					}
				}
				if (Names[Index].Contains(TEXT("gatecheck")))
				{
					// After both sockets: the gate must be out. Its flags are cleared again, so the run leaves no open gate behind.
					int32 Gates = 0;
					int32 Out = 0;
					for (TActorIterator<AGBEnergyGate> It(World); It; ++It)
					{
						++Gates;
						Out += It->IsOut() ? 1 : 0;
					}
					ClearGateFlags(World);
					if (Gates > 0 && Out == Gates)
					{
						UE_LOG(LogGB, Display, TEXT("SMOKE tour %s: the energy gate went out with both sockets filled"), *Names[Index]);
					}
					else
					{
						++Failures;
						UE_LOG(LogGB, Error, TEXT("SMOKE tour %s: the energy gate is still up (%d of %d out)"), *Names[Index], Out, Gates);
					}
				}
				if (Names[Index].Contains(TEXT("sealmap")))
				{
					// A seal that sends the player to another map: that map must be one this build can open (in a
					// package it has to be cooked). Without this the seal falls back to the travel in place, silently.
					int32 Seals = 0;
					int32 Ready = 0;
					FString Map;
					for (TActorIterator<AGBSeal> It(World); It; ++It)
					{
						++Seals;
						Ready += It->HasTravelMap() ? 1 : 0;
						Map = It->TravelMapPath;
					}
					if (Seals > 0 && Ready == Seals)
					{
						UE_LOG(LogGB, Display, TEXT("SMOKE tour %s: the seal's map %s can be opened"), *Names[Index], *Map);
					}
					else
					{
						++Failures;
						UE_LOG(LogGB, Error, TEXT("SMOKE tour %s: %d of %d seals have a map that can be opened ('%s')"), *Names[Index], Ready, Seals, *Map);
					}
				}
				if (bCastWall)
				{
					int32 Walls = 0;
					float BaseOff = 0.f;
					float Height = 0.f;
					for (TActorIterator<AGBSpell_Wall> It(World); It; ++It)
					{
						FVector Origin, Extent;
						It->GetActorBounds(false, Origin, Extent);
						BaseOff = static_cast<float>(Origin.Z - Extent.Z - Feet.Z);
						Height = static_cast<float>(Extent.Z * 2.0);
						++Walls;
					}
					if (Walls == 1 && FMath::Abs(BaseOff) < 40.f)
					{
						UE_LOG(LogGB, Display, TEXT("SMOKE tour %s: the ice wall at full strength (%.0f cm tall) stands on the floor, %.0f cm from the feet"), *Names[Index], Height, BaseOff);
					}
					else
					{
						++Failures;
						UE_LOG(LogGB, Error, TEXT("SMOKE tour %s: the ice wall at full strength is not on this floor (%d walls, base %.0f cm from the feet)"), *Names[Index], Walls, BaseOff);
					}
				}
				Shot(*FString::Printf(TEXT("tour_%s"), *Names[Index]));
				bShot = true;
			}
			else if (bShot && InPhase > ShotAt + 1.0)
			{
				++Index;
				bPlaced = false;
			}
			return true;
		}

		static void Start(UWorld* World, AGBPlayerCharacter*)
		{
			FTSTicker::GetCoreTicker().RemoveTicker(Ticker);
			Spots.Reset();
			Names.Reset();
			Index = 0;
			Failures = 0;
			bPlaced = false;
			bShot = false;
			// The tour looks at the map; it does not fight in it. A spot inside an encounter's trigger would start the
			// fight, and a dead player fails every check after it. The encounters are taken out for this run only.
			// Prespawned enemies stand in the world from BeginPlay: they go with their encounters (removed, not killed: no deaths counted, no drops).
			int32 RemovedEnemies = 0;
			for (TActorIterator<AGBEnemyCharacter> It(World); It; ++It)
			{
				if (It->GetEncounter())
				{
					It->OnDied.RemoveAll(It->GetEncounter());
					It->Destroy();
					++RemovedEnemies;
				}
			}
			for (TActorIterator<AGBEncounter> It(World); It; ++It)
			{
				It->Destroy();
			}
			if (AGBPlayerCharacter* TourPlayer = AGBPlayerCharacter::Get(World))
			{
				TourPlayer->GetAttributes()->SetInvulnerable(true);   // the tour looks, it is not hurt
			}
			UE_LOG(LogGB, Display, TEXT("SMOKE tour: removed %d encounter enemies with their encounters, player invulnerable"), RemovedEnemies);
			TArray<TPair<FString, FTransform>> Found;
			for (TActorIterator<AActor> It(World); It; ++It)
			{
				if (It->ActorHasTag(FName(TEXT("GBTour"))))
				{
					FString Name = It->GetName();
					for (const FName& Tag : It->Tags)
					{
						if (Tag != FName(TEXT("GBTour")) && Tag.ToString().StartsWith(TEXT("tour_")))
						{
							Name = Tag.ToString().RightChop(5);
						}
					}
					Found.Emplace(Name, It->GetActorTransform());
				}
			}
			if (Found.Num() == 0)
			{
				UE_LOG(LogGB, Display, TEXT("SMOKE tour skipped: this map has no tour spots"));
				return;
			}
			Found.Sort([](const TPair<FString, FTransform>& A, const TPair<FString, FTransform>& B) { return A.Key < B.Key; });
			for (const TPair<FString, FTransform>& Pair : Found)
			{
				Names.Add(Pair.Key);
				Spots.Add(Pair.Value);
			}
			UE_LOG(LogGB, Display, TEXT("SMOKE tour: %d spots"), Spots.Num());
			Ticker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&Tick));
		}
	}

	// ---- FogVisit block (gb.Smoke FogVisit): a visual probe of the tunnel smoke on a real map (FutureBase). For each AGBFluidFog
	// in the world: the player stands inside it, 2 m from one end, looking along it; the smoke is photographed plain, with a
	// bright light, with a dense blob injected, with shots and a blast across the view, and after; the frame time is measured
	// with the fog running and forced off. Only a run filtered on this name plays it (the full run and the arena skip it).
	namespace FogVisitRun
	{
		static bool bActive = false;
		static FTSTicker::FDelegateHandle Ticker;
		static TArray<TWeakObjectPtr<AGBFluidFog>> Fogs;
		static int32 Index = 0;
		static int32 Phase = 0;
		static double PhaseStart = 0.0;
		static FVector Floor = FVector::ZeroVector;
		static FVector Axis = FVector::ForwardVector;
		static TWeakObjectPtr<AActor> Light;
		static TArray<TPair<double, float>> Frames;
		static float FrameOn = 0.f;
		static bool RunningOn = false;
		static int32 MaxSources = 0;
		static const double PhaseTimes[] = { 0.0, 8.0, 8.3, 8.8, 9.0, 10.0, 10.3, 10.6, 12.6, 12.9, 14.9 };
		static const int32 PhaseCount = UE_ARRAY_COUNT(PhaseTimes);

		static float MeanFrameMs(double Now)
		{
			double Sum = 0.0;
			int32 Count = 0;
			for (int32 Pos = Frames.Num() - 1; Pos >= 0 && Frames[Pos].Key > Now - 2.0; --Pos)
			{
				Sum += Frames[Pos].Value;
				++Count;
			}
			return Count > 0 ? static_cast<float>(Sum / Count * 1000.0) : 0.f;
		}

		static void SetLight(UWorld* World, bool bOn)
		{
			if (!bOn)
			{
				if (Light.IsValid())
				{
					Light->Destroy();
				}
				Light.Reset();
				return;
			}
			const FVector At = Floor + Axis * 400.f + FVector(0.f, 0.f, 100.f);
			APointLight* Lamp = World->SpawnActor<APointLight>(At, FRotator::ZeroRotator);
			if (Lamp && Lamp->PointLightComponent)
			{
				UPointLightComponent* Comp = Lamp->PointLightComponent;
				Comp->SetMobility(EComponentMobility::Movable);
				Comp->SetLightColor(FLinearColor::White);
				Comp->SetIntensity(20000.f);
				Comp->SetAttenuationRadius(1500.f);
				Comp->SetCastShadows(false);
			}
			Light = Lamp;
		}

		static bool Tick(float Delta)
		{
			UWorld* World = SmokeWorld.Get();
			AGBPlayerCharacter* Player = World ? AGBPlayerCharacter::Get(World) : nullptr;
			if (!World || !Player)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE FogVisit FAILED: no world or no player"));
				SmokeQuitNow(TEXT("FogVisit"));
				return false;
			}
			const double Now = FPlatformTime::Seconds();
			Frames.Emplace(Now, Delta);
			while (Frames.Num() > 0 && Frames[0].Key < Now - 3.0)
			{
				Frames.RemoveAt(0);
			}
			AGBFluidFog* Fog = Fogs[Index].Get();
			if (!Fog)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE FogVisit %d FAILED: the fog actor went away"), Index);
				SmokeQuitNow(TEXT("FogVisit"));
				return false;
			}
			if (Phase >= 5 && Phase <= 8)
			{
				MaxSources = FMath::Max(MaxSources, Fog->GetLastSourceEntries());
			}
			if (Phase >= PhaseCount || Now - PhaseStart < PhaseTimes[Phase])
			{
				return true;
			}
			const int32 Ran = Phase++;
			const FVector Eye = Floor + FVector(0.f, 0.f, 160.f);
			const FVector Right = FVector::CrossProduct(FVector::UpVector, Axis);
			auto ShotName = [](const TCHAR* Tag) { return FString::Printf(TEXT("fogvisit_%d_%s"), Index, Tag); };
			switch (Ran)
			{
			case 0:
			{
				// Only this fog may run: a neighbour inside the active range would spoil the frame-time comparison.
				for (int32 Other = 0; Other < Fogs.Num(); ++Other)
				{
					if (AGBFluidFog* OtherFog = Fogs[Other].Get())
					{
						OtherFog->DebugForceOff(Other != Index);
					}
				}
				const FVector Extent = Fog->BoxExtent;
				const bool bAlongX = Extent.X >= Extent.Y;
				Axis = bAlongX ? FVector(1.f, 0.f, 0.f) : FVector(0.f, 1.f, 0.f);
				const float HalfLength = bAlongX ? Extent.X : Extent.Y;
				const FVector Centre = Fog->GetActorLocation();
				const FVector Start = Centre - Axis * FMath::Max(0.f, HalfLength - 200.f);
				FHitResult Hit;
				FCollisionQueryParams Params(SCENE_QUERY_STAT(GBSmokeFogVisitFloor), false, Player);
				const FVector From(Start.X, Start.Y, Centre.Z);
				const bool bHit = World->LineTraceSingleByChannel(Hit, From, From - FVector(0.f, 0.f, Extent.Z + 300.f), ECC_Visibility, Params);
				Floor = bHit ? FVector(Start.X, Start.Y, Hit.ImpactPoint.Z) : FVector(Start.X, Start.Y, Centre.Z - Extent.Z);
				const float HalfHeight = Player->GetSimpleCollisionHalfHeight();
				Player->CancelMantle();
				Player->ClearMoveInputForTest();
				Player->GetAttributes()->SetInvulnerable(true);
				Player->SetActorLocation(Floor + FVector(0.f, 0.f, HalfHeight + 5.f), false, nullptr, ETeleportType::TeleportPhysics);
				if (UCharacterMovementComponent* Move = Player->GetCharacterMovement())
				{
					Move->Velocity = FVector::ZeroVector;
				}
				if (AController* Controller = Player->GetController())
				{
					Controller->SetControlRotation(FRotator(-8.f, Axis.Rotation().Yaw, 0.f));
				}
				UE_LOG(LogGB, Display, TEXT("SMOKE FogVisit %d: placed at %s (floor %s, trace hit %d) along %s"), Index, *Fog->GetActorLocation().ToString(), *Floor.ToString(), bHit ? 1 : 0, *Axis.ToString());
				MaxSources = 0;
				break;
			}
			case 1:
				FrameOn = MeanFrameMs(Now);
				RunningOn = Fog->IsSimulating();
				UE_LOG(LogGB, Display, TEXT("SMOKE FogVisit %d: running=%d hasSystem=%d extent=%s frameMs(on, last 2 s)=%.2f playerEntries=%d"),
					Index, Fog->IsSimulating() ? 1 : 0, Fog->HasSystem() ? 1 : 0, *Fog->BoxExtent.ToString(), FrameOn, Fog->GetLastPlayerEntries());
				if (!Fog->HasSystem() || !RunningOn)
				{
					UE_LOG(LogGB, Error, TEXT("SMOKE FogVisit %d FAILED: the gas is not simulating with the player inside its box (hasSystem=%d running=%d)"), Index, Fog->HasSystem() ? 1 : 0, RunningOn ? 1 : 0);
				}
				Shot(*ShotName(TEXT("plain")));
				break;
			case 2:
				SetLight(World, true);
				break;
			case 3:
				Shot(*ShotName(TEXT("lit")));
				break;
			case 4:
				Fog->DebugInject(Floor + Axis * 300.f + FVector(0.f, 0.f, 150.f), 60.f, 20.f, FVector::ZeroVector, 30);
				UE_LOG(LogGB, Display, TEXT("SMOKE FogVisit %d: blob injected (radius 60, density 20, 30 frames)"), Index);
				break;
			case 5:
				UE_LOG(LogGB, Display, TEXT("SMOKE FogVisit %d: blob step, most source entries written in the last second=%d"), Index, MaxSources);
				if (MaxSources <= 0)
				{
					UE_LOG(LogGB, Error, TEXT("SMOKE FogVisit %d FAILED: the injected blob wrote no source entry into the gas channel"), Index);
				}
				Shot(*ShotName(TEXT("blob")));
				break;
			case 6:
			{
				for (int32 Index3 = 0; Index3 < 3; ++Index3)
				{
					const float Height = 40.f + 40.f * Index3;
					const FVector Mid = Floor + Axis * 300.f + FVector(0.f, 0.f, Height);
					UGBCombatLibrary::OnShotTraced().Broadcast(Mid - Right * 300.f, Mid + Right * 300.f, 0.f);
				}
				Fog->DebugBlast(Floor + Axis * 300.f + FVector(0.f, 0.f, 80.f));
				break;
			}
			case 7:
				Shot(*ShotName(TEXT("push")));
				break;
			case 8:
				UE_LOG(LogGB, Display, TEXT("SMOKE FogVisit %d: push step: shotEntriesTotal=%d sourceEntries=%d maxSources=%d"), Index, Fog->GetTotalShotEntriesWritten(), Fog->GetLastSourceEntries(), MaxSources);
				if (Fog->GetTotalShotEntriesWritten() <= 0)
				{
					UE_LOG(LogGB, Error, TEXT("SMOKE FogVisit %d FAILED: the three shots through the gas wrote no entry into its channel"), Index);
				}
				// The off sample must see the same lighting as the on sample (taken before the lamp came).
				SetLight(World, false);
				Shot(*ShotName(TEXT("after")));
				break;
			case 9:
				Fog->DebugForceOff(true);
				break;
			case 10:
			{
				const float FrameOff = MeanFrameMs(Now);
				UE_LOG(LogGB, Display, TEXT("SMOKE FogVisit %d: running=%d extent=%s frameMs on=%.2f off=%.2f (running while off=%d)"),
					Index, RunningOn ? 1 : 0, *Fog->BoxExtent.ToString(), FrameOn, FrameOff, Fog->IsSimulating() ? 1 : 0);
				Fog->DebugForceOff(false);
				++Index;
				if (Index >= Fogs.Num())
				{
					SmokeQuitNow(TEXT("FogVisit"));
					return false;
				}
				Phase = 0;
				PhaseStart = Now;
				break;
			}
			default:
				break;
			}
			return true;
		}

		static void Begin(UWorld* World, AGBPlayerCharacter*)
		{
			if (!bActive)
			{
				return;
			}
			FTSTicker::GetCoreTicker().RemoveTicker(Ticker);
			Fogs.Reset();
			for (TActorIterator<AGBFluidFog> It(World); It; ++It)
			{
				Fogs.Add(*It);
			}
			if (Fogs.Num() == 0)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE FogVisit FAILED: this map has no AGBFluidFog (run it on /Game/Glyphbound/Maps/FutureBase)"));
				SmokeQuitNow(TEXT("FogVisit"));
				return;
			}
			// The probe looks; it does not fight. Encounter enemies would write into the smoke and stand in the shots.
			int32 Removed = 0;
			for (TActorIterator<AGBEnemyCharacter> It(World); It; ++It)
			{
				if (It->GetEncounter())
				{
					It->OnDied.RemoveAll(It->GetEncounter());
					It->Destroy();
					++Removed;
				}
			}
			for (TActorIterator<AGBEncounter> It(World); It; ++It)
			{
				It->Destroy();
			}
			UE_LOG(LogGB, Display, TEXT("SMOKE FogVisit: %d fog boxes, %d encounter enemies removed"), Fogs.Num(), Removed);
			Index = 0;
			Phase = 0;
			PhaseStart = FPlatformTime::Seconds();
			Frames.Reset();
			Ticker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&Tick));
		}
	}

	// ---- Enemies3D block (gb.Smoke Enemies3D): the 3D enemies of the future, three groups (shooters, flyers, melee).
	struct FE3DEntry
	{
		TWeakObjectPtr<AGBEnemyCharacter> Enemy;
		FName Id;
		FVector Start = FVector::ZeroVector;
		float StartDistance = 0.f;
	};
	static TArray<FE3DEntry> E3DGroup;
	static TMap<FName, int32> E3DHits;
	static FDelegateHandle E3DHitHandle;
	static int32 E3DShotsBefore = 0;
	static bool bFilteredRun = false;

	static void E3DBindHits()
	{
		E3DHits.Reset();
		UGBCombatLibrary::OnDamageDealt().Remove(E3DHitHandle);
		E3DHitHandle = UGBCombatLibrary::OnDamageDealt().AddLambda([](AActor* Target, const FGBDamageInfo& Info, float Dealt)
		{
			if (Dealt > 0.f && Cast<AGBPlayerCharacter>(Target))
			{
				E3DHits.FindOrAdd(Info.SourceId) += 1;
			}
		});
	}

	/** Spawns the listed enemies (id, wanted distance) around the player where the player has a clear view of the spot. */
	static void E3DSpawnGroup(UWorld* World, AGBPlayerCharacter* Player, const TArray<TPair<FName, float>>& Items, bool bSpreadRing = false)
	{
		E3DGroup.Reset();
		E3DBindHits();
		GBEnemy3D::ResetShotCounters();
		AGBSkeletalEnemy::bTrackPelvis = true;
		UGBSquadSubsystem::bLogTactics = true;
		Player->GetAttributes()->SetInvulnerable(false);
		Player->GetAttributes()->RestoreAll();
		EGBEraPresence Presence = EGBEraPresence::Both;
		if (UGBEraSubsystem* Era = UGBEraSubsystem::Get(World))
		{
			Presence = Era->GetCurrentEra() == EGBEra::Past ? EGBEraPresence::PastOnly : EGBEraPresence::FutureOnly;
		}
		const FVector PlayerLoc = Player->GetActorLocation();
		const FVector EyeLoc = Player->GetAimOrigin();
		TArray<FVector> Used;
		int32 ItemIndex = 0;
		for (const TPair<FName, float>& Item : Items)
		{
			AGBEnemyCharacter* Enemy = nullptr;
			float PickedDistance = 0.f;
			for (const float Scale : { 1.f, 0.7f, 0.5f })
			{
				for (int32 K = 0; K < 12 && !Enemy; ++K)
				{
					// bSpreadRing: each member starts on a bearing of its own, 60 degrees apart (the tactics group: no two start near each other).
					const float Yaw = bSpreadRing ? (K + ItemIndex) * 60.f : (K + ItemIndex * 3) * 30.f;
					const FVector At = PlayerLoc + FVector(FMath::Cos(FMath::DegreesToRadians(Yaw)), FMath::Sin(FMath::DegreesToRadians(Yaw)), 0.f) * Item.Value * Scale;
					bool bTaken = false;
					for (const FVector& Other : Used)
					{
						bTaken |= FVector::Dist2D(Other, At) < 300.f;
					}
					FCollisionQueryParams Params(SCENE_QUERY_STAT(GBSmokeE3DLOS), false, Player);
					FHitResult Hit;
					if (bTaken || World->LineTraceSingleByChannel(Hit, EyeLoc, At + FVector(0.f, 0.f, 100.f), ECC_Visibility, Params))
					{
						continue;
					}
					Enemy = AGBEnemyCharacter::SpawnEnemy(World, Item.Key, At, FRotator(0.f, (PlayerLoc - At).Rotation().Yaw, 0.f), Presence);
					if (Enemy)
					{
						Used.Add(At);
						PickedDistance = Item.Value * Scale;
					}
				}
				if (Enemy)
				{
					break;
				}
			}
			if (Enemy)
			{
				FE3DEntry Entry;
				Entry.Enemy = Enemy;
				Entry.Id = Item.Key;
				Entry.Start = Enemy->GetActorLocation();
				Entry.StartDistance = FVector::Dist2D(Entry.Start, PlayerLoc);
				E3DGroup.Add(Entry);
			}
			else
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE Enemies3D: could not spawn %s with a clear view"), *Item.Key.ToString());
			}
			UE_LOG(LogGB, Display, TEXT("SMOKE Enemies3D spawned %s=%d at %.0f cm"), *Item.Key.ToString(), Enemy ? 1 : 0, PickedDistance);
			++ItemIndex;
		}
		E3DShotsBefore = GBEnemy3D::GetShotsFired();
	}

	/** Turns the player's view to the middle of the living group (so the screenshots show the enemies). */
	static void E3DLookAtGroup(AGBPlayerCharacter* Player)
	{
		FVector Sum = FVector::ZeroVector;
		int32 Count = 0;
		for (const FE3DEntry& Entry : E3DGroup)
		{
			if (const AGBEnemyCharacter* Enemy = Entry.Enemy.Get())
			{
				Sum += Enemy->GetActorLocation();
				++Count;
			}
		}
		if (Count == 0)
		{
			return;
		}
		const FVector Centre = Sum / Count;
		if (AGBPlayerController* PC = Cast<AGBPlayerController>(Player->GetController()))
		{
			PC->SetControlRotation((Centre - Player->GetAimOrigin()).Rotation());
		}
	}

	/** Puts the player 4 m from the enemy with this id (on the side he is already on), looking at its chest. */
	static void E3DCloseup(AGBPlayerCharacter* Player, FName Id)
	{
		for (const FE3DEntry& Entry : E3DGroup)
		{
			const AGBEnemyCharacter* Enemy = Entry.Enemy.Get();
			if (!Enemy || Entry.Id != Id)
			{
				continue;
			}
			const FVector EnemyLoc = Enemy->GetActorLocation();
			const FVector Side = (Player->GetActorLocation() - EnemyLoc).GetSafeNormal2D();
			FVector At = EnemyLoc + Side * 400.f;
			At.Z = Player->GetActorLocation().Z;
			Player->SetActorLocation(At, false, nullptr, ETeleportType::TeleportPhysics);
			if (AGBPlayerController* PC = Cast<AGBPlayerController>(Player->GetController()))
			{
				PC->SetControlRotation((EnemyLoc + FVector(0.f, 0.f, 40.f) - Player->GetAimOrigin()).Rotation());
			}
			UE_LOG(LogGB, Display, TEXT("SMOKE Enemies3D closeup of %s at %.0f cm"), *Id.ToString(), FVector::Dist(At, EnemyLoc));
			return;
		}
		UE_LOG(LogGB, Error, TEXT("SMOKE Enemies3D FAILED: no %s for the closeup"), *Id.ToString());
	}

	/** The drone's rotors and eye must sit on distinct sockets, the sentinel's core in front of its shell (not all at the origin). */
	static void E3DCheckParts(const TCHAR* GroupName, FName Id, const AGBFlyerEnemy* Flyer)
	{
		if (!Flyer->HasArtLook())
		{
			return;
		}
		const FVector Forward = Flyer->GetActorForwardVector();
		if (const AGBDroneEnemy* Drone = Cast<AGBDroneEnemy>(Flyer))
		{
			float MinApart = 1e9f;
			for (int32 A = 0; A < 4; ++A)
			{
				for (int32 B = A + 1; B < 4; ++B)
				{
					MinApart = FMath::Min(MinApart, FVector::Dist(Drone->GetRotorLocation(A), Drone->GetRotorLocation(B)));
				}
			}
			const float EyeForward = FVector::DotProduct(Drone->GetAimOrigin() - Drone->GetBodyLocation(), Forward);
			UE_LOG(LogGB, Display, TEXT("SMOKE Enemies3D %s: %s rotors at least %.0f cm apart, eye %.0f cm in front of the body"), GroupName, *Id.ToString(), MinApart, EyeForward);
			if (MinApart < 30.f || EyeForward < 5.f)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE Enemies3D %s FAILED: %s rotors %.0f cm apart (need > 30), eye %.0f cm in front (need > 5): sockets missing?"), GroupName, *Id.ToString(), MinApart, EyeForward);
			}
		}
		else if (const AGBSentinelEnemy* Sentinel = Cast<AGBSentinelEnemy>(Flyer))
		{
			const float CoreForward = FVector::DotProduct(Sentinel->GetCoreWorldLocation() - Sentinel->GetShellLocation(), Forward);
			UE_LOG(LogGB, Display, TEXT("SMOKE Enemies3D %s: %s core %.0f cm in front of the shell centre"), GroupName, *Id.ToString(), CoreForward);
			if (CoreForward < 15.f || CoreForward > 60.f)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE Enemies3D %s FAILED: %s core %.0f cm in front of the shell (expected about 30): socket missing?"), GroupName, *Id.ToString(), CoreForward);
			}
		}
	}

	/** (a) it moved, (b) it attacked (a shot, a lunge or a hit on the player), (c) flyers keep their hover height, plus the clips in use. */
	static void E3DCheckGroup(UWorld* World, AGBPlayerCharacter* Player, const TCHAR* GroupName)
	{
		(void)World;
		int32 TotalHits = 0;
		for (const FE3DEntry& Entry : E3DGroup)
		{
			AGBEnemyCharacter* Enemy = Entry.Enemy.Get();
			if (!Enemy)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE Enemies3D %s FAILED: %s is gone before the check"), GroupName, *Entry.Id.ToString());
				continue;
			}
			// Horizontal distance: a flyer that only rises has not moved (its height is checked on its own).
			const float Moved = FVector::Dist2D(Enemy->GetActorLocation(), Entry.Start);
			const FGBEnemyDefinition* Def = Enemy->GetDefinition();
			const bool bMelee = Def && Def->AttackMode.Equals(TEXT("Melee"), ESearchCase::IgnoreCase);
			const float DistanceToPlayer = FVector::Dist(Enemy->GetActorLocation(), Player->GetActorLocation());
			const int32 Hits = E3DHits.FindRef(Entry.Id);
			TotalHits += Hits;
			int32 Attacks = 0;
			FString Look;
			if (AGBSkeletalEnemy* Skeletal = Cast<AGBSkeletalEnemy>(Enemy))
			{
				// A weapon must leave its shots from a socket (Muzzle or Tip), not from the bounds fallback.
				if (Skeletal->HasSkeletalLook() && Skeletal->HasWeapon() && !Skeletal->HasMuzzleSocket())
				{
					UE_LOG(LogGB, Error, TEXT("SMOKE Enemies3D %s FAILED: %s has a weapon without a Muzzle / Tip socket"), GroupName, *Entry.Id.ToString());
				}
				Attacks = Skeletal->GetShotCount() + Skeletal->GetLungeCount();
				Look = Skeletal->DescribeLook();
				// The pelvis must stay over the capsule: the clips carry their travel in it (a walk used to run 5 to 10 m away from the body).
				if (Skeletal->HasSkeletalLook() && Skeletal->GetMaxPelvisDistance() > 60.f)
				{
					UE_LOG(LogGB, Error, TEXT("SMOKE Enemies3D %s FAILED: %s pelvis went %.0f cm from its capsule (the animation travel is not removed)"), GroupName, *Entry.Id.ToString(), Skeletal->GetMaxPelvisDistance());
				}
				if (const UGBEnemyAnimInstance* Anim = Skeletal->GetEnemyAnim())
				{
					if (Anim->GetClipCount() > 0 && Anim->GetEvaluateCount() == 0)
					{
						UE_LOG(LogGB, Error, TEXT("SMOKE Enemies3D %s FAILED: %s has clips but its animation proxy never evaluated"), GroupName, *Entry.Id.ToString());
					}
				}
			}
			else if (AGBFlyerEnemy* Flyer = Cast<AGBFlyerEnemy>(Enemy))
			{
				Attacks = Flyer->GetShotCount();
				Look = Flyer->DescribeFlyer();
				E3DCheckParts(GroupName, Entry.Id, Flyer);
				// The hover height is measured from the floor under the flyer. Crossing the edge of a course block moves that floor by
				// metres in one frame and the flyer needs a moment to climb or sink to its new height (max vertical speed, 2 s at
				// the slowest): a sample taken in that moment says nothing about the hover. It is judged once the floor held still.
				const float HeightError = FMath::Abs(Flyer->GetHeightAboveGround() - Flyer->GetWantedHeight());
				const float SinceGroundChange = Flyer->GetSecondsSinceGroundChange();
				if (HeightError > 150.f && SinceGroundChange > 3.f)
				{
					UE_LOG(LogGB, Error, TEXT("SMOKE Enemies3D %s FAILED: %s is %.0f cm above the ground, wanted %.0f (the floor under it has held still for %.1f s)"), GroupName, *Entry.Id.ToString(), Flyer->GetHeightAboveGround(), Flyer->GetWantedHeight(), SinceGroundChange);
				}
				else if (HeightError > 150.f)
				{
					UE_LOG(LogGB, Display, TEXT("SMOKE Enemies3D %s: %s is %.0f cm above the ground, wanted %.0f: still settling, the floor under it changed %.1f s ago (an edge it flew over)"), GroupName, *Entry.Id.ToString(), Flyer->GetHeightAboveGround(), Flyer->GetWantedHeight(), SinceGroundChange);
				}
			}
			else
			{
				Look = Enemy->DescribeMovement();
				Attacks = Hits;   // a plain enemy has no shot counter: a hit on the player is its proof
			}
			UE_LOG(LogGB, Display, TEXT("SMOKE Enemies3D %s: %s moved=%.0f distance=%.0f attacks=%d hitsOnPlayer=%d | %s"), GroupName, *Entry.Id.ToString(), Moved, DistanceToPlayer, Attacks, Hits, *Look);
			// The sentinel starting inside its band holds its place by design (it only drifts to regain a line of sight).
			const bool bHoldsByDesign = Cast<AGBSentinelEnemy>(Enemy) && Entry.StartDistance <= GBEnemy3D::Stat(Enemy, TEXT("PreferredRangeMax"), 0.f);
			if (Moved < 50.f && !bHoldsByDesign && Attacks == 0)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE Enemies3D %s FAILED: %s did not move (%.0f cm)"), GroupName, *Entry.Id.ToString(), Moved);
			}
			// A melee enemy must have landed a hit on the player (a lunge that missed is not an attack); a ranged one needs a shot logged.
			if (bMelee ? Hits == 0 : Attacks == 0)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE Enemies3D %s FAILED: %s %s"), GroupName, *Entry.Id.ToString(), bMelee ? TEXT("never hit the player") : TEXT("never fired a shot"));
			}
		}
		const float Health = Player->GetAttributes()->GetHealth();
		UE_LOG(LogGB, Display, TEXT("SMOKE Enemies3D %s: hitscan shots=%d on target=%d, hits on the player=%d, player health %.0f / %.0f"), GroupName,
			GBEnemy3D::GetShotsFired() - E3DShotsBefore, GBEnemy3D::GetShotsOnTarget(), TotalHits, Health, Player->GetAttributes()->GetMaxHealth());
		if (TotalHits == 0 && Health >= Player->GetAttributes()->GetMaxHealth())
		{
			UE_LOG(LogGB, Error, TEXT("SMOKE Enemies3D %s FAILED: the player's health never dropped"), GroupName);
		}
		Player->GetAttributes()->SetInvulnerable(true);
		E3DLookAtGroup(Player);
	}

	/** One killed enemy under watch: its death pose is checked while it plays out, and its removal is checked against its own death time. */
	struct FE3DWatch
	{
		TWeakObjectPtr<AGBEnemyCharacter> Enemy;
		FName Id;
		float Age = 0.f;               // game seconds since the kill
		TWeakObjectPtr<UWorld> World;
		double KilledAt = 0.0;
		float MinLife = 0.f;           // it must not be gone before this (a pop-out)
		float MaxLife = 0.f;           // it must be gone by this
		bool bSkeletal = false;
		bool bFlyer = false;
		bool bPoseChecked = false;
		int32 ExpectedPieces = 1;
		float WreckHold = 0.f;         // flyers: wreck time plus sinking, counted from the first ground contact
		// The last look at a flyer's wreck, for the log once the actor is gone.
		float FallTime = -1.f;
		int32 Bounces = 0;
		int32 Pieces = 0;
		int32 Sparks = 0;
		bool bDone = false;
	};
	static TArray<FE3DWatch> E3DWatches;
	static FTSTicker::FDelegateHandle E3DWatchTicker;

	static bool E3DWatchTick(float Dt)
	{
		bool bAnyLeft = false;
		for (FE3DWatch& Watch : E3DWatches)
		{
			if (Watch.bDone)
			{
				continue;
			}
			// Game time, not the ticker's: the run may not be in real time.
			if (const UWorld* WatchWorld = Watch.World.Get())
			{
				Watch.Age = static_cast<float>(WatchWorld->GetTimeSeconds() - Watch.KilledAt);
			}
			AGBEnemyCharacter* Enemy = Watch.Enemy.Get();
			if (Enemy)
			{
				if (const AGBSkeletalEnemy* Skeletal = Cast<AGBSkeletalEnemy>(Enemy))
				{
					// After the clip: the body lies on the floor (the pelvis near it), not hanging in the air or sunk.
					if (Watch.bSkeletal && !Watch.bPoseChecked && Skeletal->GetDeathElapsed() >= Skeletal->GetDeathClipLength() + 0.3f)
					{
						Watch.bPoseChecked = true;
						const float Height = Skeletal->GetPelvisHeightAboveFloor();
						if (Height == AGBSkeletalEnemy::PelvisNoBone)
						{
							UE_LOG(LogGB, Display, TEXT("SMOKE Enemies3D %s death pose: no pelvis bone on this skeleton, height not measured"), *Watch.Id.ToString());
						}
						else if (Height == AGBSkeletalEnemy::PelvisNoFloor)
						{
							UE_LOG(LogGB, Error, TEXT("SMOKE Enemies3D FAILED: %s has a pelvis bone but no floor was found under it after its death clip (sunk, or over a pit)"), *Watch.Id.ToString());
						}
						else
						{
							UE_LOG(LogGB, Display, TEXT("SMOKE Enemies3D %s death pose: pelvis %.0f cm over the floor, %.0f cm off the capsule, %.1f s after the kill"), *Watch.Id.ToString(), Height, Skeletal->GetPelvisDistance(), Watch.Age);
							if (Height > 60.f || Height < -15.f)
							{
								UE_LOG(LogGB, Error, TEXT("SMOKE Enemies3D FAILED: %s lies with its pelvis %.0f cm over the floor after its death clip (wanted -15 to 60)"), *Watch.Id.ToString(), Height);
							}
						}
					}
				}
				else if (const AGBFlyerEnemy* Flyer = Cast<AGBFlyerEnemy>(Enemy))
				{
					Watch.FallTime = Flyer->GetFallTime();
					Watch.Bounces = Flyer->GetWreckBounces();
					Watch.Pieces = Flyer->GetWreckPieces();
					Watch.Sparks = Flyer->GetSparkBursts();
				}
				if (Watch.Age > Watch.MaxLife)
				{
					UE_LOG(LogGB, Error, TEXT("SMOKE Enemies3D FAILED: %s is still there %.1f s after its death (it should be gone by %.1f s)"), *Watch.Id.ToString(), Watch.Age, Watch.MaxLife);
					Watch.bDone = true;
				}
				bAnyLeft |= !Watch.bDone;
				continue;
			}

			// Gone. Not before it should be (a pop-out), and for a flyer: the debris reached the ground and came apart.
			Watch.bDone = true;
			if (Watch.bFlyer)
			{
				UE_LOG(LogGB, Display, TEXT("SMOKE Enemies3D %s wreck: fell for %.2f s, bounces=%d, pieces=%d, spark bursts=%d, gone %.1f s after the kill"), *Watch.Id.ToString(),
					Watch.FallTime, Watch.Bounces, Watch.Pieces, Watch.Sparks, Watch.Age);
				if (Watch.FallTime < 0.f)
				{
					UE_LOG(LogGB, Error, TEXT("SMOKE Enemies3D FAILED: the wreck of %s never touched the ground (it vanished or fell into a pit)"), *Watch.Id.ToString());
				}
				else
				{
					if (Watch.Pieces < Watch.ExpectedPieces)
					{
						UE_LOG(LogGB, Error, TEXT("SMOKE Enemies3D FAILED: the wreck of %s has %d pieces, it should have come apart into %d"), *Watch.Id.ToString(), Watch.Pieces, Watch.ExpectedPieces);
					}
					if (Watch.Bounces < 1)
					{
						UE_LOG(LogGB, Error, TEXT("SMOKE Enemies3D FAILED: the wreck of %s never bounced"), *Watch.Id.ToString());
					}
					if (Watch.Sparks < 1)
					{
						UE_LOG(LogGB, Error, TEXT("SMOKE Enemies3D FAILED: the wreck of %s spat no sparks while it lay there"), *Watch.Id.ToString());
					}
					if (Watch.Age < Watch.FallTime + Watch.WreckHold - 0.5f)
					{
						UE_LOG(LogGB, Error, TEXT("SMOKE Enemies3D FAILED: the wreck of %s was gone %.1f s after the kill, before its wreck time ended (%.1f s)"), *Watch.Id.ToString(), Watch.Age, Watch.FallTime + Watch.WreckHold);
					}
				}
			}
			else if (Watch.Age < Watch.MinLife)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE Enemies3D FAILED: %s was gone %.1f s after its death, before its clip, its lie and its sinking ended (%.1f s): it popped out"), *Watch.Id.ToString(), Watch.Age, Watch.MinLife);
			}
			else
			{
				UE_LOG(LogGB, Display, TEXT("SMOKE Enemies3D %s corpse gone %.1f s after the kill (due %.1f s)"), *Watch.Id.ToString(), Watch.Age, Watch.MinLife);
			}
		}
		if (!bAnyLeft)
		{
			E3DWatches.Reset();
			E3DWatchTicker.Reset();
		}
		return bAnyLeft;
	}

	static void E3DKillGroup(AGBPlayerCharacter* Player)
	{
		Player->GetAttributes()->SetInvulnerable(true);
		for (const FE3DEntry& Entry : E3DGroup)
		{
			if (AGBEnemyCharacter* Enemy = Entry.Enemy.Get())
			{
				// The smoke checks the way a death plays out (clip, lie, sinking, gone), not the seconds in the data: the lie of a
				// corpse and the time a wreck lies there are shortened for the run.
				if (UGBStatsComponent* EnemyStats = Enemy->GetStats())
				{
					EnemyStats->SetBaseStat(FName(TEXT("CorpseTime")), 0.6f);
					EnemyStats->SetBaseStat(FName(TEXT("WreckTime")), 1.5f);
				}
				FGBDamageInfo Info;
				Info.Amount = 1000000.f;
				Info.Instigator = Player;
				Info.HitLocation = Enemy->GetActorLocation();
				Info.HitDirection = Enemy->GetActorForwardVector();   // from behind: a shield blocks the front
				Info.SourceId = TEXT("SmokeEnemies3D");
				UGBCombatLibrary::DealDamage(Enemy, Info);
				UE_LOG(LogGB, Display, TEXT("SMOKE Enemies3D killed %s: dead=%d"), *Entry.Id.ToString(), Enemy->IsDead() ? 1 : 0);
				if (!Enemy->IsDead())
				{
					UE_LOG(LogGB, Error, TEXT("SMOKE Enemies3D FAILED: %s did not die to 1,000,000 damage"), *Entry.Id.ToString());
					continue;
				}
				FE3DWatch Watch;
				Watch.Enemy = Enemy;
				Watch.Id = Entry.Id;
				Watch.World = Enemy->GetWorld();
				Watch.KilledAt = Enemy->GetWorld() ? Enemy->GetWorld()->GetTimeSeconds() : 0.0;
				Watch.MaxLife = 8.f;
				if (const AGBSkeletalEnemy* Skeletal = Cast<AGBSkeletalEnemy>(Enemy))
				{
					// Never a ragdoll: the enemy's own death clip must be playing (a humanoid, the spider, the chief).
					Watch.bSkeletal = Skeletal->HasSkeletalLook();
					UE_LOG(LogGB, Display, TEXT("SMOKE Enemies3D %s death: clip length %.2f s, playing=%d, %.1f s until it is gone"), *Entry.Id.ToString(), Skeletal->GetDeathClipLength(),
						Skeletal->IsDeathClipPlaying() ? 1 : 0, Skeletal->GetDeathTotalSeconds());
					if (Watch.bSkeletal && !Skeletal->IsDeathClipPlaying())
					{
						UE_LOG(LogGB, Error, TEXT("SMOKE Enemies3D FAILED: %s has no death clip playing (Anim.Death missing or not loaded)"), *Entry.Id.ToString());
					}
					if (Watch.bSkeletal)
					{
						Watch.MinLife = Skeletal->GetDeathTotalSeconds() - 0.5f;
						Watch.MaxLife = Skeletal->GetDeathTotalSeconds() + 1.5f;
					}
				}
				else if (const AGBFlyerEnemy* Flyer = Cast<AGBFlyerEnemy>(Enemy))
				{
					Watch.bFlyer = true;
					Watch.ExpectedPieces = Flyer->HasArtLook() ? (Cast<AGBDroneEnemy>(Flyer) ? 6 : 2) : 1;
					Watch.WreckHold = GBEnemy3D::Stat(Enemy, TEXT("WreckTime"), 6.f) + GBEnemy3D::Stat(Enemy, TEXT("WreckSinkTime"), 1.f);
					Watch.MaxLife = Flyer->GetDeathTotalSeconds() + 1.5f;
				}
				E3DWatches.Add(Watch);
			}
		}
		Player->GetAttributes()->RestoreAll();
		E3DLookAtGroup(Player);
		if (!E3DWatchTicker.IsValid())
		{
			E3DWatchTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&E3DWatchTick), 0.1f);
		}
	}

	/** The end of a group's wait: a summary only. Each corpse and wreck is checked against its own death time by E3DWatchTick. */
	static void E3DCheckCorpses(const TCHAR* GroupName)
	{
		int32 Left = 0;
		for (const FE3DEntry& Entry : E3DGroup)
		{
			Left += Entry.Enemy.IsValid() ? 1 : 0;
		}
		UE_LOG(LogGB, Display, TEXT("SMOKE Enemies3D %s: %d of %d still there at the end of the wait (each is checked against its own death time)"), GroupName, Left, E3DGroup.Num());
		UGBCombatLibrary::OnDamageDealt().Remove(E3DHitHandle);
	}

	// ---- Enemies3D tactics group: an ad-hoc squad of 4 robots and 2 orcs round blocking boxes; the player is invulnerable and still.
	static TArray<TWeakObjectPtr<AActor>> E3TBoxes;
	static FTSTicker::FDelegateHandle E3TTicker;
	static FVector E3TPlayerSpot = FVector::ZeroVector;
	static int32 E3TMaxConcurrent = 0;
	static float E3TMaxClose = 0.f;
	static FString E3TWorstWhere;
	static float E3TNavWaited = 0.f;
	static FTSTicker::FDelegateHandle E3TSpawnTicker;
	static TMap<uint64, float> E3TClose;
	static int32 E3TCap = 3;

	static void E3TSpawnBoxes(UWorld* World, AGBPlayerCharacter* Player)
	{
		for (const TWeakObjectPtr<AActor>& Old : E3TBoxes)
		{
			if (Old.IsValid())
			{
				Old->Destroy();
			}
		}
		E3TBoxes.Reset();
		E3TPlayerSpot = Player->GetActorLocation();
		UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
		for (int32 Index = 0; Cube && Index < 6; ++Index)
		{
			const float Yaw = Index * 60.f + 30.f;
			const FVector Dir(FMath::Cos(FMath::DegreesToRadians(Yaw)), FMath::Sin(FMath::DegreesToRadians(Yaw)), 0.f);
			const FVector At = E3TPlayerSpot + Dir * 850.f;
			FHitResult Ground;
			FVector Base = At;
			if (World->LineTraceSingleByObjectType(Ground, At + FVector(0.f, 0.f, 300.f), At - FVector(0.f, 0.f, 600.f), FCollisionObjectQueryParams(ECC_WorldStatic)))
			{
				Base = Ground.ImpactPoint;
			}
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AStaticMeshActor* Box = World->SpawnActor<AStaticMeshActor>(Base + FVector(0.f, 0.f, 125.f), FRotator(0.f, Yaw + 90.f, 0.f), Params);
			if (Box)
			{
				Box->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
				Box->GetStaticMeshComponent()->SetStaticMesh(Cube);
				Box->SetActorScale3D(FVector(3.f, 0.6f, 2.5f));
				E3TBoxes.Add(Box);
			}
		}
		UE_LOG(LogGB, Display, TEXT("SMOKE Enemies3D tactics: %d blocking boxes round the player"), E3TBoxes.Num());
	}

	static void E3TStartSampling(UWorld* World)
	{
		E3TMaxConcurrent = 0;
		E3TMaxClose = 0.f;
		E3TWorstWhere.Reset();
		E3TClose.Reset();
		const UGBDataSubsystem* Data = UGBDataSubsystem::Get(World);
		const float Max = Data ? Data->GetGameValue(FName(TEXT("SquadMaxShooters")), 2.f) : 2.f;
		const float Big = Data ? Data->GetGameValue(FName(TEXT("SquadBigSize")), 5.f) : 5.f;
		E3TCap = FMath::RoundToInt(Max) + (E3DGroup.Num() >= FMath::RoundToInt(Big) ? 1 : 0);
		FTSTicker::GetCoreTicker().RemoveTicker(E3TTicker);
		E3TTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float Dt)
		{
			int32 Bursts = 0;
			for (int32 A = 0; A < E3DGroup.Num(); ++A)
			{
				const AGBSkeletalEnemy* EnemyA = Cast<AGBSkeletalEnemy>(E3DGroup[A].Enemy.Get());
				if (!EnemyA)
				{
					continue;
				}
				Bursts += EnemyA->IsBurstActive() ? 1 : 0;
				for (int32 B = A + 1; B < E3DGroup.Num(); ++B)
				{
					const AGBEnemyCharacter* EnemyB = E3DGroup[B].Enemy.Get();
					if (!EnemyB)
					{
						continue;
					}
					const uint64 Key = (static_cast<uint64>(A) << 32) | static_cast<uint32>(B);
					float& Time = E3TClose.FindOrAdd(Key);
					Time = FVector::Dist2D(EnemyA->GetActorLocation(), EnemyB->GetActorLocation()) < 200.f ? Time + Dt : 0.f;
					if (Time > E3TMaxClose)
					{
						// Which pair it was and what each was doing, so a failure can be read from the log.
						const AGBSkeletalEnemy* SkeletalB = Cast<AGBSkeletalEnemy>(EnemyB);
						E3TWorstWhere = FString::Printf(TEXT("%s at (%.0f, %.0f) state %d speed %.0f and %s at (%.0f, %.0f) state %d speed %.0f, %.0f cm apart"), *E3DGroup[A].Id.ToString(), EnemyA->GetActorLocation().X, EnemyA->GetActorLocation().Y,
							static_cast<int32>(EnemyA->GetCombatState()), EnemyA->GetVelocity().Size2D(), *E3DGroup[B].Id.ToString(), EnemyB->GetActorLocation().X, EnemyB->GetActorLocation().Y, SkeletalB ? static_cast<int32>(SkeletalB->GetCombatState()) : -1,
							EnemyB->GetVelocity().Size2D(), FVector::Dist2D(EnemyA->GetActorLocation(), EnemyB->GetActorLocation()));
					}
					E3TMaxClose = FMath::Max(E3TMaxClose, Time);
				}
			}
			E3TMaxConcurrent = FMath::Max(E3TMaxConcurrent, Bursts);
			return true;
		}));
	}

	/** After 12 s: tokens respected, spread round the player, nobody stacked, cover used, bursts from peeks, no pelvis drift. */
	static void E3TCheck(UWorld* World, AGBPlayerCharacter* Player)
	{
		FTSTicker::GetCoreTicker().RemoveTicker(E3TTicker);
		const FVector PlayerPos = Player->GetActorLocation();
		TArray<float> Bearings;
		float CoverTime = 0.f;
		int32 PeekBursts = 0;
		int32 Shots = 0;
		float MaxPelvis = 0.f;
		for (const FE3DEntry& Entry : E3DGroup)
		{
			const AGBSkeletalEnemy* Enemy = Cast<AGBSkeletalEnemy>(Entry.Enemy.Get());
			if (!Enemy)
			{
				continue;
			}
			const FVector Off = Enemy->GetActorLocation() - PlayerPos;
			Bearings.Add(FMath::RadiansToDegrees(FMath::Atan2(Off.Y, Off.X)));
			CoverTime += Enemy->GetCoverTime();
			PeekBursts += Enemy->GetPeekBursts();
			Shots += Enemy->GetShotCount();
			MaxPelvis = FMath::Max(MaxPelvis, Enemy->GetMaxPelvisDistance());
			UE_LOG(LogGB, Display, TEXT("SMOKE Enemies3D tactics: %s state=%d coverTime=%.1f peekBursts=%d shots=%d reloc=%d maxPelvis=%.0f | %s"), *Entry.Id.ToString(), static_cast<int32>(Enemy->GetCombatState()),
				Enemy->GetCoverTime(), Enemy->GetPeekBursts(), Enemy->GetShotCount(), Enemy->GetRelocations(), Enemy->GetMaxPelvisDistance(), *Enemy->DescribeTactics());
		}
		Bearings.Sort();
		float MaxGap = 0.f;
		for (int32 Index = 0; Index < Bearings.Num(); ++Index)
		{
			const float Next = Index + 1 < Bearings.Num() ? Bearings[Index + 1] : Bearings[0] + 360.f;
			MaxGap = FMath::Max(MaxGap, Next - Bearings[Index]);
		}
		const float Span = Bearings.Num() > 1 ? 360.f - MaxGap : 0.f;
		UE_LOG(LogGB, Display, TEXT("SMOKE Enemies3D tactics: members=%d span=%.0f deg, max concurrent bursts=%d (limit %d), longest time two members within 200 cm=%.1f s (%s), cover time=%.1f s, peek bursts=%d, shots=%d, max pelvis distance=%.0f cm"),
			Bearings.Num(), Span, E3TMaxConcurrent, E3TCap, E3TMaxClose, E3TWorstWhere.IsEmpty() ? TEXT("never") : *E3TWorstWhere, CoverTime, PeekBursts, Shots, MaxPelvis);
		if (Bearings.Num() < 6)
		{
			UE_LOG(LogGB, Error, TEXT("SMOKE Enemies3D tactics FAILED: only %d of 6 members could be spawned"), Bearings.Num());
		}
		if (E3TMaxConcurrent > E3TCap)
		{
			UE_LOG(LogGB, Error, TEXT("SMOKE Enemies3D tactics FAILED: %d members in a burst at once, the token limit is %d"), E3TMaxConcurrent, E3TCap);
		}
		if (Span < 70.f)
		{
			UE_LOG(LogGB, Error, TEXT("SMOKE Enemies3D tactics FAILED: the members span only %.0f degrees round the player (need 70)"), Span);
		}
		if (E3TMaxClose > 3.5f)
		{
			UE_LOG(LogGB, Error, TEXT("SMOKE Enemies3D tactics FAILED: two members stood within 200 cm of each other for %.1f s (limit 3.5): %s"), E3TMaxClose, *E3TWorstWhere);
		}
		if (E3TBoxes.Num() > 0 && CoverTime < 1.f)
		{
			UE_LOG(LogGB, Error, TEXT("SMOKE Enemies3D tactics FAILED: nobody spent time in cover behind the boxes"));
		}
		if (PeekBursts < 1)
		{
			UE_LOG(LogGB, Error, TEXT("SMOKE Enemies3D tactics FAILED: no burst was fired from a peek"));
		}
		if (MaxPelvis > 60.f)
		{
			UE_LOG(LogGB, Error, TEXT("SMOKE Enemies3D tactics FAILED: a pelvis was %.0f cm from its capsule (the clips travel again?)"), MaxPelvis);
		}
		(void)World;
	}

	/** The player flies 28 m above the middle of the group looking down: the screenshot shows where everyone stands. */
	static void E3TTop(AGBPlayerCharacter* Player)
	{
		E3TPlayerSpot = Player->GetActorLocation();
		FVector Sum = FVector::ZeroVector;
		int32 Count = 0;
		for (const FE3DEntry& Entry : E3DGroup)
		{
			if (const AGBEnemyCharacter* Enemy = Entry.Enemy.Get())
			{
				Sum += Enemy->GetActorLocation();
				++Count;
			}
		}
		const FVector Centre = Count > 0 ? Sum / Count : E3TPlayerSpot;
		if (UCharacterMovementComponent* Move = Player->GetCharacterMovement())
		{
			Move->SetMovementMode(MOVE_Flying);
			Move->Velocity = FVector::ZeroVector;
		}
		Player->SetActorLocation(Centre + FVector(0.f, 0.f, 2800.f), false, nullptr, ETeleportType::TeleportPhysics);
		if (AGBPlayerController* PC = Cast<AGBPlayerController>(Player->GetController()))
		{
			PC->SetControlRotation(FRotator(-89.f, 0.f, 0.f));
		}
	}

	static void E3TBack(AGBPlayerCharacter* Player)
	{
		Player->SetActorLocation(E3TPlayerSpot - FVector(0.f, 0.f, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
		if (UCharacterMovementComponent* Move = Player->GetCharacterMovement())
		{
			Move->SetMovementMode(MOVE_Falling);
			Move->Velocity = FVector::ZeroVector;
		}
		E3DLookAtGroup(Player);
	}

	static void E3TClean()
	{
		for (const TWeakObjectPtr<AActor>& Box : E3TBoxes)
		{
			if (Box.IsValid())
			{
				Box->Destroy();
			}
		}
		E3TBoxes.Reset();
		UGBSquadSubsystem::bLogTactics = false;
	}

	// ---- Enemies3D chief group: the shielded horde chief.
	static FTSTicker::FDelegateHandle E3CTicker;
	static bool bE3CLoweredShot = false;
	static bool bE3CRaisedShot = false;

	static AGBShieldEnemy* E3CFindChief()
	{
		for (const FE3DEntry& Entry : E3DGroup)
		{
			if (AGBShieldEnemy* Chief = Cast<AGBShieldEnemy>(Entry.Enemy.Get()))
			{
				return Chief;
			}
		}
		return nullptr;
	}

	/** Takes the two screenshots when they can be seen: once with the shield lowered (he aims), once raised. */
	static void E3CStartShots()
	{
		bE3CLoweredShot = false;
		bE3CRaisedShot = false;
		FTSTicker::GetCoreTicker().RemoveTicker(E3CTicker);
		E3CTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float)
		{
			const AGBShieldEnemy* Chief = E3CFindChief();
			if (!Chief || Chief->IsDead())
			{
				return false;
			}
			if (!bE3CLoweredShot && !Chief->IsShieldUp() && !Chief->IsStaggered() && Chief->GetEnemyAnim() && Chief->GetEnemyAnim()->GetFrame().PoseAlpha < 0.2f)
			{
				bE3CLoweredShot = true;
				UE_LOG(LogGB, Display, TEXT("SMOKE Enemies3D chief: closeup with the shield lowered (aiming): %s"), *Chief->DescribeTactics());
				Shot(TEXT("enemies3d_chief_lowered"));
			}
			else if (bE3CLoweredShot && !bE3CRaisedShot && Chief->IsShieldUp() && Chief->GetEnemyAnim() && Chief->GetEnemyAnim()->GetFrame().PoseAlpha > 0.95f)
			{
				bE3CRaisedShot = true;
				UE_LOG(LogGB, Display, TEXT("SMOKE Enemies3D chief: closeup with the shield raised: %s"), *Chief->DescribeTactics());
				Shot(TEXT("enemies3d_chief_raised"));
				return false;
			}
			return true;
		}));
	}

	/** Five hits to the front with the shield raised do nothing; a hit from behind hurts. */
	static void E3CBlockTest(AGBPlayerCharacter* Player)
	{
		AGBShieldEnemy* Chief = E3CFindChief();
		if (!Chief || Chief->IsDead())
		{
			UE_LOG(LogGB, Error, TEXT("SMOKE Enemies3D chief FAILED: no chief for the block test"));
			return;
		}
		const FVector Forward = Chief->GetActorForwardVector().GetSafeNormal2D();
		const FVector Loc = Chief->GetActorLocation();
		auto Hit = [&](const FVector& From)
		{
			FGBDamageInfo Info;
			Info.Amount = 10.f;
			Info.Instigator = Player;
			Info.HitLocation = Loc + (From - Loc).GetSafeNormal() * 60.f;
			Info.HitDirection = (Loc - From).GetSafeNormal();
			Info.SourceId = TEXT("SmokeChiefBlock");
			UGBCombatLibrary::DealDamage(Chief, Info);
		};
		const float Health0 = Chief->GetAttributes()->GetHealth();
		const int32 Blocked0 = Chief->GetBlockedHits();
		Chief->DebugSetShield(true);
		for (int32 Index = 0; Index < 5; ++Index)
		{
			Hit(Loc + Forward * 800.f);
		}
		const float Health1 = Chief->GetAttributes()->GetHealth();
		const int32 Blocked1 = Chief->GetBlockedHits();
		Chief->DebugSetShield(true);
		Hit(Loc - Forward * 800.f);
		const float Health2 = Chief->GetAttributes()->GetHealth();
		Chief->DebugSetShield(true);
		// Lowered: the same front hit hurts.
		Chief->DebugSetShield(false);
		Hit(Loc + Forward * 800.f);
		const float Health3 = Chief->GetAttributes()->GetHealth();
		UE_LOG(LogGB, Display, TEXT("SMOKE Enemies3D chief: health %.1f -> front x5 with the shield up %.1f (blocked %d) -> one hit from behind %.1f -> one front hit with the shield down %.1f"), Health0, Health1, Blocked1 - Blocked0, Health2, Health3);
		if (Health1 != Health0 || Blocked1 - Blocked0 != 5)
		{
			UE_LOG(LogGB, Error, TEXT("SMOKE Enemies3D chief FAILED: front hits with the shield raised changed his health (%.1f -> %.1f), blocked %d of 5"), Health0, Health1, Blocked1 - Blocked0);
		}
		if (!(Health2 < Health1))
		{
			UE_LOG(LogGB, Error, TEXT("SMOKE Enemies3D chief FAILED: a hit from behind did no damage (%.1f -> %.1f)"), Health1, Health2);
		}
		if (!(Health3 < Health2))
		{
			UE_LOG(LogGB, Error, TEXT("SMOKE Enemies3D chief FAILED: a front hit with the shield lowered did no damage"));
		}
	}

	/** Single shots, no faster than AttackInterval, the shield never up at a shot. */
	static void E3CCheck()
	{
		FTSTicker::GetCoreTicker().RemoveTicker(E3CTicker);
		const AGBShieldEnemy* Chief = E3CFindChief();
		if (!Chief)
		{
			UE_LOG(LogGB, Error, TEXT("SMOKE Enemies3D chief FAILED: no chief at the check"));
			return;
		}
		const TArray<float>& Times = Chief->GetShotTimes();
		const float Interval = GBEnemy3D::Stat(Chief, TEXT("AttackInterval"), 3.5f);
		float MinGap = 1.0e9f;
		for (int32 Index = 1; Index < Times.Num(); ++Index)
		{
			MinGap = FMath::Min(MinGap, Times[Index] - Times[Index - 1]);
		}
		UE_LOG(LogGB, Display, TEXT("SMOKE Enemies3D chief: shots=%d, shortest gap=%.1f s (AttackInterval %.1f), shots with the shield still up=%d, blocked hits=%d, closeup shots lowered=%d raised=%d"),
			Times.Num(), Times.Num() > 1 ? MinGap : 0.f, Interval, Chief->GetShotsWithShieldUp(), Chief->GetBlockedHits(), bE3CLoweredShot ? 1 : 0, bE3CRaisedShot ? 1 : 0);
		if (Times.Num() < 2)
		{
			UE_LOG(LogGB, Error, TEXT("SMOKE Enemies3D chief FAILED: only %d shot(s) fired"), Times.Num());
		}
		if (Times.Num() > 1 && MinGap < Interval * 0.95f)
		{
			UE_LOG(LogGB, Error, TEXT("SMOKE Enemies3D chief FAILED: two shots %.1f s apart, faster than AttackInterval %.1f"), MinGap, Interval);
		}
		if (Chief->GetShotsWithShieldUp() > 0)
		{
			UE_LOG(LogGB, Error, TEXT("SMOKE Enemies3D chief FAILED: %d shot(s) fired with the shield still raised"), Chief->GetShotsWithShieldUp());
		}
		if (!bE3CLoweredShot || !bE3CRaisedShot)
		{
			UE_LOG(LogGB, Error, TEXT("SMOKE Enemies3D chief FAILED: the shield was never seen both lowered and raised (screenshots %d / %d)"), bE3CLoweredShot ? 1 : 0, bE3CRaisedShot ? 1 : 0);
		}
	}

	// ---- Enemies3D shield checks: real traces at the visible area, and a shot through the real gun path.
	static void E3CShieldTests(UWorld* World, AGBPlayerCharacter* Player)
	{
		AGBShieldEnemy* Chief = E3CFindChief();
		if (!Chief || Chief->IsDead() || !Chief->GetShieldBox())
		{
			UE_LOG(LogGB, Error, TEXT("SMOKE Enemies3D chief FAILED: no chief for the shield traces"));
			return;
		}
		UBoxComponent* Box = Chief->GetShieldBox();
		auto CountHits = [&]()
		{
			// The five points come from the shield MESH's own bounds (what the player sees), not from the box: a box that is tiny or
			// misplaced relative to the mesh then fails. Mesh local frame: face to +X, width Y, height Z.
			UStaticMeshComponent* Mesh = Chief->GetShieldMesh();
			if (!Mesh || !Mesh->GetStaticMesh())
			{
				return 0;
			}
			const FBoxSphereBounds Local = Mesh->GetStaticMesh()->GetBounds();
			const FTransform MeshTransform = Mesh->GetComponentTransform();
			const FVector Forward = MeshTransform.TransformVectorNoScale(FVector::ForwardVector);
			auto At = [&](float Y, float Z) { return MeshTransform.TransformPosition(Local.Origin + FVector(0.f, Local.BoxExtent.Y * Y, Local.BoxExtent.Z * Z)); };
			const FVector Points[5] = { At(0.f, 0.f), At(0.6f, 0.6f), At(-0.6f, 0.6f), At(0.6f, -0.6f), At(-0.6f, -0.6f) };
			int32 Hits = 0;
			FCollisionQueryParams Params(SCENE_QUERY_STAT(GBSmokeShield), false, Player);
			for (const FVector& Point : Points)
			{
				FHitResult Hit;
				// The player's gun trace channel, from 3.5 m in front of the shield to its face.
				if (World->LineTraceSingleByChannel(Hit, Point + Forward * 350.f, Point - Forward * 5.f, GBWeapon::AimTraceChannel, Params) && Hit.GetComponent() == Box)
				{
					++Hits;
				}
			}
			return Hits;
		};
		Chief->DebugSetShield(true);
		const int32 Raised = CountHits();
		Chief->DebugSetShield(false);
		const int32 Lowered = CountHits();
		UE_LOG(LogGB, Display, TEXT("SMOKE Enemies3D chief: gun-channel traces at 5 points of the shield: %d of 5 hit the hit box while raised, %d of 5 while lowered"), Raised, Lowered);
		if (Raised != 5)
		{
			UE_LOG(LogGB, Error, TEXT("SMOKE Enemies3D chief FAILED: only %d of 5 shot traces stopped on the raised shield's hit box"), Raised);
		}
		if (Lowered != 0)
		{
			UE_LOG(LogGB, Error, TEXT("SMOKE Enemies3D chief FAILED: %d of 5 shot traces still hit the hit box of a lowered shield"), Lowered);
		}

		// A shot through the real gun path at his front: his health must not change.
		// GiveWeapon returns null when the player already owns it (earlier blocks): then the pistol is equipped, or whatever gun is held is used.
		AGBHeldItem* Gun = Player->GetEquipment()->GiveWeapon(TEXT("Pistol"), true, false);
		if (!Gun)
		{
			Player->GetEquipment()->EquipOwnedWeapon(TEXT("Pistol"));
			Gun = Player->GetEquipment()->GetActiveItem();
		}
		if (!Gun)
		{
			UE_LOG(LogGB, Error, TEXT("SMOKE Enemies3D chief FAILED: the player holds nothing to fire the real gun shot with"));
			return;
		}
		const FVector Forward = Chief->GetActorForwardVector().GetSafeNormal2D();
		Player->SetActorLocation(Chief->GetActorLocation() + Forward * 800.f, false, nullptr, ETeleportType::TeleportPhysics);
		Chief->DebugSetShield(true);
		if (AGBPlayerController* PC = Cast<AGBPlayerController>(Player->GetController()))
		{
			PC->SetControlRotation((Box->GetComponentLocation() - Player->GetAimOrigin()).Rotation());
		}
		const float Health0 = Chief->GetAttributes()->GetHealth();
		const int32 Blocked0 = Chief->GetBlockedHits();
		Gun->StartPrimary();
		Gun->StopPrimary();
		const float Health1 = Chief->GetAttributes()->GetHealth();
		const int32 Blocked1 = Chief->GetBlockedHits();
		UE_LOG(LogGB, Display, TEXT("SMOKE Enemies3D chief: real gun shot at his front with the shield raised: health %.1f -> %.1f, blocked hits %d -> %d"), Health0, Health1, Blocked0, Blocked1);
		if (Health1 != Health0)
		{
			UE_LOG(LogGB, Error, TEXT("SMOKE Enemies3D chief FAILED: a real gun shot at his raised shield took %.1f health"), Health0 - Health1);
		}
		if (Blocked1 <= Blocked0)
		{
			UE_LOG(LogGB, Error, TEXT("SMOKE Enemies3D chief FAILED: the real gun shot did not reach the shield (no blocked hit counted)"));
		}
	}

	// ---- Enemies3D swarm: four spiders on the player; never more than SquadMaxMeleeAttackers in a windup or a lunge at once.
	static FTSTicker::FDelegateHandle E3WTicker;
	static int32 E3WMaxCommitted = 0;
	static int32 E3WCap = 2;

	static void E3WStart(UWorld* World)
	{
		E3WMaxCommitted = 0;
		const UGBDataSubsystem* Data = UGBDataSubsystem::Get(World);
		E3WCap = FMath::Max(1, FMath::RoundToInt(Data ? Data->GetGameValue(FName(TEXT("SquadMaxMeleeAttackers")), 2.f) : 2.f));
		FTSTicker::GetCoreTicker().RemoveTicker(E3WTicker);
		E3WTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float)
		{
			int32 Committed = 0;
			for (const FE3DEntry& Entry : E3DGroup)
			{
				if (const AGBSkeletalEnemy* Spider = Cast<AGBSkeletalEnemy>(Entry.Enemy.Get()))
				{
					Committed += (!Spider->IsDead() && Spider->IsMeleeCommitted()) ? 1 : 0;
				}
			}
			E3WMaxCommitted = FMath::Max(E3WMaxCommitted, Committed);
			return true;
		}));
	}

	static void E3WCheck()
	{
		FTSTicker::GetCoreTicker().RemoveTicker(E3WTicker);
		int32 Lunges = 0;
		for (const FE3DEntry& Entry : E3DGroup)
		{
			if (const AGBSkeletalEnemy* Spider = Cast<AGBSkeletalEnemy>(Entry.Enemy.Get()))
			{
				Lunges += Spider->GetLungeCount();
			}
		}
		UE_LOG(LogGB, Display, TEXT("SMOKE Enemies3D swarm: %d spiders, most in a windup or a lunge at once=%d (limit %d), lunges=%d"), E3DGroup.Num(), E3WMaxCommitted, E3WCap, Lunges);
		if (E3WMaxCommitted > E3WCap)
		{
			UE_LOG(LogGB, Error, TEXT("SMOKE Enemies3D swarm FAILED: %d spiders attacked at once, the limit is %d"), E3WMaxCommitted, E3WCap);
		}
		if (E3WMaxCommitted < 1)
		{
			UE_LOG(LogGB, Error, TEXT("SMOKE Enemies3D swarm FAILED: no spider ever attacked (the check proves nothing)"));
		}
	}

	// ---- Chief views: raised and lowered, from the player's eye at 4 m in front and from his left side.
	static void E3ChiefView(AGBPlayerCharacter* Player, bool bFront)
	{
		AGBShieldEnemy* Chief = E3CFindChief();
		if (!Chief)
		{
			UE_LOG(LogGB, Error, TEXT("SMOKE Chief FAILED: no chief for the view"));
			return;
		}
		const FVector Loc = Chief->GetActorLocation();
		const FVector Forward = Chief->GetActorForwardVector().GetSafeNormal2D();
		const FVector Left = -Chief->GetActorRightVector().GetSafeNormal2D();
		FVector At = Loc + (bFront ? Forward : Left) * 400.f;
		At.Z = Loc.Z;
		Player->SetActorLocation(At, false, nullptr, ETeleportType::TeleportPhysics);
		if (AGBPlayerController* PC = Cast<AGBPlayerController>(Player->GetController()))
		{
			PC->SetControlRotation((Loc + FVector(0.f, 0.f, 60.f) - Player->GetAimOrigin()).Rotation());
		}
		UE_LOG(LogGB, Display, TEXT("SMOKE Chief view %s: %s"), bFront ? TEXT("front") : TEXT("left"), *Chief->DescribeTactics());
	}

	// ---- Chief pose experiments: Saved/chief_pose_try.txt holds one "R|bone:p,y,r;..." or "L|..." per line (view from his right or left).
	static TArray<FString> E3PoseTries;
	static void E3PoseTryLoad()
	{
		E3PoseTries.Reset();
		FString Text;
		if (FFileHelper::LoadFileToString(Text, *(FPaths::ProjectSavedDir() / TEXT("chief_pose_try.txt"))))
		{
			Text.ParseIntoArrayLines(E3PoseTries);
		}
	}
	static void E3PoseTry(AGBPlayerCharacter* Player, int32 Index)
	{
		AGBShieldEnemy* Chief = E3CFindChief();
		if (!Chief || !E3PoseTries.IsValidIndex(Index) || E3PoseTries[Index].Len() < 3)
		{
			return;
		}
		const FString Line = E3PoseTries[Index];
		const bool bRight = Line.Left(1) == TEXT("R");
		Chief->DebugHold(1);
		Chief->DebugSetPose(Line.Mid(2));
		const FVector Loc = Chief->GetActorLocation();
		const FVector Forward = Chief->GetActorForwardVector().GetSafeNormal2D();
		const FVector Side = (bRight ? 1.f : -1.f) * Chief->GetActorRightVector().GetSafeNormal2D();
		FVector At = Loc + Forward * 330.f + Side * 260.f;
		At.Z = Loc.Z;
		Player->SetActorLocation(At, false, nullptr, ETeleportType::TeleportPhysics);
		if (AGBPlayerController* PC = Cast<AGBPlayerController>(Player->GetController()))
		{
			PC->SetControlRotation((Loc + FVector(0.f, 0.f, 120.f) - Player->GetAimOrigin()).Rotation());
		}
		UE_LOG(LogGB, Display, TEXT("SMOKE Chief pose try %d (%s view): %s"), Index, bRight ? TEXT("right") : TEXT("left"), *Line);
	}

	// ---- Enemies3D poses group: a robot on patrol (relaxed pose) and an orc forced into the crouch.
	static void E3PSpawn(UWorld* World, AGBPlayerCharacter* Player)
	{
		E3DSpawnGroup(World, Player, { { FName(TEXT("Robot_Future")), 1100.f }, { FName(TEXT("Orc_Future")), 1100.f } });
		Player->GetAttributes()->SetInvulnerable(true);
		for (const FE3DEntry& Entry : E3DGroup)
		{
			AGBSkeletalEnemy* Enemy = Cast<AGBSkeletalEnemy>(Entry.Enemy.Get());
			if (!Enemy)
			{
				continue;
			}
			// They must not notice him during the shots: no sight notice for a long time (data, not code).
			Enemy->GetStats()->SetBaseStat(FName(TEXT("NoticeTime")), 100000.f);
			if (Entry.Id == FName(TEXT("Robot_Future")))
			{
				const FVector Post = Enemy->GetActorLocation();
				const FBox Zone(Post - FVector(900.f, 900.f, 300.f), Post + FVector(900.f, 900.f, 300.f));
				const TArray<FVector> Route = { Post + FVector(600.f, 0.f, 0.f), Post + FVector(0.f, 600.f, 0.f), Post - FVector(600.f, 0.f, 0.f) };
				Enemy->SetEncounterContext(nullptr, Zone, Post, EGBUnawareBehavior::Patrol, Route);
			}
			else
			{
				Enemy->DebugForcePosture(2);
			}
		}
	}

	// ---- Enemies3DBase block (gb.Smoke Enemies3DBase on the real map): the encounters the builder placed in FutureBase.
	struct FE3BSpawn
	{
		TWeakObjectPtr<AGBEnemyCharacter> Enemy;
		FName Id;
		FVector Start = FVector::ZeroVector;
		float StartDistance = 0.f;
	};
	static FName E3BId = NAME_None;
	static TArray<FE3BSpawn> E3BSpawned;
	static TSet<TWeakObjectPtr<AGBEnemyCharacter>> E3BBefore;
	static int32 E3BPowerCellsBefore = 0;
	static int32 E3BShotsBefore = 0;
	static int32 E3BActedCount = 0;
	static int32 E3BKilled = 0;
	static bool bE3BNoted = false;

	/** Only a filtered run on the real map plays this block (the full run's tour removes encounters and must not be disturbed). */
	static bool E3BActive(UWorld* World)
	{
		if (!bFilteredRun || !World)
		{
			return false;
		}
		const bool bRealMap = World->GetMapName().Contains(TEXT("FutureBase"));
		if (!bRealMap && !bE3BNoted)
		{
			bE3BNoted = true;
			UE_LOG(LogGB, Display, TEXT("SMOKE Enemies3DBase skipped: it needs -Map /Game/Glyphbound/Maps/FutureBase"));
		}
		return bRealMap;
	}

	static AGBEncounter* E3BFindEncounter(UWorld* World, FName Id)
	{
		for (TActorIterator<AGBEncounter> It(World); It; ++It)
		{
			if (It->EncounterId == Id)
			{
				return *It;
			}
		}
		return nullptr;
	}

	static int32 E3BCountPowerCells(UWorld* World)
	{
		int32 Count = 0;
		for (TActorIterator<AGBPickup> It(World); It; ++It)
		{
			Count += It->GetDrop().ItemId == FName(TEXT("PowerCell")) ? 1 : 0;
		}
		return Count;
	}

	static FVector E3BStand = FVector::ZeroVector;
	static int32 E3BLastReinforceWaves = 0;
	static int32 E3BZoneSamples = 0;
	static int32 E3BZoneViolations = 0;

	static void E3BTeleport(AGBPlayerCharacter* Player, const FVector& Where, bool bFlying)
	{
		const float HalfHeight = Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		Player->SetActorLocation(Where + FVector(0.f, 0.f, HalfHeight + 3.f), false, nullptr, ETeleportType::TeleportPhysics);
		if (UCharacterMovementComponent* Move = Player->GetCharacterMovement())
		{
			Move->Velocity = FVector::ZeroVector;
			Move->SetMovementMode(bFlying ? MOVE_Flying : MOVE_Falling);
		}
	}

	/** Enemies of the encounter under test: alive, in the encounter's own list (prespawned ones are in it before the player comes). */
	static void E3BCollectNew(UWorld* World, TArray<AGBEnemyCharacter*>& Out)
	{
		Out.Reset();
		if (const AGBEncounter* Encounter = E3BFindEncounter(World, E3BId))
		{
			for (const TObjectPtr<AGBEnemyCharacter>& Enemy : Encounter->GetEnemies())
			{
				if (Enemy && !Enemy->IsDead())
				{
					Out.Add(Enemy);
				}
			}
		}
	}

	/** Before the player comes near: the first wave already stands in its zone, unaware. */
	static void E3BBegin(UWorld* World, AGBPlayerCharacter* Player, FName Id, const FVector& Stand)
	{
		if (!E3BActive(World))
		{
			return;
		}
		E3BId = Id;
		E3BStand = Stand;
		E3BLastReinforceWaves = 0;
		E3BSpawned.Reset();
		E3BActedCount = 0;
		E3BKilled = 0;
		E3BPowerCellsBefore = E3BCountPowerCells(World);
		GBEnemy3D::ResetShotCounters();
		E3BShotsBefore = 0;
		AGBSkeletalEnemy::bTrackPelvis = true;
		UGBSquadSubsystem::bLogTactics = true;
		Player->GetAttributes()->SetInvulnerable(true);
		Player->GetAttributes()->RestoreAll();
		const AGBEncounter* Encounter = E3BFindEncounter(World, Id);
		if (!Encounter)
		{
			UE_LOG(LogGB, Error, TEXT("SMOKE Enemies3DBase %s FAILED: the encounter is not in the map"), *Id.ToString());
			return;
		}
		TArray<AGBEnemyCharacter*> Enemies;
		E3BCollectNew(World, Enemies);
		const FBox Zone = Encounter->GetZone();
		int32 Aware = 0;
		int32 Outside = 0;
		for (AGBEnemyCharacter* Enemy : Enemies)
		{
			FE3BSpawn Spawn;
			Spawn.Enemy = Enemy;
			Spawn.Id = Enemy->GetEnemyId();
			Spawn.Start = Enemy->GetActorLocation();
			E3BSpawned.Add(Spawn);
			Aware += Enemy->IsAware() ? 1 : 0;
			Outside += Zone.ExpandBy(60.f).IsInside(Enemy->GetActorLocation()) ? 0 : 1;
		}
		UE_LOG(LogGB, Display, TEXT("SMOKE Enemies3DBase %s: BEFORE the player comes: state=%d, %d enemies already standing (aware %d, outside the zone %d), zone %s .. %s, player at %s"), *Id.ToString(),
			static_cast<int32>(Encounter->GetState()), Enemies.Num(), Aware, Outside, *Zone.Min.ToCompactString(), *Zone.Max.ToCompactString(), *Player->GetActorLocation().ToCompactString());
		if (Enemies.Num() == 0)
		{
			UE_LOG(LogGB, Error, TEXT("SMOKE Enemies3DBase %s FAILED: no enemy stands in the world before the player arrives (prespawn)"), *Id.ToString());
		}
		if (Encounter->GetState() != EGBEncounterState::Waiting)
		{
			UE_LOG(LogGB, Error, TEXT("SMOKE Enemies3DBase %s FAILED: the encounter is %d before the player came near (it must wait for a notice)"), *Id.ToString(), static_cast<int32>(Encounter->GetState()));
		}
		if (Outside > 0)
		{
			UE_LOG(LogGB, Error, TEXT("SMOKE Enemies3DBase %s FAILED: %d enemies start outside their zone"), *Id.ToString(), Outside);
		}
	}

	/** Some seconds later, with the player still far: at least one patroller walked along its route, nobody noticed. */
	static void E3BPatrolCheck(UWorld* World, AGBPlayerCharacter* Player)
	{
		if (!E3BActive(World))
		{
			return;
		}
		(void)Player;
		int32 Patrollers = 0;
		int32 Moved = 0;
		int32 Aware = 0;
		float MaxTravel = 0.f;
		for (const FE3BSpawn& Spawn : E3BSpawned)
		{
			const AGBEnemyCharacter* Enemy = Spawn.Enemy.Get();
			if (!Enemy || Enemy->IsDead())
			{
				continue;
			}
			const bool bPatroller = Enemy->GetUnawareBehavior() == EGBUnawareBehavior::Patrol;
			Patrollers += bPatroller ? 1 : 0;
			Aware += Enemy->IsAware() ? 1 : 0;
			const float Travel = Enemy->GetUnawareTravel();
			MaxTravel = bPatroller ? FMath::Max(MaxTravel, Travel) : MaxTravel;
			Moved += (bPatroller && Travel > 50.f) ? 1 : 0;
			UE_LOG(LogGB, Display, TEXT("SMOKE Enemies3DBase %s: unaware %s behaviour=%d route=%d travelled=%.0f cm from its post=%.0f cm aware=%d"), *E3BId.ToString(), *Spawn.Id.ToString(),
				static_cast<int32>(Enemy->GetUnawareBehavior()), Enemy->GetRouteNum(), Travel, FVector::Dist2D(Enemy->GetActorLocation(), Enemy->GetPost()), Enemy->IsAware() ? 1 : 0);
		}
		UE_LOG(LogGB, Display, TEXT("SMOKE Enemies3DBase %s: patrol check: %d patrollers, %d moved along their route (best %.0f cm), %d aware"), *E3BId.ToString(), Patrollers, Moved, MaxTravel, Aware);
		if (Patrollers > 0 && Moved == 0)
		{
			UE_LOG(LogGB, Error, TEXT("SMOKE Enemies3DBase %s FAILED: none of %d patrollers moved while unaware"), *E3BId.ToString(), Patrollers);
		}
		if (Aware > 0)
		{
			UE_LOG(LogGB, Error, TEXT("SMOKE Enemies3DBase %s FAILED: %d enemies noticed a player who was far away"), *E3BId.ToString(), Aware);
		}
	}

	/** The player walks in, in view of them. */
	static TWeakObjectPtr<AGBEnemyCharacter> E3BWatcher;

	/** A spot in front of this enemy, in its sight cone, within range, with a clear line from its eyes to the player's. */
	static bool E3BFindSpotInFront(UWorld* World, const AGBEnemyCharacter* Enemy, const AGBPlayerCharacter* Player, FVector& OutFoot)
	{
		const FVector Forward = Enemy->GetActorForwardVector().GetSafeNormal2D();
		const float HalfHeight = Enemy->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		FCollisionQueryParams Params(SCENE_QUERY_STAT(GBSmokeFront), false, Enemy);
		Params.AddIgnoredActor(Player);
		FCollisionResponseParams IgnorePawns;
		IgnorePawns.CollisionResponse.SetResponse(ECC_Pawn, ECR_Ignore);
		for (const float Distance : { 600.f, 450.f, 350.f, 800.f })
		{
			const FVector Guess = Enemy->GetActorLocation() + Forward * Distance;
			FHitResult Ground;
			if (!World->LineTraceSingleByObjectType(Ground, Guess + FVector(0.f, 0.f, 150.f), Guess - FVector(0.f, 0.f, 600.f), FCollisionObjectQueryParams(ECC_WorldStatic)))
			{
				continue;
			}
			const FVector Foot = Ground.ImpactPoint;
			const FVector Eyes = Foot + FVector(0.f, 0.f, 170.f);
			const FVector From = Enemy->GetActorLocation() + FVector(0.f, 0.f, HalfHeight * 0.5f);
			if (!World->LineTraceTestByChannel(From, Eyes, ECC_Visibility, Params, IgnorePawns) && FMath::Abs(Foot.Z - (Enemy->GetActorLocation().Z - HalfHeight)) < 120.f)
			{
				OutFoot = Foot;
				return true;
			}
		}
		return false;
	}

	static void E3BTeleportIn(UWorld* World, AGBPlayerCharacter* Player)
	{
		if (!E3BActive(World))
		{
			return;
		}
		FVector Where = E3BStand;
		// An encounter whose enemies all lie dormant wakes them by his coming close (DormantWakeRange): walk up to the nearest one.
		TArray<AGBEnemyCharacter*> Enemies;
		E3BCollectNew(World, Enemies);
		bool bAnyAwake = false;
		for (const AGBEnemyCharacter* Enemy : Enemies)
		{
			bAnyAwake |= Enemy->GetUnawareBehavior() != EGBUnawareBehavior::Dormant;
		}
		if (!bAnyAwake && Enemies.Num() > 0)
		{
			const AGBEnemyCharacter* Nearest = Enemies[0];
			for (const AGBEnemyCharacter* Enemy : Enemies)
			{
				if (FVector::Dist2D(Enemy->GetActorLocation(), E3BStand) < FVector::Dist2D(Nearest->GetActorLocation(), E3BStand))
				{
					Nearest = Enemy;
				}
			}
			const FVector Toward = (E3BStand - Nearest->GetActorLocation()).GetSafeNormal2D();
			Where = Nearest->GetActorLocation() + Toward * 450.f - FVector(0.f, 0.f, Nearest->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
		}
		// Deterministic "in view": the player stands in front of ONE specific enemy (inside its sight cone, in range, clear line) and that
		// enemy must notice him; whether some patroller happens to face him by chance no longer decides the check.
		E3BWatcher.Reset();
		if (bAnyAwake)
		{
			for (AGBEnemyCharacter* Enemy : Enemies)
			{
				FVector Foot;
				if (Enemy->UsesAwareness() && Enemy->GetUnawareBehavior() != EGBUnawareBehavior::Dormant && !Enemy->IsAware() && E3BFindSpotInFront(World, Enemy, Player, Foot))
				{
					Where = Foot;
					E3BWatcher = Enemy;
					if (AGBPlayerController* PC = Cast<AGBPlayerController>(Player->GetController()))
					{
						PC->SetControlRotation((Enemy->GetActorLocation() - (Foot + FVector(0.f, 0.f, 170.f))).Rotation());
					}
					break;
				}
			}
		}
		E3BTeleport(Player, Where, false);
		UE_LOG(LogGB, Display, TEXT("SMOKE Enemies3DBase %s: player at %s in front of %s"), *E3BId.ToString(), *Where.ToCompactString(), E3BWatcher.IsValid() ? *E3BWatcher->GetEnemyId().ToString() : TEXT("nobody (all dormant or no clear spot)"));
	}

	static void E3BRecord(UWorld* World, AGBPlayerCharacter* Player)
	{
		if (!E3BActive(World))
		{
			return;
		}
		const AGBEncounter* Encounter = E3BFindEncounter(World, E3BId);
		if (!Encounter)
		{
			return;
		}
		TArray<AGBEnemyCharacter*> Enemies;
		E3BCollectNew(World, Enemies);
		E3BSpawned.Reset();
		int32 Aware = 0;
		for (AGBEnemyCharacter* Enemy : Enemies)
		{
			FE3BSpawn Spawn;
			Spawn.Enemy = Enemy;
			Spawn.Id = Enemy->GetEnemyId();
			Spawn.Start = Enemy->GetActorLocation();
			Spawn.StartDistance = FVector::Dist(Spawn.Start, Player->GetActorLocation());
			E3BSpawned.Add(Spawn);
			Aware += Enemy->IsAware() ? 1 : 0;
		}
		UE_LOG(LogGB, Display, TEXT("SMOKE Enemies3DBase %s: state=%d wave=%d of %d, %d enemies, %d alive, %d aware"), *E3BId.ToString(), static_cast<int32>(Encounter->GetState()),
			Encounter->GetCurrentWave() + 1, Encounter->GetNumWaves(), E3BSpawned.Num(), Encounter->GetAliveCount(), Aware);
		if (E3BWatcher.IsValid())
		{
			// Only the enemy he stands in front of must have noticed him (its sight cone, range and line are guaranteed by the spot).
			if (!E3BWatcher->IsAware() && !E3BWatcher->IsDead())
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE Enemies3DBase %s FAILED: %s, which has the player in its sight cone in range with a clear line, did not notice him within 2.7 s"), *E3BId.ToString(), *E3BWatcher->GetEnemyId().ToString());
			}
			if (Encounter->GetState() != EGBEncounterState::Active)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE Enemies3DBase %s FAILED: the encounter did not become Active after its enemy noticed the player (state %d)"), *E3BId.ToString(), static_cast<int32>(Encounter->GetState()));
			}
		}
		else if (Encounter->GetState() != EGBEncounterState::Active)
		{
			UE_LOG(LogGB, Error, TEXT("SMOKE Enemies3DBase %s FAILED: the encounter did not become Active after the player came near (state %d)"), *E3BId.ToString(), static_cast<int32>(Encounter->GetState()));
		}
		if (E3BSpawned.Num() == 0)
		{
			UE_LOG(LogGB, Error, TEXT("SMOKE Enemies3DBase %s FAILED: wave 1 is empty"), *E3BId.ToString());
		}
	}

	/** Every living enemy of the encounter is inside its zone (a little tolerance for the capsule). */
	static void E3BZoneSample(UWorld* World, const TCHAR* Where)
	{
		const AGBEncounter* Encounter = E3BFindEncounter(World, E3BId);
		if (!E3BActive(World) || !Encounter)
		{
			return;
		}
		const FBox Zone = Encounter->GetZone().ExpandBy(120.f);
		TArray<AGBEnemyCharacter*> Enemies;
		E3BCollectNew(World, Enemies);
		for (const AGBEnemyCharacter* Enemy : Enemies)
		{
			++E3BZoneSamples;
			if (!Zone.IsInside(Enemy->GetActorLocation()))
			{
				++E3BZoneViolations;
				UE_LOG(LogGB, Error, TEXT("SMOKE Enemies3DBase %s FAILED (%s): %s is outside its zone at %s"), *E3BId.ToString(), Where, *Enemy->GetEnemyId().ToString(), *Enemy->GetActorLocation().ToCompactString());
			}
		}
	}

	/** At least half of the spawned enemies moved towards the player, strafed, or shot; all inside the zone. Then the camera looks at them. */
	static void E3BBehave(UWorld* World, AGBPlayerCharacter* Player)
	{
		if (!E3BActive(World))
		{
			return;
		}
		E3BZoneSamples = 0;
		E3BZoneViolations = 0;
		E3BZoneSample(World, TEXT("fight"));
		E3BActedCount = 0;
		FVector Sum = FVector::ZeroVector;
		int32 Alive = 0;
		for (const FE3BSpawn& Spawn : E3BSpawned)
		{
			const AGBEnemyCharacter* Enemy = Spawn.Enemy.Get();
			if (!Enemy || Enemy->IsDead())
			{
				continue;
			}
			const float Distance = FVector::Dist(Enemy->GetActorLocation(), Player->GetActorLocation());
			const float Moved = FVector::Dist2D(Enemy->GetActorLocation(), Spawn.Start);
			int32 Attacks = 0;
			float Pelvis = 0.f;
			if (const AGBSkeletalEnemy* Skeletal = Cast<AGBSkeletalEnemy>(Enemy))
			{
				Attacks = Skeletal->GetShotCount() + Skeletal->GetLungeCount();
				Pelvis = Skeletal->GetMaxPelvisDistance();
			}
			else if (const AGBFlyerEnemy* Flyer = Cast<AGBFlyerEnemy>(Enemy))
			{
				Attacks = Flyer->GetShotCount();
			}
			const bool bActed = Spawn.StartDistance - Distance >= 100.f || Moved >= 100.f || Attacks > 0 || Enemy->IsAware();
			E3BActedCount += bActed ? 1 : 0;
			Sum += Enemy->GetActorLocation();
			++Alive;
			UE_LOG(LogGB, Display, TEXT("SMOKE Enemies3DBase %s: %s moved=%.0f distance %.0f -> %.0f attacks=%d aware=%d maxPelvis=%.0f acted=%d | %s"), *E3BId.ToString(), *Spawn.Id.ToString(), Moved, Spawn.StartDistance, Distance, Attacks,
				Enemy->IsAware() ? 1 : 0, Pelvis, bActed ? 1 : 0, *Enemy->DescribeTactics());
			if (Pelvis > 60.f)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE Enemies3DBase %s FAILED: %s pelvis was %.0f cm from its capsule"), *E3BId.ToString(), *Spawn.Id.ToString(), Pelvis);
			}
		}
		UE_LOG(LogGB, Display, TEXT("SMOKE Enemies3DBase %s: %d of %d enemies acted, hitscan shots=%d"), *E3BId.ToString(), E3BActedCount, E3BSpawned.Num(), GBEnemy3D::GetShotsFired());
		if (E3BSpawned.Num() > 0 && E3BActedCount * 2 < E3BSpawned.Num())
		{
			UE_LOG(LogGB, Error, TEXT("SMOKE Enemies3DBase %s FAILED: only %d of %d enemies reacted to the player"), *E3BId.ToString(), E3BActedCount, E3BSpawned.Num());
		}
		if (Alive > 0)
		{
			if (AGBPlayerController* PC = Cast<AGBPlayerController>(Player->GetController()))
			{
				PC->SetControlRotation(((Sum / Alive) - Player->GetAimOrigin()).Rotation());
			}
		}
	}

	/** The player is flown 50 m away from the zone: nobody leaves it for the next seconds. */
	static void E3BZoneOut(UWorld* World, AGBPlayerCharacter* Player)
	{
		if (!E3BActive(World))
		{
			return;
		}
		const AGBEncounter* Encounter = E3BFindEncounter(World, E3BId);
		FVector Away = Encounter ? (E3BStand - Encounter->GetZone().GetCenter()).GetSafeNormal2D() : FVector::ZeroVector;
		if (Away.IsNearlyZero())
		{
			Away = FVector(1.f, 0.f, 0.f);
		}
		E3BTeleport(Player, E3BStand + Away * 5000.f, true);
		UE_LOG(LogGB, Display, TEXT("SMOKE Enemies3DBase %s: player flown 50 m away from the zone to %s"), *E3BId.ToString(), *Player->GetActorLocation().ToCompactString());
	}

	static void E3BZoneBack(UWorld* World, AGBPlayerCharacter* Player)
	{
		if (!E3BActive(World))
		{
			return;
		}
		E3BZoneSample(World, TEXT("player far away"));
		UE_LOG(LogGB, Display, TEXT("SMOKE Enemies3DBase %s: zone check %d samples, %d outside"), *E3BId.ToString(), E3BZoneSamples, E3BZoneViolations);
		E3BTeleport(Player, E3BStand, false);
	}

	/** The player stands just outside the zone, in view: nobody may leave it, and no spider may lunge out. */
	static void E3BEdgeOut(UWorld* World, AGBPlayerCharacter* Player)
	{
		if (!E3BActive(World))
		{
			return;
		}
		const AGBEncounter* Encounter = E3BFindEncounter(World, E3BId);
		if (!Encounter)
		{
			return;
		}
		TArray<AGBEnemyCharacter*> Enemies;
		E3BCollectNew(World, Enemies);
		if (Enemies.Num() == 0)
		{
			return;
		}
		const FBox Zone = Encounter->GetZone();
		const AGBEnemyCharacter* Nearest = Enemies[0];
		for (const AGBEnemyCharacter* Enemy : Enemies)
		{
			if (FVector::Dist2D(Enemy->GetActorLocation(), E3BStand) < FVector::Dist2D(Nearest->GetActorLocation(), E3BStand))
			{
				Nearest = Enemy;
			}
		}
		FVector Away = (E3BStand - Zone.GetCenter()).GetSafeNormal2D();
		if (Away.IsNearlyZero())
		{
			Away = FVector(1.f, 0.f, 0.f);
		}
		FVector Point = Nearest->GetActorLocation();
		for (int32 Step = 0; Step < 400 && Zone.IsInside(FVector(Point.X, Point.Y, Zone.GetCenter().Z)); ++Step)
		{
			Point += Away * 50.f;
		}
		Point += Away * 250.f;
		E3BTeleport(Player, FVector(Point.X, Point.Y, Nearest->GetActorLocation().Z - Nearest->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()), true);
		E3BZoneSamples = 0;
		E3BZoneViolations = 0;
		UE_LOG(LogGB, Display, TEXT("SMOKE Enemies3DBase %s: player 250 cm outside the zone at %s (nearest enemy %s)"), *E3BId.ToString(), *Point.ToCompactString(), *Nearest->GetEnemyId().ToString());
	}

	/** The lose-target time has passed: every enemy is unaware again and within 150 cm of its post, or back on its route. */
	static void E3BLoseCheck(UWorld* World)
	{
		if (!E3BActive(World))
		{
			return;
		}
		TArray<AGBEnemyCharacter*> Enemies;
		E3BCollectNew(World, Enemies);
		int32 Home = 0;
		int32 Aware = 0;
		int32 Bad = 0;
		for (const AGBEnemyCharacter* Enemy : Enemies)
		{
			const float FromPost = FVector::Dist2D(Enemy->GetActorLocation(), Enemy->GetPost());
			const bool bOnRoute = Enemy->GetUnawareBehavior() == EGBUnawareBehavior::Patrol && Enemy->GetRouteNum() >= 2 && !Enemy->IsReturningToPost();
			const bool bOk = !Enemy->IsAware() && (FromPost <= 150.f || bOnRoute);
			Aware += Enemy->IsAware() ? 1 : 0;
			Home += bOk ? 1 : 0;
			Bad += bOk ? 0 : 1;
			UE_LOG(LogGB, Display, TEXT("SMOKE Enemies3DBase %s: after losing him: %s aware=%d returning=%d from its post=%.0f cm behaviour=%d route=%d ok=%d"), *E3BId.ToString(), *Enemy->GetEnemyId().ToString(), Enemy->IsAware() ? 1 : 0,
				Enemy->IsReturningToPost() ? 1 : 0, FromPost, static_cast<int32>(Enemy->GetUnawareBehavior()), Enemy->GetRouteNum(), bOk ? 1 : 0);
		}
		UE_LOG(LogGB, Display, TEXT("SMOKE Enemies3DBase %s: lose-target check: %d enemies, %d back at the post or on their route, %d still aware"), *E3BId.ToString(), Enemies.Num(), Home, Aware);
		if (Bad > 0)
		{
			UE_LOG(LogGB, Error, TEXT("SMOKE Enemies3DBase %s FAILED: %d of %d enemies did not go back to their post after losing the player (%d still aware)"), *E3BId.ToString(), Bad, Enemies.Num(), Aware);
		}
	}

	/** Reinforcement waves: none may appear in the player's view unless the wait for a hidden point ran out. */
	static void E3BCheckReinforce(UWorld* World)
	{
		const AGBEncounter* Encounter = E3BFindEncounter(World, E3BId);
		if (!Encounter || Encounter->GetReinforceWaves() <= E3BLastReinforceWaves)
		{
			return;
		}
		E3BLastReinforceWaves = Encounter->GetReinforceWaves();
		UE_LOG(LogGB, Display, TEXT("SMOKE Enemies3DBase %s: reinforcement wave %d appeared: %d enemies in the player's view, wait ran out=%d"), *E3BId.ToString(), E3BLastReinforceWaves, Encounter->GetLastReinforceVisible(), Encounter->DidLastReinforceWaitRunOut() ? 1 : 0);
		if (Encounter->GetLastReinforceVisible() > 0 && !Encounter->DidLastReinforceWaitRunOut())
		{
			UE_LOG(LogGB, Error, TEXT("SMOKE Enemies3DBase %s FAILED: %d reinforcements appeared in the player's view before the wait for a hidden point ran out"), *E3BId.ToString(), Encounter->GetLastReinforceVisible());
		}
	}

	// ---- The Valkyrie in her tube: sealed while the glass stands (WakeFlag), wakes and comes out when it breaks.
	static FVector E3VSpawn = FVector::ZeroVector;
	static float E3VHealth0 = 0.f;
	static const TCHAR* const E3VWakeFlag = TEXT("ValkyrieTubeBroken");

	static AGBEnemyCharacter* E3VFind(UWorld* World)
	{
		TArray<AGBEnemyCharacter*> Enemies;
		E3BCollectNew(World, Enemies);
		return Enemies.Num() > 0 ? Enemies[0] : nullptr;
	}

	static void E3VBegin(UWorld* World, AGBPlayerCharacter* Player)
	{
		if (!E3BActive(World))
		{
			return;
		}
		E3BId = FName(TEXT("Boss_Valkyrie_Future"));
		E3BLastReinforceWaves = 0;
		AGBEncounter* Encounter = E3BFindEncounter(World, E3BId);
		UGBProgressSubsystem* Progress = UGBProgressSubsystem::Get(World);
		Player->GetAttributes()->SetInvulnerable(true);
		if (!Encounter || !Progress)
		{
			UE_LOG(LogGB, Error, TEXT("SMOKE Enemies3DBase Valkyrie FAILED: no Boss_Valkyrie_Future encounter in the map"));
			return;
		}
		// The gate's flags are met (she must be there), the tube's flag is not.
		for (const FName& Flag : Encounter->RequiredFlags)
		{
			Progress->SetFlag(Flag, true);
		}
		Progress->SetFlag(FName(E3VWakeFlag), false);
		UE_LOG(LogGB, Display, TEXT("SMOKE Enemies3DBase Valkyrie: encounter state=%d, wake flag cleared, required flags set"), static_cast<int32>(Encounter->GetState()));
	}

	/** In front of the tube for a while, with the flag unset: she stays asleep and unhurt, also when she is hit. */
	static void E3VRecord(UWorld* World, AGBPlayerCharacter* Player)
	{
		if (!E3BActive(World))
		{
			return;
		}
		const AGBEncounter* Encounter = E3BFindEncounter(World, E3BId);
		AGBEnemyCharacter* Valkyrie = E3VFind(World);
		if (!Encounter || !Valkyrie)
		{
			UE_LOG(LogGB, Error, TEXT("SMOKE Enemies3DBase Valkyrie FAILED: she is not standing in the world (state %d)"), Encounter ? static_cast<int32>(Encounter->GetState()) : -1);
			return;
		}
		E3VSpawn = Valkyrie->GetActorLocation();
		E3VHealth0 = Valkyrie->GetAttributes()->GetHealth();
		FHitResult Ground;
		FVector Floor = Encounter->GetActorLocation();
		if (World->LineTraceSingleByObjectType(Ground, Floor + FVector(0.f, 0.f, 200.f), Floor - FVector(0.f, 0.f, 2000.f), FCollisionObjectQueryParams(ECC_WorldStatic)))
		{
			Floor = Ground.ImpactPoint;
		}
		E3BTeleport(Player, Floor, false);
		UE_LOG(LogGB, Display, TEXT("SMOKE Enemies3DBase Valkyrie: she lies at %s sealed=%d health %.0f; player in the room at %s"), *E3VSpawn.ToCompactString(), Valkyrie->IsSealed() ? 1 : 0, E3VHealth0, *Floor.ToCompactString());
		if (!Valkyrie->IsSealed())
		{
			UE_LOG(LogGB, Error, TEXT("SMOKE Enemies3DBase Valkyrie FAILED: she is not sealed while the wake flag is unset"));
		}
	}

	static void E3VAsleep(UWorld* World, AGBPlayerCharacter* Player, bool bHit, const TCHAR* When)
	{
		if (!E3BActive(World))
		{
			return;
		}
		const AGBEncounter* Encounter = E3BFindEncounter(World, E3BId);
		AGBEnemyCharacter* Valkyrie = E3VFind(World);
		if (!Encounter || !Valkyrie)
		{
			return;
		}
		if (bHit)
		{
			FGBDamageInfo Info;
			Info.Amount = 500.f;
			Info.Instigator = Player;
			Info.HitLocation = Valkyrie->GetActorLocation();
			Info.HitDirection = (Valkyrie->GetActorLocation() - Player->GetActorLocation()).GetSafeNormal();
			Info.SourceId = TEXT("SmokeValkyrieSealed");
			UGBCombatLibrary::DealDamage(Valkyrie, Info);
		}
		const float Health = Valkyrie->GetAttributes()->GetHealth();
		const float Moved = FVector::Dist2D(Valkyrie->GetActorLocation(), E3VSpawn);
		UE_LOG(LogGB, Display, TEXT("SMOKE Enemies3DBase Valkyrie (%s): aware=%d sealed=%d health %.0f -> %.0f moved=%.0f encounter state=%d"), When, Valkyrie->IsAware() ? 1 : 0, Valkyrie->IsSealed() ? 1 : 0, E3VHealth0, Health, Moved, static_cast<int32>(Encounter->GetState()));
		if (Valkyrie->IsAware() || !Valkyrie->IsSealed() || Health != E3VHealth0 || Moved > 30.f || Encounter->GetState() != EGBEncounterState::Waiting)
		{
			UE_LOG(LogGB, Error, TEXT("SMOKE Enemies3DBase Valkyrie FAILED (%s): she must stay asleep, unhurt and in place while the flag is unset (aware %d, health %.0f -> %.0f, moved %.0f, state %d)"), When, Valkyrie->IsAware() ? 1 : 0, E3VHealth0, Health, Moved, static_cast<int32>(Encounter->GetState()));
		}
	}

	static void E3VSetFlag(UWorld* World, bool bValue)
	{
		if (!E3BActive(World))
		{
			return;
		}
		if (bValue)
		{
			// The glass is not broken by the check (its final collision comes with the next Editor session): its collision is switched off,
			// which is what a broken tube leaves her.
			for (TActorIterator<AGBBreakable> It(World); It; ++It)
			{
				if (It->BrokenFlag == FName(E3VWakeFlag))
				{
					It->SetActorEnableCollision(false);
				}
			}
		}
		if (UGBProgressSubsystem* Progress = UGBProgressSubsystem::Get(World))
		{
			Progress->SetFlag(FName(E3VWakeFlag), bValue);
			UE_LOG(LogGB, Display, TEXT("SMOKE Enemies3DBase Valkyrie: wake flag set to %d"), bValue ? 1 : 0);
		}
	}

	/** bWoken: 1.5 s after the flag she is awake and the encounter Active; later: she left the tube, on the ground, inside the zone. */
	static void E3VWoken(UWorld* World, bool bLeft)
	{
		if (!E3BActive(World))
		{
			return;
		}
		const AGBEncounter* Encounter = E3BFindEncounter(World, E3BId);
		const AGBEnemyCharacter* Valkyrie = E3VFind(World);
		if (!Encounter || !Valkyrie)
		{
			UE_LOG(LogGB, Error, TEXT("SMOKE Enemies3DBase Valkyrie FAILED: she is gone after the flag"));
			return;
		}
		const float Away = FVector::Dist2D(Valkyrie->GetActorLocation(), E3VSpawn);
		const bool bGround = Valkyrie->GetCharacterMovement()->IsMovingOnGround();
		const bool bInside = Encounter->GetZone().ExpandBy(120.f).IsInside(Valkyrie->GetActorLocation());
		UE_LOG(LogGB, Display, TEXT("SMOKE Enemies3DBase Valkyrie (%s): aware=%d sealed=%d leaping=%d state=%d away from her spawn=%.0f cm onGround=%d insideZone=%d"), bLeft ? TEXT("left the tube") : TEXT("woke"), Valkyrie->IsAware() ? 1 : 0,
			Valkyrie->IsSealed() ? 1 : 0, Valkyrie->IsWakeLeaping() ? 1 : 0, static_cast<int32>(Encounter->GetState()), Away, bGround ? 1 : 0, bInside ? 1 : 0);
		if (!bLeft)
		{
			if (!Valkyrie->IsAware() || Valkyrie->IsSealed() || Encounter->GetState() != EGBEncounterState::Active)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE Enemies3DBase Valkyrie FAILED: 1.5 s after the flag she is not awake and the encounter not Active (aware %d, sealed %d, state %d)"), Valkyrie->IsAware() ? 1 : 0, Valkyrie->IsSealed() ? 1 : 0, static_cast<int32>(Encounter->GetState()));
			}
		}
		else if (Away <= 250.f || !bGround || !bInside || Encounter->GetState() != EGBEncounterState::Active)
		{
			UE_LOG(LogGB, Error, TEXT("SMOKE Enemies3DBase Valkyrie FAILED: she did not come out (away %.0f cm, onGround %d, insideZone %d)"), Away, bGround ? 1 : 0, bInside ? 1 : 0);
		}
	}

	/** Kills every enemy the encounter has put in the world so far (the next wave comes after its delay). */
	static void E3BKill(UWorld* World, AGBPlayerCharacter* Player)
	{
		if (!E3BActive(World))
		{
			return;
		}
		E3BCheckReinforce(World);
		TArray<AGBEnemyCharacter*> Enemies;
		E3BCollectNew(World, Enemies);
		for (AGBEnemyCharacter* Enemy : Enemies)
		{
			FGBDamageInfo Info;
			Info.Amount = 1000000.f;
			Info.Instigator = Player;
			Info.HitLocation = Enemy->GetActorLocation();
			Info.HitDirection = Enemy->GetActorForwardVector();   // from behind: a shield blocks the front
			Info.SourceId = TEXT("SmokeEnemies3D");
			UGBCombatLibrary::DealDamage(Enemy, Info);
			E3BKilled += Enemy->IsDead() ? 1 : 0;
		}
		const AGBEncounter* Encounter = E3BFindEncounter(World, E3BId);
		UE_LOG(LogGB, Display, TEXT("SMOKE Enemies3DBase %s: killed %d (total %d), state=%d wave=%d of %d"), *E3BId.ToString(), Enemies.Num(), E3BKilled,
			Encounter ? static_cast<int32>(Encounter->GetState()) : -1, Encounter ? Encounter->GetCurrentWave() + 1 : 0, Encounter ? Encounter->GetNumWaves() : 0);
	}

	/** Cleared, its flag set, the Power Cell dropped where it should. One summary line per encounter. */
	static void E3BFinish(UWorld* World)
	{
		if (!E3BActive(World))
		{
			return;
		}
		const AGBEncounter* Encounter = E3BFindEncounter(World, E3BId);
		const UGBProgressSubsystem* Progress = UGBProgressSubsystem::Get(World);
		const bool bCleared = Encounter && Encounter->GetState() == EGBEncounterState::Cleared;
		const FName Flag = Encounter ? Encounter->GetClearedFlag() : NAME_None;
		const bool bFlag = Progress && !Flag.IsNone() && Progress->HasFlag(Flag);
		const int32 Cells = E3BCountPowerCells(World) - E3BPowerCellsBefore;
		const bool bWantsCell = E3BId == FName(TEXT("SubBoss_Future")) || E3BId == FName(TEXT("Future_DeepWing"));
		UE_LOG(LogGB, Display, TEXT("SMOKE Enemies3DBase %s summary: spawned=%d acted=%d shots=%d cleared=%d flag=%s(%d) powerCellDrops=%d"), *E3BId.ToString(), E3BSpawned.Num(), E3BActedCount,
			GBEnemy3D::GetShotsFired(), bCleared ? 1 : 0, *Flag.ToString(), bFlag ? 1 : 0, Cells);
		if (!bCleared)
		{
			UE_LOG(LogGB, Error, TEXT("SMOKE Enemies3DBase %s FAILED: not Cleared after killing the waves"), *E3BId.ToString());
		}
		if (!bFlag)
		{
			UE_LOG(LogGB, Error, TEXT("SMOKE Enemies3DBase %s FAILED: its cleared flag '%s' is not set"), *E3BId.ToString(), *Flag.ToString());
		}
		if (E3BId == FName(TEXT("SubBoss_Future")) && Flag != FName(TEXT("FutureSubBossDefeated")))
		{
			UE_LOG(LogGB, Error, TEXT("SMOKE Enemies3DBase SubBoss_Future FAILED: it must set FutureSubBossDefeated, not '%s'"), *Flag.ToString());
		}
		if (bWantsCell && Cells < 1)
		{
			UE_LOG(LogGB, Error, TEXT("SMOKE Enemies3DBase %s FAILED: no PowerCell pickup dropped"), *E3BId.ToString());
		}
	}

	// ---- gb.ViewPose, gb.ViewFreeze, gb.HandOverlay (first-person gun pose in data, fixed weapon FOV) ----
	// The held gun, or null (no player, or no gun in hand).
	static AGBGun* ViewPoseGun(UWorld* World)
	{
		const AGBPlayerCharacter* Player = AGBPlayerCharacter::Get(World);
		const UGBEquipmentComponent* Equipment = Player ? Player->GetEquipment() : nullptr;
		return Cast<AGBGun>(Equipment ? Equipment->GetActiveItem() : nullptr);
	}

	// Loads a hand-overlay PNG (or clears it with an empty path). False when nothing was set.
	static bool SetHandOverlayFile(UWorld* World, const FString& Path)
	{
		AGBHUD* HUD = AGBHUD::Get(World);
		if (!HUD)
		{
			UE_LOG(LogGB, Error, TEXT("gb.HandOverlay: no HUD"));
			return false;
		}
		if (Path.IsEmpty())
		{
			HUD->ClearHandOverlay();
			UE_LOG(LogGB, Display, TEXT("gb.HandOverlay: cleared"));
			return true;
		}
		if (!FPaths::FileExists(Path))
		{
			// Warning, not Error: the filtered smoke run (gb.Smoke ViewPose) feeds a bad path on purpose and must stay green.
			// A failed load clears any overlay already shown.
			HUD->ClearHandOverlay();
			UE_LOG(LogGB, Warning, TEXT("gb.HandOverlay: file not found '%s' (absolute path to a 480x270 PNG)"), *Path);
			return false;
		}
		UTexture2D* Texture = FImageUtils::ImportFileAsTexture2D(Path);
		if (!Texture)
		{
			HUD->ClearHandOverlay();
			UE_LOG(LogGB, Warning, TEXT("gb.HandOverlay: could not load '%s' as a texture (need a readable PNG, 480x270)"), *Path);
			return false;
		}
		if (Texture->GetSizeX() != 480 || Texture->GetSizeY() != 270)
		{
			UE_LOG(LogGB, Warning, TEXT("gb.HandOverlay: '%s' is %dx%d, expected 480x270 (drawn stretched)"), *Path, Texture->GetSizeX(), Texture->GetSizeY());
		}
		Texture->Filter = TF_Nearest;
		Texture->UpdateResource();
		HUD->SetHandOverlay(Texture);
		UE_LOG(LogGB, Display, TEXT("gb.HandOverlay: showing '%s'"), *Path);
		return true;
	}

	static void RunViewPose(const TArray<FString>& Args, UWorld* World)
	{
		AGBGun* Gun = ViewPoseGun(World);
		if (!Gun)
		{
			UE_LOG(LogGB, Warning, TEXT("gb.ViewPose: no gun in hand"));
			return;
		}
		if (Args.Num() == 0)
		{
			UE_LOG(LogGB, Display, TEXT("gb.ViewPose %s rest pose (paste into Weapons.json Stats):\n%s"), *Gun->GetWeaponId().ToString(), *Gun->GetPoseJsonLines());
			return;
		}
		if (Args.Num() != 7)
		{
			UE_LOG(LogGB, Warning, TEXT("gb.ViewPose: usage: gb.ViewPose X Y Z Pitch Yaw Roll Scale (no args prints the pose)"));
			return;
		}
		FGBViewPose Pose;
		Pose.Offset = FVector(FCString::Atof(*Args[0]), FCString::Atof(*Args[1]), FCString::Atof(*Args[2]));
		Pose.Rotation = FRotator(FCString::Atof(*Args[3]), FCString::Atof(*Args[4]), FCString::Atof(*Args[5]));
		Pose.Scale = FMath::Max(0.01f, FCString::Atof(*Args[6]));
		Gun->SetLivePose(Pose);
		UE_LOG(LogGB, Display, TEXT("gb.ViewPose: live pose set on %s (cleared on re-equip)"), *Gun->GetWeaponId().ToString());
	}

	static void RunViewFreeze(const TArray<FString>& Args, UWorld* World)
	{
		(void)World;
		if (Args.Num() < 1)
		{
			UE_LOG(LogGB, Warning, TEXT("gb.ViewFreeze: usage: gb.ViewFreeze 0|1 (freezes the walk bob of gun and hands)"));
			return;
		}
		GBViewBob::bFrozen = Args[0] == TEXT("1") || Args[0].ToBool();
		UE_LOG(LogGB, Display, TEXT("gb.ViewFreeze: bob %s"), GBViewBob::bFrozen ? TEXT("frozen") : TEXT("running"));
	}

	static void RunHandOverlay(const TArray<FString>& Args, UWorld* World)
	{
		// Paths may contain spaces: the args were split on them, so join them back.
		FString Path;
		for (int32 Index = 0; Index < Args.Num(); ++Index)
		{
			if (Index > 0)
			{
				Path += TEXT(" ");
			}
			Path += Args[Index];
		}
		SetHandOverlayFile(World, Path);
	}

	// ---- Castle block (gb.Smoke Castle): the castle's pixel-art enemies (sprite sheets) with the squad AI of the future.
	// One of each castle id (two barghests: a pack) round the tactics boxes; the player stands still and is healed every frame.
	struct FCastleProjectile
	{
		FVector Start = FVector::ZeroVector;
		float MaxTravel = 0.f;
		FName Source = NAME_None;
	};
	static TMap<TWeakObjectPtr<AActor>, FCastleProjectile> CastleProjectiles;
	static FTSTicker::FDelegateHandle CastleTicker;
	/** Hits on the player per enemy ACTOR (two of one id, like the barghests, must each land their own). */
	static TMap<TWeakObjectPtr<AActor>, int32> CastleHits;
	static FDelegateHandle CastleHitHandle;
	static FTSTicker::FDelegateHandle CastleSpawnTicker;
	static float CastleNavWaited = 0.f;
	static TMap<FName, float> CastleBestFlight;

	static bool CastleIsCastleId(FName Id)
	{
		static const TCHAR* const Ids[] = { TEXT("Orc_Past"), TEXT("Orc_Past_Champion"), TEXT("Orc_Past_Shaman"), TEXT("Orc_Past_Juggernaut"), TEXT("Orc_Past_Brute"), TEXT("Imp_Past"), TEXT("Centaur_Past") };
		for (const TCHAR* Each : Ids)
		{
			if (Id == FName(Each))
			{
				return true;
			}
		}
		return false;
	}

	/** The sprite quad's UV direction, read from the engine plane's own vertex data (the editor keeps it): must match the sprite look's assumption. */
	static void CastlePlaneCheck()
	{
		UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
		const FStaticMeshRenderData* Render = Plane ? Plane->GetRenderData() : nullptr;
		if (!Render || Render->LODResources.Num() == 0)
		{
			UE_LOG(LogGB, Display, TEXT("SMOKE Castle plane UVs: no render data, not checked"));
			return;
		}
		const FStaticMeshLODResources& Lod = Render->LODResources[0];
		const FPositionVertexBuffer& Positions = Lod.VertexBuffers.PositionVertexBuffer;
		const FStaticMeshVertexBuffer& Uvs = Lod.VertexBuffers.StaticMeshVertexBuffer;
		if (Positions.GetNumVertices() == 0 || Uvs.GetNumVertices() != Positions.GetNumVertices() || !Positions.GetVertexData() || !Uvs.GetTexCoordData())
		{
			UE_LOG(LogGB, Display, TEXT("SMOKE Castle plane UVs: the CPU copy of the vertices is not kept here, not checked"));
			return;
		}
		uint32 MinX = 0, MaxX = 0, MinY = 0, MaxY = 0;
		for (uint32 Index = 1; Index < Positions.GetNumVertices(); ++Index)
		{
			const FVector3f& P = Positions.VertexPosition(Index);
			MinX = P.X < Positions.VertexPosition(MinX).X ? Index : MinX;
			MaxX = P.X > Positions.VertexPosition(MaxX).X ? Index : MaxX;
			MinY = P.Y < Positions.VertexPosition(MinY).Y ? Index : MinY;
			MaxY = P.Y > Positions.VertexPosition(MaxY).Y ? Index : MaxY;
		}
		const bool bUAlongX = Uvs.GetVertexUV(MaxX, 0).X > Uvs.GetVertexUV(MinX, 0).X;
		const bool bVAlongY = Uvs.GetVertexUV(MaxY, 0).Y > Uvs.GetVertexUV(MinY, 0).Y;
		UE_LOG(LogGB, Display, TEXT("SMOKE Castle plane UVs: U grows along +X=%d, V grows along +Y=%d (the sprite look assumes U=%d, V=1)"), bUAlongX ? 1 : 0, bVAlongY ? 1 : 0,
			UGBSpriteAnimComponent::PlaneUGrowsAlongX() ? 1 : 0);
		if (bUAlongX != UGBSpriteAnimComponent::PlaneUGrowsAlongX() || !bVAlongY)
		{
			UE_LOG(LogGB, Error, TEXT("SMOKE Castle FAILED: the sprite quad would draw the sheets mirrored or upside down (fix GBSpriteAnimLocal::bPlaneUAlongX / the V flip)"));
		}
	}

	/** Every frame: the player is healed (eight enemies at once), projectiles of castle enemies are followed. */
	static void CastleStartTicker(AGBPlayerCharacter* Player)
	{
		CastleProjectiles.Reset();
		CastleBestFlight.Reset();
		CastleHits.Reset();
		UGBCombatLibrary::OnDamageDealt().Remove(CastleHitHandle);
		CastleHitHandle = UGBCombatLibrary::OnDamageDealt().AddLambda([](AActor* Target, const FGBDamageInfo& Info, float Dealt)
		{
			AActor* Attacker = Info.Instigator ? Info.Instigator.Get() : Info.Causer.Get();
			if (Dealt > 0.f && Attacker && Cast<AGBPlayerCharacter>(Target))
			{
				CastleHits.FindOrAdd(TWeakObjectPtr<AActor>(Attacker)) += 1;
			}
		});
		FTSTicker::GetCoreTicker().RemoveTicker(CastleTicker);
		CastleTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakPlayer = TWeakObjectPtr<AGBPlayerCharacter>(Player)](float)
		{
			AGBPlayerCharacter* TickPlayer = WeakPlayer.Get();
			if (!TickPlayer)
			{
				return false;
			}
			TickPlayer->GetAttributes()->RestoreAll();
			for (TActorIterator<AGBProjectile> It(TickPlayer->GetWorld()); It; ++It)
			{
				const FName Source = It->GetConfig().Damage.SourceId;
				if (!CastleIsCastleId(Source))
				{
					continue;
				}
				FCastleProjectile& Seen = CastleProjectiles.FindOrAdd(TWeakObjectPtr<AActor>(*It));
				if (Seen.Source.IsNone())
				{
					Seen.Source = Source;
					Seen.Start = It->GetActorLocation();
					const FVector SeenStart = Seen.Start;
					It->OnImpact.AddLambda([Source, SeenStart](AGBProjectile*, AActor* HitActor, const FVector& Where)
					{
						UE_LOG(LogGB, Display, TEXT("SMOKE Castle projectile of %s ended %.0f cm from where it was first seen, on %s (from %s to %s)"), *Source.ToString(),
							FVector::Dist(SeenStart, Where), HitActor ? *HitActor->GetName() : TEXT("nothing (range or lifetime)"), *SeenStart.ToCompactString(), *Where.ToCompactString());
					});
				}
				Seen.MaxTravel = FMath::Max(Seen.MaxTravel, FVector::Dist(Seen.Start, It->GetActorLocation()));
				float& Best = CastleBestFlight.FindOrAdd(Source);
				Best = FMath::Max(Best, Seen.MaxTravel);
			}
			return true;
		}));
	}

	static void CastleSpawn(UWorld* World, AGBPlayerCharacter* Player)
	{
		CastleNavWaited = 0.f;
		FTSTicker::GetCoreTicker().RemoveTicker(CastleSpawnTicker);
		CastleSpawnTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakWorld = TWeakObjectPtr<UWorld>(World), WeakPlayer = TWeakObjectPtr<AGBPlayerCharacter>(Player)](float Dt)
		{
			UWorld* TickWorld = WeakWorld.Get();
			AGBPlayerCharacter* TickPlayer = WeakPlayer.Get();
			if (!TickWorld || !TickPlayer)
			{
				return false;
			}
			CastleNavWaited += Dt;
			if (UNavigationSystemV1::IsNavigationBeingBuilt(TickWorld) && CastleNavWaited < 4.f)
			{
				return true;
			}
			UE_LOG(LogGB, Display, TEXT("SMOKE Castle: the navmesh was ready after %.1f s, spawning one of each castle enemy"), CastleNavWaited);
			// The sheets may have been baked after the first try of this run: look again.
			if (UGBSpriteSheetSubsystem* Sheets = UGBSpriteSheetSubsystem::Get(TickWorld))
			{
				Sheets->ClearMissing();
			}
			if (UGBSquadSubsystem* Squad = UGBSquadSubsystem::Get(TickWorld))
			{
				Squad->ResetSmokeCounters();
			}
			using FItems = TArray<TPair<FName, float>>;
			const FItems Items = {
				{ FName(TEXT("Orc_Past")), 1100.f }, { FName(TEXT("Orc_Past_Champion")), 1200.f }, { FName(TEXT("Orc_Past_Shaman")), 1500.f },
				{ FName(TEXT("Orc_Past_Juggernaut")), 1100.f }, { FName(TEXT("Orc_Past_Brute")), 1300.f }, { FName(TEXT("Imp_Past")), 900.f },
				{ FName(TEXT("Imp_Past")), 1000.f }, { FName(TEXT("Centaur_Past")), 1400.f } };
			E3DSpawnGroup(TickWorld, TickPlayer, Items);
			CastleStartTicker(TickPlayer);
			MarkSim(TickWorld);   // the check counts the fight in simulated seconds (a slow first run gives fewer of them per wall second)
			for (const FE3DEntry& Entry : E3DGroup)
			{
				if (const AGBSkeletalEnemy* Enemy = Cast<AGBSkeletalEnemy>(Entry.Enemy.Get()))
				{
					UE_LOG(LogGB, Display, TEXT("SMOKE Castle spawned %s: %s"), *Entry.Id.ToString(), *Enemy->DescribeLook());
				}
				else
				{
					UE_LOG(LogGB, Error, TEXT("SMOKE Castle FAILED: %s is not a GBSkeletalEnemy (ActorClass in Enemies.json)"), *Entry.Id.ToString());
				}
			}
			E3DLookAtGroup(TickPlayer);
			return false;
		}));
	}

	/** Each enemy: its sheet (or why not), it moved, it attacked (clip played, hit on the player, a projectile that flew), cover, the pincer. */
	static void CastleCheck(UWorld* World, AGBPlayerCharacter* Player)
	{
		UE_LOG(LogGB, Display, TEXT("SMOKE Castle: the fight lasted %.1f simulated seconds since the spawn"), World ? World->GetTimeSeconds() - SimMark : 0.0);
		int32 CoveredRanged = 0;
		int32 Ranged = 0;
		for (const FE3DEntry& Entry : E3DGroup)
		{
			AGBSkeletalEnemy* Enemy = Cast<AGBSkeletalEnemy>(Entry.Enemy.Get());
			if (!Enemy)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE Castle FAILED: %s is gone before the check"), *Entry.Id.ToString());
				continue;
			}
			const FGBEnemyDefinition* Def = Enemy->GetDefinition();
			const FString SheetId = Def ? Def->Strings.FindRef(FName(TEXT("SpriteSheet"))) : FString();
			const bool bSheetOnDisk = !SheetId.IsEmpty() && FPaths::FileExists(UGBSpriteSheetSubsystem::SheetDir() / (SheetId + TEXT(".json")));
			const UGBSpriteAnimComponent* Anim = Enemy->GetSpriteAnim();
			if (!Enemy->HasSpriteLook())
			{
				const FString Why = Anim ? Anim->GetFailure() : FString(TEXT("no sprite component"));
				if (bSheetOnDisk)
				{
					UE_LOG(LogGB, Error, TEXT("SMOKE Castle FAILED: %s did not load its sheet %s although it is there: %s"), *Entry.Id.ToString(), *SheetId, *Why);
				}
				else
				{
					UE_LOG(LogGB, Display, TEXT("SMOKE Castle %s: sheet %s not baked yet, the static sprite stands in (%s)"), *Entry.Id.ToString(), *SheetId, *Why);
				}
			}
			const float Moved = FVector::Dist2D(Enemy->GetActorLocation(), Entry.Start);
			const int32 Hits = CastleHits.FindRef(TWeakObjectPtr<AActor>(Enemy));
			const bool bRanged = Enemy->FiresProjectiles();
			const float Flight = CastleBestFlight.FindRef(Entry.Id);
			UE_LOG(LogGB, Display, TEXT("SMOKE Castle %s: moved=%.0f hitsOnPlayer=%d projectiles=%d held(blocked)=%d bestFlight=%.0f swings=%d lunges=%d cover=%d coverTime=%.1f | %s"), *Entry.Id.ToString(), Moved, Hits,
				Enemy->GetProjectileCount(), Enemy->GetBlockedShots(), Flight, Enemy->GetMeleeSwingCount(), Enemy->GetLungeCount(), Enemy->HasPosition() && Enemy->GetPosition().bCovered ? 1 : 0, Enemy->GetCoverTime(), *Enemy->DescribeLook());
			if (Moved < 50.f && Hits == 0 && Enemy->GetProjectileCount() == 0)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE Castle FAILED: %s neither moved nor attacked"), *Entry.Id.ToString());
			}
			if (Enemy->HasSpriteLook() && Anim && Anim->GetActionsPlayed() == 0)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE Castle FAILED: %s never played an attack clip"), *Entry.Id.ToString());
			}
			if (bRanged)
			{
				++Ranged;
				CoveredRanged += (Enemy->GetCoverTime() > 0.f || (Enemy->HasPosition() && Enemy->GetPosition().bCovered)) ? 1 : 0;
				if (Enemy->GetProjectileCount() == 0 || Flight < 300.f)
				{
					UE_LOG(LogGB, Error, TEXT("SMOKE Castle FAILED: %s fired %d projectiles, the longest flew %.0f cm (need one that flies 300)"), *Entry.Id.ToString(), Enemy->GetProjectileCount(), Flight);
				}
			}
			else if (Hits == 0)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE Castle FAILED: %s never hit the player"), *Entry.Id.ToString());
			}
		}
		const UGBSquadSubsystem* Squad = UGBSquadSubsystem::Get(World);
		const int32 Pincers = Squad ? Squad->GetPincerCount() : 0;
		UE_LOG(LogGB, Display, TEXT("SMOKE Castle squad: ranged members with cover %d of %d, pincers (two melee commits within the window from 90+ degrees apart)=%d, widest %.0f degrees"),
			CoveredRanged, Ranged, Pincers, Squad ? Squad->GetBestPincerAngle() : 0.f);
		if (Ranged > 0 && CoveredRanged == 0)
		{
			UE_LOG(LogGB, Error, TEXT("SMOKE Castle FAILED: no ranged member took a covered position behind the boxes"));
		}
		if (Pincers == 0)
		{
			UE_LOG(LogGB, Error, TEXT("SMOKE Castle FAILED: no two melee members closed in together from different sides"));
		}
		(void)Player;
	}

	/** The sprite-look enemies killed by CastleKill: each must still be there, showing its Death clip, until its hold is checked. */
	static TArray<TPair<TWeakObjectPtr<AGBSkeletalEnemy>, FName>> CastleSpriteDead;

	static void CastleKill(AGBPlayerCharacter* Player)
	{
		CastleSpriteDead.Reset();
		for (const FE3DEntry& Entry : E3DGroup)
		{
			if (AGBSkeletalEnemy* Enemy = Cast<AGBSkeletalEnemy>(Entry.Enemy.Get()))
			{
				if (Enemy->HasSpriteLook())
				{
					CastleSpriteDead.Emplace(Enemy, Entry.Id);
				}
			}
		}
		E3DKillGroup(Player);
	}

	/**
	 * After the kill: each sprite enemy shows its Death clip (bHold false: playing; bHold true: its LAST frame is on screen, the clip
	 * over). A sprite corpse that is already gone at either check has popped out before its hold: that fails.
	 */
	static void CastleDeathCheck(bool bHold)
	{
		const FName DeathClip(TEXT("Death"));
		for (const TPair<TWeakObjectPtr<AGBSkeletalEnemy>, FName>& Dead : CastleSpriteDead)
		{
			const AGBSkeletalEnemy* Enemy = Dead.Key.Get();
			if (!Enemy)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE Castle FAILED: the corpse of %s was gone before its death hold was checked"), *Dead.Value.ToString());
				continue;
			}
			const UGBSpriteAnimComponent* Anim = Enemy->GetSpriteAnim();
			if (!Anim)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE Castle FAILED: %s lost its sprite component"), *Dead.Value.ToString());
				continue;
			}
			const int32 LastFrame = Anim->GetClipFrames(DeathClip) - 1;
			const bool bShowsDeath = Anim->GetCurrentClip() == DeathClip;
			UE_LOG(LogGB, Display, TEXT("SMOKE Castle %s death: shown clip=%s frame=%d of %d, elapsed=%.2f s, clip=%.2f s | %s"), *Dead.Value.ToString(), *Anim->GetCurrentClip().ToString(),
				Anim->GetCurrentFrame(), LastFrame, Enemy->GetDeathElapsed(), Enemy->GetDeathClipLength(), *Anim->Describe());
			if (!bShowsDeath)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE Castle FAILED: %s shows %s, not its Death clip"), *Dead.Value.ToString(), *Anim->GetCurrentClip().ToString());
			}
			else if (bHold && (Enemy->GetDeathElapsed() < Enemy->GetDeathClipLength() + 0.1f || Anim->GetCurrentFrame() != LastFrame))
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE Castle FAILED: %s is not holding the last frame of its Death clip (frame %d of %d, %.2f s after the kill, clip %.2f s)"), *Dead.Value.ToString(),
					Anim->GetCurrentFrame(), LastFrame, Enemy->GetDeathElapsed(), Enemy->GetDeathClipLength());
			}
		}
	}

	static void CastleClean(AGBPlayerCharacter* Player)
	{
		FTSTicker::GetCoreTicker().RemoveTicker(CastleTicker);
		FTSTicker::GetCoreTicker().RemoveTicker(CastleSpawnTicker);
		CastleTicker.Reset();
		CastleSpawnTicker.Reset();
		CastleProjectiles.Reset();
		UGBCombatLibrary::OnDamageDealt().Remove(CastleHitHandle);
		CastleHitHandle.Reset();
		CastleHits.Reset();
		E3TClean();
		if (Player)
		{
			Player->GetAttributes()->RestoreAll();
		}
	}

	// ---- CastleVisit block (gb.Smoke CastleVisit, only on /Game/Glyphbound/Maps/PastCastle): the castle's own encounters, as placed.
	// A state machine on the core ticker. While it runs it holds the smoke clock still (StartSeconds follows the wall clock), so the
	// step after it waits however long the visit takes. Per encounter: the prespawned wave was checked before the player came (all
	// encounters at the start, the player parked far above the map); the player is put on the navmesh 6-14 m from the zone, in
	// sight of its enemies, facing them; then notice, attack, a night screenshot, kill, the next waves come and engage, Cleared.
	namespace CastleVisit
	{
		enum class EPhase : uint8 { Park, Begin, Notice, Fight, Kill, NextWave, WaveFight, Finish, Done };

		struct FPre
		{
			int32 Prespawned = 0;
			int32 InZone = 0;
			int32 OnFloor = 0;
			int32 Aware = 0;
			int32 State = 0;
			bool bChecked = false;
		};

		static TArray<TWeakObjectPtr<AGBEncounter>> Encounters;
		static TMap<FName, FPre> Pre;
		static int32 Index = -1;
		static EPhase Phase = EPhase::Park;
		static double PhaseStart = 0.0;
		static double EncStart = 0.0;
		static double BlockStart = 0.0;
		static double FrozenElapsed = 0.0;
		static double AwareAt = -1.0;
		static double FirstAttackAt = -1.0;
		static double WaveStartAt = 0.0;
		static int32 WaveAttackBase = 0;
		static int32 LastWave = -1;
		static int32 WavesSeen = 0;
		static int32 WavesEngaged = 0;
		static int32 Hits = 0;
		static bool bShot = false;
		static bool bSteppedIn = false;
		static FVector ParkSpot = FVector::ZeroVector;
		static FTSTicker::FDelegateHandle Ticker;
		static FDelegateHandle HitHandle;
		static TMap<TWeakObjectPtr<AGBEnemyCharacter>, int32> AttackCounts;
		static TSet<TWeakObjectPtr<AActor>> Members;
		static TSet<TWeakObjectPtr<AGBEnemyCharacter>> Covered;
		/** Per ranged member, its last numbers while alive (printed when the encounter ends). */
		static TMap<TWeakObjectPtr<AGBEnemyCharacter>, FString> RangedInfo;
		static TMap<TWeakObjectPtr<AGBEnemyCharacter>, int32> HeldBy;
		/** Projectiles per member (kept after it is gone). */
		static TMap<TWeakObjectPtr<AGBEnemyCharacter>, int32> ProjBy;
		static int32 WaveProjBase = 0;
		static bool bWaveEngaged = false;
		static bool bWaveSteppedIn = false;
		static bool bWaveFloorChecked = false;
		static TWeakObjectPtr<AGBValkyrieEnemy> Valk;
		static float ValkHealthBefore = 0.f;
		static float ValkHealthAfter = 0.f;
		static bool bValkAwakeOnArrival = false;
		static int32 ValkCombos = 0;
		static int32 ValkJudgements = 0;
		static bool bValkDied = false;

		static const TCHAR* const Order[] = { TEXT("Opening_Past"), TEXT("Castle_Yard"), TEXT("Castle_FrontHall"), TEXT("Castle_MainHall"),
			TEXT("Zone_Air"), TEXT("Zone_Fire"), TEXT("Zone_Ice"), TEXT("Valkyrie_Past") };

		static double Now(UWorld* World) { return World ? World->GetTimeSeconds() : 0.0; }

		static void SetPhase(UWorld* World, EPhase NewPhase)
		{
			Phase = NewPhase;
			PhaseStart = Now(World);
		}

		static AGBEncounter* Current()
		{
			return Encounters.IsValidIndex(Index) ? Encounters[Index].Get() : nullptr;
		}

		static FString IdOf(const AGBEncounter* Encounter)
		{
			return Encounter ? Encounter->EncounterId.ToString() : FString(TEXT("?"));
		}

		static void Living(const AGBEncounter* Encounter, TArray<AGBEnemyCharacter*>& Out)
		{
			Out.Reset();
			if (!Encounter)
			{
				return;
			}
			for (const TObjectPtr<AGBEnemyCharacter>& Enemy : Encounter->GetEnemies())
			{
				if (Enemy && !Enemy->IsDead())
				{
					Out.Add(Enemy);
				}
			}
		}

		static int32 AttacksOf(const AGBEnemyCharacter* Enemy)
		{
			const AGBSkeletalEnemy* Skeletal = Cast<AGBSkeletalEnemy>(Enemy);
			if (!Skeletal)
			{
				return 0;
			}
			int32 Count = Skeletal->GetProjectileCount() + Skeletal->GetMeleeSwingCount() + Skeletal->GetLungeCount();
			if (const AGBValkyrieEnemy* Vk = Cast<AGBValkyrieEnemy>(Enemy))
			{
				Count += Vk->GetSwingCount() + Vk->GetJudgementCasts();
			}
			return Count;
		}

		static int32 TotalAttacks()
		{
			int32 Total = 0;
			for (const TPair<TWeakObjectPtr<AGBEnemyCharacter>, int32>& Pair : AttackCounts)
			{
				Total += Pair.Value;
			}
			return Total;
		}

		static int32 TotalProjectiles()
		{
			int32 Total = 0;
			for (const TPair<TWeakObjectPtr<AGBEnemyCharacter>, int32>& Pair : ProjBy)
			{
				Total += Pair.Value;
			}
			return Total;
		}

		static bool HasLivingRanged(const AGBEncounter* Encounter)
		{
			TArray<AGBEnemyCharacter*> Alive;
			Living(Encounter, Alive);
			for (const AGBEnemyCharacter* Enemy : Alive)
			{
				const AGBSkeletalEnemy* Skeletal = Cast<AGBSkeletalEnemy>(Enemy);
				if (Skeletal && Skeletal->FiresProjectiles())
				{
					return true;
				}
			}
			return false;
		}

		/** Every frame: the members of the current encounter, their attack counters (kept after death), ranged members in cover. */
		static void Track(const AGBEncounter* Encounter)
		{
			TArray<AGBEnemyCharacter*> Alive;
			Living(Encounter, Alive);
			for (AGBEnemyCharacter* Enemy : Alive)
			{
				Members.Add(TWeakObjectPtr<AActor>(Enemy));
				AttackCounts.FindOrAdd(Enemy) = AttacksOf(Enemy);
				const AGBSkeletalEnemy* Skeletal = Cast<AGBSkeletalEnemy>(Enemy);
				if (Skeletal && Skeletal->FiresProjectiles() && Skeletal->HasPosition() && Skeletal->GetPosition().bCovered)
				{
					Covered.Add(Enemy);
				}
				if (Skeletal && Skeletal->FiresProjectiles())
				{
					ProjBy.FindOrAdd(Enemy) = Skeletal->GetProjectileCount();
					HeldBy.FindOrAdd(Enemy) = Skeletal->GetBlockedShots();
					RangedInfo.FindOrAdd(Enemy) = FString::Printf(TEXT("%s shots=%d projectiles=%d held=%d peekBursts=%d reloc=%d coverTime=%.1f"), *Enemy->GetEnemyId().ToString(),
						Skeletal->GetShotCount(), Skeletal->GetProjectileCount(), Skeletal->GetBlockedShots(), Skeletal->GetPeekBursts(), Skeletal->GetRelocations(), Skeletal->GetCoverTime());
				}
				if (AGBValkyrieEnemy* Vk = Cast<AGBValkyrieEnemy>(Enemy))
				{
					Valk = Vk;
					ValkCombos = FMath::Max(ValkCombos, Vk->GetComboCount());
					ValkJudgements = FMath::Max(ValkJudgements, Vk->GetJudgementCasts());
				}
			}
		}

		/** The enemy's feet are on the navmesh: on the floor, not under it. */
		static bool OnFloor(UWorld* World, const AGBEnemyCharacter* Enemy)
		{
			UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
			if (!Nav)
			{
				return false;
			}
			const FVector Foot = Enemy->GetActorLocation() - FVector(0.f, 0.f, Enemy->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
			FNavLocation Projected;
			return Nav->ProjectPointToNavigation(Foot, Projected, FVector(80.f, 80.f, 120.f)) && FMath::Abs(Projected.Location.Z - Foot.Z) < 70.f;
		}

		static FVector FootOf(const AGBEnemyCharacter* Enemy)
		{
			return Enemy->GetActorLocation() - FVector(0.f, 0.f, Enemy->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
		}

		/**
		 * A floor spot on the navmesh near the zone (6-14 m from its centre, on the level of its enemies), with a clear line from the
		 * player's eyes to as many of them as possible, from which the navmesh leads to them. Inside the zone scores better: melee
		 * enemies never leave it.
		 */
		static bool FindStand(UWorld* World, const AGBEncounter* Encounter, const TArray<AGBEnemyCharacter*>& Enemies, const AGBPlayerCharacter* Player, FVector& OutFoot, FVector& OutLook)
		{
			UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
			if (!Nav || Enemies.Num() == 0)
			{
				return false;
			}
			const FBox Zone = Encounter->GetZone();
			FVector Centroid = FVector::ZeroVector;
			for (const AGBEnemyCharacter* Enemy : Enemies)
			{
				Centroid += FootOf(Enemy);
			}
			Centroid /= static_cast<float>(Enemies.Num());
			const FVector Centre(Zone.GetCenter().X, Zone.GetCenter().Y, Centroid.Z);
			FCollisionQueryParams Params(SCENE_QUERY_STAT(GBCastleVisitStand), false, Player);
			FCollisionResponseParams IgnorePawns;
			IgnorePawns.CollisionResponse.SetResponse(ECC_Pawn, ECR_Ignore);
			float BestScore = -1.0e9f;
			bool bFound = false;
			for (const float Radius : { 1200.f, 1000.f, 1400.f, 800.f, 600.f })
			{
				for (int32 K = 0; K < 16; ++K)
				{
					const float Angle = K * PI / 8.f;
					const FVector Guess = Centre + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * Radius;
					FNavLocation Projected;
					if (!Nav->ProjectPointToNavigation(Guess, Projected, FVector(150.f, 150.f, 250.f)) || FMath::Abs(Projected.Location.Z - Centroid.Z) > 250.f)
					{
						continue;
					}
					const FVector Foot = Projected.Location;
					const FVector Eyes = Foot + FVector(0.f, 0.f, 170.f);
					int32 Visible = 0;
					FVector Look = FVector::ZeroVector;
					const AGBEnemyCharacter* Nearest = nullptr;
					for (const AGBEnemyCharacter* Enemy : Enemies)
					{
						FCollisionQueryParams Line = Params;
						Line.AddIgnoredActor(Enemy);
						if (!World->LineTraceTestByChannel(Eyes, Enemy->GetActorLocation(), ECC_Visibility, Line, IgnorePawns))
						{
							++Visible;
							Look += Enemy->GetActorLocation();
							if (!Nearest || FVector::Dist(Foot, Enemy->GetActorLocation()) < FVector::Dist(Foot, Nearest->GetActorLocation()))
							{
								Nearest = Enemy;
							}
						}
					}
					if (Visible == 0)
					{
						continue;
					}
					bool bReach = false;
					if (const UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(World, Foot, FootOf(Nearest), nullptr))
					{
						bReach = Path->IsValid() && !Path->IsPartial();
					}
					const bool bInside = Zone.IsInsideXY(Foot);
					const float Score = Visible * 10.f + (bReach ? 25.f : 0.f) + (bInside ? 8.f : 0.f) - FMath::Abs(Radius - 1100.f) / 200.f;
					if (Score > BestScore)
					{
						BestScore = Score;
						OutFoot = Foot;
						OutLook = Look / static_cast<float>(Visible);
						bFound = true;
					}
				}
			}
			return bFound;
		}

		static void Face(AGBPlayerCharacter* Player, const FVector& Target)
		{
			if (AGBPlayerController* PC = Cast<AGBPlayerController>(Player->GetController()))
			{
				PC->SetControlRotation((Target - Player->GetPawnViewLocation()).Rotation());
			}
		}

		static void KillWave(AGBPlayerCharacter* Player, AGBEncounter* Encounter)
		{
			TArray<AGBEnemyCharacter*> Alive;
			Living(Encounter, Alive);
			for (AGBEnemyCharacter* Enemy : Alive)
			{
				AttackCounts.FindOrAdd(Enemy) = AttacksOf(Enemy);
				if (UGBStatsComponent* EnemyStats = Enemy->GetStats())
				{
					EnemyStats->SetBaseStat(FName(TEXT("CorpseTime")), 0.6f);
				}
				FGBDamageInfo Info;
				Info.Amount = 1000000.f;
				Info.Instigator = Player;
				Info.HitLocation = Enemy->GetActorLocation();
				Info.HitDirection = Enemy->GetActorForwardVector();
				Info.SourceId = TEXT("SmokeCastleVisit");
				UGBCombatLibrary::DealDamage(Enemy, Info);
				if (!Enemy->IsDead())
				{
					UE_LOG(LogGB, Error, TEXT("SMOKE CastleVisit %s FAILED: %s did not die to 1,000,000 damage"), *IdOf(Encounter), *Enemy->GetEnemyId().ToString());
				}
				else if (const AGBValkyrieEnemy* Vk = Cast<AGBValkyrieEnemy>(Enemy))
				{
					bValkDied = true;
					UE_LOG(LogGB, Display, TEXT("SMOKE CastleVisit %s: the Valkyrie died, death clip playing=%d | %s"), *IdOf(Encounter), Vk->IsDeathClipPlaying() ? 1 : 0, *Vk->DescribeLook());
				}
			}
			UE_LOG(LogGB, Display, TEXT("SMOKE CastleVisit %s: killed wave %d (%d enemies), state=%d"), *IdOf(Encounter), Encounter->GetCurrentWave() + 1, Alive.Num(), static_cast<int32>(Encounter->GetState()));
		}

		/** Why an enemy is off the floor: where it stands, its nearest spawn point, the floor and the headroom there, its capsule. */
		static FString WhyOffFloor(UWorld* World, const AGBEncounter* Encounter, const AGBEnemyCharacter* Enemy)
		{
			const TArray<FVector> Points = Encounter->GetWorldSpawnPoints();
			FVector Point = Enemy->GetActorLocation();
			float Best = TNumericLimits<float>::Max();
			for (const FVector& Candidate : Points)
			{
				const float Dist = FVector::Dist2D(Candidate, Enemy->GetActorLocation());
				if (Dist < Best)
				{
					Best = Dist;
					Point = Candidate;
				}
			}
			FHitResult Floor;
			FHitResult Ceiling;
			FCollisionQueryParams Room(SCENE_QUERY_STAT(GBCastleVisitRoom), false, Enemy);
			const bool bFloorHit = World->LineTraceSingleByObjectType(Floor, Point, Point - FVector(0.f, 0.f, 600.f), FCollisionObjectQueryParams(ECC_WorldStatic), Room);
			const FVector FloorAt = bFloorHit ? FVector(Floor.ImpactPoint) : Point;
			const bool bCeiling = World->LineTraceSingleByObjectType(Ceiling, FloorAt + FVector(0.f, 0.f, 5.f), FloorAt + FVector(0.f, 0.f, 800.f), FCollisionObjectQueryParams(ECC_WorldStatic), Room);
			FHitResult Under;
			const bool bUnder = World->LineTraceSingleByObjectType(Under, Enemy->GetActorLocation(), Enemy->GetActorLocation() - FVector(0.f, 0.f, 1000.f), FCollisionObjectQueryParams(ECC_WorldStatic), Room);
			return FString::Printf(TEXT("%s stands at %s (feet z=%.0f) on %s; its nearest spawn point %s (%.0f cm off): floor z=%.0f, ceiling %s, its capsule %.0f cm tall"),
				*Enemy->GetEnemyId().ToString(), *Enemy->GetActorLocation().ToCompactString(), FootOf(Enemy).Z,
				bUnder ? *FString::Printf(TEXT("%s (%s) at z=%.0f"), Under.GetActor() ? *Under.GetActor()->GetName() : TEXT("?"),
					Under.GetComponent() ? *Under.GetComponent()->GetDetailedInfo() : TEXT("?"), Under.ImpactPoint.Z) : TEXT("nothing"),
				*Point.ToCompactString(), Best, FloorAt.Z,
				bCeiling ? *FString::Printf(TEXT("z=%.0f (%s, headroom %.0f cm)"), Ceiling.ImpactPoint.Z, Ceiling.GetActor() ? *Ceiling.GetActor()->GetName() : TEXT("?"), Ceiling.ImpactPoint.Z - FloorAt.Z) : TEXT("open"),
				2.f * Enemy->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
		}

		/** All encounters, before the player came near any of them: the first wave stands, in its zone, on the floor, unaware. */
		static void PreCheck(UWorld* World)
		{
			for (const TWeakObjectPtr<AGBEncounter>& Weak : Encounters)
			{
				const AGBEncounter* Encounter = Weak.Get();
				if (!Encounter)
				{
					continue;
				}
				FPre& Entry = Pre.FindOrAdd(Encounter->EncounterId);
				Entry.bChecked = true;
				TArray<AGBEnemyCharacter*> Alive;
				Living(Encounter, Alive);
				const FBox Zone = Encounter->GetZone().ExpandBy(60.f);
				Entry.Prespawned = Alive.Num();
				Entry.State = static_cast<int32>(Encounter->GetState());
				FString Who;
				for (const AGBEnemyCharacter* Enemy : Alive)
				{
					const bool bIn = Zone.IsInside(Enemy->GetActorLocation());
					const bool bFloor = OnFloor(World, Enemy);
					Entry.InZone += bIn ? 1 : 0;
					Entry.OnFloor += bFloor ? 1 : 0;
					if (!bFloor)
					{
						UE_LOG(LogGB, Display, TEXT("SMOKE CastleVisit %s: %s"), *IdOf(Encounter), *WhyOffFloor(World, Encounter, Enemy));
					}
					Entry.Aware += Enemy->IsAware() ? 1 : 0;
					Who += FString::Printf(TEXT(" %s(%s in=%d floor=%d %s)"), *Enemy->GetEnemyId().ToString(), bFloor ? *FString::Printf(TEXT("z=%.0f"), FootOf(Enemy).Z) : *FootOf(Enemy).ToCompactString(), bIn ? 1 : 0, bFloor ? 1 : 0,
						Enemy->GetUnawareBehavior() == EGBUnawareBehavior::Patrol ? TEXT("patrol") : (Enemy->GetUnawareBehavior() == EGBUnawareBehavior::Dormant ? TEXT("dormant") : TEXT("guard")));
				}
				UE_LOG(LogGB, Display, TEXT("SMOKE CastleVisit %s BEFORE the player: state=%d prespawn=%d wave=%d of %d, %d standing, inZone=%d onFloor=%d aware=%d |%s"), *IdOf(Encounter), Entry.State,
					Encounter->IsPrespawn() ? 1 : 0, Encounter->GetCurrentWave() + 1, Encounter->GetNumWaves(), Entry.Prespawned, Entry.InZone, Entry.OnFloor, Entry.Aware, *Who);
				if (Encounter->IsPrespawn() && Entry.Prespawned == 0)
				{
					UE_LOG(LogGB, Error, TEXT("SMOKE CastleVisit %s FAILED: a Prespawn encounter with nobody standing before the player came"), *IdOf(Encounter));
				}
				if (Entry.InZone < Entry.Prespawned)
				{
					UE_LOG(LogGB, Error, TEXT("SMOKE CastleVisit %s FAILED: %d of %d prespawned enemies stand outside their zone"), *IdOf(Encounter), Entry.Prespawned - Entry.InZone, Entry.Prespawned);
				}
				if (Entry.OnFloor < Entry.Prespawned)
				{
					UE_LOG(LogGB, Error, TEXT("SMOKE CastleVisit %s FAILED: %d of %d prespawned enemies are not on the floor (navmesh) where they stand"), *IdOf(Encounter), Entry.Prespawned - Entry.OnFloor, Entry.Prespawned);
				}
				if (Entry.State != static_cast<int32>(EGBEncounterState::Waiting) || Entry.Aware > 0)
				{
					UE_LOG(LogGB, Error, TEXT("SMOKE CastleVisit %s FAILED: state %d with %d aware before the player came near"), *IdOf(Encounter), Entry.State, Entry.Aware);
				}
			}
		}

		static void BeginEncounter(UWorld* World, AGBPlayerCharacter* Player)
		{
			AGBEncounter* Encounter = Current();
			EncStart = Now(World);
			AwareAt = -1.0;
			FirstAttackAt = -1.0;
			LastWave = -1;
			WavesSeen = 0;
			WavesEngaged = 0;
			Hits = 0;
			bShot = false;
			bSteppedIn = false;
			AttackCounts.Reset();
			Members.Reset();
			Covered.Reset();
			RangedInfo.Reset();
			HeldBy.Reset();
			ProjBy.Reset();
			Valk.Reset();
			ValkHealthBefore = ValkHealthAfter = 0.f;
			bValkAwakeOnArrival = false;
			ValkCombos = ValkJudgements = 0;
			bValkDied = false;
			if (!Encounter)
			{
				SetPhase(World, EPhase::Finish);
				return;
			}
			TArray<AGBEnemyCharacter*> Alive;
			Living(Encounter, Alive);
			LastWave = Encounter->GetCurrentWave();
			WavesSeen = Alive.Num() > 0 ? 1 : 0;
			UE_LOG(LogGB, Display, TEXT("SMOKE CastleVisit %s: visit begins, state=%d wave=%d of %d, %d standing"), *IdOf(Encounter), static_cast<int32>(Encounter->GetState()),
				Encounter->GetCurrentWave() + 1, Encounter->GetNumWaves(), Alive.Num());
			if (Encounter->GetState() == EGBEncounterState::Active)
			{
				UE_LOG(LogGB, Display, TEXT("SMOKE CastleVisit %s: already Active when the player comes (an earlier visit woke it)"), *IdOf(Encounter));
			}
			FVector Foot;
			FVector Look;
			if (!FindStand(World, Encounter, Alive, Player, Foot, Look))
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE CastleVisit %s FAILED: no floor spot on the navmesh near the zone with a line to its enemies (%d standing)"), *IdOf(Encounter), Alive.Num());
				SetPhase(World, Alive.Num() > 0 ? EPhase::Kill : EPhase::Finish);
				return;
			}
			E3BTeleport(Player, Foot, false);
			Face(Player, Look);
			const FBox Zone = Encounter->GetZone();
			UE_LOG(LogGB, Display, TEXT("SMOKE CastleVisit %s: player at %s (%.0f cm from the zone centre, inside the zone=%d), facing %s"), *IdOf(Encounter), *Foot.ToCompactString(),
				FVector::Dist2D(Foot, Zone.GetCenter()), Zone.IsInsideXY(Foot) ? 1 : 0, *Look.ToCompactString());
			for (AGBEnemyCharacter* Enemy : Alive)
			{
				if (AGBValkyrieEnemy* Vk = Cast<AGBValkyrieEnemy>(Enemy))
				{
					Valk = Vk;
					bValkAwakeOnArrival = !Vk->IsSealed() && Vk->GetUnawareBehavior() != EGBUnawareBehavior::Dormant && !Encounter->IsSealedNow();
					UE_LOG(LogGB, Display, TEXT("SMOKE CastleVisit %s: the Valkyrie as the player comes in: sealed=%d dormant=%d encounterSealed=%d | %s"), *IdOf(Encounter), Vk->IsSealed() ? 1 : 0,
						Vk->GetUnawareBehavior() == EGBUnawareBehavior::Dormant ? 1 : 0, Encounter->IsSealedNow() ? 1 : 0, *Vk->DescribeLook());
					if (!bValkAwakeOnArrival)
					{
						UE_LOG(LogGB, Error, TEXT("SMOKE CastleVisit %s FAILED: the Valkyrie is sealed or dormant when the player comes into her room"), *IdOf(Encounter));
					}
				}
			}
			SetPhase(World, EPhase::Notice);
		}

		/** Melee members never leave their zone: a player standing outside it steps in, 5 m from the nearest living member. */
		static void StepInside(UWorld* World, const AGBEncounter* Encounter, AGBPlayerCharacter* Player, const TCHAR* Why)
		{
			TArray<AGBEnemyCharacter*> Alive;
			Living(Encounter, Alive);
			if (Alive.Num() > 0)
			{
				const AGBEnemyCharacter* Nearest = Alive[0];
				for (const AGBEnemyCharacter* Enemy : Alive)
				{
					Nearest = FVector::Dist(Enemy->GetActorLocation(), Player->GetActorLocation()) < FVector::Dist(Nearest->GetActorLocation(), Player->GetActorLocation()) ? Enemy : Nearest;
				}
				FNavLocation Projected;
				UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
				const FVector Toward = FootOf(Nearest) + (Player->GetActorLocation() - Nearest->GetActorLocation()).GetSafeNormal2D() * 500.f;
				if (Nav && Nav->ProjectPointToNavigation(Toward, Projected, FVector(150.f, 150.f, 250.f)))
				{
					E3BTeleport(Player, Projected.Location, false);
					Face(Player, Nearest->GetActorLocation());
					UE_LOG(LogGB, Display, TEXT("SMOKE CastleVisit %s: %s with the player outside the zone: he steps in, 5 m from %s"), *IdOf(Encounter), Why, *Nearest->GetEnemyId().ToString());
				}
			}
		}

		static void FinishEncounter(UWorld* World)
		{
			const AGBEncounter* Encounter = Current();
			if (Encounter)
			{
				const FPre* Entry = Pre.Find(Encounter->EncounterId);
				const bool bCleared = Encounter->GetState() == EGBEncounterState::Cleared;
				for (const TPair<TWeakObjectPtr<AGBEnemyCharacter>, FString>& Info : RangedInfo)
				{
					UE_LOG(LogGB, Display, TEXT("SMOKE CastleVisit %s ranged: %s"), *IdOf(Encounter), *Info.Value);
				}
				int32 Held = 0;
				for (const TPair<TWeakObjectPtr<AGBEnemyCharacter>, int32>& Pair : HeldBy)
				{
					Held += Pair.Value;
				}
				UE_LOG(LogGB, Display, TEXT("SMOKE CastleVisit %s: prespawned=%d inZone=%d onFloor=%d awareAfter=%.1fs hits=%d projectiles=%d held=%d attacks=%d cover=%d waves=%d/%d engaged=%d cleared=%d"),
					*IdOf(Encounter), Entry ? Entry->Prespawned : -1, Entry ? Entry->InZone : -1, Entry ? Entry->OnFloor : -1, AwareAt >= 0.0 ? AwareAt - EncStart : -1.0, Hits, TotalProjectiles(),
					Held, TotalAttacks(), Covered.Num(), WavesSeen, Encounter->GetNumWaves(), WavesEngaged, bCleared ? 1 : 0);
				if (WavesEngaged < WavesSeen)
				{
					UE_LOG(LogGB, Error, TEXT("SMOKE CastleVisit %s FAILED: only %d of the %d waves that came attacked the player"), *IdOf(Encounter), WavesEngaged, WavesSeen);
				}
				if (!bCleared)
				{
					UE_LOG(LogGB, Error, TEXT("SMOKE CastleVisit %s FAILED: not Cleared after its waves were killed (state %d, wave %d of %d)"), *IdOf(Encounter), static_cast<int32>(Encounter->GetState()),
						Encounter->GetCurrentWave() + 1, Encounter->GetNumWaves());
				}
				if (Encounter->EncounterId == FName(TEXT("Valkyrie_Past")))
				{
					UE_LOG(LogGB, Display, TEXT("SMOKE CastleVisit Valkyrie_Past: awake on arrival=%d health %.0f -> %.0f when hit, combos=%d judgements=%d died=%d"), bValkAwakeOnArrival ? 1 : 0,
						ValkHealthBefore, ValkHealthAfter, ValkCombos, ValkJudgements, bValkDied ? 1 : 0);
					if (!(ValkHealthAfter < ValkHealthBefore))
					{
						UE_LOG(LogGB, Error, TEXT("SMOKE CastleVisit Valkyrie_Past FAILED: her health did not drop when hit (%.0f -> %.0f)"), ValkHealthBefore, ValkHealthAfter);
					}
					if (ValkCombos < 1 && ValkJudgements < 1)
					{
						UE_LOG(LogGB, Error, TEXT("SMOKE CastleVisit Valkyrie_Past FAILED: she fought neither a combo nor a Judgement"));
					}
					if (!bValkDied)
					{
						UE_LOG(LogGB, Error, TEXT("SMOKE CastleVisit Valkyrie_Past FAILED: she did not die"));
					}
				}
			}
			++Index;
			SetPhase(World, Encounters.IsValidIndex(Index) ? EPhase::Begin : EPhase::Done);
		}

		/** Called from the ticker itself: returning false removes it. */
		static void Stop()
		{
			bHoldClock = false;
			Ticker.Reset();
			UGBCombatLibrary::OnDamageDealt().Remove(HitHandle);
			HitHandle.Reset();
		}


		static bool Tick(float)
		{
			UWorld* World = SmokeWorld.Get();
			AGBPlayerCharacter* Player = World ? AGBPlayerCharacter::Get(World) : nullptr;
			if (!World || !Player)
			{
				Stop();
				return false;
			}
			Player->GetAttributes()->RestoreAll();
			const double T = Now(World);
			const double InPhase = T - PhaseStart;
			if (T - BlockStart > 840.0 && Phase != EPhase::Done)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE CastleVisit FAILED: the visit ran past 840 s (at %s, phase %d)"), *IdOf(Current()), static_cast<int32>(Phase));
				Phase = EPhase::Done;
			}
			AGBEncounter* Encounter = Current();
			if (Encounter)
			{
				Track(Encounter);
			}
			switch (Phase)
			{
			case EPhase::Park:
				if (UCharacterMovementComponent* Move = Player->GetCharacterMovement())
				{
					Move->Velocity = FVector::ZeroVector;
				}
				Player->SetActorLocation(ParkSpot, false, nullptr, ETeleportType::TeleportPhysics);
				if (InPhase > 2.0)
				{
					PreCheck(World);
					Index = 0;
					SetPhase(World, Encounters.Num() > 0 ? EPhase::Begin : EPhase::Done);
				}
				break;

			case EPhase::Begin:
				BeginEncounter(World, Player);
				break;

			case EPhase::Notice:
			{
				TArray<AGBEnemyCharacter*> Alive;
				Living(Encounter, Alive);
				bool bAware = false;
				for (const AGBEnemyCharacter* Enemy : Alive)
				{
					bAware |= Enemy->IsAware();
				}
				if (bAware)
				{
					AwareAt = T;
					UE_LOG(LogGB, Display, TEXT("SMOKE CastleVisit %s: noticed after %.1f s, state=%d"), *IdOf(Encounter), AwareAt - EncStart, Encounter ? static_cast<int32>(Encounter->GetState()) : -1);
					if (Valk.IsValid())
					{
						ValkHealthBefore = Valk->GetAttributes()->GetHealth();
						FGBDamageInfo Info;
						Info.Amount = 250.f;
						Info.Instigator = Player;
						Info.HitLocation = Valk->GetActorLocation();
						Info.HitDirection = (Valk->GetActorLocation() - Player->GetActorLocation()).GetSafeNormal();
						Info.SourceId = TEXT("SmokeCastleVisit");
						UGBCombatLibrary::DealDamage(Valk.Get(), Info);
						ValkHealthAfter = Valk->GetAttributes()->GetHealth();
					}
					SetPhase(World, EPhase::Fight);
				}
				else if (InPhase > 15.0)
				{
					UE_LOG(LogGB, Error, TEXT("SMOKE CastleVisit %s FAILED: nobody noticed the player in 15 s (%d standing in sight)"), *IdOf(Encounter), Alive.Num());
					SetPhase(World, EPhase::Kill);
				}
				break;
			}

			case EPhase::Fight:
			{
				const int32 Attacks = TotalAttacks();
				if (FirstAttackAt < 0.0 && (Attacks > 0 || Hits > 0))
				{
					FirstAttackAt = T;
					WavesEngaged = 1;
					UE_LOG(LogGB, Display, TEXT("SMOKE CastleVisit %s: first attack %.1f s after the notice"), *IdOf(Encounter), FirstAttackAt - AwareAt);
				}
				// Melee members never leave their zone: a player outside it after 6 s steps in towards its centre.
				if (!bSteppedIn && InPhase > 6.0 && FirstAttackAt < 0.0 && Encounter && !Encounter->GetZone().IsInsideXY(Player->GetActorLocation()))
				{
					bSteppedIn = true;
					StepInside(World, Encounter, Player, TEXT("no attack in 6 s"));
				}
				if (!bShot && T - AwareAt > 4.0)
				{
					bShot = true;
					TArray<AGBEnemyCharacter*> Alive;
					Living(Encounter, Alive);
					if (Alive.Num() > 0)
					{
						FVector Look = FVector::ZeroVector;
						for (const AGBEnemyCharacter* Enemy : Alive)
						{
							Look += Enemy->GetActorLocation();
						}
						Face(Player, Look / static_cast<float>(Alive.Num()));
					}
					Shot(*FString::Printf(TEXT("castlevisit_%s_fight"), *IdOf(Encounter)));
				}
				const bool bValkDone = !Valk.IsValid() || ValkCombos > 0 || ValkJudgements > 0;
				// The ranged need time to reach a position and peek: with archers or shamans alive, the fight goes on until one has
				// loosed a projectile (or 18 s).
				const bool bRangedDone = TotalProjectiles() > 0 || !HasLivingRanged(Encounter) || T - AwareAt > 18.0;
				if ((FirstAttackAt >= 0.0 && T - AwareAt > 8.0 && bValkDone && bRangedDone) || InPhase > (Valk.IsValid() ? 30.0 : 24.0))
				{
					if (FirstAttackAt < 0.0)
					{
						UE_LOG(LogGB, Error, TEXT("SMOKE CastleVisit %s FAILED: noticed, but the squad never attacked in %.0f s"), *IdOf(Encounter), InPhase);
					}
					SetPhase(World, EPhase::Kill);
				}
				break;
			}

			case EPhase::Kill:
				if (Encounter)
				{
					KillWave(Player, Encounter);
				}
				SetPhase(World, EPhase::NextWave);
				break;

			case EPhase::NextWave:
			{
				if (!Encounter || Encounter->GetState() == EGBEncounterState::Cleared)
				{
					SetPhase(World, EPhase::Finish);
					break;
				}
				TArray<AGBEnemyCharacter*> Alive;
				Living(Encounter, Alive);
				if (Encounter->GetCurrentWave() > LastWave && Alive.Num() > 0)
				{
					LastWave = Encounter->GetCurrentWave();
					++WavesSeen;
					WaveStartAt = T;
					WaveAttackBase = TotalAttacks() + Hits;
					WaveProjBase = TotalProjectiles();
					bWaveEngaged = false;
					bWaveFloorChecked = false;
					bWaveSteppedIn = false;
					UE_LOG(LogGB, Display, TEXT("SMOKE CastleVisit %s: wave %d of %d came: %d enemies, %d in the player's view, wait ran out=%d"), *IdOf(Encounter), LastWave + 1, Encounter->GetNumWaves(),
						Alive.Num(), Encounter->GetLastReinforceVisible(), Encounter->DidLastReinforceWaitRunOut() ? 1 : 0);
					SetPhase(World, EPhase::WaveFight);
				}
				else if (Alive.Num() > 0 && Encounter->GetCurrentWave() == LastWave && InPhase > 1.0)
				{
					SetPhase(World, EPhase::Kill);   // a straggler of this wave (spawned late): kill it too
				}
				else if (InPhase > 25.0)
				{
					UE_LOG(LogGB, Error, TEXT("SMOKE CastleVisit %s FAILED: wave %d of %d killed, the next never came in 25 s (state %d)"), *IdOf(Encounter), LastWave + 1, Encounter->GetNumWaves(),
						static_cast<int32>(Encounter->GetState()));
					SetPhase(World, EPhase::Finish);
				}
				break;
			}

			case EPhase::WaveFight:
			{
				TArray<AGBEnemyCharacter*> Alive;
				Living(Encounter, Alive);
				if (Alive.Num() > 0)
				{
					FVector Look = FVector::ZeroVector;
					for (const AGBEnemyCharacter* Enemy : Alive)
					{
						Look += Enemy->GetActorLocation();
					}
					Face(Player, Look / static_cast<float>(Alive.Num()));
				}
				// A second after it came (a fresh spawn may still settle): every one of the wave on the floor, in the zone.
				if (!bWaveFloorChecked && T - WaveStartAt >= 1.0)
				{
					bWaveFloorChecked = true;
					const FBox Zone = Encounter->GetZone().ExpandBy(60.f);
					for (const AGBEnemyCharacter* Enemy : Alive)
					{
						if (!OnFloor(World, Enemy) || !Zone.IsInside(Enemy->GetActorLocation()))
						{
							UE_LOG(LogGB, Error, TEXT("SMOKE CastleVisit %s FAILED: wave %d's %s stands off the floor or outside the zone a second after it came: %s"), *IdOf(Encounter), LastWave + 1,
								*Enemy->GetEnemyId().ToString(), *WhyOffFloor(World, Encounter, Enemy));
						}
					}
				}
				if (!bWaveEngaged && TotalAttacks() + Hits > WaveAttackBase)
				{
					bWaveEngaged = true;
					++WavesEngaged;
					UE_LOG(LogGB, Display, TEXT("SMOKE CastleVisit %s: wave %d engaged %.1f s after it came"), *IdOf(Encounter), LastWave + 1, T - WaveStartAt);
				}
				if (bWaveEngaged && bWaveFloorChecked)
				{
					// Its archers and shamans get their chance too (up to 14 s after the wave came).
					if (TotalProjectiles() > WaveProjBase || !HasLivingRanged(Encounter) || T - WaveStartAt > 14.0)
					{
						SetPhase(World, EPhase::Kill);
					}
				}
				else if (!bWaveSteppedIn && InPhase > 8.0 && Alive.Num() > 0 && !Encounter->GetZone().IsInsideXY(Player->GetActorLocation()))
				{
					bWaveSteppedIn = true;
					StepInside(World, Encounter, Player, *FString::Printf(TEXT("wave %d: no attack in 8 s"), LastWave + 1));
				}
				else if (InPhase > 16.0 || Alive.Num() == 0)
				{
					FString Where;
					for (const AGBEnemyCharacter* Enemy : Alive)
					{
						Where += FString::Printf(TEXT(" %s(%.0f cm, aware=%d)"), *Enemy->GetEnemyId().ToString(), FVector::Dist(Enemy->GetActorLocation(), Player->GetActorLocation()), Enemy->IsAware() ? 1 : 0);
					}
					UE_LOG(LogGB, Error, TEXT("SMOKE CastleVisit %s FAILED: wave %d did not attack within 16 s:%s"), *IdOf(Encounter), LastWave + 1, *Where);
					SetPhase(World, EPhase::Kill);
				}
				break;
			}

			case EPhase::Finish:
				FinishEncounter(World);
				break;

			case EPhase::Done:
				UE_LOG(LogGB, Display, TEXT("SMOKE CastleVisit: visited %d encounters in %.0f s"), Encounters.Num(), T - BlockStart);
				Stop();
				return false;
			}
			return true;
		}

		static void Start(UWorld* World, AGBPlayerCharacter* Player)
		{
			if (!World || !World->GetMapName().Contains(TEXT("PastCastle")))
			{
				UE_LOG(LogGB, Display, TEXT("SMOKE CastleVisit skipped: it needs -Map /Game/Glyphbound/Maps/PastCastle"));
				return;
			}
			Encounters.Reset();
			Pre.Reset();
			for (const TCHAR* Id : Order)
			{
				for (TActorIterator<AGBEncounter> It(World); It; ++It)
				{
					if (It->EncounterId == FName(Id))
					{
						Encounters.Add(*It);
					}
				}
			}
			for (TActorIterator<AGBEncounter> It(World); It; ++It)
			{
				if (!Encounters.Contains(TWeakObjectPtr<AGBEncounter>(*It)))
				{
					Encounters.Add(*It);
				}
			}
			UE_LOG(LogGB, Display, TEXT("SMOKE CastleVisit: %d encounters in the map"), Encounters.Num());
			for (const TCHAR* Id : Order)
			{
				if (!Encounters.ContainsByPredicate([Id](const TWeakObjectPtr<AGBEncounter>& E) { return E.IsValid() && E->EncounterId == FName(Id); }))
				{
					UE_LOG(LogGB, Error, TEXT("SMOKE CastleVisit FAILED: the encounter %s is not in the map"), Id);
				}
			}
			ParkSpot = Player->GetActorLocation() + FVector(0.f, 0.f, 20000.f);
			E3BTeleport(Player, ParkSpot, true);
			UGBSquadSubsystem::bLogTactics = true;
			Player->GetAttributes()->SetInvulnerable(false);
			BlockStart = World->GetTimeSeconds();
			// Freeze at THIS step's time (a long first frame may have put the clock past the done step already).
			FrozenElapsed = Steps.IsValidIndex(NextStep - 1) ? Steps[NextStep - 1].Time : FPlatformTime::Seconds() - StartSeconds;
			StartSeconds = FPlatformTime::Seconds() - FrozenElapsed;
			HoldElapsed = FrozenElapsed;
			bHoldClock = true;   // the step after this block waits until the visit is over
			Index = -1;
			SetPhase(World, EPhase::Park);
			UGBCombatLibrary::OnDamageDealt().Remove(HitHandle);
			HitHandle = UGBCombatLibrary::OnDamageDealt().AddLambda([](AActor* Target, const FGBDamageInfo& Info, float Dealt)
			{
				AActor* Attacker = Info.Instigator ? Info.Instigator.Get() : Info.Causer.Get();
				if (Dealt > 0.f && Attacker && Cast<AGBPlayerCharacter>(Target) && Members.Contains(TWeakObjectPtr<AActor>(Attacker)))
				{
					++Hits;
				}
			});
			FTSTicker::GetCoreTicker().RemoveTicker(Ticker);
			Ticker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&Tick));
		}
	}

	static void BuildSteps()
	{
		Steps.Reset();
		auto Add = [](float Time, const TCHAR* Name, TFunction<void(UWorld*, AGBPlayerCharacter*)> Run, float MinSimSinceMark = 0.f)
		{
			FStep Step;
			Step.Time = Time;
			Step.Name = Name;
			Step.Run = MoveTemp(Run);
			Step.MinSimSinceMark = MinSimSinceMark;
			Steps.Add(MoveTemp(Step));
		};

		Add(1.0f, TEXT("data and world summary"), [](UWorld* World, AGBPlayerCharacter* Player)
		{
			if (const UGBDataSubsystem* Data = UGBDataSubsystem::Get(World))
			{
				UE_LOG(LogGB, Display, TEXT("SMOKE data spells=%d weapons=%d effects=%d items=%d recipes=%d enemies=%d encounters=%d skills=%d sprites=%d playerStats=%d"),
					Data->GetSpells().Num(), Data->GetWeapons().Num(), Data->GetStatusEffects().Num(), Data->GetItems().Num(),
					Data->GetRecipes().Num(), Data->GetEnemies().Num(), Data->GetEncounters().Num(), Data->GetSkillNodes().Num(),
					Data->GetSprites().Num(), Data->GetPlayer().Stats.Num());
			}
			LogState(World, Player, TEXT("start"));
			if (Player && Player->GetSpellbook())
			{
				for (int32 Slot = 0; Slot < UGBSpellbookComponent::NumSlots; ++Slot)
				{
					UE_LOG(LogGB, Display, TEXT("SMOKE loadout slot %d = %s"), Slot, *Player->GetSpellbook()->GetSlotSpell(Slot).ToString());
				}
			}
		});
		Add(1.5f, TEXT("staff basic attack"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			if (AGBHeldItem* Held = Player->GetEquipment()->GetActiveItem())
			{
				Held->StartPrimary();
				Held->StopPrimary();
			}
		});
		Add(2.0f, TEXT("cast slot 0 with the QTE off"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			SetQTEEnabled(false);
			const bool bOk = Player->GetSpellbook()->TryCastSlot(0);
			UE_LOG(LogGB, Display, TEXT("SMOKE cast slot0 ok=%d mana=%.0f"), bOk ? 1 : 0, Player->GetAttributes()->GetMana());
		});
		Add(2.6f, TEXT("cast slot 1"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			UE_LOG(LogGB, Display, TEXT("SMOKE cast slot1 ok=%d"), Player->GetSpellbook()->TryCastSlot(1) ? 1 : 0);
		});
		Add(3.2f, TEXT("cast slot 2"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			UE_LOG(LogGB, Display, TEXT("SMOKE cast slot2 ok=%d"), Player->GetSpellbook()->TryCastSlot(2) ? 1 : 0);
		});
		Add(3.8f, TEXT("screenshot: past, staff"), [](UWorld*, AGBPlayerCharacter*) { Shot(TEXT("1_past_staff")); });
		Add(4.8f, TEXT("learn every spell group, take the pistol, shoot, reload"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			int32 Learned = 0;
			for (const TCHAR* Group : { TEXT("Fire"), TEXT("Lightning"), TEXT("Ice"), TEXT("Air") })
			{
				Learned += Player->GetSpellbook()->LearnGroup(FName(Group));
			}
			AGBHeldItem* Gun = Player->GetEquipment()->GiveWeapon(TEXT("Pistol"), true, false);
			UE_LOG(LogGB, Display, TEXT("SMOKE learned=%d known=%d gun=%d holdingGun=%d"), Learned, Player->GetSpellbook()->GetKnownSpells().Num(), Gun ? 1 : 0, Player->GetEquipment()->IsHoldingGun() ? 1 : 0);
			if (Gun)
			{
				Gun->StartPrimary();
				Gun->StopPrimary();
				const FGBAmmoState Ammo = Gun->GetAmmo();
				UE_LOG(LogGB, Display, TEXT("SMOKE gun ammo after one shot: %d in magazine, %d spare"), Ammo.InMagazine, Ammo.SpareMagazines);
				Gun->Reload();
			}
		});
		Add(5.4f, TEXT("weapon enchant"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			UE_LOG(LogGB, Display, TEXT("SMOKE enchant ok=%d"), Player->GetSpellbook()->TryCastEnchant() ? 1 : 0);
		});
		Add(5.8f, TEXT("potion and grenade"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			UGBInventoryComponent* Inventory = Player->GetInventory();
			const bool bPotion = Inventory->UseItem(TEXT("HealthPotion"));
			Inventory->AddItem(TEXT("ElectricGrenade"), 1);
			const bool bGrenade = Inventory->UseItem(TEXT("ElectricGrenade"));
			UE_LOG(LogGB, Display, TEXT("SMOKE potion=%d grenade=%d quick=%s"), bPotion ? 1 : 0, bGrenade ? 1 : 0, *Inventory->GetQuickUseItem().ToString());
		});
		Add(6.2f, TEXT("kill one enemy"), [](UWorld* World, AGBPlayerCharacter* Player)
		{
			for (TActorIterator<AGBEnemyCharacter> It(World); It; ++It)
			{
				FGBDamageInfo Info;
				Info.Amount = 10000000.f;
				Info.Instigator = Player;
				const float Dealt = UGBCombatLibrary::DealDamage(*It, Info);
				UE_LOG(LogGB, Display, TEXT("SMOKE hit enemy %s for %.0f, dead=%d"), *It->GetEnemyId().ToString(), Dealt, It->IsDead() ? 1 : 0);
				break;
			}
		});
		Add(6.6f, TEXT("screenshot: past, gun"), [](UWorld*, AGBPlayerCharacter*) { Shot(TEXT("2_past_gun")); });
		Add(PathCheckStart, TEXT("pathing check: an orc behind a wall, the player invulnerable"), [](UWorld* World, AGBPlayerCharacter* Player)
		{
			StartPathCheck(World, Player);
		});
		Add(PathCheckStart + PathCheckDuration, TEXT("pathing check: did the orc get around the wall"), [](UWorld* World, AGBPlayerCharacter* Player)
		{
			EndPathCheck(World, Player);
		});

		// Everything below moved PathCheckShift seconds later to make room for the pathing check.
		const float S = PathCheckShift;
		Add(7.8f + S, TEXT("travel to the future"), [](UWorld* World, AGBPlayerCharacter* Player)
		{
			const bool bOk = UGBEraSubsystem::Get(World)->RequestTravel(EGBEra::Future, Player);
			UE_LOG(LogGB, Display, TEXT("SMOKE travel requested ok=%d"), bOk ? 1 : 0);
		});
		Add(10.5f + S, TEXT("screenshot: future"), [](UWorld* World, AGBPlayerCharacter* Player)
		{
			LogState(World, Player, TEXT("after travel"));
			Shot(TEXT("3_future_gun"));
		});
		Add(11.7f + S, TEXT("open the inventory"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			if (AGBPlayerController* PC = Cast<AGBPlayerController>(Player->GetController()))
			{
				PC->SetMenu(EGBMenu::Inventory);
			}
		});
		Add(12.2f + S, TEXT("screenshot: inventory"), [](UWorld*, AGBPlayerCharacter*) { Shot(TEXT("4_inventory")); });
		Add(13.4f + S, TEXT("open the skill trees and unlock a node"), [](UWorld* World, AGBPlayerCharacter* Player)
		{
			if (AGBPlayerController* PC = Cast<AGBPlayerController>(Player->GetController()))
			{
				PC->SetMenu(EGBMenu::SkillTrees);
			}
			SmokeSkillNode = NAME_None;
			SmokeSkillPointsBefore = Player->GetSkillTree()->GetPoints();
			if (const UGBDataSubsystem* Data = UGBDataSubsystem::Get(World))
			{
				UE_LOG(LogGB, Display, TEXT("SMOKE skills loaded=%d"), Data->GetSkillNodes().Num());
				if (Data->GetSkillNodes().Num() != 30)
				{
					UE_LOG(LogGB, Error, TEXT("SMOKE skills: expected 30 skill nodes, found %d"), Data->GetSkillNodes().Num());
				}
				if (Data->GetSkillNodes().Num() > 0)
				{
					const FName Node = Data->GetSkillNodes()[0].Id;
					SmokeSkillNode = Node;
					const EGBSkillUnlockResult Result = Player->GetSkillTree()->Unlock(Node);
					UE_LOG(LogGB, Display, TEXT("SMOKE unlock %s result=%d rank=%d points=%d"), *Node.ToString(), static_cast<int32>(Result), Player->GetSkillTree()->GetRank(Node), Player->GetSkillTree()->GetPoints());
				}
			}
		});
		Add(13.9f + S, TEXT("screenshot: skill trees"), [](UWorld*, AGBPlayerCharacter*) { Shot(TEXT("5_skills")); });
		Add(14.9f + S, TEXT("reset the skills"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			const int32 Refunded = Player->GetSkillTree()->ResetAll();
			const int32 Points = Player->GetSkillTree()->GetPoints();
			const int32 Rank = SmokeSkillNode == NAME_None ? 0 : Player->GetSkillTree()->GetRank(SmokeSkillNode);
			UE_LOG(LogGB, Display, TEXT("SMOKE skill reset refunded=%d points=%d rank=%d"), Refunded, Points, Rank);
			if (Points != SmokeSkillPointsBefore || Rank != 0)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE skill reset FAILED: points=%d (expected %d) rank=%d (expected 0)"), Points, SmokeSkillPointsBefore, Rank);
			}
		});
		Add(15.4f + S, TEXT("screenshot: skills reset"), [](UWorld*, AGBPlayerCharacter*) { Shot(TEXT("5b_skills_reset")); });
		Add(15.6f + S, TEXT("weapon perks: unlock the new nodes"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			UGBSkillTreeComponent* Skills = Player->GetSkillTree();
			Skills->AddPoints(30);
			auto UnlockRanks = [Skills](FName Node, int32 Ranks)
			{
				for (int32 i = 0; i < Ranks; ++i)
				{
					if (Skills->Unlock(Node) != EGBSkillUnlockResult::Ok)
					{
						UE_LOG(LogGB, Error, TEXT("SMOKE weapon perks: could not unlock '%s' (rank %d)"), *Node.ToString(), Skills->GetRank(Node) + 1);
					}
				}
			};
			UnlockRanks(FName(TEXT("Weapon_Damage")), 1);
			UnlockRanks(FName(TEXT("Weapon_Pistol_FireRate")), 1);
			UnlockRanks(FName(TEXT("Weapon_Pistol_Reload")), 3);
			UnlockRanks(FName(TEXT("Weapon_PistolHeadshot")), 2);
			UnlockRanks(FName(TEXT("Weapon_Headshot")), 1);
			UnlockRanks(FName(TEXT("Weapon_PrecisionRecovery")), 1);
			UE_LOG(LogGB, Display, TEXT("SMOKE weapon perks reload=%d precision=%d conduit=%d deadeye=%d points=%d"),
				Skills->GetRank(FName(TEXT("Weapon_Pistol_Reload"))), Skills->GetRank(FName(TEXT("Weapon_PrecisionRecovery"))),
				Skills->GetRank(FName(TEXT("Weapon_EnchantBuildUp"))), Skills->GetRank(FName(TEXT("Weapon_PistolHeadshot"))), Skills->GetPoints());
		});
		Add(15.8f + S, TEXT("precision recovery: weak point kill heals, second kill is on cooldown"), [](UWorld* World, AGBPlayerCharacter* Player)
		{
			UGBAttributeComponent* Attributes = Player->GetAttributes();
			const float MaxHealth = Attributes->GetMaxHealth();
			Attributes->SetHealthAndMana(0.5f * MaxHealth, Attributes->GetMana());
			auto KillWeakEnemy = [World, Player](bool bHeadshot) -> AGBEnemyCharacter*
			{
				const FVector At = Player->GetActorLocation() + Player->GetAimDirection() * 500.f;
				AGBEnemyCharacter* Enemy = AGBEnemyCharacter::SpawnEnemy(World, FName(TEXT("Orc_Past")), At, FRotator::ZeroRotator, EGBEraPresence::Both);
				if (!Enemy)
				{
					return nullptr;
				}
				FGBDamageInfo Info;
				Info.Amount = 100000.f;
				Info.Instigator = Player;
				Info.bHeadshot = bHeadshot;
				UGBCombatLibrary::DealDamage(Enemy, Info);
				return Enemy;
			};
			const float Before = Attributes->GetHealth();
			AGBEnemyCharacter* First = KillWeakEnemy(true);
			const float Healed = Attributes->GetHealth() - Before;
			UE_LOG(LogGB, Display, TEXT("SMOKE precision recovery healed=%.1f"), Healed);
			if (!First || !First->IsDead())
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE precision recovery: the test orc did not die"));
			}
			else if (FMath::Abs(Healed - 0.03f * MaxHealth) > 0.5f)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE precision recovery FAILED: healed %.1f, expected %.1f (3%% of max health)"), Healed, 0.03f * MaxHealth);
			}
			const float BeforeSecond = Attributes->GetHealth();
			AGBEnemyCharacter* Second = KillWeakEnemy(true);
			const float HealedSecond = Attributes->GetHealth() - BeforeSecond;
			UE_LOG(LogGB, Display, TEXT("SMOKE precision recovery cooldown healed=%.1f"), HealedSecond);
			if (!Second || !Second->IsDead())
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE precision recovery cooldown: the second test orc did not die"));
			}
			else if (FMath::Abs(HealedSecond) > 0.001f)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE precision recovery cooldown FAILED: healed %.1f right after the first kill"), HealedSecond);
			}
		});
		Add(16.0f + S, TEXT("rapid reload: pistol reload time"), [](UWorld* World, AGBPlayerCharacter* Player)
		{
			AGBHeldItem* Pistol = Player->GetEquipment()->GetItemInSlot(EGBHeldSlot::Gun);
			if (!Pistol || Pistol->GetWeaponId() != FName(TEXT("Pistol")))
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE rapid reload: the player has no pistol in the gun slot"));
				return;
			}
			const float ReloadTime = Pistol->GetStat(GBStat::ReloadTime, 0.f);
			UE_LOG(LogGB, Display, TEXT("SMOKE rapid reload pistol=%.2f"), ReloadTime);
			float Base = 0.f;
			if (const UGBDataSubsystem* Data = UGBDataSubsystem::Get(World))
			{
				if (const FGBWeaponDefinition* Def = Data->FindWeapon(Pistol->GetWeaponId()))
				{
					Base = Def->Stats.FindRef(GBStat::ReloadTime);
				}
			}
			if (!FMath::IsNearlyEqual(ReloadTime, 0.55f * Base, 0.01f))
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE rapid reload FAILED: %.2f s, expected 0.55 x base (3 ranks of -15%%) (%.2f s)"), ReloadTime, 0.55f * Base);
			}
		});
		Add(16.4f + S, TEXT("screenshot: weapon tree with the new nodes"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			if (AGBPlayerController* PC = Cast<AGBPlayerController>(Player->GetController()))
			{
				PC->SetMenu(EGBMenu::SkillTrees);
			}
			// The trees are two pages: show the weapon one.
			if (AGBHUD* Hud = AGBHUD::Get(Player->GetWorld()))
			{
				Hud->DebugSetMenuCursor(0, 0, TEXT("Weapon_Damage"), false);
			}
			Shot(TEXT("12_weapon_tree"));
		});
		Add(16.6f + S, TEXT("weapon perks: reset the skills"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			if (AGBPlayerController* PC = Cast<AGBPlayerController>(Player->GetController()))
			{
				PC->SetMenu(EGBMenu::None);
			}
			UGBSkillTreeComponent* Skills = Player->GetSkillTree();
			const int32 Refunded = Skills->ResetAll();
			Skills->AddPoints(-30); // take back the test points added for this block
			const int32 Points = Skills->GetPoints();
			UE_LOG(LogGB, Display, TEXT("SMOKE weapon perks reset refunded=%d points=%d"), Refunded, Points);
			if (Points != SmokeSkillPointsBefore)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE weapon perks reset FAILED: points=%d (expected %d)"), Points, SmokeSkillPointsBefore);
			}
			for (const FName Node : { FName(TEXT("Weapon_Damage")), FName(TEXT("Weapon_Pistol_FireRate")), FName(TEXT("Weapon_Pistol_Reload")),
				FName(TEXT("Weapon_Headshot")), FName(TEXT("Weapon_PrecisionRecovery")), FName(TEXT("Weapon_EnchantBuildUp")),
				FName(TEXT("Weapon_PistolHeadshot")) })
			{
				if (Skills->GetRank(Node) != 0)
				{
					UE_LOG(LogGB, Error, TEXT("SMOKE weapon perks reset FAILED: '%s' still at rank %d"), *Node.ToString(), Skills->GetRank(Node));
				}
			}
		});
		Add(17.1f + S, TEXT("close the menu, staff in hand, cast with the QTE on"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			if (AGBPlayerController* PC = Cast<AGBPlayerController>(Player->GetController()))
			{
				PC->SetMenu(EGBMenu::None);
			}
			Player->GetEquipment()->SwitchTo(EGBHeldSlot::Staff);
			Player->GetAttributes()->RestoreAll();
			SetQTEEnabled(true);
			const bool bOk = Player->GetSpellbook()->TryCastSlot(3);
			UE_LOG(LogGB, Display, TEXT("SMOKE cast slot3 (%s) with QTE ok=%d qteActive=%d"), *Player->GetSpellbook()->GetSlotSpell(3).ToString(), bOk ? 1 : 0, Player->GetQTE()->IsActive() ? 1 : 0);
		});
		Add(17.5f + S, TEXT("QTE: press one key"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			if (Player->GetQTE()->IsActive() && Player->GetQTE()->GetSession().Sequence.Num() > 0)
			{
				Player->GetQTE()->HandleKey(Player->GetQTE()->GetSession().Sequence[0]);
			}
		});
		Add(17.8f + S, TEXT("screenshot: QTE"), [](UWorld*, AGBPlayerCharacter*) { Shot(TEXT("6_qte")); });
		Add(21.9f + S, TEXT("after the QTE time limit"), [](UWorld* World, AGBPlayerCharacter* Player)
		{
			// 4.8 s after the cast with the QTE on (the limit is 4 s of real time): it must be over by now.
			const FGBQTEResult& Result = Player->GetQTE()->GetLastResult();
			UE_LOG(LogGB, Display, TEXT("SMOKE qte finished active=%d power=%.2f reach=%.2f success=%d timedOut=%d dilation=%.2f"),
				Player->GetQTE()->IsActive() ? 1 : 0, Result.Power, Result.Reach, Result.bSuccess ? 1 : 0, Result.bTimedOut ? 1 : 0,
				World->GetWorldSettings() ? World->GetWorldSettings()->GetEffectiveTimeDilation() : -1.f);
		});
		Add(22.3f + S, TEXT("kill the player"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			FGBDamageInfo Info;
			Info.Amount = 10000000.f;
			UGBCombatLibrary::DealDamage(Player, Info, true);
			UE_LOG(LogGB, Display, TEXT("SMOKE player dead=%d"), Player->IsDead() ? 1 : 0);
		});
		Add(23.0f + S, TEXT("screenshot: death"), [](UWorld*, AGBPlayerCharacter*) { Shot(TEXT("7_death")); });
		Add(26.0f + S, TEXT("after the respawn"), [](UWorld* World, AGBPlayerCharacter* Player)
		{
			LogState(World, Player, TEXT("after respawn"));
			UE_LOG(LogGB, Display, TEXT("SMOKE owned weapons=%d known spells=%d"),
				Player->GetEquipment()->GetOwnedWeapons().Num(), Player->GetSpellbook()->GetKnownSpells().Num());
			Shot(TEXT("8_respawn"));
		});
		Add(26.6f + S, TEXT("arcane conduit: enchanted build-up before and after"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			// The weapon enchant has a 15 s world-time cooldown and the smoke clock runs ahead of the
			// world clock, so this runs after the respawn: the earlier cast is long over by now.
			SetQTEEnabled(false);
			UGBEquipmentComponent* Equipment = Player->GetEquipment();
			UGBSkillTreeComponent* Skills = Player->GetSkillTree();
			const int32 PointsBefore = Skills->GetPoints();
			Skills->AddPoints(8);
			Equipment->SwitchTo(EGBHeldSlot::Gun);
			UGBSpellbookComponent* Book = Player->GetSpellbook();
			const FName EnchantId = Book->GetEnchantSpell();
			UE_LOG(LogGB, Display, TEXT("SMOKE arcane conduit diag holdingGun=%d busy=%d cd=%.1f mana=%.0f knows=%d cost=%.0f cancast=%d"),
				Equipment->IsHoldingGun() ? 1 : 0, Book->IsBusy() ? 1 : 0, Book->GetCooldownRemaining(EnchantId),
				Player->GetAttributes()->GetMana(), Book->KnowsSpell(EnchantId) ? 1 : 0, Book->GetManaCost(EnchantId),
				static_cast<int32>(Book->CanCast(EnchantId)));
			AGBHeldItem* Gun = nullptr;
			float Before = 0.f;
			if (!Book->TryCastEnchant())
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE arcane conduit: could not cast the weapon enchant"));
			}
			else if ((Gun = Equipment->GetActiveItem()) == nullptr || Gun->GetSlot() != EGBHeldSlot::Gun)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE arcane conduit: no gun in hand after SwitchTo"));
				Gun = nullptr;
			}
			else
			{
				Before = Gun->MakeDamageInfo().BuildUp;
			}
			if (Gun && Before > 0.f)
			{
				Skills->Unlock(FName(TEXT("Weapon_Damage")));
				for (int32 i = 0; i < 3; ++i)
				{
					Skills->Unlock(FName(TEXT("Weapon_EnchantBuildUp")));
				}
				if (Skills->GetRank(FName(TEXT("Weapon_EnchantBuildUp"))) != 3)
				{
					UE_LOG(LogGB, Error, TEXT("SMOKE arcane conduit: conduit rank is %d, expected 3"), Skills->GetRank(FName(TEXT("Weapon_EnchantBuildUp"))));
				}
				else
				{
					const float After = Gun->MakeDamageInfo().BuildUp;
					UE_LOG(LogGB, Display, TEXT("SMOKE arcane conduit buildup=%.2f base=%.2f"), After, Before);
					if (!FMath::IsNearlyEqual(After, 1.3f * Before, 0.01f))
					{
						UE_LOG(LogGB, Error, TEXT("SMOKE arcane conduit FAILED: %.2f, expected 1.3 x base (%.2f)"), After, 1.3f * Before);
					}
				}
			}
			else if (Gun)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE arcane conduit: enchanted build-up is 0, the enchant is not active"));
			}
			Equipment->SwitchTo(EGBHeldSlot::Staff);
			Skills->ResetAll();
			Skills->AddPoints(-8); // take back the test points added for this step
			if (Skills->GetPoints() != PointsBefore || Skills->GetRank(FName(TEXT("Weapon_EnchantBuildUp"))) != 0)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE arcane conduit reset FAILED: points=%d (expected %d)"), Skills->GetPoints(), PointsBefore);
			}
			SetQTEEnabled(true);
		});
		Add(27.2f + S, TEXT("second staff goes to the inventory, equip by hand"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			UGBEquipmentComponent* Equipment = Player->GetEquipment();
			AGBHeldItem* Returned = Equipment->GiveWeapon(TEXT("Staff_Ember"), true, false);
			const AGBHeldItem* InSlot = Equipment->GetItemInSlot(EGBHeldSlot::Staff);
			UE_LOG(LogGB, Display, TEXT("SMOKE staff in slot=%s emberOwned=%d returnedItem=%d"),
				InSlot ? *InSlot->GetWeaponId().ToString() : TEXT("none"),
				Equipment->OwnsWeapon(TEXT("Staff_Ember")) ? 1 : 0, Returned ? 1 : 0);
			const bool bEquipped = Equipment->EquipOwnedWeapon(TEXT("Staff_Ember"));
			const AGBHeldItem* InSlotAfter = Equipment->GetItemInSlot(EGBHeldSlot::Staff);
			UE_LOG(LogGB, Display, TEXT("SMOKE equip ember ok=%d staff in slot=%s emberEquipped=%d"),
				bEquipped ? 1 : 0,
				InSlotAfter ? *InSlotAfter->GetWeaponId().ToString() : TEXT("none"),
				Equipment->IsWeaponEquipped(TEXT("Staff_Ember")) ? 1 : 0);
		});
		Add(28.4f + S, TEXT("summon the ice shield and raise it"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			SetQTEEnabled(false);
			Player->GetEquipment()->SwitchTo(EGBHeldSlot::Staff);
			if (!Player->GetSpellbook()->KnowsSpell(TEXT("IceShield")))
			{
				Player->GetSpellbook()->LearnSpell(TEXT("IceShield"));
			}
			Player->GetSpellbook()->EquipSpell(4, TEXT("IceShield"));
			const bool bOk = Player->GetSpellbook()->TryCastSlot(4);
			Player->GetSpellbook()->SetBlockHeld(true);
			UE_LOG(LogGB, Display, TEXT("SMOKE shield cast=%d hasShield=%d raised=%d"),
				bOk ? 1 : 0,
				Player->GetSpellbook()->HasShield() ? 1 : 0,
				Player->GetSpellbook()->IsBlockRaised() ? 1 : 0);
		});
		Add(29.1f + S, TEXT("screenshot: shield"), [](UWorld*, AGBPlayerCharacter*) { Shot(TEXT("9_shield")); });
		Add(30.1f + S, TEXT("lower the shield"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			Player->GetSpellbook()->SetBlockHeld(false);
			UE_LOG(LogGB, Display, TEXT("SMOKE shield lowered hasShield=%d raised=%d"),
				Player->GetSpellbook()->HasShield() ? 1 : 0,
				Player->GetSpellbook()->IsBlockRaised() ? 1 : 0);
		});
		Add(31.3f + S, TEXT("ground spells: back to the past, staff in hand, learn both spells"), [](UWorld* World, AGBPlayerCharacter* Player)
		{
			// The rune must survive until the screenshot: nothing hostile may step on it, so clear the map first.
			for (TActorIterator<AGBEnemyCharacter> It(World); It; ++It)
			{
				FGBDamageInfo Info;
				Info.Amount = 10000000.f;
				Info.Instigator = Player;
				UGBCombatLibrary::DealDamage(*It, Info);
			}
			UGBEraSubsystem::Get(World)->RequestTravel(EGBEra::Past, Player);
			Player->GetEquipment()->SwitchTo(EGBHeldSlot::Staff);
			Player->GetAttributes()->RestoreAll();
			SetQTEEnabled(false);
			if (!Player->GetSpellbook()->KnowsSpell(TEXT("LightningRune")))
			{
				Player->GetSpellbook()->LearnSpell(TEXT("LightningRune"));
			}
			if (!Player->GetSpellbook()->KnowsSpell(TEXT("IceFloor")))
			{
				Player->GetSpellbook()->LearnSpell(TEXT("IceFloor"));
			}
			Player->GetSpellbook()->EquipSpell(0, TEXT("LightningRune"));
			Player->GetSpellbook()->EquipSpell(1, TEXT("IceFloor"));
			UE_LOG(LogGB, Display, TEXT("SMOKE ground spells setup known=%d"), Player->GetSpellbook()->GetKnownSpells().Num());
		});
		Add(32.5f + S, TEXT("ground spells: look down and cast the lightning rune close by"), [](UWorld* World, AGBPlayerCharacter* Player)
		{
			if (AGBPlayerController* PC = Cast<AGBPlayerController>(Player->GetController()))
			{
				PC->SetControlRotation(FRotator(-30.f, PC->GetControlRotation().Yaw, 0.f));
			}
			const bool bOk = Player->GetSpellbook()->TryCastSlot(0);
			int32 Runes = 0;
			for (TActorIterator<AGBSpell_Trap> It(World); It; ++It)
			{
				++Runes;
			}
			UE_LOG(LogGB, Display, TEXT("SMOKE cast LightningRune ok=%d runes=%d"), bOk ? 1 : 0, Runes);
			if (!bOk || Runes == 0)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE ground spells: the lightning rune actor did not spawn (ok=%d runes=%d)"), bOk ? 1 : 0, Runes);
			}
		});
		Add(33.7f + S, TEXT("ground spells: cast the ice floor overlapping about half the rune"), [](UWorld* World, AGBPlayerCharacter* Player)
		{
			if (AGBPlayerController* PC = Cast<AGBPlayerController>(Player->GetController()))
			{
				PC->SetControlRotation(FRotator(-17.f, PC->GetControlRotation().Yaw, 0.f));
			}
			const bool bOk = Player->GetSpellbook()->TryCastSlot(1);
			int32 Floors = 0;
			for (TActorIterator<AGBSpell_Zone> It(World); It; ++It)
			{
				++Floors;
			}
			UE_LOG(LogGB, Display, TEXT("SMOKE cast IceFloor ok=%d floors=%d"), bOk ? 1 : 0, Floors);
			if (!bOk || Floors == 0)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE ground spells: the ice floor actor did not spawn (ok=%d floors=%d)"), bOk ? 1 : 0, Floors);
			}
			float RuneZ = 0.f;
			bool bHasRune = false;
			for (TActorIterator<AGBSpell_Trap> It(World); It; ++It)
			{
				if (It->HasGroundFigure())
				{
					RuneZ = It->GetGroundFigureWorldZ();
					bHasRune = true;
					break;
				}
			}
			float FloorZ = 0.f;
			bool bHasFloor = false;
			for (TActorIterator<AGBSpell_Zone> It(World); It; ++It)
			{
				if (It->HasGroundFigure())
				{
					FloorZ = It->GetGroundFigureWorldZ();
					bHasFloor = true;
					break;
				}
			}
			if (bHasRune && bHasFloor)
			{
				UE_LOG(LogGB, Display, TEXT("SMOKE ground layers runeZ=%.1f floorZ=%.1f"), RuneZ, FloorZ);
				if (!(FloorZ > RuneZ))
				{
					UE_LOG(LogGB, Error, TEXT("SMOKE ground layers: the ice floor (%.1f) is not above the rune (%.1f)"), FloorZ, RuneZ);
				}
			}
			else
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE ground layers: missing figure (rune=%d floor=%d)"), bHasRune ? 1 : 0, bHasFloor ? 1 : 0);
			}
		});
		Add(34.7f + S, TEXT("screenshot: ground spells"), [](UWorld* World, AGBPlayerCharacter* Player)
		{
			if (AGBPlayerController* PC = Cast<AGBPlayerController>(Player->GetController()))
			{
				PC->SetControlRotation(FRotator(-22.f, PC->GetControlRotation().Yaw, 0.f));
			}
			int32 Runes = 0;
			for (TActorIterator<AGBSpell_Trap> It(World); It; ++It)
			{
				++Runes;
			}
			int32 Floors = 0;
			for (TActorIterator<AGBSpell_Zone> It(World); It; ++It)
			{
				++Floors;
			}
			if (Runes == 0)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE ground spells: no lightning rune actor before the screenshot"));
			}
			if (Floors == 0)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE ground spells: no ice floor actor before the screenshot"));
			}
			UE_LOG(LogGB, Display, TEXT("SMOKE ground spells runes=%d floors=%d"), Runes, Floors);
			Shot(TEXT("10_ground_spells"));
		});
		Add(35.4f + S, TEXT("flamethrower: cast with the QTE off and no key held"), [](UWorld* World, AGBPlayerCharacter* Player)
		{
			// The automated run holds no key, so the hold-to-sustain beam must stop after MinDuration.
			SetQTEEnabled(false);
			Player->GetEquipment()->SwitchTo(EGBHeldSlot::Staff);
			Player->GetAttributes()->RestoreAll();
			if (!Player->GetSpellbook()->KnowsSpell(TEXT("Flamethrower")))
			{
				Player->GetSpellbook()->LearnSpell(TEXT("Flamethrower"));
			}
			Player->GetSpellbook()->EquipSpell(2, TEXT("Flamethrower"));
			const bool bOk = Player->GetSpellbook()->TryCastSlot(2);
			int32 Beams = 0;
			for (TActorIterator<AGBSpell_Beam> It(World); It; ++It)
			{
				++Beams;
			}
			UE_LOG(LogGB, Display, TEXT("SMOKE flamethrower started=%d"), (bOk && Beams > 0) ? 1 : 0);
			if (!bOk || Beams == 0)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE flamethrower: the beam actor did not spawn (ok=%d beams=%d)"), bOk ? 1 : 0, Beams);
			}
		});
		Add(36.9f + S, TEXT("flamethrower: it must have stopped after MinDuration"), [](UWorld* World, AGBPlayerCharacter*)
		{
			int32 Beams = 0;
			for (TActorIterator<AGBSpell_Beam> It(World); It; ++It)
			{
				++Beams;
			}
			const double Elapsed = FPlatformTime::Seconds() - StartSeconds;
			UE_LOG(LogGB, Display, TEXT("SMOKE flamethrower after=%.2fs alive=%d"), Elapsed - (35.4 + PathCheckShift), Beams);
			if (Beams != 0)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE flamethrower: the beam is still alive with no key held (beams=%d)"), Beams);
			}
		});
		// The flamethrower cooldown is 6 s, so the interrupt checks wait until it is ready again.
		Add(42.0f + S, TEXT("flamethrower interrupt: start a beam for the strong hit"), [](UWorld* World, AGBPlayerCharacter* Player)
		{
			SetQTEEnabled(false);
			Player->GetEquipment()->SwitchTo(EGBHeldSlot::Staff);
			Player->GetAttributes()->RestoreAll();
			Player->GetSpellbook()->SetBlockHeld(false);
			if (!Player->GetSpellbook()->KnowsSpell(TEXT("Flamethrower")))
			{
				Player->GetSpellbook()->LearnSpell(TEXT("Flamethrower"));
			}
			Player->GetSpellbook()->EquipSpell(2, TEXT("Flamethrower"));
			const bool bOk = Player->GetSpellbook()->TryCastSlot(2);
			int32 Beams = 0;
			for (TActorIterator<AGBSpell_Beam> It(World); It; ++It)
			{
				++Beams;
			}
			UE_LOG(LogGB, Display, TEXT("SMOKE flamethrower interrupt setup ok=%d beams=%d"), bOk ? 1 : 0, Beams);
			if (!bOk || Beams == 0)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE flamethrower interrupt: the beam did not start (ok=%d beams=%d)"), bOk ? 1 : 0, Beams);
			}
		});
		Add(42.2f + S, TEXT("flamethrower interrupt: a 35% hit must end the beam"), [](UWorld* World, AGBPlayerCharacter* Player)
		{
			Player->GetSpellbook()->SetBlockHeld(false);
			UGBAttributeComponent* Attributes = Player->GetAttributes();
			const float MaxHealth = Attributes ? Attributes->GetMaxHealth() : 0.f;
			FGBDamageInfo Hit;
			Hit.Amount = 0.35f * MaxHealth;
			Hit.Element = EGBElement::Physical;
			Hit.SourceId = TEXT("SmokeInterrupt");
			UGBCombatLibrary::DealDamage(Player, Hit);
			int32 Beams = 0;
			for (TActorIterator<AGBSpell_Beam> It(World); It; ++It)
			{
				++Beams;
			}
			UE_LOG(LogGB, Display, TEXT("SMOKE flamethrower interrupt alive=%d health=%.0f"), Beams, Attributes ? Attributes->GetHealth() : -1.f);
			if (Beams != 0)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE flamethrower interrupt: the beam survived a 35%% max-health hit (beams=%d)"), Beams);
			}
			if (Attributes)
			{
				Attributes->RestoreAll();
			}
		});
		Add(42.3f + S, TEXT("screenshot: interrupt shake"), [](UWorld*, AGBPlayerCharacter*) { Shot(TEXT("11_interrupt_shake")); });
		Add(48.5f + S, TEXT("flamethrower small hit: start a second beam"), [](UWorld* World, AGBPlayerCharacter* Player)
		{
			SetQTEEnabled(false);
			Player->GetEquipment()->SwitchTo(EGBHeldSlot::Staff);
			Player->GetAttributes()->RestoreAll();
			Player->GetSpellbook()->SetBlockHeld(false);
			Player->GetSpellbook()->EquipSpell(2, TEXT("Flamethrower"));
			const bool bOk = Player->GetSpellbook()->TryCastSlot(2);
			int32 Beams = 0;
			for (TActorIterator<AGBSpell_Beam> It(World); It; ++It)
			{
				++Beams;
			}
			UE_LOG(LogGB, Display, TEXT("SMOKE flamethrower small hit setup ok=%d beams=%d"), bOk ? 1 : 0, Beams);
			if (!bOk || Beams == 0)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE flamethrower small hit: the beam did not start (ok=%d beams=%d)"), bOk ? 1 : 0, Beams);
			}
		});
		Add(48.7f + S, TEXT("flamethrower small hit: a 10% hit must not end the beam"), [](UWorld* World, AGBPlayerCharacter* Player)
		{
			Player->GetSpellbook()->SetBlockHeld(false);
			UGBAttributeComponent* Attributes = Player->GetAttributes();
			const float MaxHealth = Attributes ? Attributes->GetMaxHealth() : 0.f;
			FGBDamageInfo Hit;
			Hit.Amount = 0.10f * MaxHealth;
			Hit.Element = EGBElement::Physical;
			Hit.SourceId = TEXT("SmokeSmallHit");
			UGBCombatLibrary::DealDamage(Player, Hit);
			int32 Beams = 0;
			for (TActorIterator<AGBSpell_Beam> It(World); It; ++It)
			{
				++Beams;
			}
			UE_LOG(LogGB, Display, TEXT("SMOKE flamethrower small hit alive=%d"), Beams);
			if (Beams != 1)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE flamethrower small hit: expected the beam to survive a 10%% hit (beams=%d)"), Beams);
			}
			if (Attributes)
			{
				Attributes->RestoreAll();
			}
		});
		Add(48.9f + S, TEXT("elemental build-up on the training dummy"), [](UWorld* World, AGBPlayerCharacter* Player)
		{
			AGBEnemyCharacter* Dummy = nullptr;
			for (TActorIterator<AGBEnemyCharacter> It(World); It; ++It)
			{
				if (It->GetEnemyId() == FName(TEXT("TrainingDummy")) && !It->IsDead())
				{
					Dummy = *It;
					break;
				}
			}
			if (!Dummy)
			{
				const FVector At = Player->GetActorLocation() + Player->GetAimDirection() * 500.f;
				Dummy = AGBEnemyCharacter::SpawnEnemy(World, FName(TEXT("TrainingDummy")), At, FRotator::ZeroRotator, EGBEraPresence::Both);
			}
			if (!Dummy || Dummy->IsDead())
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE elemental: no living TrainingDummy"));
				return;
			}
			UGBElementalStatusComponent* Elemental = Dummy->GetElementalStatus();
			if (!Elemental)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE elemental: dummy has no status component"));
				return;
			}
			// Dedupe: two adds of 20 with the SAME event id leave 20 (times BuildUpTaken, 1 by default).
			const int32 DedupeId = UGBCombatLibrary::NewBuildUpEventId();
			FGBDamageInfo First;
			First.Element = EGBElement::Fire;
			First.Instigator = Player;
			First.BuildUp = 20.f;
			First.BuildUpEventId = DedupeId;
			UGBCombatLibrary::ApplyBuildUp(Dummy, First);
			UGBCombatLibrary::ApplyBuildUp(Dummy, First);
			const float AfterDedupe = Elemental->GetBuildUp(EGBElement::Fire);
			const bool bDedupeOk = FMath::IsNearlyEqual(AfterDedupe, 20.f, 1.e-2f);
			UE_LOG(LogGB, Display, TEXT("SMOKE elemental dedupe ok=%d"), bDedupeOk ? 1 : 0);
			if (!bDedupeOk)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE elemental dedupe FAILED: fire bar is %.2f, expected 20"), AfterDedupe);
			}
			// Clear the fire bar before the per-element trigger checks (fresh decay via ClearAll).
			Elemental->ClearAll();

			const EGBElement Elems[3] = { EGBElement::Fire, EGBElement::Lightning, EGBElement::Ice };
			const TCHAR* Names[3] = { TEXT("Fire"), TEXT("Lightning"), TEXT("Ice") };
			for (int32 i = 0; i < 3; ++i)
			{
				Elemental->ClearAll();
				bool bTriggered = false;
				int32 Adds = 0;
				for (int32 n = 0; n < 10 && !bTriggered; ++n)
				{
					FGBDamageInfo Info;
					Info.Element = Elems[i];
					Info.Instigator = Player;
					Info.BuildUp = 30.f;
					Info.BuildUpEventId = UGBCombatLibrary::NewBuildUpEventId();
					UGBCombatLibrary::ApplyBuildUp(Dummy, Info);
					++Adds;
					bTriggered = Elemental->IsStatusActive(Elems[i]);
				}
				UE_LOG(LogGB, Display, TEXT("SMOKE elemental %s triggered=%d adds=%d"), Names[i], bTriggered ? 1 : 0, Adds);
				if (!bTriggered)
				{
					UE_LOG(LogGB, Error, TEXT("SMOKE elemental %s FAILED to trigger"), Names[i]);
					continue;
				}
				if (Elems[i] == EGBElement::Fire)
				{
					UGBStatusEffectComponent* Effects = Dummy->GetStatusEffects();
					const bool bIgnite = Effects && Effects->HasEffect(FName(TEXT("Ignite")));
					UE_LOG(LogGB, Display, TEXT("SMOKE elemental Fire ignite=%d"), bIgnite ? 1 : 0);
					if (!bIgnite)
					{
						UE_LOG(LogGB, Error, TEXT("SMOKE elemental Fire: Ignite effect missing"));
					}
				}
				FGBDamageInfo Extra;
				Extra.Element = Elems[i];
				Extra.Instigator = Player;
				Extra.BuildUp = 30.f;
				Extra.BuildUpEventId = UGBCombatLibrary::NewBuildUpEventId();
				const float Before = Elemental->GetBuildUp(Elems[i]);
				UGBCombatLibrary::ApplyBuildUp(Dummy, Extra);
				const float After = Elemental->GetBuildUp(Elems[i]);
				const bool bBlocked = Elemental->IsStatusActive(Elems[i]) && FMath::IsNearlyEqual(Before, After, 1.e-3f);
				UE_LOG(LogGB, Display, TEXT("SMOKE elemental %s blockedWhileActive=%d"), Names[i], bBlocked ? 1 : 0);
				if (!bBlocked)
				{
					UE_LOG(LogGB, Error, TEXT("SMOKE elemental %s: add during status was not refused"), Names[i]);
				}
			}
			Elemental->ClearAll();
		});
		Add(49.9f + S, TEXT("pistol display: take the pistol with a full magazine"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			Player->GetEquipment()->SwitchTo(EGBHeldSlot::Gun);
			if (AGBGun* Gun = Cast<AGBGun>(Player->GetEquipment()->GetActiveItem()))
			{
				Gun->RefillAmmo();
			}
		});
		Add(50.1f + S, TEXT("pistol display: full magazine text, one shot"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			AGBGun* Gun = Cast<AGBGun>(Player->GetEquipment()->GetActiveItem());
			if (!Gun || Gun->GetWeaponId() != FName(TEXT("Pistol")))
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE pistol display: no pistol in hand"));
				return;
			}
			PistolDisplayFullText = Gun->GetViewModelDisplayText();
			UE_LOG(LogGB, Display, TEXT("SMOKE pistol display text=%s"), *PistolDisplayFullText);
			Gun->StartPrimary();
			Gun->StopPrimary();
		});
		Add(50.3f + S, TEXT("pistol display: text after one shot"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			AGBGun* Gun = Cast<AGBGun>(Player->GetEquipment()->GetActiveItem());
			if (!Gun || Gun->GetWeaponId() != FName(TEXT("Pistol")))
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE pistol display: no pistol in hand"));
				return;
			}
			const FString AfterText = Gun->GetViewModelDisplayText();
			UE_LOG(LogGB, Display, TEXT("SMOKE pistol display after shot text=%s"), *AfterText);
			if (!PistolDisplayFullText.IsNumeric() || !AfterText.IsNumeric() || FCString::Atoi(*PistolDisplayFullText) - FCString::Atoi(*AfterText) != 1)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE pistol display FAILED: text '%s' then '%s' after one shot (expected one round less)"), *PistolDisplayFullText, *AfterText);
			}
		});
		Add(50.5f + S, TEXT("pistol: meshes and materials in hand, hip screenshot, spend rounds, reload"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			Player->GetEquipment()->SwitchTo(EGBHeldSlot::Gun);
			AGBGun* Gun = Cast<AGBGun>(Player->GetEquipment()->GetActiveItem());
			if (!Gun || Gun->GetWeaponId() != FName(TEXT("Pistol")))
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE pistol: no pistol in hand"));
				return;
			}
			const int32 Meshes = Gun->GetViewModelMeshCount();
			const int32 Mids = Gun->GetViewModelMIDCount();
			UE_LOG(LogGB, Display, TEXT("SMOKE pistol viewmodel meshes=%d mids=%d"), Meshes, Mids);
			if (Meshes != 6 || Mids <= 6)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE pistol viewmodel FAILED: meshes=%d (expected 6) mids=%d (expected more than 6)"), Meshes, Mids);
			}
			Shot(TEXT("13a_pistol_hip"));
			FGBAmmoState Ammo = Gun->GetAmmo();
			if (Ammo.SpareMagazines <= 0)
			{
				Gun->RefillAmmo();
				Ammo = Gun->GetAmmo();
			}
			if (Ammo.InMagazine >= Gun->GetMagazineSize())
			{
				Ammo.InMagazine = Gun->GetMagazineSize() - 3;
				Gun->SetAmmo(Ammo);
			}
		});
		// Reload animation: three pinned beats (a reload is too short to photograph while it plays), then a real
		// reload to its end, then one cancelled halfway. The window is longer than the old single step, so the last
		// step winds the smoke clock back to where the old step ended and everything after keeps its timing.
		const float PistolReloadStart = 50.5f + S;
		constexpr float PistolReloadWindow = 4.1f;
		Add(PistolReloadStart + 0.5f, TEXT("pistol reload: pinned at 0.30, old magazine leaving, screenshot"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			AGBGun* Gun = Cast<AGBGun>(Player->GetEquipment()->GetActiveItem());
			if (!Gun || Gun->GetWeaponId() != FName(TEXT("Pistol")))
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE pistol reload: no pistol in hand"));
				return;
			}
			Gun->SetReloadPreview(0.30f);
			CheckReloadBeat(TEXT("pistol"), Gun, 0.30f, true);
			Shot(TEXT("13b_pistol_reload_out"));
		});
		Add(PistolReloadStart + 1.0f, TEXT("pistol reload: pinned at 0.45, empty well, screenshot"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			AGBGun* Gun = Cast<AGBGun>(Player->GetEquipment()->GetActiveItem());
			if (!Gun || Gun->GetWeaponId() != FName(TEXT("Pistol")))
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE pistol reload: no pistol in hand"));
				return;
			}
			Gun->SetReloadPreview(0.45f);
			CheckReloadBeat(TEXT("pistol"), Gun, 0.45f, false);
			Shot(TEXT("13f_pistol_reload_empty"));
		});
		Add(PistolReloadStart + 1.5f, TEXT("pistol reload: pinned at 0.70, new magazine going in, screenshot"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			AGBGun* Gun = Cast<AGBGun>(Player->GetEquipment()->GetActiveItem());
			if (!Gun || Gun->GetWeaponId() != FName(TEXT("Pistol")))
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE pistol reload: no pistol in hand"));
				return;
			}
			Gun->SetReloadPreview(0.70f);
			CheckReloadBeat(TEXT("pistol"), Gun, 0.70f, true);
			Shot(TEXT("13g_pistol_reload_in"));
		});
		Add(PistolReloadStart + 2.0f, TEXT("pistol reload: release the pin, start a real reload"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			AGBGun* Gun = Cast<AGBGun>(Player->GetEquipment()->GetActiveItem());
			if (!Gun || Gun->GetWeaponId() != FName(TEXT("Pistol")))
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE pistol reload: no pistol in hand"));
				return;
			}
			Gun->ClearReloadPreview();
			ReloadAmmoBefore = Gun->GetAmmo();
			Gun->Reload();
		});
		Add(PistolReloadStart + 2.4f, TEXT("pistol reload: the display shows -- while it runs"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			AGBGun* Gun = Cast<AGBGun>(Player->GetEquipment()->GetActiveItem());
			if (!Gun || Gun->GetWeaponId() != FName(TEXT("Pistol")))
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE pistol reload: no pistol in hand"));
				return;
			}
			const FString ReloadText = Gun->GetViewModelDisplayText();
			UE_LOG(LogGB, Display, TEXT("SMOKE pistol display reloading text=%s reloading=%d"), *ReloadText, Gun->IsReloading() ? 1 : 0);
			if (ReloadText != TEXT("--"))
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE pistol display FAILED: text '%s' during the reload (expected --)"), *ReloadText);
			}
		});
		Add(PistolReloadStart + 3.4f, TEXT("pistol reload: a real reload ran to its end, then spend rounds and start another"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			AGBGun* Gun = Cast<AGBGun>(Player->GetEquipment()->GetActiveItem());
			if (!Gun || Gun->GetWeaponId() != FName(TEXT("Pistol")))
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE pistol reload: no pistol in hand"));
				return;
			}
			CheckReloadFinished(TEXT("pistol"), Gun);
			FGBAmmoState Ammo = Gun->GetAmmo();
			Ammo.InMagazine = Gun->GetMagazineSize() - 3;
			Gun->SetAmmo(Ammo);
			ReloadAmmoBefore = Gun->GetAmmo();
			Gun->Reload();
		});
		Add(PistolReloadStart + 3.7f, TEXT("pistol reload: cancelled halfway by a weapon switch"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			AGBGun* Gun = Cast<AGBGun>(Player->GetEquipment()->GetActiveItem());
			UE_LOG(LogGB, Display, TEXT("SMOKE pistol reload cancel: switching away at progress=%.2f"), Gun ? Gun->GetReloadProgress() : -1.f);
			Player->GetEquipment()->SwitchTo(EGBHeldSlot::Staff);
			Player->GetEquipment()->SwitchTo(EGBHeldSlot::Gun);
		});
		Add(PistolReloadStart + PistolReloadWindow, TEXT("pistol reload: cancelled, magazine at rest and visible, ammo unchanged; clock back"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			AGBGun* Gun = Cast<AGBGun>(Player->GetEquipment()->GetActiveItem());
			if (!Gun || Gun->GetWeaponId() != FName(TEXT("Pistol")))
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE pistol reload: no pistol in hand"));
				return;
			}
			CheckReloadCancelled(TEXT("pistol"), Gun);
			// The old single reload step sat 0.6 s after the start of this block: continue the clock from there.
			StartSeconds += PistolReloadWindow - 0.6f;
		});
		Add(51.7f + S, TEXT("pistol enchant: cast the weapon enchant with the pistol in hand"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			Player->GetEquipment()->SwitchTo(EGBHeldSlot::Gun);
			Player->GetAttributes()->RestoreAll();
			SetQTEEnabled(false);
			UE_LOG(LogGB, Display, TEXT("SMOKE pistol enchant ok=%d"), Player->GetSpellbook()->TryCastEnchant() ? 1 : 0);
		});
		Add(52.3f + S, TEXT("pistol enchanted: glow and bar, screenshot, staff back in hand"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			AGBGun* Gun = Cast<AGBGun>(Player->GetEquipment()->GetActiveItem());
			if (!Gun || Gun->GetWeaponId() != FName(TEXT("Pistol")))
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE pistol glow: no pistol in hand"));
				return;
			}
			const float Imbue = Gun->GetViewModelImbueSent();
			const FString ElementName = StaticEnum<EGBElement>()->GetNameStringByValue(static_cast<int64>(Gun->GetViewModelGlowElement()));
			UE_LOG(LogGB, Display, TEXT("SMOKE pistol glow element=%s imbue=%.2f"), *ElementName, Imbue);
			if (!Gun->IsViewModelGlowEnchanted() || Imbue <= 0.f || Imbue > 1.f)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE pistol glow FAILED: enchanted=%d element=%s imbue=%.2f (expected enchanted, imbue in (0, 1])"),
					Gun->IsViewModelGlowEnchanted() ? 1 : 0, *ElementName, Imbue);
			}
			Shot(TEXT("13c_pistol_enchanted"));
		});
		Add(52.9f + S, TEXT("pistol weak point: a head hit by the player lights the light"), [](UWorld* World, AGBPlayerCharacter* Player)
		{
			const FVector At = Player->GetActorLocation() + Player->GetAimDirection() * 500.f;
			AGBEnemyCharacter* Enemy = AGBEnemyCharacter::SpawnEnemy(World, FName(TEXT("Orc_Past")), At, FRotator::ZeroRotator, EGBEraPresence::Both);
			PistolFlashTarget = Enemy;
			if (!Enemy)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE pistol weakpoint: could not spawn the test orc"));
				return;
			}
			FGBDamageInfo Info;
			Info.Amount = 1.f;
			Info.Instigator = Player;
			Info.bHeadshot = true;
			UGBCombatLibrary::DealDamage(Enemy, Info);
		});
		Add(52.95f + S, TEXT("pistol weak point: flash is on, screenshot"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			AGBGun* Gun = Cast<AGBGun>(Player->GetEquipment()->GetActiveItem());
			if (!Gun || Gun->GetWeaponId() != FName(TEXT("Pistol")))
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE pistol weakpoint: no pistol in hand"));
				return;
			}
			const float Flash = Gun->GetViewModelWeakPointFlash();
			UE_LOG(LogGB, Display, TEXT("SMOKE pistol weakpoint flash=%.2f"), Flash);
			if (Flash <= 0.5f)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE pistol weakpoint FAILED: flash=%.2f right after a weak-point hit (expected more than 0.5)"), Flash);
			}
			Shot(TEXT("13d_pistol_weakpoint"));
		});
		Add(53.95f + S, TEXT("pistol weak point: flash is off again, then a body hit"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			AGBGun* Gun = Cast<AGBGun>(Player->GetEquipment()->GetActiveItem());
			AGBEnemyCharacter* Enemy = PistolFlashTarget.Get();
			if (!Gun || !Enemy)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE pistol weakpoint: no pistol in hand or no test orc"));
				return;
			}
			const float After = Gun->GetViewModelWeakPointFlash();
			UE_LOG(LogGB, Display, TEXT("SMOKE pistol weakpoint flash after=%.2f"), After);
			if (After > 0.001f)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE pistol weakpoint FAILED: flash=%.2f one second later (expected 0)"), After);
			}
			FGBDamageInfo Info;
			Info.Amount = 1.f;
			Info.Instigator = Player;
			Info.bHeadshot = false;
			UGBCombatLibrary::DealDamage(Enemy, Info);
		});
		Add(54.05f + S, TEXT("pistol body shot: the light stays off; half the enchant left"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			AGBGun* Gun = Cast<AGBGun>(Player->GetEquipment()->GetActiveItem());
			if (!Gun || Gun->GetWeaponId() != FName(TEXT("Pistol")))
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE pistol bodyshot: no pistol in hand"));
				return;
			}
			const float Flash = Gun->GetViewModelWeakPointFlash();
			UE_LOG(LogGB, Display, TEXT("SMOKE pistol bodyshot flash=%.2f"), Flash);
			if (Flash > 0.001f)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE pistol bodyshot FAILED: flash=%.2f after a body hit (expected 0)"), Flash);
			}
			if (AGBEnemyCharacter* Enemy = PistolFlashTarget.Get())
			{
				Enemy->Destroy();
			}
			PistolFlashTarget.Reset();
			// Half the enchant left, to look at the bar draining from the muzzle end.
			if (UGBStatusEffectComponent* Effects = Player->GetStatusEffects())
			{
				for (const FGBActiveStatusEffect& Active : Effects->GetActiveEffects())
				{
					if (Active.EffectId.ToString().StartsWith(TEXT("Enchant_")))
					{
						const FName EnchantId = Active.EffectId;
						Effects->SetTimeRemaining(EnchantId, 0.5f * Active.TotalDuration);
						break;
					}
				}
			}
		});
		Add(54.35f + S, TEXT("pistol half bar: imbue about half, screenshot"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			AGBGun* Gun = Cast<AGBGun>(Player->GetEquipment()->GetActiveItem());
			if (!Gun || Gun->GetWeaponId() != FName(TEXT("Pistol")))
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE pistol half bar: no pistol in hand"));
				return;
			}
			const float Imbue = Gun->GetViewModelImbueSent();
			UE_LOG(LogGB, Display, TEXT("SMOKE pistol half bar imbue=%.2f"), Imbue);
			CheckEnchantBlink(TEXT("pistol"), Gun, false, TEXT("at half the enchant"));
			if (FMath::Abs(Imbue - 0.5f) > 0.05f)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE pistol half bar FAILED: imbue=%.2f (expected about 0.5)"), Imbue);
			}
			Shot(TEXT("13e_pistol_half_bar"));
		});
		Add(54.5f + S, TEXT("pistol shot kick and muzzle flash"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			AGBGun* Gun = Cast<AGBGun>(Player->GetEquipment()->GetActiveItem());
			if (!Gun || Gun->GetWeaponId() != FName(TEXT("Pistol")))
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE pistol muzzle flash: no pistol in hand"));
				return;
			}
			if (Gun->GetAmmo().InMagazine <= 0)
			{
				Gun->RefillAmmo();
			}
			Gun->StartPrimary();
			Gun->StopPrimary();
			const bool bVisible = Gun->IsMuzzleFlashVisible();
			const float Kick = Gun->GetViewModelKickBackCm();
			UE_LOG(LogGB, Display, TEXT("SMOKE pistol muzzle flash visible=%d kick=%.2f"), bVisible ? 1 : 0, Kick);
			if (!bVisible || Kick <= 0.1f)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE pistol muzzle flash FAILED: visible=%d kick=%.2f right after a shot (expected visible and a kick)"), bVisible ? 1 : 0, Kick);
			}
			SetEnchantTimeLeft(Player, 2.5f);
		});
		Add(54.95f + S, TEXT("pistol done: staff back in hand"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			if (const AGBGun* BlinkGun = Cast<AGBGun>(Player->GetEquipment()->GetActiveItem()))
			{
				CheckEnchantBlink(TEXT("pistol"), BlinkGun, true, TEXT("with 2 seconds left"));
			}
			Player->GetEquipment()->SwitchTo(EGBHeldSlot::Staff);
		});
		Add(55.2f + S, TEXT("rifle: own it, equip it in the gun slot, full magazine, no enchant"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			Player->GetEquipment()->GiveWeapon(TEXT("Rifle"), false, false);
			Player->GetEquipment()->EquipOwnedWeapon(TEXT("Rifle"));
			Player->GetEquipment()->SwitchTo(EGBHeldSlot::Gun);
			if (UGBStatusEffectComponent* Effects = Player->GetStatusEffects())
			{
				Effects->RemoveAllEffects();
			}
			if (AGBGun* Gun = Cast<AGBGun>(Player->GetEquipment()->GetActiveItem()))
			{
				Gun->RefillAmmo();
			}
		});
		Add(55.6f + S, TEXT("rifle: meshes and materials in hand, hip screenshot"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			AGBGun* Gun = RifleInHand(Player);
			if (!Gun)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE rifle: no rifle in hand"));
				return;
			}
			const int32 Meshes = Gun->GetViewModelMeshCount();
			const int32 Mids = Gun->GetViewModelMIDCount();
			UE_LOG(LogGB, Display, TEXT("SMOKE rifle viewmodel meshes=%d mids=%d"), Meshes, Mids);
			if (Meshes != 5 || Mids <= 5)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE rifle viewmodel FAILED: meshes=%d (expected 5) mids=%d (expected more than 5)"), Meshes, Mids);
			}
			Shot(TEXT("14a_rifle_hip"));
		});
		Add(56.0f + S, TEXT("rifle display: full magazine text, one shot"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			AGBGun* Gun = RifleInHand(Player);
			if (!Gun)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE rifle display: no rifle in hand"));
				return;
			}
			RifleDisplayFullText = Gun->GetViewModelDisplayText();
			UE_LOG(LogGB, Display, TEXT("SMOKE rifle display text=%s"), *RifleDisplayFullText);
			Gun->StartPrimary();
			Gun->StopPrimary();
		});
		Add(56.3f + S, TEXT("rifle display: text after one shot, then reload"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			AGBGun* Gun = RifleInHand(Player);
			if (!Gun)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE rifle display: no rifle in hand"));
				return;
			}
			const FString AfterText = Gun->GetViewModelDisplayText();
			UE_LOG(LogGB, Display, TEXT("SMOKE rifle display after shot text=%s"), *AfterText);
			if (!RifleDisplayFullText.IsNumeric() || !AfterText.IsNumeric() || FCString::Atoi(*RifleDisplayFullText) - FCString::Atoi(*AfterText) != 1)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE rifle display FAILED: text '%s' then '%s' after one shot (expected one round less)"), *RifleDisplayFullText, *AfterText);
			}
		});
		// Rifle reload animation: the same sequence as the pistol's, with the rifle's longer reload.
		const float RifleReloadStart = 56.3f + S;
		constexpr float RifleReloadWindow = 5.4f;
		Add(RifleReloadStart + 1.0f, TEXT("rifle reload: pinned at 0.30, old magazine leaving, screenshot"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			AGBGun* Gun = RifleInHand(Player);
			if (!Gun)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE rifle reload: no rifle in hand"));
				return;
			}
			Gun->SetReloadPreview(0.30f);
			CheckReloadBeat(TEXT("rifle"), Gun, 0.30f, true);
			Shot(TEXT("14b_rifle_reload_out"));
		});
		Add(RifleReloadStart + 1.5f, TEXT("rifle reload: pinned at 0.45, empty well, screenshot"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			AGBGun* Gun = RifleInHand(Player);
			if (!Gun)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE rifle reload: no rifle in hand"));
				return;
			}
			Gun->SetReloadPreview(0.45f);
			CheckReloadBeat(TEXT("rifle"), Gun, 0.45f, false);
			Shot(TEXT("14h_rifle_reload_empty"));
		});
		Add(RifleReloadStart + 2.0f, TEXT("rifle reload: pinned at 0.70, new magazine going in, screenshot"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			AGBGun* Gun = RifleInHand(Player);
			if (!Gun)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE rifle reload: no rifle in hand"));
				return;
			}
			Gun->SetReloadPreview(0.70f);
			CheckReloadBeat(TEXT("rifle"), Gun, 0.70f, true);
			Shot(TEXT("14i_rifle_reload_in"));
		});
		Add(RifleReloadStart + 2.5f, TEXT("rifle reload: release the pin, start a real reload"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			AGBGun* Gun = RifleInHand(Player);
			if (!Gun)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE rifle reload: no rifle in hand"));
				return;
			}
			Gun->ClearReloadPreview();
			ReloadAmmoBefore = Gun->GetAmmo();
			Gun->Reload();
		});
		Add(RifleReloadStart + 2.9f, TEXT("rifle reload: the display shows -- while it runs"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			AGBGun* Gun = RifleInHand(Player);
			if (!Gun)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE rifle reload: no rifle in hand"));
				return;
			}
			const FString ReloadText = Gun->GetViewModelDisplayText();
			UE_LOG(LogGB, Display, TEXT("SMOKE rifle display reloading text=%s reloading=%d"), *ReloadText, Gun->IsReloading() ? 1 : 0);
			if (ReloadText != TEXT("--"))
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE rifle display FAILED: text '%s' during the reload (expected --)"), *ReloadText);
			}
		});
		Add(RifleReloadStart + 4.7f, TEXT("rifle reload: a real reload ran to its end, then spend rounds and start another"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			AGBGun* Gun = RifleInHand(Player);
			if (!Gun)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE rifle reload: no rifle in hand"));
				return;
			}
			CheckReloadFinished(TEXT("rifle"), Gun);
			FGBAmmoState Ammo = Gun->GetAmmo();
			Ammo.InMagazine = Gun->GetMagazineSize() - 5;
			Gun->SetAmmo(Ammo);
			ReloadAmmoBefore = Gun->GetAmmo();
			Gun->Reload();
		});
		Add(RifleReloadStart + 5.0f, TEXT("rifle reload: cancelled halfway by a weapon switch"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			AGBGun* Gun = RifleInHand(Player);
			UE_LOG(LogGB, Display, TEXT("SMOKE rifle reload cancel: switching away at progress=%.2f"), Gun ? Gun->GetReloadProgress() : -1.f);
			Player->GetEquipment()->SwitchTo(EGBHeldSlot::Staff);
			Player->GetEquipment()->SwitchTo(EGBHeldSlot::Gun);
		});
		Add(RifleReloadStart + RifleReloadWindow, TEXT("rifle reload: cancelled, magazine at rest and visible, ammo unchanged; clock back"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			AGBGun* Gun = RifleInHand(Player);
			if (!Gun)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE rifle reload: no rifle in hand"));
				return;
			}
			CheckReloadCancelled(TEXT("rifle"), Gun);
			// The old single reload step sat 1.0 s after the start of this block: continue the clock from there.
			StartSeconds += RifleReloadWindow - 1.0f;
		});
		Add(58.4f + S, TEXT("rifle enchant: lightning enchant on (the cast itself is on an 18 s cooldown since the pistol's)"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			Player->GetEquipment()->SwitchTo(EGBHeldSlot::Gun);
			UGBStatusEffectComponent* Effects = Player->GetStatusEffects();
			UE_LOG(LogGB, Display, TEXT("SMOKE rifle enchant ok=%d"), (Effects && Effects->ApplyEffect(FName(TEXT("Enchant_Lightning")), Player)) ? 1 : 0);
		});
		Add(59.0f + S, TEXT("rifle weak point: a head hit by the player lights the lenses"), [](UWorld* World, AGBPlayerCharacter* Player)
		{
			const FVector At = Player->GetActorLocation() + Player->GetAimDirection() * 500.f;
			AGBEnemyCharacter* Enemy = AGBEnemyCharacter::SpawnEnemy(World, FName(TEXT("Orc_Past")), At, FRotator::ZeroRotator, EGBEraPresence::Both);
			RifleFlashTarget = Enemy;
			if (!Enemy)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE rifle weakpoint: could not spawn the test orc"));
				return;
			}
			FGBDamageInfo Info;
			Info.Amount = 1.f;
			Info.Instigator = Player;
			Info.bHeadshot = true;
			UGBCombatLibrary::DealDamage(Enemy, Info);
		});
		Add(59.05f + S, TEXT("rifle weak point: flash is on, screenshot"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			AGBGun* Gun = RifleInHand(Player);
			if (!Gun)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE rifle weakpoint: no rifle in hand"));
				return;
			}
			const float Flash = Gun->GetViewModelWeakPointFlash();
			UE_LOG(LogGB, Display, TEXT("SMOKE rifle weakpoint flash=%.2f"), Flash);
			if (Flash <= 0.5f)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE rifle weakpoint FAILED: flash=%.2f right after a weak-point hit (expected more than 0.5)"), Flash);
			}
			Shot(TEXT("14c_rifle_weakpoint"));
		});
		Add(60.05f + S, TEXT("rifle weak point: flash is off again, then a body hit"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			AGBGun* Gun = RifleInHand(Player);
			AGBEnemyCharacter* Enemy = RifleFlashTarget.Get();
			if (!Gun || !Enemy)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE rifle weakpoint: no rifle in hand or no test orc"));
				return;
			}
			const float After = Gun->GetViewModelWeakPointFlash();
			UE_LOG(LogGB, Display, TEXT("SMOKE rifle weakpoint flash after=%.2f"), After);
			if (After > 0.001f)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE rifle weakpoint FAILED: flash=%.2f one second later (expected 0)"), After);
			}
			FGBDamageInfo Info;
			Info.Amount = 1.f;
			Info.Instigator = Player;
			Info.bHeadshot = false;
			UGBCombatLibrary::DealDamage(Enemy, Info);
		});
		Add(60.15f + S, TEXT("rifle body shot: the lenses stay dark; half the enchant left"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			AGBGun* Gun = RifleInHand(Player);
			if (!Gun)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE rifle bodyshot: no rifle in hand"));
				return;
			}
			const float Flash = Gun->GetViewModelWeakPointFlash();
			UE_LOG(LogGB, Display, TEXT("SMOKE rifle bodyshot flash=%.2f"), Flash);
			if (Flash > 0.001f)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE rifle bodyshot FAILED: flash=%.2f after a body hit (expected 0)"), Flash);
			}
			if (AGBEnemyCharacter* Enemy = RifleFlashTarget.Get())
			{
				Enemy->Destroy();
			}
			RifleFlashTarget.Reset();
			// Half the enchant left, to look at the bar draining from the muzzle end.
			if (UGBStatusEffectComponent* Effects = Player->GetStatusEffects())
			{
				for (const FGBActiveStatusEffect& Active : Effects->GetActiveEffects())
				{
					if (Active.EffectId.ToString().StartsWith(TEXT("Enchant_")))
					{
						const FName EnchantId = Active.EffectId;
						Effects->SetTimeRemaining(EnchantId, 0.5f * Active.TotalDuration);
						break;
					}
				}
			}
		});
		Add(60.45f + S, TEXT("rifle half bar: imbue about half, screenshot"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			AGBGun* Gun = RifleInHand(Player);
			if (!Gun)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE rifle half bar: no rifle in hand"));
				return;
			}
			const float Imbue = Gun->GetViewModelImbueSent();
			UE_LOG(LogGB, Display, TEXT("SMOKE rifle half bar imbue=%.2f"), Imbue);
			CheckEnchantBlink(TEXT("rifle"), Gun, false, TEXT("at half the enchant"));
			if (FMath::Abs(Imbue - 0.5f) > 0.05f)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE rifle half bar FAILED: imbue=%.2f (expected about 0.5)"), Imbue);
			}
			Shot(TEXT("14d_rifle_half_bar"));
		});
		Add(61.0f + S, TEXT("rifle shot kick and muzzle flash, screenshot"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			AGBGun* Gun = RifleInHand(Player);
			if (!Gun)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE rifle muzzle flash: no rifle in hand"));
				return;
			}
			Gun->StartPrimary();
			Gun->StopPrimary();
			const bool bVisible = Gun->IsMuzzleFlashVisible();
			const float Kick = Gun->GetViewModelKickBackCm();
			UE_LOG(LogGB, Display, TEXT("SMOKE rifle muzzle flash visible=%d kick=%.2f"), bVisible ? 1 : 0, Kick);
			if (!bVisible || Kick <= 0.1f)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE rifle muzzle flash FAILED: visible=%d kick=%.2f right after a shot (expected visible and a kick)"), bVisible ? 1 : 0, Kick);
			}
			SetEnchantTimeLeft(Player, 2.5f);
			Shot(TEXT("14e_rifle_fire"));
		});
		Add(61.6f + S, TEXT("rifle done: staff back in hand"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			if (const AGBGun* BlinkGun = RifleInHand(Player))
			{
				CheckEnchantBlink(TEXT("rifle"), BlinkGun, true, TEXT("with 2 seconds left"));
			}
			Player->GetEquipment()->SwitchTo(EGBHeldSlot::Staff);
		});
		// ---- Shotgun: model, pump, shell-by-shell reload (feedback15). Its clock runs on from the rifle block and is wound back at the end. ----
		const float SG = 61.7f + S;
		Add(SG, TEXT("shotgun: own it, equip it in the gun slot, full ammo, no enchant"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			Player->GetEquipment()->GiveWeapon(TEXT("Shotgun"), false, false);
			Player->GetEquipment()->EquipOwnedWeapon(TEXT("Shotgun"));
			Player->GetEquipment()->SwitchTo(EGBHeldSlot::Gun);
			if (UGBStatusEffectComponent* Effects = Player->GetStatusEffects())
			{
				Effects->RemoveAllEffects();
			}
			if (AGBGun* Gun = Cast<AGBGun>(Player->GetEquipment()->GetActiveItem()))
			{
				Gun->RefillAmmo();
			}
		});
		Add(SG + 0.4f, TEXT("shotgun: meshes and materials in hand, display 6, hip screenshot"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			AGBGun* Gun = ShotgunInHand(Player);
			if (!Gun)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE shotgun: no shotgun in hand"));
				return;
			}
			const int32 Meshes = Gun->GetViewModelMeshCount();
			const int32 Mids = Gun->GetViewModelMIDCount();
			const FGBAmmoState Ammo = Gun->GetAmmo();
			UE_LOG(LogGB, Display, TEXT("SMOKE shotgun viewmodel meshes=%d mids=%d pump=%d display=%s ammo %d / spare rounds %d"), Meshes, Mids,
				Gun->HasPump() ? 1 : 0, *Gun->GetViewModelDisplayText(), Ammo.InMagazine, Ammo.SpareMagazines);
			if (Meshes != 6 || Mids <= 6 || !Gun->HasPump())
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE shotgun viewmodel FAILED: meshes=%d (expected 6) mids=%d (expected more than 6) pump=%d"), Meshes, Mids, Gun->HasPump() ? 1 : 0);
			}
			if (Gun->GetViewModelDisplayText() != TEXT("6") || Ammo.InMagazine != 6 || Ammo.SpareMagazines != 24 || !Gun->ReloadsPerRound())
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE shotgun ammo FAILED: display '%s', %d in the gun, %d spare (expected 6, 6, 24 rounds) perRound=%d"),
					*Gun->GetViewModelDisplayText(), Ammo.InMagazine, Ammo.SpareMagazines, Gun->ReloadsPerRound() ? 1 : 0);
			}
			CheckShotgunAtRest(TEXT("hip"), Gun);
			Shot(TEXT("15a_shotgun_hip"));
		});
		Add(SG + 1.1f, TEXT("shotgun shot: kick, muzzle flash, one round less"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			AGBGun* Gun = ShotgunInHand(Player);
			if (!Gun)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE shotgun shot: no shotgun in hand"));
				return;
			}
			Gun->StartPrimary();
			Gun->StopPrimary();
			const bool bVisible = Gun->IsMuzzleFlashVisible();
			const float Kick = Gun->GetViewModelKickBackCm();
			UE_LOG(LogGB, Display, TEXT("SMOKE shotgun shot: muzzle flash visible=%d kick=%.2f ammo=%d"), bVisible ? 1 : 0, Kick, Gun->GetAmmo().InMagazine);
			if (!bVisible || Kick <= 0.1f || Gun->GetAmmo().InMagazine != 5)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE shotgun shot FAILED: visible=%d kick=%.2f ammo=%d right after a shot (expected a flash, a kick and 5 rounds)"),
					bVisible ? 1 : 0, Kick, Gun->GetAmmo().InMagazine);
			}
		});
		Add(SG + 1.47f, TEXT("shotgun pump: slid back during the interval, display 5"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			AGBGun* Gun = ShotgunInHand(Player);
			if (!Gun)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE shotgun pump: no shotgun in hand"));
				return;
			}
			const float Pump = Gun->GetViewModelPumpOffsetCm();
			const FString Text = Gun->GetViewModelDisplayText();
			UE_LOG(LogGB, Display, TEXT("SMOKE shotgun pump 0.37 s after the shot: %.2f cm from rest, display=%s"), Pump, *Text);
			if (Pump < 1.2f)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE shotgun pump FAILED: only %.2f cm from rest in the middle of the interval (expected it back, about 4 cm)"), Pump);
			}
			if (Text != TEXT("5"))
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE shotgun display FAILED: '%s' after one shot (expected 5)"), *Text);
			}
		});
		Add(SG + 1.8f, TEXT("shotgun pump: home again before the next shot is allowed"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			AGBGun* Gun = ShotgunInHand(Player);
			if (!Gun)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE shotgun pump: no shotgun in hand"));
				return;
			}
			CheckShotgunAtRest(TEXT("after the pump"), Gun);
		});
		Add(SG + 2.0f, TEXT("shotgun pump: pinned at the back of its travel, screenshot"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			AGBGun* Gun = ShotgunInHand(Player);
			if (!Gun)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE shotgun pump: no shotgun in hand"));
				return;
			}
			Gun->SetPumpPreview(1.f);
			const float Pump = Gun->GetViewModelPumpOffsetCm();
			const float RootCm = Gun->GetViewModelRootOffsetCm();
			UE_LOG(LogGB, Display, TEXT("SMOKE shotgun pump pinned: %.2f cm from rest, root %.2f cm %.2f deg, hand shift %s"), Pump, RootCm,
				Gun->GetViewModelRootAngleDeg(), *Gun->GetPumpHandShift().ToString());
			if (Pump < 3.5f || RootCm < 0.1f)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE shotgun pump FAILED: pinned back but the pump is %.2f cm from rest and the root %.2f cm (expected about 4 and a little rack)"), Pump, RootCm);
			}
			Shot(TEXT("15b_shotgun_pump"));
		});
		Add(SG + 2.5f, TEXT("shotgun pump: pin released, pump home"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			AGBGun* Gun = ShotgunInHand(Player);
			if (!Gun)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE shotgun pump: no shotgun in hand"));
				return;
			}
			Gun->ClearPumpPreview();
			CheckShotgunAtRest(TEXT("pin released"), Gun);
		});
		Add(SG + 2.6f, TEXT("shotgun enchant: ice enchant on"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			UGBStatusEffectComponent* Effects = Player->GetStatusEffects();
			UE_LOG(LogGB, Display, TEXT("SMOKE shotgun enchant ok=%d"), (Effects && Effects->ApplyEffect(FName(TEXT("Enchant_Ice")), Player)) ? 1 : 0);
		});
		Add(SG + 3.2f, TEXT("shotgun weak point: a head hit by the player lights the lens"), [](UWorld* World, AGBPlayerCharacter* Player)
		{
			const FVector At = Player->GetActorLocation() + Player->GetAimDirection() * 500.f;
			AGBEnemyCharacter* Enemy = AGBEnemyCharacter::SpawnEnemy(World, FName(TEXT("Orc_Past")), At, FRotator::ZeroRotator, EGBEraPresence::Both);
			ShotgunFlashTarget = Enemy;
			if (!Enemy)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE shotgun weakpoint: could not spawn the test orc"));
				return;
			}
			FGBDamageInfo Info;
			Info.Amount = 1.f;
			Info.Instigator = Player;
			Info.bHeadshot = true;
			UGBCombatLibrary::DealDamage(Enemy, Info);
		});
		Add(SG + 3.25f, TEXT("shotgun weak point: flash is on"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			AGBGun* Gun = ShotgunInHand(Player);
			if (!Gun)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE shotgun weakpoint: no shotgun in hand"));
				return;
			}
			const float Flash = Gun->GetViewModelWeakPointFlash();
			UE_LOG(LogGB, Display, TEXT("SMOKE shotgun weakpoint flash=%.2f"), Flash);
			if (Flash <= 0.5f)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE shotgun weakpoint FAILED: flash=%.2f right after a weak-point hit (expected more than 0.5)"), Flash);
			}
		});
		Add(SG + 4.3f, TEXT("shotgun weak point: flash is off again, then a body hit"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			AGBGun* Gun = ShotgunInHand(Player);
			AGBEnemyCharacter* Enemy = ShotgunFlashTarget.Get();
			if (!Gun || !Enemy)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE shotgun weakpoint: no shotgun in hand or no test orc"));
				return;
			}
			const float After = Gun->GetViewModelWeakPointFlash();
			UE_LOG(LogGB, Display, TEXT("SMOKE shotgun weakpoint flash after=%.2f"), After);
			if (After > 0.001f)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE shotgun weakpoint FAILED: flash=%.2f one second later (expected 0)"), After);
			}
			FGBDamageInfo Info;
			Info.Amount = 1.f;
			Info.Instigator = Player;
			Info.bHeadshot = false;
			UGBCombatLibrary::DealDamage(Enemy, Info);
		});
		Add(SG + 4.4f, TEXT("shotgun body shot: the lens stays dark; half the enchant left"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			AGBGun* Gun = ShotgunInHand(Player);
			if (!Gun)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE shotgun bodyshot: no shotgun in hand"));
				return;
			}
			const float Flash = Gun->GetViewModelWeakPointFlash();
			UE_LOG(LogGB, Display, TEXT("SMOKE shotgun bodyshot flash=%.2f"), Flash);
			if (Flash > 0.001f)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE shotgun bodyshot FAILED: flash=%.2f after a body hit (expected 0)"), Flash);
			}
			if (AGBEnemyCharacter* Enemy = ShotgunFlashTarget.Get())
			{
				Enemy->Destroy();
			}
			ShotgunFlashTarget.Reset();
			if (UGBStatusEffectComponent* Effects = Player->GetStatusEffects())
			{
				for (const FGBActiveStatusEffect& Active : Effects->GetActiveEffects())
				{
					if (Active.EffectId.ToString().StartsWith(TEXT("Enchant_")))
					{
						const FName EnchantId = Active.EffectId;
						Effects->SetTimeRemaining(EnchantId, 0.5f * Active.TotalDuration);
						break;
					}
				}
			}
		});
		Add(SG + 4.7f, TEXT("shotgun half bar: imbue about half, screenshot"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			AGBGun* Gun = ShotgunInHand(Player);
			if (!Gun)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE shotgun half bar: no shotgun in hand"));
				return;
			}
			const float Imbue = Gun->GetViewModelImbueSent();
			UE_LOG(LogGB, Display, TEXT("SMOKE shotgun half bar imbue=%.2f ammo=%d"), Imbue, Gun->GetAmmo().InMagazine);
			CheckEnchantBlink(TEXT("shotgun"), Gun, false, TEXT("at half the enchant"));
			if (FMath::Abs(Imbue - 0.5f) > 0.05f)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE shotgun half bar FAILED: imbue=%.2f (expected about 0.5)"), Imbue);
			}
			Shot(TEXT("15c_shotgun_enchanted"));
		});
		// Reload from 3 shells with 24 in reserve: one shell per third of a second.
		const float SR = SG + 4.9f;
		Add(SR, TEXT("shotgun reload: from 3 shells with 24 in reserve"), [](UWorld* World, AGBPlayerCharacter* Player)
		{
			AGBGun* Gun = ShotgunInHand(Player);
			if (!Gun)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE shotgun reload: no shotgun in hand"));
				return;
			}
			if (UGBStatusEffectComponent* Effects = Player->GetStatusEffects())
			{
				Effects->RemoveAllEffects();
			}
			FGBAmmoState Ammo;
			Ammo.InMagazine = 3;
			Ammo.SpareMagazines = 24;
			Gun->SetAmmo(Ammo);
			ShotgunAmmoBefore = Gun->GetAmmo();
			Gun->Reload();
			ShotgunReloadStartSim = World->GetTimeSeconds();
			ShotgunCycleSeconds = Gun->GetStat(GBStat::ReloadTime, 2.f) / FMath::Max(1, Gun->GetMagazineSize());
			UE_LOG(LogGB, Display, TEXT("SMOKE shotgun reload started: one shell takes %.3f s;"), ShotgunCycleSeconds);
			UE_LOG(LogGB, Display, TEXT("SMOKE shotgun reload started: reloading=%d ammo %d / %d"), Gun->IsReloading() ? 1 : 0, Gun->GetAmmo().InMagazine, Gun->GetAmmo().SpareMagazines);
			if (!Gun->IsReloading() || ShotgunAmmoBefore.InMagazine != 3 || ShotgunAmmoBefore.SpareMagazines != 24)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE shotgun reload FAILED: reloading=%d, ammo %d / %d (expected a reload from 3 with 24 in reserve)"),
					Gun->IsReloading() ? 1 : 0, ShotgunAmmoBefore.InMagazine, ShotgunAmmoBefore.SpareMagazines);
			}
		});
		Add(SR + 0.4f, TEXT("shotgun reload: one shell in (4 and 23), display follows; a shell's cycle pinned mid-way, screenshot"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			AGBGun* Gun = ShotgunInHand(Player);
			if (!Gun)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE shotgun reload: no shotgun in hand"));
				return;
			}
			const FGBAmmoState Ammo = Gun->GetAmmo();
			const FString Text = Gun->GetViewModelDisplayText();
			UE_LOG(LogGB, Display, TEXT("SMOKE shotgun reload after one cycle: ammo %d / %d, display=%s, reloading=%d"), Ammo.InMagazine, Ammo.SpareMagazines, *Text, Gun->IsReloading() ? 1 : 0);
			if (!Gun->IsReloading() || Ammo.InMagazine != 4 || Ammo.SpareMagazines != 23 || Text != FString::FromInt(Ammo.InMagazine))
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE shotgun reload FAILED: %d in the gun and %d in reserve, display '%s' after one shell (expected 4, 23 and the live count)"),
					Ammo.InMagazine, Ammo.SpareMagazines, *Text);
			}
			Gun->SetReloadPreview(0.55f);
			CheckReloadBeat(TEXT("shotgun"), Gun, 0.55f, true);
			Shot(TEXT("15d_shotgun_reload_shell"));
		});
		Add(SR + 0.8f, TEXT("shotgun reload: pin released, the real reload goes on"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			if (AGBGun* Gun = ShotgunInHand(Player))
			{
				Gun->ClearReloadPreview();
			}
		});
		Add(SR + 0.85f, TEXT("shotgun reload: the shells follow the clock, one per cycle"), [](UWorld* World, AGBPlayerCharacter* Player)
		{
			AGBGun* Gun = ShotgunInHand(Player);
			if (!Gun)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE shotgun reload: no shotgun in hand"));
				return;
			}
			const FGBAmmoState Ammo = Gun->GetAmmo();
			UE_LOG(LogGB, Display, TEXT("SMOKE shotgun reload at 0.85 s: ammo %d / %d reloading=%d progress=%.2f"), Ammo.InMagazine, Ammo.SpareMagazines, Gun->IsReloading() ? 1 : 0, Gun->GetReloadProgress());
			// Shells landed by a game time T: one per cycle from 3, up to 6. The state is that of the last frame, so allow 60 ms of lag.
			const double Elapsed = World->GetTimeSeconds() - ShotgunReloadStartSim;
			auto ShellsAt = [](double T) { return FMath::Clamp(3 + FMath::FloorToInt(static_cast<float>(FMath::Max(0.0, T)) / ShotgunCycleSeconds), 3, 6); };
			const int32 Low = ShellsAt(Elapsed - 0.06);
			const int32 High = ShellsAt(Elapsed);
			if (Ammo.InMagazine < Low || Ammo.InMagazine > High || Ammo.InMagazine + Ammo.SpareMagazines != 27 || (High < 6 && !Gun->IsReloading()))
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE shotgun reload FAILED: %d in the gun and %d in reserve after %.2f s (expected %d to %d shells, 27 rounds in all, reloading until 6)"),
					Ammo.InMagazine, Ammo.SpareMagazines, Elapsed, Low, High);
			}
		});
		Add(SR + 1.45f, TEXT("shotgun reload: ends at 6 and 21 after about half of ReloadTime, everything at rest"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			AGBGun* Gun = ShotgunInHand(Player);
			if (!Gun)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE shotgun reload: no shotgun in hand"));
				return;
			}
			const FGBAmmoState Ammo = Gun->GetAmmo();
			const FString Text = Gun->GetViewModelDisplayText();
			UE_LOG(LogGB, Display, TEXT("SMOKE shotgun reload finished: ammo %d / %d display=%s"), Ammo.InMagazine, Ammo.SpareMagazines, *Text);
			if (Ammo.InMagazine != 6 || Ammo.SpareMagazines != 21 || Text != TEXT("6"))
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE shotgun reload FAILED: %d in the gun, %d in reserve, display '%s' at the end (expected 6, 21, 6)"), Ammo.InMagazine, Ammo.SpareMagazines, *Text);
			}
			CheckShotgunAtRest(TEXT("reload finished"), Gun);
		});
		// Fire pressed during a reload from 2 shells.
		const float SF = SR + 1.6f;
		Add(SF, TEXT("shotgun fire during reload: start a reload from 2 shells"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			AGBGun* Gun = ShotgunInHand(Player);
			if (!Gun)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE shotgun fire in reload: no shotgun in hand"));
				return;
			}
			FGBAmmoState Ammo;
			Ammo.InMagazine = 2;
			Ammo.SpareMagazines = 21;
			Gun->SetAmmo(Ammo);
			Gun->Reload();
		});
		Add(SF + 0.5f, TEXT("shotgun fire during reload: fire stops the reload and shoots, nothing lost or created"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			AGBGun* Gun = ShotgunInHand(Player);
			if (!Gun)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE shotgun fire in reload: no shotgun in hand"));
				return;
			}
			const FGBAmmoState Before = Gun->GetAmmo();
			const bool bWasReloading = Gun->IsReloading();
			Gun->StartPrimary();
			const FGBAmmoState After = Gun->GetAmmo();
			UE_LOG(LogGB, Display, TEXT("SMOKE shotgun fire in reload: before %d / %d reloading=%d, after %d / %d reloading=%d, muzzle flash=%d"),
				Before.InMagazine, Before.SpareMagazines, bWasReloading ? 1 : 0, After.InMagazine, After.SpareMagazines, Gun->IsReloading() ? 1 : 0, Gun->IsMuzzleFlashVisible() ? 1 : 0);
			// Two shells and 21 in reserve make 23 rounds; one is fired, so 22 are left; the shells already loaded stay in.
			if (!bWasReloading || Before.InMagazine < 3 || Gun->IsReloading() || After.InMagazine != Before.InMagazine - 1
				|| After.SpareMagazines != Before.SpareMagazines || After.InMagazine + After.SpareMagazines != 22 || !Gun->IsMuzzleFlashVisible())
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE shotgun fire in reload FAILED: %d / %d (reloading=%d) became %d / %d (reloading=%d); expected the reload stopped, one shell fired, 22 rounds in all"),
					Before.InMagazine, Before.SpareMagazines, bWasReloading ? 1 : 0, After.InMagazine, After.SpareMagazines, Gun->IsReloading() ? 1 : 0);
			}
		});
		Add(SF + 0.6f, TEXT("shotgun fire during reload: trigger released"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			if (AGBGun* Gun = ShotgunInHand(Player))
			{
				Gun->StopPrimary();
			}
		});
		// Fire pressed on an EMPTY gun during its reload: it waits for the first shell, then stops the reload and shoots.
		const float SE = SF + 1.8f;
		Add(SE, TEXT("shotgun empty gun: start a reload from 0 and press fire"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			AGBGun* Gun = ShotgunInHand(Player);
			if (!Gun)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE shotgun empty gun: no shotgun in hand"));
				return;
			}
			FGBAmmoState Ammo;
			Ammo.InMagazine = 0;
			Ammo.SpareMagazines = 24;
			Gun->SetAmmo(Ammo);
			Gun->Reload();
			Gun->StartPrimary();
			UE_LOG(LogGB, Display, TEXT("SMOKE shotgun empty gun pressed fire: ammo %d / %d reloading=%d"), Gun->GetAmmo().InMagazine, Gun->GetAmmo().SpareMagazines, Gun->IsReloading() ? 1 : 0);
			if (!Gun->IsReloading() || Gun->GetAmmo().InMagazine != 0)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE shotgun empty gun FAILED: pressing fire on an empty gun during its reload must wait for the first shell (reloading=%d, %d in the gun)"),
					Gun->IsReloading() ? 1 : 0, Gun->GetAmmo().InMagazine);
			}
		});
		Add(SE + 0.5f, TEXT("shotgun empty gun: the first shell landed and was fired"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			AGBGun* Gun = ShotgunInHand(Player);
			if (!Gun)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE shotgun empty gun: no shotgun in hand"));
				return;
			}
			Gun->StopPrimary();
			const FGBAmmoState Ammo = Gun->GetAmmo();
			const float Since = Gun->GetTimeSinceLastAttack();
			UE_LOG(LogGB, Display, TEXT("SMOKE shotgun empty gun after 0.5 s: ammo %d / %d, last shot %.2f s ago, reloading=%d"), Ammo.InMagazine, Ammo.SpareMagazines, Since, Gun->IsReloading() ? 1 : 0);
			// 24 rounds, one landed and was fired: 23 left. The gun is empty again, so it reloads by itself.
			if (Ammo.InMagazine + Ammo.SpareMagazines != 23 || Since > 0.45f || Since < 0.f)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE shotgun empty gun FAILED: %d / %d and the last shot %.2f s ago (expected 23 rounds in all and a shot right after the first shell landed)"),
					Ammo.InMagazine, Ammo.SpareMagazines, Since);
			}
		});
		// A TAP (press and release) on an empty gun during its reload still fires when the first shell lands.
		const float ST = SE + 1.2f;
		Add(ST, TEXT("shotgun tap on an empty gun: start a reload from 0, tap fire"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			AGBGun* Gun = ShotgunInHand(Player);
			if (!Gun)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE shotgun tap: no shotgun in hand"));
				return;
			}
			FGBAmmoState Ammo;
			Ammo.InMagazine = 0;
			Ammo.SpareMagazines = 24;
			Gun->SetAmmo(Ammo);
			Gun->Reload();
			Gun->StartPrimary();
			Gun->StopPrimary();
			UE_LOG(LogGB, Display, TEXT("SMOKE shotgun tap on an empty gun: ammo %d / %d reloading=%d"), Gun->GetAmmo().InMagazine, Gun->GetAmmo().SpareMagazines, Gun->IsReloading() ? 1 : 0);
			if (!Gun->IsReloading() || Gun->GetAmmo().InMagazine != 0)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE shotgun tap FAILED: the tap must wait for the first shell (reloading=%d, %d in the gun)"), Gun->IsReloading() ? 1 : 0, Gun->GetAmmo().InMagazine);
			}
		});
		Add(ST + 0.5f, TEXT("shotgun tap on an empty gun: the shell landed and the tapped shot was fired"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			AGBGun* Gun = ShotgunInHand(Player);
			if (!Gun)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE shotgun tap: no shotgun in hand"));
				return;
			}
			const FGBAmmoState Ammo = Gun->GetAmmo();
			const float Since = Gun->GetTimeSinceLastAttack();
			UE_LOG(LogGB, Display, TEXT("SMOKE shotgun tap after 0.5 s: ammo %d / %d, last shot %.2f s ago"), Ammo.InMagazine, Ammo.SpareMagazines, Since);
			if (Ammo.InMagazine + Ammo.SpareMagazines != 23 || Since > 0.45f || Since < 0.f)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE shotgun tap FAILED: %d / %d and the last shot %.2f s ago (expected 23 rounds in all and a shot right after the first shell landed)"), Ammo.InMagazine, Ammo.SpareMagazines, Since);
			}
		});
		// Fire pressed while the fire cooldown still runs, during a reload: the reload stops at once.
		const float SP = ST + 1.2f;
		Add(SP, TEXT("shotgun cooldown press: shoot, start a reload, press fire inside the cooldown"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			AGBGun* Gun = ShotgunInHand(Player);
			if (!Gun)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE shotgun cooldown press: no shotgun in hand"));
				return;
			}
			FGBAmmoState Ammo;
			Ammo.InMagazine = 3;
			Ammo.SpareMagazines = 24;
			Gun->SetAmmo(Ammo);
			Gun->StartPrimary();
			Gun->StopPrimary();
			Gun->Reload();
			const bool bReloadingBefore = Gun->IsReloading();
			Gun->StartPrimary();
			const bool bReloadingAfter = Gun->IsReloading();
			Gun->StopPrimary(); // release inside the cooldown: no second shot follows
			UE_LOG(LogGB, Display, TEXT("SMOKE shotgun cooldown press: reloading before the press=%d, after=%d, ammo %d / %d"), bReloadingBefore ? 1 : 0, bReloadingAfter ? 1 : 0,
				Gun->GetAmmo().InMagazine, Gun->GetAmmo().SpareMagazines);
			if (!bReloadingBefore || bReloadingAfter || Gun->GetAmmo().InMagazine != 2 || Gun->GetAmmo().SpareMagazines != 24)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE shotgun cooldown press FAILED: reloading %d then %d, ammo %d / %d (expected the press to stop the reload at once, 2 / 24)"),
					bReloadingBefore ? 1 : 0, bReloadingAfter ? 1 : 0, Gun->GetAmmo().InMagazine, Gun->GetAmmo().SpareMagazines);
			}
		});
		// A reload cancelled by switching to the staff and back keeps the shells already loaded.
		const float SC = SP + 1.0f;
		Add(SC, TEXT("shotgun cancel: start a reload from 1 shell"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			AGBGun* Gun = ShotgunInHand(Player);
			if (!Gun)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE shotgun cancel: no shotgun in hand"));
				return;
			}
			FGBAmmoState Ammo;
			Ammo.InMagazine = 1;
			Ammo.SpareMagazines = 23;
			Gun->SetAmmo(Ammo);
			Gun->Reload();
		});
		Add(SC + 0.5f, TEXT("shotgun cancel: switched to the staff and back after a shell landed"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			AGBGun* Gun = ShotgunInHand(Player);
			if (!Gun)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE shotgun cancel: no shotgun in hand"));
				return;
			}
			ShotgunAmmoBefore = Gun->GetAmmo();
			UE_LOG(LogGB, Display, TEXT("SMOKE shotgun cancel: switching away with %d / %d"), ShotgunAmmoBefore.InMagazine, ShotgunAmmoBefore.SpareMagazines);
			Player->GetEquipment()->SwitchTo(EGBHeldSlot::Staff);
			Player->GetEquipment()->SwitchTo(EGBHeldSlot::Gun);
		});
		Add(SC + 0.6f, TEXT("shotgun cancel: loaded shells kept, shell and gun at rest"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			AGBGun* Gun = ShotgunInHand(Player);
			if (!Gun)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE shotgun cancel: no shotgun in hand"));
				return;
			}
			const FGBAmmoState Ammo = Gun->GetAmmo();
			UE_LOG(LogGB, Display, TEXT("SMOKE shotgun cancel: back with %d / %d (was %d / %d)"), Ammo.InMagazine, Ammo.SpareMagazines, ShotgunAmmoBefore.InMagazine, ShotgunAmmoBefore.SpareMagazines);
			if (ShotgunAmmoBefore.InMagazine < 2 || Ammo.InMagazine != ShotgunAmmoBefore.InMagazine || Ammo.SpareMagazines != ShotgunAmmoBefore.SpareMagazines)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE shotgun cancel FAILED: %d / %d after the switch, was %d / %d (expected the shells already loaded to stay, at least 2)"),
					Ammo.InMagazine, Ammo.SpareMagazines, ShotgunAmmoBefore.InMagazine, ShotgunAmmoBefore.SpareMagazines);
			}
		});
		// Extended Tube changes the tube size while the game runs (Sol 6.1 review): buy it, fill the bigger tube, start a
		// reload, then reset the skills mid-reload. The gun must drop back to its own size at once, with the same result
		// a weapon swap would give, and the reload planned for the old size must stop.
		Add(SC + 0.7f, TEXT("shotgun tube perk: Extended Tube 3, tube refilled to 9"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			AGBGun* Gun = ShotgunInHand(Player);
			UGBSkillTreeComponent* Skills = Player->GetSkillTree();
			if (!Gun || !Skills)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE shotgun tube perk: no shotgun in hand"));
				return;
			}
			ShotgunTubePointsBefore = Skills->GetPoints();
			Skills->AddPoints(5);
			Skills->Unlock(FName(TEXT("Weapon_Damage")));
			Skills->Unlock(FName(TEXT("Weapon_Shotgun_FireRate")));
			for (int32 i = 0; i < 3; ++i)
			{
				Skills->Unlock(FName(TEXT("Weapon_Shotgun_Shells")));
			}
			Gun->RefillAmmo();
			const FGBAmmoState Ammo = Gun->GetAmmo();
			UE_LOG(LogGB, Display, TEXT("SMOKE shotgun tube perk: size %d, ammo %d / %d"), Gun->GetMagazineSize(), Ammo.InMagazine, Ammo.SpareMagazines);
			if (Gun->GetMagazineSize() != 9 || Ammo.InMagazine != 9 || Ammo.SpareMagazines != 36)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE shotgun tube perk FAILED: size %d, ammo %d / %d (expected 9, 9 / 36)"), Gun->GetMagazineSize(), Ammo.InMagazine, Ammo.SpareMagazines);
			}
			// Two shells out, then a reload toward the 9-shell tube.
			FGBAmmoState Partial = Ammo;
			Partial.InMagazine = 7;
			Gun->SetAmmo(Partial);
			Gun->Reload();
		});
		Add(SC + 0.8f, TEXT("shotgun tube perk: reset the skills in the middle of the reload"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			AGBGun* Gun = ShotgunInHand(Player);
			UGBSkillTreeComponent* Skills = Player->GetSkillTree();
			if (!Gun || !Skills)
			{
				return;
			}
			UE_LOG(LogGB, Display, TEXT("SMOKE shotgun tube perk: reloading=%d ammo %d / %d before the reset"), Gun->IsReloading() ? 1 : 0, Gun->GetAmmo().InMagazine, Gun->GetAmmo().SpareMagazines);
			if (!Gun->IsReloading())
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE shotgun tube perk FAILED: not reloading before the reset, so the mid-reload case is not tested"));
			}
			Skills->ResetAll();
			Skills->AddPoints(-5);
		});
		Add(SC + 0.95f, TEXT("shotgun tube perk: back to 6 shells, reload stopped, reserve capped"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			AGBGun* Gun = ShotgunInHand(Player);
			UGBSkillTreeComponent* Skills = Player->GetSkillTree();
			if (!Gun || !Skills)
			{
				return;
			}
			const FGBAmmoState Ammo = Gun->GetAmmo();
			UE_LOG(LogGB, Display, TEXT("SMOKE shotgun tube perk: after the reset size %d, ammo %d / %d, reloading=%d, points %d (were %d)"),
				Gun->GetMagazineSize(), Ammo.InMagazine, Ammo.SpareMagazines, Gun->IsReloading() ? 1 : 0, Skills->GetPoints(), ShotgunTubePointsBefore);
			if (Gun->GetMagazineSize() != 6 || Ammo.InMagazine != 6 || Ammo.SpareMagazines != 24 || Gun->IsReloading() || Skills->GetPoints() != ShotgunTubePointsBefore)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE shotgun tube perk FAILED: size %d, ammo %d / %d, reloading=%d, points %d (expected 6, 6 / 24, no reload, %d points)"),
					Gun->GetMagazineSize(), Ammo.InMagazine, Ammo.SpareMagazines, Gun->IsReloading() ? 1 : 0, Skills->GetPoints(), ShotgunTubePointsBefore);
			}
		});
		Add(SC + 1.2f, TEXT("shotgun done: pump, shell and root at rest; staff back in hand; clock back"), [SC, SG](UWorld*, AGBPlayerCharacter* Player)
		{
			if (AGBGun* Gun = ShotgunInHand(Player))
			{
				CheckShotgunAtRest(TEXT("end of the block"), Gun);
			}
			Player->GetEquipment()->SwitchTo(EGBHeldSlot::Staff);
			// The next step (the Entity) was timed 0.1 s after this block started: continue the clock from there.
			StartSeconds += (SC + 1.2f) - (SG + 0.1f);
		});
		Add(61.8f + S, TEXT("entity: spawn where the player can see it, attacks off"), [](UWorld* World, AGBPlayerCharacter* Player)
		{
			Player->GetAttributes()->SetInvulnerable(true);
			const FVector PlayerLoc = Player->GetActorLocation();
			const FVector EyeLoc = Player->GetAimOrigin();
			EGBEraPresence Presence = EGBEraPresence::Both;
			if (UGBEraSubsystem* Era = UGBEraSubsystem::Get(World))
			{
				Presence = Era->GetCurrentEra() == EGBEra::Past ? EGBEraPresence::PastOnly : EGBEraPresence::FutureOnly;
			}
			// The player may stand anywhere (an earlier step's grenade can throw them); try yaw
			// offsets until the line from the eyes to the spawn is clear, so no pillar hides the Entity.
			AGBEntity* Entity = nullptr;
			float PickedYaw = 0.f;
			float PickedDist = 0.f;
			for (float Dist : { 800.f, 500.f })
			{
				for (int32 K = 0; K < 8 && !Entity; ++K)
				{
					const float Yaw = K * 45.f;
					const float YawRad = FMath::DegreesToRadians(Yaw);
					const FVector At = PlayerLoc + FVector(FMath::Cos(YawRad), FMath::Sin(YawRad), 0.f) * Dist;
					FCollisionQueryParams LOSParams(SCENE_QUERY_STAT(GBSmokeEntityLOS), false, Player);
					FHitResult LOSHit;
					if (World->LineTraceSingleByChannel(LOSHit, EyeLoc, At + FVector(0.f, 0.f, 100.f), ECC_Visibility, LOSParams))
					{
						continue;
					}
					Entity = Cast<AGBEntity>(AGBEnemyCharacter::SpawnEnemy(World, FName(TEXT("Entity_Phase1")), At, FRotator::ZeroRotator, Presence));
					if (Entity)
					{
						PickedYaw = Yaw;
						PickedDist = Dist;
					}
				}
				if (Entity)
				{
					break;
				}
			}
			if (Entity)
			{
				Entity->SetAttackSuppressed(true);
				SmokeEntity = Entity;
			}
			else
			{
				SmokeEntity.Reset();
				UE_LOG(LogGB, Error, TEXT("SMOKE entity: could not spawn Entity_Phase1 with a clear view"));
			}
			UE_LOG(LogGB, Display, TEXT("SMOKE entity spawned=%d yaw=%.0f dist=%.0f"), Entity ? 1 : 0, PickedYaw, PickedDist);
		});
		Add(62.3f + S, TEXT("entity: record the tentacle tips"), [](UWorld* World, AGBPlayerCharacter*)
		{
			SmokeEntityTipsA.Reset();
			MarkSim(World);
			if (AGBEntity* Entity = SmokeEntity.Get())
			{
				if (UGBTentacleRigComponent* Rig = Entity->GetTentacleRig())
				{
					for (int32 Chain = 0; Chain < Rig->GetChainCount(); ++Chain)
					{
						SmokeEntityTipsA.Add(Rig->GetChainTipPosition(Chain));
					}
				}
			}
		});
		Add(62.8f + S, TEXT("entity: idle motion between two moments 0.5 s apart"), [](UWorld*, AGBPlayerCharacter*)
		{
			float Moved = 0.f;
			int32 Bones = 0;
			int32 Chains = 0;
			if (AGBEntity* Entity = SmokeEntity.Get())
			{
				if (UGBTentacleRigComponent* Rig = Entity->GetTentacleRig())
				{
					Chains = Rig->GetChainCount();
					Bones = Rig->GetBoneCount();
					for (int32 Chain = 0; Chain < Chains && Chain < SmokeEntityTipsA.Num(); ++Chain)
					{
						Moved = FMath::Max(Moved, FVector::Dist(SmokeEntityTipsA[Chain], Rig->GetChainTipPosition(Chain)));
					}
				}
			}
			UE_LOG(LogGB, Display, TEXT("SMOKE entity bones=%d chains=%d tip_moved=%.1f"), Bones, Chains, Moved);
			if (Chains == 0 || Bones == 0)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE entity: no tentacle chains (bones=%d chains=%d)"), Bones, Chains);
			}
			else if (Moved <= 3.f)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE entity FAILED: tips moved %.1f cm in 0.5 s (expected more than 3)"), Moved);
			}
		}, 0.5f);
		Add(62.9f + S, TEXT("entity: reach 60 cm in front of the tip of chain 0"), [](UWorld* World, AGBPlayerCharacter*)
		{
			MarkSim(World);
			if (AGBEntity* Entity = SmokeEntity.Get())
			{
				Entity->SetHoldReach(true);
				if (UGBTentacleRigComponent* Rig = Entity->GetTentacleRig())
				{
					if (Rig->GetChainCount() > 0)
					{
						const FVector Tip = Rig->GetChainTipWorldPosition(0);
						FVector Out = Tip - Entity->GetActorLocation();
						Out.Z = 0.f;
						if (Out.IsNearlyZero())
						{
							Out = Entity->GetActorForwardVector();
						}
						SmokeEntityReachTarget = Tip + Out.GetSafeNormal() * 60.f;
						Rig->SetReachChain(0);
						Rig->SetReachTarget(SmokeEntityReachTarget, 1.f);
					}
				}
			}
		});
		Add(63.5f + S, TEXT("entity: reach error, then release"), [](UWorld*, AGBPlayerCharacter*)
		{
			float Error = -1.f;
			if (AGBEntity* Entity = SmokeEntity.Get())
			{
				if (UGBTentacleRigComponent* Rig = Entity->GetTentacleRig())
				{
					if (Rig->GetChainCount() > 0)
					{
						Error = FVector::Dist(Rig->GetChainTipWorldPosition(0), SmokeEntityReachTarget);
					}
					Rig->ClearReach();
				}
				Entity->SetHoldReach(false);
			}
			UE_LOG(LogGB, Display, TEXT("SMOKE entity reach error=%.1f"), Error);
			if (!(Error >= 0.f && Error < 15.f))
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE entity reach FAILED: error %.1f cm (expected less than 15)"), Error);
			}
		}, 0.8f);
		Add(63.55f + S, TEXT("entity: start measuring the shared stroke"), [](UWorld* World, AGBPlayerCharacter*)
		{
			MarkSim(World);
			if (const AGBEntity* Entity = SmokeEntity.Get())
			{
				// The body holds still meanwhile (no altitude, yaw or pitch change), so only the tentacles move.
				const_cast<AGBEntity*>(Entity)->SetHoldReach(true);
				if (UGBTentacleRigComponent* Rig = Entity->GetTentacleRig())
				{
					Rig->StartSyncMeasure();
				}
			}
		});
		Add(63.6f + S, TEXT("entity: tentacle sync, 1 simulated second"), [](UWorld*, AGBPlayerCharacter*)
		{
			float Sync = -1.f;
			if (const AGBEntity* Entity = SmokeEntity.Get())
			{
				if (const UGBTentacleRigComponent* Rig = Entity->GetTentacleRig())
				{
					Sync = Rig->GetSyncCorrelation();
				}
				const_cast<AGBEntity*>(Entity)->SetHoldReach(false);
			}
			UE_LOG(LogGB, Display, TEXT("SMOKE entity sync=%.2f"), Sync);
			if (!(Sync > 0.6f))
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE entity sync FAILED: %.2f (expected over 0.6): the tentacles do not follow one shared stroke"), Sync);
			}
		}, 1.0f);
		Add(63.8f + S, TEXT("entity: weak point and hit checks, frame a front three-quarter view"), [](UWorld* World, AGBPlayerCharacter* Player)
		{
			MarkSim(World);
			int32 Eye = 0;
			int32 Shell = 0;
			int32 HitEye = 0;
			int32 HitShell = 0;
			int32 HitBelow = 0;
			if (AGBEntity* Entity = SmokeEntity.Get())
			{
				Eye = Entity->IsHeadshotLocation(Entity->GetEyeCentreWorld()) ? 1 : 0;
				Shell = Entity->IsHeadshotLocation(Entity->GetShellTopWorld()) ? 1 : 0;
				UE_LOG(LogGB, Display, TEXT("SMOKE entity diag %s"), *Entity->DescribeVisual());
				const float BoundsHeight = Entity->GetVisualBoundsHeight();
				UE_LOG(LogGB, Display, TEXT("SMOKE entity scale=%.1f height_cm=%.0f"), Entity->GetVisualScale(), BoundsHeight);
				if (BoundsHeight < 100.f * Entity->GetVisualScale())
				{
					UE_LOG(LogGB, Error, TEXT("SMOKE entity scale FAILED: the drawn parts are only %.0f cm tall at scale %.1f"), BoundsHeight, Entity->GetVisualScale());
				}

				// Line traces on the weapon channel (the one guns use): what is drawn must be what is hit.
				FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(GBSmokeEntityHit), false, Player);
				auto Trace = [&](const FVector& From, const FVector& To, bool& bHitEntity, bool& bWeak)
				{
					FHitResult Hit;
					bHitEntity = World->LineTraceSingleByChannel(Hit, From, To, ECC_Camera, TraceParams) && Hit.GetActor() == Entity;
					bWeak = bHitEntity && Entity->IsHeadshotLocation(Hit.ImpactPoint);
				};
				bool bHitEntity = false;
				bool bWeak = false;
				const FVector EyePoint = Entity->GetEyeCentreWorld();
				Trace(EyePoint + Entity->GetActorForwardVector() * 300.f, EyePoint, bHitEntity, bWeak);
				HitEye = (bHitEntity && bWeak) ? 1 : 0;
				const FVector TopPoint = Entity->GetShellTopWorld();
				Trace(TopPoint + FVector(0.f, 0.f, 150.f), TopPoint, bHitEntity, bWeak);
				HitShell = (bHitEntity && !bWeak) ? 1 : 0;
				const FVector Below = Entity->GetShellCentreWorld() - FVector(0.f, 0.f, Entity->GetShellRadiusWorld() + 150.f);
				const FVector Right = Entity->GetActorRightVector();
				Trace(Below + Right * 300.f, Below - Right * 300.f, bHitEntity, bWeak);
				HitBelow = bHitEntity ? 1 : 0;

				// Front three-quarter view from 7 m, aimed a little below the shell so the tentacles are in frame.
				// The first free spot (inside the arena, nothing between it and the Entity) from the front three-quarter views.
				FVector PlayerSpot = Entity->GetActorLocation() + Entity->GetActorForwardVector().RotateAngleAxis(50.f, FVector::UpVector) * 1300.f;
				FCollisionQueryParams SpotParams(SCENE_QUERY_STAT(GBSmokeEntitySpot), false, Player);
				SpotParams.AddIgnoredActor(Entity);
				bool bSpotFound = false;
				for (float SpotDistance : { 1100.f, 900.f, 700.f })
				{
					for (float SpotAngle : { 90.f, -90.f, 75.f, -50.f })
					{
						const FVector Candidate = Entity->GetActorLocation() + Entity->GetActorForwardVector().RotateAngleAxis(SpotAngle, FVector::UpVector) * SpotDistance;
						const FVector ViewEye = FVector(Candidate.X, Candidate.Y, Player->GetActorLocation().Z + 70.f);
						FHitResult Floor;
						const bool bFloor = World->LineTraceSingleByChannel(Floor, ViewEye, ViewEye - FVector(0.f, 0.f, 400.f), ECC_Visibility, SpotParams);
						if (bFloor && !World->LineTraceTestByChannel(Entity->GetActorLocation() + FVector(0.f, 0.f, 150.f), ViewEye, ECC_Visibility, SpotParams)
							&& !World->LineTraceTestByChannel(ViewEye, Entity->GetShellCentreWorld(), ECC_Visibility, SpotParams))
						{
							PlayerSpot = Candidate;
							bSpotFound = true;
							break;
						}
					}
					if (bSpotFound)
					{
						break;
					}
				}
				Player->SetActorLocation(FVector(PlayerSpot.X, PlayerSpot.Y, Player->GetActorLocation().Z),
					false, nullptr, ETeleportType::TeleportPhysics);
				UE_LOG(LogGB, Display, TEXT("SMOKE entity view spot_found=%d player=%s entity=%s shell=%s"), bSpotFound ? 1 : 0, *Player->GetActorLocation().ToCompactString(), *Entity->GetActorLocation().ToCompactString(), *Entity->GetShellCentreWorld().ToCompactString());
				if (AGBPlayerController* PC = Cast<AGBPlayerController>(Player->GetController()))
				{
					// The camera manager still holds last frame's position: aim from where the camera will be.
					const FVector CameraLoc = Player->GetActorLocation() + FVector(0.f, 0.f, 70.f);
					const FVector Middle = Entity->GetShellCentreWorld() - FVector(0.f, 0.f, 170.f);
					PC->SetControlRotation((Middle - CameraLoc).Rotation());
				}
			}
			UE_LOG(LogGB, Display, TEXT("SMOKE entity weakpoint eye=%d shell=%d"), Eye, Shell);
			if (Eye != 1 || Shell != 0)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE entity weakpoint FAILED: eye=%d (expected 1) shell=%d (expected 0)"), Eye, Shell);
			}
			UE_LOG(LogGB, Display, TEXT("SMOKE entity hit eye=%d shell=%d below=%d"), HitEye, HitShell, HitBelow);
			if (HitEye != 1 || HitShell != 1 || HitBelow != 0)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE entity hit FAILED: eye=%d (expected 1) shell=%d (expected 1) below=%d (expected 0)"), HitEye, HitShell, HitBelow);
			}
		});
		Add(64.4f + S, TEXT("entity: turn test, the player jumps right behind it"), [](UWorld* World, AGBPlayerCharacter* Player)
		{
			if (AGBEntity* Entity = SmokeEntity.Get())
			{
				const FVector Behind = Entity->GetActorLocation() - Entity->GetActorForwardVector() * 500.f;
				Player->SetActorLocation(FVector(Behind.X, Behind.Y, Player->GetActorLocation().Z), false, nullptr, ETeleportType::TeleportPhysics);
				Entity->ResetMotionStats();
				SmokeEntityTurnStartError = TurnErrorDegrees(Entity, Player);
				MarkSim(World);
			}
		});
		Add(64.9f + S, TEXT("entity: turn rate over 0.6 simulated seconds"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			float MaxRate = -1.f;
			float Progress = -1.f;
			if (const AGBEntity* Entity = SmokeEntity.Get())
			{
				// Both numbers come from the actor itself: the largest yaw change per second it really made, and how far it turned towards the player.
				MaxRate = Entity->GetPeakYawRate();
				Progress = SmokeEntityTurnStartError - TurnErrorDegrees(Entity, Player);
			}
			UE_LOG(LogGB, Display, TEXT("SMOKE entity turn max_rate=%.0f"), MaxRate);
			UE_LOG(LogGB, Display, TEXT("SMOKE entity turn progress=%.0f"), Progress);
			if (!(MaxRate >= 0.f && MaxRate <= 120.f))
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE entity turn FAILED: peak %.0f deg/s (expected at most 120)"), MaxRate);
			}
			if (!(Progress >= 20.f))
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE entity turn FAILED: it turned only %.0f degrees towards the player (expected at least 20)"), Progress);
			}
		}, 0.6f);
		Add(65.0f + S, TEXT("entity: strafe, the player stands at its side"), [](UWorld* World, AGBPlayerCharacter* Player)
		{
			MarkSim(World);
			if (AGBEntity* Entity = SmokeEntity.Get())
			{
				Entity->ResetMotionStats();
				Entity->ForceStrafe(2.4f);
				const FVector Side = Entity->GetActorForwardVector().RotateAngleAxis(70.f, FVector::UpVector);
				const FVector PlayerSpot = Entity->GetActorLocation() + Side * 1100.f;
				Player->SetActorLocation(FVector(PlayerSpot.X, PlayerSpot.Y, Player->GetActorLocation().Z),
					false, nullptr, ETeleportType::TeleportPhysics);
				if (AGBPlayerController* PC = Cast<AGBPlayerController>(Player->GetController()))
				{
					// The camera manager still holds last frame's position: aim from where the camera will be.
					const FVector CameraLoc = Player->GetActorLocation() + FVector(0.f, 0.f, 70.f);
					const FVector Middle = Entity->GetShellCentreWorld() - FVector(0.f, 0.f, 170.f);
					PC->SetControlRotation((Middle - CameraLoc).Rotation());
				}
			}
		});
		Add(65.3f + S, TEXT("entity: screenshot, three-quarter view of the whole Entity while it strafes"), [](UWorld*, AGBPlayerCharacter*)
		{
			Shot(TEXT("15a_entity"));
		}, 0.5f);
		Add(65.5f + S, TEXT("entity: fire one energy bolt past the player"), [](UWorld* World, AGBPlayerCharacter* Player)
		{
			MarkSim(World);
			int32 HasLight = 0;
			float Length = 0.f;
			if (AGBEntity* Entity = SmokeEntity.Get())
			{
				const FVector ToPlayer = Player->GetActorLocation() - Entity->GetShellCentreWorld();
				// Aimed 12 degrees to one side, slow, so it crosses the frame and is still in flight for the screenshot.
				const FVector Direction = ToPlayer.GetSafeNormal().RotateAngleAxis(12.f, FVector::UpVector);
				if (const AGBProjectile* Bolt = Entity->FireDebugBolt(Direction, 900.f))
				{
					HasLight = Bolt->HasBoltLight() ? 1 : 0;
					Length = Bolt->GetBoltLength();
				}
			}
			UE_LOG(LogGB, Display, TEXT("SMOKE entity bolt light=%d length=%.0f"), HasLight, Length);
			if (HasLight != 1 || Length < 50.f)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE entity bolt FAILED: light=%d (expected 1) length=%.0f (expected at least 50)"), HasLight, Length);
			}
		});
		Add(66.0f + S, TEXT("entity: screenshot from the side during the strafe"), [](UWorld*, AGBPlayerCharacter*)
		{
			Shot(TEXT("15b_entity_side"));
		}, 0.6f);
		Add(67.5f + S, TEXT("entity: flight numbers over the strafe window"), [](UWorld*, AGBPlayerCharacter*)
		{
			float ZRange = -1.f;
			float RollMax = -1.f;
			if (const AGBEntity* Entity = SmokeEntity.Get())
			{
				ZRange = Entity->GetShellZRange();
				RollMax = Entity->GetPeakRoll();
			}
			UE_LOG(LogGB, Display, TEXT("SMOKE entity flight z_range=%.0f roll_max=%.1f"), ZRange, RollMax);
			if (!(ZRange > 20.f) || !(RollMax > 3.f))
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE entity flight FAILED: z_range=%.0f (expected over 20) roll_max=%.1f (expected over 3)"), ZRange, RollMax);
			}
		}, 2.2f);
		Add(67.7f + S, TEXT("entity: destroy it, so the smoke steps after it are not affected"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			if (AGBEntity* Entity = SmokeEntity.Get())
			{
				Entity->Destroy();
			}
			SmokeEntity.Reset();
			Player->GetAttributes()->SetInvulnerable(false);
		});
		// GBPixelVfx plugin: one baked Niagara Fluids clip in the game's own level and lighting. The game module does not depend
		// on the plugin: it goes through the plugin's console command, and both steps do nothing when the plugin is off.
		Add(68.3f + S, TEXT("pixelvfx: spawn the baked fire on the ground in front of the player"), [](UWorld* World, AGBPlayerCharacter*)
		{
			MarkSim(World);
			if (!IConsoleManager::Get().FindConsoleObject(TEXT("pvfx.Spawn")))
			{
				UE_LOG(LogGB, Display, TEXT("SMOKE pixelvfx skipped: the GBPixelVfx plugin is not enabled"));
				return;
			}
			if (GEngine)
			{
				GEngine->Exec(World, TEXT("pvfx.Spawn FluidFire 0.6 700"));
			}
		});
		Add(68.7f + S, TEXT("pixelvfx: screenshot of the baked fire lighting the room"), [](UWorld* World, AGBPlayerCharacter*)
		{
			if (!IConsoleManager::Get().FindConsoleObject(TEXT("pvfx.Spawn")))
			{
				return;
			}
			int32 Clips = 0;
			for (TActorIterator<AActor> It(World); It; ++It)
			{
				if (It->GetClass()->GetName() == TEXT("GBPixelVfxActor"))
				{
					++Clips;
				}
			}
			UE_LOG(LogGB, Display, TEXT("SMOKE pixelvfx clips=%d"), Clips);
			if (Clips != 1)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE pixelvfx FAILED: %d baked clips in the world (expected 1: is Content/Data/Vfx/FluidFire there?)"), Clips);
			}
			Shot(TEXT("16_pixelvfx_fire"));
		}, 0.35f);
		// The Fireball carries the baked fire clip in flight and plays the baked explosion where it lands (Spells.json:
		// Strings FlightVfx / ImpactVfx). Aimed at the floor a few metres ahead so it lands inside this block.
		// Earlier steps change the loadout: the block equips the Fireball in slot 0 and puts the previous spell back at the end.
		static FName PixelVfxPreviousSlot0;
		// Set by the Fireball itself when it lands: the impact screenshot is taken on that frame, not on a guessed time.
		static bool bPixelVfxImpactSeen = false;
		Add(68.72f + S, TEXT("pixelvfx: aim at the floor a few metres ahead"), [](UWorld* World, AGBPlayerCharacter* Player)
		{
			bPixelVfxImpactSeen = false;
			PixelVfxPreviousSlot0 = Player->GetSpellbook()->GetSlotSpell(0);
			Player->GetSpellbook()->EquipSpell(0, FName(TEXT("Fireball")));
			// The ground fire has had its screenshot: remove it, or it stands right behind the Fireball in the next two.
			for (TActorIterator<AActor> It(World); It; ++It)
			{
				if (It->GetClass()->GetName() == TEXT("GBPixelVfxActor"))
				{
					It->Destroy();
				}
			}
			SetQTEEnabled(false);
			Player->GetAttributes()->RestoreAll();
			if (AGBPlayerController* PC = Cast<AGBPlayerController>(Player->GetController()))
			{
				PC->SetControlRotation(FRotator(-15.f, PC->GetControlRotation().Yaw, 0.f));
			}
			MarkSim(World);
		});
		Add(68.8f + S, TEXT("pixelvfx: cast the Fireball"), [](UWorld* World, AGBPlayerCharacter* Player)
		{
			const bool bOk = Player->GetSpellbook()->TryCastSlot(0);
			MarkSim(World);
			UE_LOG(LogGB, Display, TEXT("SMOKE pixelvfx fireball cast ok=%d spell=%s"), bOk ? 1 : 0, *Player->GetSpellbook()->GetSlotSpell(0).ToString());
		}, 0.03f);
		Add(68.9f + S, TEXT("pixelvfx: screenshot of the Fireball in flight"), [](UWorld* World, AGBPlayerCharacter*)
		{
			int32 Projectiles = 0;
			int32 WithClip = 0;
			for (TActorIterator<AGBProjectile> It(World); It; ++It)
			{
				++Projectiles;
				WithClip += It->HasFlightVfx() ? 1 : 0;
				// The shockwave and its flash are at their strongest on the frame the Fireball lands.
				It->OnImpact.AddLambda([](AGBProjectile*, AActor*, const FVector&)
				{
					bPixelVfxImpactSeen = true;
					Shot(TEXT("18_pixelvfx_fireball_impact"));
				});
			}
			UE_LOG(LogGB, Display, TEXT("SMOKE pixelvfx fireball flight projectiles=%d with_clip=%d"), Projectiles, WithClip);
			if (WithClip < 1)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE pixelvfx fireball FAILED: no projectile in flight carries the baked clip (projectiles=%d)"), Projectiles);
			}
			Shot(TEXT("17_pixelvfx_fireball_flight"));
		}, 0.07f);
		// Owner (2026-11-13): no baked explosion on the Fireball's impact, the shockwave alone. The screenshot was taken by
		// the Fireball when it landed (see the flight step); this step checks what is left afterwards.
		Add(69.2f + S, TEXT("pixelvfx: after the Fireball's impact, shockwave only"), [](UWorld* World, AGBPlayerCharacter*)
		{
			int32 Clips = 0;
			int32 Projectiles = 0;
			for (TActorIterator<AActor> It(World); It; ++It)
			{
				Clips += It->GetClass()->GetName() == TEXT("GBPixelVfxActor") ? 1 : 0;
			}
			for (TActorIterator<AGBProjectile> It(World); It; ++It)
			{
				++Projectiles;
			}
			UE_LOG(LogGB, Display, TEXT("SMOKE pixelvfx fireball impact clips=%d projectiles=%d"), Clips, Projectiles);
			if (Clips != 0 || Projectiles != 0)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE pixelvfx fireball FAILED: after the impact there are %d baked clips (expected 0: no baked explosion) and %d projectiles (expected 0: it landed, and its flame went with it)"), Clips, Projectiles);
			}
			if (!bPixelVfxImpactSeen)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE pixelvfx fireball FAILED: the Fireball never reported its impact, so there is no impact screenshot"));
			}
		}, 0.5f);
		Add(69.3f + S, TEXT("pixelvfx: remove the clips and look ahead again, so the steps after this block are not affected"), [](UWorld* World, AGBPlayerCharacter* Player)
		{
			for (TActorIterator<AActor> It(World); It; ++It)
			{
				if (It->GetClass()->GetName() == TEXT("GBPixelVfxActor"))
				{
					It->Destroy();
				}
			}
			if (AGBPlayerController* PC = Cast<AGBPlayerController>(Player->GetController()))
			{
				PC->SetControlRotation(FRotator(0.f, PC->GetControlRotation().Yaw, 0.f));
			}
			if (!PixelVfxPreviousSlot0.IsNone() && PixelVfxPreviousSlot0 != FName(TEXT("Fireball")))
			{
				Player->GetSpellbook()->EquipSpell(0, PixelVfxPreviousSlot0);
			}
		}, 0.7f);
		// ---- UI block (gb.Smoke ui): every HUD state and menu in both skins, one screenshot each. No checks: these are
		// for people (and the architect) to look at. The RPG skin in the past with the staff, then the cyberpunk skin in
		// the future with the pistol.
		const float U = 69.4f + S;
		auto Menu = [](AGBPlayerCharacter* Player, EGBMenu Which)
		{
			if (AGBPlayerController* PC = Cast<AGBPlayerController>(Player->GetController()))
			{
				PC->SetMenu(Which);
			}
		};
		auto Cursor = [](UWorld* World, int32 Page, int32 Row, FName Node, bool bChoosing)
		{
			if (AGBHUD* Hud = AGBHUD::Get(World))
			{
				Hud->DebugSetMenuCursor(Page, Row, Node, bChoosing);
			}
		};
		Add(U + 0.0f, TEXT("ui: setup, past, staff, bag, skill points, one cooldown"), [Menu](UWorld* World, AGBPlayerCharacter* Player)
		{
			Menu(Player, EGBMenu::None);
			SetQTEEnabled(false);
			Player->GetEquipment()->SwitchTo(EGBHeldSlot::Staff);
			Player->GetAttributes()->RestoreAll();
			UGBInventoryComponent* Inventory = Player->GetInventory();
			for (const TCHAR* Item : { TEXT("HealthPotion"), TEXT("ManaPotion"), TEXT("Gem_Fire"), TEXT("SwordHilt"), TEXT("Armor_Chronoweave"), TEXT("ElectricGrenade") })
			{
				Inventory->AddItem(Item, 2);
			}
			for (const TCHAR* Spell : { TEXT("Flamethrower"), TEXT("LightningRune"), TEXT("IceFloor"), TEXT("IceWall") })
			{
				if (!Player->GetSpellbook()->KnowsSpell(Spell))
				{
					Player->GetSpellbook()->LearnSpell(Spell);
				}
			}
			Player->GetSkillTree()->AddPoints(6);
			if (const UGBDataSubsystem* Data = UGBDataSubsystem::Get(World))
			{
				int32 Unlocked = 0;
				for (const FGBSkillNodeDefinition& Node : Data->GetSkillNodes())
				{
					if (Node.Row == 0 && Unlocked < 3)
					{
						Unlocked += Player->GetSkillTree()->Unlock(Node.Id) == EGBSkillUnlockResult::Ok ? 1 : 0;
					}
				}
			}
			Player->GetSpellbook()->TryCastSlot(0);
			Player->GetAttributes()->SetHealthAndMana(Player->GetAttributes()->GetMaxHealth() * 0.72f, Player->GetAttributes()->GetMaxMana() * 0.45f);
			if (AGBHUD* Hud = AGBHUD::Get(World))
			{
				Hud->ShowMessage(TEXT("Checkpoint reached"), 4.f);
			}
		});
		Add(U + 0.7f, TEXT("ui: screenshot rpg hud"), [](UWorld*, AGBPlayerCharacter*) { Shot(TEXT("ui_01_rpg_hud")); });
		Add(U + 0.9f, TEXT("ui: rpg low health, shield and effects"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			Player->GetSpellbook()->LearnSpell(TEXT("IceShield"));
			Player->GetSpellbook()->EquipSpell(4, TEXT("IceShield"));
			Player->GetAttributes()->RestoreAll();
			Player->GetSpellbook()->TryCastSlot(4);
			Player->GetStatusEffects()->ApplyEffect(TEXT("Slow"), Player);
			Player->GetStatusEffects()->ApplyEffect(TEXT("Ignite"), Player);
			Player->GetAttributes()->SetHealthAndMana(Player->GetAttributes()->GetMaxHealth() * 0.2f, Player->GetAttributes()->GetMaxMana() * 0.08f);
		});
		Add(U + 1.5f, TEXT("ui: screenshot rpg low"), [](UWorld*, AGBPlayerCharacter*) { Shot(TEXT("ui_02_rpg_low")); });
		Add(U + 1.7f, TEXT("ui: rpg pause"), [Menu](UWorld*, AGBPlayerCharacter* Player) { Player->GetAttributes()->RestoreAll(); Menu(Player, EGBMenu::Pause); });
		Add(U + 2.1f, TEXT("ui: screenshot rpg pause"), [](UWorld*, AGBPlayerCharacter*) { Shot(TEXT("ui_03_rpg_pause")); });
		Add(U + 2.3f, TEXT("ui: rpg inventory items"), [Menu, Cursor](UWorld* World, AGBPlayerCharacter* Player) { Menu(Player, EGBMenu::Inventory); Cursor(World, 0, 1, NAME_None, false); });
		Add(U + 2.7f, TEXT("ui: screenshot rpg items"), [](UWorld*, AGBPlayerCharacter*) { Shot(TEXT("ui_04_rpg_items")); });
		Add(U + 2.9f, TEXT("ui: rpg inventory weapons"), [Cursor](UWorld* World, AGBPlayerCharacter*) { Cursor(World, 1, 0, NAME_None, false); });
		Add(U + 3.3f, TEXT("ui: screenshot rpg weapons"), [](UWorld*, AGBPlayerCharacter*) { Shot(TEXT("ui_05_rpg_weapons")); });
		Add(U + 3.5f, TEXT("ui: rpg inventory spells"), [Cursor](UWorld* World, AGBPlayerCharacter*) { Cursor(World, 2, 0, NAME_None, false); });
		Add(U + 3.9f, TEXT("ui: screenshot rpg spells"), [](UWorld*, AGBPlayerCharacter*) { Shot(TEXT("ui_06_rpg_spells")); });
		Add(U + 4.1f, TEXT("ui: rpg spell chooser"), [Cursor](UWorld* World, AGBPlayerCharacter*) { Cursor(World, 2, 1, NAME_None, true); });
		Add(U + 4.5f, TEXT("ui: screenshot rpg spell chooser"), [](UWorld*, AGBPlayerCharacter*) { Shot(TEXT("ui_07_rpg_spell_choose")); });
		Add(U + 4.7f, TEXT("ui: rpg inventory skills, spell tree"), [Cursor](UWorld* World, AGBPlayerCharacter*) { Cursor(World, 3, 0, TEXT("Spell_FireballDamage"), false); });
		Add(U + 5.1f, TEXT("ui: screenshot rpg skills page"), [](UWorld*, AGBPlayerCharacter*) { Shot(TEXT("ui_08_rpg_skills_page")); });
		Add(U + 5.3f, TEXT("ui: rpg skill trees, a weapon node"), [Menu, Cursor](UWorld* World, AGBPlayerCharacter* Player)
		{
			Menu(Player, EGBMenu::SkillTrees);
			Cursor(World, 0, 0, TEXT("Weapon_Rifle_FireRate"), false);
		});
		Add(U + 5.7f, TEXT("ui: screenshot rpg skill trees"), [](UWorld*, AGBPlayerCharacter*) { Shot(TEXT("ui_09_rpg_skilltrees")); });
		Add(U + 5.75f, TEXT("ui: rpg skill trees, tab to the other tree"), [](UWorld* World, AGBPlayerCharacter*) { if (AGBHUD* Hud = AGBHUD::Get(World)) { Hud->MenuTab(1); } });
		Add(U + 5.85f, TEXT("ui: screenshot rpg skill trees, other tree"), [](UWorld*, AGBPlayerCharacter*) { Shot(TEXT("ui_09b_rpg_skilltrees_other")); });
		Add(U + 5.9f, TEXT("ui: rpg QTE"), [Menu](UWorld*, AGBPlayerCharacter* Player)
		{
			Menu(Player, EGBMenu::None);
			Player->GetAttributes()->RestoreAll();
			SetQTEEnabled(true);
			Player->GetSpellbook()->TryCastSlot(1);
		});
		Add(U + 6.2f, TEXT("ui: rpg QTE, one key"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			if (Player->GetQTE()->IsActive() && Player->GetQTE()->GetSession().Sequence.Num() > 0)
			{
				Player->GetQTE()->HandleKey(Player->GetQTE()->GetSession().Sequence[0]);
			}
		});
		Add(U + 6.5f, TEXT("ui: screenshot rpg QTE"), [](UWorld*, AGBPlayerCharacter*) { Shot(TEXT("ui_10_rpg_qte")); });
		Add(U + 10.8f, TEXT("ui: rpg death"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			SetQTEEnabled(false);
			FGBDamageInfo Info;
			Info.Amount = 10000000.f;
			UGBCombatLibrary::DealDamage(Player, Info, true);
		});
		Add(U + 11.6f, TEXT("ui: screenshot rpg death"), [](UWorld*, AGBPlayerCharacter*) { Shot(TEXT("ui_11_rpg_death")); });
		Add(U + 14.8f, TEXT("ui: travel to the future, pistol in hand"), [](UWorld* World, AGBPlayerCharacter* Player)
		{
			UGBEraSubsystem::Get(World)->RequestTravel(EGBEra::Future, Player);
		});
		Add(U + 17.8f, TEXT("ui: cyber QTE (enchant), pistol in hand"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			Player->GetEquipment()->GiveWeapon(TEXT("Pistol"), true, false);
			Player->GetEquipment()->SwitchTo(EGBHeldSlot::Gun);
			Player->GetAttributes()->RestoreAll();
			SetQTEEnabled(true);
			Player->GetSpellbook()->TryCastEnchant();
		});
		// The enchant's QTE opens with the element choice on fixed keys (owner, Nov 13): A fire, W lightning, D ice.
		Add(U + 18.0f, TEXT("ui: screenshot cyber enchant element choice"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			const bool bChoosing = Player->GetQTE()->IsActive() && Player->GetQTE()->GetSession().Phase == EGBQTEPhase::Choice;
			UE_LOG(LogGB, Display, TEXT("SMOKE enchant choice: QTE active=%d choosing=%d current=%s"), Player->GetQTE()->IsActive() ? 1 : 0, bChoosing ? 1 : 0,
				*UEnum::GetValueAsString(Player->GetSpellbook()->GetEnchantElement()));
			if (!bChoosing)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE enchant choice FAILED: the enchant's QTE did not open on the element choice"));
			}
			Shot(TEXT("ui_12a_cyber_enchant_choice"));
		});
		Add(U + 18.5f, TEXT("ui: cyber QTE, D picks ice, then one key"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			UGBQTEComponent* QTE = Player->GetQTE();
			if (QTE->IsActive())
			{
				QTE->HandleKey(EGBQTEKey::Right);
				if (QTE->GetSession().Phase == EGBQTEPhase::Sequence && QTE->GetSession().Sequence.Num() > 0)
				{
					QTE->HandleKey(QTE->GetSession().Sequence[0]);
				}
			}
		});
		Add(U + 18.8f, TEXT("ui: screenshot cyber QTE"), [](UWorld*, AGBPlayerCharacter*) { Shot(TEXT("ui_12_cyber_qte")); });
		Add(U + 22.7f, TEXT("ui: the element picked in the QTE is the enchant's element now"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			// The QTE normally ran out by now; if a hitch kept it alive, end it (the choice was made before, it still counts).
			const bool bWasActive = Player->GetQTE()->IsActive();
			if (bWasActive)
			{
				Player->GetQTE()->CancelQTE();
			}
			const EGBElement Element = Player->GetSpellbook()->GetEnchantElement();
			UE_LOG(LogGB, Display, TEXT("SMOKE enchant choice: element after the QTE %s (QTE was still active=%d)"), *UEnum::GetValueAsString(Element), bWasActive ? 1 : 0);
			if (Element != EGBElement::Ice)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE enchant choice FAILED: element %s after picking ice"), *UEnum::GetValueAsString(Element));
			}
			Player->GetSpellbook()->SetEnchantElement(EGBElement::Fire); // the cyberpunk HUD shots below show the fire enchant
		});
		Add(U + 22.8f, TEXT("ui: cyber setup, enchanted, spent rounds"), [](UWorld* World, AGBPlayerCharacter* Player)
		{
			SetQTEEnabled(false);
			Player->GetStatusEffects()->ApplyEffect(TEXT("Enchant_Fire"), Player);
			if (AGBHeldItem* Gun = Player->GetEquipment()->GetActiveItem())
			{
				FGBAmmoState Ammo = Gun->GetAmmo();
				Ammo.InMagazine = FMath::Max(0, Ammo.InMagazine - 4);
				Gun->SetAmmo(Ammo);
			}
			Player->GetAttributes()->SetHealthAndMana(Player->GetAttributes()->GetMaxHealth() * 0.72f, Player->GetAttributes()->GetMaxMana() * 0.45f);
			if (AGBHUD* Hud = AGBHUD::Get(World))
			{
				Hud->ShowMessage(TEXT("Picked up Pistol"), 4.f);
			}
		});
		Add(U + 23.5f, TEXT("ui: screenshot cyber hud"), [](UWorld*, AGBPlayerCharacter*) { Shot(TEXT("ui_13_cyber_hud")); });
		Add(U + 23.7f, TEXT("ui: cyber low health, reloading"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			Player->GetStatusEffects()->ApplyEffect(TEXT("Slow"), Player);
			Player->GetAttributes()->SetHealthAndMana(Player->GetAttributes()->GetMaxHealth() * 0.2f, Player->GetAttributes()->GetMaxMana() * 0.1f);
			if (AGBHeldItem* Gun = Player->GetEquipment()->GetActiveItem())
			{
				Gun->Reload();
			}
		});
		Add(U + 24.2f, TEXT("ui: screenshot cyber low"), [](UWorld*, AGBPlayerCharacter*) { Shot(TEXT("ui_14_cyber_low")); });
		Add(U + 24.4f, TEXT("ui: cyber pause"), [Menu](UWorld*, AGBPlayerCharacter* Player) { Player->GetAttributes()->RestoreAll(); Menu(Player, EGBMenu::Pause); });
		Add(U + 24.8f, TEXT("ui: screenshot cyber pause"), [](UWorld*, AGBPlayerCharacter*) { Shot(TEXT("ui_15_cyber_pause")); });
		Add(U + 25.0f, TEXT("ui: cyber inventory items"), [Menu, Cursor](UWorld* World, AGBPlayerCharacter* Player) { Menu(Player, EGBMenu::Inventory); Cursor(World, 0, 1, NAME_None, false); });
		Add(U + 25.4f, TEXT("ui: screenshot cyber items"), [](UWorld*, AGBPlayerCharacter*) { Shot(TEXT("ui_16_cyber_items")); });
		Add(U + 25.6f, TEXT("ui: cyber inventory weapons"), [Cursor](UWorld* World, AGBPlayerCharacter*) { Cursor(World, 1, 1, NAME_None, false); });
		Add(U + 26.0f, TEXT("ui: screenshot cyber weapons"), [](UWorld*, AGBPlayerCharacter*) { Shot(TEXT("ui_17_cyber_weapons")); });
		Add(U + 26.2f, TEXT("ui: cyber inventory spells"), [Cursor](UWorld* World, AGBPlayerCharacter*) { Cursor(World, 2, 5, NAME_None, false); });
		Add(U + 26.6f, TEXT("ui: screenshot cyber spells"), [](UWorld*, AGBPlayerCharacter*) { Shot(TEXT("ui_18_cyber_spells")); });
		Add(U + 26.8f, TEXT("ui: cyber spell chooser"), [Cursor](UWorld* World, AGBPlayerCharacter*) { Cursor(World, 2, 2, NAME_None, true); });
		Add(U + 27.2f, TEXT("ui: screenshot cyber spell chooser"), [](UWorld*, AGBPlayerCharacter*) { Shot(TEXT("ui_19_cyber_spell_choose")); });
		Add(U + 27.4f, TEXT("ui: cyber inventory skills"), [Cursor](UWorld* World, AGBPlayerCharacter*) { Cursor(World, 3, 0, TEXT("Weapon_Damage"), false); });
		Add(U + 27.8f, TEXT("ui: screenshot cyber skills page"), [](UWorld*, AGBPlayerCharacter*) { Shot(TEXT("ui_20_cyber_skills_page")); });
		Add(U + 28.0f, TEXT("ui: cyber skill trees"), [Menu](UWorld*, AGBPlayerCharacter* Player) { Menu(Player, EGBMenu::SkillTrees); });
		Add(U + 28.4f, TEXT("ui: screenshot cyber skill trees"), [](UWorld*, AGBPlayerCharacter*) { Shot(TEXT("ui_21_cyber_skilltrees")); });
		Add(U + 28.45f, TEXT("ui: cyber skill trees, tab to the other tree"), [](UWorld* World, AGBPlayerCharacter*) { if (AGBHUD* Hud = AGBHUD::Get(World)) { Hud->MenuTab(-1); } });
		Add(U + 28.55f, TEXT("ui: screenshot cyber skill trees, other tree"), [](UWorld*, AGBPlayerCharacter*) { Shot(TEXT("ui_21b_cyber_skilltrees_other")); });
		// The menu look changes while the inventory stays open (owner, Nov 13): in the future, the Skills page on the
		// spell tree is RPG and the other pages are cyberpunk. Each jump plays a transition; catch both mid-way.
		Add(U + 28.6f, TEXT("ui: future inventory, skills page on the spell tree (RPG look)"), [Menu, Cursor](UWorld* World, AGBPlayerCharacter* Player)
		{
			Menu(Player, EGBMenu::Inventory);
			Cursor(World, 3, 0, TEXT("Spell_FireballDamage"), false);
		});
		Add(U + 29.0f, TEXT("ui: tab to the spells page (into cyberpunk)"), [](UWorld* World, AGBPlayerCharacter*) { if (AGBHUD* Hud = AGBHUD::Get(World)) { Hud->MenuTab(-1); } });
		Add(U + 29.12f, TEXT("ui: screenshot transition into cyberpunk"), [](UWorld*, AGBPlayerCharacter*) { Shot(TEXT("ui_23_transition_to_cyber")); });
		Add(U + 29.7f, TEXT("ui: tab back to the skills page (into RPG)"), [](UWorld* World, AGBPlayerCharacter*) { if (AGBHUD* Hud = AGBHUD::Get(World)) { Hud->MenuTab(1); } });
		Add(U + 29.85f, TEXT("ui: screenshot transition into RPG"), [](UWorld*, AGBPlayerCharacter*) { Shot(TEXT("ui_24_transition_to_rpg")); });
		// Every skill point asks first (owner, Nov 13): Enter opens the dialog, Esc cancels it without spending, and
		// Enter on the dialog spends exactly one rank.
		Add(U + 30.4f, TEXT("ui: skill trees, Enter on Heavy Rounds opens the confirmation"), [Menu](UWorld* World, AGBPlayerCharacter* Player)
		{
			Menu(Player, EGBMenu::SkillTrees);
			if (AGBHUD* Hud = AGBHUD::Get(World))
			{
				Hud->DebugSetMenuCursor(0, 0, TEXT("Weapon_Damage"), false);
				UiConfirmPointsBefore = Player->GetSkillTree()->GetPoints();
				Hud->MenuConfirm();
			}
		});
		Add(U + 30.7f, TEXT("ui: screenshot cyberpunk confirmation"), [](UWorld*, AGBPlayerCharacter*) { Shot(TEXT("ui_25_cyber_confirm")); });
		Add(U + 31.3f, TEXT("ui: tab to the spell tree (cancels), Enter on a spell perk"), [](UWorld* World, AGBPlayerCharacter* Player)
		{
			if (AGBHUD* Hud = AGBHUD::Get(World))
			{
				Hud->MenuTab(-1);
				Hud->DebugSetMenuCursor(0, 0, TEXT("Spell_FireballDamage"), false);
				Hud->MenuConfirm();
			}
		});
		Add(U + 31.9f, TEXT("ui: screenshot RPG confirmation"), [](UWorld*, AGBPlayerCharacter*) { Shot(TEXT("ui_26_rpg_confirm")); });
		Add(U + 32.4f, TEXT("ui: Esc cancels: no point spent; then Enter, Enter spends one"), [](UWorld* World, AGBPlayerCharacter* Player)
		{
			AGBHUD* Hud = AGBHUD::Get(World);
			UGBSkillTreeComponent* Skills = Player->GetSkillTree();
			if (!Hud || !Skills)
			{
				return;
			}
			const FName Node(TEXT("Spell_FireballDamage"));
			const int32 RankBefore = Skills->GetRank(Node);
			const bool bClosed = !Hud->MenuBack(); // Esc: the dialog takes it (returns true) and closes
			const int32 AfterCancel = Skills->GetPoints();
			Hud->MenuConfirm(); // asks again
			const int32 AfterAsk = Skills->GetPoints();
			Hud->MenuConfirm(); // spends
			const int32 AfterSpend = Skills->GetPoints();
			UE_LOG(LogGB, Display, TEXT("SMOKE skill confirm: points %d before, %d after Esc, %d after one Enter, %d after Enter on the dialog; rank %d -> %d"),
				UiConfirmPointsBefore, AfterCancel, AfterAsk, AfterSpend, RankBefore, Skills->GetRank(Node));
			if (bClosed || AfterCancel != UiConfirmPointsBefore || AfterAsk != UiConfirmPointsBefore || AfterSpend != UiConfirmPointsBefore - 1 || Skills->GetRank(Node) != RankBefore + 1)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE skill confirm FAILED: Esc closed the menu=%d; points %d / %d / %d (expected %d, %d, %d); rank %d (expected %d)"),
					bClosed ? 1 : 0, AfterCancel, AfterAsk, AfterSpend, UiConfirmPointsBefore, UiConfirmPointsBefore, UiConfirmPointsBefore - 1, Skills->GetRank(Node), RankBefore + 1);
			}
		});
		Add(U + 32.6f, TEXT("ui: cyber death"), [Menu](UWorld*, AGBPlayerCharacter* Player)
		{
			Menu(Player, EGBMenu::None);
			FGBDamageInfo Info;
			Info.Amount = 10000000.f;
			UGBCombatLibrary::DealDamage(Player, Info, true);
		});
		Add(U + 33.4f, TEXT("ui: screenshot cyber death"), [](UWorld*, AGBPlayerCharacter*) { Shot(TEXT("ui_22_cyber_death")); });
		// ---- Mantle block (gb.Smoke mantle): ledge climb of the 200 cm path-test wall and the 600 cm platform.
		// The arena numbers are the design's (wall centre (-2500, -1150, 100), size (600, 100, 200): north face
		// at Y = -1100, top at Z = 200; platform centre (2300, 2300), 1000 x 1000, top 600, west face at X = 1800).
		// Quit is the only step after this block, so the end puts the player back at the arena start.
		// A filtered run (gb.Smoke mantle) starts this block 1 s after the map load, while frames still hitch and a
		// screenshot can capture a stale frame: the block waits 3 s first. The shots freeze time (dilation) so they
		// catch the climb mid-way however slow the frame is, and the next step resumes it.
		Add(U + 33.6f, TEXT("mantle: let the renderer settle"), [](UWorld*, AGBPlayerCharacter*) {});
		const float M = U + 36.6f;
		auto MantleShot = [](UWorld* World, AGBPlayerCharacter* Player, const TCHAR* Name)
		{
			const GBMantle::FMantlePose Pose = Player->GetMantlePose();
			UE_LOG(LogGB, Display, TEXT("SMOKE mantle shot %s mantling=%d alpha=%.2f grip=%.2f lower=%.2f dip=%.2f control=%d"), Name,
				Player->IsMantling() ? 1 : 0, Player->GetMantleAlpha(), Pose.Grip01, Pose.ItemLower01, Pose.CameraDip01, Player->IsControlEnabled() ? 1 : 0);
			if (AWorldSettings* Settings = World ? World->GetWorldSettings() : nullptr)
			{
				Settings->SetTimeDilation(0.01f);
			}
			Shot(Name);
		};
		auto MantleResume = [](UWorld* World, AGBPlayerCharacter*)
		{
			if (AWorldSettings* Settings = World ? World->GetWorldSettings() : nullptr)
			{
				Settings->SetTimeDilation(1.f);
			}
		};
		auto MantleCapsule = [](AGBPlayerCharacter* Player, float& OutRadius, float& OutHalfHeight)
		{
			const UCapsuleComponent* Capsule = Player->GetCapsuleComponent();
			OutRadius = Capsule ? Capsule->GetScaledCapsuleRadius() : 38.f;
			OutHalfHeight = Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 90.f;
		};
		Add(M + 0.0f, TEXT("mantle: ground press next to the 200 cm wall"), [MantleCapsule](UWorld* World, AGBPlayerCharacter* Player)
		{
			// The ui cyber death step before this block kills the player: start alive and in control again.
			if (AGBPlayerController* PC = Cast<AGBPlayerController>(Player->GetController()))
			{
				PC->SetMenu(EGBMenu::None);
			}
			if (Player->GetQTE()->IsActive())
			{
				Player->GetQTE()->CancelQTE();
			}
			Player->RestoreAll();
			Player->GetStatusEffects()->RemoveAllEffects();
			Player->GetSpellbook()->SetBlockHeld(false);
			Player->SetControlEnabled(true);
			float R = 38.f;
			float HH = 90.f;
			MantleCapsule(Player, R, HH);
			if (AGBPlayerController* PC = Cast<AGBPlayerController>(Player->GetController()))
			{
				PC->SetControlRotation(FRotator(0.f, -90.f, 0.f));
			}
			Player->SetActorLocation(FVector(-2500.f, -1100.f + R + 5.f, HH), false, nullptr, ETeleportType::TeleportPhysics);
			if (UCharacterMovementComponent* Move = Player->GetCharacterMovement())
			{
				Move->SetMovementMode(MOVE_Walking);
				Move->Velocity = FVector::ZeroVector;
			}
			MarkSim(World);
			Player->PressJump();
			const bool bStarted = Player->IsMantling();
			UE_LOG(LogGB, Display, TEXT("SMOKE mantle ground started=%d"), bStarted ? 1 : 0);
			if (!bStarted)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE mantle ground FAILED: no mantle started on jump press next to the 200 cm wall"));
			}
		});
		Add(M + 0.1f, TEXT("mantle: screenshot mid-climb"), [MantleShot](UWorld* World, AGBPlayerCharacter* Player) { MantleShot(World, Player, TEXT("mantle_grip")); }, 0.12f);
		Add(M + 0.8f, TEXT("mantle: resume time after the shot"), MantleResume);
		Add(M + 1.4f, TEXT("mantle: ground result"), [MantleCapsule](UWorld*, AGBPlayerCharacter* Player)
		{
			float R = 38.f;
			float HH = 90.f;
			MantleCapsule(Player, R, HH);
			(void)R;
			const FVector Loc = Player->GetActorLocation();
			const float Feet = Loc.Z - HH;
			const bool bOnTop = FMath::Abs(Feet - 200.f) <= 10.f && Loc.Y < -1100.f;
			const UCharacterMovementComponent* GroundMove = Player->GetCharacterMovement();
			const bool bDone = !Player->IsMantling() && GroundMove && GroundMove->IsMovingOnGround();
			// The view offsets go through the camera's additive offset: back to identity at rest.
			FTransform CamOffset;
			float CamFovOffset = 0.f;
			Player->GetCamera()->GetAdditiveOffset(CamOffset, CamFovOffset);
			const FRotator CamRel = CamOffset.Rotator();
			const bool bCamRest = CamRel.IsNearlyZero(0.1f) && CamOffset.GetLocation().IsNearlyZero(0.1f);
			const FVector ItemRel = Player->GetHeldItemAnchor()->GetRelativeLocation();
			const bool bItemRest = ItemRel.Equals(FVector(30.f, 15.f, -15.f), 1.f);
			UE_LOG(LogGB, Display, TEXT("SMOKE mantle ground feet=%.0f y=%.0f mantling=%d cam=(%.1f,%.1f,%.1f) item=%s"),
				Feet, Loc.Y, Player->IsMantling() ? 1 : 0, CamRel.Pitch, CamRel.Yaw, CamRel.Roll, *ItemRel.ToCompactString());
			if (!bOnTop || !bDone || !bCamRest || !bItemRest)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE mantle ground FAILED: onTop=%d done=%d camRest=%d itemRest=%d"), bOnTop ? 1 : 0, bDone ? 1 : 0, bCamRest ? 1 : 0, bItemRest ? 1 : 0);
			}
		}, 1.0f);
		static float MantleJumpStartZ = 0.f;
		Add(M + 1.6f, TEXT("mantle: wall too high is a normal jump"), [MantleCapsule](UWorld* World, AGBPlayerCharacter* Player)
		{
			float R = 38.f;
			float HH = 90.f;
			MantleCapsule(Player, R, HH);
			if (AGBPlayerController* PC = Cast<AGBPlayerController>(Player->GetController()))
			{
				PC->SetControlRotation(FRotator(0.f, 0.f, 0.f));
			}
			Player->SetActorLocation(FVector(1800.f - R - 5.f, 2600.f, HH), false, nullptr, ETeleportType::TeleportPhysics);
			if (UCharacterMovementComponent* Move = Player->GetCharacterMovement())
			{
				Move->SetMovementMode(MOVE_Walking);
				Move->Velocity = FVector::ZeroVector;
			}
			MarkSim(World);
			MantleJumpStartZ = Player->GetActorLocation().Z;
			Player->PressJump();
			const bool bMantling = Player->IsMantling();
			UE_LOG(LogGB, Display, TEXT("SMOKE mantle too high mantling=%d"), bMantling ? 1 : 0);
			if (bMantling)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE mantle too high FAILED: a mantle started for the 600 cm platform (out of one jump + reach)"));
			}
		});
		Add(M + 1.9f, TEXT("mantle: wall too high leaves the ground"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			const UCharacterMovementComponent* Move = Player->GetCharacterMovement();
			const float Rise = Player->GetActorLocation().Z - MantleJumpStartZ;
			const bool bAirborne = Move && Move->MovementMode == MOVE_Falling && Rise > 10.f;
			UE_LOG(LogGB, Display, TEXT("SMOKE mantle too high airborne=%d rise=%.0f mantling=%d"), bAirborne ? 1 : 0, Rise, Player->IsMantling() ? 1 : 0);
			if (Player->IsMantling() || !bAirborne)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE mantle too high FAILED: expected a normal jump (airborne, not mantling)"));
			}
		}, 0.2f);
		Add(M + 2.1f, TEXT("mantle: air grab at the platform edge"), [MantleCapsule](UWorld* World, AGBPlayerCharacter* Player)
		{
			float R = 38.f;
			float HH = 90.f;
			MantleCapsule(Player, R, HH);
			if (AGBPlayerController* PC = Cast<AGBPlayerController>(Player->GetController()))
			{
				PC->SetControlRotation(FRotator(0.f, 0.f, 0.f));
			}
			Player->SetActorLocation(FVector(1800.f - R - 5.f, 2600.f, 600.f - 120.f + HH), false, nullptr, ETeleportType::TeleportPhysics);
			if (UCharacterMovementComponent* Move = Player->GetCharacterMovement())
			{
				Move->SetMovementMode(MOVE_Falling);
				Move->Velocity = FVector::ZeroVector;
			}
			MarkSim(World);
		});
		// Second take of the grip, from the air grab (about a third into the climb, hands on the ledge).
		Add(M + 2.2f, TEXT("mantle: screenshot mid air-climb"), [MantleShot](UWorld* World, AGBPlayerCharacter* Player) { MantleShot(World, Player, TEXT("mantle_grip_air")); }, 0.1f);
		Add(M + 2.7f, TEXT("mantle: resume time after the air shot"), MantleResume);
		Add(M + 3.3f, TEXT("mantle: air result on the platform"), [MantleCapsule](UWorld*, AGBPlayerCharacter* Player)
		{
			float R = 38.f;
			float HH = 90.f;
			MantleCapsule(Player, R, HH);
			(void)R;
			const FVector Loc = Player->GetActorLocation();
			const float Feet = Loc.Z - HH;
			const UCharacterMovementComponent* AirMove = Player->GetCharacterMovement();
			const bool bOnTop = FMath::Abs(Feet - 600.f) <= 10.f && Loc.X > 1800.f && !Player->IsMantling() && AirMove && AirMove->IsMovingOnGround();
			UE_LOG(LogGB, Display, TEXT("SMOKE mantle air feet=%.0f x=%.0f mantling=%d walking=%d"), Feet, Loc.X, Player->IsMantling() ? 1 : 0, AirMove && AirMove->IsMovingOnGround() ? 1 : 0);
			if (!bOnTop)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE mantle air FAILED: feet %.0f (expected 600 +- 10) x %.0f (expected over 1800)"), Feet, Loc.X);
			}
		}, 1.0f);
		Add(M + 3.5f, TEXT("mantle: back to the 200 cm wall, standing jump press"), [MantleCapsule](UWorld* World, AGBPlayerCharacter* Player)
		{
			float R = 38.f;
			float HH = 90.f;
			MantleCapsule(Player, R, HH);
			// LastMoveInput is protected: ReleaseHeldInputs clears it instead of reading it back.
			Player->ReleaseHeldInputs();
			if (AGBPlayerController* PC = Cast<AGBPlayerController>(Player->GetController()))
			{
				PC->SetControlRotation(FRotator(0.f, 90.f, 0.f)); // back to the wall
			}
			Player->SetActorLocation(FVector(-2500.f, -1100.f + R + 5.f, HH), false, nullptr, ETeleportType::TeleportPhysics);
			if (UCharacterMovementComponent* Move = Player->GetCharacterMovement())
			{
				Move->SetMovementMode(MOVE_Walking);
				Move->Velocity = FVector::ZeroVector;
			}
			MarkSim(World);
			Player->PressJump();
			const bool bStarted = Player->IsMantling();
			UE_LOG(LogGB, Display, TEXT("SMOKE mantle back started=%d"), bStarted ? 1 : 0);
			if (!bStarted)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE mantle back FAILED: no mantle started on jump press with the back to the 200 cm wall"));
			}
		});
		Add(M + 4.9f, TEXT("mantle: back climb result"), [MantleCapsule](UWorld*, AGBPlayerCharacter* Player)
		{
			float R = 38.f;
			float HH = 90.f;
			MantleCapsule(Player, R, HH);
			(void)R;
			const FVector Loc = Player->GetActorLocation();
			const float Feet = Loc.Z - HH;
			const UCharacterMovementComponent* GroundMove = Player->GetCharacterMovement();
			const bool bWalking = !Player->IsMantling() && GroundMove && GroundMove->IsMovingOnGround();
			const float ControlYaw = Player->GetController() ? Player->GetController()->GetControlRotation().Yaw : 0.f;
			const float YawError = FMath::Abs(FRotator::NormalizeAxis(ControlYaw + 90.f));
			const bool bOnTop = FMath::Abs(Feet - 200.f) <= 10.f && Loc.Y < -1100.f;
			UE_LOG(LogGB, Display, TEXT("SMOKE mantle back feet=%.0f y=%.0f mantling=%d walking=%d yaw=%.0f"), Feet, Loc.Y,
				Player->IsMantling() ? 1 : 0, bWalking ? 1 : 0, ControlYaw);
			if (!bOnTop || !bWalking || YawError > 15.f)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE mantle back FAILED: onTop=%d walking=%d yaw error=%.0f (expected the 200 cm top and the view turned to -90)"),
					bOnTop ? 1 : 0, bWalking ? 1 : 0, YawError);
			}
		}, 1.0f);
		Add(M + 5.1f, TEXT("mantle: back to the platform, air grab"), [MantleCapsule](UWorld* World, AGBPlayerCharacter* Player)
		{
			float R = 38.f;
			float HH = 90.f;
			MantleCapsule(Player, R, HH);
			if (AGBPlayerController* PC = Cast<AGBPlayerController>(Player->GetController()))
			{
				PC->SetControlRotation(FRotator(0.f, 180.f, 0.f)); // back to the platform
			}
			Player->SetActorLocation(FVector(1800.f - R - 5.f, 2600.f, 600.f - 120.f + HH), false, nullptr, ETeleportType::TeleportPhysics);
			if (UCharacterMovementComponent* Move = Player->GetCharacterMovement())
			{
				Move->SetMovementMode(MOVE_Falling);
				Move->Velocity = FVector::ZeroVector;
			}
			MarkSim(World);
		});
		Add(M + 6.1f, TEXT("mantle: back air result"), [MantleCapsule](UWorld*, AGBPlayerCharacter* Player)
		{
			float R = 38.f;
			float HH = 90.f;
			MantleCapsule(Player, R, HH);
			(void)R;
			const FVector Loc = Player->GetActorLocation();
			const float Feet = Loc.Z - HH;
			const UCharacterMovementComponent* AirMove = Player->GetCharacterMovement();
			const bool bWalking = !Player->IsMantling() && AirMove && AirMove->IsMovingOnGround();
			const float ControlYaw = Player->GetController() ? Player->GetController()->GetControlRotation().Yaw : 0.f;
			const float YawError = FMath::Abs(FRotator::NormalizeAxis(ControlYaw));
			const bool bOnTop = FMath::Abs(Feet - 600.f) <= 10.f && Loc.X > 1800.f;
			UE_LOG(LogGB, Display, TEXT("SMOKE mantle back air feet=%.0f x=%.0f mantling=%d walking=%d yaw=%.0f"), Feet, Loc.X,
				Player->IsMantling() ? 1 : 0, bWalking ? 1 : 0, ControlYaw);
			if (!bOnTop || !bWalking || YawError > 15.f)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE mantle back air FAILED: onTop=%d walking=%d yaw error=%.0f (expected the 600 cm top and the view turned to 0)"),
					bOnTop ? 1 : 0, bWalking ? 1 : 0, YawError);
			}
		}, 1.0f);
		Add(M + 6.3f, TEXT("mantle: holding away from a wall behind is a jump"), [MantleCapsule](UWorld* World, AGBPlayerCharacter* Player)
		{
			float R = 38.f;
			float HH = 90.f;
			MantleCapsule(Player, R, HH);
			if (AGBPlayerController* PC = Cast<AGBPlayerController>(Player->GetController()))
			{
				PC->SetControlRotation(FRotator(0.f, 90.f, 0.f)); // back to the wall
			}
			Player->SetActorLocation(FVector(-2500.f, -1100.f + R + 5.f, HH), false, nullptr, ETeleportType::TeleportPhysics);
			if (UCharacterMovementComponent* Move = Player->GetCharacterMovement())
			{
				Move->SetMovementMode(MOVE_Walking);
				Move->Velocity = FVector::ZeroVector;
			}
			Player->SetMoveInputForTest(FVector2D(0.f, 1.f)); // forward, away from the wall behind
			MarkSim(World);
			MantleJumpStartZ = Player->GetActorLocation().Z;
			Player->PressJump();
			const bool bMantling = Player->IsMantling();
			UE_LOG(LogGB, Display, TEXT("SMOKE mantle held away mantling=%d"), bMantling ? 1 : 0);
			if (bMantling)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE mantle held away FAILED: a mantle started while holding away from the wall behind"));
			}
		});
		Add(M + 6.4f, TEXT("mantle: held away result, a real jump away from the wall"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			// The wall is behind (south, -Y): a normal jump forward rises and moves north, never climbs.
			const UCharacterMovementComponent* Move = Player->GetCharacterMovement();
			const float Rise = Player->GetActorLocation().Z - MantleJumpStartZ;
			const float AwaySpeed = Move ? Move->Velocity.Y : 0.f;
			const bool bJump = Move && Move->MovementMode == MOVE_Falling && Rise > 10.f && AwaySpeed > 100.f && !Player->IsMantling();
			UE_LOG(LogGB, Display, TEXT("SMOKE mantle held away jump=%d rise=%.0f away_speed=%.0f mantling=%d"), bJump ? 1 : 0, Rise, AwaySpeed, Player->IsMantling() ? 1 : 0);
			if (!bJump)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE mantle held away FAILED: expected a jump away from the wall (falling, rising, moving north, not climbing)"));
			}
		}, 0.2f);
		Add(M + 6.5f, TEXT("mantle: back to the arena start"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			FVector EnemySpawn;
			FVector PlayerSpot;
			AGBTestArena::GetPathTestPoints(EnemySpawn, PlayerSpot);
			if (AGBPlayerController* PC = Cast<AGBPlayerController>(Player->GetController()))
			{
				PC->SetControlRotation(FRotator(0.f, 0.f, 0.f));
			}
			Player->SetActorLocation(PlayerSpot, false, nullptr, ETeleportType::TeleportPhysics);
			if (UCharacterMovementComponent* Move = Player->GetCharacterMovement())
			{
				Move->SetMovementMode(MOVE_Falling);
				Move->Velocity = FVector::ZeroVector;
			}
		});
		// ---- Vault block (gb.Smoke mantle): a thin obstacle too high to jump is vaulted (jump press or pushing), five
		// lanes along X at the same wall line (Y = 1500, 10 cm deep), the player facing north (+Y). Positive: a 120 cm wall;
		// the same on a 250 cm slab with the floor beyond 250 lower; the same under a 300 cm ceiling. Negative: a 200 cm
		// wall; a 120 cm wall on a 600 cm slab (the floor beyond is past VaultMaxDrop). Then the push (no jump press).
		const float V = U + 43.2f;
		constexpr float VaultSpan = 28.6f;
		static constexpr float VaultWallY = 1500.f;
		static constexpr float VaultWallDepth = 10.f;
		static constexpr float VaultLaneA = -2800.f;
		static constexpr float VaultLaneB = -2450.f;
		static constexpr float VaultLaneC = -2100.f;
		static constexpr float VaultLaneD = -1750.f;
		static constexpr float VaultLaneE = -1400.f;
		static constexpr float VaultLaneG = -1050.f;
		static constexpr float VaultLaneH = -700.f;
		static TArray<TWeakObjectPtr<AActor>> VaultActors;
		auto VaultBox = [](UWorld* World, const FVector& Center, const FVector& Size, const FRotator& Rotation = FRotator::ZeroRotator)
		{
			UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AStaticMeshActor* Box = Cube ? World->SpawnActor<AStaticMeshActor>(Center, Rotation, Params) : nullptr;
			if (Box)
			{
				Box->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
				Box->GetStaticMeshComponent()->SetStaticMesh(Cube);
				Box->SetActorScale3D(Size / 100.f);
				VaultActors.Add(Box);
			}
		};
		Add(V + 0.0f, TEXT("mantle: vault, build the test obstacles"), [VaultBox](UWorld* World, AGBPlayerCharacter*)
		{
			const FVector Wall(300.f, VaultWallDepth, 120.f);
			VaultBox(World, FVector(VaultLaneA, VaultWallY, 60.f), Wall);
			VaultBox(World, FVector(VaultLaneB, 1250.f, 125.f), FVector(300.f, 500.f, 250.f));
			VaultBox(World, FVector(VaultLaneB, VaultWallY, 250.f + 60.f), Wall);
			VaultBox(World, FVector(VaultLaneC, VaultWallY, 63.5f), FVector(300.f, VaultWallDepth, 127.f));
			VaultBox(World, FVector(VaultLaneC, VaultWallY, 310.f), FVector(400.f, 700.f, 20.f));
			VaultBox(World, FVector(VaultLaneD, VaultWallY, 100.f), FVector(300.f, VaultWallDepth, 200.f));
			VaultBox(World, FVector(VaultLaneE, 1250.f, 300.f), FVector(300.f, 500.f, 600.f));
			VaultBox(World, FVector(VaultLaneE, VaultWallY, 600.f + 60.f), Wall);
			// A railing-like frame of thin bars with open gaps (the real corridor's): a top bar at 125, a bottom bar, three uprights.
			VaultBox(World, FVector(VaultLaneG, VaultWallY, 123.f), FVector(240.f, 6.f, 4.f));
			VaultBox(World, FVector(VaultLaneG, VaultWallY, 12.f), FVector(240.f, 6.f, 4.f));
			for (const float Offset : { -117.f, 0.f, 117.f })
			{
				VaultBox(World, FVector(VaultLaneG + Offset, VaultWallY, 62.5f), FVector(6.f, 6.f, 125.f));
			}
			// The same with a diagonal brace, as in the real railing (a truss seen from the side): a bottom bar near the player,
			// a top bar 48 cm further back, and a bar leaning about 25 degrees from vertical between them, its lower end
			// sticking out in front of the bottom bar so the capsule sweep meets the slanted brace first (its normal points
			// up or down by about 0.4). 56 cm deep in all, like the real one.
			VaultBox(World, FVector(VaultLaneH, 1500.f, 12.f), FVector(240.f, 6.f, 4.f));
			VaultBox(World, FVector(VaultLaneH, 1545.f, 123.f), FVector(240.f, 6.f, 4.f));
			VaultBox(World, FVector(VaultLaneH - 117.f, 1522.f, 62.5f), FVector(6.f, 56.f, 125.f));
			VaultBox(World, FVector(VaultLaneH + 117.f, 1522.f, 62.5f), FVector(6.f, 56.f, 125.f));
			VaultBox(World, FVector(VaultLaneH, 1519.f, 70.5f), FVector(6.f, 6.f, 112.f), FRotator(0.f, 0.f, -25.4f));
			UE_LOG(LogGB, Display, TEXT("SMOKE mantle vault obstacles built: %d boxes"), VaultActors.Num());
			if (VaultActors.Num() != 18)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE mantle vault FAILED: only %d of 18 obstacle boxes were built"), VaultActors.Num());
			}
		});
		// Stands the player against the wall of a lane, facing it, on the near side (feet at FeetZ); presses jump if asked.
		auto VaultPlace = [](UWorld* World, AGBPlayerCharacter* Player, float Lane, float FeetZ, bool bPress, float FaceY = VaultWallY - VaultWallDepth * 0.5f) -> bool
		{
			const UCapsuleComponent* Capsule = Player->GetCapsuleComponent();
			const float R = Capsule ? Capsule->GetScaledCapsuleRadius() : 38.f;
			const float HH = Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 90.f;
			Player->ClearMoveInputForTest();
			Player->ReleaseHeldInputs();
			Player->SetVaultMeasure(true);
			if (AController* Controller = Player->GetController())
			{
				Controller->SetControlRotation(FRotator(0.f, 90.f, 0.f));
			}
			Player->SetActorLocation(FVector(Lane, FaceY - R - 5.f, FeetZ + HH), false, nullptr, ETeleportType::TeleportPhysics);
			if (UCharacterMovementComponent* Move = Player->GetCharacterMovement())
			{
				Move->SetMovementMode(MOVE_Walking);
				Move->Velocity = FVector::ZeroVector;
			}
			MarkSim(World);
			if (bPress)
			{
				Player->PressJump();
			}
			return Player->IsVaulting();
		};
		// Over the wall, on his feet on the floor, view and held item at rest: logged and checked.
		auto VaultCheckLanded = [](UWorld* World, AGBPlayerCharacter* Player, const TCHAR* Tag, float ExpectX)
		{
			const UCapsuleComponent* Capsule = Player->GetCapsuleComponent();
			const float R = Capsule ? Capsule->GetScaledCapsuleRadius() : 38.f;
			const float HH = Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 90.f;
			const FVector Loc = Player->GetActorLocation();
			const float Feet = Loc.Z - HH;
			const UCharacterMovementComponent* Move = Player->GetCharacterMovement();
			const bool bWalking = !Player->IsMantling() && Move && Move->IsMovingOnGround();
			const bool bFar = Loc.Y > VaultWallY + VaultWallDepth * 0.5f + R;
			const bool bOnFloor = FMath::Abs(Feet) <= 12.f;
			// Not pushed sideways by anything (the movement component ejecting a capsule that overlapped geometry) and never moved between ticks.
			const bool bStraight = FMath::Abs(Loc.X - ExpectX) <= 6.f && Player->GetVaultMaxDrift() <= 1.f
				&& Player->GetVaultMaxPenetration() <= 1.f && !Player->IsVaultTucked();
			FTransform CamOffset;
			float CamFovOffset = 0.f;
			Player->GetCamera()->GetAdditiveOffset(CamOffset, CamFovOffset);
			const bool bCamRest = CamOffset.Rotator().IsNearlyZero(0.1f) && CamOffset.GetLocation().IsNearlyZero(0.1f);
			const bool bItemRest = Player->GetHeldItemAnchor()->GetRelativeLocation().Equals(FVector(30.f, 15.f, -15.f), 1.f);
			UE_LOG(LogGB, Display, TEXT("SMOKE mantle vault %s feet=%.0f x=%.1f y=%.0f walking=%d far=%d camRest=%d itemRest=%d sim=%.2f maxPenetration=%.2f maxDrift=%.2f"), Tag, Feet, Loc.X, Loc.Y,
				bWalking ? 1 : 0, bFar ? 1 : 0, bCamRest ? 1 : 0, bItemRest ? 1 : 0, World->GetTimeSeconds() - SimMark, Player->GetVaultMaxPenetration(), Player->GetVaultMaxDrift());
			if (!bWalking || !bFar || !bOnFloor || !bStraight || !bCamRest || !bItemRest)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE mantle vault %s FAILED: walking=%d farSide=%d onFloor=%d straight=%d camRest=%d itemRest=%d (feet %.0f, x %.1f expected %.1f, y %.0f, drift %.2f)"), Tag,
					bWalking ? 1 : 0, bFar ? 1 : 0, bOnFloor ? 1 : 0, bStraight ? 1 : 0, bCamRest ? 1 : 0, bItemRest ? 1 : 0, Feet, Loc.X, ExpectX, Loc.Y, Player->GetVaultMaxDrift());
			}
		};
		// Not vaulted: still on the near side of the wall, not vaulting.
		auto VaultCheckStayed = [](AGBPlayerCharacter* Player, const TCHAR* Tag)
		{
			const FVector Loc = Player->GetActorLocation();
			const bool bNear = Loc.Y < VaultWallY - VaultWallDepth * 0.5f;
			UE_LOG(LogGB, Display, TEXT("SMOKE mantle vault %s y=%.0f vaulting=%d near=%d"), Tag, Loc.Y, Player->IsVaulting() ? 1 : 0, bNear ? 1 : 0);
			if (Player->IsVaulting() || !bNear)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE mantle vault %s FAILED: the obstacle must not be vaulted (vaulting=%d, near side=%d)"), Tag, Player->IsVaulting() ? 1 : 0, bNear ? 1 : 0);
			}
		};
		{
			struct FVaultCase { const TCHAR* Place; const TCHAR* Result; const TCHAR* Tag; float Lane; float FeetZ; };
			static const FVaultCase PositiveCases[] = {
				{ TEXT("mantle: vault, 120 cm wall, jump press"), TEXT("mantle: vault, 120 cm wall result"), TEXT("plain"), VaultLaneA, 0.f },
				{ TEXT("mantle: vault, 120 cm wall on a slab, floor 250 cm lower beyond, jump press"), TEXT("mantle: vault, slab drop result"), TEXT("drop"), VaultLaneB, 250.f },
				{ TEXT("mantle: vault, 127 cm wall under a 300 cm ceiling, jump press"), TEXT("mantle: vault, ceiling result"), TEXT("ceiling"), VaultLaneC, 0.f },
			};
			for (int32 Index = 0; Index < UE_ARRAY_COUNT(PositiveCases); ++Index)
			{
				const FVaultCase& Case = PositiveCases[Index];
				const float T = V + 1.0f + Index * 2.5f;
				Add(T, Case.Place, [VaultPlace, &Case](UWorld* World, AGBPlayerCharacter* Player)
				{
					const bool bStarted = VaultPlace(World, Player, Case.Lane, Case.FeetZ, true);
					UE_LOG(LogGB, Display, TEXT("SMOKE mantle vault %s started=%d"), Case.Tag, bStarted ? 1 : 0);
					if (!bStarted)
					{
						UE_LOG(LogGB, Error, TEXT("SMOKE mantle vault %s FAILED: no vault started on jump press next to the wall"), Case.Tag);
					}
				});
				if (Index == 0)
				{
					// Over the top: the view must be posed (tipped, rolled, item lowered), not just the body moved.
					Add(T + 0.3f, TEXT("mantle: vault, pose over the top"), [](UWorld*, AGBPlayerCharacter* Player)
					{
						FTransform CamOffset;
						float CamFovOffset = 0.f;
						Player->GetCamera()->GetAdditiveOffset(CamOffset, CamFovOffset);
						const FRotator CamRel = CamOffset.Rotator();
						const GBMantle::FMantlePose Pose = Player->GetMantlePose();
						const bool bPosed = Player->IsVaulting() && Pose.ItemLower01 > 0.5f && CamRel.Pitch < -3.f && FMath::Abs(CamRel.Roll) > 0.5f;
						UE_LOG(LogGB, Display, TEXT("SMOKE mantle vault pose vaulting=%d alpha=%.2f lower=%.2f grip=%.2f cam=(%.1f,%.1f,%.1f)"),
							Player->IsVaulting() ? 1 : 0, Player->GetMantleAlpha(), Pose.ItemLower01, Pose.Grip01, CamRel.Pitch, CamRel.Yaw, CamRel.Roll);
						if (!bPosed)
						{
							UE_LOG(LogGB, Error, TEXT("SMOKE mantle vault pose FAILED: expected a vault in progress with the item lowered and the view tipped and rolled"));
						}
					}, 0.25f);
				}
				Add(T + 1.6f, Case.Result, [VaultCheckLanded, &Case](UWorld* World, AGBPlayerCharacter* Player) { VaultCheckLanded(World, Player, Case.Tag, Case.Lane); }, 1.45f);
			}
		}
		Add(V + 8.5f, TEXT("mantle: vault, 200 cm wall is not vaulted"), [VaultPlace](UWorld* World, AGBPlayerCharacter* Player)
		{
			VaultPlace(World, Player, VaultLaneD, 0.f, true);
		});
		Add(V + 9.3f, TEXT("mantle: vault, 200 cm wall result"), [VaultCheckStayed](UWorld*, AGBPlayerCharacter* Player) { VaultCheckStayed(Player, TEXT("tall")); }, 0.5f);
		Add(V + 10.0f, TEXT("mantle: vault, no floor within the drop is not vaulted"), [VaultPlace](UWorld* World, AGBPlayerCharacter* Player)
		{
			VaultPlace(World, Player, VaultLaneE, 600.f, true);
		});
		Add(V + 10.8f, TEXT("mantle: vault, no floor result"), [VaultCheckStayed](UWorld*, AGBPlayerCharacter* Player) { VaultCheckStayed(Player, TEXT("nofloor")); }, 0.5f);
		Add(V + 11.5f, TEXT("mantle: vault, standing at the wall without input does nothing"), [VaultPlace](UWorld* World, AGBPlayerCharacter* Player)
		{
			VaultPlace(World, Player, VaultLaneA, 0.f, false);
		});
		Add(V + 12.4f, TEXT("mantle: vault, no input result"), [VaultCheckStayed](UWorld*, AGBPlayerCharacter* Player) { VaultCheckStayed(Player, TEXT("noinput")); }, 0.7f);
		Add(V + 12.6f, TEXT("mantle: vault, push forward against the wall"), [](UWorld* World, AGBPlayerCharacter* Player)
		{
			Player->SetMoveInputForTest(FVector2D(0.f, 1.f));
			MarkSim(World);
		});
		Add(V + 14.4f, TEXT("mantle: vault, push result"), [VaultCheckLanded](UWorld* World, AGBPlayerCharacter* Player) { VaultCheckLanded(World, Player, TEXT("push"), VaultLaneA); }, 1.6f);
		// The railing-like frame of bars with gaps, pushed (no jump press), standing in the gap between two uprights.
		Add(V + 14.8f, TEXT("mantle: vault, frame of thin bars, push"), [VaultPlace](UWorld* World, AGBPlayerCharacter* Player)
		{
			VaultPlace(World, Player, VaultLaneG + 58.f, 0.f, false);
			Player->SetMoveInputForTest(FVector2D(0.f, 1.f));
		});
		Add(V + 16.8f, TEXT("mantle: vault, frame result"), [VaultCheckLanded](UWorld* World, AGBPlayerCharacter* Player) { VaultCheckLanded(World, Player, TEXT("frame"), VaultLaneG + 58.f); }, 1.6f);
		// The same frame with a slanted brace the capsule meets first: the vault must not depend on the hit's normal.
		Add(V + 17.2f, TEXT("mantle: vault, frame with a slanted brace, push"), [VaultPlace](UWorld* World, AGBPlayerCharacter* Player)
		{
			VaultPlace(World, Player, VaultLaneH, 0.f, false);
			Player->SetMoveInputForTest(FVector2D(0.f, 1.f));
		});
		Add(V + 19.2f, TEXT("mantle: vault, brace frame result"), [VaultCheckLanded](UWorld* World, AGBPlayerCharacter* Player) { VaultCheckLanded(World, Player, TEXT("brace"), VaultLaneH); }, 1.6f);
		// ---- The capsule is really shrunk and every step is swept: things that appear or vanish during the move.
		// Not inside anything and not past the blocker: where he ended must be clear of geometry.
		auto VaultCheckClear = [](UWorld* World, AGBPlayerCharacter* Player, const TCHAR* Tag, float MaxY, bool bAllowTucked)
		{
			const UCapsuleComponent* Capsule = Player->GetCapsuleComponent();
			const FVector Loc = Player->GetActorLocation();
			FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(GBSmokeVaultClear), false, Player);
			const bool bInside = Capsule && World->OverlapBlockingTestByChannel(Loc, FQuat::Identity, ECC_Pawn,
				FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight()), QueryParams, FCollisionResponseParams(Capsule->GetCollisionResponseToChannels()));
			const bool bShort = Loc.Y < MaxY;
			UE_LOG(LogGB, Display, TEXT("SMOKE mantle vault %s x=%.1f y=%.0f z=%.0f inside=%d short=%d vaulting=%d tucked=%d"), Tag, Loc.X, Loc.Y, Loc.Z,
				bInside ? 1 : 0, bShort ? 1 : 0, Player->IsVaulting() ? 1 : 0, Player->IsVaultTucked() ? 1 : 0);
			if (bInside || !bShort || Player->IsVaulting() || (Player->IsVaultTucked() && !bAllowTucked))
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE mantle vault %s FAILED: inside geometry=%d, stopped short of y %.0f=%d, still vaulting=%d, still tucked=%d (y %.0f)"), Tag,
					bInside ? 1 : 0, MaxY, bShort ? 1 : 0, Player->IsVaulting() ? 1 : 0, Player->IsVaultTucked() ? 1 : 0, Loc.Y);
			}
		};
		static TWeakObjectPtr<AActor> VaultTemp;
		// A blocking box appears in the path after the vault began (alpha about 0.4): he must end clear of it, short of it.
		Add(V + 19.6f, TEXT("mantle: vault, a blocker appears mid-move: start"), [VaultPlace](UWorld* World, AGBPlayerCharacter* Player) { VaultPlace(World, Player, VaultLaneA, 0.f, true); });
		Add(V + 19.7f, TEXT("mantle: vault, a blocker appears mid-move: the box"), [VaultBox](UWorld* World, AGBPlayerCharacter* Player)
		{
			UE_LOG(LogGB, Display, TEXT("SMOKE mantle vault blocker: vaulting=%d alpha=%.2f"), Player->IsVaulting() ? 1 : 0, Player->GetMantleAlpha());
			VaultBox(World, FVector(VaultLaneA, 1560.f, 150.f), FVector(120.f, 80.f, 300.f));
			VaultTemp = VaultActors.Last();
		}, 0.22f);
		Add(V + 21.4f, TEXT("mantle: vault, a blocker appears mid-move: result"), [VaultCheckClear](UWorld* World, AGBPlayerCharacter* Player)
		{
			VaultCheckClear(World, Player, TEXT("blocker"), 1520.f, true);   // he may still be tucked: the box is still there
			if (VaultTemp.IsValid())
			{
				VaultTemp->Destroy();
			}
		}, 1.6f);
		// The box is gone: the full capsule comes back where it fits.
		Add(V + 21.6f, TEXT("mantle: vault, a blocker appears mid-move: restored"), [VaultCheckClear](UWorld* World, AGBPlayerCharacter* Player) { VaultCheckClear(World, Player, TEXT("blocker restored"), 1520.f, false); }, 0.2f);
		// The landing slab (the floor beyond) is removed before the crossing (alpha about 0.1): he must not cross.
		Add(V + 21.8f, TEXT("mantle: vault, the landing floor vanishes: slab"), [VaultBox](UWorld* World, AGBPlayerCharacter*)
		{
			VaultBox(World, FVector(VaultLaneA, 1600.f, 30.f), FVector(200.f, 200.f, 60.f));
			VaultTemp = VaultActors.Last();
		});
		Add(V + 22.0f, TEXT("mantle: vault, the landing floor vanishes: start"), [VaultPlace](UWorld* World, AGBPlayerCharacter* Player)
		{
			const bool bStarted = VaultPlace(World, Player, VaultLaneA, 0.f, true);
			UE_LOG(LogGB, Display, TEXT("SMOKE mantle vault slab started=%d"), bStarted ? 1 : 0);
			if (!bStarted)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE mantle vault slab FAILED: no vault started towards a landing slab 60 cm high"));
			}
		});
		Add(V + 22.1f, TEXT("mantle: vault, the landing floor vanishes: remove"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			UE_LOG(LogGB, Display, TEXT("SMOKE mantle vault slab removal: vaulting=%d alpha=%.2f"), Player->IsVaulting() ? 1 : 0, Player->GetMantleAlpha());
			if (VaultTemp.IsValid())
			{
				VaultTemp->Destroy();
			}
		}, 0.05f);
		Add(V + 23.6f, TEXT("mantle: vault, the landing floor vanishes: result"), [VaultCheckClear](UWorld* World, AGBPlayerCharacter* Player) { VaultCheckClear(World, Player, TEXT("slab"), 1495.f, false); }, 1.2f);
		// A long frame (0.5 s in one step) with a thin plate across the crossing that the end position does not touch:
		// without the swept steps he would jump across it.
		Add(V + 24.0f, TEXT("mantle: vault, a long frame across a thin plate"), [VaultPlace, VaultBox](UWorld* World, AGBPlayerCharacter* Player)
		{
			const bool bStarted = VaultPlace(World, Player, VaultLaneA, 0.f, true);
			VaultBox(World, FVector(VaultLaneA, 1508.f, 150.f), FVector(200.f, 2.f, 300.f));
			VaultTemp = VaultActors.Last();
			Player->AdvanceVaultForTest(0.5f);
			UE_LOG(LogGB, Display, TEXT("SMOKE mantle vault longframe started=%d vaultingAfterLongTick=%d"), bStarted ? 1 : 0, Player->IsVaulting() ? 1 : 0);
			if (!bStarted)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE mantle vault longframe FAILED: no vault started"));
			}
		});
		Add(V + 25.6f, TEXT("mantle: vault, a long frame result"), [VaultCheckClear](UWorld* World, AGBPlayerCharacter* Player)
		{
			VaultCheckClear(World, Player, TEXT("longframe"), 1507.f, true);
			if (VaultTemp.IsValid())
			{
				VaultTemp->Destroy();
			}
		}, 1.2f);
		// Waist-high cover, thick (a crate 100 x 100 x 110): leaning on it with forward input must not vault it.
		Add(V + 26.0f, TEXT("mantle: vault, a thick crate is not pushed over"), [VaultPlace, VaultBox](UWorld* World, AGBPlayerCharacter* Player)
		{
			VaultBox(World, FVector(VaultLaneB, 1900.f, 55.f), FVector(100.f, 100.f, 110.f));
			VaultTemp = VaultActors.Last();
			VaultPlace(World, Player, VaultLaneB, 0.f, false, 1850.f);
			Player->SetMoveInputForTest(FVector2D(0.f, 1.f));
		});
		Add(V + 27.4f, TEXT("mantle: vault, a thick crate result"), [VaultCheckStayed](UWorld*, AGBPlayerCharacter* Player)
		{
			const FVector Loc = Player->GetActorLocation();
			const bool bNear = Loc.Y < 1850.f;
			UE_LOG(LogGB, Display, TEXT("SMOKE mantle vault crate y=%.0f vaulting=%d near=%d"), Loc.Y, Player->IsVaulting() ? 1 : 0, bNear ? 1 : 0);
			if (Player->IsVaulting() || !bNear)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE mantle vault crate FAILED: a 100 cm deep crate must not be vaulted by pushing (y %.0f)"), Loc.Y);
			}
			if (VaultTemp.IsValid())
			{
				VaultTemp->Destroy();
			}
			(void)VaultCheckStayed;
		}, 1.2f);
		Add(V + 27.8f, TEXT("mantle: vault, clean up"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			for (const TWeakObjectPtr<AActor>& Box : VaultActors)
			{
				if (Box.IsValid())
				{
					Box->Destroy();
				}
			}
			VaultActors.Reset();
			Player->ClearMoveInputForTest();
			FVector EnemySpawn;
			FVector PlayerSpot;
			AGBTestArena::GetPathTestPoints(EnemySpawn, PlayerSpot);
			if (AController* Controller = Player->GetController())
			{
				Controller->SetControlRotation(FRotator(0.f, 0.f, 0.f));
			}
			Player->SetActorLocation(PlayerSpot, false, nullptr, ETeleportType::TeleportPhysics);
			if (UCharacterMovementComponent* Move = Player->GetCharacterMovement())
			{
				Move->SetMovementMode(MOVE_Falling);
				Move->Velocity = FVector::ZeroVector;
			}
		});
		// A filtered run (gb.Smoke mantle) has no other block to end it: it quits here. The full run goes on.
		Add(V + 28.0f, TEXT("mantle: vault, done"), [](UWorld*, AGBPlayerCharacter*)
		{
			if (bFilteredRun)
			{
				UE_LOG(LogGB, Display, TEXT("SMOKE DONE"));
				FPlatformMisc::RequestExit(false);
			}
		});
		// ---- Enemies3D block (gb.Smoke Enemies3D): three groups, each: spawn, check (moves, attacks, hover), kill, corpses gone.
		const float E3 = U + 43.2f + VaultSpan;
		const float E3Group = 23.0f;      // the longest group: 16 s to its check, 6.2 s to kill it and see its corpses go
		{
			using FItems = TArray<TPair<FName, float>>;
			auto Items = [](std::initializer_list<TPair<FName, float>> List) { return FItems(List); };
			struct FGroup { const TCHAR* Name; FItems Spawn; float Check; };
			const FGroup Groups[] = {
				{ TEXT("shooters"), Items({ { FName(TEXT("Robot_Future")), 1700.f }, { FName(TEXT("Orc_Future")), 1700.f }, { FName(TEXT("Orc_Future_Enforcer")), 1700.f }, { FName(TEXT("Orc_Future_GunCarrier")), 1300.f } }), 16.0f },
				{ TEXT("flyers"), Items({ { FName(TEXT("Drone_Future")), 1500.f }, { FName(TEXT("Sentinel_Future")), 2300.f }, { FName(TEXT("Valkyrie_Future")), 1500.f } }), 5.5f },
				{ TEXT("melee"), Items({ { FName(TEXT("Spider_Future")), 900.f }, { FName(TEXT("Orc_Future_Siege")), 450.f } }), 5.f },
			};
			for (int32 G = 0; G < UE_ARRAY_COUNT(Groups); ++G)
			{
				const float T = E3 + G * E3Group;
				const FGroup Group = Groups[G];
				const FString StepSpawn = FString::Printf(TEXT("Enemies3D: %s spawn"), Group.Name);
				// The step names must outlive BuildSteps: keep them in a static pool.
				static TArray<TSharedPtr<FString>> NamePool;
				auto Keep = [](const FString& Text) -> const TCHAR* { NamePool.Add(MakeShared<FString>(Text)); return **NamePool.Last(); };
				Add(T, Keep(StepSpawn), [Group](UWorld* World, AGBPlayerCharacter* Player)
				{
					if (AGBPlayerController* PC = Cast<AGBPlayerController>(Player->GetController()))
					{
						PC->SetControlRotation(FRotator::ZeroRotator);
					}
					FVector EnemySpawn;
					FVector PlayerSpot;
					AGBTestArena::GetPathTestPoints(EnemySpawn, PlayerSpot);
					Player->SetActorLocation(PlayerSpot, false, nullptr, ETeleportType::TeleportPhysics);
					E3DSpawnGroup(World, Player, Group.Spawn);
				});
				// Half way, a heal: a strong group (the Valkyrie) must not kill the player before the check and the screenshot.
				Add(T + Group.Check * 0.5f, Keep(FString::Printf(TEXT("Enemies3D: %s heal"), Group.Name)), [](UWorld*, AGBPlayerCharacter* Player) { Player->GetAttributes()->RestoreAll(); });
				Add(T + Group.Check, Keep(FString::Printf(TEXT("Enemies3D: %s check"), Group.Name)), [Group](UWorld* World, AGBPlayerCharacter* Player)
				{
					E3DCheckGroup(World, Player, Group.Name);
				});
				Add(T + Group.Check + 0.1f, Keep(FString::Printf(TEXT("Enemies3D: %s screenshot"), Group.Name)), [Group](UWorld*, AGBPlayerCharacter*)
				{
					Shot(*FString::Printf(TEXT("enemies3d_%s"), Group.Name));
				});
				if (G == 0)
				{
					Add(T + Group.Check + 0.3f, TEXT("Enemies3D: closeup of an armored orc"), [](UWorld*, AGBPlayerCharacter* Player) { E3DCloseup(Player, FName(TEXT("Orc_Future_Enforcer"))); });
					Add(T + Group.Check + 0.9f, TEXT("Enemies3D: closeup screenshot"), [](UWorld*, AGBPlayerCharacter*) { Shot(TEXT("enemies3d_enforcer_closeup")); });
				}
				Add(T + Group.Check + 1.8f, Keep(FString::Printf(TEXT("Enemies3D: %s kill"), Group.Name)), [](UWorld*, AGBPlayerCharacter* Player) { E3DKillGroup(Player); });
				// Two looks at the deaths: just after the kill (the fall, the first ground contact) and a moment later (the body on the floor, the wreck).
				Add(T + Group.Check + 2.7f, Keep(FString::Printf(TEXT("Enemies3D: %s death screenshot 1"), Group.Name)), [Group](UWorld*, AGBPlayerCharacter*) { Shot(*FString::Printf(TEXT("enemies3d_%s_death1"), Group.Name)); });
				Add(T + Group.Check + 3.4f, Keep(FString::Printf(TEXT("Enemies3D: %s death closeup"), Group.Name)), [Group](UWorld*, AGBPlayerCharacter* Player) { E3DCloseup(Player, Group.Spawn[0].Key); });
				Add(T + Group.Check + 4.0f, Keep(FString::Printf(TEXT("Enemies3D: %s death screenshot 2"), Group.Name)), [Group](UWorld*, AGBPlayerCharacter*) { Shot(*FString::Printf(TEXT("enemies3d_%s_death2"), Group.Name)); });
				Add(T + Group.Check + 6.2f, Keep(FString::Printf(TEXT("Enemies3D: %s corpses removed"), Group.Name)), [Group](UWorld*, AGBPlayerCharacter*) { E3DCheckCorpses(Group.Name); });
			}
		}
		// ---- tactics: six shooters in an ad-hoc squad round blocking boxes
		const float E3T = E3 + 3.f * E3Group;
		Add(E3T, TEXT("Enemies3D: tactics boxes"), [](UWorld* World, AGBPlayerCharacter* Player)
		{
			if (AGBPlayerController* PC = Cast<AGBPlayerController>(Player->GetController()))
			{
				PC->SetControlRotation(FRotator::ZeroRotator);
			}
			FVector EnemySpawn;
			FVector PlayerSpot;
			AGBTestArena::GetPathTestPoints(EnemySpawn, PlayerSpot);
			Player->SetActorLocation(PlayerSpot, false, nullptr, ETeleportType::TeleportPhysics);
			E3TSpawnBoxes(World, Player);
		});
		// The boxes cut the navmesh once it has rebuilt, and in a long run that takes longer than a fixed wait: the squad plans its
		// cover on the navmesh, so it is only spawned when the build is over (at most 4 s of waiting; the wait is logged).
		Add(E3T + 3.5f, TEXT("Enemies3D: tactics spawn"), [](UWorld* World, AGBPlayerCharacter* Player)
		{
			E3TNavWaited = 0.f;
			FTSTicker::GetCoreTicker().RemoveTicker(E3TSpawnTicker);
			E3TSpawnTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakWorld = TWeakObjectPtr<UWorld>(World), WeakPlayer = TWeakObjectPtr<AGBPlayerCharacter>(Player)](float Dt)
			{
				UWorld* TickWorld = WeakWorld.Get();
				AGBPlayerCharacter* TickPlayer = WeakPlayer.Get();
				if (!TickWorld || !TickPlayer)
				{
					return false;
				}
				E3TNavWaited += Dt;
				if (UNavigationSystemV1::IsNavigationBeingBuilt(TickWorld) && E3TNavWaited < 4.f)
				{
					return true;
				}
				UE_LOG(LogGB, Display, TEXT("SMOKE Enemies3D tactics: the navmesh was ready after %.1f s (building=%d), spawning the squad"), E3TNavWaited, UNavigationSystemV1::IsNavigationBeingBuilt(TickWorld) ? 1 : 0);
				using FItems = TArray<TPair<FName, float>>;
				const FItems Items = {
					{ FName(TEXT("Robot_Future")), 1500.f }, { FName(TEXT("Robot_Future")), 1700.f }, { FName(TEXT("Robot_Future")), 1300.f }, { FName(TEXT("Robot_Future")), 1600.f },
					{ FName(TEXT("Orc_Future")), 1500.f }, { FName(TEXT("Orc_Future")), 1400.f } };
				E3DSpawnGroup(TickWorld, TickPlayer, Items, true);
				TickPlayer->GetAttributes()->SetInvulnerable(true);
				E3TStartSampling(TickWorld);
				return false;
			}), 0.1f);
		});
		Add(E3T + 16.f, TEXT("Enemies3D: tactics check"), [](UWorld* World, AGBPlayerCharacter* Player) { E3TCheck(World, Player); });
		Add(E3T + 16.2f, TEXT("Enemies3D: tactics fly up"), [](UWorld*, AGBPlayerCharacter* Player) { E3TTop(Player); });
		Add(E3T + 16.9f, TEXT("Enemies3D: tactics top screenshot"), [](UWorld*, AGBPlayerCharacter*) { Shot(TEXT("enemies3d_tactics_top")); });
		Add(E3T + 17.2f, TEXT("Enemies3D: tactics back down"), [](UWorld*, AGBPlayerCharacter* Player) { E3TBack(Player); });
		Add(E3T + 17.9f, TEXT("Enemies3D: tactics eyes screenshot"), [](UWorld*, AGBPlayerCharacter*) { Shot(TEXT("enemies3d_tactics_eyes")); });
		Add(E3T + 18.3f, TEXT("Enemies3D: tactics kill"), [](UWorld*, AGBPlayerCharacter* Player) { E3DKillGroup(Player); E3TClean(); });
		Add(E3T + 24.5f, TEXT("Enemies3D: tactics corpses removed"), [](UWorld*, AGBPlayerCharacter*) { E3DCheckCorpses(TEXT("tactics")); });

		// ---- chief: the shielded horde chief
		const float E3C = E3T + 27.f;
		Add(E3C, TEXT("Enemies3D: chief spawn"), [](UWorld* World, AGBPlayerCharacter* Player)
		{
			if (AGBPlayerController* PC = Cast<AGBPlayerController>(Player->GetController()))
			{
				PC->SetControlRotation(FRotator::ZeroRotator);
			}
			FVector EnemySpawn;
			FVector PlayerSpot;
			AGBTestArena::GetPathTestPoints(EnemySpawn, PlayerSpot);
			Player->SetActorLocation(PlayerSpot, false, nullptr, ETeleportType::TeleportPhysics);
			using FItems = TArray<TPair<FName, float>>;
			const FItems Items = { { FName(TEXT("Orc_Future_Warlord")), 1300.f } };
			E3DSpawnGroup(World, Player, Items);
			Player->GetAttributes()->SetInvulnerable(true);
		});
		Add(E3C + 1.2f, TEXT("Enemies3D: chief closeup"), [](UWorld*, AGBPlayerCharacter* Player) { E3DCloseup(Player, FName(TEXT("Orc_Future_Warlord"))); E3CStartShots(); });
		Add(E3C + 10.f, TEXT("Enemies3D: chief block test"), [](UWorld* World, AGBPlayerCharacter* Player) { E3CBlockTest(Player); E3CShieldTests(World, Player); });
		Add(E3C + 19.f, TEXT("Enemies3D: chief check"), [](UWorld*, AGBPlayerCharacter*) { E3CCheck(); });
		Add(E3C + 19.5f, TEXT("Enemies3D: chief kill"), [](UWorld*, AGBPlayerCharacter* Player) { E3DKillGroup(Player); });
		Add(E3C + 25.5f, TEXT("Enemies3D: chief corpses removed"), [](UWorld*, AGBPlayerCharacter*) { E3DCheckCorpses(TEXT("chief")); });

		// ---- poses: a robot on patrol (relaxed idle / patrol walk) and an orc in the crouch; closeups to look at
		const float E3P = E3C + 29.f;
		Add(E3P, TEXT("Enemies3D: poses spawn"), [](UWorld* World, AGBPlayerCharacter* Player)
		{
			if (AGBPlayerController* PC = Cast<AGBPlayerController>(Player->GetController()))
			{
				PC->SetControlRotation(FRotator::ZeroRotator);
			}
			FVector EnemySpawn;
			FVector PlayerSpot;
			AGBTestArena::GetPathTestPoints(EnemySpawn, PlayerSpot);
			Player->SetActorLocation(PlayerSpot, false, nullptr, ETeleportType::TeleportPhysics);
			E3PSpawn(World, Player);
		});
		Add(E3P + 3.f, TEXT("Enemies3D: poses patrol closeup"), [](UWorld*, AGBPlayerCharacter* Player) { E3DCloseup(Player, FName(TEXT("Robot_Future"))); });
		Add(E3P + 3.6f, TEXT("Enemies3D: poses patrol screenshot"), [](UWorld*, AGBPlayerCharacter*) { Shot(TEXT("enemies3d_pose_patrol")); });
		Add(E3P + 4.2f, TEXT("Enemies3D: poses crouch closeup"), [](UWorld*, AGBPlayerCharacter* Player) { E3DCloseup(Player, FName(TEXT("Orc_Future"))); });
		Add(E3P + 4.8f, TEXT("Enemies3D: poses crouch screenshot"), [](UWorld*, AGBPlayerCharacter*) { Shot(TEXT("enemies3d_pose_crouch")); });
		Add(E3P + 5.4f, TEXT("Enemies3D: poses kill"), [](UWorld*, AGBPlayerCharacter* Player) { E3DKillGroup(Player); });
		// ---- swarm: four spiders on the player
		const float E3S = E3P + 12.f;
		Add(E3S, TEXT("Enemies3D: swarm spawn"), [](UWorld* World, AGBPlayerCharacter* Player)
		{
			if (AGBPlayerController* PC = Cast<AGBPlayerController>(Player->GetController()))
			{
				PC->SetControlRotation(FRotator::ZeroRotator);
			}
			FVector EnemySpawn;
			FVector PlayerSpot;
			AGBTestArena::GetPathTestPoints(EnemySpawn, PlayerSpot);
			Player->SetActorLocation(PlayerSpot, false, nullptr, ETeleportType::TeleportPhysics);
			using FItems = TArray<TPair<FName, float>>;
			const FItems Items = { { FName(TEXT("Spider_Future")), 800.f }, { FName(TEXT("Spider_Future")), 850.f }, { FName(TEXT("Spider_Future")), 900.f }, { FName(TEXT("Spider_Future")), 950.f } };
			E3DSpawnGroup(World, Player, Items);
			Player->GetAttributes()->SetInvulnerable(true);
			E3WStart(World);
		});
		Add(E3S + 9.f, TEXT("Enemies3D: swarm check"), [](UWorld*, AGBPlayerCharacter*) { E3WCheck(); });
		Add(E3S + 9.5f, TEXT("Enemies3D: swarm kill"), [](UWorld*, AGBPlayerCharacter* Player) { E3DKillGroup(Player); });
		Add(E3S + 15.f, TEXT("Enemies3D: swarm corpses removed"), [](UWorld*, AGBPlayerCharacter*) { E3DCheckCorpses(TEXT("swarm")); });
		// ---- chief poses (run alone with -Only Chief): four closeups, raised and lowered, front at eye height and from his left
		// ---- sight: an enemy facing AWAY must not notice him by sight; one facing him must (encounterless, so they stand where they are)
		const float E3G = E3S + 17.f;
		Add(E3G, TEXT("Enemies3D: sight away spawn"), [](UWorld* World, AGBPlayerCharacter* Player)
		{
			if (AGBPlayerController* PC = Cast<AGBPlayerController>(Player->GetController()))
			{
				PC->SetControlRotation(FRotator::ZeroRotator);
			}
			FVector EnemySpawn;
			FVector PlayerSpot;
			AGBTestArena::GetPathTestPoints(EnemySpawn, PlayerSpot);
			Player->SetActorLocation(PlayerSpot, false, nullptr, ETeleportType::TeleportPhysics);
			using FItems = TArray<TPair<FName, float>>;
			const FItems Items = { { FName(TEXT("Robot_Future")), 900.f } };
			E3DSpawnGroup(World, Player, Items);
			Player->GetAttributes()->SetInvulnerable(true);
			for (const FE3DEntry& Entry : E3DGroup)
			{
				if (AGBEnemyCharacter* Enemy = Entry.Enemy.Get())
				{
					// Turned with its back to the player, clear line, in range: only the sight cone keeps it from noticing.
					Enemy->SetActorRotation(FRotator(0.f, (Enemy->GetActorLocation() - Player->GetActorLocation()).Rotation().Yaw, 0.f));
				}
			}
		});
		Add(E3G + 3.f, TEXT("Enemies3D: sight away check"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			for (const FE3DEntry& Entry : E3DGroup)
			{
				if (const AGBEnemyCharacter* Enemy = Entry.Enemy.Get())
				{
					UE_LOG(LogGB, Display, TEXT("SMOKE Enemies3D sight: the robot facing away, 3 s in range with a clear line: aware=%d"), Enemy->IsAware() ? 1 : 0);
					if (Enemy->IsAware())
					{
						UE_LOG(LogGB, Error, TEXT("SMOKE Enemies3D sight FAILED: an enemy facing away noticed the player by sight (everyone sees through the back of the head)"));
					}
				}
			}
			E3DKillGroup(Player);
		});
		Add(E3G + 4.f, TEXT("Enemies3D: sight facing spawn"), [](UWorld* World, AGBPlayerCharacter* Player)
		{
			using FItems = TArray<TPair<FName, float>>;
			const FItems Items = { { FName(TEXT("Robot_Future")), 900.f } };
			E3DSpawnGroup(World, Player, Items);
			Player->GetAttributes()->SetInvulnerable(true);
		});
		Add(E3G + 7.f, TEXT("Enemies3D: sight facing check"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			for (const FE3DEntry& Entry : E3DGroup)
			{
				if (const AGBEnemyCharacter* Enemy = Entry.Enemy.Get())
				{
					UE_LOG(LogGB, Display, TEXT("SMOKE Enemies3D sight: the robot facing him, 3 s in range with a clear line: aware=%d"), Enemy->IsAware() ? 1 : 0);
					if (!Enemy->IsAware())
					{
						UE_LOG(LogGB, Error, TEXT("SMOKE Enemies3D sight FAILED: an enemy facing the player in range with a clear line did not notice him"));
					}
				}
			}
			E3DKillGroup(Player);
		});
		const float E3H = E3G + 14.f;
		Add(E3H, TEXT("Chief: spawn"), [](UWorld* World, AGBPlayerCharacter* Player)
		{
			if (AGBPlayerController* PC = Cast<AGBPlayerController>(Player->GetController()))
			{
				PC->SetControlRotation(FRotator::ZeroRotator);
			}
			FVector EnemySpawn;
			FVector PlayerSpot;
			AGBTestArena::GetPathTestPoints(EnemySpawn, PlayerSpot);
			Player->SetActorLocation(PlayerSpot, false, nullptr, ETeleportType::TeleportPhysics);
			using FItems = TArray<TPair<FName, float>>;
			const FItems Items = { { FName(TEXT("Orc_Future_Warlord")), 1300.f } };
			E3DSpawnGroup(World, Player, Items);
			Player->GetAttributes()->SetInvulnerable(true);
		});
		Add(E3H + 5.f, TEXT("Chief: raised front"), [](UWorld*, AGBPlayerCharacter* Player) { if (AGBShieldEnemy* Chief = E3CFindChief()) { Chief->DebugHold(1); } E3ChiefView(Player, true); });
		Add(E3H + 9.f, TEXT("Chief: raised front shot"), [](UWorld*, AGBPlayerCharacter*) { Shot(TEXT("chief_raised_front")); });
		Add(E3H + 10.f, TEXT("Chief: raised left"), [](UWorld*, AGBPlayerCharacter* Player) { E3ChiefView(Player, false); });
		Add(E3H + 13.f, TEXT("Chief: raised left shot"), [](UWorld*, AGBPlayerCharacter*) { Shot(TEXT("chief_raised_left")); });
		Add(E3H + 14.f, TEXT("Chief: lowered front"), [](UWorld*, AGBPlayerCharacter* Player) { if (AGBShieldEnemy* Chief = E3CFindChief()) { Chief->DebugHold(2); } E3ChiefView(Player, true); });
		Add(E3H + 17.f, TEXT("Chief: lowered front shot"), [](UWorld*, AGBPlayerCharacter*) { Shot(TEXT("chief_lowered_front")); });
		Add(E3H + 18.f, TEXT("Chief: lowered left"), [](UWorld*, AGBPlayerCharacter* Player) { E3ChiefView(Player, false); });
		Add(E3H + 21.f, TEXT("Chief: lowered left shot"), [](UWorld*, AGBPlayerCharacter*) { Shot(TEXT("chief_lowered_left")); });
		Add(E3H + 22.f, TEXT("Chief: bash front"), [](UWorld*, AGBPlayerCharacter* Player) { if (AGBShieldEnemy* Chief = E3CFindChief()) { Chief->DebugHold(3); } E3ChiefView(Player, true); });
		Add(E3H + 25.f, TEXT("Chief: bash front shot"), [](UWorld*, AGBPlayerCharacter*) { Shot(TEXT("chief_bash_front")); });
		Add(E3H + 26.f, TEXT("Chief: bash left"), [](UWorld*, AGBPlayerCharacter* Player) { E3ChiefView(Player, false); });
		Add(E3H + 29.f, TEXT("Chief: bash left shot"), [](UWorld*, AGBPlayerCharacter*) { Shot(TEXT("chief_bash_left")); });
		E3PoseTryLoad();
		for (int32 Try = 0; Try < E3PoseTries.Num() && Try < 8; ++Try)
		{
			static TArray<TSharedPtr<FString>> TryPool;
			auto TryName = [](const FString& Text) -> const TCHAR* { TryPool.Add(MakeShared<FString>(Text)); return **TryPool.Last(); };
			Add(E3H + 30.f + 5.f * Try, TryName(FString::Printf(TEXT("Chief: pose try %d"), Try)), [Try](UWorld*, AGBPlayerCharacter* Player) { E3PoseTry(Player, Try); });
			Add(E3H + 32.5f + 5.f * Try, TryName(FString::Printf(TEXT("Chief: pose try %d shot"), Try)), [Try](UWorld*, AGBPlayerCharacter*) { Shot(*FString::Printf(TEXT("chief_try_%d"), Try)); });
		}
		const float E3Tries = 5.f * FMath::Min(E3PoseTries.Num(), 8);
		Add(E3H + 30.f + E3Tries, TEXT("Chief: kill"), [](UWorld*, AGBPlayerCharacter* Player) { if (AGBShieldEnemy* Chief = E3CFindChief()) { Chief->DebugHold(0); } E3DKillGroup(Player); });
		Add(E3H + 40.f + E3Tries, TEXT("Chief: done"), [](UWorld*, AGBPlayerCharacter*)
		{
			if (bFilteredRun)
			{
				UE_LOG(LogGB, Display, TEXT("SMOKE DONE"));
				FPlatformMisc::RequestExit(false);
			}
		});
		const float E3End = E3H + 41.f + E3Tries;   // the chief's corpse is watched for its clip, its lie and its sinking after the kill
		// A filtered run (gb.Smoke Enemies3D) has no seal block to end it: it quits here. The full run goes on.
		Add(E3End, TEXT("Enemies3D: done"), [](UWorld*, AGBPlayerCharacter*)
		{
			if (bFilteredRun)
			{
				UE_LOG(LogGB, Display, TEXT("SMOKE DONE"));
				FPlatformMisc::RequestExit(false);
			}
		});
		// ---- Valkyrie block (gb.Smoke Valkyrie): her fight. Sealed then woken, bursts at range, her own combo, Judgement, lances, phase two, death.
		// Her own choice is switched off (DebugAutoMask 0) except in the combo step, so every forced action starts when the step says.
		const float VK = E3End + 0.6f;
		{
			static TWeakObjectPtr<AGBValkyrieEnemy> Valk;
			static int32 Base[9];
			auto Snapshot = []()
			{
				if (const AGBValkyrieEnemy* Vk = Valk.Get())
				{
					Base[0] = Vk->GetSwingCount(); Base[1] = Vk->GetSwingHits(); Base[2] = Vk->GetJudgementCasts(); Base[3] = Vk->GetCirclesMade();
					Base[4] = Vk->GetCirclesStruck(); Base[5] = Vk->GetCircleHits(); Base[6] = Vk->GetLancesFired(); Base[7] = Vk->GetShotCount(); Base[8] = Vk->GetActionClipsPlayed();
				}
			};
			auto Face = [](AGBPlayerCharacter* Player, float Pitch)
			{
				const AGBValkyrieEnemy* Vk = Valk.Get();
				if (Vk && Player)
				{
					FRotator Look = (Vk->GetActorLocation() + FVector(0.f, 0.f, 40.f) - Player->GetAimOrigin()).Rotation();
					Look.Pitch = Pitch;
					if (AGBPlayerController* PC = Cast<AGBPlayerController>(Player->GetController()))
					{
						PC->SetControlRotation(Look);
					}
				}
			};
			Add(VK, TEXT("Valkyrie: spawn sealed"), [Snapshot](UWorld* World, AGBPlayerCharacter* Player)
			{
				FVector EnemySpawn;
				FVector PlayerSpot;
				AGBTestArena::GetPathTestPoints(EnemySpawn, PlayerSpot);
				Player->SetActorLocation(PlayerSpot, false, nullptr, ETeleportType::TeleportPhysics);
				E3DSpawnGroup(World, Player, { { FName(TEXT("Valkyrie_Future")), 1400.f } });
				Player->GetAttributes()->SetInvulnerable(true);
				Valk = nullptr;
				for (const FE3DEntry& Entry : E3DGroup)
				{
					Valk = Cast<AGBValkyrieEnemy>(Entry.Enemy.Get());
				}
				if (AGBValkyrieEnemy* Vk = Valk.Get())
				{
					Vk->DebugAutoMask(0);
					Vk->SetSealed(true);
					UE_LOG(LogGB, Display, TEXT("SMOKE Valkyrie spawned: class=%s look=%d grip=%d sealed=%d | %s"), *Vk->GetClass()->GetName(), Vk->HasSkeletalLook() ? 1 : 0, Vk->HasGripCurl() ? 1 : 0, Vk->IsSealed() ? 1 : 0, *Vk->DescribeLook());
					if (const UGBEnemyAnimInstance* Anim = Vk->GetEnemyAnim())
					{
						FString Lengths;
						for (int32 Index = 0; Index < 6; ++Index)
						{
							const UAnimSequence* Clip = Anim->GetClip(static_cast<EGBEnemyClip>(static_cast<int32>(EGBEnemyClip::Action0) + Index));
							Lengths += FString::Printf(TEXT(" Action%d=%.2f"), Index, Clip ? Clip->GetPlayLength() : -1.f);
							if (!Clip && Index < 5 && Vk->HasSkeletalLook())
							{
								UE_LOG(LogGB, Error, TEXT("SMOKE Valkyrie FAILED: clip Anim.Action%d did not load (the fight would run with no animation)"), Index);
							}
						}
						UE_LOG(LogGB, Display, TEXT("SMOKE Valkyrie action clip lengths (s):%s"), *Lengths);
					}
				}
				else
				{
					UE_LOG(LogGB, Error, TEXT("SMOKE Valkyrie FAILED: Valkyrie_Future did not spawn as an AGBValkyrieEnemy"));
				}
				Snapshot();
			});
			Add(VK + 0.5f, TEXT("Valkyrie: wake"), [](UWorld*, AGBPlayerCharacter* Player)
			{
				if (AGBValkyrieEnemy* Vk = Valk.Get())
				{
					const FVector At = Vk->GetActorLocation() + (Player->GetActorLocation() - Vk->GetActorLocation()).GetSafeNormal2D() * 300.f;
					FVector Landing = At;
					Vk->ProjectPointToNavigationForWake(Landing);
					Vk->BeginWake(Landing);
					UE_LOG(LogGB, Display, TEXT("SMOKE Valkyrie wake: sealed=%d leaping=%d"), Vk->IsSealed() ? 1 : 0, Vk->IsWakeLeaping() ? 1 : 0);
				}
			});
			Add(VK + 0.9f, TEXT("Valkyrie: look at her"), [Face](UWorld*, AGBPlayerCharacter* Player) { Face(Player, 0.f); });
			Add(VK + 6.5f, TEXT("Valkyrie: far check"), [Snapshot](UWorld*, AGBPlayerCharacter*)
			{
				const AGBValkyrieEnemy* Vk = Valk.Get();
				if (!Vk)
				{
					UE_LOG(LogGB, Error, TEXT("SMOKE Valkyrie far FAILED: she is gone"));
					return;
				}
				UE_LOG(LogGB, Display, TEXT("SMOKE Valkyrie far: shots=%d bursts=%d aware=%d sealed=%d leaping=%d act=%d | %s"), Vk->GetShotCount(), Vk->GetBurstCount(), Vk->IsAware() ? 1 : 0, Vk->IsSealed() ? 1 : 0, Vk->IsWakeLeaping() ? 1 : 0, static_cast<int32>(Vk->GetAct()), *Vk->DescribeTactics());
				if (Vk->IsSealed() || Vk->GetShotCount() <= 0)
				{
					UE_LOG(LogGB, Error, TEXT("SMOKE Valkyrie far FAILED: after the wake she never fired her light bolts (sealed=%d shots=%d)"), Vk->IsSealed() ? 1 : 0, Vk->GetShotCount());
				}
				Snapshot();
			});
			Add(VK + 6.7f, TEXT("Valkyrie: close, her own combo"), [Snapshot](UWorld*, AGBPlayerCharacter* Player)
			{
				E3DCloseup(Player, FName(TEXT("Valkyrie_Future")));
				if (AGBValkyrieEnemy* Vk = Valk.Get())
				{
					Snapshot();
					Vk->DebugAutoMask(1 << static_cast<int32>(AGBValkyrieEnemy::EKind::Combo));
				}
			});
			Add(VK + 8.4f, TEXT("Valkyrie: combo screenshot"), [](UWorld*, AGBPlayerCharacter*) { Shot(TEXT("valkyrie_combo")); });
			Add(VK + 11.7f, TEXT("Valkyrie: combo check"), [](UWorld*, AGBPlayerCharacter*)
			{
				AGBValkyrieEnemy* Vk = Valk.Get();
				if (!Vk)
				{
					return;
				}
				Vk->DebugAutoMask(0);
				const int32 Swings = Vk->GetSwingCount() - Base[0];
				const int32 Hits = Vk->GetSwingHits() - Base[1];
				const int32 Clips = Vk->GetActionClipsPlayed() - Base[8];
				UE_LOG(LogGB, Display, TEXT("SMOKE Valkyrie combo: swings=%d hits=%d clips=%d hops=%d dashes=%d | %s"), Swings, Hits, Clips, Vk->GetHopCount(), Vk->GetDashCount(), *Vk->DescribeTactics());
				if (Swings < 2 || Hits < 1)
				{
					UE_LOG(LogGB, Error, TEXT("SMOKE Valkyrie combo FAILED: %d swings and %d hits on a player standing 400 cm from her (wanted 2 and 1, started by her own choice)"), Swings, Hits);
				}
				if (Vk->HasSkeletalLook() && Clips < Swings)
				{
					UE_LOG(LogGB, Error, TEXT("SMOKE Valkyrie combo FAILED: %d swings but only %d swing clips played"), Swings, Clips);
				}
			});
			Add(VK + 11.9f, TEXT("Valkyrie: hand closeup"), [](UWorld*, AGBPlayerCharacter* Player)
			{
				AGBValkyrieEnemy* Vk = Valk.Get();
				if (Vk && Vk->GetMesh() && Player)
				{
					Vk->DebugFreeze(true);
					const FVector Hand = Vk->GetMesh()->GetBoneLocation(FName(TEXT("hand_r")), EBoneSpaces::WorldSpace);
					const FVector Side = (Player->GetActorLocation() - Hand).GetSafeNormal2D();
					Player->SetActorLocation(Hand + Side * 90.f - FVector(0.f, 0.f, 70.f), false, nullptr, ETeleportType::TeleportPhysics);
					if (AGBPlayerController* PC = Cast<AGBPlayerController>(Player->GetController()))
					{
						PC->SetControlRotation((Hand - Player->GetAimOrigin()).Rotation());
					}
				}
			});
			Add(VK + 12.5f, TEXT("Valkyrie: hand screenshot"), [](UWorld*, AGBPlayerCharacter* Player)
			{
				if (const AGBValkyrieEnemy* Held = Valk.Get())
				{
					// Aim again at where the hand is now (the pose moved since the closeup step).
					if (AGBPlayerController* PC = Cast<AGBPlayerController>(Player->GetController()))
					{
						PC->SetControlRotation((Held->GetMesh()->GetBoneLocation(FName(TEXT("hand_r")), EBoneSpaces::WorldSpace) - Player->GetAimOrigin()).Rotation());
					}
				}
				Shot(TEXT("valkyrie_hand"));
				if (AGBValkyrieEnemy* Vk = Valk.Get())
				{
					Vk->DebugFreeze(false);
				}
			});
			Add(VK + 12.7f, TEXT("Valkyrie: Judgement"), [Snapshot, Face](UWorld*, AGBPlayerCharacter* Player)
			{
				E3DCloseup(Player, FName(TEXT("Valkyrie_Future")));
				if (AGBValkyrieEnemy* Vk = Valk.Get())
				{
					Snapshot();
					Vk->DebugQueue(AGBValkyrieEnemy::EKind::Judgement);
				}
				Face(Player, -20.f);
			});
			Add(VK + 14.2f, TEXT("Valkyrie: Judgement telegraph screenshot"), [](UWorld*, AGBPlayerCharacter*) { Shot(TEXT("valkyrie_judgement_telegraph")); });
			Add(VK + 14.7f, TEXT("Valkyrie: Judgement strike screenshot"), [](UWorld*, AGBPlayerCharacter*) { Shot(TEXT("valkyrie_judgement_strike")); });
			Add(VK + 16.9f, TEXT("Valkyrie: Judgement check"), [](UWorld*, AGBPlayerCharacter*)
			{
				const AGBValkyrieEnemy* Vk = Valk.Get();
				if (!Vk)
				{
					return;
				}
				const int32 Casts = Vk->GetJudgementCasts() - Base[2];
				const int32 Made = Vk->GetCirclesMade() - Base[3];
				const int32 Struck = Vk->GetCirclesStruck() - Base[4];
				const int32 Hit = Vk->GetCircleHits() - Base[5];
				UE_LOG(LogGB, Display, TEXT("SMOKE Valkyrie judgement: casts=%d circles=%d struck=%d on the player=%d live=%d lift=%.0f | %s"), Casts, Made, Struck, Hit, Vk->GetLiveMarks(), Vk->GetLift(), *Vk->DescribeTactics());
				if (Casts < 1 || Made < 3 || Struck != Made || Hit < 1 || Vk->GetLiveMarks() != 0)
				{
					UE_LOG(LogGB, Error, TEXT("SMOKE Valkyrie judgement FAILED: casts=%d circles=%d struck=%d hit=%d live=%d (wanted 1, 3, all struck, the player's circle hit, none left)"), Casts, Made, Struck, Hit, Vk->GetLiveMarks());
				}
			});
			Add(VK + 17.1f, TEXT("Valkyrie: lances"), [Snapshot, Face](UWorld*, AGBPlayerCharacter* Player)
			{
				E3DCloseup(Player, FName(TEXT("Valkyrie_Future")));
				if (AGBValkyrieEnemy* Vk = Valk.Get())
				{
					Snapshot();
					Vk->DebugQueue(AGBValkyrieEnemy::EKind::Lances);
				}
				Face(Player, 0.f);
			});
			Add(VK + 17.85f, TEXT("Valkyrie: lances screenshot"), [](UWorld*, AGBPlayerCharacter*) { Shot(TEXT("valkyrie_lances")); });
			Add(VK + 19.6f, TEXT("Valkyrie: lances check"), [](UWorld*, AGBPlayerCharacter*)
			{
				const AGBValkyrieEnemy* Vk = Valk.Get();
				if (!Vk)
				{
					return;
				}
				const int32 Fired = Vk->GetLancesFired() - Base[6];
				UE_LOG(LogGB, Display, TEXT("SMOKE Valkyrie lances: fired=%d volleys=%d | %s"), Fired, Vk->GetVolleys(), *Vk->DescribeTactics());
				if (Fired < 5)
				{
					UE_LOG(LogGB, Error, TEXT("SMOKE Valkyrie lances FAILED: %d lances fired (wanted at least 5)"), Fired);
				}
			});
			Add(VK + 19.8f, TEXT("Valkyrie: phase two Judgement"), [Snapshot](UWorld*, AGBPlayerCharacter*)
			{
				if (AGBValkyrieEnemy* Vk = Valk.Get())
				{
					Snapshot();
					Vk->DebugPhaseTwo();
					Vk->DebugQueue(AGBValkyrieEnemy::EKind::Judgement);
				}
			});
			Add(VK + 24.1f, TEXT("Valkyrie: phase two check"), [](UWorld*, AGBPlayerCharacter*)
			{
				const AGBValkyrieEnemy* Vk = Valk.Get();
				if (!Vk)
				{
					return;
				}
				const int32 Made = Vk->GetCirclesMade() - Base[3];
				const int32 Struck = Vk->GetCirclesStruck() - Base[4];
				UE_LOG(LogGB, Display, TEXT("SMOKE Valkyrie phase two: phase2=%d circles=%d struck=%d | %s"), Vk->IsPhaseTwo() ? 1 : 0, Made, Struck, *Vk->DescribeTactics());
				if (!Vk->IsPhaseTwo() || Made < 4 || Struck != Made)
				{
					UE_LOG(LogGB, Error, TEXT("SMOKE Valkyrie phase two FAILED: phase2=%d circles=%d struck=%d (wanted phase two, at least 4 circles, all struck)"), Vk->IsPhaseTwo() ? 1 : 0, Made, Struck);
				}
			});
			Add(VK + 24.3f, TEXT("Valkyrie: kill"), [](UWorld*, AGBPlayerCharacter* Player) { E3DKillGroup(Player); });
			Add(VK + 25.3f, TEXT("Valkyrie: death check"), [](UWorld*, AGBPlayerCharacter*)
			{
				const AGBValkyrieEnemy* Vk = Valk.Get();
				UE_LOG(LogGB, Display, TEXT("SMOKE Valkyrie death: dead=%d marks=%d lift=%.0f"), Vk && Vk->IsDead() ? 1 : 0, Vk ? Vk->GetLiveMarks() : 0, Vk ? Vk->GetLift() : 0.f);
				if (Vk && (!Vk->IsDead() || Vk->GetLiveMarks() != 0))
				{
					UE_LOG(LogGB, Error, TEXT("SMOKE Valkyrie death FAILED: she did not die cleanly"));
				}
			});
			Add(VK + 25.8f, TEXT("Valkyrie: done"), [](UWorld*, AGBPlayerCharacter*)
			{
				if (bFilteredRun)
				{
					UE_LOG(LogGB, Display, TEXT("SMOKE DONE"));
					FPlatformMisc::RequestExit(false);
				}
			});
		}
		// ---- Castle block (gb.Smoke Castle): the castle's sprite enemies with the squad AI. See CastleSpawn / CastleCheck.
		const float CS = VK + 26.4f;
		{
			Add(CS, TEXT("Castle: boxes and the sprite quad's UV check"), [](UWorld* World, AGBPlayerCharacter* Player)
			{
				if (AGBPlayerController* PC = Cast<AGBPlayerController>(Player->GetController()))
				{
					PC->SetControlRotation(FRotator::ZeroRotator);
				}
				FVector EnemySpawn;
				FVector PlayerSpot;
				AGBTestArena::GetPathTestPoints(EnemySpawn, PlayerSpot);
				Player->SetActorLocation(PlayerSpot, false, nullptr, ETeleportType::TeleportPhysics);
				E3TSpawnBoxes(World, Player);
				CastlePlaneCheck();
			});
			// The boxes cut the navmesh first (the squad plans its cover on it): the spawn waits for the build, at most 4 s.
			Add(CS + 3.5f, TEXT("Castle: spawn one of each castle enemy"), [](UWorld* World, AGBPlayerCharacter* Player) { CastleSpawn(World, Player); });
			Add(CS + 9.f, TEXT("Castle: group screenshot"), [](UWorld*, AGBPlayerCharacter* Player) { E3DLookAtGroup(Player); Shot(TEXT("castle_group")); });
			Add(CS + 21.f, TEXT("Castle: check"), [](UWorld* World, AGBPlayerCharacter* Player) { CastleCheck(World, Player); }, 16.5f);
			Add(CS + 21.2f, TEXT("Castle: fight screenshot"), [](UWorld*, AGBPlayerCharacter* Player) { E3DLookAtGroup(Player); Shot(TEXT("castle_fight")); });
			Add(CS + 21.5f, TEXT("Castle: fly up"), [](UWorld*, AGBPlayerCharacter* Player) { E3TTop(Player); });
			Add(CS + 22.2f, TEXT("Castle: top screenshot"), [](UWorld*, AGBPlayerCharacter*) { Shot(TEXT("castle_top")); });
			Add(CS + 22.5f, TEXT("Castle: back down"), [](UWorld*, AGBPlayerCharacter* Player) { E3TBack(Player); });
			struct FCloseup { const TCHAR* Id; const TCHAR* Shot; };
			const FCloseup Closeups[] = { { TEXT("Orc_Past"), TEXT("castle_orc") }, { TEXT("Orc_Past_Shaman"), TEXT("castle_shaman") }, { TEXT("Centaur_Past"), TEXT("castle_centaur") },
				{ TEXT("Imp_Past"), TEXT("castle_barghest") }, { TEXT("Orc_Past_Brute"), TEXT("castle_brute") } };
			static TArray<TSharedPtr<FString>> CastlePool;
			auto CastleName = [](const FString& Text) -> const TCHAR* { CastlePool.Add(MakeShared<FString>(Text)); return **CastlePool.Last(); };
			for (int32 Index = 0; Index < UE_ARRAY_COUNT(Closeups); ++Index)
			{
				const FCloseup Each = Closeups[Index];
				Add(CS + 23.f + Index, CastleName(FString::Printf(TEXT("Castle: closeup of %s"), Each.Id)), [Each](UWorld*, AGBPlayerCharacter* Player) { E3DCloseup(Player, FName(Each.Id)); });
				Add(CS + 23.6f + Index, CastleName(FString::Printf(TEXT("Castle: closeup screenshot %s"), Each.Shot)), [Each](UWorld*, AGBPlayerCharacter*) { Shot(Each.Shot); });
			}
			Add(CS + 28.f, TEXT("Castle: kill"), [](UWorld*, AGBPlayerCharacter* Player) { E3DLookAtGroup(Player); CastleKill(Player); });
			Add(CS + 28.5f, TEXT("Castle: death clips"), [](UWorld*, AGBPlayerCharacter*) { CastleDeathCheck(false); Shot(TEXT("castle_death1")); });
			// Clip 0.75 s, lie 0.6 s (shortened by the kill step): at 1.3 s each corpse holds its last frame and has not sunk yet.
			Add(CS + 29.3f, TEXT("Castle: death hold"), [](UWorld*, AGBPlayerCharacter*) { CastleDeathCheck(true); Shot(TEXT("castle_death2")); });
			Add(CS + 33.f, TEXT("Castle: corpses removed"), [](UWorld*, AGBPlayerCharacter* Player) { E3DCheckCorpses(TEXT("castle")); CastleClean(Player); });
			// The castle's boss: Valkyrie_Past is her twin's fight (AGBValkyrieEnemy) on the ValkyriePixel sheet. Sealed, woken, one combo,
			// one Judgement, killed: her death clip holds and her corpse goes.
			static TWeakObjectPtr<AGBValkyrieEnemy> CastleValk;
			static int32 CastleValkBase[4];
			Add(CS + 34.f, TEXT("Castle: Valkyrie spawn sealed"), [](UWorld* World, AGBPlayerCharacter* Player)
			{
				FVector EnemySpawn;
				FVector PlayerSpot;
				AGBTestArena::GetPathTestPoints(EnemySpawn, PlayerSpot);
				Player->SetActorLocation(PlayerSpot, false, nullptr, ETeleportType::TeleportPhysics);
				E3DSpawnGroup(World, Player, { { FName(TEXT("Valkyrie_Past")), 1400.f } });
				Player->GetAttributes()->SetInvulnerable(true);
				CastleValk = E3DGroup.Num() > 0 ? Cast<AGBValkyrieEnemy>(E3DGroup[0].Enemy.Get()) : nullptr;
				AGBValkyrieEnemy* Vk = CastleValk.Get();
				if (!Vk)
				{
					UE_LOG(LogGB, Error, TEXT("SMOKE Castle Valkyrie FAILED: Valkyrie_Past did not spawn as an AGBValkyrieEnemy (ActorClass)"));
					return;
				}
				Vk->DebugAutoMask(0);
				Vk->SetSealed(true);
				UE_LOG(LogGB, Display, TEXT("SMOKE Castle Valkyrie spawned: class=%s spriteLook=%d sealed=%d | %s"), *Vk->GetClass()->GetName(), Vk->HasSpriteLook() ? 1 : 0, Vk->IsSealed() ? 1 : 0, *Vk->DescribeLook());
				const bool bSheet = FPaths::FileExists(UGBSpriteSheetSubsystem::SheetDir() / TEXT("ValkyriePixel.json"));
				if (bSheet && !Vk->HasSpriteLook())
				{
					UE_LOG(LogGB, Error, TEXT("SMOKE Castle Valkyrie FAILED: her sheet is there but did not load"));
				}
			});
			Add(CS + 34.5f, TEXT("Castle: Valkyrie wake"), [](UWorld*, AGBPlayerCharacter* Player)
			{
				if (AGBValkyrieEnemy* Vk = CastleValk.Get())
				{
					FVector Landing = Vk->GetActorLocation() + (Player->GetActorLocation() - Vk->GetActorLocation()).GetSafeNormal2D() * 300.f;
					Vk->ProjectPointToNavigationForWake(Landing);
					Vk->BeginWake(Landing);
					E3DLookAtGroup(Player);
				}
			});
			Add(CS + 37.5f, TEXT("Castle: Valkyrie combo"), [](UWorld*, AGBPlayerCharacter* Player)
			{
				E3DCloseup(Player, FName(TEXT("Valkyrie_Past")));
				if (AGBValkyrieEnemy* Vk = CastleValk.Get())
				{
					CastleValkBase[0] = Vk->GetSwingCount();
					CastleValkBase[1] = Vk->GetActionClipsPlayed();
					Vk->DebugQueue(AGBValkyrieEnemy::EKind::Combo);
				}
			});
			Add(CS + 38.2f, TEXT("Castle: Valkyrie combo screenshot"), [](UWorld*, AGBPlayerCharacter*) { Shot(TEXT("castle_valkyrie_combo")); });
			Add(CS + 41.f, TEXT("Castle: Valkyrie combo check, Judgement"), [](UWorld*, AGBPlayerCharacter* Player)
			{
				AGBValkyrieEnemy* Vk = CastleValk.Get();
				if (!Vk)
				{
					UE_LOG(LogGB, Error, TEXT("SMOKE Castle Valkyrie FAILED: she is gone before the combo check"));
					return;
				}
				const int32 Swings = Vk->GetSwingCount() - CastleValkBase[0];
				const int32 Clips = Vk->GetActionClipsPlayed() - CastleValkBase[1];
				UE_LOG(LogGB, Display, TEXT("SMOKE Castle Valkyrie combo: swings=%d clips=%d hits=%d sealed=%d | %s"), Swings, Clips, Vk->GetSwingHits(), Vk->IsSealed() ? 1 : 0, *Vk->DescribeLook());
				if (Swings < 1 || (Vk->HasSpriteLook() && Clips < Swings))
				{
					UE_LOG(LogGB, Error, TEXT("SMOKE Castle Valkyrie FAILED: combo swings=%d, sprite clips=%d (wanted a swing, a clip per swing)"), Swings, Clips);
				}
				CastleValkBase[2] = Vk->GetJudgementCasts();
				CastleValkBase[3] = Vk->GetCirclesStruck();
				E3DCloseup(Player, FName(TEXT("Valkyrie_Past")));
				Vk->DebugQueue(AGBValkyrieEnemy::EKind::Judgement);
			});
			Add(CS + 42.5f, TEXT("Castle: Valkyrie Judgement screenshot"), [](UWorld*, AGBPlayerCharacter*) { Shot(TEXT("castle_valkyrie_judgement")); });
			Add(CS + 45.f, TEXT("Castle: Valkyrie Judgement check, kill"), [](UWorld*, AGBPlayerCharacter* Player)
			{
				AGBValkyrieEnemy* Vk = CastleValk.Get();
				if (!Vk)
				{
					UE_LOG(LogGB, Error, TEXT("SMOKE Castle Valkyrie FAILED: she is gone before the Judgement check"));
					return;
				}
				const int32 Casts = Vk->GetJudgementCasts() - CastleValkBase[2];
				const int32 Struck = Vk->GetCirclesStruck() - CastleValkBase[3];
				UE_LOG(LogGB, Display, TEXT("SMOKE Castle Valkyrie judgement: casts=%d struck=%d live=%d lancesFired=%d shots=%d | %s"), Casts, Struck, Vk->GetLiveMarks(), Vk->GetLancesFired(), Vk->GetShotCount(), *Vk->DescribeLook());
				if (Casts < 1 || Struck < 1)
				{
					UE_LOG(LogGB, Error, TEXT("SMOKE Castle Valkyrie FAILED: Judgement casts=%d circles struck=%d"), Casts, Struck);
				}
				CastleKill(Player);
			});
			Add(CS + 46.2f, TEXT("Castle: Valkyrie death hold"), [](UWorld*, AGBPlayerCharacter*) { CastleDeathCheck(true); Shot(TEXT("castle_valkyrie_death")); });
			Add(CS + 50.f, TEXT("Castle: Valkyrie corpse removed"), [](UWorld*, AGBPlayerCharacter*)
			{
				UE_LOG(LogGB, Display, TEXT("SMOKE Castle Valkyrie corpse gone=%d"), CastleValk.IsValid() ? 0 : 1);
				if (CastleValk.IsValid())
				{
					UE_LOG(LogGB, Error, TEXT("SMOKE Castle Valkyrie FAILED: her corpse is still there 5 s after the kill"));
				}
				E3DCheckCorpses(TEXT("castle valkyrie"));
			});
			Add(CS + 50.5f, TEXT("Castle: done"), [](UWorld*, AGBPlayerCharacter*)
			{
				if (bFilteredRun)
				{
					UE_LOG(LogGB, Display, TEXT("SMOKE DONE"));
					FPlatformMisc::RequestExit(false);
				}
			});
		}
		// ---- CastleVisit block (gb.Smoke CastleVisit -Map /Game/Glyphbound/Maps/PastCastle): a state machine, see CastleVisit. It holds
		// the smoke clock while it runs, so its done step comes when the visit is over. On any other map it is skipped.
		Add(CS + 50.6f, TEXT("CastleVisit: the castle's encounters as placed (skipped on any other map)"), [](UWorld* World, AGBPlayerCharacter* Player) { CastleVisit::Start(World, Player); });
		Add(CS + 50.8f, TEXT("CastleVisit: done"), [](UWorld* World, AGBPlayerCharacter*)
		{
			if (bFilteredRun && World && World->GetMapName().Contains(TEXT("PastCastle")))
			{
				UE_LOG(LogGB, Display, TEXT("SMOKE DONE"));
				FPlatformMisc::RequestExit(false);
			}
		});
		// ---- Seal block (gb.Smoke seal): a state machine, see SealRun.
		// It quits the run itself when done; the quit step below is only a far safety net.
		const float Z = CS + 51.f;
		Add(Z, TEXT("seal: seal, era block with a real doorway, captions, cancel, gun drop"), SealRun::Start);
		Add(Z + 0.5f, TEXT("tour: a real map's own review spots (skipped on a map without them)"), TourRun::Start);
		// ---- Enemies3DBase block: the four builder encounters of FutureBase, only in a filtered run on that map.
		{
			struct FSpot { const TCHAR* Id; FVector Stand; };
			const FSpot Spots[] = {
				{ TEXT("Future_Bay"), FVector(2700.f, -75.f, 0.f) },
				{ TEXT("Future_Spiders"), FVector(3380.f, 5300.f, 0.f) },
				{ TEXT("SubBoss_Future"), FVector(3380.f, 8300.f, 0.f) },
				{ TEXT("Future_DeepWing"), FVector(11300.f, 0.f, -600.f) },
			};
			const float Durations[4] = { 88.f, 53.f, 53.f, 53.f };
			static TArray<TSharedPtr<FString>> BasePool;
			auto BaseName = [](const FString& Text) -> const TCHAR* { BasePool.Add(MakeShared<FString>(Text)); return **BasePool.Last(); };
			for (int32 Index = 0; Index < UE_ARRAY_COUNT(Spots); ++Index)
			{
				float T = Z + 5.f;
				for (int32 Before = 0; Before < Index; ++Before)
				{
					T += Durations[Before];
				}
				const float KillStart = Index == 0 ? 71.f : 36.f;
				const FSpot Spot = Spots[Index];
				const FName Id(Spot.Id);
				Add(T, BaseName(FString::Printf(TEXT("Enemies3DBase: %s teleport"), Spot.Id)), [Id, Spot](UWorld* World, AGBPlayerCharacter* Player) { E3BBegin(World, Player, Id, Spot.Stand); });
				Add(T + 11.f, BaseName(FString::Printf(TEXT("Enemies3DBase: %s patrol check"), Spot.Id)), [](UWorld* World, AGBPlayerCharacter* Player) { E3BPatrolCheck(World, Player); });
				Add(T + 11.3f, BaseName(FString::Printf(TEXT("Enemies3DBase: %s walk in"), Spot.Id)), [](UWorld* World, AGBPlayerCharacter* Player) { E3BTeleportIn(World, Player); });
				Add(T + 14.f, BaseName(FString::Printf(TEXT("Enemies3DBase: %s wave 1"), Spot.Id)), [](UWorld* World, AGBPlayerCharacter* Player) { E3BRecord(World, Player); });
				Add(T + 19.5f, BaseName(FString::Printf(TEXT("Enemies3DBase: %s behaviour"), Spot.Id)), [](UWorld* World, AGBPlayerCharacter* Player) { E3BBehave(World, Player); });
				Add(T + 20.f, BaseName(FString::Printf(TEXT("Enemies3DBase: %s player far away"), Spot.Id)), [](UWorld* World, AGBPlayerCharacter* Player) { E3BZoneOut(World, Player); });
				for (int32 Sample = 1; Sample <= 6; ++Sample)
				{
					Add(T + 20.f + Sample, BaseName(FString::Printf(TEXT("Enemies3DBase: %s zone sample %d"), Spot.Id, Sample)), [](UWorld* World, AGBPlayerCharacter*) { E3BZoneSample(World, TEXT("player far away")); });
				}
				Add(T + 26.8f, BaseName(FString::Printf(TEXT("Enemies3DBase: %s player back"), Spot.Id)), [](UWorld* World, AGBPlayerCharacter* Player) { E3BZoneBack(World, Player); });
				Add(T + 27.3f, BaseName(FString::Printf(TEXT("Enemies3DBase: %s screenshot"), Spot.Id)), [Id](UWorld* World, AGBPlayerCharacter*)
				{
					if (E3BActive(World))
					{
						Shot(*FString::Printf(TEXT("enemies3dbase_%s"), *Id.ToString()));
					}
				});
				// The player just outside the zone for 5 s (nobody leaves, no lunge out), then back.
				Add(T + 28.f, BaseName(FString::Printf(TEXT("Enemies3DBase: %s player at the edge"), Spot.Id)), [](UWorld* World, AGBPlayerCharacter* Player) { E3BEdgeOut(World, Player); });
				for (int32 Sample = 1; Sample <= 10; ++Sample)
				{
					Add(T + 28.f + 0.5f * Sample, BaseName(FString::Printf(TEXT("Enemies3DBase: %s edge sample %d"), Spot.Id, Sample)), [](UWorld* World, AGBPlayerCharacter*) { E3BZoneSample(World, TEXT("player at the edge")); });
				}
				Add(T + 33.5f, BaseName(FString::Printf(TEXT("Enemies3DBase: %s edge back"), Spot.Id)), [](UWorld* World, AGBPlayerCharacter* Player) { E3BZoneBack(World, Player); });
				if (Index == 0)
				{
					// Lose-target: the player is far away for longer than EncounterLoseTargetTime; they go back to their posts.
					Add(T + 34.f, BaseName(FString::Printf(TEXT("Enemies3DBase: %s lost him"), Spot.Id)), [](UWorld* World, AGBPlayerCharacter* Player) { E3BZoneOut(World, Player); });
					Add(T + 68.f, BaseName(FString::Printf(TEXT("Enemies3DBase: %s lose check"), Spot.Id)), [](UWorld* World, AGBPlayerCharacter*) { E3BLoseCheck(World); });
					Add(T + 69.f, BaseName(FString::Printf(TEXT("Enemies3DBase: %s found him again"), Spot.Id)), [](UWorld* World, AGBPlayerCharacter* Player) { E3BZoneBack(World, Player); });
				}
				for (int32 Round = 0; Round < 6; ++Round)
				{
					Add(T + KillStart + Round * 2.6f, BaseName(FString::Printf(TEXT("Enemies3DBase: %s kill %d"), Spot.Id, Round + 1)), [](UWorld* World, AGBPlayerCharacter* Player) { E3BKill(World, Player); });
				}
				Add(T + KillStart + 16.f, BaseName(FString::Printf(TEXT("Enemies3DBase: %s cleared"), Spot.Id)), [](UWorld* World, AGBPlayerCharacter*) { E3BFinish(World); });
			}
			// ---- the Valkyrie in her tube
			const float V0 = Z + 5.f + Durations[0] + Durations[1] + Durations[2] + Durations[3];
			Add(V0, BaseName(TEXT("Enemies3DBase: Valkyrie begin")), [](UWorld* World, AGBPlayerCharacter* Player) { E3VBegin(World, Player); });
			Add(V0 + 1.5f, BaseName(TEXT("Enemies3DBase: Valkyrie in her room")), [](UWorld* World, AGBPlayerCharacter* Player) { E3VRecord(World, Player); });
			Add(V0 + 5.7f, BaseName(TEXT("Enemies3DBase: Valkyrie asleep after 4 s")), [](UWorld* World, AGBPlayerCharacter* Player) { E3VAsleep(World, Player, false, TEXT("4 s in front of the tube")); });
			Add(V0 + 5.9f, BaseName(TEXT("Enemies3DBase: Valkyrie hit while sealed")), [](UWorld* World, AGBPlayerCharacter* Player) { E3VAsleep(World, Player, true, TEXT("hit by a damage call")); });
			Add(V0 + 6.8f, BaseName(TEXT("Enemies3DBase: Valkyrie asleep after the hit")), [](UWorld* World, AGBPlayerCharacter* Player) { E3VAsleep(World, Player, false, TEXT("after the hit")); });
			Add(V0 + 7.f, BaseName(TEXT("Enemies3DBase: Valkyrie flag set")), [](UWorld* World, AGBPlayerCharacter*) { E3VSetFlag(World, true); });
			Add(V0 + 8.5f, BaseName(TEXT("Enemies3DBase: Valkyrie woke")), [](UWorld* World, AGBPlayerCharacter*) { E3VWoken(World, false); });
			Add(V0 + 12.f, BaseName(TEXT("Enemies3DBase: Valkyrie left the tube")), [](UWorld* World, AGBPlayerCharacter*) { E3VWoken(World, true); });
			Add(V0 + 12.2f, BaseName(TEXT("Enemies3DBase: Valkyrie flag cleared")), [](UWorld* World, AGBPlayerCharacter*) { E3VSetFlag(World, false); });
			Add(V0 + 13.f, BaseName(TEXT("Enemies3DBase: Valkyrie kill 1")), [](UWorld* World, AGBPlayerCharacter* Player) { E3BKill(World, Player); });
			Add(V0 + 15.6f, BaseName(TEXT("Enemies3DBase: Valkyrie kill 2")), [](UWorld* World, AGBPlayerCharacter* Player) { E3BKill(World, Player); });
			Add(V0 + 18.f, BaseName(TEXT("Enemies3DBase: Valkyrie cleared")), [](UWorld* World, AGBPlayerCharacter*) { E3BFinish(World); });
			Add(V0 + 20.f, TEXT("Enemies3DBase: done"), [](UWorld*, AGBPlayerCharacter*)
			{
				if (bFilteredRun)
				{
					UE_LOG(LogGB, Display, TEXT("SMOKE DONE"));
					FPlatformMisc::RequestExit(false);
				}
			});
		}
		// ---- ViewPose block (gb.Smoke ViewPose): first-person gun pose in data, fixed weapon FOV, one shared motion.
		// Each gun: equip, freeze the bob, the root sits at the pose from Weapons.json, hip screenshot to
		// Saved/Screenshots/viewpose_*.png, aim, the gun's screen position (fixed weapon FOV) does not move.
		// Then gb.HandOverlay with a bad path must log an error and draw nothing.
		static FVector2D ViewPoseHipScreen = FVector2D::ZeroVector;
		static float ViewPoseHipFov = 0.f;
		static bool bViewPoseHipValid = false;

		auto ViewPoseEquip = [](AGBPlayerCharacter* Player, FName WeaponId) -> AGBGun*
		{
			UGBEquipmentComponent* Equipment = Player ? Player->GetEquipment() : nullptr;
			if (!Equipment)
			{
				return nullptr;
			}
			// GiveWeapon only stores a weapon whose slot is taken (the gun in hand is never replaced), so equip after.
			if (!Equipment->OwnsWeapon(WeaponId))
			{
				Equipment->GiveWeapon(WeaponId, true);
			}
			if (!Equipment->IsWeaponEquipped(WeaponId))
			{
				Equipment->EquipOwnedWeapon(WeaponId);
			}
			else
			{
				Equipment->SwitchTo(EGBHeldSlot::Gun);
			}
			AGBGun* Gun = Cast<AGBGun>(Equipment->GetActiveItem());
			if (Gun && Gun->GetWeaponId() == WeaponId)
			{
				Gun->RefillAmmo();
				Gun->ClearReloadPreview();
				Gun->ClearPumpPreview();
				return Gun;
			}
			return nullptr;
		};

		auto ViewPoseProject = [](AGBPlayerCharacter* Player, const FVector& WorldPos, float FovDeg, FVector2D& OutScreen) -> bool
		{
			UCameraComponent* Camera = Player ? Player->GetCamera() : nullptr;
			if (!Camera || !GEngine || !GEngine->GameViewport)
			{
				return false;
			}
			FVector2D ViewSize = FVector2D::ZeroVector;
			GEngine->GameViewport->GetViewportSize(ViewSize);
			if (ViewSize.X <= 0.f || ViewSize.Y <= 0.f)
			{
				return false;
			}
			const FVector Local = Camera->GetComponentTransform().InverseTransformPosition(WorldPos);
			if (Local.X < 1.f)
			{
				return false;
			}
			// Maintain Y-Axis (legacy matrix, see DefaultEngine.ini): the FOV value is the vertical one.
			const float TanHalfV = FMath::Tan(FMath::DegreesToRadians(FMath::Clamp(FovDeg, 1.f, 179.f) * 0.5f));
			const float Aspect = ViewSize.X / ViewSize.Y;
			OutScreen = FVector2D(((Local.Y / Local.X) / (TanHalfV * Aspect) * 0.5f + 0.5f) * ViewSize.X,
				(0.5f - (Local.Z / Local.X) / TanHalfV * 0.5f) * ViewSize.Y);
			return true;
		};

		auto ViewPoseShot = [](const TCHAR* Name)
		{
			const FString Path = FPaths::ProjectSavedDir() / TEXT("Screenshots") / (FString(TEXT("viewpose_")) + Name + TEXT(".png"));
			FScreenshotRequest::RequestScreenshot(Path, false, false);
			UE_LOG(LogGB, Display, TEXT("SMOKE screenshot %s"), *Path);
		};

		auto ViewPoseCheckAtDataPose = [](const TCHAR* Label, AGBGun* Gun, UWorld* World)
		{
			const UGBDataSubsystem* Data = UGBDataSubsystem::Get(World);
			const FGBWeaponDefinition* Def = Data ? Data->FindWeapon(Gun->GetWeaponId()) : nullptr;
			if (!Def || !Gun->GetRootComponent())
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE ViewPose %s FAILED: '%s' has no data or no root"), Label, *Gun->GetWeaponId().ToString());
				return;
			}
			auto Raw = [Def](const TCHAR* Key, float Fallback)
			{
				if (const float* Found = Def->Stats.Find(FName(Key)))
				{
					return *Found;
				}
				return Fallback;
			};
			const FVector Expected(Raw(TEXT("ViewOffsetX"), 0.f), Raw(TEXT("ViewOffsetY"), 0.f), Raw(TEXT("ViewOffsetZ"), 0.f));
			const FRotator ExpectedRot(Raw(TEXT("ViewPitch"), 0.f), Raw(TEXT("ViewYaw"), -6.f), Raw(TEXT("ViewRoll"), 0.f));
			const FVector Actual = Gun->GetRootComponent()->GetRelativeLocation();
			const FRotator ActualRot = Gun->GetRootComponent()->GetRelativeRotation();
			UE_LOG(LogGB, Display, TEXT("SMOKE ViewPose %s: root at (%.2f, %.2f, %.2f) rot (%.2f, %.2f, %.2f), data (%.2f, %.2f, %.2f) rot (%.2f, %.2f, %.2f)"),
				Label, Actual.X, Actual.Y, Actual.Z, ActualRot.Pitch, ActualRot.Yaw, ActualRot.Roll,
				Expected.X, Expected.Y, Expected.Z, ExpectedRot.Pitch, ExpectedRot.Yaw, ExpectedRot.Roll);
			if (!Actual.Equals(Expected, 0.05f) || !ActualRot.Equals(ExpectedRot, 0.1f))
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE ViewPose %s FAILED: the root is not at the pose from Weapons.json"), Label);
			}
		};

		auto ViewPoseHip = [ViewPoseEquip, ViewPoseProject, ViewPoseShot, ViewPoseCheckAtDataPose](UWorld* World, AGBPlayerCharacter* Player, FName WeaponId, const TCHAR* Label)
		{
			AGBGun* Gun = ViewPoseEquip(Player, WeaponId);
			if (!Gun)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE ViewPose %s FAILED: could not equip"), Label);
				bViewPoseHipValid = false;
				return;
			}
			GBViewBob::bFrozen = true;
			Gun->StopSecondary();
			ViewPoseCheckAtDataPose(Label, Gun, World);
			bViewPoseHipValid = ViewPoseProject(Player, Gun->GetActorLocation(), Gun->GetViewModelFovDeg(), ViewPoseHipScreen);
			ViewPoseHipFov = Player->GetCamera() ? Player->GetCamera()->FieldOfView : 0.f;
			UE_LOG(LogGB, Display, TEXT("SMOKE ViewPose %s hip: screen (%.1f, %.1f) worldFOV %.2f"), Label, ViewPoseHipScreen.X, ViewPoseHipScreen.Y, ViewPoseHipFov);
			ViewPoseShot(Label);
			Gun->StartSecondary(); // the aim check runs in the next step, once AimAlpha had time to reach 1
		};

		auto ViewPoseAim = [ViewPoseProject, ViewPoseCheckAtDataPose](UWorld* World, AGBPlayerCharacter* Player, FName WeaponId, const TCHAR* Label)
		{
			AGBGun* Gun = Cast<AGBGun>(Player->GetEquipment() ? Player->GetEquipment()->GetActiveItem() : nullptr);
			if (!Gun || Gun->GetWeaponId() != WeaponId)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE ViewPose %s FAILED: '%s' not in hand for the aim check"), Label, *WeaponId.ToString());
				return;
			}
			UCameraComponent* Camera = Player->GetCamera();
			const float AimFOV = Gun->GetStat(GBStat::AimFOV, 0.f);
			UE_LOG(LogGB, Display, TEXT("SMOKE ViewPose %s aim: worldFOV %.2f (AimFOV %.2f) weaponFOV %.2f (ViewModelFOV %.2f)"),
				Label, Camera ? Camera->FieldOfView : -1.f, AimFOV,
				Camera ? Camera->FirstPersonFieldOfView : -1.f, Gun->GetViewModelFovDeg());
			if (!Camera || !FMath::IsNearlyEqual(Camera->FieldOfView, AimFOV, 0.5f))
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE ViewPose %s FAILED: the world camera did not reach full AimFOV"), Label);
			}
			if (!Camera || !FMath::IsNearlyEqual(Camera->FirstPersonFieldOfView, Gun->GetViewModelFovDeg(), 0.01f))
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE ViewPose %s FAILED: the weapon FOV moved while aiming"), Label);
			}
			// The aim check must be able to fail with the feature off: the camera must render the viewmodel
			// with the fixed first-person FOV, and every primitive of the held gun (all attached components,
			// including the muzzle flash) must use it.
			if (!Camera || !Camera->bEnableFirstPersonFieldOfView)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE ViewPose %s FAILED: the camera does not have the first-person FOV enabled"), Label);
			}
			{
				TArray<UPrimitiveComponent*> Prims;
				Gun->GetComponents<UPrimitiveComponent>(Prims);
				bool bAllFirstPerson = Prims.Num() > 0;
				for (UPrimitiveComponent* Prim : Prims)
				{
					if (!Prim || Prim->FirstPersonPrimitiveType != EFirstPersonPrimitiveType::FirstPerson)
					{
						UE_LOG(LogGB, Error, TEXT("SMOKE ViewPose %s FAILED: component '%s' is not FirstPerson"), Label,
							Prim ? *Prim->GetName() : TEXT("<null>"));
						bAllFirstPerson = false;
					}
				}
				if (!bAllFirstPerson && Prims.Num() == 0)
				{
					UE_LOG(LogGB, Error, TEXT("SMOKE ViewPose %s FAILED: the held gun has no primitives to check"), Label);
				}
			}
			ViewPoseCheckAtDataPose(Label, Gun, World);
			FVector2D AimScreen = FVector2D::ZeroVector;
			if (bViewPoseHipValid && ViewPoseProject(Player, Gun->GetActorLocation(), Gun->GetViewModelFovDeg(), AimScreen))
			{
				const float Drift = FVector2D::Distance(AimScreen, ViewPoseHipScreen);
				UE_LOG(LogGB, Display, TEXT("SMOKE ViewPose %s: hip screen (%.1f, %.1f) aim screen (%.1f, %.1f) drift %.2f px"),
					Label, ViewPoseHipScreen.X, ViewPoseHipScreen.Y, AimScreen.X, AimScreen.Y, Drift);
				if (Drift > 1.f)
				{
					UE_LOG(LogGB, Error, TEXT("SMOKE ViewPose %s FAILED: the gun moved %.2f px on screen between hip and full aim"), Label, Drift);
				}
			}
			else
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE ViewPose %s FAILED: could not project the gun"), Label);
			}
			// Sanity: the world-FOV projection must differ a lot (aim really engaged, the check above is not vacuous).
			FVector2D HipWorld = FVector2D::ZeroVector;
			FVector2D AimWorld = FVector2D::ZeroVector;
			if (ViewPoseProject(Player, Gun->GetActorLocation(), ViewPoseHipFov, HipWorld)
				&& Camera && ViewPoseProject(Player, Gun->GetActorLocation(), Camera->FieldOfView, AimWorld)
				&& FVector2D::Distance(HipWorld, AimWorld) < 20.f)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE ViewPose %s FAILED: the world projection barely changed (aim not engaged?)"), Label);
			}
			Gun->StopSecondary();
		};

		Add(Z + 390.f, TEXT("ViewPose pistol: equip, freeze, root at data pose, hip shot, aim"), [ViewPoseHip](UWorld* World, AGBPlayerCharacter* Player) { ViewPoseHip(World, Player, FName(TEXT("Pistol")), TEXT("pistol")); });
		Add(Z + 391.5f, TEXT("ViewPose pistol: full aim leaves the gun static on screen"), [ViewPoseAim](UWorld* World, AGBPlayerCharacter* Player) { ViewPoseAim(World, Player, FName(TEXT("Pistol")), TEXT("pistol")); });
		Add(Z + 393.f, TEXT("ViewPose shotgun: equip, freeze, root at data pose, hip shot, aim"), [ViewPoseHip](UWorld* World, AGBPlayerCharacter* Player) { ViewPoseHip(World, Player, FName(TEXT("Shotgun")), TEXT("shotgun")); });
		Add(Z + 394.5f, TEXT("ViewPose shotgun: full aim leaves the gun static on screen"), [ViewPoseAim](UWorld* World, AGBPlayerCharacter* Player) { ViewPoseAim(World, Player, FName(TEXT("Shotgun")), TEXT("shotgun")); });
		Add(Z + 396.f, TEXT("ViewPose rifle: equip, freeze, root at data pose, hip shot, aim"), [ViewPoseHip](UWorld* World, AGBPlayerCharacter* Player) { ViewPoseHip(World, Player, FName(TEXT("Rifle")), TEXT("rifle")); });
		Add(Z + 397.5f, TEXT("ViewPose rifle: full aim leaves the gun static on screen"), [ViewPoseAim](UWorld* World, AGBPlayerCharacter* Player) { ViewPoseAim(World, Player, FName(TEXT("Rifle")), TEXT("rifle")); });
		Add(Z + 399.f, TEXT("ViewPose overlay: a good PNG shows, a bad path clears and draws nothing"), [](UWorld* World, AGBPlayerCharacter*)
		{
			// Write a small 480x270 test PNG into Saved/, load it (must show), then feed a bad path (must clear).
			const FString GoodPath = FPaths::ProjectSavedDir() / TEXT("viewpose_overlay_test.png");
			TArray<FColor> Pixels;
			Pixels.SetNumUninitialized(480 * 270);
			for (int32 Y = 0; Y < 270; ++Y)
			{
				for (int32 X = 0; X < 480; ++X)
				{
					const bool bStripe = ((X / 30) + (Y / 30)) % 2 == 0;
					Pixels[Y * 480 + X] = bStripe ? FColor(200, 50, 50, 255) : FColor(30, 30, 30, 255);
				}
			}
			TArray64<uint8> Compressed;
			FImageUtils::PNGCompressImageArray(480, 270, TArrayView64<const FColor>(Pixels.GetData(), Pixels.Num()), Compressed);
			const bool bWrote = FFileHelper::SaveArrayToFile(Compressed, *GoodPath);
			UE_LOG(LogGB, Display, TEXT("SMOKE ViewPose overlay test PNG: wrote=%d '%s'"), bWrote ? 1 : 0, *GoodPath);
			if (!bWrote)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE ViewPose overlay FAILED: could not write the test PNG"));
				return;
			}
			const bool bGoodLoaded = SetHandOverlayFile(World, GoodPath);
			const AGBHUD* HUD = AGBHUD::Get(World);
			const bool bShows = HUD && HUD->HasHandOverlay();
			UE_LOG(LogGB, Display, TEXT("SMOKE ViewPose overlay good path: loaded=%d shows=%d"), bGoodLoaded ? 1 : 0, bShows ? 1 : 0);
			if (!bGoodLoaded || !bShows)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE ViewPose overlay FAILED: the valid PNG must load and draw"));
				return;
			}
			const bool bLoaded = SetHandOverlayFile(World, FString(TEXT("Z:/definitely-not-here/viewpose_missing.png")));
			const bool bDrawsNothing = HUD && !HUD->HasHandOverlay();
			UE_LOG(LogGB, Display, TEXT("SMOKE ViewPose overlay bad path: loaded=%d draws nothing=%d"), bLoaded ? 1 : 0, bDrawsNothing ? 1 : 0);
			if (bLoaded || !bDrawsNothing)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE ViewPose overlay FAILED: a bad path must log an error and draw nothing"));
			}
		});
		Add(Z + 399.5f, TEXT("ViewPose done: unfreeze, staff back in hand"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			GBViewBob::bFrozen = false;
			if (AGBGun* Gun = Cast<AGBGun>(Player->GetEquipment() ? Player->GetEquipment()->GetActiveItem() : nullptr))
			{
				Gun->StopSecondary();
			}
			Player->GetEquipment()->SwitchTo(EGBHeldSlot::Staff);
		});
		// ---- Fog block (gb.Smoke Fog): the tunnel smoke. An AGBFluidFog around the player: the system runs, the player and
		// the shots are written to the data channels, it stops when the player is far. Without the Niagara system asset
		// the actor only warns and stays idle; the checks that need the simulation are then skipped.
		static TWeakObjectPtr<AGBFluidFog> SmokeFog;
		static FVector SmokeFogPlayerHome = FVector::ZeroVector;
		const float FogT = Z + 401.f;
		Add(FogT, TEXT("Fog: spawn a tunnel-sized fog box in front of the player"), [](UWorld* World, AGBPlayerCharacter* Player)
		{
			SmokeFogPlayerHome = Player->GetActorLocation();
			// Like the dark corridor: 18 m long, 4.4 m wide, 3.5 m high, its floor on the player's floor, the player inside
			// near one end looking along it (the box is axis-aligned: its long axis is the one nearest the player's facing).
			const FVector Fwd = Player->GetActorForwardVector();
			const bool bAlongX = FMath::Abs(Fwd.X) >= FMath::Abs(Fwd.Y);
			const FVector Axis = bAlongX ? FVector(FMath::Sign(Fwd.X), 0.f, 0.f) : FVector(0.f, FMath::Sign(Fwd.Y), 0.f);
			const float HalfHeight = Player->GetCapsuleComponent() ? Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.f;
			const FVector Centre = SmokeFogPlayerHome + Axis * 700.f + FVector(0.f, 0.f, 175.f - HalfHeight);
			Player->SetActorRotation(Axis.Rotation());
			if (AController* Controller = Player->GetController())
			{
				Controller->SetControlRotation(Axis.Rotation() + FRotator(-8.f, 0.f, 0.f));
			}
			FTransform Where(FRotator::ZeroRotator, Centre);
			AGBFluidFog* Fog = World->SpawnActorDeferred<AGBFluidFog>(AGBFluidFog::StaticClass(), Where);
			if (!Fog)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE Fog FAILED: could not spawn the fog actor"));
				return;
			}
			Fog->BoxExtent = bAlongX ? FVector(900.f, 220.f, 175.f) : FVector(220.f, 900.f, 175.f);
			Fog->FinishSpawning(Where);
			SmokeFog = Fog;
			UE_LOG(LogGB, Display, TEXT("SMOKE Fog spawned: system loaded=%d"), Fog->HasSystem() ? 1 : 0);
		});
		Add(FogT + 8.f, TEXT("Fog: running (8 s to spread), the player is written, screenshot before the shots"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			AGBFluidFog* Fog = SmokeFog.Get();
			if (!Fog)
			{
				return;
			}
			if (Fog->HasSystem())
			{
				UE_LOG(LogGB, Display, TEXT("SMOKE Fog running=%d playerEntries=%d collisionEntries=%d"), Fog->IsSimulating() ? 1 : 0, Fog->GetLastPlayerEntries(), Fog->GetLastCollisionEntries());
				if (!Fog->IsSimulating() || Fog->GetLastPlayerEntries() != 2)
				{
					UE_LOG(LogGB, Error, TEXT("SMOKE Fog FAILED: with the player inside, the simulation must run and the player must be written as two spheres"));
				}
			}
			else
			{
				UE_LOG(LogGB, Display, TEXT("SMOKE Fog: no system asset, simulation checks skipped (running=%d)"), Fog->IsSimulating() ? 1 : 0);
			}
			// Walk through it: a velocity the player entries carry.
			if (UCharacterMovementComponent* Movement = Player->GetCharacterMovement())
			{
				Movement->Velocity = FVector(600.f, 0.f, 0.f);
			}
			Shot(TEXT("fog_before"));
		});
		Add(FogT + 8.7f, TEXT("Fog: three shots through it"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			AGBFluidFog* Fog = SmokeFog.Get();
			if (!Fog)
			{
				return;
			}
			// Along the tunnel from the player's eyes, low and level: the holes they tear show in the next screenshot.
			const FVector Eye = Player->GetActorLocation() + FVector(0.f, 0.f, 50.f);
			const FVector Along = Player->GetActorForwardVector().GetSafeNormal2D();
			for (int32 Index = 0; Index < 3; ++Index)
			{
				const FVector Side = FVector::CrossProduct(Along, FVector::UpVector) * (-80.f + Index * 80.f);
				UGBCombatLibrary::OnShotTraced().Broadcast(Eye + Side - FVector(0.f, 0.f, 60.f), Eye + Side - FVector(0.f, 0.f, 60.f) + Along * 1800.f, 0.f);
			}
		});
		Add(FogT + 9.2f, TEXT("Fog: the shots were written, screenshot after"), [](UWorld*, AGBPlayerCharacter*)
		{
			AGBFluidFog* Fog = SmokeFog.Get();
			if (!Fog)
			{
				return;
			}
			if (Fog->HasSystem())
			{
				UE_LOG(LogGB, Display, TEXT("SMOKE Fog after shots: shotEntriesTotal=%d playerEntries=%d collisionEntries=%d sourceEntries=%d"),
					Fog->GetTotalShotEntriesWritten(), Fog->GetLastPlayerEntries(), Fog->GetLastCollisionEntries(), Fog->GetLastSourceEntries());
				if (Fog->GetTotalShotEntriesWritten() <= 0)
				{
					UE_LOG(LogGB, Error, TEXT("SMOKE Fog FAILED: three shots through the box wrote no collision entries"));
				}
			}
			Shot(TEXT("fog_after"));
		});
		Add(FogT + 11.f, TEXT("Fog: two seconds later (the holes close), screenshot"), [](UWorld*, AGBPlayerCharacter*)
		{
			Shot(TEXT("fog_later"));
		});
		Add(FogT + 11.5f, TEXT("Fog: far from the box the simulation stops"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			AGBFluidFog* Fog = SmokeFog.Get();
			if (!Fog)
			{
				return;
			}
			if (UCharacterMovementComponent* Movement = Player->GetCharacterMovement())
			{
				Movement->Velocity = FVector::ZeroVector;
			}
			Player->SetActorLocation(SmokeFogPlayerHome + FVector(0.f, 0.f, 20000.f), false, nullptr, ETeleportType::TeleportPhysics);
		});
		Add(FogT + 12.2f, TEXT("Fog: stopped, then clean up"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			AGBFluidFog* Fog = SmokeFog.Get();
			if (Fog)
			{
				UE_LOG(LogGB, Display, TEXT("SMOKE Fog far away: running=%d"), Fog->IsSimulating() ? 1 : 0);
				if (Fog->IsSimulating())
				{
					UE_LOG(LogGB, Error, TEXT("SMOKE Fog FAILED: the simulation must be off while the player is far from the box"));
				}
				Fog->Destroy();
			}
			SmokeFog.Reset();
			Player->SetActorLocation(SmokeFogPlayerHome, false, nullptr, ETeleportType::TeleportPhysics);
		});
		// A filtered run (gb.Smoke Fog) quits here. The full run goes on to the quit step.
		Add(FogT + 13.f, TEXT("Fog: done"), [](UWorld*, AGBPlayerCharacter*)
		{
			if (bFilteredRun)
			{
				UE_LOG(LogGB, Display, TEXT("SMOKE DONE"));
				FPlatformMisc::RequestExit(false);
			}
		});
		// FogVisit: a state machine on its own ticker that quits the run itself (only in a run filtered on FogVisit; see FogVisitRun).
		Add(FogT + 14.f, TEXT("FogVisit: the tunnel smoke on a real map, lit, injected, shot, timed"), FogVisitRun::Begin);
		Add(FogT + 15.f, TEXT("quit"), [](UWorld*, AGBPlayerCharacter*)
		{
			UE_LOG(LogGB, Display, TEXT("SMOKE DONE"));
			FPlatformMisc::RequestExit(false);
		});
	}

	static bool Tick(float)
	{
		UWorld* World = SmokeWorld.Get();
		if (!World)
		{
			UE_LOG(LogGB, Error, TEXT("SMOKE aborted: the world went away"));
			return false;
		}
		if (bHoldClock)
		{
			StartSeconds = FPlatformTime::Seconds() - HoldElapsed;
		}
		// Read the clock again for every step: a step may wind StartSeconds back (the reload steps stretch the run).
		while (NextStep < Steps.Num() && Steps[NextStep].Time <= static_cast<float>(FPlatformTime::Seconds() - StartSeconds))
		{
			const FStep& Gate = Steps[NextStep];
			if (Gate.MinSimSinceMark > 0.f && World->GetTimeSeconds() - SimMark < Gate.MinSimSinceMark
				&& FPlatformTime::Seconds() - StartSeconds - Gate.Time < SimGateTimeoutSeconds)
			{
				break; // not enough simulated time since the first sample yet: try again next tick
			}
			const FStep& Step = Steps[NextStep++];
			AGBPlayerCharacter* Player = AGBPlayerCharacter::Get(World);
			UE_LOG(LogGB, Display, TEXT("SMOKE step %d/%d (%.1f s): %s"), NextStep, Steps.Num(), Step.Time, Step.Name);
			if (!Player)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE step skipped: there is no player character"));
				continue;
			}
			Step.Run(World, Player);
		}
		return NextStep < Steps.Num();
	}

	static void Start(const TArray<FString>& Args, UWorld* World)
	{
		if (!World || !World->IsGameWorld())
		{
			UE_LOG(LogGB, Warning, TEXT("gb.Smoke needs a running game world"));
			return;
		}
		BuildSteps();
		if (Args.Num() > 0)
		{
			// Filtered run (gb.Smoke Entity): only the steps whose name starts with the argument, plus the quit step, from 1 s on.
			const FString Prefix = Args[0];
			bFilteredRun = true;
			FogVisitRun::bActive = Prefix.StartsWith(TEXT("FogVisit"), ESearchCase::IgnoreCase);
			Steps.RemoveAll([&Prefix](const FStep& Step) { return !FString(Step.Name).StartsWith(Prefix, ESearchCase::IgnoreCase) && FCString::Strcmp(Step.Name, TEXT("quit")) != 0; });
			if (Steps.Num() <= 1)
			{
				UE_LOG(LogGB, Warning, TEXT("gb.Smoke %s: no step starts with that name"), *Prefix);
			}
			float FirstTime = Steps.Num() > 0 ? Steps[0].Time : 0.f;
			for (const FStep& Step : Steps)
			{
				FirstTime = FMath::Min(FirstTime, Step.Time);
			}
			for (FStep& Step : Steps)
			{
				Step.Time -= FirstTime - 1.f;
				// A FogVisit run quits by itself when its state machine is done; the quit step is only a far safety net.
				if (FogVisitRun::bActive && FCString::Strcmp(Step.Name, TEXT("quit")) == 0)
				{
					Step.Time = 100000.f;
				}
			}
		}
		NextStep = 0;
		StartSeconds = FPlatformTime::Seconds();
		SmokeWorld = World;
		FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
		TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&Tick));
		UE_LOG(LogGB, Display, TEXT("SMOKE started: %d steps"), Steps.Num());
	}

	static FAutoConsoleCommandWithWorldAndArgs SmokeCommand(
		TEXT("gb.Smoke"),
		TEXT("Scripted run of the main systems: logs lines starting with SMOKE, saves screenshots to Saved/Screenshots/Smoke and quits. gb.Smoke <Prefix> runs only the steps whose name starts with it (gb.Smoke Entity)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Start));

	static void RunBuildUp(const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() < 2)
		{
			UE_LOG(LogGB, Warning, TEXT("gb.BuildUp <Fire|Lightning|Ice> <Amount>"));
			return;
		}
		EGBElement Element = EGBElement::Fire;
		if (Args[0].Equals(TEXT("Fire"), ESearchCase::IgnoreCase))
		{
			Element = EGBElement::Fire;
		}
		else if (Args[0].Equals(TEXT("Lightning"), ESearchCase::IgnoreCase))
		{
			Element = EGBElement::Lightning;
		}
		else if (Args[0].Equals(TEXT("Ice"), ESearchCase::IgnoreCase))
		{
			Element = EGBElement::Ice;
		}
		else
		{
			UE_LOG(LogGB, Warning, TEXT("gb.BuildUp: unknown element '%s' (Fire|Lightning|Ice)"), *Args[0]);
			return;
		}
		const float Amount = FCString::Atof(*Args[1]);
		AGBPlayerCharacter* Player = AGBPlayerCharacter::Get(World);
		if (!Player)
		{
			UE_LOG(LogGB, Warning, TEXT("gb.BuildUp: no player character"));
			return;
		}
		const FVector PlayerLoc = Player->GetActorLocation();
		for (TActorIterator<AGBEnemyCharacter> It(World); It; ++It)
		{
			AGBEnemyCharacter* Enemy = *It;
			if (!Enemy || Enemy->IsDead())
			{
				continue;
			}
			if (FVector::Dist(PlayerLoc, Enemy->GetActorLocation()) > 1500.f)
			{
				continue;
			}
			FGBDamageInfo Info;
			Info.Element = Element;
			Info.Instigator = Player;
			Info.BuildUp = Amount;
			Info.BuildUpEventId = UGBCombatLibrary::NewBuildUpEventId();
			UGBCombatLibrary::ApplyBuildUp(Enemy, Info);
			if (const UGBElementalStatusComponent* Elemental = Enemy->GetElementalStatus())
			{
				UE_LOG(LogGB, Display, TEXT("BuildUp %s: %.1f/%.1f phase=%d"),
					*Enemy->GetEnemyId().ToString(),
					Elemental->GetBuildUp(Element), Elemental->GetThreshold(Element),
					static_cast<int32>(Elemental->GetPhase(Element)));
			}
		}
	}

	static FAutoConsoleCommandWithWorldAndArgs BuildUpCommand(
		TEXT("gb.BuildUp"),
		TEXT("Adds raw elemental build-up to every living enemy within 1500 cm: gb.BuildUp <Fire|Lightning|Ice> <Amount>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunBuildUp));

	static void RunSpawnEntity(UWorld* World)
	{
		AGBPlayerCharacter* Player = AGBPlayerCharacter::Get(World);
		if (!Player)
		{
			UE_LOG(LogGB, Warning, TEXT("gb.SpawnEntity: no player character"));
			return;
		}
		FVector Aim = Player->GetAimDirection();
		Aim.Z = 0.f;
		if (Aim.IsNearlyZero())
		{
			Aim = Player->GetActorForwardVector();
		}
		const FVector At = Player->GetActorLocation() + Aim.GetSafeNormal() * 1200.f;
		EGBEraPresence Presence = EGBEraPresence::Both;
		if (UGBEraSubsystem* Era = UGBEraSubsystem::Get(World))
		{
			Presence = Era->GetCurrentEra() == EGBEra::Past ? EGBEraPresence::PastOnly : EGBEraPresence::FutureOnly;
		}
		if (!AGBEnemyCharacter::SpawnEnemy(World, FName(TEXT("Entity_Phase1")), At, FRotator::ZeroRotator, Presence))
		{
			UE_LOG(LogGB, Warning, TEXT("gb.SpawnEntity: spawn of Entity_Phase1 failed"));
			return;
		}
		UE_LOG(LogGB, Display, TEXT("gb.SpawnEntity: spawned Entity_Phase1 at %s"), *At.ToCompactString());
	}

	static FAutoConsoleCommandWithWorld SpawnEntityCommand(
		TEXT("gb.SpawnEntity"),
		TEXT("Spawns Entity_Phase1 12 m in front of the player, in the player's current era."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&RunSpawnEntity));

	// ---- gb.God, gb.OpenValkyrie, gb.OpenDoors ----
	static void RunGod(const TArray<FString>& Args, UWorld* World)
	{
		AGBPlayerCharacter* Player = AGBPlayerCharacter::Get(World);
		if (!Player || !Player->GetAttributes())
		{
			UE_LOG(LogGB, Warning, TEXT("gb.God: no player character"));
			return;
		}
		const bool bNow = Player->GetAttributes()->IsPlainInvulnerable();
		const bool bWanted = Args.Num() > 0 ? (Args[0].ToBool() || Args[0] == TEXT("1")) : !bNow;
		Player->GetAttributes()->SetInvulnerable(bWanted);
		UE_LOG(LogGB, Display, TEXT("gb.God: invulnerable=%d"), bWanted ? 1 : 0);
		if (AGBHUD* HUD = AGBHUD::Get(World))
		{
			HUD->ShowMessage(bWanted ? TEXT("God mode: ON") : TEXT("God mode: OFF"));
		}
	}

	static FAutoConsoleCommandWithWorldAndArgs GodCommand(
		TEXT("gb.God"),
		TEXT("Player invulnerability: gb.God [0|1] (toggles without an argument)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunGod));

	static int32 SetGateFlags(UWorld* World)
	{
		UGBProgressSubsystem* Progress = UGBProgressSubsystem::Get(World);
		int32 Count = 0;
		if (Progress)
		{
			for (TActorIterator<AGBEnergyGate> It(World); It; ++It)
			{
				for (const FName& Flag : It->RequiredFlags)
				{
					if (!Flag.IsNone() && !Progress->HasFlag(Flag))
					{
						Progress->SetFlag(Flag, true);
						++Count;
					}
				}
			}
		}
		return Count;
	}

	static void RunOpenValkyrie(UWorld* World)
	{
		const int32 Count = SetGateFlags(World);
		UE_LOG(LogGB, Display, TEXT("gb.OpenValkyrie: set %d flag(s) required by the energy gates"), Count);
		if (AGBHUD* HUD = AGBHUD::Get(World))
		{
			HUD->ShowMessage(TEXT("The way to the Valkyrie is open"));
		}
	}

	static FAutoConsoleCommandWithWorld OpenValkyrieCommand(
		TEXT("gb.OpenValkyrie"),
		TEXT("Sets every progress flag the energy gates of this map require (the way to the Valkyrie's zone opens)."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&RunOpenValkyrie));

	static void RunOpenDoors(UWorld* World)
	{
		int32 Count = SetGateFlags(World);
		if (UGBProgressSubsystem* Progress = UGBProgressSubsystem::Get(World))
		{
			for (TActorIterator<AGBDoor> It(World); It; ++It)
			{
				if (!It->RequiredFlag.IsNone() && !Progress->HasFlag(It->RequiredFlag))
				{
					Progress->SetFlag(It->RequiredFlag, true);
					++Count;
				}
			}
		}
		UE_LOG(LogGB, Display, TEXT("gb.OpenDoors: set %d flag(s) (gates and doors)"), Count);
	}

	static FAutoConsoleCommandWithWorld OpenDoorsCommand(
		TEXT("gb.OpenDoors"),
		TEXT("Sets every progress flag required by a flagged door or energy gate of this map."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&RunOpenDoors));

	// ---- gb.ViewPose, gb.ViewFreeze, gb.HandOverlay ----
	static FAutoConsoleCommandWithWorldAndArgs ViewPoseCommand(
		TEXT("gb.ViewPose"),
		TEXT("Held gun's rest pose as Weapons.json lines (no args) or sets it live: gb.ViewPose X Y Z Pitch Yaw Roll Scale (until re-equip)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunViewPose));

	static FAutoConsoleCommandWithWorldAndArgs ViewFreezeCommand(
		TEXT("gb.ViewFreeze"),
		TEXT("Freezes the walk bob of gun and hands for clean template screenshots: gb.ViewFreeze 1 (0 releases)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunViewFreeze));

	static FAutoConsoleCommandWithWorldAndArgs HandOverlayCommand(
		TEXT("gb.HandOverlay"),
		TEXT("Draws a PNG (absolute path, 480x270) as the hands layer over the held gun; no argument clears it."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunHandOverlay));
}

#endif // !UE_BUILD_SHIPPING
