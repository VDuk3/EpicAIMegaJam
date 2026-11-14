# Code review: the horde chief's arms (two-bone IK), shield in the fist, one-handed pistol (Glyphbound, UE 5.9 C++)

You are reviewing uncommitted work in `C:\GameJam` (read-only). Context: `CLAUDE.md`, `Design/GDD.md`. Run
`git diff -- Glyphbound/Source Glyphbound/Content/Data/Enemies.json` to see the change (ignore `Devlog/raw`).

What changed and why: the owner said the chief's posture and animations were very bad, the shield "anywhere but his arm"
and the animation erratic, and asked that he always fire the pistol one-handed, like a revolver. Causes found: the shield
was carried on the capsule while the left arm was bent by a fixed bone-rotation pose that never reached it; every pistol
shot played the Attack clip (a melee swing) over the upper body; the two-handed pistol clips and the Hit clip fought the pose.

The change:
1. `AI/GBEnemyAnimInstance.*`: `FGBArmIK` per arm (left 0, right 1) in the frame handed to the proxy; `ApplyArmIK` in
   the proxy solves a two-bone IK (law of cosines, pole vector, no stretch) on upperarm/lowerarm/hand in component space
   and writes back local rotations; the hand keeps its rotation relative to the forearm or takes a goal rotation; a
   recoil kick turns a shooting arm's aim up. While an arm is driven, `NotifyShot` / `NotifyHit` use the procedural kicks.
2. `AI/GBShieldEnemy.*`: the shield's place stays on the capsule (raised / lowered / pushed by the bash) and the left
   hand's IK goal is its handle; the right arm holds the pistol low at his side or stretched out at the aim point, the
   hand turned so the pistol (`WeaponOffset` relative to `hand_r`) points along the aim; the bash is procedural (pull
   back, drive forward, follow-through) instead of the melee clip; a smoke hold mode 3 (bash strike).
3. `Content/Data/Enemies.json`: the chief's new place stats, `ShieldScale`, the old pose layer removed.
4. `Core/GBDevCommands.cpp`: two bash closeup steps in the Chief smoke block, later steps shifted.

Look for real defects, most severe first:
- Quaternion and transform order mistakes (UE: `FTransform A * B` applies A then B; component rotation = parent's
  component rotation * local rotation; `InverseTransformRotation`), wrong spaces (capsule / actor / mesh component with
  yaw -90 and scale 1.65), anything that would put a hand or the pistol in the wrong place or flip it.
- Thread safety: data read by the animation proxy on a worker thread while the game thread writes it.
- NaN / degenerate cases: zero-length vectors, goal on the shoulder, pole parallel to the reach direction, missing bones,
  a skeleton where lowerarm is not the direct child of upperarm, death and ragdoll with IK still set.
- Behaviour: is the shield still blocking only when raised; does the hit box still match the shield; can the bash push or
  the aim leave the arm in a broken state; do other enemies (that never set arm IK) change at all.
Do not report style. For each finding: file and line, what goes wrong in a concrete case, and the fix.
