# Block: the future base, slice 1 (Nov 13, CENTRAL)

> Written by the architect chat of this block (Claude Code, Claude Fable 5.2). To be merged into the day entry by whoever closes the day. Times in UTC.

## The owner's request
Original prompt (Spanish, shortened to its instructions):

> *"vamos primero con el plan del mundo cyberpunk futurista. La idea es que sea como una especie de base militar, en los cuales hay más que nada pasillos, pero esos pasillos dan a habitaciones pequeñas y luego a salas grandes. Puedes usar ejemplos, juegos como Halo o juegos como Cyberpunk 2077. [...] Tiene que tener una coherencia. [...] Las texturas de suelo tienen que ir en el suelo con una coherencia. No puede ser todo solo plano. Tiene que haber relieves, tiene que haber subidas, bajadas, tiene que haber terrenos que sean rejillas, terrenos que sean solo metal. [...] no sirve una textura que sea un cuadrado que se repite 80 veces [...] las zonas donde se hacen los combates tiene que tener escaleras, desniveles. Empecemos haciendo la sala donde el jugador llega después de entrar al portal. Un pasillo con un par de habitaciones. Y una sala. Para combatir. Solamente eso. Repito, busca referencias."*

Translation: plan the futuristic world first. A kind of military base: mostly corridors that lead to small rooms and then to large halls; use Halo and Cyberpunk 2077 as examples. It must be coherent: floor textures on the floor with a logic, not everything flat, ways up and down, some floors grating and some plain metal, no texture repeated 80 times, and stairs and level changes where fights happen. Start with the room where the player arrives through the portal, a corridor with a couple of rooms, and a hall to fight in. Only that. Look for references.

