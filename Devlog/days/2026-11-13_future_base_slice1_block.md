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

## Not done in this block
- The two side rooms off the corridor (the plan's store room and a second room). The corridor door leads nowhere yet.
- The containment pit, the lock, the corridor bend, the bay's entry landing and a walkable pit (plan sections 3.1 to 3.3 and 3.6).
- Enemies, the portal back, pickups and encounters on this map.
- The game's default map is still the test arena, and nothing was packaged.

## Checks
- **In the real game, on the new map:** `Smoke-Jam.ps1 -Map /Game/Glyphbound/Maps/FutureBase -Only tour -Offscreen`: `SMOKE tour block passed: 12 spots, 0 without floor`, no LogGB errors, no engine errors, 59 s. The player stood on the marked floor at every spot, including the catwalk, the bridge and the lookout (within 2 cm).
- **Screenshots through the game camera:** `media/2026-11-13/futurebase_v1_tour_a.png` and `futurebase_v1_tour_b.png` (all twelve spots), and five single views (`futurebase_v1_02_arrival_back.png`, `_03_corridor.png`, `_05_enter_bay.png`, `_09_lookout.png`, `_11_branch.png`). Sent to the owner.
- **Builds:** Editor target `Succeeded` (18 s); game target `Succeeded` (24 s).
- **Full arena smoke, offscreen, once at the end:** `done=True crashed=False LogGB errors=0`, 156 s. The tour block logs `skipped: this map has no tour spots` on the arena, as intended.
- **Not checked:** nobody has walked the map by hand yet. The tour teleports; it does not prove that every stair can be climbed on foot or that enemies find paths (no enemies are placed). The test suite was not run: no tested logic changed.
