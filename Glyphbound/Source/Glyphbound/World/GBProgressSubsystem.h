// Run progress that must outlive the pawn and the level: progress flags and the last checkpoint.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Core/GBTypes.h"
#include "Weapons/GBHeldItem.h"
#include "GBProgressSubsystem.generated.h"

class AGBPlayerCharacter;

/**
 * Everything of the player that a checkpoint (or a map change to the final-boss dimension) must bring back.
 * Each player component fills / reads its own part with SaveToSnapshot / LoadFromSnapshot.
 */
USTRUCT()
struct GLYPHBOUND_API FGBPlayerSnapshot
{
	GENERATED_BODY()

	UPROPERTY() bool bValid = false;
	UPROPERTY() FTransform Transform;
	UPROPERTY() FRotator ControlRotation = FRotator::ZeroRotator;
	UPROPERTY() EGBEra Era = EGBEra::Past;
	UPROPERTY() float Health = 0.f;
	UPROPERTY() float Mana = 0.f;

	// Equipment
	/** Weapon ids held, any order (the slot comes from each definition). */
	UPROPERTY() TArray<FName> Weapons;
	UPROPERTY() TMap<FName, FGBAmmoState> Ammo;
	UPROPERTY() TMap<FName, EGBElement> WeaponElements;
	UPROPERTY() EGBHeldSlot ActiveSlot = EGBHeldSlot::Staff;

	// Spellbook
	UPROPERTY() TArray<FName> KnownSpells;
	UPROPERTY() TArray<FName> Loadout;
	UPROPERTY() EGBElement EnchantElement = EGBElement::Fire;

	// Inventory
	UPROPERTY() TMap<FName, int32> Items;
	UPROPERTY() FName QuickUseItem = NAME_None;
	UPROPERTY() FName EquippedArmor = NAME_None;

	// Skill trees
	UPROPERTY() int32 SkillPoints = 0;
	UPROPERTY() TMap<FName, int32> SkillRanks;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FGBOnFlagChanged, FName /*Flag*/);

/**
 * Saving is checkpoints only, in memory (GDD 0.1): nothing is written to disk between sessions.
 *  - Flags: named booleans for everything that happened ("Encounter.Courtyard.Cleared", "GotFirstGun",
 *    "FinalPortalOpen"). UGBEraComponent, encounters and doors read them. Flags are NOT rolled back on death
 *    (enemies do not respawn; a cleared zone stays cleared).
 *  - Checkpoint: a FGBPlayerSnapshot taken automatically (portals, checkpoint volumes, encounter cleared).
 */
UCLASS()
class GLYPHBOUND_API UGBProgressSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UGBProgressSubsystem* Get(const UObject* WorldContextObject);

	// ---- Flags ----
	bool HasFlag(FName Flag) const;
	/** Broadcasts OnFlagChanged if the value really changed. */
	void SetFlag(FName Flag, bool bValue = true);
	const TSet<FName>& GetFlags() const { return Flags; }

	// ---- Checkpoints ----
	/** Takes a snapshot of the player (all components + transform + era). */
	void SaveCheckpoint(AGBPlayerCharacter* Player);
	bool HasCheckpoint() const { return Checkpoint.bValid; }
	const FGBPlayerSnapshot& GetCheckpoint() const { return Checkpoint; }
	/** Puts the player back to the checkpoint state (also revives and switches era, no fade). False if there is none. */
	bool RestoreCheckpoint(AGBPlayerCharacter* Player);

	/** New run: clears flags and the checkpoint. */
	void ResetProgress();

	FGBOnFlagChanged OnFlagChanged;
	FSimpleMulticastDelegate OnCheckpointSaved;

protected:
	UPROPERTY() TSet<FName> Flags;
	UPROPERTY() FGBPlayerSnapshot Checkpoint;
};
