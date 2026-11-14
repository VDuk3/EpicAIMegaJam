# 2026-11-14, block: the future's enemies (the ASSETS deliveries made playable)

Architect: Claude Code (Opus 5.5) on CENTRAL, working overnight while the owner slept. Implementer: a Claude Sonnet 5.5 subagent (exception to the Muse rule: the owner allowed Haiku and Sonnet subagents for this block in his own words, and the work needed many engine-specific build-and-smoke rounds). Code review: Sol 6.1 through Codex CLI.

## What the owner asked
> Hay múltiples assets que se te han enviado desde la computadora de assets, incluyendo personajes, armas, drones, arañas mecánicas, un centinela, orcos, etc. [...] Tu tarea es agarrar todos esos assets, incorporarlos en el juego y dejarlos en un estado jugable. Todos los humanoides tienen el esqueleto del mannequin de Unreal Engine 5 [...] Para animar el dron solamente tienes que animar su ojo y animar sus hélices. [...] Tenemos que animar también el ojo del centinela. Y a la araña no estoy seguro si se le hizo el rig. [...] Dale un comportamiento de IA básico de shooter en primera persona a los enemigos que pongas en el proyecto. [...] Ya hay VFX para armas en el proyecto, úsalos también. Comunícate con el chat de "Mapas de mundos" para que no se pisen.

Translation: Several assets were sent from the assets machine: characters, weapons, drones, mechanical spiders, a sentinel, orcs. Take all of them, put them in the game and leave them playable. The humanoids have the UE5 mannequin skeleton, so they should be the easy part. For the drone, animate only its eye and its rotors; the sentinel's eye too. The spider was probably not rigged. Two Fab links are offered for animations and enemy AI: analyse them, do not copy everything. Give the enemies a basic first-person-shooter AI. Use the weapon VFX already in the project. Coordinate with the maps chat so we do not step on each other. (He also said: work in batches, do not stop, he would be asleep; questions go to the advisor, not to him.)

