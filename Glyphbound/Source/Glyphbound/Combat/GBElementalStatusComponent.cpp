#include "Combat/GBElementalStatusComponent.h"

#include "AI/GBEnemyCharacter.h"
#include "Combat/GBCombatLibrary.h"
#include "Combat/GBStatusEffectComponent.h"
#include "Core/GBStatNames.h"
#include "Data/GBDataSubsystem.h"
#include "Stats/GBAttributeComponent.h"
#include "Stats/GBStatsComponent.h"
#include "Engine/World.h"

namespace
{
	const FName ElementalTickIntervalKey(TEXT("ElementalTickInterval"));
	constexpr float DefaultTickInterval = 0.1f;
	constexpr float FrostSlowMax = 0.95f;

	bool IsValidReaction(int32 Index, FName Reaction)
	{
		using namespace GBElemental;
		if (Index == 0)
		{
			return Reaction == ReactionIgnite || Reaction == ReactionNone;
		}
		if (Index == 1)
		{
			return Reaction == ReactionStun || Reaction == ReactionInterrupt || Reaction == ReactionNone;
		}
		if (Index == 2)
		{
			return Reaction == ReactionFreeze || Reaction == ReactionSlow || Reaction == ReactionNone;
		}
		return false;
	}
}

UGBElementalStatusComponent::UGBElementalStatusComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

UGBElementalStatusComponent* UGBElementalStatusComponent::Find(const AActor* Actor)
{
	return Actor ? Actor->FindComponentByClass<UGBElementalStatusComponent>() : nullptr;
}

void UGBElementalStatusComponent::BeginPlay()
{
	Super::BeginPlay();

	const UGBDataSubsystem* Data = UGBDataSubsystem::Get(this);
	const float Interval = Data ? Data->GetGameValue(ElementalTickIntervalKey, DefaultTickInterval) : DefaultTickInterval;
	SetComponentTickInterval(FMath::Max(0.01f, Interval));
	SetComponentTickEnabled(false);

	if (UGBAttributeComponent* Attributes = UGBAttributeComponent::Find(GetOwner()))
	{
		DeathHandle = Attributes->OnDeath.AddUObject(this, &UGBElementalStatusComponent::HandleOwnerDeath);
	}
}

void UGBElementalStatusComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UGBAttributeComponent* Attributes = UGBAttributeComponent::Find(GetOwner()))
	{
		Attributes->OnDeath.Remove(DeathHandle);
	}
	DeathHandle.Reset();
	if (UGBStatsComponent* Stats = UGBStatsComponent::Find(GetOwner()))
	{
		Stats->RemoveModifiers(GBElemental::FrostSource);
	}
	Super::EndPlay(EndPlayReason);
}

void UGBElementalStatusComponent::HandleOwnerDeath(const FGBDamageInfo& KillingBlow)
{
	(void)KillingBlow;
	ClearAll();
}

