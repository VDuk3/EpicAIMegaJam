# Block: three new gun perks chosen by the owner (Quick Draw, Extended Mag, Extended Tube) (Nov 13, CENTRAL)

> Written by the architect chat of this block (Claude Code, Claude Opus 5.5). To be merged into the day entry by whoever closes the day. Times in UTC.

## The owner's choices
After the balance pass (block "general perks and balance"), the architect turned the table's weak spots into three questions with numbers. The owner chose:
- **Shotgun:** Steady Grip, which bought +1% damage per point, becomes **"+2 cartuchos"** ("+2 shells") per rank.
- **Pistol:** Steady Grip (+2% per point) becomes **Quick Draw**: aim 20% faster and move 10% faster while aiming, per rank.
- **Rifle:** add **Extended Mag** as a fifth perk: +5 rounds per magazine per rank.

## Something the question had missed, and the decision taken
The architect measured the three perks before writing them. A bigger magazine also carries **more ammo**:
- **Rifle:** the spare magazines are the bigger size.
- **Shotgun:** the shotgun keeps its reserve as spare magazines x tube size.

So the balance tool's perk table gained a column: the change in the damage of the full load, before the gun runs dry. That is what matters in the boss fights, where the guns run dry.

- **Extended Mag:** +4.8% damage per second per point, and +50% load damage at rank 3. Per point, that is the same as Long Shot's load gain.
- **"+2 shells":** +5.7% per point, and **+100% load damage**, twice any other perk. The +5.7% the owner saw when choosing left the ammo out.
- **The decision:** the architect set **Extended Tube to +1 shell per rank** (6 -> 9): +3.6% per point and +50% load, like Extended Mag. GDD v0.42 marks it as provisional until the owner says otherwise.
- **Encounter lengths:** they do not change (Valkyries 128 / 129 s, Entity 190 s), because the reference builds are pistol builds.

## Review: GPT Sol 6.1, two rounds (Codex CLI, medium, read-only, logged through `Invoke-LoggedAgent.ps1`)
**First round** (`Prompts/new_gun_perks_review_sol.md`): **three real bugs**, all from a magazine whose size now changes while the game runs (a perk bought or reset):
1. After a reset, the gun in hand kept more rounds than fit (for example 9 / 36 in a 6-shell shotgun). A later weapon swap then deleted the excess: up to 15 shells gone at once.
2. A shell-by-shell reload kept the plan of the old size, so its timing and the progress bar no longer matched.
3. The ammo screen on the gun did not redraw its pips when only the size changed.

**The fix (`Weapons/GBGun.cpp`):**
- One rule, `ClampAmmoToCapacity`. Rounds loaded above the size go back to a shell-by-shell gun's reserve (a magazine gun cannot keep loose rounds), then the reserve is capped. `SetAmmo` uses it.
- A new `ReconcileCapacity`, run at the start of every tick. When the size changed, it stops the reload (the shells already loaded stay) and applies the same rule. Keeping the gun in hand or swapping it away now gives the same result.
- The ammo screen's redraw cache includes the size.

**Second round** (`Prompts/capacity_fix_review_sol.md`): the three bugs are closed. Two timing edge cases were left, both fixed:
- A reload started right after buying the perk, before the next tick, was cancelled as a stale one. `Reload()` now settles the size first, and `RefillAmmo()` records it.
- A gun spawned full under a perk that was reset before its first tick kept its ammo above the size. The first look now clamps too, but cancels nothing.

**New smoke sequence, shotgun block ("tube perk"):**
1. Buy Extended Tube 3, refill to 9 / 36, fire down to 7 and start a reload.
2. Check that the reload is running, as Sol asked.
3. Reset the skills mid-reload.
4. Check the gun is at 6 shells, 6 / 24, with no reload and the points restored. It passes.

Without the fix the gun would have stayed at 7 / 36, so the check can fail.

## Checks
- Filtered shotgun smoke after each fix.
- Final, once each:
  - Full smoke `done=True crashed=False LogGB errors=0`, 60 screenshots.
  - Tests 44 run, 0 failed.
  - The Editor build and the game target build both succeeded.
- **Not packaged in this block.**
- **Implementer:** the architect (data, tool, a small ammo rule in the gun, smoke). The fixes came from Sol's two reviews.

## Screenshot
- `media/2026-11-13/newperks_rifle_page_720.png`: the Rifle page at 1280x720. Long Shot -> Extended Mag; Quick Trigger -> Rapid Reload and Steady Grip.

## Needs a person playing
- Quick Draw's feel: aiming is 60% faster at rank 3, and aiming no longer slows the player.
- Whether Extended Tube at +1 feels worth a point.
- The reset with a gun in hand, to see the ammo drop back.
