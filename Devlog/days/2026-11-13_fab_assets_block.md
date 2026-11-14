# Block: inventory of the owner's Fab assets, and the plugins they needed (Nov 13, CENTRAL)

> Written by the architect chat of this block (Claude Code, Claude Opus 5.5). To be merged into the day entry by whoever closes the day. Times in UTC.

## The owner's request
> *"Descargamos múltiples assets a través de Fab que ya están en el proyecto. Adicionalmente, te dejamos la ruta de un asset que no nos permite descargarlo, sino crear un proyecto aparte. Y te dejamos el zip de otro escenario que no nos permite añadirlo al proyecto, pero podemos descargarlo en formato de Blender. Analiza y haz inventario de los assets."*

Translation: "We downloaded several assets through Fab that are already in the project. We also give you the path of an asset that cannot be downloaded into it, only created as a separate project. And the zip of another environment that cannot be added to the project but can be downloaded in Blender format. Analyse and inventory the assets."

Mid-task: *"Hubo un asset que al importarlo pidió unos plugins, no recuerdo cuál, encárgate de eso."* ("One asset asked for some plugins when it was imported, I don't remember which; take care of it.")

## How the inventory was made
- **Which listing made each folder:** the Fab plugin writes `Add listing to project - <id>` and one `Found <file>` line per imported file to the Editor log. Grouping those lines by top folder tied each `Content/` folder to its listing.
- **Title, seller and license:** the Editor's Fab toolset over MCP (`GetFabListingDetails` and `SearchFabMyLibrary`, which says which license the owner holds).
- **Engine modules each pack needs:** a scan of every `.uasset`/`.umap` for `/Script/<Module>` strings, compared with the plugins the Editor has enabled (`PluginToolset.ListEnabledPlugins`).
- **The Blender zip:** listed and its file headers read without extracting it. Opening the `.blend` in Blender (headless, scripts disabled) was blocked by the session's permission check, so its mesh counts come from the listing.

## In the project (`Glyphbound/Content`, outside git like all Content)

