// Data definitions loaded from Content/Data/*.json by UGBDataSubsystem.
// Rule: gameplay numbers live here (data), never in code. Field names are the JSON keys.
#pragma once

#include "CoreMinimal.h"
#include "Core/GBTypes.h"
#include "GBDataTypes.generated.h"

/**
 * One spell parameter (GDD 7): "every parameter is data and can be tagged as scaled by QTE part 1, part 2, or neither".
 *  - ScaledBy == None, or the QTE is off: the value is Base.
 *  - ScaledBy == Power / Reach and the QTE ran: Lerp(Min, Max, score of that part), score in 0..1.
 */
USTRUCT(BlueprintType)
struct GLYPHBOUND_API FGBScaledParam
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	float Base = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	float Min = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	float Max = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	EGBScaleSource ScaledBy = EGBScaleSource::None;

	/** Upper limit applied after QTE scaling AND modifiers. 0 = no cap. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	float Cap = 0.f;

	/** bUseQTE false = fixed default strength (Base). Power and Reach are clamped to 0..1. */
	float Resolve(float Power, float Reach, bool bUseQTE) const;
};

/** One stat change: skill tree rank, armor, status effect, air passive, weapon enchant... */
USTRUCT(BlueprintType)
struct GLYPHBOUND_API FGBModifierDef
{
	GENERATED_BODY()

	/** Stat key: an actor stat ("MoveSpeed"), "Weapon.<Id>.<Stat>", "Weapon.<Slot>.<Stat>", "Weapon.All.<Stat>", "Spell.<Id>.<Param>", "Spell.All.<Param>", "QTE.TimeDilation"... */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	FName Stat = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	EGBModOp Op = EGBModOp::Add;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	float Value = 0.f;
};

