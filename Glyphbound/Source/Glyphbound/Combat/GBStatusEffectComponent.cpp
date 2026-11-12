#include "Combat/GBStatusEffectComponent.h"

#include "Combat/GBCombatLibrary.h"
#include "Data/GBDataSubsystem.h"
#include "Stats/GBAttributeComponent.h"
#include "Stats/GBStatsComponent.h"
#include "GameFramework/Actor.h"

namespace
{
	// Float slack for timers so a tick that lands exactly on the end of the effect still runs.
	constexpr float GBEffectTimeEpsilon = 1.e-4f;

	const FGBStatusEffectDefinition* FindDefinition(const UObject* Context, FName EffectId)
	{
		const UGBDataSubsystem* Data = UGBDataSubsystem::Get(Context);
		return Data ? Data->FindStatusEffect(EffectId) : nullptr;
	}
}

UGBStatusEffectComponent::UGBStatusEffectComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

UGBStatusEffectComponent* UGBStatusEffectComponent::Find(const AActor* Actor)
{
	return Actor ? Actor->FindComponentByClass<UGBStatusEffectComponent>() : nullptr;
}

void UGBStatusEffectComponent::BeginPlay()
{
	Super::BeginPlay();

	if (UGBAttributeComponent* Attributes = UGBAttributeComponent::Find(GetOwner()))
	{
		Attributes->OnDeath.AddUObject(this, &UGBStatusEffectComponent::HandleOwnerDeath);
	}
}

void UGBStatusEffectComponent::HandleOwnerDeath(const FGBDamageInfo& KillingBlow)
{
	RemoveAllEffects();
}

bool UGBStatusEffectComponent::ApplyEffect(FName EffectId, AActor* Instigator, float DurationScale, float StrengthScale)
{
	const FGBStatusEffectDefinition* FoundDefinition = FindDefinition(this, EffectId);
	if (!FoundDefinition)
	{
		UE_LOG(LogGB, Warning, TEXT("StatusEffect: unknown effect id '%s'"), *EffectId.ToString());
		return false;
	}
	// Copy: applying damage below runs other systems' callbacks; never hold a definition pointer across them.
	const FGBStatusEffectDefinition Definition = *FoundDefinition;

	AActor* Owner = GetOwner();
	const UGBAttributeComponent* Attributes = UGBAttributeComponent::Find(Owner);
	if (!Owner || (Attributes && Attributes->IsDead()))
	{
		return false;
	}

	// Resistance to one effect: an optional stat "<EffectId>Taken" on the target scales the strength it receives
	// (Enemies.json "SlowTaken": 0.4 = a boss takes 40% of the ice floor's slow). Missing = 1.
	float Taken = 1.f;
	if (const UGBStatsComponent* OwnerStats = UGBStatsComponent::Find(Owner))
	{
		Taken = FMath::Max(0.f, OwnerStats->GetStat(FName(*(EffectId.ToString() + TEXT("Taken"))), 1.f));
	}
	const float Strength = FMath::Max(0.f, StrengthScale) * Taken;
	const float Duration = Definition.Duration * FMath::Max(0.f, DurationScale);

	ApplyHealthAndMana(EffectId, Definition.InstantHealth * Strength, Definition.InstantMana * Strength, Definition.Element, Instigator);

	// The instant part may have killed the owner (death clears every effect).
	if (Attributes && Attributes->IsDead())
	{
		return true;
	}
	if (Duration <= 0.f)
	{
		return true;
	}

	int32 ExistingCount = 0;
	int32 FirstIndex = INDEX_NONE;
	int32 ShortestIndex = INDEX_NONE;
	for (int32 Index = 0; Index < ActiveEffects.Num(); ++Index)
	{
		if (ActiveEffects[Index].EffectId != EffectId)
		{
			continue;
		}
		++ExistingCount;
		if (FirstIndex == INDEX_NONE)
		{
			FirstIndex = Index;
		}
		if (ShortestIndex == INDEX_NONE || ActiveEffects[Index].TimeRemaining < ActiveEffects[ShortestIndex].TimeRemaining)
		{
			ShortestIndex = Index;
		}
	}

	if (ExistingCount > 0 && Definition.bRefreshOnReapply)
	{
		RefreshEntry(FirstIndex, Duration, Strength, Instigator);
		return true;
	}
	if (ExistingCount >= FMath::Max(1, Definition.MaxStacks))
	{
		// At max stacks: renew the stack closest to running out.
		RefreshEntry(ShortestIndex, Duration, Strength, Instigator);
		return true;
	}

	FGBActiveStatusEffect& Entry = ActiveEffects.AddDefaulted_GetRef();
	Entry.EffectId = EffectId;
	Entry.TimeRemaining = Duration;
	Entry.TotalDuration = Duration;
	Entry.TimeToNextTick = Definition.TickInterval;
	Entry.StrengthScale = Strength;
	Entry.Instigator = Instigator;
	Entry.ModifierSource = FName(*FString::Printf(TEXT("Effect.%s#%d"), *EffectId.ToString(), NextEffectSerial++));
	const FName ModifierSource = Entry.ModifierSource;

	UpdateStackCount(EffectId);

	if (Definition.Modifiers.Num() > 0)
	{
		if (UGBStatsComponent* Stats = UGBStatsComponent::Find(Owner))
		{
			Stats->AddModifiers(ModifierSource, Definition.Modifiers, Strength);
		}
	}

	OnEffectsChanged.Broadcast();
	return true;
}

