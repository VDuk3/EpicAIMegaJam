# Task: the code the real site map needs (seal, era blocks, encounter starters, new data) - Glyphbound (UE 5.9, C++)

You are the implementer. Read `C:\GameJam\CLAUDE.md` (rules). Do NOT edit `Design/GDD.md`, `Design/Levels/*` or the dev log. Do not commit. Do not open, create or edit any map or `.uasset`: the architect is building the site map itself in the Editor from `Design/Levels/world_layout.json`, and will place your new actors there.

## Context
The game is moving from the code-built test arena (`World/GBTestArena.*`) to a real map: one persistent level with two sub-levels ending `_Past` and `_Future` (`UGBEraSubsystem::ApplyEra` already swaps their visibility). Read `Design/Levels/WORLD_PLAN.md` sections 4, 5 and 10 ("Code needs"): this task is items 1, 2, 3 and 6 of that list, plus one new actor (A below). Everything here must also work when placed by hand in a level (editable properties on instances), not only when spawned in code.

Existing pieces to reuse: `World/GBEraComponent.*` (Presence, RequiredFlag, RemovedByFlag), `World/GBInteractable.*` (`AGBInteractableActor` already has an era component, a placeholder look and an interact volume), `World/GBPortal.*`, `World/GBPickup.*` (`FlagOnPickup`), `World/GBProgressSubsystem.*` (flags), `AI/GBEncounter.*`, `Data/*.json`, the HUD toasts in `UI/GBHUD*`.

## What to build