bool UGBElementalStatusComponent::ReadConfig(int32 Index, FGBBuildUpConfig& OutConfig, FName& OutReaction) const
{
	OutConfig = FGBBuildUpConfig();
	OutReaction = GBElemental::ReactionNone;

	if (Index < 0 || Index >= GBElemental::NumElements)
	{
		return false;
	}

	const AActor* Owner = GetOwner();
	const UGBStatsComponent* Stats = UGBStatsComponent::Find(Owner);
	const UGBDataSubsystem* Data = UGBDataSubsystem::Get(this);

	auto ReadNumber = [&](FName Key, float Fallback) -> float
	{
		const float GameDefault = Data ? Data->GetGameValue(GBElemental::DefaultKey(Key), Fallback) : Fallback;
		return Stats ? Stats->GetStat(Key, GameDefault) : GameDefault;
	};

	const FName ThresholdKey = GBElemental::ThresholdKey(Index);
	const FName BuildUpTakenKey = GBElemental::BuildUpTakenKey(Index);
	const FName DecayDelayKey = GBElemental::DecayDelayKey(Index);
	const FName DecayRateKey = GBElemental::DecayRateKey(Index);
	const FName DurationKey = GBElemental::StatusDurationKey(Index);
	const FName ImmunityKey = GBElemental::StatusImmunityKey(Index);

	OutConfig.Threshold = ReadNumber(ThresholdKey, 0.f);
	OutConfig.BuildUpTaken = ReadNumber(BuildUpTakenKey, 0.f);
	OutConfig.DecayDelay = ReadNumber(DecayDelayKey, 0.f);
	OutConfig.DecayRate = ReadNumber(DecayRateKey, 0.f);
	OutConfig.StatusDuration = ReadNumber(DurationKey, 0.f);
	OutConfig.ImmunityDuration = ReadNumber(ImmunityKey, 0.f);

	// Reaction: enemy definition Strings, else Game.json default.
	const FName ReactionKey = GBElemental::ReactionKey(Index);
	FString ReactionStr;
	bool bHasReaction = false;
	FName EnemyId = NAME_None;
	if (const AGBEnemyCharacter* Enemy = Cast<AGBEnemyCharacter>(Owner))
	{
		EnemyId = Enemy->GetEnemyId();
		if (const FGBEnemyDefinition* Def = Enemy->GetDefinition())
		{
			if (const FString* Found = Def->Strings.Find(ReactionKey))
			{
				ReactionStr = *Found;
				bHasReaction = true;
			}
		}
	}
	if (!bHasReaction)
	{
		const FString DefaultStr = Data ? Data->GetGameString(GBElemental::DefaultKey(ReactionKey), FString()) : FString();
		if (!DefaultStr.IsEmpty())
		{
			ReactionStr = DefaultStr;
			bHasReaction = true;
		}
	}

	FName Reaction = bHasReaction ? FName(*ReactionStr) : GBElemental::ReactionNone;
	if (!IsValidReaction(Index, Reaction))
	{
		if (!EnemyId.IsNone())
		{
			const FName WarnKey(*FString::Printf(TEXT("%s:%d"), *EnemyId.ToString(), Index));
			if (!WarnedReactions.Contains(WarnKey))
			{
				WarnedReactions.Add(WarnKey);
				UE_LOG(LogGB, Warning, TEXT("Enemy '%s': unknown %s '%s', treating as None."),
					*EnemyId.ToString(), *ReactionKey.ToString(), *Reaction.ToString());
			}
		}
		else
		{
			UE_LOG(LogGB, Warning, TEXT("Elemental: unknown %s '%s', treating as None."), *ReactionKey.ToString(), *Reaction.ToString());
		}
		Reaction = GBElemental::ReactionNone;
	}
	OutReaction = Reaction;
	return true;
}

bool UGBElementalStatusComponent::AddBuildUp(EGBElement Element, float Amount, int32 EventId, AActor* Instigator)
{
	const int32 Index = GBElemental::ElementIndex(Element);
	if (Index < 0 || Index >= GBElemental::NumElements)
	{
		return false;
	}
	if (Amount <= 0.f)
	{
		return false;
	}
	FGBBuildUpConfig Config;
	FName Reaction;
	if (!ReadConfig(Index, Config, Reaction))
	{
		return false;
	}
	// Immune to the status: no build-up (damage itself is untouched).
	if (Reaction == GBElemental::ReactionNone)
	{
		return false;
	}
	if (Config.Threshold <= 0.f)
	{
		return false;
	}

	// Time since the meters were last advanced: the hit happened now, not at the last tick.
	const double Now = GetGameTime();
	if (!IsComponentTickEnabled() || LastAdvanceTime < 0.0)
	{
		LastAdvanceTime = Now;
	}
	const float Lag = FMath::Max(0.f, static_cast<float>(Now - LastAdvanceTime));

	float ToAdd = Amount;
	if (EventId != 0)
	{
		ToAdd = EventLog.Accept(EventId, Amount);
		if (ToAdd <= 0.f)
		{
			return false;
		}
	}

	const float ValueBefore = Meters[Index].Value;
	const bool bTriggered = Meters[Index].Add(ToAdd, Config);
	if (bTriggered)
	{
		// The next Advance covers Lag seconds from before the hit: give them back.
		Meters[Index].PhaseTimeRemaining += Lag;
	}
	else if (Meters[Index].Value != ValueBefore)
	{
		// Same for the decay delay, which restarts at the hit.
		Meters[Index].TimeSinceBuildUp = -Lag;
	}
	UpdateTickEnabled();
	if (bTriggered)
	{
		StartStatus(Index, Config, Reaction, Config.StatusDuration, Instigator);
		return true;
	}
	OnBuildUpChanged.Broadcast(Element, Meters[Index].Value, Config.Threshold);
	return false;
}

