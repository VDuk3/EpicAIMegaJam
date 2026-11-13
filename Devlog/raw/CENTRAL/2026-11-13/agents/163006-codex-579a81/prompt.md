# Review: weapon perks per gun (data, smoke steps, balance tool) — Glyphbound (UE 5.9)

You are reviewing, read-only. Run `git diff HEAD -- Glyphbound/Content/Data Glyphbound/Source Tools/Balance` and read
what the diff touches. Ignore Devlog and GDD files except where asked below.

What changed (owner: "perks per gun"):
1. `Skills.json`: the weapon tree's shared perks are gone. Every gun branch (Pistol, Rifle, Shotgun; the hub UI from
   the previous commit shows each branch as its own page) gets its own Quick Trigger (`Weapon.<Gun>.FireInterval`
   x0.92 per rank), Rapid Reload (`Weapon.<Gun>.ReloadTime` x0.9) and Steady Grip (`Weapon.<Gun>.RecoilPitch` and
   `RecoilYaw` x0.8). Signature chains: Pistol Deadeye -> Marksman (still `Weapon.All.HeadshotMult`, on purpose:
   it is the staff's and sword's only weak-point perk) -> Precision Recovery; Rifle Long Shot -> Arcane Conduit (now
   `Weapon.Rifle.EnchantBuildUpMult`); Shotgun Point Blank -> Tight Choke (new: `Weapon.Shotgun.Spread` and
   `AimSpread` x0.9). Old ids `Weapon_FireRate`, `Weapon_ReloadSpeed`, `Weapon_Recoil`, `Weapon_EnchantBuildUp` are
   removed; the 22-node count became 29.
2. `Enemies.json`: each Entity phase 8,000 -> 7,300 health, because the old `Weapon.All.FireInterval` /
   `ReloadTime` perks also sped up the staff and the sword (`AGBHeldItem::GetStat` evaluates the gun's id key, its
   slot key and the All key), and the balance tool showed the Entity fight growing from 191 to 214 s (target 160-200).
3. `GBDevCommands.cpp` (smoke): the perk steps use the pistol's new ids; Marksman now needs Deadeye, so Deadeye is
   unlocked first; the Arcane Conduit check gives and equips the Rifle, checks x1.3 build-up, then re-equips the
   Pistol; the node count check is 29.
4. `Tools/Balance/balance_model.js`: reference builds moved to the pistol ids; the gun specialist swaps the old
   all-gun Recoil / Conduit for Pistol Steady Grip and Shotgun Tight Choke (same 33 points).

## What to look for (ranked)
1. Data bugs: a modifier key the game does not resolve (the data subsystem warns about unknown weapon keys at load;
   check that every new key is a real stat of that gun in `Weapons.json`), a requirement on a missing id, a node
   that cannot be reached, a description that does not match its modifier, ops that stack differently from what the
   description says (how do several ranks of a Multiply 0.9 combine? the smoke expects 3 ranks of Rapid Reload to
   give exactly 0.7 x base).
2. Smoke: the rifle swap in the Arcane Conduit step leaving the player in a state later steps do not expect (which
   gun is in the gun slot, ammo, the enchant still on the rifle, the later rifle block calling GiveWeapon("Rifle")
   again on an owned weapon), the reset checks, point arithmetic (points added vs. spent with the new costs).
3. Anything else in code, tests or tools that still names the removed ids or relies on "All guns" perks (for
   example HUD text, tutorials, save data, balance tables).
4. The balance reasoning: is lowering Entity health the right lever for the cause described, given how the tool
   models the fight? Only flag a concrete error.

Do not suggest style rewrites or new features. For each finding give: file:line, what goes wrong, a concrete scenario,
and a fix. If you find nothing real, say so plainly.