| Folder | Fab listing (seller) | License held | Files / size | What it is | Plugins it needs |
|---|---|---|---|---|---|
| `CastleTown` + most of `__ExternalActors__` | [Medieval Kingdom](https://www.fab.com/listings/42d4a792-2b66-423d-9b20-84d6b2c578d8) (Hivemind) | Professional | 1,798 / 7.9 GB, plus 8,864 external actor files / 430 MB | Modular medieval castle and town: castle, houses, props, scanned foliage, sky sphere, water, VFX. Level `Levels/Persistant/PL_CastleTown` with 10 sub-levels and 29 level instances, plus an asset zoo and a foliage zoo. Textures are 5.4 GB of the 7.9 | Water, ChaosCloth (enabled in this block); MovieRenderPipeline and WorldPartitionHLODUtilities (left off, see below) |
| `SciFiWorld` | [Sci-Fi Creatures Research Lab](https://www.fab.com/listings/c2494db5-55d2-4e35-9c57-540d0f2cf455) (Tirgames Assets) | Professional | 455 / 914 MB | One modular lab room (white and dark skins), 6 unrigged alien creatures in stasis capsules, hologram, screen and liquid-glass shaders; 3 maps (`SciFiCreaturesResearchRoomA/B/_Overview`). Its screen images are AI-generated, per the listing | none extra |
| `SciFi_Impacts` | [Sci-fi Impacts](https://www.fab.com/listings/2b206307-1e17-430c-9e43-39bb39e2d4b1) (SHOGUN GAMES) | Professional | 250 / 396 MB | 27 Niagara bullet impacts, 15 sonic-boom tracers, 7 plasma burn decals, demo map `Alleyway/Maps/AlleyDemo` and `Maps/Overview` | CableComponent, only for `AlleyDemo` (enabled in this block) |
| `MuzzleFlash3D` | [Muzzle Flash 3D](https://www.fab.com/listings/5cbcbcfa-8d17-4ed2-b0f1-9f9d96d95ba1) (SHOGUN GAMES) | Professional | 57 / 159 MB | 20 Niagara muzzle flashes with user parameters, map `Maps/Overview` | none extra |
| `MaterialsScifi` | [Scifi Material Pack](https://www.fab.com/listings/5450f0ae-52b1-4194-890e-097d0427617a) (JulioSillet, free) | Personal | 43 / 222 MB | 9 PBR sci-fi panel materials, 4K textures, map `Maps/L_Showcase` | none extra |
| `HologramShader` (from Nov 12) | [Hologram Shader](https://www.fab.com/listings/cfb64bbf-70e6-42db-9cab-7be76f796445) (SERLO, free) | Personal | 14 / 34 MB | Already trimmed on Nov 12 to `M_Hologram`, three instances and their textures | none extra |

**Orphan files that came with Medieval Kingdom.** `__ExternalActors__` also holds `Medieval_Docks` (2,277 files), `Maps/bloking`, `Maps/SL_Landscape` (308), `FirstPerson` and `ThirdPerson` (1 each). Their maps are not in the pack, so nothing loads them, and the cook only takes what a cooked map references. `CastleTown/Levels/SubLevels/bloking_HLOD0_Instancing` belongs to the missing `bloking` map too. Clean-up candidates, left in place for now.

**Package size.** The cook only takes what our maps reference (`DirectoriesToAlwaysCook` lists only `/Game/Glyphbound/*`), so these 9.6 GB only reach the zip as far as the game uses them. Medieval Kingdom's 5.4 GB of textures is the one to watch against the 1 GB limit.

## Two downloads that were cancelled, then installed again
Both were added at 21:54 and cancelled by the Fab plugin at 22:10:19, 0.1 s after Medieval Kingdom finished. The log gives no reason.

The owner then said to download both again (*"Sí, ambos"*). The architect installed them from the owner's library with the Fab toolset (`InstallFabListing`, format unreal-engine, built for UE 5.8). Both imported at 22:37 to 22:39.

| Folder | Fab listing (seller) | License held | Files / size | What it is | Plugins it needs |
|---|---|---|---|---|---|
| `Smoke_NiagaraFluids` | [Smoke Niagara Fluids](https://www.fab.com/listings/cdb1ca29-88fc-4818-9980-cbbed6abc59e) (SHOGUN GAMES) | Professional | 6 / 44 MB | 5 smoke systems built on Niagara Fluids (`NS_Smoke_NF_01..05`), map `Overview` | **NiagaraFluids** (beta). The systems are built from its Grid3D modules and gas materials. Enabled in this block |
| `GunFX` | [Realistic Gun VFX](https://www.fab.com/listings/21573c1a-e494-4194-9926-0c03f48a5563) (Hivemind) | Professional | 571 / 833 MB | 125 Niagara systems: 14 muzzle flashes, 93 bullet impacts by surface, 12 shell ejections, overheat smoke, a bullet trail and a flare. Meshes, materials, maps `ShowcaseMap` and `Overview` | none extra |

**Check:** NiagaraFluids was enabled over MCP and the Editor restarted twice. The first boot took 20 s, the second 15 s, so there is no steady cost. Content validation passed on 10 assets with no errors or warnings: the 5 smoke systems, plus a muzzle flash, an ejection, an impact, the bullet trail and an overheat smoke from GunFX. The game target still builds (11 s).

**Note for use:** Niagara Fluids is a GPU grid simulation, and the seller warns it gets expensive at high quality. Before a smoke system goes into the game, measure it on the RTX 4080 Laptop, or bake it to a flipbook like the pixel VFX.

## Outside the project
**High Tech Base** ([listing](https://www.fab.com/listings/9970b512-825c-4d6d-917a-1c0480de16fb), YaMaKundra, free, Personal license held). Fab ships it only as a complete project, so the owner created `Documents/Unreal Projects/HighTechBase`.
- **Contents:** `Content/High_Tech_Base`, 565 files / 1.0 GB, almost all textures (999 MB); 101 meshes / 6 MB, 97 materials, 3 maps (`L_HighTechBase_Level`, `L_Materials`, `L_Overview`).
- **Engine:** made for UE 5.7, which 5.9 opens. It references no extra plugin modules. The listing mentions the Substance plugin, but says the textures show without it.
- **Risk:** the seller says every material uses **Substrate**. Glyphbound runs with Substrate off (`r.Substrate=0`, checked live). Before bringing it in, migrate one material and check how it renders here.
- **How to bring it in:** Migrate (`Asset Actions > Migrate`) from the HighTechBase project into `Glyphbound/Content`. Not done yet.

**Spaceship Corridor Damaged** ([listing](https://www.fab.com/listings/dcdef266-660e-477d-9f5a-03dc9579f3c6), ArmTm, free, Personal license held). Formats Blender and OBJ only, so it cannot go through the Fab plugin.
- **The zip** (`spaceship_corridor_blend.zip`, 141 MB):
  - `Spaceship corridor.blend`: 103 MB, uncompressed, header `BLENDER-v403` (Blender 4.3).
  - `Screen_textures/`: `planet_low_orbit.exr` (62 MB), `Screen.jpg`, `Screen_video.mp4` (1 MB).
- **The seller's own description:**
  - 323,783 vertices and 344,393 faces; main body 40.5 x 5.3 x 3.33 m.
  - Most textures are procedural, and the model **has no UVs**, has n-gons and non-manifold edges.
  - It was made "for inspiration", with an animated flicker from damaged cables.
- **Cost to use it in Unreal:** UV unwrapping, baking the procedural materials to textures, cleaning the n-gons, and splitting it into pieces. Real Blender work, not just an import.

## The plugins the import asked for
The asset was **Medieval Kingdom**. When its import finished, the Fab plugin scanned its 10,630 files for plugin dependencies. The module scan found that the pack uses modules from plugins we had turned off to boot faster.

**Decision (architect): enable only what the packs' real assets use.**
- **Water** (experimental; pulls in Landmass and Buoyancy).
  - Used by the castle's water (Gerstner waves, buoyancy, water fluid simulation), by 8 building level instances (`LI_Building_0x_Fix`) and by 2 landscape actors of `SL_Landscape`.
- **ChaosCloth**: 2 cloth skeletal meshes (`SKM_Flag_03`, `SKM_MarketCloth_01`). In 5.9 it depends on Water.
- **CableComponent**: `SciFi_Impacts/Alleyway/Maps/AlleyDemo`. A small runtime plugin.
- **Left off:**
  - MovieRenderPipeline: only `CastleTown/Cinematics/VideoPreset`, a render preset.
  - WorldPartitionHLODUtilities: only the HLOD of the missing `bloking` map.
  - NiagaraFluids at first: its pack was not in the project yet. Enabled later, when Smoke Niagara Fluids was installed again (see above).

**How:** `PluginToolset.SetPluginEnabled` over MCP, then an Editor restart through `Stop-EditorMcp.ps1` / `Start-EditorMcp.ps1`. `Glyphbound.uproject`: the `false` entries for ChaosCloth and CableComponent removed (both are on by default), Water added.

**Measured:** the Editor and MCP were ready in 15 s before the change. The first boot with the new plugins took 40 s, and the next took 15 s again, so the steady cost is nil.

**Check:** content validation over MCP on 7 assets:
- **Load without errors:** the two cloth meshes, `BP_BuoyancyExample`, `GerstnerWaves_Ocean`, `LI_Building_02_Fix` and `AlleyDemo`.
- **One real error:** `CastleTown/Water/Effects/Waterbody/WaterBody_Niagara_ShoreFoam` reads `WaterZone.WaterInfoTexture`, which no longer exists in UE 5.9's Water. The pack was built for 5.3 to 5.8. Only the shore foam effect is affected; it gets fixed or dropped if the game uses the castle's water.

## Licenses and AI
- **Fab Standard License for all eight listings** (all eight now in the project or the owner's folders): Professional for the paid packs, Personal for the free ones. The third-party list for the submission gets each pack's name, its seller and that license.
- **"No AI" on every listing:** `aiTrainingForbidden` is set on all eight, under the Fab EULA section "Content that May Not be Used As Training Input Into Generative AI Programs".
  - Decision: these packs' files are not fed to the generative tools we use for our own art (Meshy, Tripo, image generators).
  - The owner should read that EULA section to confirm what else it covers.

## Toward the maps: the owner's direction, the planner, High Tech Base brought in
The owner: *"vamos a armar los mapas ambos... que se armen pensando en el uno y el otro al mismo tiempo"*, then *"es el mismo sitio, pero uno en el futuro y otro en el pasado"*. Translation: build both maps, designing each with the other in mind, because they are the same site, one in the future and one in the past.

**The owner's rules:**
- Build the spaces from the **modular pieces**, not by dropping whole assets in as they are.
- Medieval Kingdom: only the castle is needed. The town can be deleted, and the castle will become pixel art in another workflow.
- The futuristic assets are used as they are.

**The owner's answers to four questions:**
- **The future site:** a futuristic military base.
- **The future's content:** several wave arenas, plus a mini-boss: the tentacle creature already in the project.
- **The size:** a bit more than "compact".
- **The Blender corridor:** prepare it and use it.

On combat: *"rápido, amplio... combates al estilo Cyberpunk 2077... los combates principales serán en salas amplias"*, that is, fast and wide, in the style of Cyberpunk 2077, with the main fights in large rooms and only a few in corridors.

**The planner:** a Claude Fable 5.2 subagent at High effort, as the owner asked. Its brief is `Prompts/world_plan_fable.md`. It is read-only in Unreal and writes only to `Design/Levels/`; the architect assembles the levels from its plan.

**High Tech Base brought in.** Its `Content/High_Tech_Base` (565 files, 1.0 GB) was copied into `Glyphbound/Content`, so the planner can measure its modules.
- **Why a copy:** its only references outside its folder are stale map names and two reference models, so a copy does the same job as Migrate.
- **Substrate:** one of its five master materials, `M_Material`, did not compile with our `r.Substrate=0`. It is the master behind the walls, concrete, panels and rubber. Its "Use Opacity" static switch had nothing on its False pin, so every instance would have shown the default material.
- **The fix:** the architect wired a constant 1 (opaque) to that pin over MCP (`MaterialToolset`), then recompiled and saved.
- **Check:** the five masters and eight instances and meshes now validate with no errors or warnings. Substrate stays off for the project.

## The world plan (Claude Fable 5.2, High, about 46 minutes)
**What the planner delivered in `Design/Levels/`:**
- `WORLD_PLAN.md` (737 lines).
- Top-down plans of each era, an overlay of the two and side sections, as SVG, at one scale and origin.
- `world_layout.json`: every coordinate.
- `make_plans.js`: generates the JSON and the drawings, and checks the layout: spawns out of walls, portals clear in both eras, at least 2 m between obstacles, nothing free-standing within 3 m of a wall, no fight starting on arrival.
- `castle_keep_list.txt`.

It read mesh bounds, vertices and collision through the Editor's MCP, and changed nothing (0 dirty packages).

**The layout:**
- **The castle:** 3 x 3 blocks on a 16 m grid (112 x 112 m).
- **The courtyard:** it is the base's yard.
- **The Great Hall:** it is the Containment Hall, and holds the Valkyrie in both eras.
- **The Seal Room:** it is the Seal Vault. The final portal opens on the seal.
- **The east wing:** the Forge (fire) in the past, the Generator Hall in the future.
- **The west wing:** the Stormworks (lightning) in the past, the Server Hall in the future.
- **The north corners:** the Frozen Cistern (ice) and the High Ward (air) in the past. In the future they shrink to corridors.
- **South of the gate, future only:** the Breach Corridor (the Blender model), an antechamber and a 48 x 48 x 24 m hangar for the tentacle machine.

**The route:** 21 stations, six portals on the shared floor, four era-gated zones and three cross-era unlocks. About 22 to 25 minutes for a first run.

**What the planner measured:**
- Two kit pieces block their own doorways with one convex hull, and the HTB bridge is solid under its deck. So each era gets a collision shell of boxes, built first as the greybox, with the kits placed as looks only.
- The castle needs 31 meshes for the pixel-art conversion, and 101 of the 1,797 `CastleTown` packages are kept.

**Provisional choices for the owner:**
- The Valkyrie arenas merged into the Great Hall / Containment Hall.
- The mini-boss is named "the Herald" and shares the Entity's model.
- The zone names and the play length.

**Renders:** `media/2026-11-13/worldplan_plan_past.png`, `worldplan_plan_future.png`, `worldplan_plan_overlay.png` and `worldplan_section_side.png`, made with headless Edge from the SVG files.

**The corridor model, read in headless Blender with its scripts disabled:**
- **Size:** 5.3 x 40.5 x 3.3 m.
- **Geometry:** about 800k placed triangles, 39 meshes.
- **Materials:** 34 procedural materials.
- **Lights:** 8.
- **Props:** fire extinguishers, an axe and crates, whose textures are packed inside the file.

Its source files moved to `Glyphbound/SourceArt/Environment/SpaceshipCorridor/`.

## Assembly starts (owner: "Sí, adelante")
**The work is split by the team rules.**
- **Code (Muse Spark 1.3 Contributor via OpenCode, the default implementer):** the plan's code needs. Brief: `Prompts/site_code_needs.md`.
  - `AGBEraBlock`: a placed piece that exists in one era and/or depends on flags.
  - The seal (`AGBSeal`) and caption cutscenes.
  - `RequiredFlags` on encounters, plus a starter interactable for the Valkyrie fights.
  - The data: a gun-carrier orc, the Herald, two corridor skirmishes.
  - The final portal.
- **Map (the architect, as the owner asked):** the assembly in the Editor.

**The town is gone.**
- **The keep list:** the planner's `castle_keep_list.txt` (101 packages: the 31 castle meshes and their materials and textures).
- **Deleted:** with the Editor closed, the other 1,697 `CastleTown` files (7.2 GB) and the pack's seven orphan `__ExternalActors__` / `__ExternalObjects__` folders.
- **Where they went:** the Windows Recycle Bin, not a permanent delete.
- **Result:** `CastleTown` is now 651 MB.

**The map builder.** `Tools/Jam/Build-SiteMap.py` runs in the Editor's Python and reads `world_layout.json`. It builds `/Game/Glyphbound/Maps/Site` (persistent) and its sub-levels `Site_Past` / `Site_Future`.
- **Shell, ceilings and structures:** engine cubes with the engine's world-aligned grid material, and ramps for the stairs.
- **Era blocks:** door locks, pylons and the cryo tube, once `AGBEraBlock` exists.
- **Gameplay actors:** portals, encounters (spawn points, trigger boxes, required flags), checkpoints, the player start, the navmesh bounds and `KillZ`.
- **Lighting:** sun, sky light, atmosphere and fog per era.
- **Idempotent:** every generated actor is tagged `GBGen`, and a run replaces only those.

## The code needs: Muse stalled, Sonnet built them, Sol reviewed
- **Muse Spark 1.3 Contributor** (OpenCode, logged) read the code for a while and exited without writing anything.
- **Exception to the default implementer:** the team rule after a Muse stall sends the work to a **Claude Sonnet 5.5 subagent**. It used the same brief, plus one addition: placed pickups configurable in the Editor.
- **What it built:**
  - `AGBEraBlock`.
  - `AGBSeal`, with a small caption-cutscene subsystem (`Cutscenes.json`: `Seal`, and `FirstGun` triggered by the flag `GotFirstGun`).
  - `RequiredFlags` on encounters and `AGBEncounterStarter`.
  - `AGBFinalPortal`.
  - Pickups whose `Drop` and `FlagOnPickup` are editable, and a `FlagOnPickup` that drops carry.
  - The data: `Orc_Future_GunCarrier` (its pistol sets `GotFirstGun`), the `Herald`, `Skirmish_Breach`, `Skirmish_Signal` and `MiniBoss_Herald`.
  - The test `Glyphbound.World.RequiredFlags` and a smoke block `site`.
- **Checks:** 49 tests green, Editor and game builds, full smoke clean.
- **GPT Sol 6.1 review** (Codex CLI, medium, read-only, logged; prompt `Prompts/site_code_needs_review_sol.md`): 11 findings, all sent back to the Sonnet subagent.
  - **P1, final portal:** travelling to the Entity map would lose the inventory. The map does not exist yet, so the portal will only travel to a real package.
  - **P1, seal:** a refused travel spends the seal.
  - **P1, cutscene during a QTE:** it can lock weapon switching for good.
  - **P2, the cutscene:** captions queued behind a travel, death during a caption, a trigger caption replaying, a caption under an open menu.
  - **P2, the map path:** an invalid Entity map path.
  - **P2, the smoke:** it checks a navmesh flag instead of a real path, and uses fixed wall-clock timing.
  - **P3, the test:** a required-flags test that misses one wrong implementation.
- **Also asked of it:** a `sitemap` smoke block that runs on the real map.

## The dressing pass
`Tools/Jam/Dress-SiteMap.py` fits each kit mesh into the rectangle the plan gives its structure. It turns the mesh to the long side, tiles low walls and bookshelves, stacks the base's columns, and turns the castle stairs by their climbing direction. The greybox boxes underneath keep the collision and are hidden.

**What it places:**
- **The castle:** the 47 wall pieces at the planner's origins and turns, pillars with capitals, podiums, low walls, bookshelves, crenels and props.
- **The base:** HTB machine blocks, consoles, docks, the hangar gantry, stacked columns and lab capsules.
- **Boxes that stay as the look:** a new world-aligned master material, `M_GB_WorldAligned`, with eight instances, so a 48 m wall does not stretch one texture.
  - **The castle:** stone walls, wooden ceilings, stone floors.
  - **The base:** panel walls (`Scifi_Panels_10`), dark metal floor (`_09`), grate ceilings (`_07`), containers (`_12`) and emissive accents (`_11`).

**Self-review before showing the owner** (renders `media/2026-11-13/site_dressed_*.png`):
- **The castle's floor:** the 12 m floor tile the plan chose (`SM_CastleTower_Floor_A_05`) has round holes, because it is a tower floor. The castle floor is now a stone skin per room.
- **The shared boxes:** the galleries showed castle stone in the future too. Each era now gets its own skinned copy.
- **The base's floor:** the first panel was covered in "WARNING" signs, so three were compared on the yard and the darkest, calmest one won.
- **Not a bug:** the pylons of the past appear in the future's Editor renders because the Editor does not apply the eras; the game hides them.

**The game's map:** `GameDefaultMap` is now `/Game/Glyphbound/Maps/Site`, and it is cooked. `Smoke-Jam.ps1` got `-Map`, whose default is the engine's Entry map, so the arena smoke is unchanged.

## The fixes, the real-map smoke and the package
- **Sol's 11 findings are fixed** (Sonnet subagent, second pass):
  - **Final portal:** it opens only an existing map package, with no stale checkpoint; the snapshot handover is a TODO until the Entity map exists.
  - **Seal:** it retries a refused travel.
  - **The cutscene's locks:** separate cutscene locks for control, invulnerability and weapon switching, instead of saving and restoring a shared flag. A caption ends a running QTE and closes open menus.
  - **The cutscene's flow:** caption time is real time, a queued caption waits for the travel, death cancels without running the completion and frees the seal, and a trigger caption plays once.
  - **The smoke:** the arena block (`seal`) is a state machine with real navmesh path queries through a doorway the era block closes: 3,372 cm detour shut, 1,001 cm open.
  - **The test:** `RequiredFlags` tests each missing flag.
- **`sitemap` smoke on the real map:**
  - The sub-levels load, and the past shows at the start.
  - The player stands on the floor (2 cm gap).
  - The counts: 6 portals, 14 encounters, 1 seal, 2 starters, 1 final portal, 23 era blocks, 6 chests.
  - A nav path runs from the start into the courtyard, 3,300 cm.
  - After a no-teleport travel the future shows, the player is still on the floor, and there is no whole path from the yard to the checkpoint room while D1f is shut.
  - The architect fixed its screenshots, which faced east: north is +X in the site's frame. Screenshots: `media/2026-11-13/site_smoke_start_past.png` (the gothic doorway into the courtyard) and `_future.png` (the closed leaf of D1f).
- **Checks:** 49 tests green, Editor and game builds, full arena smoke clean.
- **Package:** `TeamTBD_Glyphbound.zip` from commit `b0102db`, **819.6 MB** (299.7 MB before: the kits' textures), BuildCookRun 105 s.
  - On the packaged game, the `sitemap` smoke is clean (11 s, the same counts and paths), and the full arena smoke is clean (147 s, 67 screenshots, no LogGB errors).
  - The game now starts on the site.
- **Next for size:** cap the kits' textures (the plan suggests 1024) before the corridor model adds more.

## The owner's first play: stuck at the start
The owner: *"No puedo pasar del inicio"* ("I can't get past the start"), with a screenshot of the open gothic doorway D1.

**The cause, measured in the Editor:** all 374 dressing meshes still had the `BlockAll` profile with full collision.
- `Dress-SiteMap.py` turned their collision off with `set_collision_enabled`, but a named collision profile re-applies its own settings when the map is loaded.
- So the kit door piece's single hull, which the planner had warned about, closed the doorway again.
- The navmesh smoke passed anyway, because the dressing meshes do not affect navigation.

**The fix:**
- The script now sets the `NoCollision` profile, and marks actors and components modified, so the change is saved.
- The 374 meshes were fixed in place, and a reload confirms `NoCollision` on all of them.

**A check that would have caught it:** the `sitemap` smoke now sweeps the player's own capsule (pawn channel) from the start through D1 into the courtyard, and fails naming the blocking actor.
- **Seen failing:** in the Editor with the door piece's collision put back, the same capsule trace stops at `Wall_04_D1`.
- **Seen passing** with the fix.

**Repackaged:** `TeamTBD_Glyphbound.zip`, 819.6 MB, BuildCookRun 63 s.
- The packaged `sitemap` smoke is clean, including the walk.
- The full arena smoke is clean (147 s).
- Sent to the owner again.

## Not done in this block
- Nothing from the packs is placed in a game level yet.
- High Tech Base is not migrated and the corridor is not imported. Per the team rule, renders go to the owner first.
- The orphan external actors are not deleted.
- Not packaged. Only the `.uproject` changed, and the game target still builds with the three plugins on (`Build-Jam.ps1 -Target Glyphbound`: Succeeded, 13 s).