/** Spells.json. A spell = this data + the actor class that gives it behavior. New spells need no new code if a behavior fits. */
USTRUCT(BlueprintType)
struct GLYPHBOUND_API FGBSpellDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	FName Id = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	FString Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	EGBElement Element = EGBElement::Arcane;

	/** Class path of the AGBSpellActor that runs the spell, e.g. "/Script/Glyphbound.GBSpell_Projectile". Empty for passives. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	FString ActorClass;

	/** Air passives (GDD 6): always active once known, never equipped; they only apply PassiveModifiers. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	bool bPassive = false;

	/** The weapon enchant: only castable with a gun in hand, lives in its own fixed slot. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	bool bRequiresGun = false;

	/** Sustained spells (flamethrower, ice shield): they run until Duration ends or the cast input is pressed again. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	bool bSustained = false;

	/** Known from the start of the game (GDD 6 "Starting spells"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	bool bStartsKnown = false;

	/** Spells with the same group are learned together (the knowledge of an elemental zone), e.g. "Fire". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	FName UnlockGroup = NAME_None;

	/** Every tunable number. Keys: see GBStat (ManaCost, Cooldown, Damage, Speed, Range, Radius, Duration...). A behavior may read any key it documents. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	TMap<FName, FGBScaledParam> Params;

	/** Non-numeric settings. Keys: see GBString (Sprite, Color, OnHitEffect...). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	TMap<FName, FString> Strings;

	/** Only for bPassive spells. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	TArray<FGBModifierDef> PassiveModifiers;
};

/** Weapons.json: staffs, guns and the elemental sword. */
USTRUCT(BlueprintType)
struct GLYPHBOUND_API FGBWeaponDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	FName Id = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	EGBHeldSlot Slot = EGBHeldSlot::Gun;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	EGBElement Element = EGBElement::Physical;

	/** Class path of the AGBHeldItem. Empty = default class for the slot (AGBStaff, AGBGun, AGBSword). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	FString ActorClass;

	/** Viewmodel style. Empty = the placeholder shapes; "Pistol" or "Rifle" = the real meshes of that model (Weapons/GBGun). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	FString ViewModel;

	/** "Hitscan", "Projectile" or "Melee". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	FString FireMode = TEXT("Hitscan");

	/** Hold the trigger to keep firing. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	bool bAutomatic = false;

	/** Keys: see GBStat "Weapon stats". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	TMap<FName, float> Stats;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	TMap<FName, FString> Strings;
};

/** StatusEffects.json: slow, burn, heal over time, instant heal, the enchant buffs... */
USTRUCT(BlueprintType)
struct GLYPHBOUND_API FGBStatusEffectDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	FName Id = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	FString DisplayName;

	/** Seconds. 0 = instant (only the Instant* fields apply). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	float Duration = 0.f;

	/** Seconds between ticks of HealthPerTick / ManaPerTick. 0 = no ticking. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	float TickInterval = 0.f;

	/** Positive heals, negative damages (damage uses Element and the effect's instigator). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	float HealthPerTick = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	float ManaPerTick = 0.f;

	/** Applied once when the effect starts. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	float InstantHealth = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	float InstantMana = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	EGBElement Element = EGBElement::Physical;

	/** Stat modifiers active while the effect lasts (slow = MoveSpeed x0.5, enchant = Weapon.All.Damage x1.3...). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	TArray<FGBModifierDef> Modifiers;

	/** Applying it again restarts the timer instead of stacking. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	bool bRefreshOnReapply = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	int32 MaxStacks = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	TMap<FName, FString> Strings;
};

/** Items.json: consumables, throwables, armor, key items, materials (gems, hilt), ammo. */
USTRUCT(BlueprintType)
struct GLYPHBOUND_API FGBItemDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	FName Id = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	FString Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	EGBItemType Type = EGBItemType::Consumable;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	int32 MaxStack = 9;

	/** Goes in the quick-use slot (grenade / potion / injection share one slot, GDD 9). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	bool bQuickUse = false;

	/** Consumable: status effect applied to the user. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	FName UseEffect = NAME_None;

	/** Armor: modifiers while equipped. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	TArray<FGBModifierDef> Modifiers;

	/** Throwable: Damage, Radius, Speed, FuseTime, ProjectileGravity. Ammo: SpareMagazines (how many it gives). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	TMap<FName, float> Stats;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	EGBElement Element = EGBElement::Physical;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	TMap<FName, FString> Strings;
};

/** Items.json "Recipes": hilt + gem = elemental sword, quest item + quest item = final portal (GDD 11). */
USTRUCT(BlueprintType)
struct GLYPHBOUND_API FGBRecipeDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	FName Id = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	FName A = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	FName B = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	bool bConsumeA = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	bool bConsumeB = true;

	/** Any of these may be set. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	FName ResultItem = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	FName ResultWeapon = NAME_None;

	/** Element given to ResultWeapon (the sword takes the gem's element). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	EGBElement ResultElement = EGBElement::Physical;

	/** Progress flag set by the recipe (e.g. "FinalPortalOpen"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	FName SetsFlag = NAME_None;
};

/** One possible drop (enemies, chests). Exactly one of ItemId / WeaponId / SpellGroup is normally set. */
USTRUCT(BlueprintType)
struct GLYPHBOUND_API FGBDropDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	FName ItemId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	FName WeaponId = NAME_None;

	/** Teaches every spell of that UnlockGroup (the knowledge of an elemental zone). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	FName SpellGroup = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	int32 Count = 1;

	/** 0..1 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	float Chance = 1.f;
};

/** Enemies.json. */
USTRUCT(BlueprintType)
struct GLYPHBOUND_API FGBEnemyDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	FName Id = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	FString DisplayName;

	/** Class path of the AGBEnemyCharacter. Empty = AGBEnemyCharacter. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	FString ActorClass;

	/** "Melee" or "Ranged". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	FString AttackMode = TEXT("Melee");

	/** Element of its attacks. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	EGBElement Element = EGBElement::Physical;

	/** Art rule (GDD 3): true = pixel-art sprite that faces the camera, false = 3D mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	bool bPixelArt = true;

	/** Keys: GBStat actor stats + enemy stats (+ ProjectileSpeed, ProjectileRadius for ranged). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	TMap<FName, float> Stats;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	TMap<FName, FString> Strings;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	TArray<FGBDropDefinition> Drops;
};

USTRUCT(BlueprintType)
struct GLYPHBOUND_API FGBEncounterSpawn
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	FName EnemyId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	int32 Count = 1;
};

USTRUCT(BlueprintType)
struct GLYPHBOUND_API FGBEncounterWave
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	TArray<FGBEncounterSpawn> Spawns;

	/** Seconds between the previous wave being cleared and this one spawning. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	float DelayBefore = 1.f;
};

/** Encounters.json: waves, sub-bosses and bosses are all encounters (GDD 10). */
USTRUCT(BlueprintType)
struct GLYPHBOUND_API FGBEncounterDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	FName Id = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	TArray<FGBEncounterWave> Waves;

	/** Skill points for clearing it (GDD 5). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	int32 SkillPoints = 1;

	/** Progress flag set when cleared; also what keeps it from running again (enemies do not respawn). Empty = "Encounter.<Id>.Cleared". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	FName ClearedFlag = NAME_None;

	/** Extra rewards dropped at the encounter's location when cleared. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	TArray<FGBDropDefinition> Rewards;
};

/** Skills.json: nodes of the two skill trees (GDD 5). */
USTRUCT(BlueprintType)
struct GLYPHBOUND_API FGBSkillNodeDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	FName Id = NAME_None;

	/** "Spell" or "Weapon". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	FName Tree = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	FString Description;

	/** Skill points per rank. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	int32 Cost = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	int32 MaxRank = 1;

	/** Node ids that need at least rank 1 first. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	TArray<FName> Requires;

	/** Applied once per rank owned. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	TArray<FGBModifierDef> ModifiersPerRank;

	/** Position in the tree screen. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	int32 Column = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	int32 Row = 0;
};

/** Sprites.json: placeholder pixel art as text. Each row is a string; each character is a key of Palette ('.' or ' ' = transparent). */
USTRUCT(BlueprintType)
struct GLYPHBOUND_API FGBSpriteDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	FName Id = NAME_None;

	/** One-character key -> "#RRGGBB" or "#RRGGBBAA". Keys ignore case (Unreal string keys do): never use both 's' and 'S' in one palette; use digits for extra colors. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	TMap<FString, FString> Palette;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	TArray<FString> Rows;
};

/** Player.json */
USTRUCT(BlueprintType)
struct GLYPHBOUND_API FGBPlayerDefinition
{
	GENERATED_BODY()

