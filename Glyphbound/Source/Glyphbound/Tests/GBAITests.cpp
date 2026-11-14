// Pure-logic test of the enemies' local steering decision: which candidate direction is taken around an obstacle.
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "AI/GBSteering.h"
#include "AI/GBEngage.h"
#include "AI/GBTactics.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GBAITestsLocal
{
	/** Three steps per side (7 candidates, offsets -3..3), every candidate blocked except the listed offsets. */
	TArray<bool> OnlyFree(std::initializer_list<int32> FreeOffsets)
	{
		constexpr int32 N = 3;
		TArray<bool> Blocked;
		Blocked.Init(true, 2 * N + 1);
		for (const int32 Offset : FreeOffsets)
		{
			Blocked[Offset + N] = false;
		}
		return Blocked;
	}

	int32 Pick(std::initializer_list<int32> FreeOffsets, EGBSteerMode Mode, int32 Side)
	{
		const TOptional<int32> Choice = GBSteering::ChooseOffset(OnlyFree(FreeOffsets), Mode, Side);
		return Choice.IsSet() ? Choice.GetValue() : 99;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBAISteerChoiceTest, "Glyphbound.AI.SteerChoice",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGBAISteerChoiceTest::RunTest(const FString& Parameters)
{
	using namespace GBAITestsLocal;
	const EGBSteerMode Free = EGBSteerMode::Free;
	const EGBSteerMode Hold = EGBSteerMode::HoldSide;
	const EGBSteerMode Around = EGBSteerMode::GoAround;

	// Free: straight wins; otherwise the smallest angle, the preferred side only breaks ties.
	TestEqual(TEXT("Free: straight when it is free"), Pick({ 0, 1, -1 }, Free, -1), 0);
	TestEqual(TEXT("Free: closest angle beats the preferred side"), Pick({ -1, 2 }, Free, 1), -1);
	TestEqual(TEXT("Free: tie goes to the preferred side (-)"), Pick({ 1, -1 }, Free, -1), -1);
	TestEqual(TEXT("Free: tie goes to the preferred side (+)"), Pick({ 1, -1 }, Free, 1), 1);
	TestEqual(TEXT("Free: widest angle when it is the only way"), Pick({ -3 }, Free, 1), -3);

	// HoldSide: no flipping at a corner while the held side still has a way.
	TestEqual(TEXT("Hold: straight still first"), Pick({ 0, 2 }, Hold, 1), 0);
	TestEqual(TEXT("Hold: wider angle on the held side beats the other side"), Pick({ -1, 2 }, Hold, 1), 2);
	TestEqual(TEXT("Hold: held side blocked, falls back to the other side"), Pick({ -3 }, Hold, 1), -3);

	// GoAround: committed to a side, straight only as a last resort.
	TestEqual(TEXT("Around: skips a free straight direction"), Pick({ 0, -1, 1 }, Around, -1), -1);
	TestEqual(TEXT("Around: other side when the committed side is closed"), Pick({ 0, 2 }, Around, -1), 2);
	TestEqual(TEXT("Around: straight when nothing else is free"), Pick({ 0 }, Around, 1), 0);

	// Boxed in, or a malformed candidate list: no direction.
	TestFalse(TEXT("All blocked: no direction"), GBSteering::ChooseOffset(OnlyFree({}), Free, 1).IsSet());
	const TArray<bool> EvenSized = { false, false };
	TestFalse(TEXT("Even-sized list: no direction"), GBSteering::ChooseOffset(EvenSized, Free, 1).IsSet());
	return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBAIEngageTest, "Glyphbound.AI.Engage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGBAIEngageTest::RunTest(const FString& Parameters)
{
	using GBEngage::Decide;
	const EGBEngageAction Advance = EGBEngageAction::Advance;
	const EGBEngageAction Hold = EGBEngageAction::Hold;
	const EGBEngageAction Retreat = EGBEngageAction::Retreat;

	// Band 600..1400.
	TestTrue(TEXT("Too far: advance"), Decide(1800.f, true, 600.f, 1400.f, true) == Advance);
	TestTrue(TEXT("No sight in the band: advance"), Decide(1000.f, false, 600.f, 1400.f, true) == Advance);
	TestTrue(TEXT("No sight and too close: still advance (it has nothing to shoot at)"), Decide(300.f, false, 600.f, 1400.f, true) == Advance);
	TestTrue(TEXT("In the band with sight: hold"), Decide(1000.f, true, 600.f, 1400.f, true) == Hold);
	TestTrue(TEXT("Edges of the band are inside it (max)"), Decide(1400.f, true, 600.f, 1400.f, true) == Hold);
	TestTrue(TEXT("Edges of the band are inside it (min)"), Decide(600.f, true, 600.f, 1400.f, true) == Hold);
	TestTrue(TEXT("Too close with room behind: retreat"), Decide(300.f, true, 600.f, 1400.f, true) == Retreat);
	TestTrue(TEXT("Too close with a wall behind: hold, do not retreat into it"), Decide(300.f, true, 600.f, 1400.f, false) == Hold);

	// Notice rule: cone 140 degrees (half angle 70), aggro 2600.
	using GBEngage::Notices;
	TestTrue(TEXT("In front, in range, sight: notices"), Notices(1500.f, 20.f, true, 2600.f, 140.f, false));
	TestFalse(TEXT("Behind its back: does not notice"), Notices(1500.f, 120.f, true, 2600.f, 140.f, false));
	TestFalse(TEXT("Behind a wall: does not notice"), Notices(1500.f, 20.f, false, 2600.f, 140.f, false));
	TestFalse(TEXT("Out of range: does not notice"), Notices(3000.f, 20.f, true, 2600.f, 140.f, false));
	TestTrue(TEXT("Hit or alerted: notices from behind, out of range, without sight"), Notices(5000.f, 170.f, false, 2600.f, 140.f, true));
	TestFalse(TEXT("Aggro range 0 (a dummy) never notices, even when hit"), Notices(100.f, 0.f, true, 0.f, 140.f, true));
	TestTrue(TEXT("Negative angle counts like the positive one"), Notices(1500.f, -60.f, true, 2600.f, 140.f, false));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBAIZoneTest, "Glyphbound.AI.Zone",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGBAIZoneTest::RunTest(const FString& Parameters)
{
	const FBox Box(FVector(0.f, 0.f, 0.f), FVector(1000.f, 1000.f, 500.f));
	TestEqual(TEXT("Far outside on X: clamped to the edge, the rest kept"), GBTactics::ClampToBox(Box, FVector(2000.f, -50.f, 100.f)), FVector(1000.f, 0.f, 100.f));
	TestEqual(TEXT("Inside: unchanged"), GBTactics::ClampToBox(Box, FVector(300.f, 400.f, 50.f)), FVector(300.f, 400.f, 50.f));
	TestEqual(TEXT("Above the zone: clamped in Z"), GBTactics::ClampToBox(Box, FVector(10.f, 10.f, 900.f)), FVector(10.f, 10.f, 500.f));
	TestEqual(TEXT("No zone: the point is returned"), GBTactics::ClampToBox(FBox(ForceInit), FVector(9.f, 9.f, 9.f)), FVector(9.f, 9.f, 9.f));
	TestTrue(TEXT("Inside"), GBTactics::IsInsideBox(Box, FVector(500.f, 500.f, 250.f)));
	TestFalse(TEXT("Outside"), GBTactics::IsInsideBox(Box, FVector(1001.f, 500.f, 250.f)));
	TestTrue(TEXT("No zone: everywhere is inside"), GBTactics::IsInsideBox(FBox(ForceInit), FVector(1.0e6f, 0.f, 0.f)));

	// The zone: trigger bounds and spawn points, padded on X and Y, with its own margin in Z.
	const FBox Trigger(FVector(0.f, 0.f, 0.f), FVector(100.f, 100.f, 100.f));
	const TArray<FVector> Points = { FVector(500.f, 0.f, 50.f) };
	const FBox Zone = GBTactics::MakeZone(Trigger, Points, 400.f, 300.f);
	TestEqual(TEXT("Zone min"), Zone.Min, FVector(-400.f, -400.f, -300.f));
	TestEqual(TEXT("Zone max"), Zone.Max, FVector(900.f, 500.f, 400.f));

	const TArray<FVector> Others = { FVector(0.f, 0.f, 0.f), FVector(1000.f, 0.f, 0.f) };
	TestTrue(TEXT("Too close to one of them"), GBTactics::IsTooClose(FVector(100.f, 100.f, 500.f), Others, 350.f));
	TestFalse(TEXT("Far enough from all (height is ignored)"), GBTactics::IsTooClose(FVector(500.f, 0.f, 0.f), Others, 350.f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBAIRolesTest, "Glyphbound.AI.Roles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGBAIRolesTest::RunTest(const FString& Parameters)
{
	using GBTactics::FRoleInput;
	auto Member = [](float Distance, float Aggression, bool bAnchor = false)
	{
		FRoleInput Input;
		Input.DistanceToPlayer = Distance;
		Input.Aggression = Aggression;
		Input.bAnchor = bAnchor;
		return Input;
	};
	auto Assign = [](std::initializer_list<FRoleInput> List)
	{
		TArray<FRoleInput> Inputs(List);
		return GBTactics::AssignRoles(Inputs);
	};

	TestEqual(TEXT("One member alone suppresses"), Assign({ Member(900.f, 0.5f) })[0], EGBSquadRole::Suppress);

	// Three: the nearest suppresses, the next two flank on each side (input order is not distance order).
	const TArray<EGBSquadRole> Three = Assign({ Member(1200.f, 0.5f), Member(500.f, 0.5f), Member(800.f, 0.5f) });
	TestEqual(TEXT("Nearest is the base of fire"), Three[1], EGBSquadRole::Suppress);
	TestEqual(TEXT("Second nearest flanks left"), Three[2], EGBSquadRole::FlankLeft);
	TestEqual(TEXT("Third flanks right"), Three[0], EGBSquadRole::FlankRight);

	// Extras alternate Assault and Suppress; an aggressive extra always pushes.
	const TArray<EGBSquadRole> Five = Assign({ Member(500.f, 0.5f), Member(600.f, 0.5f), Member(700.f, 0.5f), Member(800.f, 0.5f), Member(900.f, 0.5f) });
	TestEqual(TEXT("Fourth is an assault"), Five[3], EGBSquadRole::Assault);
	TestEqual(TEXT("Fifth suppresses again"), Five[4], EGBSquadRole::Suppress);
	const TArray<EGBSquadRole> Pushy = Assign({ Member(500.f, 0.5f), Member(600.f, 0.5f), Member(700.f, 0.5f), Member(800.f, 0.5f), Member(900.f, 0.9f) });
	TestEqual(TEXT("An aggressive fifth pushes"), Pushy[4], EGBSquadRole::Assault);

	// Aggression: the near but aggressive member does not become the base of fire.
	const TArray<EGBSquadRole> Aggro = Assign({ Member(500.f, 1.f), Member(700.f, 0.f) });
	TestEqual(TEXT("The disciplined one holds"), Aggro[1], EGBSquadRole::Suppress);
	TestEqual(TEXT("The aggressive one flanks"), Aggro[0], EGBSquadRole::FlankLeft);

	// An anchor stays the anchor and the rest flank, then push.
	const TArray<EGBSquadRole> Chief = Assign({ Member(600.f, 0.5f), Member(300.f, 1.f, true), Member(900.f, 0.5f), Member(1000.f, 0.5f) });
	TestEqual(TEXT("The chief is the anchor"), Chief[1], EGBSquadRole::Anchor);
	TestEqual(TEXT("Nearest follower flanks left"), Chief[0], EGBSquadRole::FlankLeft);
	TestEqual(TEXT("Next flanks right"), Chief[2], EGBSquadRole::FlankRight);
	TestEqual(TEXT("Then assaults"), Chief[3], EGBSquadRole::Assault);
	TestEqual(TEXT("Nobody: nothing"), Assign({}).Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBAITokensTest, "Glyphbound.AI.Tokens",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGBAITokensTest::RunTest(const FString& Parameters)
{
	using GBTactics::FTokenRequest;
	auto Req = [](int32 Id, float Since, float LastEnd = -1000.f, float Weight = 1.f)
	{
		FTokenRequest R;
		R.Id = Id;
		R.WaitingSince = Since;
		R.LastBurstEnd = LastEnd;
		R.Weight = Weight;
		return R;
	};
	auto Grant = [](std::initializer_list<FTokenRequest> List, float Held, float Max, int32 Total)
	{
		TArray<FTokenRequest> Waiting(List);
		return GBTactics::GrantTokens(Waiting, Held, Max, Total, 5, 10.f, 1.5f);
	};

	// Two tokens, three waiting: the two that waited longest, longest first.
	TArray<int32> Granted = Grant({ Req(0, 5.f), Req(1, 2.f), Req(2, 3.f) }, 0.f, 2.f, 3);
	TestEqual(TEXT("Two granted"), Granted.Num(), 2);
	TestTrue(TEXT("Longest waiting first, then the next"), Granted.Num() == 2 && Granted[0] == 1 && Granted[1] == 2);

	// A holder already uses one.
	Granted = Grant({ Req(0, 5.f), Req(1, 2.f) }, 1.f, 2.f, 3);
	TestTrue(TEXT("One free token: only the longest waiting gets it"), Granted.Num() == 1 && Granted[0] == 1);

	// Rest: the member that just fired waits behind the rested ones.
	Granted = Grant({ Req(0, 5.f), Req(1, 2.f, 9.5f), Req(2, 3.f) }, 0.f, 2.f, 3);
	TestTrue(TEXT("The one that just fired is passed over"), Granted.Num() == 2 && Granted[0] == 2 && Granted[1] == 0);
	Granted = Grant({ Req(1, 2.f, 9.5f) }, 0.f, 2.f, 3);
	TestEqual(TEXT("Alone, it still gets one"), Granted.Num(), 1);

	// Flyers count half; a big squad gets one more.
	Granted = Grant({ Req(0, 1.f, -1000.f, 1.f), Req(1, 2.f, -1000.f, 0.5f) }, 1.5f, 2.f, 3);
	TestTrue(TEXT("Half a token left: the flyer fits, the shooter does not"), Granted.Num() == 1 && Granted[0] == 1);
	Granted = Grant({ Req(0, 1.f), Req(1, 2.f), Req(2, 3.f) }, 0.f, 2.f, 5);
	TestEqual(TEXT("Five members: three shooters"), Granted.Num(), 3);
	Granted = Grant({ Req(0, 1.f), Req(1, 2.f), Req(2, 3.f) }, 0.f, 2.f, 4);
	TestEqual(TEXT("Four members: still two"), Granted.Num(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBAIPositionScoreTest, "Glyphbound.AI.PositionScore",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGBAIPositionScoreTest::RunTest(const FString& Parameters)
{
	using GBTactics::FPositionInput;
	const GBTactics::FPositionWeights W;
	auto Base = []()
	{
		FPositionInput In;
		In.DistanceToPlayer = 1000.f;
		In.RangeMin = 600.f;
		In.RangeMax = 1400.f;
		In.MinAngleToOthers = 90.f;
		return In;
	};

	FPositionInput CoverFiring = Base();
	CoverFiring.bCovered = true;
	CoverFiring.bFiringSpot = true;
	FPositionInput Open = Base();
	Open.bOpenLineOfFire = true;
	FPositionInput CoverBlind = Base();
	CoverBlind.bCovered = true;
	FPositionInput Nothing = Base();
	const float ScoreCoverFiring = GBTactics::ScorePosition(CoverFiring, W);
	const float ScoreOpen = GBTactics::ScorePosition(Open, W);
	const float ScoreCoverBlind = GBTactics::ScorePosition(CoverBlind, W);
	const float ScoreNothing = GBTactics::ScorePosition(Nothing, W);
	TestTrue(TEXT("Cover with a firing spot beats an open spot with a line"), ScoreCoverFiring > ScoreOpen);
	TestTrue(TEXT("An open spot with a line beats cover without a firing spot"), ScoreOpen > ScoreCoverBlind);
	TestTrue(TEXT("Cover without a firing spot beats nothing"), ScoreCoverBlind > ScoreNothing);

	// Spreading out: the same point is worth more when it is far (seen from the player) from the others.
	FPositionInput Bunched = CoverFiring;
	Bunched.MinAngleToOthers = 5.f;
	TestTrue(TEXT("Away from the others beats bunched up"), ScoreCoverFiring > GBTactics::ScorePosition(Bunched, W));

	// The band: outside it the score drops.
	FPositionInput TooFar = CoverFiring;
	TooFar.DistanceToPlayer = 1900.f;
	TestTrue(TEXT("Inside the band beats too far"), ScoreCoverFiring > GBTactics::ScorePosition(TooFar, W));

	// Flank angle: a flanker wants 60 to 120 degrees from the base of fire; a suppressor does not care.
	FPositionInput Wide = CoverFiring;
	Wide.Role = EGBSquadRole::FlankLeft;
	Wide.AngleFromBase = 90.f;
	FPositionInput Same = Wide;
	Same.AngleFromBase = 5.f;
	TestTrue(TEXT("A flanker prefers 90 degrees to 5"), GBTactics::ScorePosition(Wide, W) > GBTactics::ScorePosition(Same, W));
	FPositionInput SuppressorWide = CoverFiring;
	SuppressorWide.AngleFromBase = 90.f;
	FPositionInput SuppressorSame = CoverFiring;
	SuppressorSame.AngleFromBase = 5.f;
	TestEqual(TEXT("A suppressor does not care about the angle"), GBTactics::ScorePosition(SuppressorWide, W), GBTactics::ScorePosition(SuppressorSame, W));
	// The flanker in his view cone at close range is punished.
	FPositionInput Seen = Wide;
	Seen.bInViewCone = true;
	Seen.DistanceToPlayer = 700.f;
	FPositionInput Hidden = Seen;
	Hidden.bInViewCone = false;
	TestTrue(TEXT("A flanker in his view cone close by is worse"), GBTactics::ScorePosition(Seen, W) < GBTactics::ScorePosition(Hidden, W));

	// Path: a long path and a path that crosses his line of fire cost points.
	FPositionInput Far = CoverFiring;
	Far.PathLength = 1500.f;
	TestTrue(TEXT("A short path beats a long one"), ScoreCoverFiring > GBTactics::ScorePosition(Far, W));
	FPositionInput Crossing = CoverFiring;
	Crossing.bCrossesFireLine = true;
	TestTrue(TEXT("A path across his line of fire is worse"), ScoreCoverFiring > GBTactics::ScorePosition(Crossing, W));

	// Never a point he is practically standing on, whatever its cover.
	FPositionInput Adjacent = CoverFiring;
	Adjacent.DistanceToPlayer = 250.f;
	TestTrue(TEXT("Closer than half the minimum range is never acceptable"), GBTactics::ScorePosition(Adjacent, W) < -1000.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBAIBlockWindowTest, "Glyphbound.AI.BlockWindow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGBAIBlockWindowTest::RunTest(const FString& Parameters)
{
	TArray<GBTactics::FBlockedHit> Hits;
	TestEqual(TEXT("First hit"), GBTactics::AddBlockedHit(Hits, 0.f, 50.f, 3.f), 50.f);
	TestEqual(TEXT("Second hit 2 s later: both inside the window"), GBTactics::AddBlockedHit(Hits, 2.f, 50.f, 3.f), 100.f);
	// A hit at t=4: the one at t=0 is 4 s old and drops out, the one at t=2 stays. A window that restarts on every hit would give 150.
	TestEqual(TEXT("Third hit at 4 s: the first one is dropped"), GBTactics::AddBlockedHit(Hits, 4.f, 50.f, 3.f), 100.f);
	TestEqual(TEXT("Only two hits are kept"), Hits.Num(), 2);
	TestEqual(TEXT("A hit long after: alone"), GBTactics::AddBlockedHit(Hits, 20.f, 10.f, 3.f), 10.f);
	TestTrue(TEXT("Role names are words"), FCString::Strcmp(GBTactics::RoleName(EGBSquadRole::FlankLeft), TEXT("FlankLeft")) == 0 && FCString::Strcmp(GBTactics::RoleName(EGBSquadRole::None), TEXT("None")) == 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
