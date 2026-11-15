# 2026-11-15, overnight block: the owner's seven-point list (Valkyrie, deaths, smoke, castle enemies, portals)

Architect: Claude Code (Opus 5.5) on CENTRAL, working alone through the night while the owner sleeps (his rule for
tonight: decide, do not ask; the advisor is Fable 5.2). Implementers: Claude subagents (Opus for the sprite baker and
the castle enemies, Sonnet for the deaths, the Valkyrie's fight and the tunnel smoke's game side). **Exception to the
Muse rule, noted here as the rule asks:** the owner's overnight list explicitly says to use Opus / Sonnet / Haiku
subagents by difficulty, and the night needs several implementers in parallel on disjoint files with a shared build
lock, which one OpenCode session at a time cannot give. Code review: Sol 6.1 through Codex CLI (logged wrapper).

## The owner's list (Spanish, translated; the full text is `Design/OVERNIGHT_GOAL.md`)
1. The cyberpunk Valkyrie: rig her wings so they stay in place on her back; watch the sword in her hand's socket;
   an AI that fights near and far, with at least one spell (placeholders allowed).
2. The drone and the sentinel: when they die they must fall to the floor and, on touching it, spark, explode or come
   apart; not stand still and vanish.
3. No ragdoll on any 3D enemy: each one its own death animation, different if possible (Fab allowed).
4. Smoke or fog in the tunnels to the tentacle boss's hall: Niagara Fluids, slow, reacting to physics, realistic, no
   cloud silhouettes; quality first, watch the cost on the RTX 4080 Laptop.
5. The castle's pixel-art enemies: orcs with combat animations; a few new 90s-RPG enemies (big and small, tougher and
   weaker, some ranged) with animations and VFX; balanced fights in every open zone; the pixel-art Valkyrie as the
   castle's boss in its highest zone.
6. The castle enemies' AI at the level of the cyberpunk ones: distance by style, cover, attacking together, cornering.
7. A portal plan in the GDD that makes the player alternate eras, balanced, with castle zones reached only from the future.

## Point 7: the portal plan (done, `945e6c6`)
GDD v0.70, §4 "Portals and the route between the eras". Five portals (P0 the seal, P1 to P4), each at the same spot
in both eras; six legs that alternate castle and base, each ending in a dead end whose only way on is a portal and
each leaving the cause that opens the next leg in the other era (the castle's winch powers the base's service
corridor). The wall walk, the King's Hall (from its gallery), the undercroft and the Top Level (the past Valkyrie)
are reached first from the future; shortcuts opened from inside join the castle back together. Elemental zones fall
on the legs. A plan only: what exists and what it needs is listed in the section.

## Point 1, first part: the wings (`b65ae01`)
- **Diagnosis** (Blender, `Tools/Blender/valkyrie_wings_inspect.py` / `_render.py`): the model (Tripo, 34,027
  vertices, one material) has 625 mesh islands; the wings are 156 of them, a slab just behind her T-posed arms, and the
  auto-rig skinned them to `upperarm`, `lowerarm`, `clavicle` and even finger bones. Every arm movement in a clip bent
  and tore the feathers: the "disaster" the owner saw.
- **Fix** (`valkyrie_wings_lib.py`, run by `enemies_export_fbx.py valkyrie`): an island is a wing piece when its middle
  is out from the spine and fewer than 55 % of its vertices lie within 5.5 cm (GLB scale) of the arm bones, or it sits
  behind the arm line; islands near the leg bones are excluded (the first rule took a boot). Its 11,824 vertices get one
  weight, 1.0 on `spine_05` (the upper chest), and each half is swept 22 degrees back about its root, so they read as
  wings on her back and not as a flat cross. Checked in Blender with the arms posed (`valkyrie_wings_check.py`) and in
  the Editor in seven clips (idle, walk, three attacks, charged, hit) from the front, back and three quarters.
- **Reimport** (`Tools/Jam/reimport_valkyrie.py`): onto her existing skeleton, so her retargeted clips, the IK rig
  and the retargeter stay valid; material slot kept.
