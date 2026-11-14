#include "Data/GBDataSubsystem.h"

#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace GBDataPrivate
{
	void ReportError(const FString& Message)
	{
		UE_LOG(LogGB, Error, TEXT("%s"), *Message);
#if !UE_BUILD_SHIPPING
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 15.f, FColor::Red, Message);
		}
#endif
	}

	/** Reads and parses one file of the data folder. False (and an error) if it is missing or not valid JSON. */
	bool ReadJsonFile(const FString& FileName, TSharedPtr<FJsonObject>& OutRoot)
	{
		const FString Path = UGBDataSubsystem::GetDataDir() / FileName;
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *Path))
		{
			ReportError(FString::Printf(TEXT("Data: cannot read %s"), *Path));
			return false;
		}

		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
		if (!FJsonSerializer::Deserialize(Reader, OutRoot) || !OutRoot.IsValid())
		{
			ReportError(FString::Printf(TEXT("Data: %s is not valid JSON: %s"), *FileName, *Reader->GetErrorMessage()));
			OutRoot.Reset();
			return false;
		}
		return true;
	}

	/** Loads the array field Field of FileName into Out. Out is left empty on any error. */
	template <typename T>
	void LoadArray(const FString& FileName, const TCHAR* Field, TArray<T>& Out)
	{
		Out.Reset();

		TSharedPtr<FJsonObject> Root;
		if (!ReadJsonFile(FileName, Root))
		{
			return;
		}

		const TArray<TSharedPtr<FJsonValue>>* JsonArray = nullptr;
		if (!Root->TryGetArrayField(Field, JsonArray) || !JsonArray)
		{
			ReportError(FString::Printf(TEXT("Data: %s has no array \"%s\""), *FileName, Field));
			return;
		}

		FText FailReason;
		if (!FJsonObjectConverter::JsonArrayToUStruct(*JsonArray, &Out, 0, 0, false, &FailReason))
		{
			ReportError(FString::Printf(TEXT("Data: %s \"%s\" could not be read: %s"), *FileName, Field, *FailReason.ToString()));
			Out.Reset();
		}
	}

	/** Loads the whole of FileName into Out. Out is reset to defaults on any error. */
	template <typename T>
	void LoadStruct(const FString& FileName, T& Out)
	{
		Out = T();

		TSharedPtr<FJsonObject> Root;
		if (!ReadJsonFile(FileName, Root))
		{
			return;
		}

		FText FailReason;
		if (!FJsonObjectConverter::JsonObjectToUStruct(Root.ToSharedRef(), &Out, 0, 0, false, &FailReason))
		{
			ReportError(FString::Printf(TEXT("Data: %s could not be read: %s"), *FileName, *FailReason.ToString()));
			Out = T();
		}
	}

	/** Id -> index. Warns about empty and duplicate ids; the first entry with an id wins. */
	template <typename T>
	void BuildIndex(const TArray<T>& Rows, TMap<FName, int32>& OutIndex, const TCHAR* FileName)
	{
		OutIndex.Reset();
		for (int32 i = 0; i < Rows.Num(); ++i)
		{
			const FName Id = Rows[i].Id;
			if (Id.IsNone())
			{
				UE_LOG(LogGB, Warning, TEXT("Data: %s entry %d has an empty Id; it cannot be found by id."), FileName, i);
				continue;
			}
			if (OutIndex.Contains(Id))
			{
				UE_LOG(LogGB, Warning, TEXT("Data: %s has a duplicate Id \"%s\" (entry %d); the first one is used."), FileName, *Id.ToString(), i);
				continue;
			}
			OutIndex.Add(Id, i);
		}
	}

	template <typename T>
	const T* FindIn(const TArray<T>& Rows, const TMap<FName, int32>& Index, FName Id)
	{
		const int32* Found = Index.Find(Id);
		return (Found && Rows.IsValidIndex(*Found)) ? &Rows[*Found] : nullptr;
	}
}

UGBDataSubsystem* UGBDataSubsystem::Get(const UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}
	if (const UGameInstance* DirectInstance = Cast<UGameInstance>(WorldContextObject))
	{
		return DirectInstance->GetSubsystem<UGBDataSubsystem>();
	}
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UGBDataSubsystem>() : nullptr;
}

void UGBDataSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LoadAll();
}

void UGBDataSubsystem::ReloadAll()
{
	LoadAll();
	UE_LOG(LogGB, Log, TEXT("Data: reloaded from %s"), *GetDataDir());
	OnDataReloaded.Broadcast();
}

FString UGBDataSubsystem::GetDataDir()
{
	return FPaths::ProjectContentDir() / TEXT("Data/");
}

