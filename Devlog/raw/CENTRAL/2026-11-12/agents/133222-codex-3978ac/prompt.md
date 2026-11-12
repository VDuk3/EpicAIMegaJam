# Code review: Glyphbound, changes after the owner's first play-test (read-only)

You are the second-opinion reviewer for **Glyphbound**, a first-person RPG shooter in Unreal Engine 5.9.0 (C++), made for a 4-day game jam. Do not modify any file. Do not build or run anything.

## Context
- Read `Design/ARCHITECTURE.md` first (short). The design is in `Design/GDD.md`; the changelog entry v0.14 at the top lists what the owner asked for after playing the first build.
- The change under review is everything between git commit `798a4e5` and `HEAD` (`git diff 798a4e5 HEAD --stat`, then read the files). It was written by four AI implementers in parallel (reports in `Prompts/feedback1_notes.md`) and then compiled and integrated by a fifth. It builds, the tests pass and a scripted smoke run passes, so a missing include is NOT what we want from you.
- Code: `Glyphbound/Source/Glyphbound/`. Data: `Glyphbound/Content/Data/*.json`.
- Engine source for checking APIs: `C:\Program Files\Epic Games\UE_5.9\Engine\Source`.

## What changed (review these areas)
1. **Owned weapons** (`Weapons/GBEquipmentComponent.*`, `World/GBPickup.*`, `World/GBChest.cpp`, `Items/GBInventoryComponent.cpp`, `World/GBProgressSubsystem.h`): a picked-up weapon joins the owned list and goes to the hands only if its slot is empty; the player equips owned weapons from the Weapons page of the inventory. Ammo and element of stored weapons must survive swaps, checkpoints, death and era travel.
2. **Held ice shield** (`Spells/GBSpell_Shield.*`, `Spells/GBSpellbookComponent.*`, `Player/GBPlayerCharacter.*`, `Player/GBInputSet.*`): the spell summons a shield for a duration; the Block input (Q / L2, hold) raises it; only a raised shield blocks, and only from the front. Look for: the shield blocking when lowered, never expiring, surviving death or era travel wrongly, the move-speed modifier left on after it ends, blocking while a menu is open, the L2 conflict with the spell modifier on gamepad.
3. **Mouse in menus** (`Player/GBPlayerController.*`, `UI/GBHUD*.cpp`): cursor shown in menus, click handling through hit rects, input mode switched on open / close. Look for: the cursor or the UI input mode left on after closing a menu or after death / respawn, a click acting twice, clicks acting on a stale hit rect of a previous frame or menu, the game staying paused.
4. **Kill-Z** (`Core/GBGameMode.*`, `Player/GBPlayerCharacter.*`, `AI/GBEnemyCharacter.*`): falling below `KillZ` kills the player (once) and enemies (through the attribute component so encounters progress).
5. **Damage numbers** (`UI/GBHUD_DamageNumbers.cpp`, `Combat/GBCombatLibrary.*` global event): look for delegate lifetime problems (HUD destroyed and still bound, bound twice after a respawn), unbounded growth, numbers for the player's own damage taken.
6. **Explosions and textured structures** (`Combat/GBExplosionEffect.*`, `Combat/GBProjectile.*`, `Spells/GBSpell_Projectile.*`, `Spells/GBSpell_Wall.*`, `Core/GBPlaceholder.cpp`, `UI/GBPixelSpriteLibrary.*`, `Tools/Jam/make_materials.py`): every projectile explosion path must spawn the shockwave exactly once and never damage twice; knockback only on hostile characters; the material `/Game/Glyphbound/Materials/M_PixelSurface` must load in a packaged build (see `Glyphbound/Config/DefaultGame.ini`) and missing-material fallbacks must not crash.
7. **Enemy pathing** (`AI/GBSteering.*`, `AI/GBEnemyCharacter.*`, `World/GBTestArena.*`, `Config/DefaultEngine.ini`): navmesh built at runtime plus a local steering fallback and a stuck check. Look for: state machine dead ends (an enemy that can never attack because of the new line-of-sight rule, e.g. the trace hitting its own capsule, the player's shield, another enemy or an era-hidden prop), steering toward ledges, timers never reset, enemies frozen after era travel.

## What to report (real defects only, most severe first)
- Runtime bugs a player would hit, state that leaks between death / respawn / era travel / menus, and contract breaks between the modules above.
- Data and code mismatches: a key in a JSON file that the code spells differently or never reads, and the reverse.
- Things that work in the Editor but break in a packaged build.
- Real balance numbers hardcoded where data should decide.

Do not report: style, naming, formatting, missing comments, micro-optimizations, multiplayer issues (single player), or the number of tests.

## Output format
A numbered list, most severe first, at most 20 items. For each:
- `file:line` (or lines),
- what is wrong, in one or two sentences,
- why it matters when playing (what the player would see),
- the smallest fix you would make.
Finish with one short paragraph: the three things you would fix first, and anything you checked and found solid.
