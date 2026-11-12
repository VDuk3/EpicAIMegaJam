#include "Stats/GBStatsComponent.h"

#include "GameFramework/Actor.h"

UGBStatsComponent::UGBStatsComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

UGBStatsComponent* UGBStatsComponent::Find(const AActor* Actor)
{
	return Actor ? Actor->FindComponentByClass<UGBStatsComponent>() : nullptr;
}

void UGBStatsComponent::SetBaseStats(const TMap<FName, float>& InBaseStats)
{
	BaseStats = InBaseStats;
	OnStatsChanged.Broadcast();
}

void UGBStatsComponent::SetBaseStat(FName Stat, float Value)
{
	BaseStats.Add(Stat, Value);
	OnStatsChanged.Broadcast();
}

bool UGBStatsComponent::HasBaseStat(FName Stat) const
{
	return BaseStats.Contains(Stat);
}

float UGBStatsComponent::GetStat(FName Stat, float DefaultValue) const
{
	const float* Base = BaseStats.Find(Stat);
	return Evaluate(Stat, Base ? *Base : DefaultValue);
}

float UGBStatsComponent::Evaluate(FName Key, float Base) const
{
	return EvaluateKeys(MakeArrayView(&Key, 1), Base);
}

float UGBStatsComponent::EvaluateKeys(TConstArrayView<FName> Keys, float Base) const
{
	TArray<FGBModifierDef, TInlineAllocator<8>> Matching;
	for (const FSourcedModifier& Entry : ActiveModifiers)
	{
		if (Keys.Contains(Entry.Modifier.Stat))
		{
			Matching.Add(Entry.Modifier);
		}
	}
	return ApplyModifiers(Base, Matching);
}

void UGBStatsComponent::AddModifiers(FName Source, TConstArrayView<FGBModifierDef> Modifiers, float ValueScale)
{
	ActiveModifiers.RemoveAll([Source](const FSourcedModifier& Entry) { return Entry.Source == Source; });

	for (const FGBModifierDef& Modifier : Modifiers)
	{
		FSourcedModifier& Entry = ActiveModifiers.AddDefaulted_GetRef();
		Entry.Source = Source;
		Entry.Modifier = Modifier;
		switch (Modifier.Op)
		{
		case EGBModOp::Add:
			Entry.Modifier.Value = Modifier.Value * ValueScale;
			break;
		case EGBModOp::Multiply:
			// Scaled around 1, so a x1.5 buff at half strength is x1.25, not x0.75.
			Entry.Modifier.Value = 1.f + (Modifier.Value - 1.f) * ValueScale;
			break;
		case EGBModOp::Override:
			break;
		}
	}

	OnStatsChanged.Broadcast();
}

void UGBStatsComponent::RemoveModifiers(FName Source)
{
	if (ActiveModifiers.RemoveAll([Source](const FSourcedModifier& Entry) { return Entry.Source == Source; }) > 0)
	{
		OnStatsChanged.Broadcast();
	}
}

bool UGBStatsComponent::HasModifierSource(FName Source) const
{
	return ActiveModifiers.ContainsByPredicate([Source](const FSourcedModifier& Entry) { return Entry.Source == Source; });
}

float UGBStatsComponent::ApplyModifiers(float Base, TConstArrayView<FGBModifierDef> Modifiers)
{
	float Sum = Base;
	float Product = 1.f;
	const FGBModifierDef* LastOverride = nullptr;

	for (const FGBModifierDef& Modifier : Modifiers)
	{
		switch (Modifier.Op)
		{
		case EGBModOp::Add:
			Sum += Modifier.Value;
			break;
		case EGBModOp::Multiply:
			Product *= Modifier.Value;
			break;
		case EGBModOp::Override:
			LastOverride = &Modifier;
			break;
		}
	}

	return LastOverride ? LastOverride->Value : Sum * Product;
}
