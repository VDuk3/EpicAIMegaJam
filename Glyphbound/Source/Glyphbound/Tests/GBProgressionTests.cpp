#include "Misc/AutomationTest.h"
#include "Progression/GBSkillTreeComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FGBSkillNodeDefinition MakeNode(FName Id, int32 Cost, int32 MaxRank, const TArray<FName>& Requires)
	{
		FGBSkillNodeDefinition Node;
		Node.Id = Id;
		Node.Cost = Cost;
		Node.MaxRank = MaxRank;
		Node.Requires = Requires;
		return Node;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBProgressionUnlockRulesTest, "Glyphbound.Progression.UnlockRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGBProgressionUnlockRulesTest::RunTest(const FString& Parameters)
{
	const FName NodeId(TEXT("Node"));
	const FName ReqId(TEXT("Req"));

	auto Check = [this](const TCHAR* What, const FGBSkillNodeDefinition& Node, const TMap<FName, int32>& Ranks, int32 Points, EGBSkillUnlockResult Expected)
	{
		TestEqual(What, UEnum::GetValueAsString(UGBSkillTreeComponent::EvaluateUnlock(Node, Ranks, Points)), UEnum::GetValueAsString(Expected));
	};

	TMap<FName, int32> NoRanks;
	TMap<FName, int32> ReqAtZero;
	ReqAtZero.Add(ReqId, 0);
	TMap<FName, int32> ReqOwned;
	ReqOwned.Add(ReqId, 1);
	TMap<FName, int32> NodeMaxed;
	NodeMaxed.Add(NodeId, 3);
	TMap<FName, int32> NodeMaxedReqOwned = NodeMaxed;
	NodeMaxedReqOwned.Add(ReqId, 1);
	TMap<FName, int32> NodeBelowMaxReqOwned;
	NodeBelowMaxReqOwned.Add(NodeId, 2);
	NodeBelowMaxReqOwned.Add(ReqId, 1);

	// Not enough points: one short fails, exactly the cost passes.
	const FGBSkillNodeDefinition Costly = MakeNode(NodeId, 2, 3, {});
	Check(TEXT("1 point for a cost of 2"), Costly, NoRanks, 1, EGBSkillUnlockResult::NotEnoughPoints);
	Check(TEXT("exactly the cost"), Costly, NoRanks, 2, EGBSkillUnlockResult::Ok);

	// Missing requirement, even with plenty of points and even when the requirement is listed at rank 0.
	const FGBSkillNodeDefinition Gated = MakeNode(NodeId, 1, 3, { ReqId });
	Check(TEXT("requirement absent"), Gated, NoRanks, 10, EGBSkillUnlockResult::MissingRequirement);
	Check(TEXT("requirement at rank 0"), Gated, ReqAtZero, 10, EGBSkillUnlockResult::MissingRequirement);
	Check(TEXT("missing requirement is reported before lack of points"), Gated, NoRanks, 0, EGBSkillUnlockResult::MissingRequirement);
	Check(TEXT("requirement owned"), Gated, ReqOwned, 10, EGBSkillUnlockResult::Ok);

	// Max rank: checked first, and one rank below it is still unlockable.
	Check(TEXT("at max rank"), Gated, NodeMaxedReqOwned, 10, EGBSkillUnlockResult::MaxRank);
	Check(TEXT("max rank is reported before missing requirement and points"), Gated, NodeMaxed, 0, EGBSkillUnlockResult::MaxRank);
	Check(TEXT("one rank below max"), Gated, NodeBelowMaxReqOwned, 1, EGBSkillUnlockResult::Ok);

	// Ok: another node's rank does not count as this node's rank.
	const FGBSkillNodeDefinition Single = MakeNode(NodeId, 1, 1, {});
	TMap<FName, int32> OtherRanked;
	OtherRanked.Add(ReqId, 5);
	Check(TEXT("fresh node, other node ranked"), Single, OtherRanked, 1, EGBSkillUnlockResult::Ok);

	return true;
}

#endif
