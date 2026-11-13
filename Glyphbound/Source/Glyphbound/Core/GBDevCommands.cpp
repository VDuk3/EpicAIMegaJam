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
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "AI/GBEnemyCharacter.h"
#include "AI/GBEntity.h"
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
#include "Spells/GBSpellbookComponent.h"
#include "Stats/GBAttributeComponent.h"
#include "Stats/GBStatsComponent.h"
#include "Weapons/GBEquipmentComponent.h"
#include "Weapons/GBGun.h"
#include "Weapons/GBHeldItem.h"
#include "World/GBEraSubsystem.h"
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
		Add(U + 43.5f, TEXT("quit"), [](UWorld*, AGBPlayerCharacter*)
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
}

#endif // !UE_BUILD_SHIPPING