void UGBDataSubsystem::LoadAll()
{
	using namespace GBDataPrivate;

	LoadStruct(TEXT("Player.json"), Player);
	LoadStruct(TEXT("QTE.json"), QTE);
	LoadStruct(TEXT("Game.json"), Game);

	LoadArray(TEXT("Spells.json"), TEXT("Spells"), Spells);
	LoadArray(TEXT("Weapons.json"), TEXT("Weapons"), Weapons);
	LoadArray(TEXT("StatusEffects.json"), TEXT("StatusEffects"), StatusEffects);
	LoadArray(TEXT("Items.json"), TEXT("Items"), Items);
	LoadArray(TEXT("Items.json"), TEXT("Recipes"), Recipes);
	LoadArray(TEXT("Enemies.json"), TEXT("Enemies"), Enemies);
	LoadArray(TEXT("Encounters.json"), TEXT("Encounters"), Encounters);
	LoadArray(TEXT("Skills.json"), TEXT("Skills"), SkillNodes);
	LoadArray(TEXT("Sprites.json"), TEXT("Sprites"), Sprites);
	LoadArray(TEXT("Cutscenes.json"), TEXT("Cutscenes"), Cutscenes);

	BuildIndex(Spells, SpellIndex, TEXT("Spells.json"));
	BuildIndex(Weapons, WeaponIndex, TEXT("Weapons.json"));
	BuildIndex(StatusEffects, StatusEffectIndex, TEXT("StatusEffects.json"));
	BuildIndex(Items, ItemIndex, TEXT("Items.json"));
	BuildIndex(Enemies, EnemyIndex, TEXT("Enemies.json"));
	BuildIndex(Encounters, EncounterIndex, TEXT("Encounters.json"));
	BuildIndex(SkillNodes, SkillNodeIndex, TEXT("Skills.json"));
	BuildIndex(Sprites, SpriteIndex, TEXT("Sprites.json"));
	BuildIndex(Cutscenes, CutsceneIndex, TEXT("Cutscenes.json"));

	// Recipes are never looked up by id, but a duplicate is still a data mistake worth a warning.
	TMap<FName, int32> RecipeIndex;
	BuildIndex(Recipes, RecipeIndex, TEXT("Items.json (Recipes)"));

	WarnedGameKeys.Reset();

	ValidateModifierKeys();
}

const FGBSpellDefinition* UGBDataSubsystem::FindSpell(FName Id) const { return GBDataPrivate::FindIn(Spells, SpellIndex, Id); }
const FGBWeaponDefinition* UGBDataSubsystem::FindWeapon(FName Id) const { return GBDataPrivate::FindIn(Weapons, WeaponIndex, Id); }
const FGBStatusEffectDefinition* UGBDataSubsystem::FindStatusEffect(FName Id) const { return GBDataPrivate::FindIn(StatusEffects, StatusEffectIndex, Id); }
const FGBItemDefinition* UGBDataSubsystem::FindItem(FName Id) const { return GBDataPrivate::FindIn(Items, ItemIndex, Id); }
const FGBEnemyDefinition* UGBDataSubsystem::FindEnemy(FName Id) const { return GBDataPrivate::FindIn(Enemies, EnemyIndex, Id); }
const FGBEncounterDefinition* UGBDataSubsystem::FindEncounter(FName Id) const { return GBDataPrivate::FindIn(Encounters, EncounterIndex, Id); }
const FGBSkillNodeDefinition* UGBDataSubsystem::FindSkillNode(FName Id) const { return GBDataPrivate::FindIn(SkillNodes, SkillNodeIndex, Id); }
const FGBSpriteDefinition* UGBDataSubsystem::FindSprite(FName Id) const { return GBDataPrivate::FindIn(Sprites, SpriteIndex, Id); }
const FGBCutsceneDefinition* UGBDataSubsystem::FindCutscene(FName Id) const { return GBDataPrivate::FindIn(Cutscenes, CutsceneIndex, Id); }

