#include "Stats/GBAttributeComponent.h"

#include "Core/GBStatNames.h"
#include "GameFramework/Actor.h"
#include "Stats/GBStatsComponent.h"

namespace GBAttributePrivate
{
	// Fallbacks only for an actor whose data has no MaxHealth / MaxMana (a warning is logged).
	constexpr float FallbackMaxHealth = 100.f;
	constexpr float FallbackMaxMana = 100.f;

	float ReadStat(const UGBStatsComponent* Stats, FName Stat, float DefaultValue)
	{
		return Stats ? Stats->GetStat(Stat, DefaultValue) : DefaultValue;
	}
}

UGBAttributeComponent::UGBAttributeComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

UGBAttributeComponent* UGBAttributeComponent::Find(const AActor* Actor)
{
	return Actor ? Actor->FindComponentByClass<UGBAttributeComponent>() : nullptr;
}

UGBStatsComponent* UGBAttributeComponent::GetStats() const
{
	return UGBStatsComponent::Find(GetOwner());
}

float UGBAttributeComponent::GetMaxHealth() const
{
	return FMath::Max(0.f, GBAttributePrivate::ReadStat(GetStats(), GBStat::MaxHealth, GBAttributePrivate::FallbackMaxHealth));
}

float UGBAttributeComponent::GetMaxMana() const
{
	return FMath::Max(0.f, GBAttributePrivate::ReadStat(GetStats(), GBStat::MaxMana, GBAttributePrivate::FallbackMaxMana));
}

void UGBAttributeComponent::BeginPlay()
{
	Super::BeginPlay();

	if (UGBStatsComponent* OwnerStats = GetStats())
	{
		OwnerStats->OnStatsChanged.AddWeakLambda(this, [this]()
		{
			// Max values may have dropped (armor removed, effect ended): clamp and let the HUD redraw.
			Health = FMath::Min(Health, GetMaxHealth());
			Mana = FMath::Min(Mana, GetMaxMana());
			OnHealthChanged.Broadcast(Health, GetMaxHealth());
			OnManaChanged.Broadcast(Mana, GetMaxMana());
		});
	}

	// Safety net for owners that set their stats before BeginPlay but never call InitializeFromStats.
	const UGBStatsComponent* Stats = GetStats();
	if (!bDead && Health <= 0.f && Stats && Stats->HasBaseStat(GBStat::MaxHealth))
	{
		InitializeFromStats();
	}
}

void UGBAttributeComponent::InitializeFromStats()
{
	const UGBStatsComponent* Stats = GetStats();
	if (!Stats || !Stats->HasBaseStat(GBStat::MaxHealth))
	{
		UE_LOG(LogGB, Warning, TEXT("%s: no MaxHealth stat in the data; using %f."), *GetNameSafe(GetOwner()), GBAttributePrivate::FallbackMaxHealth);
	}

	bDead = false;
	Health = GetMaxHealth();
	Mana = GetMaxMana();
	TimeSinceDamage = 1000.f;
	TimeSinceManaSpend = 1000.f;
	OnHealthChanged.Broadcast(Health, GetMaxHealth());
	OnManaChanged.Broadcast(Mana, GetMaxMana());
}

float UGBAttributeComponent::ApplyDamage(const FGBDamageInfo& Info)
{
	if (bDead || bInvulnerable || Info.Amount <= 0.f)
	{
		return 0.f;
	}

	FGBDamageInfo Hit = Info;
	OnModifyIncomingDamage.Broadcast(Hit);

	const float Amount = FMath::Max(0.f, Hit.Amount * GBAttributePrivate::ReadStat(GetStats(), GBStat::DamageTaken, 1.f)
		* (GBElemental::ElementIndex(Hit.Element) >= 0
			? GBAttributePrivate::ReadStat(GetStats(), GBElemental::DamageTakenKey(GBElemental::ElementIndex(Hit.Element)), 1.f)
			: 1.f));
	if (Amount <= 0.f)
	{
		// Fully blocked or cancelled: not a hit, so no damage events and the regen delay keeps running.
		return 0.f;
	}

	const float Actual = FMath::Min(Amount, Health);
	Health -= Actual;
	TimeSinceDamage = 0.f;

	OnHealthChanged.Broadcast(Health, GetMaxHealth());
	OnDamaged.Broadcast(Hit, Actual);

	if (Health <= 0.f && !bDead)
	{
		Health = 0.f;
		bDead = true;
		OnDeath.Broadcast(Hit);
	}
	return Actual;
}

