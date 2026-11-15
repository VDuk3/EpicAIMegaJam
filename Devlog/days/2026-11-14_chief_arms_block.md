# 2026-11-14, block: the horde chief's arms (shield in the fist, one-handed pistol)

Architect: Claude Code (Opus 5.5) on CENTRAL. **Exception to the default implementer:** the architect wrote the code
himself instead of Muse Spark. Reason: hand-written animation math (two-bone IK, quaternion order across three spaces),
the kind of task where Muse stalled before; small enough to do and check directly.

## What the owner asked

> La postura y las animaciones del jefe orco son muy malas. El escudo es un sinsentido, estÃ¡ por cualquier parte menos en su brazo, y en general las animaciones son extremadamente errÃ¡ticas.

Translation: The orc chief's posture and animations are very bad. The shield makes no sense, it is anywhere but on his arm, and overall the animations are extremely erratic.

After the diagnosis below and the plan, the owner said go ahead, and added:

> Y que siempre dispare con una mano la pistola, como si fuera un revolver

Translation: And he should always fire the pistol with one hand, like a revolver.

## Diagnosis (code and the smoke screenshots, before any change)

- The shield was carried on the **capsule** at two fixed places, never on the hand; the left arm was bent by a fixed
  two-bone rotation layer (`upperarm_l:-60,0,0`) that did not know where the shield was. Screenshots
  `orc_refit_game_chief.png` and `chief_shield_raised_side.png` show the arm stretched sideways and the shield floating.
- **Every pistol shot played `A_Orc_Attack_01`, a melee swing**, over the upper body (`NotifyShot` plays the Attack
  clip when there is one). The shield bash played the same swing over the whole body, pistol in hand.
- The locomotion clips (`A_Orc_KB_Pistol_*`) hold the pistol two-handed; the pose layer and the Hit clip were applied
  on top of them, and the shield toggles every few seconds: three layers fighting each other.

## Decision and what was built

No shield clips exist in the project, so the arms are procedural (architect's call):

- `AI/GBEnemyAnimInstance.*`: a generic **two-bone arm IK** (`FGBArmIK`, left and right) solved last in the native
  proxy: law of cosines, pole vector, no stretch, written back as local rotations. The hand keeps its rotation relative
  to the forearm or takes a goal rotation; a recoil kick turns a shooting arm's aim up. While an arm is driven, shots and
  hits use the procedural kicks, never a clip. Enemies that set no IK are unchanged.
- `AI/GBShieldEnemy.*`: the shield's place stays on the capsule (raised before the chest, lowered at the left hip,
  pushed forward by the bash) and **the left hand's IK goal is its handle**, so the shield is always in the fist. The
  right arm holds the pistol **one-handed**: low at his side while he walks behind the shield, stretched out at the player
  in the aim and the moment after the shot, the hand turned so the pistol points along the aim (`WeaponOffset` composed
  back). The bash is procedural: pull back, drive forward, follow-through, a small forward lean.
- `Enemies.json` (chief): new place stats (`ShieldHandle*`, `ShieldLowered*`, `ShieldGripBack`, `BashPull`,
  `BashPush`), `ShieldScale` 1.45 (the shield 1.67 m on a 3.2 m chief, about half his height), the old pose layer removed.
- Smoke: two bash closeups in the Chief block (hold mode 3).

Correction to what the owner was told in the plan: I had said the shield would shrink to about 1.1-1.2 m. Measured
against the chief (3.2 m tall at scale 1.65) that would look like a buckler; it went from 1.9 m to 1.67 m instead.

## Review (Sol 6.1, medium, through Codex CLI)

Prompt `Prompts/chief_arms_review_sol.md`. No quaternion-order or threading defects found. Two findings, both fixed:
1. The hit box stayed on the raised place while the bash pushed the shield 66 cm forward: it now follows the shield's place.
2. The bash could push the handle out of the arm's reach and open a gap between fist and shield: the push is now clamped
   so the grip stays within 95 % of the left arm's reach (measured on the skeleton each frame).

## Checks

- Editor target build: succeeded. `Smoke-Jam.ps1 -Only Chief -Offscreen`: done, 0 errors, 91 s, six closeups.
- Full `Smoke-Jam.ps1 -Offscreen`: done, 0 LogGB errors, 0 engine errors, 392 s.
- **Game target build: failed**, on another chat's unfinished HUD work in the shared tree (`AGBHUD::SetHandOverlay`
  protected, called from `GBDevCommands.cpp`), not on this change. Only this block's hunks are committed.
  **Update:** once that chat finished its HUD work, `Build-Jam.ps1 -Target Glyphbound` succeeded (44 s, exit 0) on
  the tree that holds this change.
- Not checked: the chief walking and turning, the arm rising during a real aim, the bash in a real fight. A person has to
  play it (PIE is never run by an agent).

## Second pass: the fist still left the shield

The owner played it and sent a screenshot (from his left, shield raised):

> aca me parece que el escudo estÃ¡ separado de la mano aun

Translation: here it looks to me like the shield is still apart from the hand.

Measured, not guessed: a temporary log in the smoke printed the left hand's bones in the shield mesh's own space. The IK
put the **wrist** on the handle and let the clip (a two-handed pistol clip) turn the hand. Raised, the fingers happened to
lie along the shield's back. Lowered, they pointed away and the fist hung about 20 cm off it. The shield mesh's origin
was also not where the first pass assumed: the back face sits at x = -3.5, and the spikes reach x = +46.

Fix (`GBShieldEnemy.cpp`, `UpdateArms`): the hand takes one fixed turn in the shield's space. The fingers run along its
back, toward his body and a little down. The knuckle line runs into the shield (index at the plate, pinky away). The
**palm's middle**, not the wrist, goes on the handle. The fingers' place is read in the hand bone's own frame, which no
turn of the hand changes. `ShieldGripBack` is now the palm's distance behind the origin (8).

Review (Sol 6.1, medium; prompt `Prompts/chief_grip_review_sol.md`) of the first version of this fix. That version bent
the fingers along the forearm's line, measured on last frame's pose. Two findings, both fixed by the design above:
1. That feedback from one frame to the next can settle into a two-frame wobble of about 15 cm. Sol reproduced it
   numerically.
2. Near a pose where the forearm is parallel to the shield's axis, it fell back to the wrist-only grip with a snap.

The final design has neither, because nothing in it depends on the forearm or on the frame before.

Checks: editor build, `-Only Chief -Offscreen` (0 errors; the fist is on the shield's back in the raised, lowered and bash
closeups) and the game target build all pass. The full `Smoke-Jam.ps1 -Offscreen` passed (387 s, 0 errors) on the first
version of the fix. The final change touches only the chief's grip, which the Chief steps cover.

Coordination: the Editor was open in the chat rebuilding the castle (Live Coding blocks a build). At the owner's request
the two chats agreed by message: that chat closed it with `Stop-EditorMcp.ps1`, this one built and ran the smoke, then
handed it back.

Media: `media/2026-11-14/chief_grip_raised_left.png`, `chief_grip_bash_left.png` (after the fix).

Media (first pass): `media/2026-11-14/chief_arms_raised_front.png`, `chief_arms_raised_left.png`, `chief_arms_lowered_front.png`
(aiming one-handed), `chief_arms_lowered_left.png`, `chief_arms_bash_front.png`, `chief_arms_bash_left.png`.