void UGBDataSubsystem::ValidateModifierKeys() const
{
	auto SpellParamExists = [this](const FName& SpellId, const FName& Param) -> bool
	{
		if (SpellId == FName(TEXT("All")))
		{
			for (const FGBSpellDefinition& Definition : Spells)
			{
				if (Definition.Params.Contains(Param))
				{
					return true;
				}
			}
			return false;
		}
		const FGBSpellDefinition* Definition = FindSpell(SpellId);
		return Definition && Definition->Params.Contains(Param);
	};

	auto WeaponStatExists = [this](const FString& Scope, const FName& Stat) -> bool
	{
		// The weapon stat names are the "Stats" keys of Weapons.json; "All" and the slot
		// names ("Staff", "Gun", "Sword") match when at least one weapon has the stat.
		if (Scope == TEXT("All") || Scope == TEXT("Staff") || Scope == TEXT("Gun") || Scope == TEXT("Sword"))
		{
			for (const FGBWeaponDefinition& Definition : Weapons)
			{
				if (Scope != TEXT("All"))
				{
					const TCHAR* SlotName = TEXT("");
					switch (Definition.Slot)
					{
					case EGBHeldSlot::Staff: SlotName = TEXT("Staff"); break;
					case EGBHeldSlot::Gun:   SlotName = TEXT("Gun"); break;
					case EGBHeldSlot::Sword:  SlotName = TEXT("Sword"); break;
					}
					if (Scope != SlotName)
					{
						continue;
					}
				}
				if (Definition.Stats.Contains(Stat))
				{
					return true;
				}
			}
			return false;
		}
		const FGBWeaponDefinition* Definition = FindWeapon(FName(*Scope));
		return Definition && Definition->Stats.Contains(Stat);
	};

	auto CheckKey = [&](const FString& Context, const FGBModifierDef& Modifier)
	{
		const FString Key = Modifier.Stat.ToString();
		TArray<FString> Parts;
		if (Key.StartsWith(TEXT("Spell.")))
		{
			Key.ParseIntoArray(Parts, TEXT("."), true);
			if (Parts.Num() != 3 || Parts[1].IsEmpty() || Parts[2].IsEmpty() ||
				!SpellParamExists(FName(*Parts[1]), FName(*Parts[2])))
			{
				UE_LOG(LogGB, Warning, TEXT("Data: %s has unknown spell modifier key '%s'."), *Context, *Key);
			}
		}
		else if (Key.StartsWith(TEXT("Weapon.")))
		{
			Key.ParseIntoArray(Parts, TEXT("."), true);
			if (Parts.Num() != 3 || Parts[1].IsEmpty() || Parts[2].IsEmpty() ||
				!WeaponStatExists(Parts[1], FName(*Parts[2])))
			{
				UE_LOG(LogGB, Warning, TEXT("Data: %s has unknown weapon modifier key '%s'."), *Context, *Key);
			}
		}
		// Anything else (actor stats like "MoveSpeed", "QTE.*"...) is not a data id lookup: skip.
	};

	for (const FGBSkillNodeDefinition& Node : SkillNodes)
	{
		const FString Context = FString::Printf(TEXT("skill node '%s'"), *Node.Id.ToString());
		for (const FGBModifierDef& Modifier : Node.ModifiersPerRank)
		{
			CheckKey(Context, Modifier);
		}
	}
	for (const FGBItemDefinition& Item : Items)
	{
		const FString Context = FString::Printf(TEXT("item '%s'"), *Item.Id.ToString());
		for (const FGBModifierDef& Modifier : Item.Modifiers)
		{
			CheckKey(Context, Modifier);
		}
	}
	for (const FGBStatusEffectDefinition& Effect : StatusEffects)
	{
		const FString Context = FString::Printf(TEXT("status effect '%s'"), *Effect.Id.ToString());
		for (const FGBModifierDef& Modifier : Effect.Modifiers)
		{
			CheckKey(Context, Modifier);
		}
	}
	for (const FGBSpellDefinition& Spell : Spells)
	{
		const FString Context = FString::Printf(TEXT("passive spell '%s'"), *Spell.Id.ToString());
		for (const FGBModifierDef& Modifier : Spell.PassiveModifiers)
		{
			CheckKey(Context, Modifier);
		}
	}
}

float UGBDataSubsystem::GetGameValue(FName Key, float DefaultValue) const
{
	if (const float* Found = Game.Values.Find(Key))
	{
		return *Found;
	}
	if (!WarnedGameKeys.Contains(Key))
	{
		WarnedGameKeys.Add(Key);
		UE_LOG(LogGB, Warning, TEXT("Data: Game.json has no Values \"%s\"; using %f."), *Key.ToString(), DefaultValue);
	}
	return DefaultValue;
}

FString UGBDataSubsystem::GetGameString(FName Key, const FString& DefaultValue) const
{
	if (const FString* Found = Game.Strings.Find(Key))
	{
		return *Found;
	}
	if (!WarnedGameKeys.Contains(Key))
	{
		WarnedGameKeys.Add(Key);
		UE_LOG(LogGB, Warning, TEXT("Data: Game.json has no Strings \"%s\"; using \"%s\"."), *Key.ToString(), *DefaultValue);
	}
	return DefaultValue;
}

FLinearColor UGBDataSubsystem::ParseColor(const FString& Hex, const FLinearColor& DefaultColor)
{
	FString Digits = Hex.TrimStartAndEnd();
	Digits.RemoveFromStart(TEXT("#"));
	if (Digits.Len() != 6 && Digits.Len() != 8)
	{
		return DefaultColor;
	}
	for (const TCHAR Char : Digits)
	{
		if (!FChar::IsHexDigit(Char))
		{
			return DefaultColor;
		}
	}
	return FLinearColor::FromSRGBColor(FColor::FromHex(Digits));
}
