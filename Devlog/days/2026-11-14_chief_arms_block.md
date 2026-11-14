# 2026-11-14, block: the horde chief's arms (shield in the fist, one-handed pistol)

Architect: Claude Code (Opus 5.5) on CENTRAL. **Exception to the default implementer:** the architect wrote the code
himself instead of Muse Spark. Reason: hand-written animation math (two-bone IK, quaternion order across three spaces),
the kind of task where Muse stalled before; small enough to do and check directly.

## What the owner asked

> La postura y las animaciones del jefe orco son muy malas. El escudo es un sinsentido, está por cualquier parte menos en su brazo, y en general las animaciones son extremadamente erráticas.

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
- Not checked: the chief walking and turning, the arm rising during a real aim, the bash in a real fight. A person has to
  play it (PIE is never run by an agent).

Media: `media/2026-11-14/chief_arms_raised_front.png`, `chief_arms_raised_left.png`, `chief_arms_lowered_front.png`
(aiming one-handed), `chief_arms_lowered_left.png`, `chief_arms_bash_front.png`, `chief_arms_bash_left.png`.