While the block ran he sent 26 reference screenshots (Cyberpunk 2077, Halo Infinite, Halo CE Anniversary, Halo Reach) and three packs from his Fab library, and added: *"la propia estructura de los assets de mapa ya hechos te sirve de referencia. Y mira las texturas, las formas y dibujos"* (the structure of the kits' own demo maps is your reference; look at the textures, their shapes and drawings) and *"lo importante es el diseño de niveles"* (what matters is the level design).

## References
- **The owner's images.** What was read from them is in `Design/Levels/FUTURE_BASE_SLICE1.md`, section 2: the stair as a combat space, platforms at several heights, big ribs that set the rhythm, floors drawn as lane, strips and trim, the ramp that rises to a door, the corridor that bends before the hall, the hall with one hero object in its centre seen through an opening before entering.
- **A search of developer texts** (a Claude subagent, read-only, 56 tool calls). It found no published corridor or arena sizes for either series and no developer text on floor material logic; those parts of the plan are our own reasoning. What it did find, with sources:
  - Halo CE's own metrics (player 2.13 m, run 6.9 m/s, smallest path 1.2 m): https://nikon.bungie.org/misc/hek_tutorial/references/general/halo_player_stats.html
  - Hallway, door and stair metrics, cover heights, arena footholds, the three-floor-planes rule: https://book.leveldesignbook.com/process/blockout/metrics and its combat and layout chapters
  - Halo 5 "bowl" arenas: https://gamedeveloper.com/design/inside-the-campaign-design-of-i-halo-5-guardians-i-
  - "30 seconds of fun": https://www.engadget.com/2011-07-14-half-minute-halo-an-interview-with-jaime-griesemer.html
  - Kit footprints and kit mistakes (Skyrim): https://gamedeveloper.com/design/skyrim-s-modular-approach-to-level-design
  - Cyberpunk 2077 rooms, lighting and readability: https://80.lv/articles/creating-lighting-and-environments-for-cyberpunk-2077/ and https://80.lv/articles/interview-how-cyberpunk-2077-s-night-city-was-built-almost-entirely-by-hand/

## Looking at the kits before using them
The first maps failed because pieces were placed by name and size. This time every piece was looked at first.

- **Tools written for it** (all in `Tools/Jam/`):
  - `mcp_call.js` calls the Editor's MCP server over HTTP from a script. The chat's own MCP connection was down because the Editor was opened after the chat started.
  - `capture_views.py` renders views of the open level: offscreen (any buffer, plans by orthographic cut) or through the real viewport.
  - `Make-ContactSheet.ps1` and `Make-Sheet.ps1` tile pictures for review.
  - `extract_level_pieces.py` writes every mesh and light of a level to JSON.
- **What was looked at:** a picture of each of the 66 High Tech Base meshes, 85 lab meshes and 147 Modular SciFi Station meshes; every colour and relief texture of High Tech Base (166); the materials on spheres; the demo maps of High Tech Base and Modular SciFi Station from inside, in plan and piece by piece (which mesh, where, turned how, with which material).

## "Why is everything white?" (the owner, looking at the Editor)
The whole High Tech Base kit rendered one flat grey-white.

- **Cause:** the pack's materials are authored for Substrate, and our project runs with Substrate off (`r.Substrate=0`, read live). Colour and roughness only reached the Substrate Slab node.
- **What did not work:** wiring the classic Base Color and Roughness pins on the pack's three master materials, disconnecting the slab, setting the shading model to Default Lit, turning its parallax switch off. The Base Color buffer kept showing the same grey on every surface. The same textures on a brand-new plain material rendered correctly, so the textures were fine and the pack's masters were not usable.
- **The fix** (`Tools/Jam/fix_htb_materials.py`): two plain masters of our own, `M_GBKit_Surface` and `M_GBKit_Paint`, with the same parameter names as the pack's, and the pack's 85 material instances re-parented to them. Each instance keeps its own textures and numbers.
- **A crash on the way:** re-parenting while the pack's demo level was on screen crashed the Editor (render-thread assert, `UniformExpressionCache should be up to date`, on an instance of an instance). The script now runs on the empty start-up map, refreshes every instance and only then saves. The crashed Editor's log was collected by hand with `Collect-UnrealAgentLog.ps1`, because `Stop-EditorMcp.ps1` cannot find a process that is already gone.
- **Result:** the kit shows its real colours: dark plate, dark red columns, amber lit grates, hazard stripes, cyan screens. Before and after: `media/2026-11-13/htb_materials_before_grey.png`, `htb_materials_after_corridor.png`, `htb_materials_after_hangar.png`, `htb_materials_after_lit.png`.

## A pack added by the owner
| Asset | Source | License | In the project |
|---|---|---|---|
| Modular SciFi Station (Martin Milz) | Fab, https://www.fab.com/listings/0667e321-31a7-40ed-b85c-6db5bbc4366b | Professional, from the owner's Fab library | `Glyphbound/Content/ModularSciFiStation`, 323 assets, installed through the Editor's Fab toolset |

The owner offered two more from his library, not installed yet: Deep Water Station (Kyrylo Sibiriakov, https://www.fab.com/listings/ec2385d7-3e02-494b-a243-e18cac7f4a69) and Modular Sci-Fi Treatment Station Environment (Tris Games, https://www.fab.com/listings/d8ac4186-78f7-43c7-b21c-04b4dc0901a2).

## The plan
`Design/Levels/FUTURE_BASE_SLICE1.md`. It describes places before pieces: Blackwall Station, the Anomaly wing, built to contain and watch the seal. Section 6 says what version 1 really builds and where it differs.

## How version 1 is built
`Tools/Jam/Build-FutureBase.py` builds `/Game/Glyphbound/Maps/FutureBase`.

- **Sections from the kit's own demo.** The control room, two corridor bays and the hangar shell of the High Tech Base demo are placed again exactly as their author assembled them (409 pieces and lights, from `Design/Levels/kits/htb_demo.json`), with the demo's exposure. This is the owner's "the structure of the demo maps is your reference", taken literally for the shells.
- **Our level design, piece by piece**, inside those shells:
  - **Arrival:** the seal on a low round plinth in the middle of the watch consoles, four emitters around it, a cable tray from each emitter to the plinth. The player starts in front of the seal, facing the corridor.
  - **Transfer bay:** a catwalk 3 m up along the north wall that crosses the mouth of the branch as a bridge; a stair at each end; a cross bridge over the lane to a lookout above the service trench, open on the lane side so a drop closes the loop; ten crates in four groups where a loader would leave them; two machine blocks that break the 32 m lane; wall lamps over the catwalk, each with its fixture.
- **Collision:** kit pieces block exactly as they look (complex collision used as simple). The kits' simple hulls close doorways, which is what trapped the player on the deleted map.

## Self-review (before the owner saw anything)
Rendered from the player's eye and corrected:
- The catwalk steel came out white (the pack is a white Mars station) against the dark base. Fixed with four darker instances of the pack's own steel materials.
- The seal's light cone wrapped the camera at the start. The player now starts in front of the seal.
- One render had the camera inside a machine block; the spot was moved.

## Decisions (architect)
- **Exception to the implementer rule:** this block was done by the architect directly. It is level assembly, material repair and one small smoke block; it needed looking at renders at every step, which an implementer brief cannot carry.
- **A new smoke block, `gb.Smoke tour`.** A map marks review spots with actors tagged `GBTour`; the block puts the player on each one, checks the floor and takes a screenshot through the real game camera. It replaces the deleted `sitemap` block with one that knows nothing about any particular map.
- **GDD:** no version change; §4 "The site" already says there is no map yet. It will be updated when the owner accepts a version.

## The two side rooms (after the owner's "Sí, sigue")
The owner saw version 1 and answered *"Sí, sigue"* (yes, go on) to "do I keep this approach and build the corridor's two rooms?".

- **Source:** the Modular SciFi Station example map, written to `Design/Levels/kits/mss_demo.json` (1,694 pieces). Its small stair module (11 x 9 m, two floors) was read piece by piece: lower door, four stair modules, the gallery ring 4 m up, the roof assembly.
- **Air plant** (behind the right alcove's door, reached over the kit's bridge across the service pit): the module placed whole, as its author built it. The demo's upper door led to an outside tunnel; here that wall is closed with the kit's plain wall piece.
- **Store room** (behind a new door in the first bay's left wall): the module's lower story only, closed by its own roof assembly brought down 4 m. The door is the same frame the High Tech Base demo uses in the right alcove, put in place of one wall panel (same 2 x 3 m module). What the demo kept behind that wall was removed to make room.
- **Furnishing of the store:** seven crates stacked against the back wall, three glass-door cabinets from the lab kit (dark skin), a desk by the door, barrels and two drums.
- **Lamps:** the demo's lamp Blueprints are replaced by the same lamp mesh with a light of our own, in the units and levels of the rest of the map (the pack's lamps are 5 to 35 lumens and rely on its own exposure).

**Self-review, in the game** (`gb.Smoke tour`, now 17 spots):
- First pass: both rooms were white and far brighter than the corridor (the pack's walls are white panels); the gallery ended in a door onto black nothing; the store was nearly empty.
- Fixes: every station material except glass, lamps and signs now uses a darker instance of itself; room lamps cut to a third; the blind door closed; the store furnished.
- Second pass: `SMOKE tour block passed: 17 spots, 0 without floor`, no errors, 76 s. Screenshots: `media/2026-11-13/futurebase_v2_rooms.png`.
- Also fixed: the bay's stairs sat 20 cm high against the kit's own convention (a floor tile's surface is 20 cm above its origin, a stair module sits at origin height).

## Not done in this block
- The containment pit, the lock, the corridor bend, the bay's entry landing and a walkable pit (plan sections 3.1 to 3.3 and 3.6).
- Enemies, the portal back, pickups and encounters on this map.
- The game's default map is still the test arena, and nothing was packaged.

## Checks
- **In the real game, on the new map:** `Smoke-Jam.ps1 -Map /Game/Glyphbound/Maps/FutureBase -Only tour -Offscreen`: `SMOKE tour block passed: 12 spots, 0 without floor`, no LogGB errors, no engine errors, 59 s. The player stood on the marked floor at every spot, including the catwalk, the bridge and the lookout (within 2 cm).
- **Screenshots through the game camera:** `media/2026-11-13/futurebase_v1_tour_a.png` and `futurebase_v1_tour_b.png` (all twelve spots), and five single views (`futurebase_v1_02_arrival_back.png`, `_03_corridor.png`, `_05_enter_bay.png`, `_09_lookout.png`, `_11_branch.png`). Sent to the owner.
- **Builds:** Editor target `Succeeded` (18 s); game target `Succeeded` (24 s).
- **Full arena smoke, offscreen, once at the end:** `done=True crashed=False LogGB errors=0`, 156 s. The tour block logs `skipped: this map has no tour spots` on the arena, as intended.
- **Not checked:** nobody has walked the map by hand yet. The tour teleports; it does not prove that every stair can be climbed on foot or that enemies find paths (no enemies are placed). The test suite was not run: no tested logic changed.

## The package (owner: "Sí, empaquétalo")
The owner asked for a package that starts on the new map, so he can walk it.

- **Start map:** `GameDefaultMap` is `/Game/Glyphbound/Maps/FutureBase`, and the map is in `MapsToCook`. The Editor and the smoke runs still start on the Entry map (the test arena). GDD v0.48, §4.
- **Package:** `Glyphbound/TeamTBD_Glyphbound.zip`, **1,232.7 MB**. BuildCookRun `BUILD SUCCESSFUL` (150 s the first time, 41 s the second).
- **Over the jam's 1 GB.** `Package-Jam.ps1` reports it as a failure. Where the size is (cooked): High Tech Base textures 1,655 MB, Modular SciFi Station textures 559 MB, Scifi Material Pack 278 MB; the kits' textures cook at 4096 px.
- **The cap that did not happen.** `Tools/Jam/cap_kit_textures.py` caps the kits' textures at 2048 px. Its first run capped nothing: the Editor reports 2048 for textures that cook at 4096, and the script trusted that number. The size test was removed and a second run started. The owner then said the limit does not matter for now (*"El limite de 1GB no importa por ahora"*), so that run was stopped part-way and the Editor closed. **Some kit textures are now capped at 2048 and some are not**; nothing was repackaged after that. When the limit matters, run the script to the end and package again.
- **Checks on the package that is on disk** (built before any cap):
  - `Smoke-Jam.ps1 -Packaged -Map /Game/Glyphbound/Maps/FutureBase -Only tour -Offscreen`: `SMOKE tour block passed: 17 spots, 0 without floor`, no errors, 66 s.
  - `Smoke-Jam.ps1 -Packaged -Offscreen` (full arena smoke): `done=True crashed=False LogGB errors=0`, 148 s.
- **Still true:** nobody has walked the map by hand; the tour teleports.

## The owner's first walk: five notes
The owner played the package and sent five screenshots with five notes (Spanish):

> *"1. En el pasillo, primera sala a la izquierda hay un hueco, revisa todas las conexiones a pasillos que hay huecos. 2. Quita lo de la derecha que bloquea las escaleras. 3. Coloca una puerta hacia una habitacion mediana donde haya algo de combate. 4. Coloca una puerta funcional y que siga el camino. 5. Haz estas estructuras escalables por si cae un jugador al comienzo y no tiene doble salto aun."*

Translation: (1) there is a hole at the first room on the left of the corridor; check every connection to the corridors for holes. (2) Remove the thing on the right that blocks the stairs. (3) Put a door (in the air plant's gallery wall) to a medium room with some fighting. (4) Make this (the freight door) a working door, and let the way go on. (5) Make these structures (the alcove's service pit) climbable, in case a player falls in early and has no double jump yet.

| Note | What was wrong | What was done |
|---|---|---|
| 1 | The kit's corridor floor stops at its wall line and each side room's floor starts behind its own wall: 2 m of nothing in the store room's doorway | A steel threshold plate in both side doorways and at the lab's gate. A new check in the builder (`check_walks`) traces the floor every 20 cm along seven walks through every connection and reports holes; it now reports none (one sample falls on the 1 cm seam between two floor pieces of the bay) |
| 2 | The bay's west stair began right behind the wing of the corridor's last frame. The wing is part of the frame's mesh, so it cannot be taken away without removing the frame | The stair now starts 2.5 m inside the bay, clear of the frame, and the catwalk starts where the stair arrives |
| 3 | The gallery ended in a wall | The gallery's far door is open again and leads to the **specimen lab**: the lab kit's own room (dark skin, 29 x 22 m, the kit's alien specimens in stasis capsules), placed whole as its author built it, 4 m above the corridor. The encounter `Opening_Future` starts when the player is well inside |
| 4 | The freight door was a wall piece | New class `AGBDoor`: a leaf that slides open while the player is near and shuts behind them (optional progress flag to keep it locked). The freight door lifts. Beyond it: one more bay of the kit's corridor, to a second freight door that stays shut, where the next slice will continue |
| 5 | The alcove's service pit is 2.9 m deep and fenced: no way out without the double jump | Crate steps on both sides of the bridge, under the gaps the railing leaves at the corridor's edge (1.05 m, 1.05 m, then 0.8 m: single jumps). A crate step at each end of the bay's trench too |

**Also needed for note 3:** the map now starts in the future (`UGBEraSubsystem` reads the tag `GBStartFuture` on the player start), so the future's enemies exist on it.

**Code (architect, directly; small and tied to the map work):** `World/GBDoor.h/.cpp`, the era start in `World/GBEraSubsystem.cpp`, test `Glyphbound.World.DoorLeaf` (the leaf is shut at 0, open at 1, never past either end, eased at both ends).

**Self-review in the game** (`gb.Smoke tour`, 21 spots): passed, no errors. Screenshots: `media/2026-11-13/futurebase_v3_notes.png`. Seen in them: the freight door open with the corridor behind it; the threshold in the store doorway; the lab through the gallery's door; the fight starting in the lab (the player's health drops during the tour).

**Known and left:**
- The lab is far brighter than the rest of the base. It is the kit author's own lighting and it reads as a clean lab; say if it should come down.
- The gallery's door to the lab is 1.3 m wide: large enemies cannot follow the player out of the lab through it.
- The two small doors of the corridor still have no leaf.

**Checks for this round (once, at the end):**
- Editor target `Succeeded` (52 s); game target `Succeeded` (37 s).
- Whole test suite: `50 run, 0 failed` (1.10 s of tests), including the new `Glyphbound.World.DoorLeaf`.
- Full arena smoke, offscreen: `done=True crashed=False LogGB errors=0`, 156 s.
- Package: `BUILD SUCCESSFUL`; `TeamTBD_Glyphbound.zip` is **1,445.4 MB** (the lab kit's room is now in the cook; still over the jam's 1 GB, which the owner has set aside for now).
- On the package: tour `passed: 21 spots, 0 without floor` (80 s); full arena smoke `LogGB errors=0` (148 s).
- **Not checked by a person yet:** climbing out of the pits on the crate steps, walking through the doorways, the fight in the lab, the freight door seen from both sides.

## The owner's second walk: four notes
Original (Spanish), with five screenshots:

> *"1. La sala esta bien, pero no uses esos assets porque son los que se usaran para la pelea con la Valkiria. 2. A esta escalera haz lo mismo que la otra. 3. Sigue un pasillo largo, con cables en los suelos y tuberias en el techo. Ya hay un asset asi. 4. Hay un glitch antes de eso, ya no deja salir."* and, with a fifth screenshot, *"Ahi es el glitch"*.

Translation: (1) the room is fine, but do not use those assets: they are the ones for the Valkyrie's fight. (2) Do to this stair what you did to the other. (3) A long corridor follows, with cables on the floors and pipes on the ceiling; there is already an asset like that. (4) There is a glitch before that; it no longer lets you out. "There is the glitch."

| Note | What was wrong | What was done |
|---|---|---|
| 1 | The combat room was the lab kit's own room | **Muster hall** in the same place and size, from the base's kit: the High Tech Base hangar branch placed again as its author built it, plus our end wall around the gallery's door, a loading dock 1 m up with its stair, nine crates and a machine block. Same encounter (`Opening_Future`). No lab-kit piece is left on the map: the store's cabinets, desk and barrels and the projector at the seal went too (the seal now sits in the station kit's geodesic sphere) |
| 2 | The east stair's foot was 1 m from the end wall, in the dark, with a machine block in front | The stair ends 2.5 m short of the wall, the block moved to the corner by the trench, a wall lamp over the stair's foot |
| 3 | The way stopped at the second freight door | That door works too, and behind it runs the **long service corridor**: 36 m, 4 m wide and high, from the station kit: a covered cable tray and two loose cables on the floor, an air hose along the wall, a bundle of pipes and a truss under the ceiling, two grated bays, staggered wall lamps, a personnel door at the far end, shut for now |
| 4 | **The architect's mistake.** The corridor bay behind the first freight door was taken from the demo by a range of positions, and the control room's big pieces (its wall mesh, floor, roof, decals) have their pivots just inside that range. They came along, cut across the branch and the door line, invisible from one side and solid from both | Those pieces are filtered out by their bounds. The door line then had no floor (the control room's floor had been covering it), which the builder's walk check reported; a threshold strip lies under each freight door now |

**On note 3, "there is already an asset like that".** Two candidates: the station kit's own interiors (cable trays, pipe bundles, hoses), and the Spaceship Corridor Damaged model in `SourceArt` (Blender 4.3 only, no UVs, procedural textures). The second cannot be brought in on this machine: Blender is not installed on CENTRAL. The corridor is built from the station kit, which was looked at from inside first. If the owner meant the Blender model, it has to be converted on a machine with Blender.

**Self-review in the game** (`gb.Smoke tour`, 27 spots, passed):
- The long corridor's plain wall panels were large flat white sheets that glared under the lamps: only the riveted panels are used now, and the lamps are dimmer.
- A machine block right inside the muster hall's door read as a black wall: moved deeper into the hall.
- The player died in the muster hall during the tour (the encounter works; a teleported player does not fight back), which put the death screen in one screenshot. The hall's spots are last in the tour now.
- Screenshots of this round: `media/2026-11-13/futurebase_v4_round2.png`.

**Checks for the second round:**
- No code changed since the first round's checks (50 tests, both builds, full arena smoke): only the map and its builder.
- Builder walk check: no holes on nine walks through every connection (one sample on the 1 cm seam between two floor pieces of the bay).
- Package: `BUILD SUCCESSFUL`; `TeamTBD_Glyphbound.zip` is **1,206.2 MB** (the lab kit is out of the cook).
- On the package: tour `passed: 27 spots, 0 without floor` (102 s); full arena smoke `LogGB errors=0` (149 s). Screenshots taken on the package: `media/2026-11-13/futurebase_v4_packaged.png`.
- **Not checked by a person yet:** this round's changes on foot.
