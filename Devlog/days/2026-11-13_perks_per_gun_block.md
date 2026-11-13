# Block: perks per gun (Nov 13, CENTRAL)

> Written by the architect chat of this block (Claude Code, Claude Opus 5.5). To be merged into the day entry by whoever closes the day. Times in UTC.

## The owner's choice
The previous block regrouped the nine existing weapon perks under the three gun pages and kept their effects. Three of them worked on every gun ("All guns"). The architect asked whether each gun should get its own perks instead. The owner picked **"Perks per gun"** (the second option in the end-of-turn box).

## The new weapon tree (`Skills.json`, GDD v0.40)
- **Hub:** Heavy Rounds (+10% gun damage per rank, all guns), unchanged.
- **Every gun page** has its own handling chain:
  - **Quick Trigger:** -8% time between shots of that gun per rank.
  - **Rapid Reload:** -10% reload time, -30% at rank 3. It needs Quick Trigger.
  - **Steady Grip:** -20% recoil. It needs Quick Trigger.
- **Signature chains:**
  - **Pistol:** Deadeye -> Marksman -> Precision Recovery. Marksman keeps its effect on every weapon on purpose: it is the only weak-point perk the staff and the sword have.
  - **Rifle:** Long Shot -> Arcane Conduit. Arcane Conduit is now rifle-only: the gun that lands the most hits builds elemental status fastest.
  - **Shotgun:** Point Blank -> **Tight Choke**. Tight Choke is new and provisional: -10% pellet spread, hip and aimed, per rank, so more pellets land at range.
- **Totals:** 17 weapon perks, 29 nodes across both trees. The weapon tree goes from 36 points to 57. With the 36-point budget, a player can finish about one gun and a half, which is what the owner asked for when the budget was set.
- Old ids that are gone: `Weapon_FireRate`, `Weapon_ReloadSpeed`, `Weapon_Recoil`, `Weapon_EnchantBuildUp`. New ids follow `Weapon_<Gun>_<Perk>`.

## A side effect found with the balance tool, and the fix
The architect ran `Tools/Balance/balance_model.js` before and after the change, with its reference builds moved to the pistol's new perks.

**What changed in the numbers:**
- The old all-gun Quick Trigger and Rapid Reload used the `Weapon.All.*` keys. `AGBHeldItem::GetStat` applies those keys to **every** held weapon, so they had been speeding up the staff and the sword too.
- With per-gun keys that stops. The sword's damage per second in the final build drops from 110 to 84, and the Entity fight grows from 191 to **214 s**, past the approved 160-200 s.

**The fix:** each Entity phase goes from 8,000 to **7,300** health, which brings the fight back to **193 s**. The other encounters stay inside their targets: the Valkyries take 128 and 129 s, against 120 and 120 before.

**Tool change:** the gun specialist build at the full budget swaps the old all-gun Steady Grip and Arcane Conduit for the pistol's Steady Grip and the shotgun's Tight Choke. It costs the same 33 points.

## Smoke
- The node-count check is now 29.
- The perk step unlocks Deadeye before Marksman, since Marksman now needs Deadeye.
- The reload check uses the pistol's Rapid Reload: 0.84 s, which is 0.7 × 1.2.
- The Arcane Conduit check now holds the rifle and gets build-up 7.8, which is 1.3 × 6. After the check it puts the pistol back in hand.

## Review: GPT Sol 6.1 (Codex CLI, medium, read-only, logged through `Invoke-LoggedAgent.ps1`)
Prompt: `Prompts/perks_per_gun_review_sol.md`. **No findings.** Sol checked six things:
- Every node is reachable.
- Every new modifier key is a real stat of that gun.
- Rank stacking matches the smoke checks.
- The point refunds balance.
- The rifle swap leaves the later steps in the state they expect.
- No code, data or tool still names a removed id.

## Checks, once each
- Full smoke: `done=True crashed=False LogGB errors=0`, 60 screenshots.
- Tests: 44 run, 0 failed.
- Builds: the Editor build and the game target build both succeeded.
- **Package** (owner: "Yes, package"), made from `e427900` with a clean tree while holding the machine lock: `TeamTBD_Glyphbound.zip`, **299.7 MB**, BuildCookRun 130 s.
- **Full smoke on the packaged game:** `done=True crashed=False LogGB errors=0`, no LogGB warnings, 60 screenshots, 131 s.
  - The packaged data has the 29 skill nodes.
  - The pistol's Rapid Reload gives 0.84 s, and the rifle's Arcane Conduit gives 7.8 build-up.
  - The shotgun has its 6 meshes, and the Entity loads its real art.
- **Implementer:** the architect, not Muse Spark. This was a data rewrite with a balance check and a few smoke lines, faster to do directly than to hand over.

## Screenshot
- `media/2026-11-13/perkspergun_rifle_page_1080.png`: the Rifle page with its five perks (Long Shot -> Arcane Conduit, Quick Trigger -> Rapid Reload and Steady Grip).

## Needs a person playing
Whether the per-gun split feels fair with 36 points, and whether Tight Choke is noticeable.
