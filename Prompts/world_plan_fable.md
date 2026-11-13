# Brief: plans for the game's one site in two eras (planner: Claude Fable 5.2, High)

You are the **level planner** for Glyphbound, a game for the Epic AI MegaJam 2026 (UE 5.9.0, submission Nov 15 20:59 EST, today is Nov 13 evening). Your job is **planning only**: the plans and the written level design. The architect (Claude Opus 5.5, the chat that launched you) does all the assembly in Unreal afterwards from your plan, so write it for that reader: exact enough to build from, with no guessing.

Write everything in **English** (the game and the dev log are in English). Reply to the architect at the end with a short summary and the list of files you wrote.

## Read first
- `CLAUDE.md` (project rules), `Design/GDD.md` (sections 0.1, 1-4, 6 "elemental zones", 10, 11 are the ones that matter), `Design/ARCHITECTURE.md`.
- Era code: `Glyphbound/Source/Glyphbound/World/GBEraSubsystem.*`, `GBEraComponent.*`, `GBPortal*`, `GBCheckpointVolume*`, `GBTestArena.*` (how eras, `_Past` / `_Future` sub-levels, portals and era-gated actors work today). Plan within what the code supports; where the plan needs something new, say so in a "Code needs" section.
- Movement data: `Glyphbound/Content/Data/Player.json`. Walk 750 cm/s, jump velocity 800 with gravity scale 1.6 (one jump reaches about 2.0 m), double jump is an air passive learned in the air zone (`MaxJumps` 1 at start), dash 700 cm in 0.18 s, capsule radius 38 / half-height 90, ledge climb (mantle) up to 160 cm above the hands.
- Asset inventory: `Devlog/days/2026-11-13_fab_assets_block.md`.

