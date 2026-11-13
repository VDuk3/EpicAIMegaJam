# Code review: weapon skill tree as a hub with gun branch pages; skill pages take their tree's look — Glyphbound (UE 5.9, C++)

You are reviewing, read-only, a change to the game's canvas HUD (`AHUD::DrawHUD`, no UMG) and to `Skills.json`. Run
`git diff HEAD -- Glyphbound/Source Glyphbound/Content/Data/Skills.json` and read the surrounding code in
`UI/GBHUD.h`, `UI/GBHUD.cpp` (`GetMenuSkin`), `UI/GBHUD_Menus.cpp` (`GetSkillNodes`, `OpenSkillBranch`,
`CloseSkillBranch`, `SelectSkillTree`, `NavigateSkills`, `ConfirmSkill`, `MenuBack`, `ActivateMenuHit`,
`UpdateMenuMouse`, `MenuClick`, `DrawSkillTrees`, `DrawSkillCategory`, the footer hints), `Data/GBDataTypes.h`
(`FGBSkillNodeDefinition::Branch`), `Progression/GBSkillTreeComponent.cpp` (how unlock checks requirements) and
`Player/GBPlayerController.cpp` (`InputMenuBack`). In `Core/GBDevCommands.cpp` read only the changed smoke lines.
Ignore Devlog and GDD files.

What changed (owner's requests):
1. **Weapon tree hub.** The weapon tree's perks now carry a `Branch` (Pistol, Rifle, Shotgun). The hub page shows
   the root perk (Heavy Rounds) and one emblem per gun: pseudo-nodes built in `GetSkillNodes` (`bCategory`, Id
   `Branch_<Gun>`, not in the data). Confirming or clicking an emblem opens that gun's page (the cursor moves to the
   branch's first perk); the shown page is always derived from the cursor node's (Tree, Branch). Back (button, Esc,
   gamepad B through `MenuBack`) returns to the hub on that gun's emblem. Requirement links to nodes on another page
   are not drawn; the emblems hang from the hub's root perks.
2. **Data:** the nine perks keep effects, costs and ranks; two prerequisites changed (Rapid Reload now needs Heavy
   Rounds, Arcane Conduit needs Point Blank). Three descriptions now start with "All guns:".
3. **Skill pages take their tree's look for the whole screen** (`GetMenuSkin`): on the Skill Trees screen and the
   inventory's Skills page, weapons = cyberpunk, spells = RPG, whatever the era.
4. **Spell page:** a darker panel background.
5. Grid: the weapon pages are centred vertically; the emblems are 2.3 perk boxes tall.

## What to look for (ranked)
1. Real bugs: a pseudo-node id (`Branch_*`) reaching `UGBSkillTreeComponent` (`Unlock`, `CanUnlock`, `GetRank`) or
   `FindSkillNode` and being treated as an error or logged; Enter on an emblem doing anything but opening; the cursor
   ending on a node of a page that is not drawn; Back that closes the whole menu instead of returning (or the
   opposite on the hub); `TreeCursor` restoring a stale pseudo-node; the inventory's Tab key (also MenuBack) now
   going back instead of closing; mouse hits from the previous page after Open / Back (same family as the stale-hit
   bug fixed earlier); `GetMenuSkin` disagreeing with the page that is drawn (cursor None on the first frame).
2. Data: any requirement cycle or a perk that can no longer be reached; anything else (code, smoke, the balance tool
   `Tools/Balance/balance_model.js`) that relied on the old prerequisites; the `Branch` JSON field parsing.
3. Layout at 1280x720 and 2560x1440: the emblem's outline glyph, title and rank segments overlapping or leaving the
   box; the Back button and the "WEAPONS // GUN" title overlapping on a narrow panel; the centred grid running out
   of the panel.
4. Per-frame cost: `GetSkillNodes()` builds an array; it is now also called from `GetMenuSkin` and the footer every
   frame. Only flag it if it matters at 60 fps.

Do not suggest style rewrites, renames or new features, and do not judge the design (perk grouping, art). For each
finding give: file:line, what goes wrong, a concrete scenario, and a fix. If you find nothing real, say so plainly.
