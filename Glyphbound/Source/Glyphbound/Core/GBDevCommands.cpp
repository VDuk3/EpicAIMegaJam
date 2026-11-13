// Developer console commands. "gb.Smoke" plays a scripted run of the main systems with no human input:
// it logs what it does (lines starting with "SMOKE"), saves screenshots and quits. AI agents may not run
// Play In Editor, so this is how a build is checked for crashes and obvious breakage before a person plays.
// Usage: Glyphbound.exe -ExecCmds="gb.Smoke"   (or UnrealEditor.exe <project> -game -ExecCmds="gb.Smoke")
// Screenshots: <Project>/Saved/Screenshots/Smoke/*.png. Not compiled in Shipping builds.
#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "Containers/Ticker.h"
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
#include "Combat/GBCombatLibrary.h"
#include "Combat/GBElementalStatusComponent.h"
#include "Combat/GBStatusEffectComponent.h"
#include "Core/GBStatNames.h"
#include "Core/GBTypes.h"
#include "Data/GBDataSubsystem.h"
#include "Items/GBInventoryComponent.h"
#include "Player/GBPlayerCharacter.h"
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

namespace GBSmokeRun
{
	struct FStep
	{
		float Time = 0.f;
		const TCHAR* Name = TEXT("");
		TFunction<void(UWorld*, AGBPlayerCharacter*)> Run;
	};

	static TArray<FStep> Steps;
	static int32 NextStep = 0;
	static double StartSeconds = 0.0;
	static TWeakObjectPtr<UWorld> SmokeWorld;
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

	static AGBGun* RifleInHand(AGBPlayerCharacter* Player)
	{
		AGBGun* Gun = Cast<AGBGun>(Player->GetEquipment()->GetActiveItem());
		return (Gun && Gun->GetWeaponId() == FName(TEXT("Rifle"))) ? Gun : nullptr;
	}

