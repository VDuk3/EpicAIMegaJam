// The stat system: base values from data + stacked modifiers. One per actor that has tunable numbers.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/GBDataTypes.h"
#include "GBStatsComponent.generated.h"

/**
 * How every tunable number is read in the game:
 *   value = Evaluate(Key, Base)  ->  (Base + sum of Add) * product of Multiply, or the last Override if any.
 * - Actor stats (MoveSpeed, MaxHealth...): base values are stored here with SetBaseStats and read with GetStat.
 * - Weapon and spell numbers: the base comes from the weapon / spell definition and the caller passes the
 *   modifier keys ("Weapon.<Id>.<Stat>", "Weapon.All.<Stat>", "Spell.<Id>.<Param>", "Spell.All.<Param>").
 * Modifiers are added in groups identified by a Source name ("Skill.FireballDamage", "Armor", "Effect.Slow#3",
 * "Passive.DoubleJump"), and removed by that same name. Skill trees, armor, status effects, air passives and
 * the weapon enchant are all just sources of modifiers.
 */
UCLASS(ClassGroup = (Glyphbound), meta = (BlueprintSpawnableComponent))
class GLYPHBOUND_API UGBStatsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGBStatsComponent();

	/** The stats component of an actor, or null. */
	static UGBStatsComponent* Find(const AActor* Actor);

	/** Replaces the base stats (from Player.json / Enemies.json) and broadcasts OnStatsChanged. */
	void SetBaseStats(const TMap<FName, float>& InBaseStats);
	void SetBaseStat(FName Stat, float Value);
	bool HasBaseStat(FName Stat) const;

	/** Base stat of this actor with its modifiers applied. DefaultValue is the base when the stat is not in the data. */
	UFUNCTION(BlueprintCallable, Category = "Glyphbound|Stats")
	float GetStat(FName Stat, float DefaultValue = 0.f) const;

	/** Applies the modifiers registered under Key to an external base value. */
	float Evaluate(FName Key, float Base) const;

	/** Same, merging the modifiers of several keys (e.g. "Weapon.Pistol.Damage" and "Weapon.All.Damage"). */
	float EvaluateKeys(TConstArrayView<FName> Keys, float Base) const;

	/** Adds a group of modifiers. Calling it again with the same Source replaces that source's modifiers. */
	void AddModifiers(FName Source, TConstArrayView<FGBModifierDef> Modifiers, float ValueScale = 1.f);
	void RemoveModifiers(FName Source);
	bool HasModifierSource(FName Source) const;

	/** Pure math, also used by the tests: applies Modifiers (already filtered to one stat) to Base. */
	static float ApplyModifiers(float Base, TConstArrayView<FGBModifierDef> Modifiers);

	/** Base stats or modifiers changed: re-read whatever you cached (walk speed, max jumps...). */
	FSimpleMulticastDelegate OnStatsChanged;

protected:
	struct FSourcedModifier
	{
		FName Source;
		FGBModifierDef Modifier;
	};

	UPROPERTY()
	TMap<FName, float> BaseStats;

	TArray<FSourcedModifier> ActiveModifiers;
};
