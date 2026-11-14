# Handoff: rebuild the castle as real pixel art, and make it light

For: a new Claude Code chat on CENTRAL, model **Opus** (the owner's choice for this task, 2026-11-14; it is the
exception to the "Muse implements" rule, note it in the day's entry). You become the architect of the castle map.
Written by the chat that built the map and ran the pixel test. Everything below was true at commit `cb53e89`.

## 1. What the owner asked

After seeing the pixel-art test of the castle's first zone (two variants side by side), the owner wrote (Spanish):

> "La cosa tambien es que el beneficio del pixel art es que sea ligero para el almacenamiento, ram, gpu y todo. Ten eso en cuenta. Pero sí, hay que reemplazar todo. Dame el handoff para hacerlo con Opus en otro chat"

Translation: the point of pixel art is also that it is light on storage, RAM, GPU and everything; keep that in mind.
But yes, everything has to be replaced. Give me the handoff to do it with Opus in another chat.

So two goals, and the second is not decoration:

1. **Everything in the castle map becomes real pixel art**: flat or low-poly geometry that carries drawn,
   nearest-filtered tiles. Not the castle pack with a different texture on it.
2. **The castle becomes light**: no Nanite sculpts, no 4096 px pictures, few triangles, few materials, and in the end
   **nothing of `/Game/CastleTown` in the package**.

The deadline of the jam is **15 Nov 20:59 EST**. A playable, packaged castle must exist at every moment until then.

## 2. Read first (in this order)

- `CLAUDE.md` (loaded for you) and the memory index: the machine lock, the Editor scripts, no PIE, the devlog in
  English, commit and push every change, replies to the owner in Spanish.
- `Design/GDD.md` section 3 "The pixel-art look" (the rule: 3D geometry with low-resolution pixel textures, nearest
  filter; structures are never flat sprites) and the changelog lines v0.59 to v0.67.
- `Design/VisualStyleGuide.md` (ramps, outline, light from the top left).
- `Design/Levels/PAST_CASTLE_PLAN.md`, sections 5, 6 and 10: what the castle is, every height, what is built.
- `Devlog/days/2026-11-14_castle_plan_block.md`, fourth and fifth parts: the owner's notes on his first walk, and
  the pixel test with its three faults and the reading of its pictures.
- Pictures: `Devlog/media/2026-11-14/castle_pixel_*.png` (the test), `castle_plan_v1.png`, `castle_notes_*.png`.
- Memory notes that bite here: `editor-mcp-from-script`, `maps-place-before-assets`, `ask-before-importing-art`,
  `self-review-art-before-delivering`, `solve-dont-park-problems`, `kit-own-piece-before-improvising`,
  `machine-lock-shared`.

## 3. Where things stand

- Map `/Game/Glyphbound/Maps/PastCastle`, the game's start map. It is **not in git** (`Content` is ignored): it is
  whatever `Tools/Jam/Build-PastCastle.py` last built. Run it with `node Tools/Jam/mcp_call.js py <script>` against
  an Editor opened with `Tools/Jam/Start-EditorMcp.ps1`; stages are chosen in `Glyphbound/Saved/castle_build.json`
  (`{"stages": [...], "no_walks": true}`); the result is written to `Glyphbound/Saved/castle_build_result.txt`.
- The builder places the pack author's own sections piece by piece from records of his levels
  (`Design/Levels/kits/castle/*.json`) and adds our level design: wards, walls, the crypt, the fight, 42 tour spots.
  **About 6,000 placed pieces, 141 different meshes of the pack.** `Design/Levels/kits/castle/pieces.json` has all
  336 meshes of the kit measured: bounds, triangles, materials (322 of them are Nanite; 8,800 triangles on average,
  a plain 4 m wall panel has 50,000).
- Three checks guard the map and must keep passing: `walks` (12 routes, a capsule of the player's size swept along
  each), `windows` (no window can be passed) and, in the game, `Smoke-Jam.ps1 -Map /Game/Glyphbound/Maps/PastCastle
  -Only tour -Offscreen` (42 spots; from **PowerShell**, Git Bash rewrites `/Game/...`).
- The pixel test is still applied to the first zone (gatehouse and lower ward): variant A left of the road, B right
  of it. Building the map again removes it. Its tools: `Tools/Jam/make_castle_tiles.js` (eight 32 x 32 tiles from
  seeded rules, into `Glyphbound/SourceArt/PixelArt/CastleTiles/`) and `Tools/Jam/castle_pixel_test.py`.
- The Editor is closed and `Glyphbound/Saved/machine.lock` is free. Other chats work on this machine: other
  people's C++ is uncommitted in the working tree (HUD, gun, player). Commit only your own paths
  (`git commit --only ... -- <paths>`).
- Last package: `BUILD SUCCESSFUL`, zip **2,628.8 MB**; the jam's limit is **1 GB**.

## 4. What the test already settled (do not run it again)

- **Variant B is dead** (the pack's own pictures cut to 64 px on the pack's meshes). It reads as the same sculpted
  castle with a blocky texture. Delete its leftovers: `/Game/Glyphbound/Materials/CastlePixel/M_GBPixelUV`,
  `MI_PixUV_*`, `T_px64_*`. That folder is in `DirectoriesToAlwaysCook`, so whatever stays there ships.
- **A material cannot do this.** The pack's relief is in its geometry (Nanite), and the sun lights the geometry.
  Drawn bricks over modelled stones are two patterns that disagree.
- **What worked, and the owner agreed:** hide the sculpted piece and put a flat one with a drawn tile in its place
  (the lower ward's cobbles on A's side). Flat, drawn, crisp at every distance, at home under the pixel HUD.
- `M_PixelSurface` (made by `Tools/Jam/make_materials.py`; parameters Texture, TileSize, Tint, Glow) lays a tile on
  the world, one tile per TileSize cm, the plane picked by the face normal's dominant axis. No UVs are needed.
  **Not yet checked:** faces at 45 degrees to the world (the octagonal towers' walls). The tile will stretch by 1.4
  there and may switch plane along the face. Look at one before you count on it; a tile laid in the piece's own
  space, or real UVs at a fixed texel size, is the way out if it looks wrong.

## 5. Done means

1. **No reference from the map to `/Game/CastleTown`**: not a mesh, not a material, not a texture, props and
   torches included. Proof: after a package, `Glyphbound/Saved/Cooked/Windows/Glyphbound/Content/CastleTown` does
   not exist (it is 1,172 MB of 6,491 MB cooked today).
2. **The same castle.** Every floor height, door, stair, opening and wall stays where the plan and the builder put
   it; the owner's nine notes stay fixed. `walks` clear on 12, `windows` open 0, tour 42 spots, on the Editor build
   and on the package.
3. **Light, with numbers.** Measure before and after, on the same spots (ward, Main Hall, crypt): triangles drawn,
   draw calls, frame time, texture memory (`stat unit`, `stat rhi`, `stat streaming` through the tour, or what you
   find better), plus cooked size and zip size. Targets to hold yourself to (provisional, the architect's): a piece
   in the hundreds of triangles, not thousands; no Nanite needed; every castle texture 64 px or less; a handful of
   materials (instances of one master); simple box collision instead of "complex as simple".
4. **It looks drawn, not cheap.** The owner rejects what looks cheap and expects you to criticise your own
   pictures before he sees them. Silhouette matters: battlements, arches, window openings, tower shapes, stair
   steps and vault curves are geometry and must stay; surface lumps are what goes.
5. Devlog (English, with the owner's words and their translation, the numbers before and after, the tools used),
   GDD and plan section 10 updated, commits pushed, a package on disk, the lock released.

## 6. How I would do it (the engineering is yours to change)

**Step 0, an hour at most: one zone, shown to the owner.** The first zone again (road, gatehouse, front wall,
lower ward with its camp, the retaining wall and the wide stair), all of it flat this time, both sides. Pictures
through the game's camera (the tour), your own critique, then the owner's OK **before** the keep. He decides the
look; do not convert 5,000 pieces on a look he has not seen.

**A twin for each pack mesh, same pivot, same size.** The builder's records then work unchanged: where it placed
`SM_X` it places `SM_Px_X` with the same transform, and every measured height stays true. 141 meshes are few
families: wall panel, wall with a window, wall with a door, floor tile, stair, parapet with teeth, tower wall
(straight, at 45 degrees), arch, column, vault, roof. Make each family from a few numbers (the piece's bounds from
`pieces.json`, its openings; the `windows` stage and the memory note on reading a shape from its vertices show how
to find them). Ways to make the meshes, cheapest first:

- From Python in the Editor (Geometry Script or a procedural mesh saved as a static mesh): no second tool, no
  import step.
- From a Blender script that writes FBX (precedent: `Tools/Jam/corridor_blender_export.py` and
  `import_corridor.py`; check where Blender is installed on this machine before you plan on it).
- An automatic simplification of the pack's mesh (planar or convex). Unknown quality on sculpted stone: give it
  twenty minutes on three pieces before you bet on it.

Where our own design placed things and not the author's (wards, curtain walls, retaining wall, crypt, rock), plain
boxes from the builder may be all that is needed, as the cobble floor was.

**Keep a way back until the owner accepts.** A setting in `castle_build.json` (`"look": "pack"` or `"pixel"`, your
naming) so that either castle can be built. The pack's look goes only when he says the pixel one replaces it; the
pack's folder is deleted from the project only on his word.

**Tiles.** Eight exist (wall, dressed stone, dark stone, cobble, wood, iron, earth, cloth). More will be needed
(door, roof, window glass or bars, banner, torch). They are drawn by `make_castle_tiles.js` from rules and the style
guide's ramps: created in the jam, no third-party asset, and say so in the entry. Tile size on the wall is a look
decision: show the owner two sizes in step 0.

**Props.** Kegs, crates, the cart, the fire, torches, cell bars. The GDD: structures are 3D with pixel textures;
camera-facing sprites are for characters, items and effects. Simple low-poly props with tiles follow the rule.
If you think a prop wants to be a sprite, that is a design question: one line to the owner, and keep working.

**Lighting.** The low sun and one warm light per torch are part of what reads well now; keep them. If you find
that the castle's cost is in the lighting and not in the meshes, report it with numbers before changing anything
global: the future base shares the project's render settings.

**Iterate cheaply** (CLAUDE.md): one stage of the builder at a time, `no_walks` while shaping, the tour with
`-Only tour -Offscreen`, the Editor target only. The full smoke, `Build-Jam.ps1 -Target Glyphbound` and the package
once, at the end. Long commands in the background.

## 7. Traps already paid for

- The pack's large pictures are virtual textures: a material of ours that samples one does not compile.
- A list of names that never block (`NO_BLOCK`) once matched 207 wall pieces. With twins, give them their own
  names and simple collision, and let the `windows` and `walks` stages tell you the truth.
- Double-sided collision set on a mesh asset takes effect only after the Editor is restarted.
- A level's collision is not ready in the same script run that loads it: trace in a second call.
- A long builder run outlives the HTTP request ("fetch failed" after about 5 minutes) and keeps running: wait for
  the result file.
- Lit pictures of interiors: use the game's tour. The offscreen capture has no Lumen history (black interiors) and
  the viewport capture needs a visible Editor window. `Unlit` renders are good for materials and for holes.
- `Remove-Item` under `Glyphbound\Saved\...` is refused in this setup: keep scratch pictures in your scratchpad.
- Patches to long files: a script file with `split(a).join(b)` and a "not found" check, never inline quoting.
  `Design/GDD.md` has CRLF line ends and UTF-8 characters: do not rewrite it through PowerShell.
- The Editor is closed only with `Tools/Jam/Stop-EditorMcp.ps1`. PIE is for people.

## 8. What this will not fix (tell the owner the number, do not hide it)

The castle pack is 1,172 MB of 6,491 MB of cooked content. The rest is mostly the future base's kits (Deep Water
Station 1,426 MB, High Tech Base 1,411, SciFiWorld 842, ModularSciFiStation 632). **Removing the castle pack alone
will not bring the zip under 1 GB.** That is a second job (which pieces of those kits the base really uses) and it
is not part of this handoff unless the owner adds it. Report the zip's size after your work and what is left.

## 9. Your first reply to the owner

In Spanish, short: what you understood, the one-zone step and when he will see pictures, and anything in this
handoff you disagree with. Then start.
