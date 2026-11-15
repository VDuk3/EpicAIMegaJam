#include "Progression/GBSkillTreeComponent.h"

#include "Data/GBDataSubsystem.h"
#include "Stats/GBAttributeComponent.h"
#include "Stats/GBStatsComponent.h"
#include "World/GBProgressSubsystem.h"

UGBSkillTreeComponent::UGBSkillTreeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UGBSkillTreeComponent::BeginPlay()
{
	Super::BeginPlay();

	if (UGBDataSubsystem* Data = UGBDataSubsystem::Get(this))
	{
		DataReloadedHandle = Data->OnDataReloaded.AddUObject(this, &UGBSkillTreeComponent::HandleDataReloaded);
	}
}

void UGBSkillTreeComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UGBDataSubsystem* Data = UGBDataSubsystem::Get(this))
	{
		Data->OnDataReloaded.Remove(DataReloadedHandle);
	}
	DataReloadedHandle.Reset();

	Super::EndPlay(EndPlayReason);
}

void UGBSkillTreeComponent::InitializeFromData()
{
	RemoveAllModifiers();
	Ranks.Reset();
	PointsSpent = 0;

	const UGBDataSubsystem* Data = UGBDataSubsystem::Get(this);
	if (!Data)
	{
		UE_LOG(LogGB, Warning, TEXT("Skill tree: no data subsystem, starting with 0 points."));
	}
	Points = Data ? FMath::Max(0, Data->GetPlayer().StartingSkillPoints) : 0;
	OnSkillsChanged.Broadcast();
}

void UGBSkillTreeComponent::AddPoints(int32 Amount)
{
	if (Amount == 0)
	{
		return;
	}
	Points = FMath::Max(0, Points + Amount);
	OnSkillsChanged.Broadcast();
}

int32 UGBSkillTreeComponent::GetRank(FName NodeId) const
{
	return Ranks.FindRef(NodeId);
}

EGBSkillUnlockResult UGBSkillTreeComponent::CanUnlock(FName NodeId) const
{
	const UGBDataSubsystem* Data = UGBDataSubsystem::Get(this);
	const FGBSkillNodeDefinition* Node = Data ? Data->FindSkillNode(NodeId) : nullptr;
	if (!Node)
	{
		return EGBSkillUnlockResult::UnknownNode;
	}
	return EvaluateUnlock(*Node, Ranks, Points);
}

EGBSkillUnlockResult UGBSkillTreeComponent::Unlock(FName NodeId)
{
	const UGBDataSubsystem* Data = UGBDataSubsystem::Get(this);
	const FGBSkillNodeDefinition* Node = Data ? Data->FindSkillNode(NodeId) : nullptr;
	if (!Node)
	{
		UE_LOG(LogGB, Warning, TEXT("Unlock: unknown skill node '%s'."), *NodeId.ToString());
		return EGBSkillUnlockResult::UnknownNode;
	}

	const EGBSkillUnlockResult Result = EvaluateUnlock(*Node, Ranks, Points);
	if (Result != EGBSkillUnlockResult::Ok)
	{
		return Result;
	}

	Points -= FMath::Max(0, Node->Cost);
	PointsSpent += FMath::Max(0, Node->Cost);
	Ranks.FindOrAdd(NodeId) += 1;

	// A perk that raises max health / mana (Vitality, Deep Reserves) also fills the new part of the bar,
	// so the bar grows instead of showing a sudden empty slice.
	UGBAttributeComponent* Attributes = UGBAttributeComponent::Find(GetOwner());
	const float OldMaxHealth = Attributes ? Attributes->GetMaxHealth() : 0.f;
	const float OldMaxMana = Attributes ? Attributes->GetMaxMana() : 0.f;
	ApplyNodeModifiers(NodeId);
	if (Attributes)
	{
		Attributes->Heal(Attributes->GetMaxHealth() - OldMaxHealth);
		Attributes->RestoreMana(Attributes->GetMaxMana() - OldMaxMana);
	}
	OnSkillsChanged.Broadcast();
	return EGBSkillUnlockResult::Ok;
}