## The deliveries (from the ASSETS machine, 2026-11-13, all made with Tripo from Nano Banana reference images)
| Asset | What arrived | What it needed |
|---|---|---|
| Robot | 1 mesh, 80-bone skin on UE5 mannequin bone names, no clips | scale (1.70 m), clips |
| Orc | body (80 bones) + two armor line-ups (one mesh each, no skin): 4 sets x helmet, chest, gauntlet, boot, belt, thigh plates, upper-arm plates, shoulder pads, greaves | a modular system (the owner's explicit goal) |
| Valkyrie | 1 mesh, 79 bones, wings; initial drawing by Angélica Leon | scale, clips; she is the future boss |
| Robot spider | 1 mesh on a round display base, **no skeleton** | base removed, a rig and clips made from nothing |
| Sentinel | a sphere with a blue core, one fused mesh | the core split off to move as an eye |
| Drone | one fused mesh | eye and four rotors split off |
| Rifle for robots | one mesh, lying diagonally | turned, grip origin, muzzle socket |
| Valkyrie's sword | one mesh (first reference: a hand drawing) | turned, grip origin |

## The two Fab links
- **Pistol and Rifle Locomotion Animations 1700** (Kingboars, free): 1,688 locomotion clips on the UEFN mannequin. No firing, hit or death clips. Not installed for now: the UE template mannequin's own content (Templates/TemplateResources, shipped with the engine) already has an 8-direction rifle walk and jog set **plus fire, reload, hit reacts and deaths**, which is what an enemy needs. Kingboars stays the option for more locomotion variety (crouch, sprint).
- **AdaptivePathfindingSystem** (NordVader): a code plugin that shapes navmesh paths into arcs. Listed for UE 4.27 to 5.8; we are on 5.9, it would add a compiled third-party dependency, and our enemies already have navmesh paths plus local obstacle steering (GDD §10 "never stuck against a wall"). Not installed.

## What was built

### Art: from the GLBs to Unreal (architect, Blender headless + the Editor's Python)
- **One export script** (`Tools/Blender/enemies_export_fbx.py`) does every asset from its source GLB, so the work can be redone; Content is not in git, the scripts are the record. Intermediates go to a temporary stage folder.
- **Humanoids** keep Tripo's rig (UE5 mannequin bone names, A pose): scaled to robot 1.70 m, orc 2.10 m, Valkyrie 1.95 m with wings.
- **Drone and sentinel**: each GLB is one fused mesh. Its connected islands were listed with their centres and sizes, highlighted in red on renders, and turned into rules: the drone's four rotor hubs (four identical 3.7 cm islands at the top corners), their blades (thin islands within 30 cm of a hub, above it) and caps; the eye ball (the largest island) and everything inside its bounds. The sentinel's core is its orb and octagonal cage (islands in front, within 17 cm of the axis); the red arcs around it stay on the shell. Each moving part is exported with its pivot (rotor hub, eye centre, a point 12 cm behind the core so it rolls like an eyeball) and the body gets sockets where they go.
- **The spider had no skeleton**, and stood on a round display base. The base (every island under 4.6 cm) is dropped. The rig is built from the geometry: six legs at about ±45, ±90 and ±133 degrees around the body; for each, the hip (the leg vertex nearest the body axis), the knee (the highest vertex further out) and the tip (far and low); two bones per leg; every vertex weighted fully to its leg's nearer bone (a machine's segments are rigid). Five clips are keyed by the script: Idle, Walk (alternating tripods), Attack (rears up, front legs raised, strikes), Hit, Death (flips onto its back, legs curled). Checked on rendered frames before export.
- **Orc armor, modular (the owner's stated goal).** The two armor files are line-ups of loose pieces, four sets in four columns. A labelled render of the islands gave the layout; each island is assigned to a set and a slot by its position. Each piece is fitted to its slot's body region (the orc's vertices whose strongest bone belongs to that slot) in the frame of the slot's bone (long axis along the bone, front to the front), scaled per axis to the region times a margin, plates (thigh, greave, upper arm) put in front of the limb instead of centred in it, and skinned by copying the weights of the nearest body vertices. A single gauntlet or boot is mirrored for the other side. Result: 36 skeletal meshes `SK_OrcArmor_<A..D>_<Slot>` on the orc's skeleton, worn with Leader Pose. The first renders showed thigh plates buried in the legs (fixed by the front alignment); **the boots do not fit the orc's big feet and are left out of every preset**.
- **Weapons**: the enemy rifle lies diagonally in its GLB: turned along +X from the positions of its stock and muzzle islands, origin at the top of the pistol grip read off an orthographic side render with a millimetre scale, 80 cm long, a Muzzle socket. The sword: blade along +X, origin mid-grip, a Tip socket.
- **Import** (`Tools/Jam/import_enemies.py`): one shared master material `M_GBEnemy` (base colour, normal, metal/roughness from the glTF maps; a glow mask that makes the blue lights or the drone's red lens emissive; a hit-flash parameter), one instance per asset, physics assets for ragdolls.
- **Animation**: the UE template mannequin content that ships with the engine (Templates/TemplateResources/High/Characters) was copied into the project, and its rifle, unarmed, hit and death clips retargeted onto the robot (12 clips), the orc (22) and the Valkyrie (13) with IK rigs and IK retargeters made by script (`Tools/Jam/retarget_enemies.py`).
- **Review** (`Tools/Jam/review_enemies.py`): a line-up in a review level, posed with the clips, weapons in hand, rendered offscreen. The weapon's place in the hand is computed from the aim pose: barrel from the right hand towards the left, grip at the palm.

### Problems found on the way (each cost real time)
- **Characters 1/100 the size once animated.** The skeletal FBX files, exported in metres like the guns, came into Unreal with the root bone at scale 100. In the reference pose that looks right; every clip resets the root to scale 1. Exporting with "scale all" gave metres; Unreal's import scale put the 100 back on the root. Fixed by exporting the skeletal assets in centimetres (scene unit 0.01, objects x100, keyed offsets x100).
- **Everyone lying on their backs.** First blamed on the retargeter's Root Motion op (it is off anyway: it copied the mannequin's root rotation onto a root with another orientation), it was the review script: Unreal's Python `Rotator(a, b, c)` is (roll, pitch, yaw), so the line-up was spawned pitched 90 degrees.
- **Grey materials**: texture parameters made from Python had no default texture, so the master material did not compile.
- **Sockets not saved**: sockets created in Python without the mesh as their outer were lost; the drone's rotors and the sentinel's core sat at the body origin in the first smoke runs.

### Code (Sonnet subagent, from `Prompts/enemies3d_code.md`; reviewed by Sol 6.1)
- `UGBEnemyAnimInstance`: animation **without AnimBlueprint assets**: a native AnimInstance with its own proxy that samples and blends the clips by speed and direction (8-way set), adds the aim pitch over the spine, plays one-shot attack and hit overlays (additive clips added, others blended) and holds the death clip.
- `AGBSkeletalEnemy`: the look from data (mesh, armor list, weapon on a socket with an offset); the **basic FPS shooter AI**: sight cone and alerting the others, a distance band with strafing on the navmesh, always facing the player, telegraphed bursts of hitscan shots with spread, line of sight needed; melee with a lunge (spider); ragdoll death with the weapon dropping; hit sparks.
- `AGBFlyerEnemy`, `AGBDroneEnemy`, `AGBSentinelEnemy`: hover height, bob, sweeps around obstacles; the drone's rotors spin and its eye tracks the player; the sentinel turns slowly, its core eye tracks fast inside the opening and pulses, it charges a beam with a lagging aiming line, and its core is the weak point.
- VFX from the project's packs: muzzle flashes (MuzzleFlash3D), tracers and energy/technology impacts (SciFi Impacts), concrete impacts (GunFX).
- **Sol 6.1 review: seven findings, all fixed**: flyer corpses fell through the floor (actor collision still off), shooters lost the stuck recovery, local movement without a navmesh came in isolated pushes, Slow did not slow flyers, armor did not follow the body's scale, and two smoke checks that wrong code passed (a lunge that dealt no damage, a flyer that only rose).

### Data
- `Enemies.json`: the five future ids get their classes, assets and tuning; new `Robot_Future`, `Sentinel_Future`, `Spider_Future`; `Valkyrie_Future` is 3D with her sword (bursts of light from it) until her own fight exists. A burst deals about one hit of the GDD table (3 x 3 for a soldier).
- `Encounters.json`: `Future_Bay`, `Future_Spiders`, `Future_DeepWing` (2nd Power Cell), `SubBoss_Future` (Warlord, sets `FutureSubBossDefeated`, 1st Power Cell). Placed in FutureBase by the maps chat's builder (its entry: `2026-11-14_future_base_encounters_block.md`).

## Checked in the game (no PIE; smoke runs and screenshots)
- **Arena smoke block `Enemies3D`**: every new type moves (horizontal distance), attacks and hits the player (shots logged for gun enemies, a registered hit for melee ones), flyers hold their height, the rifle's Muzzle socket is found, the drone's four rotors and eye and the sentinel's core sit at their sockets, every humanoid and the spider ragdoll on death, corpses are removed.
- **Real map block `Enemies3DBase`** (FutureBase, the four placed encounters, player invulnerable): each starts from its trigger, spawns, its enemies act, it clears, sets its flag, and the sub-boss hall and the deep wing drop a Power Cell each.

  | Encounter | Wave 1 | Acted | Shots | Flag | Cells |
  |---|---|---|---|---|---|
  | Future_Bay | 5 (13 in 3 waves) | 5/5 | 36 | Encounter.Future_Bay.Cleared | - |
  | Future_Spiders | 4 | 4/4 | lunges | Encounter.Future_Spiders.Cleared | - |
  | SubBoss_Future | 5 | 5/5 | 32 | FutureSubBossDefeated | 1 |
  | Future_DeepWing | 5 (8 in 2 waves) | 5/5 | 25 | Encounter.Future_DeepWing.Cleared | 1 |
- **55 tests green**, Editor and game builds, **full smoke clean** (297 steps, about 200 s: the script's default timeout went from 180 to 300 s).
- **Package**: `TeamTBD_Glyphbound.zip`, **1,999.8 MB** (the owner said the 1 GB limit does not matter for now). Packaged full smoke clean; packaged `Enemies3DBase` on FutureBase: all four encounters cleared, flags set, both cells dropped.
- Nobody has played it by hand yet.

## Media
- `media/2026-11-14/enemies_review_armored_orcs.png`: armored orcs (sets A to D) in the review line-up, aiming.
- `media/2026-11-14/enemies_review_robot_rifle.png`: the robot holding the rifle (grip and support hand computed from the aim pose).
- `media/2026-11-14/enemies_spider_rig_clips.png`: frames of the spider's script-made clips (idle, walk, attack, death, hit).
- `media/2026-11-14/enemies_armor2_islands.png`: the second armor line-up with its island numbers, used to map pieces to slots.
- `media/2026-11-14/enemies3dbase_*.png`: the four fights in FutureBase from the player's view (smoke).

## Left open
- **The owner should play it**: feel of the shooters (burst rate, accuracy, how fast they strafe), the drone's movement (he said "we adjust the movement later"), the sentinel's charge, the spider's leap.
- **Which enemies infest the deep wing** is the owner's call (spiders are a provisional proposal; the sentinel there carries the second Power Cell).
- **Boots** of the orc armor are left out (they do not fit the orc's feet); the fit of each piece was judged on renders, not up close in the game.
- **The Valkyrie** fights as a generic 3D enemy with sword bolts until her own fight is designed; she is not placed in her tube yet.
- **Kingboars locomotion pack** not installed (template clips cover what the AI needs); it is the option for crouch, sprint and more variety. **AdaptivePathfindingSystem** not installed (UE 5.8 at most, and our steering covers it).
- Package size, now 2.0 GB.
