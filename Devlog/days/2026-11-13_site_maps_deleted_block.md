# Block: the owner rejects the first maps, and they are deleted (Nov 13, CENTRAL)

> Written by the architect chat of this block (Claude Code, Claude Fable 5.2). To be merged into the day entry by whoever closes the day. Times in UTC.

## The owner's request
Original prompts (Spanish), two messages:

> *"se hizo un prototipo de mapa para ambos mundos. Ninguno sirve en nada, elimina ambos. Se dieron assets que se usaron indiscriminadamente y al azar, no con sentido ni buscando una coherencia en el mundo. Vamos por pasos. Primero, dime qué entiendes del plan con respecto a los mapas."*

> *"absolutamente nada de los últimos 2 mapas hechos sirve para nada. Son completamente inútiles y un trabajo pésimo"*

Translation: "A map prototype was made for both worlds. Neither is of any use, delete both. Assets were given and they were used indiscriminately and at random, not with sense nor looking for coherence in the world. Let's go step by step. First, tell me what you understand of the plan regarding the maps." Then: "Absolutely nothing of the last two maps is of any use. They are completely useless and terrible work."

The second message answered the architect's one question: whether the plan and the greybox went too, or only the dressing. Everything goes.

## What went wrong (the architect's reading)
The maps were built in three steps earlier today (block `2026-11-13_fab_assets_block.md`): a planner drew the site, a script built a greybox from the plan's numbers, and a second script dressed it with the kits.

- **The plan was a gameplay drawing, not a place.** It fixed room sizes, doors, cover, spawn points and jump reach. For looks it gave a parts list per room ("2 kegs, 4 podiums, 8 torches") and boxes with a kit material.
- **The dressing was mechanical.** `Dress-SiteMap.py` fitted each kit mesh into the rectangle the plan gave it: 374 meshes placed by a rule, none placed because the room needed it.
- **Nobody asked what each room is.** No step described a space as a place in its world before filling it. The self-review looked for technical faults (gaps, z-fighting, collision) and missed that the result had no coherence.

## What was deleted
| What | Where | In git |
|---|---|---|
| The three maps | `Glyphbound/Content/Glyphbound/Maps/Site.umap`, `Site_Past.umap`, `Site_Future.umap` | never (Content is outside git) |
| The map builder and the dressing pass | `Tools/Jam/Build-SiteMap.py`, `Tools/Jam/Dress-SiteMap.py` | history |
| The capture script for the site views | `Tools/Jam/capture_site_views.py` | history |
| The world plan | `Design/Levels/WORLD_PLAN.md`, `world_layout.json`, `make_plans.js`, four SVG drawings | history, from `ac54fbc` |
| The `sitemap` smoke block (337 lines) | `Core/GBDevCommands.cpp` | history |

**Changed back:** `GameDefaultMap` is the engine's Entry map again, where the game builds the test arena in code, and the site is no longer in `MapsToCook`.

## What was kept, and why
- **The code written for the site** (`AGBEraBlock`, `AGBSeal`, `UGBCutsceneSubsystem`, `AGBFinalPortal`, `RequiredFlags`, the new encounters and `Cutscenes.json`). It is game logic that any map of this game needs, and the `seal` smoke block tests it in the arena.
- **The kits.** The owner's complaint is how they were used, not the assets.
- **`Design/Levels/castle_keep_list.txt`.** It records which 101 castle packages were kept when the town was removed; that already happened.
- **The dev log of the attempt:** the earlier block, its screenshots (`site_greybox_*`, `site_dressed_*`, `worldplan_*` in `Devlog/media/2026-11-13/`) and the briefs in `Prompts/`. They are the record of a rejected attempt and stay as they are.

## Decisions
- **GDD v0.47, §4 "The site".** The owner's direction stands (castle in the past, military base on the same site in the future). The deleted plan's layout, route, zone names, lock rules and module lists do not, and are not a starting point.
- **A rule for the next attempt (architect, provisional):** a space is described as a place first (what it is, who built it, what it is for, what it looks like), and an asset goes in only because that description asks for it. The next map is planned step by step with the owner.
- **Exception to the implementer rule:** the architect made this change directly. It is deletions, two config lines and comments; there was nothing to implement.

## Checks
Run once, under the machine lock, with the filtered smoke only (the full smoke and the package wait for the end of the day):

- Editor target: `Result: Succeeded`, 63 s.
- Game target (`-Target Glyphbound`): `Result: Succeeded`, 55 s.
- `Smoke-Jam.ps1 -Only seal -Offscreen`, on the Entry map that is the default again: `SMOKE seal block passed`, `LogGB errors=0`, 42 s. The run now has 2 steps; the `sitemap` step is gone.
- The test suite was not run: no tested logic changed.

## Not done in this block
- **Not repackaged.** `Glyphbound/TeamTBD_Glyphbound.zip` on CENTRAL is still the 819.6 MB package that starts on the rejected map. The next package starts on the test arena.
- No new map and no new plan: that is the next step with the owner.
