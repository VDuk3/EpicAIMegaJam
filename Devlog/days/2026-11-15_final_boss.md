# 2026-11-15: the final boss, the plague reaper, and its arena

Task from the orchestrator chat (the session coordinating the last day): build the final boss fight. That meant the arena, the boss placed in it, its animations taken from an animation pack we already had, and its full combat logic. The owner directed every design point in this chat, in Spanish; his words are quoted and translated below.

## Directing: what the owner decided
- **Which model.** I first described it wrongly as "a mage with a staff". The owner (*"No, no es un mago con un bastón. Es este"*, "No, it is not a mage with a staff. It is this one") sent the review render: a hooded plague doctor with a scythe. He then approved the import at about 3.5 m tall.
- **Which animations.** *"La animación tiene que salir del RPG pack de animaciones, el jefe de la alabarda"*, "the animation must come from the RPG animation pack, the halberd boss". So it uses the ARPG Pack's halberd set (Fab). The scythe is held as a two-handed polearm.
- **Design is his call.** I had proposed an arena layout of my own and filled in the boss's attacks. He stopped me: *"Estás tomando decisiones de diseño sin preguntarme, no hagas eso"*, "You are making design decisions without asking me; don't do that". I took my data change back. From then on every design point went to him as a question with options, and the plumbing went on meanwhile. This is the director's-cut theme in practice: the owner sets the vision, the agents build it.
- **His answers:**
  - Only the reaper fights; the floating machine (Entity phase 1) is not in this fight.
  - The fight is aggressive and uses four attacks: scythe combo, dash, floor circles, and a projectile fan.
  - The arena is 40 m across. The boss waits in its centre.
  - No VFX without his say: *"No hagas magias sin antes pedirme los VFX"*, "don't do magic without first asking me for the VFX".
- **Arena:** *"una arena circular, completamente negra ... en el suelo quiero un efecto de Niagara Fluids blanco que tenga un poquito de iluminación ... líquido, pero no agua como tal"*, "a circular arena, completely black ... on the floor a white Niagara Fluids effect with a little lighting ... liquid, but not water as such". He also wants it to react to the fight.
- **Attack VFX:**
  - The floor circles are made from the white liquid: it churns, then erupts.
  - The projectiles are *"bolas de plaga. Un brillo verde rodeado de humo de niagara fluids"*, "plague balls: a green glow surrounded by Niagara Fluids smoke". The explosions match, and he warned *"vigila los tonos de verde"*, "watch the shades of green".
  - He allowed Fable subagents for the heavy VFX work.
- **A new mechanic, mid-task: the empowered scythe.** *"Puedes hacer que el jefe bufe su espada en un momento y haga más daño"*, "make the boss buff its blade at some point and deal more damage". It uses the **Slash Trail FX Elemental** pack (Fab, SoftTofuVFX, Professional licence already in his library), Nature type. Only the VFX may glow, it affects only the blade, and the trail comes from the blade.
- **The blade.** The model's crescent blade turned out to be skinned to the shoulder, not to the staff; it stayed on the back while the staff swung. I showed him side renders. He chose to have it ride the staff.

## How it was built
- **Model:** `FinalBoss.glb` from ASSETS (Tripo, from Angélica Leon's hand-drawn front and back drawings), moved to `Glyphbound/SourceArt/Enemies/Reaper`.
  - `Tools/Blender/enemies_export_fbx.py reaper` exports it at 3.5 m, in cm, without the stray icosphere.
  - The same step re-skins the crescent's 495 vertices (its thin islands on the model's right, skinned to clavicle/upper arm) rigidly to `hand_r`. `reaper_blade_island.py` found those islands and checked them with an arm-raised pose render.
  - `import_enemies.py` and `reimport_reaper.py` import it to `/Game/Glyphbound/Characters/Reaper` with a green-glow material instance. Sockets `BladeBase`, `BladeMid`, `BladeTip` and `Grip` sit on `hand_r`; the hand bone matched Blender to 0.01 cm.
- **Animations:** `Tools/Jam/retarget_reaper.py` builds an IK rig and a Manny-to-Reaper retargeter, then retargets 20 ARPG halberd clips (idle, walk ×4, run, combo 1-4, heavy 1-2, the heavy windup, sprint attack, evade back, hits, deaths, equip). Every clip has Force Root Lock.
- **Fight code: no new class.** The Valkyrie's fight (`AGBValkyrieEnemy`) is already data-driven, so the reaper is the `Entity_Phase2` entry of `Enemies.json` on that class. Its new looks and moves are opt-in data keys, so the Valkyries do not change:
  - plague balls and their impact: Niagara on the lance projectiles;
  - white-liquid circles: a one-shot Niagara per circle, with the mark's own shapes hidden;
  - the floor liquid stirred by the player, the boss, swings, impacts and circle strikes: a Niagara Vector4 array on actors tagged `ArenaLiquid`;
  - **Empower**: a new action. The boss plays a windup clip (a new `Action6` clip slot), then for 12 s the Nature slash trails and leaves ride the blade sockets and its swings deal ×1.6, with the Nature hit burst.
  - The old energy-bolt bursts are switched off for it (`AttackRange` 0): the owner listed the four attacks it uses.
- **Arena:** `Tools/Jam/Build-EntityArena.py` builds `/Game/Glyphbound/Maps/EntityArena`:
  - a black floor and rim 40 m across, no lights, no sky, manual exposure;
  - the `Entity` encounter, prespawned in the centre;
  - the player start at the south edge.
  - `Game.json` `EntityMapPath` sends the final portal there. The map and the Nature systems are added to the cook.
- **Portal hand-over:** this was a TODO in `GBFinalPortal.cpp`. Before `OpenLevel`, the player's whole state is saved as the progress checkpoint. When the new map has loaded (`PostLoadMapWithWorld`), it is put back on the new pawn at the arena's start, and saved there so a death respawns in the arena.
- **VFX:** two Fable 5.2 subagents (owner's permission) work in the shared Editor, each on assets only.
  - One makes the plague ball and its impact (`/Game/Glyphbound/FX/Reaper`).
  - The other makes the floor liquid and the eruption (`/Game/Glyphbound/FX/Arena`).

## Implementer exception
Implemented by the architect chat itself (Claude Opus 5.5), not by Muse. The orchestrator's brief called for it because of the deadline, with three sessions sharing one Editor and its restart windows. The VFX went to Fable subagents with the owner's permission.

## Third-party assets used here
- ARPG Pack, halberd set (Fab; already in the project, already listed).
- Slash Trail FX Elemental, SoftTofuVFX (Fab, Professional licence in the owner's library), Nature systems only.
- NiagaraFluids (engine plugin, Epic) for the floor liquid and the plague smoke.
- Tripo (the model, from Angélica Leon's drawings).