	static void Shot(const TCHAR* Name)
	{
		const FString Path = FPaths::ProjectSavedDir() / TEXT("Screenshots/Smoke") / (FString(Name) + TEXT(".png"));
		FScreenshotRequest::RequestScreenshot(Path, false, false);
		UE_LOG(LogGB, Display, TEXT("SMOKE screenshot %s"), *Path);
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
		auto Add = [](float Time, const TCHAR* Name, TFunction<void(UWorld*, AGBPlayerCharacter*)> Run)
		{
			FStep Step;
			Step.Time = Time;
			Step.Name = Name;
			Step.Run = MoveTemp(Run);
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
				if (Data->GetSkillNodes().Num() != 22)
				{
					UE_LOG(LogGB, Error, TEXT("SMOKE skills: expected 22 skill nodes, found %d"), Data->GetSkillNodes().Num());
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
			UnlockRanks(FName(TEXT("Weapon_FireRate")), 1);
			UnlockRanks(FName(TEXT("Weapon_ReloadSpeed")), 3);
			UnlockRanks(FName(TEXT("Weapon_Headshot")), 1);
			UnlockRanks(FName(TEXT("Weapon_PrecisionRecovery")), 1);
			UnlockRanks(FName(TEXT("Weapon_PistolHeadshot")), 2);
			UE_LOG(LogGB, Display, TEXT("SMOKE weapon perks reload=%d precision=%d conduit=%d deadeye=%d points=%d"),
				Skills->GetRank(FName(TEXT("Weapon_ReloadSpeed"))), Skills->GetRank(FName(TEXT("Weapon_PrecisionRecovery"))),
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
			if (!FMath::IsNearlyEqual(ReloadTime, 0.7f * Base, 0.01f))
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE rapid reload FAILED: %.2f s, expected 0.7 x base (%.2f s)"), ReloadTime, 0.7f * Base);
			}
		});
		Add(16.4f + S, TEXT("screenshot: weapon tree with the new nodes"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			if (AGBPlayerController* PC = Cast<AGBPlayerController>(Player->GetController()))
			{
				PC->SetMenu(EGBMenu::SkillTrees);
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
			for (const FName Node : { FName(TEXT("Weapon_Damage")), FName(TEXT("Weapon_FireRate")), FName(TEXT("Weapon_ReloadSpeed")),
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
				Skills->Unlock(FName(TEXT("Weapon_Headshot")));
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
			Gun->Reload();
		});
		Add(51.1f + S, TEXT("pistol reload: magazine out halfway, screenshot"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			AGBGun* Gun = Cast<AGBGun>(Player->GetEquipment()->GetActiveItem());
			if (!Gun || Gun->GetWeaponId() != FName(TEXT("Pistol")))
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE pistol reload: no pistol in hand"));
				return;
			}
			const float Progress = Gun->GetReloadProgress();
			const float Offset = Gun->GetViewModelMagazineOffsetCm();
			UE_LOG(LogGB, Display, TEXT("SMOKE pistol magazine offset=%.1f"), Offset);
			if (Progress < 0.3f || Progress > 0.7f || FMath::Abs(Offset - 27.f) > 1.f)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE pistol magazine FAILED: offset=%.1f (expected about 27.0) progress=%.2f (expected about 0.5)"), Offset, Progress);
			}
			const FString ReloadText = Gun->GetViewModelDisplayText();
			UE_LOG(LogGB, Display, TEXT("SMOKE pistol display reloading text=%s"), *ReloadText);
			if (ReloadText != TEXT("--"))
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE pistol display FAILED: text '%s' during the reload (expected --)"), *ReloadText);
			}
			Shot(TEXT("13b_pistol_reload"));
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
		});
		Add(54.95f + S, TEXT("pistol done: staff back in hand"), [](UWorld*, AGBPlayerCharacter* Player)
		{
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
			Gun->Reload();
		});
		Add(57.3f + S, TEXT("rifle reload: magazine out halfway, screenshot"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			AGBGun* Gun = RifleInHand(Player);
			if (!Gun)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE rifle reload: no rifle in hand"));
				return;
			}
			const float Progress = Gun->GetReloadProgress();
			const float Offset = Gun->GetViewModelMagazineOffsetCm();
			UE_LOG(LogGB, Display, TEXT("SMOKE rifle magazine offset=%.1f"), Offset);
			if (Progress < 0.3f || Progress > 0.7f || Offset <= 5.f)
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE rifle magazine FAILED: offset=%.1f (expected more than 5.0) progress=%.2f (expected about 0.5)"), Offset, Progress);
			}
			const FString ReloadText = Gun->GetViewModelDisplayText();
			UE_LOG(LogGB, Display, TEXT("SMOKE rifle display reloading text=%s"), *ReloadText);
			if (ReloadText != TEXT("--"))
			{
				UE_LOG(LogGB, Error, TEXT("SMOKE rifle display FAILED: text '%s' during the reload (expected --)"), *ReloadText);
			}
			Shot(TEXT("14b_rifle_reload"));
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
			Shot(TEXT("14e_rifle_fire"));
		});
		Add(61.6f + S, TEXT("rifle done: staff back in hand"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			Player->GetEquipment()->SwitchTo(EGBHeldSlot::Staff);
		});
		Add(62.2f + S, TEXT("quit"), [](UWorld*, AGBPlayerCharacter*)
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
		const float Elapsed = static_cast<float>(FPlatformTime::Seconds() - StartSeconds);
		while (NextStep < Steps.Num() && Steps[NextStep].Time <= Elapsed)
		{
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

	static void Start(UWorld* World)
	{
		if (!World || !World->IsGameWorld())
		{
			UE_LOG(LogGB, Warning, TEXT("gb.Smoke needs a running game world"));
			return;
		}
		BuildSteps();
		NextStep = 0;
		StartSeconds = FPlatformTime::Seconds();
		SmokeWorld = World;
		FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
		TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&Tick));
		UE_LOG(LogGB, Display, TEXT("SMOKE started: %d steps"), Steps.Num());
	}

	static FAutoConsoleCommandWithWorld SmokeCommand(
		TEXT("gb.Smoke"),
		TEXT("Scripted run of the main systems: logs lines starting with SMOKE, saves screenshots to Saved/Screenshots/Smoke and quits."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&Start));

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
}

#endif // !UE_BUILD_SHIPPING