void UGBElementalStatusComponent::StartStatus(int32 Index, const FGBBuildUpConfig& Config, FName Reaction, float Duration, AActor* Instigator)
{
	(void)Config;
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}
	const EGBElement Element = GBElemental::IndexToElement(Index);
	UGBStatsComponent* Stats = UGBStatsComponent::Find(Owner);
	const UGBDataSubsystem* Data = UGBDataSubsystem::Get(this);

	if (Index == 0)
	{
		// Fire: Ignite DOT through the status effect system.
		float IgniteDamageMult = 1.f;
		if (Stats)
		{
			const float GameDefault = Data ? Data->GetGameValue(GBElemental::DefaultKey(GBElemental::IgniteDamageMult), 1.f) : 1.f;
			IgniteDamageMult = Stats->GetStat(GBElemental::IgniteDamageMult, GameDefault);
		}
		else if (Data)
		{
			IgniteDamageMult = Data->GetGameValue(GBElemental::DefaultKey(GBElemental::IgniteDamageMult), 1.f);
		}
		float DurationScale = 1.f;
		if (Data)
		{
			if (const FGBStatusEffectDefinition* IgniteDef = Data->FindStatusEffect(GBElemental::IgniteEffect))
			{
				if (IgniteDef->Duration > 1.e-4f)
				{
					DurationScale = Duration / IgniteDef->Duration;
				}
			}
		}
		if (UGBStatusEffectComponent* Effects = UGBStatusEffectComponent::Find(Owner))
		{
			AActor* RealInstigator = IsValid(Instigator) ? Instigator : nullptr;
			Effects->ApplyEffect(GBElemental::IgniteEffect, RealInstigator, DurationScale, FMath::Max(0.f, IgniteDamageMult));
		}
	}
	else if (Index == 1)
	{
		// Lightning: discharge first.
		float Discharge = 0.f;
		if (Stats)
		{
			const float GameDefault = Data ? Data->GetGameValue(GBElemental::DefaultKey(GBElemental::ShockDischargeDamage), 0.f) : 0.f;
			Discharge = Stats->GetStat(GBElemental::ShockDischargeDamage, GameDefault);
		}
		else if (Data)
		{
			Discharge = Data->GetGameValue(GBElemental::DefaultKey(GBElemental::ShockDischargeDamage), 0.f);
		}
		if (Discharge > 0.f)
		{
			FGBDamageInfo Info;
			Info.Amount = Discharge;
			Info.Element = EGBElement::Lightning;
			Info.Instigator = IsValid(Instigator) ? Instigator : nullptr;
			Info.Causer = Owner;
			Info.SourceId = GBElemental::ShockSource;
			Info.HitLocation = Owner->GetActorLocation();
			Info.BuildUp = 0.f;
			UGBCombatLibrary::DealDamage(Owner, Info);
			if (!UGBCombatLibrary::IsAlive(Owner))
			{
				return;
			}
		}
	}
	else if (Index == 2)
	{
		// Ice: Slow is a MultiplyMin modifier; Freeze is done by the enemy.
		if (Reaction == GBElemental::ReactionSlow && Stats)
		{
			float FrostSlow = 0.f;
			const float GameDefault = Data ? Data->GetGameValue(GBElemental::DefaultKey(GBElemental::FrostSlow), 0.f) : 0.f;
			FrostSlow = Stats->GetStat(GBElemental::FrostSlow, GameDefault);
			const float Clamped = FMath::Clamp(FrostSlow, 0.f, FrostSlowMax);
			FGBModifierDef Mod;
			Mod.Stat = GBStat::MoveSpeed;
			Mod.Op = EGBModOp::MultiplyMin;
			Mod.Value = 1.f - Clamped;
			Stats->AddModifiers(GBElemental::FrostSource, MakeArrayView(&Mod, 1));
		}
	}

	UpdateTickEnabled();
	OnStatusStarted.Broadcast(Element, Reaction, Duration);
	OnBuildUpChanged.Broadcast(Element, 0.f, Config.Threshold);
}

void UGBElementalStatusComponent::EndStatus(int32 Index)
{
	const EGBElement Element = GBElemental::IndexToElement(Index);
	// Fire: the Ignite effect runs out by itself (its duration is the status duration, scaled in StartStatus),
	// so its last damage tick is never cut; death clears it through UGBStatusEffectComponent.
	if (Index == 2)
	{
		if (UGBStatsComponent* Stats = UGBStatsComponent::Find(GetOwner()))
		{
			Stats->RemoveModifiers(GBElemental::FrostSource);
		}
	}
	OnStatusEnded.Broadcast(Element);
}