- **The sword** (`Tools/Jam/review_valkyrie.py` and close-ups at `hand_r` in four poses): the grip sits in the hand in
  every clip; the fingers are open around it because her clips are the mannequin's unarmed set. The fight's code
  closes the hand procedurally (block D).

## Points 2 and 3: deaths without ragdolls (block A and the architect)
- **Block A** (Sonnet subagent): no ragdoll anywhere. A humanoid plays its own death clip once, holds the last frame, drops
  its weapon, lies, sinks and goes (`AGBSkeletalEnemy::TickDeath`). The drone and the sentinel got a small debris
  simulation of our own (`AI/GBDebris`, a pure step tested by `Glyphbound.Debris.*`): the wreck falls, at the first
  touch of the floor it sparks and explodes, the drone's rotors and eye and the sentinel's core fly off as pieces, the
  wreck sparks a moment, sinks and is removed. The Sol review found three issues in it; A fixed them.
- **The clips (architect).** The first smoke showed every humanoid still standing after its death: the UE template's six
  death clips are only the stagger before a ragdoll (measured: the pelvis stays at 83 to 88 cm, the root slides 1.3 to
  2.2 m). The **ARPG Pack** (Fab) has full falls; measured in the Editor, its four weapon sets share two in-place falls
  (back and forward) and four deaths that carry the body 2 to 4 m with the root. `Tools/Jam/retarget_arpg_deaths.py`
  retargets one per enemy with the existing retargeters and sets **Force Root Lock**, so the travelling ones fall where
  the enemy stood: Orc_Future falls back, the gun carrier forward, the enforcer is knocked down, the siege orc collapses,
  the warlord falls to his knees, the robot slumps, the Valkyrie falls forward. Checked in renders of the last frame
  (`Saved/Review/deaths`) and in the smoke: pelvis 12 to 25 cm over the floor 1.3 to 2.9 s after the kill.

## Point 4: the tunnel smoke (block E and the architect)
- **Block E** (Sonnet subagent): `AGBFluidFog`, a box that runs `NS_TunnelFog` only within 30 m of the player and feeds
  the gas each frame with what moves through it (player, enemies, shots, projectiles, blasts); the clipping helper is
  tested (`Glyphbound.FluidFog.Clip`); shots reach it through a new static event `UGBCombatLibrary::OnShotTraced`.
- **Authoring (architect).** `NS_TunnelFog` is the plugin's Grid 3D smoke template, edited through the Editor's Niagara
  toolset: no heat and no buoyancy (it hangs and drifts instead of rising), a long low ellipsoid source on the floor sized
  from the box (new user parameters FillRadius, FillScale, FillDensity), two layers of source noise, vorticity
  confinement on, a slow wind along the tunnel (18 cm/s), slower density dissipation, a cool grey with a higher albedo,
  the grid centred on the actor, the debug bounds off. The walls stop it through mesh distance fields.
- **A finding worth keeping:** no gas template reads the plugin's `NDC_FluidCollision` channel (searched every asset of
  the plugin); only `NDC_Grid3D_Gas` is read, and only when the emitter's "Use Data Channel Source" is on (it was off).
  So the moving bodies are written into the gas channel as velocity sources with outward divergence (the gas is pushed
  out of the sphere and dragged along), and the source reader is switched on. Velocity is composited by adding.
- Placed by `Build-FutureBase.py` (stage deep only): the whole dark corridor and the last 15 m of the worn corridor.
- **Tuned on the real map (architect, `8c67581`, `2d1fc75`).** First passes looked wrong in the captures, in order:
  the test box sat half under the floor; then the gas filled the box like a wall; then a standing player blew the whole
  box full (a constant outward push every frame); then the worn corridor was opaque. Fixes: the gas hangs low (density
  buoyancy, gentler turbulence with no sideways bias, less small-scale force); a body pushes in proportion to its speed;
  stronger, shorter shot pushes; at render, density range 0-5 with gain 0.1, so 15 m of it reads as layered smoke over
  the floor. The dark corridor showed nothing at all: unlit smoke cannot be seen, so three dim cold emergency lights now
  sit low on its walls, one flickering (provisional: the owner had asked for that corridor fully dark).
