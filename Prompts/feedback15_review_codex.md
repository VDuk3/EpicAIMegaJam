# Code review: the shotgun's game code (read-only)

You are the second-opinion reviewer for **Glyphbound**, a first-person RPG shooter in Unreal Engine 5.9.0 (C++), made for a 4-day game jam. Do not modify any file. Do not build or run anything.

## Context
- Read `Design/ARCHITECTURE.md` first (short). The design of this change is in `Design/GDD.md`, section 18, the "Reload" bullet and its two sub-bullets about the shotgun.
- The implementer's brief is `Prompts/feedback15_shotgun_viewmodel_pump_shells.md`: it is the specification of what was asked.
- The change is the uncommitted work in these files (compare with `git diff HEAD -- <file>`): `Glyphbound/Source/Glyphbound/Weapons/GBGun.h`, `Weapons/GBGun.cpp`, `Weapons/GBHeldItem.h`, `Weapons/GBEquipmentComponent.cpp`, `Core/GBStatNames.h`, `UI/GBHUD.cpp`, `Tests/GBWeaponsTests.cpp`, `Glyphbound/Content/Data/Weapons.json`, and the shotgun block of `Core/GBDevCommands.cpp`.
- **Not part of this review:** another chat has uncommitted work in the same tree (the Entity boss: `AI/GBEntity*`, `AI/GBTentacle*`, `Combat/GBCombatLibrary.*`, `Combat/GBProjectile.*`, `Tests/GBEntityTests.cpp`, and its own blocks inside `Core/GBDevCommands.cpp`). Ignore it.
- Engine source for checking APIs: `C:\Program Files\Epic Games\UE_5.9\Engine\Source`.

## What to look for (real defects only, in this priority order)
1. **Ammo and reload state.** The shotgun reloads one round at a time and keeps its reserve in rounds inside `FGBAmmoState::SpareMagazines`. Look for: any reader or writer of `SpareMagazines` anywhere in the code that still treats the shotgun's value as magazines (pickups, inventory, equipment's full ammo state, stored guns, the HUD, era travel and respawn refills, dev commands); rounds created or lost when a reload is stopped by firing, by a weapon switch, by `SetAmmo` or `RefillAmmo`, or when a frame is longer than one shell's cycle; a reload that can start or continue with a full gun or an empty reserve; fire queued during a reload firing twice or never; division by zero when `MagazineSize` or `ReloadTime` is 0 or missing.
2. **Regressions on the pistol and the rifle.** They are approved by the owner and must behave exactly as before: reload timing, rounds lost on an early reload, the `--` display, the reload animation and its dev pin, the shot kick, the trigger, the glow, the energy bar and its blink.
3. **The pump and the reload look.** State left behind after a weapon switch, a death or an era travel (pump off its rest place, shell visible, gun stuck in its reload pose); two places writing the viewmodel root's transform; the dev pins changing gameplay state.
4. **The pure reload logic and its tests** (`Tests/GBWeaponsTests.cpp`): for each new test, name a wrong implementation that would still pass it, if there is one.
5. **Packaged build:** editor-only APIs outside guards; meshes loaded by path at runtime that the cook would not include (see `Glyphbound/Config/DefaultGame.ini` and how the pistol's and rifle's meshes are cooked).
6. **Gameplay numbers hardcoded** where the data should decide (visual offsets and animation shares in code are allowed by the project's rules).

Do not report: style, naming, formatting, missing comments, micro-optimizations, hypothetical multiplayer issues (the game is single player), or anything about the art values being provisional.

## Output format
A numbered list, most severe first, at most 20 items. For each:
- `file:line` (or lines),
- what is wrong, in one or two sentences,
- why it matters when playing (what the player would see),
- the smallest fix you would make.
Finish with one short paragraph: what you would fix first, and what you checked and found solid.