void UGBStatusEffectComponent::RefreshEntry(int32 Index, float Duration, float StrengthScale, AActor* EffectInstigator)
{
	if (!ActiveEffects.IsValidIndex(Index))
	{
		return;
	}

	FGBActiveStatusEffect& Entry = ActiveEffects[Index];
	Entry.TimeRemaining = Duration;
	Entry.TotalDuration = Duration;
	if (EffectInstigator)
	{
		Entry.Instigator = EffectInstigator;
	}

	if (StrengthScale > Entry.StrengthScale)
	{
		Entry.StrengthScale = StrengthScale;
		const FName EffectId = Entry.EffectId;
		const FName ModifierSource = Entry.ModifierSource;
		const FGBStatusEffectDefinition* Definition = FindDefinition(this, EffectId);
		UGBStatsComponent* Stats = UGBStatsComponent::Find(GetOwner());
		if (Definition && Stats && Definition->Modifiers.Num() > 0)
		{
			// Same source name: replaces the old modifiers with the stronger ones.
			const TArray<FGBModifierDef> Modifiers = Definition->Modifiers;
			Stats->AddModifiers(ModifierSource, Modifiers, StrengthScale);
		}
	}

	OnEffectsChanged.Broadcast();
}

void UGBStatusEffectComponent::UpdateStackCount(FName EffectId)
{
	int32 Count = 0;
	for (const FGBActiveStatusEffect& Entry : ActiveEffects)
	{
		if (Entry.EffectId == EffectId)
		{
			++Count;
		}
	}
	for (FGBActiveStatusEffect& Entry : ActiveEffects)
	{
		if (Entry.EffectId == EffectId)
		{
			Entry.Stacks = Count;
		}
	}
}

void UGBStatusEffectComponent::ApplyHealthAndMana(FName EffectId, float HealthAmount, float ManaAmount, EGBElement Element, AActor* EffectInstigator)
{
	AActor* Owner = GetOwner();
	UGBAttributeComponent* Attributes = UGBAttributeComponent::Find(Owner);
	if (!Attributes || (FMath::IsNearlyZero(HealthAmount) && FMath::IsNearlyZero(ManaAmount)))
	{
		return;
	}

	if (HealthAmount > 0.f)
	{
		Attributes->Heal(HealthAmount);
	}
	else if (HealthAmount < 0.f)
	{
		FGBDamageInfo Info;
		Info.Amount = -HealthAmount;
		Info.Element = Element;
		Info.Instigator = IsValid(EffectInstigator) ? EffectInstigator : nullptr;
		Info.HitLocation = Owner->GetActorLocation();
		Info.SourceId = EffectId;
		// OnHitEffect stays None: periodic damage must not re-apply effects.
		UGBCombatLibrary::DealDamage(Owner, Info, /*bAllowFriendlyFire*/ true);
	}

	if (Attributes->IsDead())
	{
		return;
	}

	if (ManaAmount > 0.f)
	{
		Attributes->RestoreMana(ManaAmount);
	}
	else if (ManaAmount < 0.f)
	{
		const float Drain = FMath::Min(-ManaAmount, Attributes->GetMana());
		if (Drain > 0.f)
		{
			Attributes->SpendMana(Drain);
		}
	}
}

void UGBStatusEffectComponent::RemoveEffect(FName EffectId)
{
	UGBStatsComponent* Stats = UGBStatsComponent::Find(GetOwner());
	bool bRemoved = false;
	for (int32 Index = ActiveEffects.Num() - 1; Index >= 0; --Index)
	{
		if (ActiveEffects[Index].EffectId != EffectId)
		{
			continue;
		}
		const FName ModifierSource = ActiveEffects[Index].ModifierSource;
		ActiveEffects.RemoveAt(Index);
		if (Stats)
		{
			Stats->RemoveModifiers(ModifierSource);
		}
		bRemoved = true;
	}

	if (bRemoved)
	{
		OnEffectsChanged.Broadcast();
	}
}

void UGBStatusEffectComponent::RemoveAllEffects()
{
	if (ActiveEffects.Num() == 0)
	{
		return;
	}

	TArray<FGBActiveStatusEffect> Removed = MoveTemp(ActiveEffects);
	ActiveEffects.Reset();

	if (UGBStatsComponent* Stats = UGBStatsComponent::Find(GetOwner()))
	{
		for (const FGBActiveStatusEffect& Entry : Removed)
		{
			Stats->RemoveModifiers(Entry.ModifierSource);
		}
	}

	OnEffectsChanged.Broadcast();
}

