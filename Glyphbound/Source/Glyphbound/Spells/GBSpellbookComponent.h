// The player's spells: what is known, the 5-slot loadout, the fixed enchant slot, mana, cooldowns, casting.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Spells/GBSpellTypes.h"
#include "GBSpellbookComponent.generated.h"

class AGBSpellActor;
struct FGBPlayerSnapshot;
struct FGBQTEResult;

DECLARE_MULTICAST_DELEGATE_OneParam(FGBOnSpellCast, const FGBSpellCast& /*Cast*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FGBOnCastFailed, FName /*SpellId*/, EGBCastFailReason /*Reason*/);

/**
 * Cast flow (GDD 6 and 7):
 *   TryCastSlot / TryCastEnchant -> checks (held item, known, cooldown, mana, not busy)
 *   -> mana is spent now ("mana is always spent, even when the spell fails")
 *   -> the owner's UGBQTEComponent runs (or returns at once when the QTE is off)
 *   -> if at least one QTE step was right: ResolveCast + spawn the behavior actor; cooldown starts either way.
 * Rules: spells need the staff in hand; the enchant needs a gun in hand; air passives are never cast, they just
 * apply their modifiers while known.
 */
UCLASS(ClassGroup = (Glyphbound), meta = (BlueprintSpawnableComponent))
class GLYPHBOUND_API UGBSpellbookComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	static constexpr int32 NumSlots = 5;

	UGBSpellbookComponent();

	/** Learns the starting spells and loadout from Player.json / Spells.json. Called by the player on BeginPlay. */
	void InitializeFromData();

	// ---- Knowledge ----
	bool KnowsSpell(FName SpellId) const;
	/** Returns true if it was new. Passives start applying their modifiers at once. */
	bool LearnSpell(FName SpellId);
	/** Learns every spell whose UnlockGroup matches (the knowledge of an elemental zone). Returns how many were new. */
	int32 LearnGroup(FName UnlockGroup);
	const TArray<FName>& GetKnownSpells() const { return KnownSpells; }

	// ---- Loadout ----
	/** NAME_None for an empty slot. Slot 0..NumSlots-1. */
	FName GetSlotSpell(int32 Slot) const;
	/** Only known, non-passive, non-enchant spells. A spell already in another slot is moved. NAME_None clears the slot. */
	bool EquipSpell(int32 Slot, FName SpellId);
	FName GetEnchantSpell() const { return EnchantSpellId; }
	EGBElement GetEnchantElement() const { return EnchantElement; }
	void SetEnchantElement(EGBElement InElement);

	// ---- Casting ----
	/** Slot 0 = right click / L1, slots 1-4 = keys 1-4. If a sustained spell of that slot is running, this ends it instead. */
	bool TryCastSlot(int32 Slot);
	bool TryCastEnchant();
	/** Why the spell could not be cast right now; None if it can. */
	EGBCastFailReason CanCast(FName SpellId) const;
	/** A QTE is running or a sustained spell is active. */
	bool IsBusy() const;
	/** Ends the active sustained spell, if any (weapon switch, death). */
	void CancelSustained();

	/** Seconds left / total, for the HUD. */
	float GetCooldownRemaining(FName SpellId) const;
	float GetCooldownDuration(FName SpellId) const;
	/** Mana cost right now (with modifiers, at default strength). */
	float GetManaCost(FName SpellId) const;

	void SaveToSnapshot(FGBPlayerSnapshot& Snapshot) const;
	void LoadFromSnapshot(const FGBPlayerSnapshot& Snapshot);

	FGBOnSpellCast OnSpellCast;
	FGBOnCastFailed OnCastFailed;
	FSimpleMulticastDelegate OnLoadoutChanged;

protected:
	bool BeginCast(FName SpellId);
	void FinishCast(FName SpellId, const FGBQTEResult& Result);

	UPROPERTY() TArray<FName> KnownSpells;
	UPROPERTY() TArray<FName> Loadout;
	UPROPERTY() FName EnchantSpellId = NAME_None;
	UPROPERTY() EGBElement EnchantElement = EGBElement::Fire;
	/** Spell id -> world time when it is ready again. */
	TMap<FName, double> CooldownEnd;
	TMap<FName, float> CooldownLength;
	UPROPERTY() TObjectPtr<AGBSpellActor> ActiveSustained;
	FName PendingSpellId = NAME_None;
};
