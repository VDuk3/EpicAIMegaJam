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
#include "AI/GBEnemyCharacter.h"
#include "Combat/GBCombatLibrary.h"
#include "Core/GBTypes.h"
#include "Data/GBDataSubsystem.h"
#include "Items/GBInventoryComponent.h"
#include "Player/GBPlayerCharacter.h"
#include "Player/GBPlayerController.h"
#include "Progression/GBSkillTreeComponent.h"
#include "QTE/GBQTEComponent.h"
#include "Spells/GBSpellbookComponent.h"
#include "Stats/GBAttributeComponent.h"
#include "Weapons/GBEquipmentComponent.h"
#include "Weapons/GBHeldItem.h"
#include "World/GBEraSubsystem.h"
#include "World/GBProgressSubsystem.h"

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
		Add(7.8f, TEXT("travel to the future"), [](UWorld* World, AGBPlayerCharacter* Player)
		{
			const bool bOk = UGBEraSubsystem::Get(World)->RequestTravel(EGBEra::Future, Player);
			UE_LOG(LogGB, Display, TEXT("SMOKE travel requested ok=%d"), bOk ? 1 : 0);
		});
		Add(10.5f, TEXT("screenshot: future"), [](UWorld* World, AGBPlayerCharacter* Player)
		{
			LogState(World, Player, TEXT("after travel"));
			Shot(TEXT("3_future_gun"));
		});
		Add(11.7f, TEXT("open the inventory"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			if (AGBPlayerController* PC = Cast<AGBPlayerController>(Player->GetController()))
			{
				PC->SetMenu(EGBMenu::Inventory);
			}
		});
		Add(12.2f, TEXT("screenshot: inventory"), [](UWorld*, AGBPlayerCharacter*) { Shot(TEXT("4_inventory")); });
		Add(13.4f, TEXT("open the skill trees and unlock a node"), [](UWorld* World, AGBPlayerCharacter* Player)
		{
			if (AGBPlayerController* PC = Cast<AGBPlayerController>(Player->GetController()))
			{
				PC->SetMenu(EGBMenu::SkillTrees);
			}
			if (const UGBDataSubsystem* Data = UGBDataSubsystem::Get(World))
			{
				if (Data->GetSkillNodes().Num() > 0)
				{
					const FName Node = Data->GetSkillNodes()[0].Id;
					const EGBSkillUnlockResult Result = Player->GetSkillTree()->Unlock(Node);
					UE_LOG(LogGB, Display, TEXT("SMOKE unlock %s result=%d rank=%d points=%d"), *Node.ToString(), static_cast<int32>(Result), Player->GetSkillTree()->GetRank(Node), Player->GetSkillTree()->GetPoints());
				}
			}
		});
		Add(13.9f, TEXT("screenshot: skill trees"), [](UWorld*, AGBPlayerCharacter*) { Shot(TEXT("5_skills")); });
		Add(15.1f, TEXT("close the menu, staff in hand, cast with the QTE on"), [](UWorld*, AGBPlayerCharacter* Player)
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
		Add(15.5f, TEXT("QTE: press one key"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			if (Player->GetQTE()->IsActive() && Player->GetQTE()->GetSession().Sequence.Num() > 0)
			{
				Player->GetQTE()->HandleKey(Player->GetQTE()->GetSession().Sequence[0]);
			}
		});
		Add(15.8f, TEXT("screenshot: QTE"), [](UWorld*, AGBPlayerCharacter*) { Shot(TEXT("6_qte")); });
		Add(19.0f, TEXT("after the QTE time limit"), [](UWorld* World, AGBPlayerCharacter* Player)
		{
			const FGBQTEResult& Result = Player->GetQTE()->GetLastResult();
			UE_LOG(LogGB, Display, TEXT("SMOKE qte finished active=%d power=%.2f reach=%.2f success=%d timedOut=%d dilation=%.2f"),
				Player->GetQTE()->IsActive() ? 1 : 0, Result.Power, Result.Reach, Result.bSuccess ? 1 : 0, Result.bTimedOut ? 1 : 0,
				World->GetWorldSettings() ? World->GetWorldSettings()->GetEffectiveTimeDilation() : -1.f);
		});
		Add(19.3f, TEXT("kill the player"), [](UWorld*, AGBPlayerCharacter* Player)
		{
			FGBDamageInfo Info;
			Info.Amount = 10000000.f;
			UGBCombatLibrary::DealDamage(Player, Info, true);
			UE_LOG(LogGB, Display, TEXT("SMOKE player dead=%d"), Player->IsDead() ? 1 : 0);
		});
		Add(20.0f, TEXT("screenshot: death"), [](UWorld*, AGBPlayerCharacter*) { Shot(TEXT("7_death")); });
		Add(24.0f, TEXT("after the respawn"), [](UWorld* World, AGBPlayerCharacter* Player)
		{
			LogState(World, Player, TEXT("after respawn"));
			Shot(TEXT("8_respawn"));
		});
		Add(25.5f, TEXT("quit"), [](UWorld*, AGBPlayerCharacter*)
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
}

#endif // !UE_BUILD_SHIPPING
