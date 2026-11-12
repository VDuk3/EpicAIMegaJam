// A resolved spell cast and the pure functions that resolve it (unit-tested).
#pragma once

#include "CoreMinimal.h"
#include "Core/GBTypes.h"
#include "Data/GBDataTypes.h"
#include "GBSpellTypes.generated.h"

class UGBStatsComponent;

/**
 * One cast, with every parameter already turned into a number:
 *   data (Spells.json Params) -> QTE scaling (Power / Reach) -> modifiers (skill tree, gear) -> Values.
 * Spell behaviors read only this struct, never the JSON, so any parameter can be retuned or re-tagged in data.
 */
USTRUCT(BlueprintType)
struct GLYPHBOUND_API FGBSpellCast
{
	GENERATED_BODY()

	UPROPERTY() FName SpellId = NAME_None;
	UPROPERTY() EGBElement Element = EGBElement::Arcane;
	/** QTE part 1 score, 0..1. */
	UPROPERTY() float Power = 0.f;
	/** QTE part 2 score, 0..1. */
	UPROPERTY() float Reach = 0.f;
	/** False when the QTE is off: every parameter used its Base value. */
	UPROPERTY() bool bUsedQTE = false;
	UPROPERTY() TMap<FName, float> Values;
	UPROPERTY() TMap<FName, FString> Strings;
	UPROPERTY() TObjectPtr<AActor> Caster = nullptr;
	/** Where the cast starts (the caster's aim origin) and where it points. */
	UPROPERTY() FVector Origin = FVector::ZeroVector;
	UPROPERTY() FVector Direction = FVector::ForwardVector;

	float Get(FName Param, float DefaultValue = 0.f) const;
	bool Has(FName Param) const;
	FString GetString(FName Key, const FString& DefaultValue = FString()) const;

	/** A hit of this spell: Amount = Get(AmountParam), spell element, caster as instigator, SourceId, OnHitEffect + its scales. */
	FGBDamageInfo MakeDamageInfo(FName AmountParam, AActor* Causer = nullptr) const;
};

/** Why a cast did not happen (HUD feedback). */
UENUM(BlueprintType)
enum class EGBCastFailReason : uint8
{
	None,
	UnknownSpell,
	EmptySlot,
	NotKnown,
	WrongHeldItem,
	OnCooldown,
	NotEnoughMana,
	Busy,
	QTEFailed
};

namespace GBSpell
{
	/**
	 * Resolves every parameter of Definition. Stats may be null (no modifiers).
	 * Modifier keys per parameter: "Spell.<Id>.<Param>" and "Spell.All.<Param>".
	 */
	GLYPHBOUND_API FGBSpellCast ResolveCast(const FGBSpellDefinition& Definition, float Power, float Reach, bool bUseQTE, const UGBStatsComponent* Stats);

	/** One parameter, same rules; DefaultValue when the spell does not define it. */
	GLYPHBOUND_API float ResolveParam(const FGBSpellDefinition& Definition, FName Param, float Power, float Reach, bool bUseQTE, const UGBStatsComponent* Stats, float DefaultValue = 0.f);
}
