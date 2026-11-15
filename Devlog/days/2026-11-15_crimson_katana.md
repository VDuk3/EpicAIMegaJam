# 2026-11-15 - The Crimson Katana: a second sword for the future

The owner asked for a second sword so the end-game Elemental Sword is no longer the only one: a sci-fi katana from Fab, dropped by an enemy in the cyberpunk world (`FutureBase`), with the same melee logic as the existing sword, physical damage, and the weapon enchant that the guns already take. Done by a side session started by the orchestrator (Claude Code, Opus 5.5). Implementer exception: small, spread over existing files and in a hurry at the end of the jam, so the side session wrote it directly instead of briefing Muse.

## Directing: what the owner decided
- The model: the Fab listing he chose (below), shown to him in close-up renders before any in-game use (his rule). Renders: `Glyphbound/Saved/KatanaReview/` (flat colors and lit).
- Where it is found: an enemy in the future drops it, kept simple and data-only.
- How it fights: like the existing sword (`AGBSword`, the `Sword` slot), physical damage, and it can be enchanted.

## How it was built (architect's decisions)
- **Data, not a new class.** `Weapons.json` gets `CrimsonKatana` (slot `Sword`, element `Physical`, damage 40 every 0.45 s, the sword's range and arc). It spawns as an `AGBSword` like the Elemental Sword.
- **Shown in 3D, like the guns, not as a pixel sprite.** The cyberpunk world's guns are 3D view models without hands (owner's rule, 2026-11-14), so the katana follows them: `Strings.ViewMesh` names its mesh and `AGBSword` builds a first-person view model from it (fixed weapon FOV, self-shadow only). Its own motion: a fast cut down to the left pivoting on the grip and a slower return (`SwingTime`). Pose numbers (`ViewScale`, `ViewOffsetX/Y/Z`, `ViewPitch/Yaw/Roll`, `ViewGripX`) are data, provisional until the owner sees it in PIE. The HUD draws no pixel blade for a sword that has a `ViewMesh`.
- **Swords take the enchant (both of them).** The enchant spell was gun-only. Now a sword in hand can cast it too (input, spellbook check, HUD hint), the hit takes the enchant's element and on-hit effect while it lasts, and the three `Enchant_*` status effects multiply `Weapon.Sword.Damage` by 1.25 as they already did for `Weapon.Gun.Damage`. Done in `.cpp` files and data only, so no Editor restart. The katana shows a light in the enchant's color on its blade; the Elemental Sword's pixel blade takes the enchant's tint.
- **Who drops it.** The Armored Orc (`Orc_Future_GunCarrier`), the one enemy that is always in the first fight of the future (`Opening_Future`) and already drops the first pistol. It now also drops the katana (chance 1). No map edit.
- **On the floor** the dropped katana shows its own model, lying flat, instead of the staff sprite other non-gun weapon pickups use.

## Checks
Live Coding compile clean; own review of the code. No smoke or tests (owner's rule for the final stretch). PIE by a human: kill the Armored Orc in the first fight of the future, pick up the katana, switch to the sword slot (or equip it from the inventory if the Elemental Sword is already held), swing, then cast the enchant (T) with it in hand.

## Third-party assets used here
- **red scifi katana** by M@xim, Fab (https://www.fab.com/listings/ec6f2549-f8f9-4805-981b-534868d14aee), licence **CC-BY** (free; attribution: "red scifi katana" by M@xim, Fab). Installed through the Editor's Fab toolset (FBX) and moved to `/Game/Glyphbound/Weapons/Katana/` (mesh renamed `SM_Katana`).
