// Health and mana with regeneration, team, damage intake and death. Used by the player and by enemies.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/GBTypes.h"
#include "GBAttributeComponent.generated.h"

class UGBStatsComponent;

DECLARE_MULTICAST_DELEGATE_TwoParams(FGBOnAttributeChanged, float /*NewValue*/, float /*MaxValue*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FGBOnDamaged, const FGBDamageInfo& /*Info*/, float /*ActualDamage*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FGBOnDeath, const FGBDamageInfo& /*KillingBlow*/);
/** Lets shields, blocks and buffs change a hit before it lands (set Info.Amount to 0 to cancel it). */
DECLARE_MULTICAST_DELEGATE_OneParam(FGBOnModifyIncomingDamage, FGBDamageInfo& /*InOutInfo*/);

/**
 * Reads its numbers from the owner's UGBStatsComponent (MaxHealth, HealthRegen, HealthRegenDelay, MaxMana,
 * ManaRegen, ManaRegenDelay, DamageTaken), so modifiers change them live. Regeneration is automatic (GDD 6):
 * it starts HealthRegenDelay seconds after the last damage / ManaRegenDelay seconds after the last spend.
 */
UCLASS(ClassGroup = (Glyphbound), meta = (BlueprintSpawnableComponent))
class GLYPHBOUND_API UGBAttributeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGBAttributeComponent();

	/** The attribute component of an actor, or null. */
	static UGBAttributeComponent* Find(const AActor* Actor);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glyphbound|Attributes")
	EGBTeam Team = EGBTeam::Neutral;

	/** Call once the owner's base stats are set: fills health and mana to their maximum. */
	void InitializeFromStats();

	float GetHealth() const { return Health; }
	float GetMaxHealth() const;
	float GetMana() const { return Mana; }
	float GetMaxMana() const;
	bool IsDead() const { return bDead; }

	/** Runs OnModifyIncomingDamage, applies the DamageTaken stat, subtracts health, fires OnDamaged / OnDeath. Returns the damage really dealt. */
	float ApplyDamage(const FGBDamageInfo& Info);

	void Heal(float Amount);
	void RestoreMana(float Amount);

	bool HasMana(float Amount) const;
	/** Spends mana if there is enough; false and nothing spent otherwise. Restarts the mana regen delay. */
	bool SpendMana(float Amount);

	/** Full health and mana (era travel, GDD 4). Also revives. */
	void RestoreAll();

	/** For checkpoints and snapshots. Clamped to the current maximums. */
	void SetHealthAndMana(float NewHealth, float NewMana);

	/** While true, ApplyDamage does nothing (dash i-frames, cutscenes, respawn). */
	void SetInvulnerable(bool bInInvulnerable) { bInvulnerable = bInInvulnerable; }
	bool IsInvulnerable() const { return bInvulnerable; }

	/** Pure regen step, also used by the tests: the new value after DeltaTime, given the time since the last damage/spend. */
	static float StepRegen(float Current, float Max, float RegenPerSecond, float RegenDelay, float TimeSinceLastLoss, float DeltaTime);

	FGBOnAttributeChanged OnHealthChanged;
	FGBOnAttributeChanged OnManaChanged;
	FGBOnDamaged OnDamaged;
	FGBOnDeath OnDeath;
	FGBOnModifyIncomingDamage OnModifyIncomingDamage;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;

	UGBStatsComponent* GetStats() const;

	float Health = 0.f;
	float Mana = 0.f;
	float TimeSinceDamage = 1000.f;
	float TimeSinceManaSpend = 1000.f;
	bool bDead = false;
	bool bInvulnerable = false;
};