- **Checked on the map, not only in the arena:** `gb.Smoke FogVisit` on FutureBase stands the player in each fog box and
  checks that the gas simulates there, that a blob written by the game reaches the GPU simulation, that three shots and a
  blast write into its channel (312 and 264 entries), and compares the frame time with the gas forced off: about 0.8 ms
  (dark corridor) and 1.4 ms (worn corridor) offscreen on the RTX 4080 Laptop. The Sol review of this smoke found that it
  passed even with no gas at all and that it compared two different lightings; both fixed (it now fails, and the lamp it
  adds for the captures is gone before the gas-off sample).

## Point 5: one fight in every open zone of the castle (architect)
- Measured the keep with traces in the Editor (`castle_probe2.py` in the scratchpad: a floor counts when 2 m above it are
  clear; a spawn cell is open when its eight neighbours are floor at the same height): the keep's floor is 250 cm, the
  upper ward 225, the wall walk 1203, the Top Level's highest room 2612 (a room of about 20 x 26 m; the other "floors"
  up there are turret stairs and roofs). Top-down captures of each hall in `Saved/Review/castle_zones`.
- Fights placed for each zone's shape (`Build-PastCastle.py` ENCOUNTERS, `Encounters.json`, provisional): the lower
  ward, the side yard, the Front Hall (close combat down the aisle), the Main Hall (the brute as sub-boss), the wall walk
  (long sight lines: ranged at the far tower, barghests from behind), the King's Hall, the Side Hall, and the past
  Valkyrie alone in the Top Level's highest room, with a new navmesh volume for the Top Level. The first wave of each
  stands in its zone (patrol or guard) and the ranged enemies take the far spots.
- **Checked on the map (`2d1fc75`):** `gb.Smoke CastleVisit` on PastCastle walks the player into each of the eight
  fights in turn: every prespawned enemy stands on the floor inside its zone, notices the player, attacks; every later
  wave comes, attacks and is killed; the fight ends Cleared. The past Valkyrie is awake on arrival, takes damage, combos,
  casts Judgement and dies. What it found and what was fixed: two spawn points inside wall meshes (moved); the wall
  walk's first point inside the corner tower (2.1 m of headroom, moved out); a builder check now traces 270 cm (the brute)
  over every one of the 54 spawn points; wide archers that stalled near their firing spot never fired (they now fire from
  where they stand); an enemy projectile's flight clip filled the screen when it hit the camera (hidden within 1.5 m); the
  barghest and the brute were too dark at night (more sprite glow). The Sol review found that a wave that never attacked
  only gave a warning; it now fails the run (all eight fights: waves engaged = waves that came).

