// Timed effects defined in StatusEffects.json: slow, burn, potions, injection, weapon enchant buffs...
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/GBTypes.h"
#include "GBStatusEffectComponent.generated.h"

/** A running effect, exposed for the HUD. */
struct FGBActiveStatusEffect
{
	FName EffectId = NAME_None;
	float TimeRemaining = 0.f;
	float TotalDuration = 0.f;
	float TimeToNextTick = 0.f;
	float StrengthScale = 1.f;
	int32 Stacks = 1;
	TWeakObjectPtr<AActor> Instigator;
	/** Source name used for its modifiers in UGBStatsComponent. */
	FName ModifierSource = NAME_None;
};

/**
 * Applies data-defined effects to its owner:
 *  - instant health / mana on start (injection),
 *  - periodic health / mana (potions over time, burn),
 *  - stat modifiers while active (slow, enchant), through the owner's UGBStatsComponent.
 * Periodic damage goes through UGBCombatLibrary::DealDamage, so shields and DamageTaken apply.
 */
UCLASS(ClassGroup = (Glyphbound), meta = (BlueprintSpawnableComponent))
class GLYPHBOUND_API UGBStatusEffectComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGBStatusEffectComponent();

	static UGBStatusEffectComponent* Find(const AActor* Actor);

	/**
	 * Starts (or refreshes / stacks, as the definition says) an effect.
	 * DurationScale multiplies the definition's Duration; StrengthScale multiplies the per-tick and instant amounts
	 * and the Add part of its modifiers (Multiply modifiers are scaled around 1: 1 + (Value - 1) * StrengthScale).
	 * Returns false if the id is unknown.
	 */
	bool ApplyEffect(FName EffectId, AActor* Instigator, float DurationScale = 1.f, float StrengthScale = 1.f);

	void RemoveEffect(FName EffectId);
	void RemoveAllEffects();
	bool HasEffect(FName EffectId) const;
	float GetTimeRemaining(FName EffectId) const;

	const TArray<FGBActiveStatusEffect>& GetActiveEffects() const { return ActiveEffects; }

	FSimpleMulticastDelegate OnEffectsChanged;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;

	void HandleOwnerDeath(const FGBDamageInfo& KillingBlow);
	/** Instant or per-tick health / mana on the owner. Negative health is damage through UGBCombatLibrary::DealDamage. */
	void ApplyHealthAndMana(FName EffectId, float HealthAmount, float ManaAmount, EGBElement Element, AActor* EffectInstigator);
	/** Restarts an entry's timer (and keeps the larger strength). */
	void RefreshEntry(int32 Index, float Duration, float StrengthScale, AActor* EffectInstigator);
	/** Keeps FGBActiveStatusEffect::Stacks equal to the number of entries of that id. */
	void UpdateStackCount(FName EffectId);

	TArray<FGBActiveStatusEffect> ActiveEffects;
	int32 NextEffectSerial = 1;
};
