// The two skill trees (spells and weapons) and their shared pool of skill points (GDD 5).
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/GBDataTypes.h"
#include "GBSkillTreeComponent.generated.h"

struct FGBPlayerSnapshot;

UENUM(BlueprintType)
enum class EGBSkillUnlockResult : uint8
{
	Ok,
	UnknownNode,
	MaxRank,
	NotEnoughPoints,
	MissingRequirement
};

/**
 * No character levels: progression is skill points (from sub-bosses, bosses and cleared waves) spent on nodes
 * from Skills.json. A node rank is nothing more than a group of stat modifiers (ModifiersPerRank x rank) added to
 * the player's UGBStatsComponent under the source "Skill.<NodeId>", so a node can upgrade any spell parameter or
 * weapon stat without new code.
 */
UCLASS(ClassGroup = (Glyphbound), meta = (BlueprintSpawnableComponent))
class GLYPHBOUND_API UGBSkillTreeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGBSkillTreeComponent();

	/** Starting points from Player.json. Called by the player on BeginPlay. */
	void InitializeFromData();

	int32 GetPoints() const { return Points; }
	void AddPoints(int32 Amount);

	int32 GetRank(FName NodeId) const;
	EGBSkillUnlockResult CanUnlock(FName NodeId) const;
	/** Spends the points and applies the modifiers. */
	EGBSkillUnlockResult Unlock(FName NodeId);

	/** Pure rule, also used by the tests. Ranks = node id -> owned rank. */
	static EGBSkillUnlockResult EvaluateUnlock(const FGBSkillNodeDefinition& Node, const TMap<FName, int32>& Ranks, int32 AvailablePoints);

	void SaveToSnapshot(FGBPlayerSnapshot& Snapshot) const;
	void LoadFromSnapshot(const FGBPlayerSnapshot& Snapshot);

	FSimpleMulticastDelegate OnSkillsChanged;

protected:
	/** Re-applies the modifiers of every owned node (after load or data reload). */
	void ReapplyAllModifiers();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** "Skill.<NodeId>" */
	static FName ModifierSource(FName NodeId);
	/** Sets the node's modifiers on the owner's stats for its current rank (removes them at rank 0 or unknown node). */
	void ApplyNodeModifiers(FName NodeId);
	/** Removes the modifiers of every owned node. */
	void RemoveAllModifiers();
	void HandleDataReloaded();

	UPROPERTY() int32 Points = 0;
	UPROPERTY() TMap<FName, int32> Ranks;

	FDelegateHandle DataReloadedHandle;
};