## Point 6: the castle's squad AI (block C, Opus subagent, audited by the architect, `9b49578`)
- The castle's sprites run the future's squad AI through one "look" abstraction in `AGBSkeletalEnemy`: the same zones,
  distance bands by style (shamans and centaurs far, orcs near, the brute through the middle), cover, attack tokens and
  flanking. New opt-in tactics for the castle squads: a **pincer** (two attackers from opposite sides of the player),
  **turns** between attackers, and **cornering** (they move to close the player's open sides, probing with traces).
- Checked in the arena (`gb.Smoke Castle`, 3 of 3 green: tactics, ranged, waves) and on the map (CastleVisit's TACTICS
  lines per squad: roles, cover, relocations).

## Assets brought in tonight (third party, Fab, from the owner's library)
| Pack | Seller | Licence | Used for |
|---|---|---|---|
| Quadruped Fantasy Creatures | PROTOFACTOR INC | Fab personal | the Barghest and the Centaur, rendered to pixel-art sprites |
| Zombie Animation Pack: Standard | (Fab) | Fab personal | orc strike and walk clips, retargeted, rendered to sprites |
| Combat Magic Animations | (Fab) | Fab professional | the Valkyrie's spell clips; the orc shaman's casts |
| Dark Fantasy Weapons (Medieval_Weapons) | (Fab) | Fab professional | mace, axe, staff on the castle orcs (rendered to sprites) |
| Stylized Fantasy Characters | (Fab) | Fab professional | installed, not used so far |
| ARPG Pack (Dual Wield, Halberd, Samurai, Warrior) | (Fab) | Fab (tier to confirm) | the 3D enemies' death falls, retargeted |
| Free Sample Animation Set | (Fab) | Fab (tier to confirm) | downloaded to look for deaths; it has none, not used |
Sellers left as "(Fab)" are to be filled from the listing pages before submission.

## Pipeline decisions (architect)
- **Death clips without a download:** the UE template mannequin has six death clips; two were retargeted before. The
  other four (`Front_02`, `Front_03`, `Left_01`, `Right_01`) are retargeted onto the Orc, Robot and Valkyrie with the
  existing retargeters (`Tools/Jam/retarget_deaths.py`, read-only on the retargeters), so every humanoid gets its own.
- **The castle's characters are pre-rendered sprites**, the way 90s games made theirs: animated 3D models rendered from
  8 directions and reduced to the style guide's palette ramps with a black outline (`Tools/Blender/sprite_bake.py`,
  format in `Prompts/sprite_sheet_format.md`), like the project already bakes Niagara fire into pixel flipbooks. The
  hand-drawn `valki` sprite (the owner's, 75 x 93 px) sets the pixel Valkyrie's colours and scale.
- **One AI for both eras:** the castle enemies use the future enemies' squad AI (`AGBSkeletalEnemy`) with a second,
  sprite "look", instead of a parallel pixel AI.
- **The tunnel smoke is a real Grid 3D gas simulation** (Niagara Fluids), fed every frame through the plugin's gas data
  channel (`NDC_Grid3D_Gas`; the collision channel has no reader, see point 4) with the player, the shots and the blasts,
  so it parts and swirls around them.
- **A flaky red in the final full smoke, fixed at its cause, not retried away:** the Enemies3D tactics check failed in
  both full runs (two members within 200 cm for 4.5 s, then 6.1 s; limit 3.5) and passed in the filtered runs; before
  tonight the full runs measured 0 to 2.1 s at the same spots. First case: an orc whose path ran into a robot at its
  cover gave up moving and held there; second: a robot and an orc on their way to their positions walked together.
  Now a ranged enemy that stays within 220 cm of a ranged squad mate for 1.5 s outside a peek (holding or moving) asks
  the squad for a new position, which keeps 350 cm from every mate. The Sol review of the first version: the crowd test
  counted melee mates the position search ignores (an endless relocation); fixed by counting the same members. The
  check's failure line now also prints both speeds.
- **A second Sol review of the final rule:** in a narrow corridor where no spot keeps 350 cm from every mate, the search
  answers "nothing" and the enemy would ask again every cooldown, forever. Now a search that finds nothing rests the
  crowd rule for 6 s.
- **The advisor's final review caught a check never run since the Valkyrie's new fight:** `Enemies3DBase` on FutureBase
  (her tube in the lab). It went red once in two runs: 3.5 s after waking she was casting Judgement 2.1 m from where she
  lay, raised 70 cm (flying), and the check, written for yesterday's Valkyrie who ran straight at the player, wanted her
  on the ground and 2.5 m out. Her wake leap lands on the navmesh past the glass at about 2.15 m; the check now takes her
  as out when, at some frame between waking and the check (sampled every frame), she stood more than 1.5 m from where
  she lay, inside her zone, on the floor or raised by Judgement: a single instant was not enough, since the packaged run
  caught her 4.6 m out but in the air in a hop. It still fails if she stays in the tube (Sol: no defects).
- **CastleVisit's stricter wave check (Sol):** a later wave of melee-only enemies stays in its zone; if the player stood
  outside it after the first wave, they could never reach him. The visit now steps the player in after 8 s without an
  attack, as it already did for the first wave, before it fails a wave.
- **Code reviews (Sol 6.1, Codex CLI, logged wrapper):** `Prompts/review_overnight_valkyrie_fog_sol.md`,
  `review_overnight_castle_sol.md`, `review_overnight_visits_sol.md`. Every finding was fixed before the commit that
  followed it.
- **Tools used tonight** (for the submission's "Tools you used"): Unreal Engine 5.9 (Niagara, Niagara Fluids, IK
  Retargeter, Editor MCP), Blender (the sprite baker), Fab (the packs above), Claude Code (Opus 5.5, Sonnet, Fable 5.2 as
  advisor), Codex CLI (GPT-6.1 Sol for reviews).

## End of the block: verification and the package
- Test suite: 73 tests, 0 failed (1.2 s). Full `Smoke-Jam.ps1` (uncooked, offscreen, 559 steps): 0 errors, 472 s.
- Game target build; package `TeamTBD_Glyphbound.zip` 933.7 MB, no `.pdb`.
- Packaged smokes, all green: Fog, Valkyrie, Castle (arena), Enemies3D (no two squad members within 200 cm at any
  time), FogVisit on FutureBase (the gas simulates in the cooked build; 0.77 and 1.53 ms), CastleVisit on PastCastle
  (8 of 8 fights cleared).
- For the owner's eyes (no PIE tonight): the tunnel smoke in motion, the castle fights' feel and difficulty, the pixel
  Valkyrie's wings (clean but blocky), the Judgement rings (3D, not pixel art), the dark corridor's emergency lights.

## After the block: the owner awake (late morning)
- **The pixel Valkyrie's Judgement in pixel art** (the circles were 3D shapes in a pixel castle): when her caster has
  the sprite look, each circle is a flat pixel sigil on the floor (`judgement_sigil` in `Sprites.json`, 32 x 32, a
  gold ring and a cross, generated clean by script) over a dim pixel disc that grows to it (`judgement_fill`), and the
  strike is the baked pixel blast `FluidExplosion` as wide as the circle instead of the 3D pillar; its light is a
  quarter of the 3D one (three strikes at once washed the castle white in the capture). Castle smoke green with
  telegraph, strike and blast captures.
- **Her wings, owner's note** (Spanish, translated: "the Valkyrie's wings should not be one colour, they should have
  shape, shadows, nuances, different colours. Also remove the border: it makes them look flat and solid instead of
  something ethereal made of feathers"). The baker gets a per-region `outline: false` (`sprite_pixel.py`: no black ring
  and no filled pinholes next to such a region); the wings get a six-tone ramp from violet at the root through mauve and
  rose to gold and cream at the tips, the feather texture's luminance in the tone (`albedo_weight` 0.9), and none of the
  smoothing that had flattened them (no close, open, blur or mode passes): the feathers now fan out one by one.
