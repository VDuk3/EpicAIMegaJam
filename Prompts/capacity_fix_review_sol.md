# Re-review: magazine capacity changing at runtime — Glyphbound (UE 5.9)

Your previous review found three issues when Extended Mag / Extended Tube change a gun's magazine size while the
game runs (a reset leaving live guns over capacity and a swap then deleting ammo; a round-by-round reload keeping its
old-capacity plan; the gun's ammo screen not redrawing its pips). Read-only: run `git diff HEAD --
Glyphbound/Source/Glyphbound/Weapons Glyphbound/Source/Glyphbound/Core/GBDevCommands.cpp` and read around it.

The fix: `AGBGun::ClampAmmoToCapacity` is the one rule (loaded rounds over capacity go back to a round-by-round gun's
reserve; a magazine gun drops them; then the reserve is capped) used by `SetAmmo` and by `ReconcileCapacity`, which
runs at the start of every `Tick`: when the capacity differs from the last tick it cancels the reload and clamps.
The ammo screen's redraw cache now includes the capacity. A new smoke sequence (shotgun block, "tube perk") buys
Extended Tube 3, fills 9 / 36, starts a reload from 7, resets the skills mid-reload and checks 6 / 24, no reload,
points restored; it passes.

Check: does the fix close each of your three findings? Does it open new problems: guns that do not tick while stored
(is their ammo restored through `SetAmmo` on equip?), the first tick after spawn or equip (`LastKnownCapacity`),
`RefillAmmo` not updating `LastKnownCapacity` (a refill followed by a needless reconcile that cancels a reload just
started?), `CancelReload` side effects (pose blend, `bFireWhenLoaded`) firing on a tick where nothing visible
changed, the cost of `GetMagazineSize()` (a stat evaluation) every tick. For each finding give file:line, a concrete
scenario and a fix; if the fix is sound, say so plainly.
