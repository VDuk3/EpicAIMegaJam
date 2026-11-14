// Loads every Content/Data/*.json file once per game instance and hands out read-only definitions.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Data/GBDataTypes.h"
#include "GBDataSubsystem.generated.h"

/**
 * The single source of gameplay numbers.
 * Files (all under <Project>/Content/Data, shipped as loose files so they can be tuned without repackaging):
 *   Player.json         -> FGBPlayerDefinition (one object)
 *   QTE.json            -> FGBQTEDefinition (one object)
 *   Game.json           -> FGBGameDefinition (one object)
 *   Spells.json         -> { "Spells": [ FGBSpellDefinition ] }
 *   Weapons.json        -> { "Weapons": [ FGBWeaponDefinition ] }
 *   StatusEffects.json  -> { "StatusEffects": [ FGBStatusEffectDefinition ] }
 *   Items.json          -> { "Items": [ FGBItemDefinition ], "Recipes": [ FGBRecipeDefinition ] }
 *   Enemies.json        -> { "Enemies": [ FGBEnemyDefinition ] }
 *   Encounters.json     -> { "Encounters": [ FGBEncounterDefinition ] }
 *   Skills.json         -> { "Skills": [ FGBSkillNodeDefinition ] }
 *   Sprites.json        -> { "Sprites": [ FGBSpriteDefinition ] }
 *   Cutscenes.json      -> { "Cutscenes": [ FGBCutsceneDefinition ] }
 * A missing or broken file logs an error (and an on-screen message in non-shipping builds) and leaves that table empty;
 * it never crashes. Console command "gb.ReloadData" calls ReloadAll() while the game runs.
 */
UCLASS()
class GLYPHBOUND_API UGBDataSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** Null only when there is no game instance (e.g. a unit test without a world). */
	static UGBDataSubsystem* Get(const UObject* WorldContextObject);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** Reads every file again and broadcasts OnDataReloaded. */
	UFUNCTION(BlueprintCallable, Category = "Glyphbound|Data")
	void ReloadAll();

	/** <Project>/Content/Data/ */
	static FString GetDataDir();

	// Pointers stay valid until the next ReloadAll(). Do not keep them across frames: keep the id.
	const FGBSpellDefinition* FindSpell(FName Id) const;
	const FGBWeaponDefinition* FindWeapon(FName Id) const;
	const FGBStatusEffectDefinition* FindStatusEffect(FName Id) const;
	const FGBItemDefinition* FindItem(FName Id) const;
	const FGBEnemyDefinition* FindEnemy(FName Id) const;
	const FGBEncounterDefinition* FindEncounter(FName Id) const;
	const FGBSkillNodeDefinition* FindSkillNode(FName Id) const;
	const FGBSpriteDefinition* FindSprite(FName Id) const;
	const FGBCutsceneDefinition* FindCutscene(FName Id) const;

	// In file order.
	const TArray<FGBSpellDefinition>& GetSpells() const { return Spells; }
	const TArray<FGBWeaponDefinition>& GetWeapons() const { return Weapons; }
	const TArray<FGBStatusEffectDefinition>& GetStatusEffects() const { return StatusEffects; }
	const TArray<FGBItemDefinition>& GetItems() const { return Items; }
	const TArray<FGBRecipeDefinition>& GetRecipes() const { return Recipes; }
	const TArray<FGBEnemyDefinition>& GetEnemies() const { return Enemies; }
	const TArray<FGBEncounterDefinition>& GetEncounters() const { return Encounters; }
	const TArray<FGBSkillNodeDefinition>& GetSkillNodes() const { return SkillNodes; }
	const TArray<FGBSpriteDefinition>& GetSprites() const { return Sprites; }
	const TArray<FGBCutsceneDefinition>& GetCutscenes() const { return Cutscenes; }

	const FGBPlayerDefinition& GetPlayer() const { return Player; }
	const FGBQTEDefinition& GetQTE() const { return QTE; }

	/** Game.json "Values" / "Strings". */
	float GetGameValue(FName Key, float DefaultValue) const;
	FString GetGameString(FName Key, const FString& DefaultValue) const;

	/** "#RRGGBB" or "#RRGGBBAA" -> color; DefaultColor when the text is empty or invalid. */
	static FLinearColor ParseColor(const FString& Hex, const FLinearColor& DefaultColor);

	/** Fired after ReloadAll() so systems can re-read their base stats. */
	FSimpleMulticastDelegate OnDataReloaded;

protected:
	UPROPERTY() TArray<FGBSpellDefinition> Spells;
	UPROPERTY() TArray<FGBWeaponDefinition> Weapons;
	UPROPERTY() TArray<FGBStatusEffectDefinition> StatusEffects;
	UPROPERTY() TArray<FGBItemDefinition> Items;
	UPROPERTY() TArray<FGBRecipeDefinition> Recipes;
	UPROPERTY() TArray<FGBEnemyDefinition> Enemies;
	UPROPERTY() TArray<FGBEncounterDefinition> Encounters;
	UPROPERTY() TArray<FGBSkillNodeDefinition> SkillNodes;
	UPROPERTY() TArray<FGBSpriteDefinition> Sprites;
	UPROPERTY() TArray<FGBCutsceneDefinition> Cutscenes;
	UPROPERTY() FGBPlayerDefinition Player;
	UPROPERTY() FGBQTEDefinition QTE;
	UPROPERTY() FGBGameDefinition Game;

private:
	/** Reads every file and rebuilds the indices (no broadcast). */
	void LoadAll();

	/** Warns about "Spell.<Id>.<Param>" / "Weapon.<Scope>.<Stat>" modifier keys that name nothing, so a typo can never silently do nothing. */
	void ValidateModifierKeys() const;

	// Id -> position in the matching array, rebuilt by LoadAll().
	TMap<FName, int32> SpellIndex;
	TMap<FName, int32> WeaponIndex;
	TMap<FName, int32> StatusEffectIndex;
	TMap<FName, int32> ItemIndex;
	TMap<FName, int32> EnemyIndex;
	TMap<FName, int32> EncounterIndex;
	TMap<FName, int32> SkillNodeIndex;
	TMap<FName, int32> SpriteIndex;
	TMap<FName, int32> CutsceneIndex;

	/** Game.json keys already reported as missing, so a per-frame read warns only once. */
	mutable TSet<FName> WarnedGameKeys;
};
