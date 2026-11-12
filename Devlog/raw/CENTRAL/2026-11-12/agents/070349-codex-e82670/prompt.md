# Code review: Glyphbound day 1 (read-only)

You are the second-opinion reviewer for **Glyphbound**, a first-person RPG shooter in Unreal Engine 5.9.0 (C++), made for a 4-day game jam. Do not modify any file. Do not build or run anything.

## Context
- Read `Design/ARCHITECTURE.md` first (short). Design is in `Design/GDD.md` (sections 3, 4, 6, 7, 8 matter most).
- Code: `Glyphbound/Source/Glyphbound/` (about 17,000 lines, written today by ten AI implementers in parallel against header contracts, then being compiled by another agent right now, so a missing include is NOT what we want from you).
- Data: `Glyphbound/Content/Data/*.json`, loaded by `Data/GBDataSubsystem.cpp` into the structs of `Data/GBDataTypes.h`.
- Engine source for checking APIs: `C:\Program Files\Epic Games\UE_5.9\Engine\Source`.

## What to look for (real defects only, in this priority order)
1. **Runtime bugs in the combat core**: `Combat/`, `Stats/`, `Weapons/`, `Spells/` (damage path, status effects, projectile collisions and lifetimes, ammo and reload state, cooldowns, mana spent / refunded wrongly, sustained spells never ending, time dilation left on after the QTE).
2. **Mismatches between data and code**: a stat, param, string key, id, enum value or class path used in a JSON file that the code never reads or spells differently (and the reverse: code reading a key no JSON defines, so it silently falls back to a default). Check ids across files (spells, weapons, items, status effects, enemies, encounters, sprites, skills modifier keys like `Spell.Fireball.ExplosionRadius`).
3. **Cross-module contract breaks**: one module assuming something another does not do (initialization order in `Player/GBPlayerCharacter.cpp` BeginPlay, who restores health on era travel, who saves checkpoints, who closes the death menu, delegate bindings never removed, objects kept alive only by raw pointers and collected by the GC, `TWeakObjectPtr` not checked).
4. **Things that work in the Editor but break in a packaged build**: editor-only APIs outside guards, assets loaded by path at runtime that the cook would not include (see `Glyphbound/Config/DefaultGame.ini`), transient textures, `LoadClass` paths.
5. **Logic errors in the QTE** (`QTE/GBQTELogic.cpp`, `QTE/GBQTEComponent.cpp`): scoring, time limit in real time, input routed while inactive.
6. **Violations of the project rule "no gameplay numbers in code"** only when it is a real balance value (damage, cost, cooldown, speed, duration) hardcoded where data should decide.

Do not report: style, naming, formatting, missing comments, micro-optimizations, hypothetical multiplayer issues (the game is single player), or anything about tests being too few.

## Output format
A numbered list, most severe first, at most 30 items. For each:
- `file:line` (or lines),
- what is wrong, in one or two sentences,
- why it matters when playing (what the player would see),
- the smallest fix you would make.
Finish with one short paragraph: the three things you would fix first, and anything you checked and found solid.