float UGBElementalStatusComponent::GetBuildUp(EGBElement Element) const
{
	const int32 Index = GBElemental::ElementIndex(Element);
	return (Index >= 0 && Index < GBElemental::NumElements) ? Meters[Index].Value : 0.f;
}

float UGBElementalStatusComponent::GetThreshold(EGBElement Element) const
{
	const int32 Index = GBElemental::ElementIndex(Element);
	if (Index < 0 || Index >= GBElemental::NumElements)
	{
		return 0.f;
	}
	FGBBuildUpConfig Config;
	FName Reaction;
	if (!ReadConfig(Index, Config, Reaction))
	{
		return 0.f;
	}
	return Config.Threshold;
}

EGBBuildUpPhase UGBElementalStatusComponent::GetPhase(EGBElement Element) const
{
	const int32 Index = GBElemental::ElementIndex(Element);
	return (Index >= 0 && Index < GBElemental::NumElements) ? Meters[Index].Phase : EGBBuildUpPhase::Building;
}

float UGBElementalStatusComponent::GetPhaseTimeRemaining(EGBElement Element) const
{
	const int32 Index = GBElemental::ElementIndex(Element);
	return (Index >= 0 && Index < GBElemental::NumElements) ? Meters[Index].PhaseTimeRemaining : 0.f;
}

bool UGBElementalStatusComponent::IsStatusActive(EGBElement Element) const
{
	return GetPhase(Element) == EGBBuildUpPhase::Active;
}

FName UGBElementalStatusComponent::GetReaction(EGBElement Element) const
{
	const int32 Index = GBElemental::ElementIndex(Element);
	if (Index < 0 || Index >= GBElemental::NumElements)
	{
		return GBElemental::ReactionNone;
	}
	FGBBuildUpConfig Config;
	FName Reaction;
	if (!ReadConfig(Index, Config, Reaction))
	{
		return GBElemental::ReactionNone;
	}
	return Reaction;
}

void UGBElementalStatusComponent::ClearAll()
{
	for (int32 i = 0; i < GBElemental::NumElements; ++i)
	{
		Meters[i].Value = 0.f;
		Meters[i].Phase = EGBBuildUpPhase::Building;
		Meters[i].PhaseTimeRemaining = 0.f;
		Meters[i].TimeSinceBuildUp = 0.f;
	}
	EventLog = FGBBuildUpEventLog();
	LastAdvanceTime = -1.0;
	if (UGBStatsComponent* Stats = UGBStatsComponent::Find(GetOwner()))
	{
		Stats->RemoveModifiers(GBElemental::FrostSource);
	}
	UpdateTickEnabled();
}

double UGBElementalStatusComponent::GetGameTime() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetTimeSeconds() : 0.0;
}

void UGBElementalStatusComponent::UpdateTickEnabled()
{
	bool bAnyBusy = false;
	for (int32 i = 0; i < GBElemental::NumElements; ++i)
	{
		if (!Meters[i].IsIdle())
		{
			bAnyBusy = true;
			break;
		}
	}
	SetComponentTickEnabled(bAnyBusy);
}

void UGBElementalStatusComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const double Now = GetGameTime();
	if (LastAdvanceTime >= 0.0)
	{
		DeltaTime = static_cast<float>(Now - LastAdvanceTime);
	}
	LastAdvanceTime = Now;
	if (DeltaTime <= 0.f)
	{
		return;
	}

	for (int32 i = 0; i < GBElemental::NumElements; ++i)
	{
		FGBBuildUpConfig Config;
		FName Reaction;
		if (!ReadConfig(i, Config, Reaction))
		{
			continue;
		}
		const uint8 Events = Meters[i].Advance(DeltaTime, Config);
		if (Events & static_cast<uint8>(EGBBuildUpEvent::ValueChanged))
		{
			OnBuildUpChanged.Broadcast(GBElemental::IndexToElement(i), Meters[i].Value, Config.Threshold);
		}
		if (Events & static_cast<uint8>(EGBBuildUpEvent::StatusEnded))
		{
			EndStatus(i);
		}
		if (Events & static_cast<uint8>(EGBBuildUpEvent::ImmunityEnded))
		{
			OnImmunityEnded.Broadcast(GBElemental::IndexToElement(i));
		}
	}
	UpdateTickEnabled();
}
