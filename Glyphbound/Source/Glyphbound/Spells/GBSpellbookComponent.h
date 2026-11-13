// The player's spells: what is known, the 5-slot loadout, the fixed enchant slot, mana, cooldowns, casting.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Spells/GBSpellTypes.h"
#include "QTE/GBQTELogic.h"
#include "GBSpellbookComponent.generated.h"

class AGBSpellActor;
class AGBSpell_Shield;
class AGBHeldItem;
class UGBStatsComponent;
class UGBAttributeComponent;
class UGBEquipmentComponent;
class UGBQTEComponent;
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
	/**
	 * The weapon enchant's QTE starts with the element choice (owner, Nov 13), always in the same places:
	 * A / Square = fire, W / Triangle = lightning, D / Circle = ice. Index i of one goes with index i of the other.
	 */
	static constexpr int32 NumEnchantChoices = 3;
	static EGBQTEKey GetEnchantChoiceKey(int32 Index);
	static EGBElement GetEnchantChoiceElement(int32 Index);

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

	// ---- Summoned shield (GDD 6: the ice shield is a held block, like a first-person RPG shield) ----
	// Casting a shield spell summons the shield for its Duration; it does nothing by itself. The player raises it
	// by holding the Block input (Q / L2 with the staff or the sword in hand) and it only blocks while raised.
	// A summoned shield does not make the spellbook busy: other spells can be cast while it is lowered.
	/** Called by a shield behavior when it starts / ends. A new shield replaces the previous one (which ends). */
	void RegisterShield(AGBSpellActor* Shield);
	void UnregisterShield(AGBSpellActor* Shield);
	/** The Block input. Ignored (and forced to false) when there is no shield or the held item is a gun. */
	void SetBlockHeld(bool bHeld);
	bool HasShield() const;
	bool IsBlockRaised() const;
	/** 0..1 of the shield's remaining health, and seconds until it expires; 0 when there is no shield. */
	float GetShieldHealthFraction() const;
	float GetShieldTimeRemaining() const;
	/** The summoned shield (null if none), e.g. for the HUD to read its spell's Strings (Sprite, Color). */
	AGBSpellActor* GetShieldActor() const;

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
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	bool BeginCast(FName SpellId);
	void FinishCast(FName SpellId, const FGBQTEResult& Result);

	/** QTE delegate target: delegate payloads come after the delegate's own params, so this forwards to FinishCast. */
	void HandleQTEFinished(const FGBQTEResult& Result, FName SpellId);
	/** Common path of TryCastSlot / TryCastEnchant: checks, broadcasts the failure reason, or begins the cast. */
	bool TryCastSpell(FName SpellId);
	void HandleSustainedEnded();
	void HandleActiveItemChanged(AGBHeldItem* NewItem);
	void HandleOwnerDeath(const FGBDamageInfo& KillingBlow);

	bool LearnSpellInternal(FName SpellId, bool bAutoEquip);
	/** Known, defined, castable, not passive, not the enchant. */
	bool IsEquippable(FName SpellId) const;
	void StartCooldown(FName SpellId, float Seconds);

	static FName PassiveSource(FName SpellId);
	void ApplyPassive(FName SpellId);
	void RemoveAllPassives();
	void ReapplyPassives();

	UGBStatsComponent* GetOwnerStats() const;
	UGBAttributeComponent* GetOwnerAttributes() const;
	UGBEquipmentComponent* GetOwnerEquipment() const;
	UGBQTEComponent* GetOwnerQTE() const;
	double GetNow() const;

	FDelegateHandle DataReloadedHandle;
	FDelegateHandle ActiveItemChangedHandle;
	FDelegateHandle DeathHandle;

	UPROPERTY() TArray<FName> KnownSpells;
	UPROPERTY() TArray<FName> Loadout;
	UPROPERTY() FName EnchantSpellId = NAME_None;
	UPROPERTY() EGBElement EnchantElement = EGBElement::Fire;
	/** Spell id -> world time when it is ready again. */
	TMap<FName, double> CooldownEnd;
	TMap<FName, float> CooldownLength;
	UPROPERTY() TObjectPtr<AGBSpellActor> ActiveSustained;
	FName PendingSpellId = NAME_None;

	/** The summoned shield, if any (it ends itself: Duration, broken, death). */
	TWeakObjectPtr<AGBSpell_Shield> Shield;
	/** Block input accepted and the shield raised (forced false when refused). */
	bool bBlockHeld = false;
	/** The shield, if it is still active. */
	AGBSpell_Shield* GetActiveShield() const;
	/** Block is refused with a gun in hand, while a cast / QTE runs, and while dead. */
	bool CanRaiseBlock() const;
	/** Lowers and ends the summoned shield now (death, teardown). */
	void EndShield();
};