- **Time savings (owner: under 12 hours left):** measured where the runs went. A filtered smoke run kept its quit step
  at the full run's end, so a block sat idle until then and a sub-block until the timeout: it now quits 5 s after its
  last step. The filter ignores spaces, so a sub-block can be run alone (`-Only Enemies3D:tactics`: 45 s instead of
  223 s). `Tools/Jam/Verify-Jam.ps1` is the end-of-block check in one background command (suite, package, packaged
  full smoke; `-Visits` adds the three map visits): the uncooked full smoke (the same 559 steps), the separate Game
  build (the package compiles it) and filtered packaged runs the full run covers are no longer repeated. Rules added to
  `CLAUDE.md`.
- **The tactics red came back (3.6 s, then 4.3 s in the other chat's parallel run): it was not fixed at its cause.**
  The advisor stopped a quick redefinition: the failure line printed only the last frame, and every reading over
  1.5 s was the same Robot_Future / Orc_Future pair in the same corner, only in full runs. Nothing is left in the
  arena by the blocks before (entity, pixelvfx and mantle clean up; the navmesh was ready at once in every run). The
  check now splits each close stretch per frame: in two measured full runs, 0.7 s both moving + 1.1 s one standing
  (a robot running past an orc peeking at its firing spot), then 1.8 s both moving + 0.9 s one standing (the two
  running to their places side by side). Two decisions: the crowd rule reacts after 1.0 s instead of 1.5 s (the
  standing case is its latency); the 3.5 s limit now counts only close time with one of the two standing (stacked),
  and two members moving together have their own limit of 8 s (one following the other around), since a squad
  running together is what the owner asked for ("se mueven en grupo"). Both are logged in every run.
- **Sol 6.1 on these changes:** the early quit would have cut the seal and tour blocks short (they run their own state
  machines and quit the game when done): any filtered run that keeps a seal, tour or FogVisit step keeps the far quit.
  `Verify-Jam` now needs the stage's exit code 0 as well as its success line (a zip or size failure after UAT's BUILD
  SUCCESSFUL used to pass) and passes `-Project` to every stage. Its first real run: package 933.9 MB, packaged full
  smoke, FogVisit, CastleVisit and Enemies3DBase green; the suite (73, 0 failed) showed red only because "0 failed"
  matched FAILED without case: the check is case-sensitive now. `-Only seal` passes and quits by itself (18 s).