int32 UGBSkillTreeComponent::ResetAll()
{
	if (Ranks.Num() == 0)
	{
		return 0;
	}
	RemoveAllModifiers();
	const int32 Refunded = PointsSpent;
	Points += PointsSpent;
	PointsSpent = 0;
	Ranks.Reset();
	OnSkillsChanged.Broadcast();
	return Refunded;
}

EGBSkillUnlockResult UGBSkillTreeComponent::EvaluateUnlock(const FGBSkillNodeDefinition& Node, const TMap<FName, int32>& InRanks, int32 AvailablePoints)
{
	if (InRanks.FindRef(Node.Id) >= Node.MaxRank)
	{
		return EGBSkillUnlockResult::MaxRank;
	}
	for (const FName& Required : Node.Requires)
	{
		if (!Required.IsNone() && InRanks.FindRef(Required) <= 0)
		{
			return EGBSkillUnlockResult::MissingRequirement;
		}
	}
	if (AvailablePoints < Node.Cost)
	{
		return EGBSkillUnlockResult::NotEnoughPoints;
	}
	return EGBSkillUnlockResult::Ok;
}

FName UGBSkillTreeComponent::ModifierSource(FName NodeId)
{
	return FName(*FString::Printf(TEXT("Skill.%s"), *NodeId.ToString()));
}

void UGBSkillTreeComponent::ApplyNodeModifiers(FName NodeId)
{
	UGBStatsComponent* Stats = UGBStatsComponent::Find(GetOwner());
	if (!Stats)
	{
		return;
	}

	const FName Source = ModifierSource(NodeId);
	const int32 Rank = GetRank(NodeId);
	const UGBDataSubsystem* Data = UGBDataSubsystem::Get(this);
	const FGBSkillNodeDefinition* Node = Data ? Data->FindSkillNode(NodeId) : nullptr;
	if (!Node || Rank <= 0)
	{
		if (!Node && Rank > 0)
		{
			UE_LOG(LogGB, Warning, TEXT("Skill node '%s' is owned but missing from Skills.json; its modifiers are off."), *NodeId.ToString());
		}
		Stats->RemoveModifiers(Source);
		return;
	}

	// ValueScale = rank: Add modifiers count once per rank, Multiply modifiers stack linearly (not compounding).
	Stats->AddModifiers(Source, Node->ModifiersPerRank, static_cast<float>(Rank));
}

void UGBSkillTreeComponent::RemoveAllModifiers()
{
	UGBStatsComponent* Stats = UGBStatsComponent::Find(GetOwner());
	if (!Stats)
	{
		return;
	}
	for (const TPair<FName, int32>& Pair : Ranks)
	{
		Stats->RemoveModifiers(ModifierSource(Pair.Key));
	}
}

void UGBSkillTreeComponent::ReapplyAllModifiers()
{
	for (const TPair<FName, int32>& Pair : Ranks)
	{
		ApplyNodeModifiers(Pair.Key);
	}
}

void UGBSkillTreeComponent::SaveToSnapshot(FGBPlayerSnapshot& Snapshot) const
{
	Snapshot.SkillPoints = Points;
	Snapshot.SkillPointsSpent = PointsSpent;
	Snapshot.SkillRanks = Ranks;
}

void UGBSkillTreeComponent::LoadFromSnapshot(const FGBPlayerSnapshot& Snapshot)
{
	RemoveAllModifiers();

	Points = FMath::Max(0, Snapshot.SkillPoints);
	PointsSpent = FMath::Max(0, Snapshot.SkillPointsSpent);
	Ranks.Reset();
	for (const TPair<FName, int32>& Pair : Snapshot.SkillRanks)
	{
		if (Pair.Value > 0)
		{
			Ranks.Add(Pair.Key, Pair.Value);
		}
	}

	ReapplyAllModifiers();
	OnSkillsChanged.Broadcast();
}

void UGBSkillTreeComponent::HandleDataReloaded()
{
	ReapplyAllModifiers();
	OnSkillsChanged.Broadcast();
}