bool UGBStatusEffectComponent::HasEffect(FName EffectId) const
{
	for (const FGBActiveStatusEffect& Entry : ActiveEffects)
	{
		if (Entry.EffectId == EffectId)
		{
			return true;
		}
	}
	return false;
}

float UGBStatusEffectComponent::GetTimeRemaining(FName EffectId) const
{
	float Remaining = 0.f;
	for (const FGBActiveStatusEffect& Entry : ActiveEffects)
	{
		if (Entry.EffectId == EffectId)
		{
			Remaining = FMath::Max(Remaining, Entry.TimeRemaining);
		}
	}
	return Remaining;
}

void UGBStatusEffectComponent::SetTimeRemaining(FName EffectId, float Seconds)
{
	bool bChanged = false;
	for (FGBActiveStatusEffect& Entry : ActiveEffects)
	{
		if (Entry.EffectId == EffectId)
		{
			Entry.TimeRemaining = FMath::Clamp(Seconds, 0.f, Entry.TotalDuration);
			bChanged = true;
		}
	}
	if (bChanged)
	{
		OnEffectsChanged.Broadcast();
	}
}

void UGBStatusEffectComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (ActiveEffects.Num() == 0)
	{
		return;
	}

	const UGBAttributeComponent* Attributes = UGBAttributeComponent::Find(GetOwner());
	if (Attributes && Attributes->IsDead())
	{
		return;
	}

	struct FPendingTick
	{
		FName EffectId;
		float Health = 0.f;
		float Mana = 0.f;
		EGBElement Element = EGBElement::Physical;
		TWeakObjectPtr<AActor> Instigator;
	};

	// Phase 1: advance timers only (no callbacks), so the array is not changed while we walk it.
	TArray<FPendingTick> PendingTicks;
	TArray<int32> ExpiredIndices;
	for (int32 Index = 0; Index < ActiveEffects.Num(); ++Index)
	{
		FGBActiveStatusEffect& Entry = ActiveEffects[Index];
		const FGBStatusEffectDefinition* Definition = FindDefinition(this, Entry.EffectId);
		if (!Definition)
		{
			UE_LOG(LogGB, Warning, TEXT("StatusEffect: effect '%s' is no longer in the data; removing it"), *Entry.EffectId.ToString());
			ExpiredIndices.Add(Index);
			continue;
		}

		// Ticks only count time inside the effect, so none run after its end.
		const float ActiveStep = FMath::Min(DeltaTime, FMath::Max(0.f, Entry.TimeRemaining));
		if (Definition->TickInterval > 0.f)
		{
			Entry.TimeToNextTick -= ActiveStep;
			while (Entry.TimeToNextTick <= GBEffectTimeEpsilon)
			{
				Entry.TimeToNextTick += Definition->TickInterval;
				FPendingTick& Pending = PendingTicks.AddDefaulted_GetRef();
				Pending.EffectId = Entry.EffectId;
				Pending.Health = Definition->HealthPerTick * Entry.StrengthScale;
				Pending.Mana = Definition->ManaPerTick * Entry.StrengthScale;
				Pending.Element = Definition->Element;
				Pending.Instigator = Entry.Instigator;
			}
		}

		Entry.TimeRemaining -= DeltaTime;
		if (Entry.TimeRemaining <= GBEffectTimeEpsilon)
		{
			ExpiredIndices.Add(Index);
		}
	}

	// Phase 2: remove expired entries and their modifiers.
	if (ExpiredIndices.Num() > 0)
	{
		UGBStatsComponent* Stats = UGBStatsComponent::Find(GetOwner());
		TArray<FName> ChangedIds;
		for (int32 i = ExpiredIndices.Num() - 1; i >= 0; --i)
		{
			const int32 Index = ExpiredIndices[i];
			const FName ModifierSource = ActiveEffects[Index].ModifierSource;
			ChangedIds.AddUnique(ActiveEffects[Index].EffectId);
			ActiveEffects.RemoveAt(Index);
			if (Stats)
			{
				Stats->RemoveModifiers(ModifierSource);
			}
		}
		for (const FName& EffectId : ChangedIds)
		{
			UpdateStackCount(EffectId);
		}
		OnEffectsChanged.Broadcast();
	}

	// Phase 3: run the ticks (they may damage, kill and clear everything).
	for (const FPendingTick& Pending : PendingTicks)
	{
		if (Attributes && Attributes->IsDead())
		{
			break;
		}
		ApplyHealthAndMana(Pending.EffectId, Pending.Health, Pending.Mana, Pending.Element, Pending.Instigator.Get());
	}
}