- The POV hands (owner's question): the other chat's removal (`b377478`) is in the build; the packaged captures show
  the pistol and the rifle with no hands.

## Owner's castle playtest (afternoon, 11 notes)

The owner played the packaged castle and sent 11 notes with screenshots, asking to use a subagent and to save time.
Split: the four AI notes went to an **Opus subagent** (Muse exception, noted: time, and the AI fixes needed measured
smoke runs between attempts); the map notes stayed in this chat and were prepared while the subagent held the machine.

- **AI (subagent, `4dd53fb`; Sol 6.1 review: no bugs).** Castle enemies 35% faster (move, combat and sprite-run speed;
  patrol speed kept, since raising it made a CastleVisit zone miss the player). Centaurs fire more often (shorter
  interval and windup, three arrows a burst). Melee enemies already in reach swing at once (the pincer wait and the
  squad cap of two no longer hold them). The circling near the player had three causes, all fixed: the enemy that
  had just swung walked back out to the wait ring, the ring's reference attacker flipped every frame, and a waiter at
  its slot counted as stuck and jittered. Castle, Enemies3D:tactics and CastleVisit green.
- **Wall at the entrance:** a slit of sky stood between the front curtain wall and the gatehouse on both sides; a stone
  column fills each.
- **The moon looked like a sun:** smaller disc glow, a third of the Mie halo, little bloom.
- **Entrance light:** the point light became the baked FluidFire pixel clip (Niagara Fluids, baked by our plugin) over
  the fire pit, as an `AGBTorch` with its own light.
- **Windows:** two pixel panes drawn by script (leaded diamond glass; a stained-glass arch with a gold star), material
  `M_PxGlass` (lit, a little glow). The window stage's blocking panes are now visible outside the towers: 165 leaded,
  and 20 stained glass in the keep's tall windows above 23 m; the towers' 250 openings stay bare, as asked.
- **Stairs:** one stone tile spanned several steps. `M_PxStairs` lays the dressed stone on the treads and a dark course
  on the risers, by the face normal, on the five stair twins. A first pass picked meshes by name and caught a vaulted
  ceiling and a corridor wall; their tiles were put back the same minute.
- **Floor separation between rooms:** `castle_ledges.py` scanned the keep with a 50 cm grid of traces and listed every
  step between flat floors. The plank floors stood 22 cm apart where a raised square-plank patch met the hall's planks;
  `castle_floor_level.py` lowers such tiles onto their neighbours (with what stands on them) and runs after the keep
  stage. Captures before and after. The stone thresholds under the walls stay.
- **The VSM overflow message:** `r.Shadow.Virtual.AllowScreenOverflowMessages=0` (a debug message, never for players).

## No hands at all, and the hand-drawn staffs

- **The owner asked why the demo still showed hands** after the other chat removed them on 2026-11-14. Not a version
  or commit problem: that commit (`b377478`) removed the hands with a gun only, and its own message says the staff and
  the sword kept their hand. The staff is the starting weapon, so the hands were the first thing seen. The owner's
  rule is no hands at all: `GBHUD` now draws no hand on the staff or the sword, no hand when empty-handed, no left
  hand, no casting hand and no climbing hands; only the weapon sprites and the 3D guns with their own motion remain.
- **Four staffs drawn by Romelt in Pixelorama** (2026-11-12, during the jam; sent from the LAPTOP inbox with hashes):
  basic (`baston1`, white-silver orb), storm (`baston1.2`), ice (`baston2`), fire (`baston3`). Sources in
  `Glyphbound/SourceArt/PixelArt/Staffs` (the `.pxo` files, the 1:1 PNGs, the laptop's notes and Play captures).
  `Tools/Jam/png_to_sprite.js` turns each PNG into a `Sprites.json` entry (palette and rows, cropped to the drawing).
  The basic staff is now the starting staff (`Staff_Apprentice`); the fire staff replaces the ember staff's sprite;
  `Staff_Frost` (ice) and `Staff_Storm` (lightning) are defined. The new art is twice the old staffs' resolution
  (22x90), so `SpritePixelScale` 0.5 draws it at about the old size; it stays on whole screen pixels (Sol noted the
  rounding makes it 10% larger at 1080p; kept for a clean pixel grid).
- Sol 6.1 on the HUD and data change: no bugs besides that rounding note.
- Capture: `Devlog/media/2026-11-15_staff_no_hands.png` (packaged smoke: the basic staff at rest and during a cast, no hands). Verify-Jam green: suite 73, package 934 MB, packaged full smoke 0 errors.
- Checked in the packaged game (gb.Smoke tour on PastCastle, green, `Devlog/media/2026-11-15_castle_entrance_moon_campfire.png`): the pixel campfire burns in the ward and lights the cobbles; the moon is a small disc with a short glow; the leaded windows show on the keep.
- **Who did what, this afternoon:** the AI notes went to the Opus subagent (Muse exception: time, and they needed
  measured smoke runs between attempts). The map scripts (windows, stairs, floors, walls, moon, campfire) and the HUD
  hand removal were done directly by the orchestrator (Muse exception: each was a small, Editor-bound or one-file
  change that had to be checked with captures at once). Sol 6.1 reviewed the code changes.
- **Note 9 is only partly fixed, and reported so:** the raised plank patch in the hall was levelled, but the owner's
  picture may be another doorway. The scan also lists 28 cm steps (ground floor) and 48 cm steps (Top Level) between
  the halls' planks and the timber floor beside them. A try at half-height timber steps there was undone the same
  minute: the scan reads the foot of a wall as a doorway, and the steps came out as grey slabs along the walls. Open
  until the owner says which doorway he saw.
- **Decision (provisional, GDD §8):** the frost and storm staffs are defined but no chest gives them yet.

- **Dungeon corridor floor turned to lava (owner request, ~19:40 EST).** `Tools/Jam/castle_lava.py` (written directly by the orchestrator: a small data/material change, no time for a Muse round) builds `M_PxLava` (procedural pixel lava: world-space noise snapped to a 12.5 cm pixel grid, five flat heat steps, unlit) and `M_PxHeatHaze` (an invisible refracting two-sided box over the lava whose normal scrolls upward; it samples `T_HeatHaze_Normal` from the MuzzleFlash3D pack already in the project), puts lava on six `SM_Px_FloorCorridor` tiles of PastCastle and an orange point light over each. Torch lights also moved onto the flame (`LightOffset` 0 on all 34 torches, owner: "the light must be exactly in the flame").

- **Lava heat haze redone with Niagara (owner rejected three material-volume tries: blobs, too big and fast, invisible).** `NS_PxLavaHeat` is a copy of the MuzzleFlash3D pack's `NS_MuzzleFlash_Single` with every emitter but `NE_Haze` removed (a looping refracting sprite, material `M_HeatHaze` of the same pack), slowed down through its user variables; `castle_lava.py` places one every 2 m just over the lava (96 systems). `M_PxHeatHaze` deleted.

- **Lava heat haze removed for good** (owner rejected the Niagara sprites too: they read as spheres again). `castle_lava.py` now only lays the lava and its lights, and deletes the haze assets. **Lava lights no longer pass through walls:** shadows on and the radius cut from 9 m to 5.2 m (the corridor is 8 m wide).

- **The lava is the light source** (owner: "there can be no light in the air just because; the only light there must be the lava"). The six floating point lights are replaced by rect lights the size of each tile lying on the lava and shining up, shadows on; the lighting-channel try is undone. Checked with two Editor captures at the barred cell wall: the cell behind it is dark.
