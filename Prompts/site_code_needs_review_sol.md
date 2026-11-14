# Code review: the site map's code needs (Glyphbound, UE 5.9 C++)

Review the uncommitted changes (`git diff`, plus the untracked files under `Glyphbound/Source/Glyphbound` and `Glyphbound/Content/Data/Cutscenes.json`). Read-only. Ignore `Tools/Jam/Dress-SiteMap.py`.
Spec: `Prompts/site_code_needs.md`. Context: `Design/Levels/WORLD_PLAN.md` sections 4, 5 and 10.

Look for concrete defects, each with a failure scenario:
- `AGBEraBlock`: placed in a level (not spawned) with Presence/RequiredFlag/RemovedByFlag set per instance; hidden + no collision + out of the navmesh when absent, and exactly restored when present again (collision profile, visibility, navigation); era swap and flag change in any order; the navmesh really updates (dynamic navmesh).
- `AGBSeal`: usable once; `SealUsed` set before the cutscene; the travel when the cutscene ends or is skipped; player death or a second interact during the cutscene; travel refused because one is already running (then the player is stuck in the past with `SealUsed` set?).
- `UGBCutsceneSubsystem`: control, invulnerability and the weapon lock always restored (skip, death, level change, two cutscenes queued, a cutscene triggered by a flag while another plays, the menu or the QTE open); `TriggerFlag` plays once only; the skip delay.
- `AGBEncounter::RequiredFlags` and `AGBEncounterStarter`: both start paths gated; an encounter in the other era; a cleared encounter; the starter's prompt and toast.
- Pickups: placed pickups configured from `Drop` / `FlagOnPickup` in BeginPlay; spawned ones unchanged; `FGBDropDefinition::FlagOnPickup` through `SpawnDrops`; the gun carrier's pistol setting `GotFirstGun` exactly once.
- `AGBFinalPortal`: present only with `FinalPortalOpen`; an empty or wrong map path.
- Data (`Enemies.json`, `Encounters.json`, `Game.json`, `Cutscenes.json`): ids that do not exist, the Herald's class and look, the gun carrier first in the first wave.
- The new test `Glyphbound.World.RequiredFlags`: could a wrong implementation pass it?
- Smoke `site` steps in `Core/GBDevCommands.cpp`: do they check what they claim; ordering/timing.

Report findings ranked: [P1/P2/P3] title - file:line - failure scenario - fix. If none, say so.