## The site and the owner's vision (owner's answers, Nov 13)
- **One site, two times.** The past is a **medieval castle**; the future is a **futuristic military base** built on the same site. The player crosses between them **only through fixed portals**, which are safe points (travel restores health, mana and ammo). Design both versions together: the same macro layout, so the player recognises the site (the castle's courtyard is where the base's yard is, its keep is where the base's command block is, and so on), with era-gated zones and cross-era unlocks (something done in the past opens something in the future).
- **Size:** a bit more than "compact". Compact was: courtyard, seal room, 4 elemental zones and the Valkyrie arena in each era, about 10-15 minutes of play. Go a bit beyond that, but everything must be buildable by one architect in about a day and a half, so keep the piece count sane.
- **Combat** (owner): fast and wide, Cyberpunk 2077 style, the player keeps moving around the room with dash and double jump. **Main fights happen in large rooms with plenty of space**; a few fights happen in corridors, but only a few. Large rooms need: space to dash and circle, cover the enemies can path around (enemies must never get stuck on walls; they use the navmesh), some verticality (platforms reachable with a jump, the double jump or the 1.6 m ledge climb), and more than one way in or out.
- **Past content** (GDD): opening (the player starts in the castle, kills a small group of pixel-art orcs, reaches **the seal**, is sent to the future), **4 elemental zones** in the castle (fire, lightning, ice, air), each with a challenge (wave, sub-boss or boss) and a chest or book at the end, the **Valkyrie arena (past)**: she is trapped inside magic runes.
- **Future content** (owner): the intro fight against armed orcs (one of the first drops the first gun, and the player cannot go on without picking it up), **several wave arenas**, a **mini-boss arena for the tentacle creature** already in the project (`/Game/Glyphbound/Enemies/Entity`: a floating machine with light tentacles, about **9.4 m tall** as built, it floats, banks and strafes and shoots bolts; it will also strike with its tentacles in melee, so plan a big tall room), and the **Valkyrie arena (future)**: she is in a cryogenic tube. The final portal to the Entity's separate map opens late; plan where it stands.

## The asset kits
All under `Glyphbound/Content` (outside git). Use **the modular pieces to build the spaces**: walls, floors, stairs, arches, pillars, platforms, doors. **Do not** drop whole pre-built buildings or the packs' demo levels in as they are.
- **Medieval (past):** `CastleTown` (Hivemind, Medieval Kingdom). **Only the castle is needed**; the town (houses, market, roads, the town's props, etc.) is not, and the architect will delete it. This castle will later be turned into **pixel art in another workflow** (3D geometry with low-resolution pixel textures, see GDD section 3), so it is not the final look: keep the set of castle modules **small** (aim for about 25-40 distinct meshes), because each one has to be converted. List them.
- **Future (used as they are):**
  - `SciFiWorld` (Sci-Fi Creatures Research Lab): modular lab room pieces in `Modules/`, cryo capsules, screens, holograms.
  - `High_Tech_Base` (copied in today): 101 meshes in `Models/`, sci-fi base pieces (bridges, panels, pipes, props).
  - `MaterialsScifi` (9 sci-fi panel materials, usable on simple shapes).
  - **Spaceship Corridor Damaged**: a Blender model the architect will prepare and import (not in Unreal yet). Main body **40.5 x 5.3 x 3.33 m**, a damaged corridor with flickering light from broken cables. Plan it as a corridor run (it can be cut into segments); remember corridor fights are few.
  - VFX for dressing: `SciFi_Impacts`, `MuzzleFlash3D`, `GunFX`, `Smoke_NiagaraFluids`, `HologramShader`.
- **Measure, don't guess.** The Unreal Editor is running with an MCP server (tools `mcp__unreal-mcp-59__*`; discover with `list_toolsets` -> `describe_toolsets` -> `call_tool`, never invent tool names). Use the read-only tools (for example `EditorToolset.StaticMeshToolset`, `EditorToolset.AssetToolset.FindAssets`) to read each candidate module's bounds and pivot, so the plan can state a snapping grid per kit and exact piece counts.

## Hard limits
- **Read-only in Unreal.** Do not create, edit, save, move or delete any asset or level, do not open or change maps, do not start Play In Editor, do not restart or close the Editor, do not compile. Only read.
- Write only under `Design/Levels/`. Do not commit (the architect commits).
- Do not run other agents or CLIs.
- Keep tool use cheap: query the meshes you consider, not all 3,000 assets one by one.

## What to deliver (in `Design/Levels/`)
1. `WORLD_PLAN.md`:
   - **Overview:** the critical path in both eras and the order the player meets things, with an estimated play time.
   - **Shared coordinate frame:** origin, axes, floor levels in cm, one grid per kit (from the measured module sizes) and how the two eras line up.
   - **Zone by zone, in both eras:** name, purpose, bounds (cm), floor heights, entrances, cover and platforms, enemy spawn points and encounter (wave sizes are provisional), pickups and chests, lighting mood, and the modules used with counts.
   - **Portals:** position of each one. The floor must be walkable and clear at that spot in both eras; travel only happens at portals, so elsewhere the two layouts may differ.
   - **Era-gated zones and cross-era unlocks** (at least two of each), and where the final portal stands.
   - **Module catalog per kit:** asset path, measured size, use. Plus the **short list of castle modules** for the pixel-art conversion.
   - **Delete list:** the `CastleTown` folders and assets that are town-only and not used by your castle (check that nothing you keep depends on them).
   - **Budget:** rough piece and draw counts, lights, and a warning list of anything heavy (Niagara Fluids smoke, large textures) against a packaged game near 1 GB.
   - **Build order:** for the architect, so a playable slice exists early (for example, the opening plus one arena per era first).
   - **Code needs:** anything the plan needs that the code does not do today.
   - **Provisional:** mark every design choice the owner has not made as provisional.
2. **Top-down plans** for each era (and a side section where height matters), as SVG files drawn to scale with a grid, zone names, portals, spawns and the critical path. Use a scale bar. Keep the two eras' plans at the same scale and origin so they can be overlaid.

## Iteration rule (team rule, applies to every brief)
While working on one part, run only the checks for that part (filtered smoke and tests, no game window when possible); the full game run and full suite run once at the end. For you this means: no builds, no smoke, no tests at all; this is a planning task.
