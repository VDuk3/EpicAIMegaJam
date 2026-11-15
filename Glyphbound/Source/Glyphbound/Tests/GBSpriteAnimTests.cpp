#include "AI/GBSpriteAnimComponent.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBSpriteAnimDirectionTest, "Glyphbound.SpriteAnim.Direction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGBSpriteAnimDirectionTest::RunTest(const FString& Parameters)
{
	using GBSpriteAnim::ViewSector;
	// The sheet's columns turn counter-clockwise seen from above: 2 = the viewer stands at the character's LEFT.
	// In Unreal the actor's right is +90 yaw (Y is right), so with the actor facing +X its left side is -Y (bearing -90).
	TestEqual(TEXT("viewer in front sees column 0"), ViewSector(0.f, 0.f, -1, 0.f), 0);
	TestEqual(TEXT("viewer at the left (-Y) sees column 2"), ViewSector(0.f, -90.f, -1, 0.f), 2);
	TestEqual(TEXT("viewer at the right (+Y) sees column 6"), ViewSector(0.f, 90.f, -1, 0.f), 6);
	TestEqual(TEXT("viewer behind sees column 4"), ViewSector(0.f, 180.f, -1, 0.f), 4);
	TestEqual(TEXT("front-left is 1"), ViewSector(0.f, -45.f, -1, 0.f), 1);
	TestEqual(TEXT("front-right is 7"), ViewSector(0.f, 45.f, -1, 0.f), 7);
	// The same with the actor turned: facing +Y (yaw 90), its right is -X, so its left is +X (bearing 0).
	TestEqual(TEXT("actor yaw 90, viewer at +X is on its left"), ViewSector(90.f, 0.f, -1, 0.f), 2);
	// Yaw -170, viewer bearing -35: -135 degrees, i.e. 225 counter-clockwise (behind, on its right side).
	TestEqual(TEXT("actor yaw -170, viewer bearing -35 (wraps): behind-right is 5"), ViewSector(-170.f, -35.f, -1, 0.f), 5);
	// Sector borders at 22.5 degrees either side of a column.
	TestEqual(TEXT("20 degrees left of the face is still 0"), ViewSector(0.f, -20.f, -1, 0.f), 0);
	TestEqual(TEXT("25 degrees left of the face is 1"), ViewSector(0.f, -25.f, -1, 0.f), 1);
	TestEqual(TEXT("20 degrees right of the face is still 0"), ViewSector(0.f, 20.f, -1, 0.f), 0);
	TestEqual(TEXT("25 degrees right of the face is 7"), ViewSector(0.f, 25.f, -1, 0.f), 7);
	// Hysteresis: the previous column holds until the angle is past the border by the margin.
	TestEqual(TEXT("hysteresis keeps 0 at 25 degrees"), ViewSector(0.f, -25.f, 0, 6.f), 0);
	TestEqual(TEXT("hysteresis lets go at 30 degrees"), ViewSector(0.f, -30.f, 0, 6.f), 1);
	TestEqual(TEXT("hysteresis keeps 7 just past the border to 0"), ViewSector(0.f, 20.f, 7, 6.f), 7);
	TestEqual(TEXT("a far previous column does not hold"), ViewSector(0.f, 180.f, 0, 6.f), 4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGBSpriteAnimFramesTest, "Glyphbound.SpriteAnim.Frames",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGBSpriteAnimFramesTest::RunTest(const FString& Parameters)
{
	using GBSpriteAnim::FrameAtTime;
	bool bFinished = false;
	// A one-shot of 6 frames at 10 fps: each frame shows for 0.1 s, the last one counts as finished only after it was shown.
	TestEqual(TEXT("one-shot starts on frame 0"), FrameAtTime(6, 10.f, false, 0.f, bFinished), 0);
	TestEqual(TEXT("0.09 s is still frame 0"), FrameAtTime(6, 10.f, false, 0.09f, bFinished), 0);
	TestEqual(TEXT("0.15 s is frame 1"), FrameAtTime(6, 10.f, false, 0.15f, bFinished), 1);
	TestEqual(TEXT("0.58 s is the last frame"), FrameAtTime(6, 10.f, false, 0.58f, bFinished), 5);
	TestFalse(TEXT("the last frame is still showing at 0.58 s"), bFinished);
	TestEqual(TEXT("after the clip the last frame is held"), FrameAtTime(6, 10.f, false, 3.f, bFinished), 5);
	TestTrue(TEXT("and the one-shot is finished"), bFinished);
	TestEqual(TEXT("negative time (a timed clip waiting) is frame 0"), FrameAtTime(6, 10.f, false, -0.4f, bFinished), 0);
	TestEqual(TEXT("a loop of 8 at 8 fps wraps at 1 s"), FrameAtTime(8, 8.f, true, 1.1f, bFinished), 0);
	TestEqual(TEXT("and is on frame 2 at 1.3 s"), FrameAtTime(8, 8.f, true, 1.3f, bFinished), 2);
	TestFalse(TEXT("a loop never finishes"), bFinished);

	// Timing the hit frame to the AI's windup: the hit (frame 3 at 10 fps, 0.3 s of clip) must begin exactly HitIn seconds
	// from now, whatever the clamp did to the rate.
	const float HitIns[] = { 0.6f, 0.45f, 1.2f, 0.06f };
	for (const float HitIn : HitIns)
	{
		float Rate = 0.f;
		float Start = 0.f;
		GBSpriteAnim::TimeToHit(3, 10.f, HitIn, 0.5f, 2.5f, Rate, Start);
		TestTrue(*FString::Printf(TEXT("rate within its limits for %.2f s"), HitIn), Rate >= 0.5f - KINDA_SMALL_NUMBER && Rate <= 2.5f + KINDA_SMALL_NUMBER);
		TestEqual(*FString::Printf(TEXT("hit begins after %.2f s"), HitIn), (0.3f - Start) / Rate, HitIn, 1.e-4f);
	}
	float Rate = 0.f;
	float Start = 0.f;
	GBSpriteAnim::TimeToHit(3, 10.f, 1.2f, 0.5f, 2.5f, Rate, Start);
	TestTrue(TEXT("a long windup waits on frame 0 (negative start)"), Start < 0.f);
	GBSpriteAnim::TimeToHit(3, 10.f, 0.06f, 0.5f, 2.5f, Rate, Start);
	TestTrue(TEXT("a very short windup starts part way into the clip"), Start > 0.f);

	// Fallbacks for clips a sheet does not have.
	FGBSpriteSheet Sheet;
	Sheet.CellWidth = 8;
	Sheet.CellHeight = 8;
	Sheet.Clips.Add(TEXT("Idle"), FGBSpriteClip());
	Sheet.Clips.Add(TEXT("Walk"), FGBSpriteClip());
	Sheet.Clips.Add(TEXT("Attack"), FGBSpriteClip());
	TestEqual(TEXT("Run falls back to Walk"), GBSpriteAnim::ResolveClip(Sheet, TEXT("Run")), FName(TEXT("Walk")));
	TestEqual(TEXT("Cast falls back to Attack"), GBSpriteAnim::ResolveClip(Sheet, TEXT("Cast")), FName(TEXT("Attack")));
	TestEqual(TEXT("Leap falls back to Attack"), GBSpriteAnim::ResolveClip(Sheet, TEXT("Leap")), FName(TEXT("Attack")));
	TestEqual(TEXT("Attack2 falls back to Attack"), GBSpriteAnim::ResolveClip(Sheet, TEXT("Attack2")), FName(TEXT("Attack")));
	TestEqual(TEXT("Hurt has no fallback"), GBSpriteAnim::ResolveClip(Sheet, TEXT("Hurt")), FName(NAME_None));
	TestEqual(TEXT("a clip it has is itself"), GBSpriteAnim::ResolveClip(Sheet, TEXT("Walk")), FName(TEXT("Walk")));

	// A clip with a negative row or no frames is refused at load (it would sample outside the atlas); the good ones stay.
	FGBSpriteSheet Loaded;
	const FString Json = TEXT("{\"Id\":\"T\",\"CellWidth\":8,\"CellHeight\":8,\"Clips\":{\"Idle\":{\"Row\":0,\"Frames\":2,\"Fps\":4,\"Loop\":true},")
		TEXT("\"Bad\":{\"Row\":-1,\"Frames\":3,\"Fps\":4},\"Empty\":{\"Row\":2,\"Frames\":0,\"Fps\":4}}}");
	TestTrue(TEXT("a sheet with one good clip loads"), Loaded.LoadFromJsonString(Json));
	TestTrue(TEXT("the good clip stays"), Loaded.Clips.Contains(TEXT("Idle")));
	TestFalse(TEXT("Row -1 is refused"), Loaded.Clips.Contains(TEXT("Bad")));
	TestFalse(TEXT("Frames 0 is refused"), Loaded.Clips.Contains(TEXT("Empty")));
	TestEqual(TEXT("rows used come from the good clips only"), Loaded.RowCount(), 2);
	return true;
}

#endif
