# Task: reset the perks (skill trees) from the skill tree screen

Project: **Glyphbound** (Unreal Engine 5.9.0, C++), in `Glyphbound/`. Read `Design/ARCHITECTURE.md` and `Prompts/implementer_rules.md` first (you DO build and run things in this task; the rest of "Hard rules" applies). One build or run at a time. Do not open the Unreal Editor. Do not commit.

The owner asked for a way to reset the perks so he can test them better (GDD v0.16: changelog at the top of `Design/GDD.md`, section 5 and the "Skill reset" line in section 18). The architect decided the design below; implement exactly this.

## 1. The rule, in `Progression/GBSkillTreeComponent.h/.cpp`
- New member `UPROPERTY() int32 PointsSpent = 0;` = the points actually paid for the nodes owned now. `Unlock` adds the cost it really subtracted (`FMath::Max(0, Node->Cost)`). `InitializeFromData` sets it to 0.
- New public function `int32 ResetAll();`:
  - removes the modifiers of every owned node (`RemoveAllModifiers`, before clearing `Ranks`),
  - `Points += PointsSpent`, `PointsSpent = 0`, `Ranks.Reset()`,
  - broadcasts `OnSkillsChanged` (only if something changed), returns the refunded points (0 when nothing was owned).
  - The refund is the tracked `PointsSpent`, NOT recomputed from the current `Skills.json` costs: the owner edits costs while testing (`gb.ReloadData`), and a reset must give back exactly what was paid.
- New `int32 GetPointsSpent() const`.
- Snapshot: add `UPROPERTY() int32 SkillPointsSpent = 0;` to `FGBPlayerSnapshot` (`World/GBProgressSubsystem.h`, next to `SkillPoints`); save and load it in `SaveToSnapshot` / `LoadFromSnapshot` (clamped to >= 0).
- Header comment of `ResetAll`: free, any time (GDD 18, provisional).

## 2. The option on the skill tree screen, `UI/GBHUD.h`, `UI/GBHUD_Menus.cpp`
The skill trees are drawn by `AGBHUD::DrawSkillTrees`, used by both the K screen and the Skills page of the inventory; put the button there so both get it.
- A button labelled `Reset skills (+N pt)` where N = `GetPointsSpent()`, drawn just right of the "Skill points: X" label at the top (or in the free space beside it; it must not overlap the trees or the details panel at 1280x720 and 1920x1080). When `GetPointsSpent()` is 0 it is drawn greyed (`MenuGrey`) and does nothing.
- Clickable: new `EMenuHitKind::SkillReset` (add it to the enum; `ApplyMenuHover` ignores it like tabs; `ActivateMenuHit` calls the new `AGBHUD::ResetSkills(AGBPlayerCharacter*)`).
- `ResetSkills`: calls `ResetAll()`; if it refunded points shows the message `Skills reset: N points refunded` (2 s), else `No skills to reset` (1.5 s). The cursor stays on the same node.
- Keyboard / gamepad: a new menu input action `MenuReset` in `Player/GBInputSet.*` (Boolean, menu context only): **R** and **Gamepad_FaceButton_Top**. Check first that neither key is already mapped in the menu context; if one is, tell me in the report and do not remap it. Bind it in `AGBPlayerController` (`InputMenuReset`, `ETriggerEvent::Started`) -> new `AGBHUD::MenuReset()`, which acts only when the open screen shows the skill trees (`EGBMenu::SkillTrees`, or `EGBMenu::Inventory` on `PageSkills`) and then calls `ResetSkills`. Anywhere else it does nothing.
- Help line at the bottom of the K screen: add `R: reset skills`. In the details panel nothing changes.

## 3. Smoke script, `Core/GBDevCommands.cpp`
In the step "open the skill trees and unlock a node": remember the points before the unlock. Add a new step right after the "screenshot: skill trees" step (leave about one second after the screenshot; shift the later steps if needed, keeping the existing spacing between them) that calls `ResetAll()` and logs
`SMOKE skill reset refunded=%d points=%d rank=%d` (rank of the node unlocked before).
If the points after the reset differ from the points before the unlock, or the rank is not 0, log it with `UE_LOG(LogGB, Error, ...)` so the smoke run fails. Then take a screenshot `5b_skills_reset` one step later (so the screen shows the refunded points and the greyed button).

## 4. Tests
No new automation test (this is visible by playing and covered by the smoke check). Existing suite stays green.

## Verification (all must be true when you finish)
1. `powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Jam/Build-Jam.ps1` -> `Result: Succeeded`.
2. `powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Jam/Build-Jam.ps1 -Target Glyphbound` -> `Result: Succeeded`.
3. `powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Jam/Test-Jam.ps1 -MinTests 26` passes.
4. `powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Jam/Smoke-Jam.ps1` ends with `Smoke: done=True crashed=False LogGB errors=0 timedOut=False`, with `SMOKE path check passed`, the new `SMOKE skill reset` line with rank=0, and no `LogGB` warning.
Never make an error disappear by deleting behavior. All JSON files stay valid. No gameplay numbers in code.

## Final report (plain text)
1. The four result lines, copied verbatim, plus the `SMOKE unlock` and `SMOKE skill reset` lines.
2. What you changed, with file names.
3. Anything you could not do as described, and why (key conflicts included).
4. What a person should check by playing.