### A. `AGBEraBlock` (new, `World/GBEraBlock.h/.cpp`)
A placed level piece that exists only in one era and/or depends on progress flags: door barriers (rune wards, rune-sealed plugs, door leaves, the lockdown shutter), the four rune pylons, the cryo tube shell, a portcullis.
- Root: a `UStaticMeshComponent` (mesh and materials set per instance in the Editor; default the engine cube `/Engine/BasicShapes/Cube`, collision BlockAll, can affect navigation).
- A default `UGBEraComponent` subobject whose `Presence`, `RequiredFlag` and `RemovedByFlag` are editable on each placed instance.
- When it is not present it must not render, not collide, and not block the navmesh (the navmesh is dynamic; check that removing it lets enemies path through, the way the test arena's barriers do).
- No tick.

### B. The seal: `AGBSeal` (new, an `AGBInteractableActor`)
GDD 4, "Opening sequence" step 3. Placed in the past's Seal Room.
- Present only in the past, usable once: `CanInteract` is false once the flag `SealUsed` is set.
- Prompt: "Touch the seal".
- `Interact`: plays **caption cutscene 1** (C below), sets `SealUsed`, then calls `UGBEraSubsystem::RequestTravel(Future, Player, false, ...)` (no teleport: the player stays on the spot, the future's Seal Vault is the same room).
- A visible placeholder: a flat glowing disc on the floor (about 2 m wide), purple.

### C. Caption cutscenes (small, new)
No cutscene system exists. Build the smallest one the GDD needs: a sequence of English caption lines shown over a dimmed screen, with player control locked and the player invulnerable for its length (`SetControlEnabled`, the attribute component's invulnerability, the equipment lock), then control returns.
- Data in a new block of `Content/Data/Game.json` (or a new `Cutscenes.json` if cleaner): an id, and lines with text and seconds each.
- **Cutscene 1** (`Seal`), provisional text: "The seal flares under your hand." / "The castle tears away." / "Somewhere far ahead in time, a voice: 'Help me... please.'"
- **Cutscene 2** (`FirstGun`), provisional: "A strange weapon. Cold metal, no magic in it." / "The woman's voice again: 'You found it. Now come back for me.'"
- Cutscene 2 plays when the flag `GotFirstGun` is first set (whatever sets it).
- Drawn by the HUD in the current skin's style (the 90s RPG or the cyberpunk look, `UI/GBHUD*`); keep it simple and legible at 720p and 1080p.
- Skippable with Interact or Esc after the first second (skips the rest of the lines).

### D. Starting an encounter by interaction, with required flags
- `AGBEncounter` gets `TArray<FName> RequiredFlags` (editable): the encounter cannot start (neither from its trigger nor from `StartEncounter`) until every flag in it is set. Empty = as today.
- New `AGBEncounterStarter` (an `AGBInteractableActor`): references one `AGBEncounter` placed in the same level (editable on the instance), an editable prompt text (defaults "Begin"), and optional text shown when the flags are missing (default "Something still holds it shut"). `CanInteract` is true while the encounter is waiting, not cleared, in the current era and its required flags are set; `Interact` calls `StartEncounter()`. Used for the Valkyrie's rune cage (past: needs the four zone flags) and the cryo tube console (future: needs `Encounter.Valkyrie_Past.Cleared` and `Encounter.MiniBoss_Herald.Cleared`).
- Make the pure part (are the required flags met, given a flag set) a static function, and test it (E).

### E. Data (in `Content/Data/*.json`, provisional numbers, all marked provisional in a comment field if the format has one)
- **The gun carrier:** a new enemy `Orc_Future_GunCarrier` (an `Orc_Future` copy, a bit tougher) whose drop is the pistol at 100% and whose dropped pickup sets the flag `GotFirstGun` on pickup (`AGBPickup::FlagOnPickup`; if drops cannot carry a flag today, add that). `Opening_Future`'s first wave spawns it first, at the first spawn point. If spawn points are not used in list order, make them so.
- **New encounters:** `Skirmish_Breach` (future corridor fight: 4 `Orc_Future` + 2 `Drone_Future` in one wave), `Skirmish_Signal` (3 `Orc_Future` + 1 `Orc_Future_Enforcer`), `MiniBoss_Herald` (one enemy `Herald`).
- **The Herald:** a new enemy entry using the same actor class and look as `Entity_Phase1` (the tentacle machine) but with mini-boss numbers: about 55% of `Entity_Phase1`'s health and 80% of its damage (provisional), 3 skill points on its encounter, and a crate reward of the rifle (if encounter rewards can give a weapon; if not, say so in your report).
- Check `gb.ReloadData` still loads everything with no warnings.

### F. The final portal: `AGBFinalPortal` (new, an `AGBInteractableActor`)
Present in the future only when `FinalPortalOpen` is set. Prompt "Enter the rift". `Interact`: if `Game.json` has a non-empty value for the Entity map path, open that map; otherwise show a HUD toast "The rift is not stable yet" (the Entity's map does not exist yet). Placeholder look: a tall red-violet ring.

## Tests (pure logic only, `Tests/`, follow `CLAUDE.md` "Tests y verificación")
- `Glyphbound.World.RequiredFlags`: the static required-flags check (empty list, all set, one missing, flag set then cleared).
- Nothing else unless it is pure logic that is easy to break.
Every test must be able to fail: ask yourself which wrong implementation would still pass.

## Checks
- **Iterate cheaply (team rule):** while working on one part, run only the checks for that part (filtered smoke and tests, no game window when possible). The full game run and the full suite run once at the end, before you report.
- Build: `Tools/Jam/Build-Jam.ps1` (Editor target) while iterating. The Unreal Editor is CLOSED for you during this task; do not open it, do not run PIE.
- Tests: the anchored filter of what you touched, then the full suite once (under 30 s).
- Smoke: add one small step to the arena smoke (`gb.Smoke`, see `Core/GBDevCommands.cpp`) that spawns an `AGBSeal` and an `AGBEraBlock` (Presence Past, RemovedByFlag a test flag) in the arena, uses the seal, and checks: the travel to the future happened, `SealUsed` is set, the seal can no longer be used, the block is gone in the future and gone after its flag is set when back in the past. Run it with `Tools/Jam/Smoke-Jam.ps1 -Only <YourStep> -Offscreen` while iterating; the full `Smoke-Jam.ps1` once at the end, plus `Build-Jam.ps1 -Target Glyphbound`.

## Report
At the end, a short report: files changed, what each new class does, the data entries added, test and smoke results with their numbers, and anything you could not do or changed from this brief and why.