	/** Keys: GBStat actor stats. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	TMap<FName, float> Stats;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	TArray<FName> StartingWeapons;

	/** Extra spells known at start, besides the ones with bStartsKnown. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	TArray<FName> StartingSpells;

	/** Up to 5 spell ids, in slot order. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	TArray<FName> StartingLoadout;

	/** Item id -> count. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	TMap<FName, int32> StartingItems;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	int32 StartingSkillPoints = 0;

	/** Id of the weapon enchant spell (fixed slot). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	FName EnchantSpell = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	EGBElement StartingEnchantElement = EGBElement::Fire;
};

/** QTE.json (GDD 7). */
USTRUCT(BlueprintType)
struct GLYPHBOUND_API FGBQTEDefinition
{
	GENERATED_BODY()

	/** false = spells cast at their fixed default strength (every param uses Base). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	bool bEnabled = true;

	/** Keys: QTE.TimeDilation, QTE.TimeLimit, QTE.SequenceLength, QTE.GestureCount, plus the recognizer thresholds. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	TMap<FName, float> Stats;

	/** Gesture names that may be asked for (see EGBGesture). Empty = all. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	TArray<FString> Gestures;
};

/** Game.json: global values that belong to no single system (fade times, respawn delay, test arena switches...). */
USTRUCT(BlueprintType)
struct GLYPHBOUND_API FGBGameDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	TMap<FName, float> Values;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	TMap<FName, FString> Strings;
};
