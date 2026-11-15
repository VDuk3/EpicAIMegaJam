# Overnight block C: the castle's pixel-art enemies, with the cyberpunk enemies' squad AI (owner's points 5 and 6)

Read first: `Prompts/implementer_rules.md` (hard rules apply except the build rule: see "Build" below),
`Design/ARCHITECTURE.md`, `Design/GDD.md` §3 (art rule), §10 (enemies; the v0.57 block "How the future's enemies
fight" is the AI level the owner wants in the castle too), `Prompts/sprite_sheet_format.md` (the sprite sheets you load).

## The owner's words (Spanish, translated)
- Point 5: "Orcs: give them combat animations, even simple ones; no static sprites walking at the player like cardboard
  cut-outs. New enemies typical of first-person medieval pixel-art games and 90s RPGs, varied but not too many: big and
  small, some tougher than others, ranged attackers (magic, or physical: arrows, pellets, whatever fits best), each with
  fitting animations and VFX. Final boss of the castle: the pixel-art Valkyrie."
- Point 6: "The castle enemies' AI: the same level of work done for the cyberpunk enemies (reuse that system where you
  can). Each enemy behaves by its combat style (closer or farther). They use cover, gather to attack several at once and
  try to corner the player. They are intelligent and move as a group."

## The decision (architect)
- The castle's characters are 8-direction animated sprite sheets (another agent is baking them right now from 3D
  models: `Glyphbound/Content/Data/Sprites/<SheetId>.png/.json`; sheets: `OrcWarrior`, `OrcBrute`, `OrcShaman`,
  `Barghest`, `CentaurArcher`, `ValkyriePixel`). Until a sheet exists, the enemy keeps today's static text sprite.
- **One AI for both eras.** `AGBSkeletalEnemy` (`AI/GBSkeletalEnemy.*`) already has the squad shooter (positions with
  cover, roles, flanking, attack tokens, peeks, relocation), the melee slots and lunge (with the base class's squads,
  zones, patrols, awareness: `AI/GBEnemyCharacter.*`, `AI/GBSquad.*`, `AI/GBTactics.*`, `AI/GBEngage.*`). Give it a
  second **look**: a sprite look chosen by data (Strings `SpriteSheet` = a SheetId). With the sprite look, everything that
  today drives the skeletal mesh and `UGBEnemyAnimInstance` drives a new `UGBSpriteAnimComponent` instead (clips by
  state, the 8 view directions, the death clip held, hit flash). The castle enemy ids then use
  `ActorClass /Script/Glyphbound.GBSkeletalEnemy` with a sprite sheet, and get the same tactics as the robots and orcs
  of the future. Keep the change contained: one abstraction point (for example a small set of "look" calls the AI
  makes: play attack, notify hit, play death, set moving speed, set aim) implemented for the two looks.