void UGBAttributeComponent::Heal(float Amount)
{
	if (bDead || Amount <= 0.f)
	{
		return;
	}
	const float NewHealth = FMath::Min(Health + Amount, GetMaxHealth());
	if (NewHealth != Health)
	{
		Health = NewHealth;
		OnHealthChanged.Broadcast(Health, GetMaxHealth());
	}
}

void UGBAttributeComponent::RestoreMana(float Amount)
{
	if (bDead || Amount <= 0.f)
	{
		return;
	}
	const float NewMana = FMath::Min(Mana + Amount, GetMaxMana());
	if (NewMana != Mana)
	{
		Mana = NewMana;
		OnManaChanged.Broadcast(Mana, GetMaxMana());
	}
}

bool UGBAttributeComponent::HasMana(float Amount) const
{
	return Mana >= Amount;
}

bool UGBAttributeComponent::SpendMana(float Amount)
{
	if (Amount <= 0.f)
	{
		return true;
	}
	if (Mana < Amount)
	{
		return false;
	}
	Mana -= Amount;
	TimeSinceManaSpend = 0.f;
	OnManaChanged.Broadcast(Mana, GetMaxMana());
	return true;
}

void UGBAttributeComponent::RestoreAll()
{
	bDead = false;
	Health = GetMaxHealth();
	Mana = GetMaxMana();
	OnHealthChanged.Broadcast(Health, GetMaxHealth());
	OnManaChanged.Broadcast(Mana, GetMaxMana());
}

void UGBAttributeComponent::SetHealthAndMana(float NewHealth, float NewMana)
{
	Health = FMath::Clamp(NewHealth, 0.f, GetMaxHealth());
	Mana = FMath::Clamp(NewMana, 0.f, GetMaxMana());
	// A snapshot with health revives; one at 0 leaves the owner dead (no OnDeath: nothing hit it).
	bDead = Health <= 0.f;
	OnHealthChanged.Broadcast(Health, GetMaxHealth());
	OnManaChanged.Broadcast(Mana, GetMaxMana());
}

float UGBAttributeComponent::StepRegen(float Current, float Max, float RegenPerSecond, float RegenDelay, float TimeSinceLastLoss, float DeltaTime)
{
	if (Current >= Max || RegenPerSecond <= 0.f || DeltaTime <= 0.f)
	{
		return Current;
	}
	// Only the part of this step that lies past the delay regenerates, so the result does not depend on frame rate.
	const float RegenTime = FMath::Clamp(TimeSinceLastLoss + DeltaTime - FMath::Max(0.f, RegenDelay), 0.f, DeltaTime);
	return FMath::Min(Current + RegenPerSecond * RegenTime, Max);
}

void UGBAttributeComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bDead)
	{
		const UGBStatsComponent* Stats = GetStats();

		const float MaxHealth = GetMaxHealth();
		const float NewHealth = StepRegen(Health, MaxHealth, GBAttributePrivate::ReadStat(Stats, GBStat::HealthRegen, 0.f),
			GBAttributePrivate::ReadStat(Stats, GBStat::HealthRegenDelay, 0.f), TimeSinceDamage, DeltaTime);
		if (NewHealth != Health)
		{
			Health = NewHealth;
			OnHealthChanged.Broadcast(Health, MaxHealth);
		}

		const float MaxMana = GetMaxMana();
		const float NewMana = StepRegen(Mana, MaxMana, GBAttributePrivate::ReadStat(Stats, GBStat::ManaRegen, 0.f),
			GBAttributePrivate::ReadStat(Stats, GBStat::ManaRegenDelay, 0.f), TimeSinceManaSpend, DeltaTime);
		if (NewMana != Mana)
		{
			Mana = NewMana;
			OnManaChanged.Broadcast(Mana, MaxMana);
		}
	}

	TimeSinceDamage += DeltaTime;
	TimeSinceManaSpend += DeltaTime;
}
