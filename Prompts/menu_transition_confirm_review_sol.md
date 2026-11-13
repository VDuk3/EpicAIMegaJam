# Review: menu look transition and skill point confirmation — Glyphbound (UE 5.9, canvas HUD)

You are reviewing, read-only. Run `git diff HEAD -- Glyphbound/Source` and read around it in `UI/GBHUD.h`,
`UI/GBHUD.cpp`, `UI/GBHUD_Menus.cpp`, `Player/GBPlayerController.cpp` (how menu input reaches the HUD:
`InputMenuConfirm`, `InputMenuBack`, `InputMenuTabKey`, `InputMenuClick`, `InputSkillTrees`, `InputInventory`) and the
changed smoke steps in `Core/GBDevCommands.cpp` (UI block from "ui: future inventory"). Ignore Devlog files.

What changed (owner's requests, Nov 13):
1. **Transition when the menu's look changes while it stays open** (`DrawMenus` tracks `MenuSkinShown`; on a change
   it stamps `MenuSkinChangeTime`, replays the title glitch through `MenuOpenTime`, and `DrawMenuSkinTransition`
   draws for 0.42 s over the menu: into cyberpunk a scan line sweeping down over dark stone, into RPG a pixel dissolve
   of near-black cells). Opening or closing a menu resets the tracking, so a menu never animates on open.
2. **Every skill point asks first:** `ConfirmSkill` on a perk that can be bought sets `MenuState.PendingUnlock` and
   returns; the dialog (`DrawUnlockConfirm`, drawn last in `DrawMenus`, in the screen's look) has Confirm / Cancel
   buttons (`EMenuHitKind::ConfirmYes` / `ConfirmNo`). Enter (or Confirm) spends; Esc / B (`MenuBack`) cancels; a tab
   key cancels and turns the page; arrows and R do nothing while it is up; hover and clicks on the page underneath
   are ignored. A perk that cannot be bought shows its reason as before, with no dialog.

## What to look for (ranked)
1. A skill point spent without the dialog, or spent twice, or on a different node than the one the dialog names
   (cursor moved by hover / click / tab while pending; Enter repeating; a click on Confirm while the cursor sits on
   another node; the gun emblems' Enter, which opens a branch page, now interacting with `PendingUnlock`).
2. A dialog that cannot be closed or that stays up on the wrong page (switching menus with K / I / Tab / Esc /
   gamepad View / Menu, the inventory turning pages, death while a menu is open, `+1 skill point` or `Reset` buttons
   clicked while pending, the node becoming unaffordable while pending).
3. The transition: firing when it should not (menu open, every frame, a page change that keeps the same look),
   timing with real time vs paused game time, the `MenuOpenTime` reuse changing anything else that reads it (the death
   screen), drawing cost (the dissolve fills up to about 1,000 rectangles for 0.42 s).
4. Smoke: are the new checks able to fail (Esc spending, one Enter spending)?

Do not suggest style rewrites or new features, and do not judge the art. For each finding give: file:line, what goes
wrong, a concrete scenario, and a fix. If you find nothing real, say so plainly.