- **Ranged castle enemies fire visible projectiles, not hitscan**: the shaman's arcane bolt (the baked clip
  `FluidArcaneFire` from `Glyphbound/Plugins/GBPixelVfx`, shown in flight like the player's Fireball uses `FluidFire`:
  see how `Spells.json` `FlightVfx` is played) and the centaur's arrows (a pixel arrow sprite; add one to `Sprites.json`
  if none exists). Use the project's one projectile class `AGBProjectile` (`Combat/GBProjectile.*`); the base enemy
  already fires projectiles (`AGBEnemyCharacter`, look for `ProjectileSprite`, `ProjectileSpeed`). A shot leaves at the
  sheet's `HitFrame` from the clip's `Origin` (or the hands' height). Telegraph: the cast/draw frames before it.
- **Melee castle enemies**: orcs swing (Attack clip timed so the damage lands on `HitFrame`), the brute is slow and
  hits hard, the barghest is fast and leaps (the existing lunge, its `Leap` clip). The squad's melee slots and the
  "only N attack at once" rule stay; add what point 6 asks: melee members **close in together** (when the squad has
  two or more melee members ready, they commit within a short window from different slots: a pincer, the player
  surrounded), ranged members keep their band and cover, and the squad pushes the player toward walls/corners (score
  positions that cut the player's open escape directions; keep it simple and data-driven).
- **VFX**: an attack swing shows a short pixel slash (a sprite or a baked clip), hits show the existing hit effects
  in pixel form where one exists; the barghest's leap leaves ember sparks if a clip fits (`FluidFire` small);
  projectiles as above. Simple is fine, but nothing static.

## UGBSpriteAnimComponent (new: `AI/GBSpriteAnimComponent.h/.cpp`)
- Loads `Content/Data/Sprites/<SheetId>.json` + `.png` at runtime (see how `Plugins/GBPixelVfx` loads
  `Content/Data/Vfx/<Id>.png` into a transient nearest-filtered texture: reuse its approach or its subsystem; the game
  module already depends on the plugin's runtime module). Cache per sheet (many enemies share one sheet).
- Draws a quad that faces the camera around the vertical axis only (the sprite stands upright), its pivot on the
  ground point, world size from `PixelsPerMeter`. Material: the plugin's `M_PixelVfxSpriteLit` (lit, so torches light
  them; parameters `Atlas`, `Tint`, `Glow`, `FrameU/FrameV/FrameW/FrameH`, see `GBPixelVfxComponent.cpp`
  `RefreshClipMaterial` and its frame update) so castle enemies are lit by the torches and the moon; check it reads
  well in the dark castle (a small `Glow` floor if needed).
- View direction: from the angle between the actor's forward and the direction to the player's camera, 8 sectors,
  with a few degrees of hysteresis; the column order is in `sprite_sheet_format.md` (counter-clockwise seen from above:
  check your sign with a test).
- Clips: `Play(Clip, bRestart)`, looping or one-shot, `OnHitFrame` event or a query for the AI, `IsFinished`;
  locomotion picks Idle/Walk/Run by speed (thresholds from data); Hurt as a short overlay that returns to locomotion;
  Death held on its last frame. Missing clip: fall back (Attack2 -> Attack, Run -> Walk, Cast/Shoot -> Attack, Leap ->
  Attack, Hurt -> none).
- A pure helper for the direction sector and the frame-at-time math, with a test `Glyphbound.SpriteAnim.Direction`
  (`Tests/GBSpriteAnimTests.cpp`) that a wrong sign or an off-by-one sector would fail.

## Data (`Content/Data/Enemies.json`, castle ids)
Keep the existing ids (encounters use them) and give them sheets and the shared class; add the new ones. Numbers
from the balance templates of GDD §19 (read it; the past is "mostly melee and swarm"), all provisional:
| Id | Sheet | Role |
|---|---|---|
| `Orc_Past` | OrcWarrior | melee grunt (template 60 HP) |
| `Orc_Past_Champion` | OrcWarrior (tinted / scaled 1.1) | tougher melee |
| `Orc_Past_Shaman` | OrcShaman | ranged magic, keeps 800 to 1500 cm, uses cover |
| `Orc_Past_Brute`, `Orc_Past_Juggernaut` | OrcBrute | big, slow, hits hard, a lot of HP (the castle's tank) |
| `Imp_Past` -> keep the id, display "Barghest" | Barghest | small, fast, fragile, leaps, comes in packs |
| `Centaur_Past` (new) | CentaurArcher | big ranged physical (arrows), keeps distance, high HP |
| `Valkyrie_Past` | ValkyriePixel | the castle's boss: leave her data pointing at the sheet; her fight is another agent's work (class `AGBValkyrieEnemy`), do not build her AI |
Sizes: the capsule from the sheet's world size (a 2.1 m orc, a 1.3 m tall barghest, a 2.6 m centaur...).

## Tests and smoke
- The pure test above.
- A smoke step `gb.Smoke Castle` (`Core/GBDevCommands.cpp`, a new block; read how the `Enemies3D` block is written):
  in the test arena spawn one of each castle id, check each loaded its sheet (or logs why not), moves, plays its
  attack and hits the player (god mode as the other blocks do), a ranged one fires a projectile that flies, the squad
  gets positions with cover for the ranged members and two melee members commit together, then kill them and check
  the death clip holds and the corpses go. Screenshots along the way. `Smoke-Jam.ps1 -Only Castle -Offscreen` must
  run it (check how `-Only` maps to steps).

## Files you own
New: `AI/GBSpriteAnimComponent.*`, `Tests/GBSpriteAnimTests.cpp`. Shared (Edit tool only, never rewrite whole files;
another agent may be adding a `virtual` keyword or a protected accessor in the same headers at the same time):
`AI/GBSkeletalEnemy.*`, `AI/GBEnemyCharacter.*`, `AI/GBSquad.*`, `AI/GBTactics.*`, `Core/GBDevCommands.cpp` (your new
block only), `Content/Data/Enemies.json` (castle ids only), `Content/Data/Sprites.json` (an arrow sprite if needed).
Do not edit `AI/GBValkyrieEnemy.*` (another agent's), the map builders, or other data files without saying so.

## Build
You may build: take the machine lock (`Glyphbound/Saved/machine.lock`: one line, who, what, UTC time; if another
agent holds it and it is younger than 45 min, wait and do other work; delete your line when done; an open
`UnrealEditor.exe` also blocks the build: never close it yourself, report and wait). Iterate cheaply (owner's rule):
Editor target only, `powershell -File Tools/Jam/Build-Jam.ps1`; tests with an anchored filter
(`Test-Jam.ps1 -Filter '^Glyphbound.SpriteAnim.'`); smoke `Smoke-Jam.ps1 -Only Castle -Offscreen`. No full smoke, no
game-target build, no package, no commit: the architect does those once at the end. Look at the screenshots yourself.

## Final report
As in `implementer_rules.md`, plus: the look abstraction you chose (where the two looks branch), what the squad does
for "attack together" and "corner the player", the smoke output lines, what is still weak.
