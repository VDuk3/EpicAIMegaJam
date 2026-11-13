# Review: three new gun perks (Quick Draw, Extended Mag, Extended Tube) — Glyphbound (UE 5.9)

You are reviewing, read-only. Run `git diff HEAD -- Glyphbound/Content/Data Glyphbound/Source Tools/Balance` and read
what it touches, plus `Weapons/GBGun.cpp` (ammo: `GetMagazineSize`, `GetMaxSpareMagazines`, `AddSpareMagazines`,
`SetAmmo`, `RefillAmmo`, reload, round-by-round reload with `FGBPerRoundReload`, aiming with `AimTime` and
`AimMoveSpeedMult`), the HUD's ammo readouts (`UI/`), and the equipment / pickups code that stores or restores ammo.
Ignore Devlog files; read `Design/GDD.md` only for the new "Weapon tree, v0.42" entry.

What changed (owner's choices, balanced by the architect):
1. `Skills.json`: pistol Steady Grip replaced by Quick Draw (`Weapon.Pistol.AimTime` x0.8 and
   `Weapon.Pistol.AimMoveSpeedMult` +0.1 per rank); rifle gets Extended Mag (`Weapon.Rifle.MagazineSize` +5 per
   rank); shotgun Steady Grip replaced by Extended Tube (`Weapon.Shotgun.MagazineSize` +1 per rank). 30 nodes.
2. The balance tool shows the ammo effect of magazine perks; reference build updated.
3. Smoke node count 30.

## What to look for (ranked)
1. **Magazine size that changes while the game runs** (a perk unlocked, or reset with "Reset skills", while the gun
   holds rounds): `InMagazine` above the new size after a reset, the shotgun's reserve (kept in rounds, capped by
   magazines x tube size) losing or creating shells when the size shrinks or grows, the round-by-round reload's
   cycle time (`ReloadTime / capacity`) changing mid-reload, a reload that never ends or loads past capacity, the
   ammo display / pips, the HUD reserve text. Give the concrete sequence and what the player would see.
2. Stored ammo: guns kept in the inventory or restored at a checkpoint with a size from before a perk change.
3. Quick Draw: `AimMoveSpeedMult` reaching 1.0 or more (does anything assume < 1?), `AimTime` near 0 (division).
4. Data: keys, reachability, descriptions that match the modifiers.

Do not suggest style rewrites or new features, and do not judge the design. For each finding give: file:line, what
goes wrong, a concrete scenario, and a fix. If you find nothing real, say so plainly.
