# Game Design Document (working draft)

> Epic AI MegaJam 2026 · Theme: **DIRECTOR'S CUT** · Unreal Engine 5.9.0
> Status: **v0.20, 2026-11-12** — owner's vision plus eleven rounds of answers. Anything marked **TBD** is not decided yet; see "Open questions" at the end.
> Audience: **the architect** = whichever Claude Code chat is currently implementing the game. There is normally only one at a time, and it is replaced when its context fills up or its tasks are done. This document is the source of truth that every new architect chat reads first. Implementation is delegated as `CLAUDE.md` says ("Implementador").
> Project code name: **Glyphbound** (provisional; the game title is still TBD). Written in English because it is part of the public dev log (Glass Box) and the jam requires English.

## Changelog
- v0.20 (2026-11-12): owner: six new spell-tree perks (one per spell that had none); the flamethrower runs while its key is held, until the key is released or the mana runs out.
- v0.19 (2026-11-12): owner: when ground spells overlap, the one placed last covers the others completely, and they never flicker.
- v0.18 (2026-11-12): owner asked for a "+1 skill point" button on the skill tree screen, to test perks.
- v0.17 (2026-11-12): owner, after playing: the pixel-art figure of the lightning rune and the ice floor must cover the whole area of the spell, lying on the ground, not a small sprite in the middle.
- v0.16 (2026-11-12): owner asked for a way to reset the perks from the skill tree screen, to test them better. All spent skill points come back.
- v0.15 (2026-11-12): owner's second round of play-test notes. Jumps commit to their direction (no steering in the air; the second jump can pick a new direction and then commits too); the sword hits where the player aims, not the center of the target; sword hits and the staff's basic attack get the head / weak point bonus.
- v0.14 (2026-11-12): owner's first play-test. Magic structures (ice wall) are 3D with pixel textures; the fireball always explodes with a shockwave; both jumps higher; picked-up staffs go to the inventory and are equipped by hand; the ice shield is summoned by the spell and raised with a Block key; enemies must path around obstacles; menus work with the mouse; falling out of the map kills; floating damage numbers.
- v0.13 (2026-11-12): architect's day-1 technical decisions recorded in 0.1 (C++ only, no GAS, JSON data, code-built test arena, canvas HUD); new section 18 lists every provisional rule and number the architect had to pick. Code structure in `Design/ARCHITECTURE.md`.
- v0.12 (2026-11-12): architect autonomy on every technical decision; iteration time is a priority (see `CLAUDE.md`).
- v0.11 (2026-11-12): the 3 s QTE limit covers the whole QTE.
- v0.10 (2026-11-12): QTE lasts 3 s and the player keeps moving in the direction they were moving; CENTRAL has an NVIDIA RTX 4080 Laptop GPU, so DLSS can be tested here.
- v0.9 (2026-11-12): code name Glyphbound; pixel-art look defined (3D geometry with low-res textures, 8-angle sprites); new §0.1 technical direction (C++, placeholders, PIE rule, data tables, travel fade, checkpoint-only saving, single player, size budget, NVIDIA award); camera locked during the QTE.
- v0.8 (2026-11-12): enemies do not respawn; provisional game order; story questions parked; open questions narrowed to what the prototype and base systems need.
- v0.7 (2026-11-12): full story (the entity, the false voice, the Valkyrie); elemental zones in the castle unlock the rest of each element; air passives learned the same way; shotgun and rifle dropped by enemies; era travel restores everything (bonfire-like); remaining keys confirmed.
- v0.6 (2026-11-12): art rule clarified for characters (style of the world they belong to); story premise; opening sequence with the first gun; starting spells; enchant only with a gun in hand; key changes (F quick-use, G interact, L1 enchant on gamepad); counter-style ammo refilled on era travel; automatic checkpoints; no sprint; one full-armor slot.
- v0.5 (2026-11-12): sword is a third slot; magazines, reload, limited ammo dropped by enemies; health and mana regenerate, no stamina; state carries across eras; checkpoints; one skill point pool; controls filled in; opening sequence; armor from chests; difficulty = hard only for now.
- v0.4 (2026-11-12): weapon skill tree contents; two held slots (one gun + one staff) and how to switch; no spells with a gun except the enchant; enchant key; air passives always on; ice shield QTE; staffs from chests; sword blade is pixel art with a medieval HUD; final boss mixes both styles.
- v0.3 (2026-11-12): art rule follows the object; the player is always pixel art; 5-spell loadout and controls; staffs and basic magic attack; gun aim; enchant has a QTE, player-chosen element, not on the sword; QTE grading and mana; two UI styles (90s RPG vs. modern cyberpunk) for HUD and skill trees.
- v0.2 (2026-11-12): theme reading, art rule, shared map with era-gated zones, cross-era style mixing, QTE = power + reach, weapon enchant spell, skill points from encounters, enemy waves and sub-bosses, no early melee, architect definition.
- v0.1 (2026-11-12): first dump of the owner's pitch.

## 0. Baseline
At its core this is a **classic first-person RPG plus a classic first-person shooter**. Anything not stated here behaves as players expect from those two genres (magazines and reload, ammo pickups, standard shooter gamepad layout, etc.). This document only lists what is specific to our game.

## 0.1 Technical direction
- **Engine:** Unreal Engine 5.9.0, pinned until submission. **Single player only.**
- **Architect autonomy:** nobody on the team is a programmer. **Every technical and engineering decision is the architect's**, taken without asking the owner: code architecture, C++ vs. Blueprints, plugins, tools, data formats, optimization. Decide, build, and log the decision and its reason in the dev log. When a design detail is missing, pick a reasonable provisional value, mark it provisional here, and keep going; ask the owner only when the game's vision would change.
- **Time first:** iteration speed matters more than elegance. Unused plugins off, Live Coding for `.cpp` changes, only the touched tests on each change, the full suite (under 30 s) once before the daily package. Rules in `CLAUDE.md` ("Tiempo de iteración").
- **Code (decided by the architect, 2026-11-12):** C++ only, one module, no Blueprint graphs; structure in `Design/ARCHITECTURE.md`. **Unreal's Gameplay Ability System is not used**: a small stat-and-modifier system, one damage function and one status-effect component cover the same needs with far less setup.
- **Tuning data (decided by the architect):** every gameplay number lives in text files, `Glyphbound/Content/Data/*.json` (spells, weapons, enemies, items, skills, player, QTE...). They ship as loose files: the owner can change numbers in a packaged build with a text editor, and `gb.ReloadData` in the console reloads them while playing. This replaces the data tables suggested earlier (git only carries text in this team, and AI agents edit text reliably).
- **Test level (decided by the architect):** the day-1 test arena is built in code on the engine's empty map, so no binary map asset is needed yet. Real levels come later from the level designers.
- **Placeholders first:** pixel art and 3D models are made by other team members on machines on the local network. **The architect never waits for art:** it uses quickly generated placeholders and swaps them later.
- **Placeholder numbers:** the architect puts provisional values (damage, mana, cooldowns, QTE length and time limit…) in data tables; the owner tunes them later.
- **PIE is human-only:** Play In Editor is always run by a person. AI agents do not run PIE; they compile, package and run tests, then ask a human to play-test. (Also in `CLAUDE.md`.)
- **Era travel:** a fade with an effect when crossing a portal; the transition hides loading the other era's version of the map.
- **Saving:** checkpoints only. No save to disk between sessions.
- **Size:** the jam page says "keep it under 1 GB" for desktop zips, but **bigger builds are allowed** if uploaded with butler (installed on CENTRAL) or Blendo Itch Uploader. The Official Rules PDF sets no size limit. So 1 GB is a target, not a hard limit.
- **NVIDIA RTX Award:** we intend to compete. It is the "best Render Royalty entry built in Unreal Engine" that uses at least one of: an NvRTX feature (RTX Mega Geometry, RTX Direct Illumination, ReSTIR GI, ReSTIR PT path tracing, LSS Hair), **DLSS 4.5 (Unreal Engine)**, or NVIDIA ACE; plus the NVIDIA questions at submission and listing the tool in "Tools you used".
- **Modifier rule (Official Rules §10):** a team can win **only one modifier** (Glass Box, Render Royalty, Best Team…) unless no other eligible winner remains. We still enter all that apply.

## 1. One-line pitch
A fast first-person **shooter/RPG hybrid**: the player, a medieval mage, jumps through fixed portals between a **pixel-art medieval past** full of magic and a **3D cyberpunk future** full of guns, on the same map in two times, and wins by **mixing spells and firearms** rather than sticking to one style.

Working title: **TBD**. Project code name: **Glyphbound**.

## 2. Pillars
1. **Two eras, one fight.** Past = magic, pixel art. Future = guns, 3D cyberpunk. Both feed the same character.
2. **Combos over specialization.** Spells and guns can be used in both eras, and the game rewards combining them. Weapon and spell mechanics have synergies.
3. **Fast and dynamic combat:** spells, ranged spells, shooting, and melee once the elemental sword is unlocked.
4. **Magic feels crafted, not triggered:** casting uses a two-part slowed-time quick-time event that scales the spell (see §7).

## 3. Art rule (never broken)
- **Everything medieval, wizards and magic is pixel art (2D).**
- **Everything cyberpunk and futuristic is 3D.**
- **The rule follows the object, not the era.** A 3D gun stays 3D in the past; a pixel-art fireball stays pixel art in the future.
- **Characters take the style of the world they belong to.** An orc of the castle is a classic pixel-art fantasy orc (80s, 90s, early 2000s games). An orc of the future is a 3D, heavily armored sci-fi orc. They are different enemies, not the same orc in two styles.
- **The player is a medieval mage, so the player is always pixel art, hands included.** In first person, the hands are always pixel art, even when they hold a 3D gun.
- No exceptions.

### The pixel-art look
- **Visual reference:** retro first-person fantasy shooters with modern lighting (the owner's reference: *Wizordum*). Reference only, nothing copied.
- **Pixel-art environments are 3D geometry with low-resolution pixel textures** (nearest filtering, no smoothing), not flat 2D.
- **Pixel-art characters are sprites that always face the camera.** Target: **8 view angles**; fewer if time runs short.
- **Headshots on sprites:** the head is the top part of the sprite. The head / weak point bonus applies to **every aimed weapon attack** (owner, 2026-11-12): guns, the staff's basic attack and the sword. Each weapon has its own multiplier.
- **Ground spells are drawn on the ground** (owner, 2026-11-12): a spell that marks an area of the floor (lightning rune trap, ice floor) shows its pixel-art figure lying flat on the ground and stretched over the whole area it affects, so its size on screen is its real reach. It is not a small camera-facing sprite in the middle of a colored disc.
- **Structures are never flat sprites** (owner, 2026-11-12). Anything that is a structure, the castle and also what magic builds (the ice wall), is a 3D object with 2D pixel textures. Camera-facing sprites are for characters, items, projectiles and effects (flames, bolts).

## 4. World and travel
- **One map, two times** (past and future). The player travels between them **only through predefined portals** placed on the map.
- **Era-gated zones:** some zones are reachable in one era and not in the other.
- **Cross-era unlocks:** some zones must be unlocked in the past so they open in the future (cause in the past, effect in the future).
- These interactions are few (short jam) but they exist and are part of level design.
- **Nothing is lost when changing era.** Health, mana, ammo, inventory, spells and weapons all carry over. Mana exists and works in the future; guns work in the past.
- **Era travel restores everything** (health, mana, ammo), a bit like the rest points of classic action RPGs. Portals are the safe points of the map.

- A third, separate map unlocks late: a **dimension between space and time** where the true final boss lives (see §11).

### Story
- The player is a **mage** who travels to the castle out of **curiosity**: he had heard it was a mysterious place.
- On reaching the **seal** in the castle he is **trapped** and thrown into the future.
- In the future he hears **a woman's voice asking for help**. That voice is his reason to fix the problem.
- **The twist:** the voice is **not the Valkyrie**. It is **the Entity**, the true final boss, pretending to be her.
- **The Entity:** a fusion of an **apocalyptic artificial intelligence** and a **ghost that embodies disease**. It is locked in the dimension between space and time and wants out. To escape it needs two things: someone to **open the portal** and a **physical body to possess**.
- **The Valkyrie:** someone the Entity **manipulated across time**. The plan failed, and she ended up **trapped in both eras** (runes in the past, cryogenic tube in the future).
- **What the player does:** defeats the Valkyrie in both eras, gathers what is needed, and opens the portal: **exactly the Entity's plan**. The player walks in with the body it wants to possess. The only thing the Entity did not plan for: **the player beats it**.
- Cutscenes: **few and short**, each one written for its situation.

### Opening sequence
1. The player starts in the **medieval castle** (past) with a **staff and the starting spells** (§6).
2. Kills a **small group of pixel-art orcs**.
3. **Interacts with the seal** → **short cutscene #1** (trapped, sent forward in time).
4. Appears in the **future**.
5. Fights **3D armed orcs**. One of the first ones **drops the first gun**.
6. The player **picks up the gun** → **short cutscene #2**. The player **cannot progress** without picking it up.
7. Back to the **castle**.
8. **Elemental zones**, in the order the player chooses (§6).
9. **Valkyrie, past.**
10. **Valkyrie, future.**
11. **Final portal** → the Entity (§11).

Steps 7–11 are **provisional** (owner, 2026-11-12).

## 5. Player progression
- **No character levels / no XP leveling.**
- Progression is through **skill trees**: one for spells and one for weapons (see §13 for their look).
  - **Spell tree:** upgrades spells.
  - New perks (owner, 2026-11-12), each **1 skill point per rank, 3 ranks**:

    | Spell | Perk | Effect per rank |
    |---|---|---|
    | Fire wall | Lingering Flames | +15% wall duration |
    | Flamethrower | Intense Heat | +12% damage |
    | Lightning rune | Expanded Sigil | +10% blast radius |
    | Static orb | Persistent Charge | +15% orb duration |
    | Ice shield | Enduring Guard | +15% maximum shield duration (Glacier Wall stays the "toughness" perk) |
    | Ice floor | Deep Freeze | +5 percentage points of slow, with a cap so enemies are never fully stopped |
  - **Weapon tree:** damage, fire rate, **recoil** (better aim), **damage by distance** (shotgun stronger up close, rifle stronger far away), **headshot damage**.
- **One shared pool of skill points** for both trees.
- **Perks can be reset** (owner, 2026-11-12, to test them): a "Reset skills" option on the skill tree screen removes every unlocked node and gives back all the points spent on them. Provisional details in section 18.
- **Skill points** are given at specific points, mainly for defeating enemies:
  - defeating each **sub-boss**;
  - defeating each **boss**;
  - **clearing a wave** of enemies.

## 6. Magic (4 elements + weapon enchant)
Each element has 3 entries. Air is passives only; the other three are active spells.

| Element | 1 | 2 | 3 |
|---|---|---|---|
| **Fire** | Fireball (projectile; **always explodes with a shockwave**, on an enemy, on the world or at the end of its range) | Fire wall | Flamethrower (sustained DPS; **runs while the spell key is held**, until it is released or the mana runs out) |
| **Lightning** | Lightning bolt (projectile) | Lightning rune trap on the ground; triggers when an enemy walks over it | Static-electricity ball: moves and damages every enemy around it |
| **Ice** | Ice shield (**summoned by the spell, then raised by holding the Block key**, like the shield of a first-person RPG; it only protects while raised; cooldown and limits) | Ice wall | Ice floor: slows enemies, light crowd control |
| **Air** (passives) | Double jump | Improved dash | Increased movement speed |

### Spell loadout
- The player has **5 spells equipped** at a time and can **equip and unequip** them freely.
- **Air passives are always active**: they are not equipped and do not take a slot.
- **Starting spells:** Fireball (very early tier), Lightning bolt (early tier), Ice wall.

### Learning the other spells: elemental zones
- The **medieval castle** has **elemental zones**, one per element.
- Each zone has a **challenge**: a wave, a sub-boss or a boss.
- After the challenge the player gets the element's **knowledge** (from a chest, a book or similar) and **unlocks the missing spells of that element** (fire: fire wall + flamethrower; lightning: rune trap + static ball; ice: ice shield + ice floor).
- **Air passives** are learned the same way, in the air zone.
- The spell tree then upgrades what the player knows.
- **Spells can only be cast with the staff in hand.** With a gun in hand, the only spell available is the weapon enchant. To cast, the player switches to the staff mid-fight.
- Keys: see §9.

### Ice shield (owner, 2026-11-12)
- Casting it (with its QTE) **summons** the shield for a time. It is not a force field in front of the player.
- The player **raises it by holding a secondary key** (Block) and lowers it by letting go. It protects from the front **only while raised**.
- Provisional details (architect): Block is **Q** on keyboard and **L2** on gamepad; it needs the staff or the sword in hand (with a gun, L2 is aim); while raised the player moves slower and cannot attack or cast; the shield has health and breaks when it runs out; other spells can be cast while it is summoned and lowered.

### Weapon enchant (always equipped)
- A spell that **cannot be unequipped**.
- **Only usable with a gun in hand** (it enchants the gun).
- Animation: the **left hand draws a magic glyph over the weapon held in the right hand**.
- Effect: the weapon is **temporarily buffed with magic** of one element.
- **Element:** chosen by the player when equipping the spell.
- **Uses the QTE** like every other spell (§7).
- **Does not work on the elemental sword.**
- Duration, cost and exact buff per element: **TBD**.

### General
- Spells cost **mana**.
- **Health and mana regenerate on their own** (potions and the injection speed it up). **No stamina.**
- Numbers for each spell (damage, cost, cooldowns, upgrade tiers): **TBD**, given later.
- The ice shield's exact limits (duration, damage absorbed, break behavior): **TBD**.

## 7. Spell-casting quick-time event
**Status:** wanted for every spell, the enchant included; still a trial for the 4-day schedule, so it must be possible to switch it off (spells then cast at a fixed default strength).

Purpose: make magic feel more elaborate than pulling a trigger.

### Flow
1. Player casts. **Time slows** by about **90%** (world at ~10% speed). Inspired by the slowed time some games use while the player picks combat hacks.
2. **Part 1 — button sequence → POWER.** Random sequence.
   - Keyboard & mouse: movement keys (W, A, S, D).
   - Gamepad: the four face buttons (A/B/X/Y or Cross/Circle/Square/Triangle).
3. **Part 2 — gesture → REACH.**
   - Mouse: left-to-right, top-to-bottom, a circle, etc.
   - Gamepad: the right stick.
4. Spell fires with its parameters scaled by the two results; time returns to normal.

### Grading and cost
- Each step counts on its own: every mistake lowers the result of its part.
- **The spell is cast if the player gets at least one step right.** It fails only if **every** step is missed.
- **Mana is always spent**, even when the spell fails.

### What each part scales
| Spell | Part 1 (power) | Part 2 (reach) |
|---|---|---|
| Fireball | Damage | Blast radius / range |
| Ice wall | Duration and wall health | Width and height |
| Ice shield | Duration and block effectiveness (which part scales which: **TBD**) | |
| Others | **TBD**, following the same idea: part 1 = strength, part 2 = size/area/range |

### Requirements for the architect
- **Every spell parameter is data, not code**, and every one can be tagged as scaled by part 1, part 2, or neither. Each spell defines its own min/max per parameter.
- The **time-slow amount is configurable** and can be modified by gear (e.g. some armor). Armor/gear as a system is otherwise **TBD**.
- **The camera is locked during the QTE** (the mouse / right stick draws the gesture).
- **The player keeps moving during the QTE, in the direction they were already moving** (the movement keys are busy with part 1, so the last movement input is held).
- **QTE time limit: 4 seconds** for the **whole QTE** (both parts together), in real time (owner, 2026-11-12, after the first play-test: one second more than the original 3; the value is in data).

## 8. Weapons and items

### Held slots
- The player carries **one gun and one staff.** Never two guns or two staffs.
- Once obtained, the **elemental sword is a special third slot.**
- Switch with the **mouse wheel or Alt**; on gamepad, with a **D-pad arrow** (which one: **TBD**).
- Staff in hand: basic magic attack + the 5 spells. Gun in hand: shoot + aim + reload + the enchant.

### Staffs (medieval, pixel art)
- With a staff in hand, **primary fire = basic magic attack**. The attack **depends on which staff** the player holds, so there is more than one staff.
- Staffs are found in **chests** around the map.
- **A staff that is picked up goes to the inventory** (owner, 2026-11-12). Nothing is dropped on the floor and the staff in hand is not replaced: the player opens the inventory and equips the new one by hand. (Provisional, architect: guns and the sword follow the same rule; the first weapon of each kind goes straight to its empty slot.)
- Staff list and what each basic attack does: **TBD**.

### Guns (cyberpunk, 3D)
- Pistol, shotgun, assault rifle.
- The **first gun** is dropped by one of the first armed orcs in the future (§4). The **shotgun and the rifle** are dropped later by enemies, picked up from the floor.
- With a gun in hand: **primary fire = shoot**, **secondary = aim**.
- **Magazines and reload**, like any shooter.
- **Ammo works like classic tactical shooters:** each gun has a fixed number of spare magazines, and each magazine a fixed number of bullets.
- **All ammo refills completely every time the player travels between eras.**
- Between refills, ammo is **picked up from the floor** where enemies drop it.
- Level design keeps ammo tight: never infinite, never starving.
- **Electric grenade** (throwable).

### Armor
- Found in **chests**, rarely.
- **One armor slot = one full armor.** No separate helmet, arms or chest pieces.
- Known effect: some armor changes the QTE time-slow amount (§7). Other stats: **TBD**.

### Chests
- Contain **staffs**, and rarely **armor**. Other contents: **TBD**.

### Melee
- **No melee before the elemental sword** (§11).
- **The sword hits where the player aims** (owner, 2026-11-12): looking at the head hits the head (and gets the head bonus), looking at the legs hits the legs. It is never "the center of the target".

### Consumables
| Item | Era | Effect |
|---|---|---|
| Health potion | Past | Heals a set amount **over time** |
| Mana potion | Past | Restores a set amount of mana **over time** |
| Electric grenade | Future | Throwable, electric damage |
| Health injection | Future | Heals **instantly**, but less than the potion |

### Synergies
Weapon mechanics and spell mechanics combine. The weapon enchant (§6) is the first confirmed one. Other synergies (e.g. shooting through a fire wall, electric grenade on an ice floor) are **TBD**.

## 9. Controls

| Action | Keyboard & mouse | Gamepad |
|---|---|---|
| Move | W A S D | Left stick |
| Look | Mouse | Right stick |
| Primary attack (staff: basic magic attack; gun: shoot) | Left click | R2 / RT |
| Aim (gun in hand) | Right click | L2 / LT |
| Spell slot 1 (staff in hand) | Right click | L1 / LB |
| Spell slots 2–5 (staff in hand) | 1, 2, 3, 4 | Hold L2 / LT + Cross, Circle, Square, Triangle (A, B, X, Y) |
| Reload (gun) | R | Standard shooter layout |
| Weapon enchant (gun in hand only) | T (R is reload) | L1 / LB (free with a gun in hand) |
| Switch staff ↔ gun (↔ sword) | Mouse wheel or Alt | D-pad up |
| Jump | Space | Cross (A) |
| Dash | Ctrl | Circle (B) |
| Quick-use item (grenade / potion / injection share one slot) | F | R1 / RB |
| Cycle the quick-use item | Shift | D-pad left / right |
| Interact (chests, portals, objects) | G | Triangle (Y) |
| Block: raise the summoned ice shield (staff or sword in hand; provisional keys) | Q (hold) | L2 / LT (hold) |
| QTE part 1 | W A S D | Face buttons |
| QTE part 2 | Mouse gesture | Right stick |
| Inventory | Tab | Options / View |
| Skill trees | K (also a tab inside the inventory) | From the inventory |
| Pause | Esc | Start / Menu |

Gun controls on gamepad follow the standard shooter layout.

Right click / L1 change meaning with the held weapon and never clash: staff = spell slot 1; gun = aim (right click) and enchant (L1). L2 means "spell modifier" with the staff and "aim" with a gun.

No sprint (the air speed passive covers movement speed).

Menus (inventory, skill trees, pause) work with keyboard, gamepad **and mouse**: the cursor is visible while a menu is open and a click does what confirm does.

## 10. Enemies and encounters
- **Enemy waves (hordes).** Clearing a wave gives skill points.
- **A couple of sub-bosses.**
- **Two bosses** (the Valkyrie in each era, §11).
- **The true final boss.**
- **Enemies must get around obstacles** to reach the player (owner, 2026-11-12): never stand stuck against a wall.
- Known enemies: **orcs** (past) and **armed orcs** (future). Other types and behavior: **TBD**. Their art follows §3 (medieval = pixel art, cyberpunk = 3D).
- Enemies **drop ammo** on the floor.
- **Enemies do not respawn.** A cleared zone stays cleared, also after era travel.

### Death and difficulty
- On death the player **returns to the last checkpoint**. Checkpoints are **automatic**.
- **Falling out of the map kills** the player (and any enemy).
- **Difficulty: hard**, the only one for now. A separate normal mode is possible later (for example, the spell QTE only in hard); out of scope until the owner says otherwise.
- No target length: we build as much as the time allows.

## 11. Bosses and endgame
**The Valkyrie (two bosses, one in each era).**
- **Past:** trapped inside magic runes in the medieval castle.
- **Future:** trapped in a cryogenic tube.
- The player must **free her and defeat her in both eras.**

Rewards:
| Defeated in | Drops |
|---|---|
| Past (castle) | **Elemental stones/gems** |
| Future | **A weapon hilt** |
| Both fights | One **quest item** each (what they are: **TBD**) |

- **Elemental sword:** in the inventory, the player combines the hilt with a gem and gets an elemental sword that comes out of the hilt. The player can switch the sword to the other elements from the inventory. It is the only melee weapon. The weapon enchant does not apply to it.
  - Art (§3): the **hilt is 3D** (it comes from the future), the **blade is pixel art** (it is magic).
  - HUD: **medieval** (provisional).
- **Final portal:** combining the two quest items opens a portal to a separate map, a dimension between space and time, where the player fights the **true final boss**.
- **True final boss: the Entity** (story in §4). A **mix of both art styles**, which fits what it is (suggestion, not decided: the AI half in 3D, the ghost half in pixel art). Undecided how: two phases with different styles, or one character in both styles at once, or something else (**TBD**).
- This happens in mid/late game.

## 12. Inventory
Needed for: consumables, equipping/unequipping spells, choosing the enchant element, combining hilt + gem, switching the sword's element, combining the two quest items. Layout and limits **TBD**.

## 13. UI: two styles
The UI changes with what the player holds:

| Holding | HUD style |
|---|---|
| Staff (magic) | **1990s first-person RPG** |
| Gun | **Modern cyberpunk** |
| Elemental sword | Medieval / 90s RPG (provisional) |

- **Skill trees** follow the same split: the spell tree has its own 90s-RPG look, the weapon tree its own cyberpunk look.
- The HUD switches when the player switches weapon. The inventory's look is **TBD**.
- **Damage numbers** (owner, 2026-11-12): every hit on an enemy shows a number that comes out of the enemy, rises with a slightly random drift and fades. **Especially effective hits show a bigger number** (provisional, architect: headshots, and hits that take a large share of the enemy's health).

## 14. Theme fit (DIRECTOR'S CUT)
Our reading (owner, 2026-11-12): the theme is effectively **open**; what the organizers ask for is a game with an **author's signature**, a personal vision. We answer it with the game's own identity:
- the hard art split (pixel-art magic vs. 3D cyberpunk) that follows each object and never breaks, with a pixel-art mage holding 3D guns;
- a UI that also changes style with what the player holds;
- magic that has to be performed (the QTE) rather than triggered;
- a combat style that rewards mixing the two eras.

The dev log shows the directing side too: how we direct the AI agents, and every scope decision ("what made the final cut"). Any feature we drop gets logged as a cut, with the reason.

## 15. Scope for the jam (proposal, to discuss)
The jam is 4 days and we package a playable build every day. Suggested order so that every day ends with something playable:

| Day | Target |
|---|---|
| 1 (Nov 12) | First-person controller (move, dash, double jump), pixel-art hands, one staff, one gun, one spell with data-driven parameters, mana/health, one portal swapping eras in a test map. Packaged. |
| 2 (Nov 13) | Rest of guns + spells, 5-slot loadout, weapon enchant, both HUD styles, basic enemies and one wave, consumables, QTE first version. |
| 3 (Nov 14) | Sub-bosses, Valkyrie fights, inventory + elemental sword, skill trees, era-gated zones. |
| 4 (Nov 15) | Final boss arena, polish, audio, video, submission (deadline 20:59 EST). |

## 16. For the architect: systems to design
Not decisions, just the list of systems the design implies:
- Player controller: movement, double jump, dash, speed modifiers (air passives).
- Health, mana, damage, status effects (slow, heal/mana over time, instant heal, temporary weapon buff).
- Ability system shared by spells, staff attacks, guns and grenade (cooldowns, costs, upgrades from the skill trees). Unreal's Gameplay Ability System is one candidate; the architect decides.
- **Data-driven spell definitions:** every parameter configurable, with min/max and a tag saying whether QTE part 1 (power) or part 2 (reach) scales it.
- 5-slot spell loadout + the fixed enchant slot; equip/unequip.
- Spell QTE: input sequence (KB/M and gamepad), gesture recognition (mouse and right stick), per-step grading, global time dilation as a configurable, gear-modifiable value; can be disabled.
- Held items: staffs (basic attack per staff), guns (shoot, aim), electric grenade, enchant buff, melee sword with swappable element.
- Spawned spell actors: walls, floor zone, ground trap, moving damage ball, sustained flamethrower.
- Era system: same map in two times, portals, era-gated zones, past actions that open future zones, player state kept across eras.
- **Rendering of the art rule per object:** pixel-art sprites (player hands, medieval props, enemies, spells) and 3D meshes (guns, cyberpunk world) in the same first-person view.
- Two UI skins (90s RPG, modern cyberpunk) for HUD and skill trees, switched by the held item.
- Encounters: waves, sub-bosses, bosses (free + fight + drops), skill point rewards.
- Inventory with item combining; two skill trees.
- Final portal and separate final-boss map.
- Input for keyboard & mouse and gamepad (layout in §9).

## 17. Open questions
Answers get folded into the sections above as they come. The prototype questions of 2026-11-12 are answered (see §0.1 and §3).

### Still open for the base systems
1. **NVIDIA feature:** which one? DLSS 4.5 is the simplest. CENTRAL (where the game is built) has an **NVIDIA GeForce RTX 4080 Laptop GPU**, so it can be tested here. The architect checks that a DLSS plugin exists for UE 5.9.0. (The AMD RX 7700 XT belongs to the 3D assets machine, ASSETS.)

### Content questions (parked by the owner, 2026-11-12)
Not needed for the prototype; to be answered later.
- Why the Valkyrie fights the player; whether the possession attempt is part of the final fight.
- Staffs: how many, element of each basic attack.
- Ice shield QTE: which part scales duration and which block effectiveness.
- Armor stats besides the QTE time slow.
- How "combining styles" is rewarded beyond synergies.
- What the player does in the future besides the intro and the Valkyrie; challenge per elemental zone; sub-bosses.
- Examples of era-gated zones and past→future unlocks.
- Other enemy types per era; the two quest items.
- Game and team name.

## 18. Provisional rules and numbers picked by the architect (2026-11-12)
The owner asked the architect not to stop for missing design details. Everything below was chosen so the prototype could be built; **all of it is provisional** and waits for the owner's word. Numbers are in `Glyphbound/Content/Data/*.json` and can be changed there without touching code.

### Rules
- **Jumps commit to their direction (owner, 2026-11-12):** like a real jump, the direction cannot be changed in the air (air control 0; the same when walking off a ledge). The direction is chosen at takeoff: the movement keys held at that moment. The second jump can choose a new direction and then commits again. Provisional details (architect): a jump from standing with a direction held leaves at 60% of walking speed at least; the second jump with a direction held leaves at walking speed at least, and with no direction held it keeps the current drift. The dash still works in the air and is not affected.
- **Double jump on day 1:** the player starts knowing the air passive "Double jump" so it can be tested now. In the real game it is learned in the air zone (§6); remove it from `Player.json` `StartingSpells` then.
- **Dash:** available from the start (the air passive only improves it). It goes in the movement direction (forward when standing still), works in the air, gives no invulnerability, and does not refill jumps.
- **Owning weapons:** decided by the owner for staffs (section 8): picked-up weapons go to the inventory and are equipped by hand. Provisional part: guns and the sword follow the same rule; each owned gun keeps its own ammo while stored.
- **Reload:** the bullets left in the old magazine are lost (classic tactical style). A gun reloads by itself when its magazine runs dry. Ammo pickups give one spare magazine, up to the gun's maximum.
- **Sustained spells (flamethrower), changed by the owner (2026-11-12):** after the QTE the flamethrower runs **while its spell key is held**, until the key is released or the mana runs out (also on weapon switch or death). No time limit. Provisional (architect): it pays a smaller cost at the start (10 mana, always spent, like every QTE spell) and then drains 15 mana per second; if the key was already released when the QTE ends, it still burns for a short minimum (0.75 s) so the cast is never wasted; the cooldown starts at the cast. All values in `Spells.json` (`ManaCost`, `ManaPerSecond`, `MinDuration`).
- **New spell perks (owner's list, placement by the architect):** in the spell tree, each under its element's column (fire, lightning, ice), with no prerequisite so they can be tested right away. Deep Freeze: the ice floor slows by 50% at normal QTE power (the slow effect, `StatusEffects.json`), so +5 points per rank makes 55 / 60 / 65%; the cap is **80%** (also reached by a perfect QTE with 3 ranks). The cap is a general rule: any spell parameter can declare a `Cap` in `Spells.json`, applied after QTE scaling and perks.
- **Ice shield QTE:** part 1 (power) scales its duration and health; part 2 (reach) scales how much frontal damage it blocks.
- **Modifier keys by weapon type:** besides one weapon (`Weapon.Pistol.Damage`) and all weapons (`Weapon.All.Damage`), a modifier can target a slot: `Weapon.Gun.*`, `Weapon.Staff.*`, `Weapon.Sword.*`. The enchant uses `Weapon.Gun.Damage`, so it never buffs the staff or the sword.
- **Explosions and enemy melee need a clear line** to their target: no damage through walls or through the ice wall.
- **Enemies and cover (after the first play-test):** enemies walk around obstacles. A melee enemy only starts its attack when it sees the player; a ranged enemy that loses sight stops aiming and moves until it sees the player again. Enemies do not walk off ledges on their own; one that falls out of the map dies without drops.
- **Projectile explosions push enemies back** (knockback is a spell parameter, scaled by QTE power); they do not push the player.
- **Falling:** the kill height is 2,500 cm below the floor of the level (`Game.json` `KillZ`).
- **Weapon enchant QTE:** part 1 scales how long the buff lasts; part 2 scales how strong it is. Only one enchant at a time. The enchanted gun deals the element's damage type and applies its effect on hit (fire burns, lightning shocks, ice chills).
- **Spell walls:** the fire wall does not block; it burns enemies that touch it. The ice wall blocks movement and shots and has health; anyone's shots can break it, the player's included. Enemies do not attack walls on purpose yet.
- **QTE:** a wrong key consumes its step. A gesture is judged when the hand stops for a moment or the stroke is clearly complete. If both parts are fully missed the spell fizzles, and the mana is still spent and the cooldown still starts.
- **New spells** go to the first free loadout slot.
- **Ground spell figures (architect, after the owner's note):** the figure is the spell's sprite stretched to the diameter of its area, a few cm above the floor, keeping the pixels sharp; transparent pixels show the floor. It does not turn with the camera; it keeps the yaw the player was facing when casting. The rune shows dimmer until it is armed. The old colored disc is gone.
- **Overlapping ground spells (owner, 2026-11-12):** the figure placed last is drawn on top of every figure it overlaps, completely (an ice floor cast over a rune hides the rune where they overlap; a rune cast over an ice floor shows over it), and overlapping figures never flicker. Provisional (architect): each new figure sits a little higher than the highest figure it overlaps (1.5 cm per layer, values in `Game.json`); this is only how it looks, the spells' areas and effects do not change.
- **Skill reset (owner asked for it, details provisional):** a "Reset skills" button on the skill tree screen (K, and the Skills page of the inventory), also the R key (Y / Triangle on a gamepad) while that screen is open. Free, any time, no confirmation, refunds exactly the points that were spent (even if a node's cost changed in `Skills.json` since). Whether the final game keeps it free, limits it (for example only at portals) or keeps it only as a test option is open.
- **"+1 skill point" button (owner asked for it, to test perks):** on the same screen, left of the skill points; each click adds one free point to the pool (no key, no limit). A reset refunds only what was paid, so free points stay. It is a test option: it must be removed or hidden before the jam submission (provisional, architect).
- **Elemental sword:** the hilt and the gems are not consumed when forging, so the player re-forges from the inventory to switch element.
- **Enemies:** once hit, an enemy chases the player from any distance. Enemies of the other era are frozen and hidden, not simulated.
- **Encounters:** if the player dies during one, its living enemies are removed and it starts again from wave 1 on return. Progress flags are never rolled back.
- **Checkpoints:** saved at the start, on every era travel, on entering a checkpoint zone and when an encounter is cleared. Death sends the player back to the checkpoint's place and era with full health, mana and ammo, and **keeps everything collected since** (items, weapons, spells, skill points): pickups and chests do not come back, so rolling the inventory back would lose rewards for good (found by the code review).
- **Consumables** can be used at full health or mana (they are wasted).
- **Art rule on pickups:** cyberpunk items (injection, grenade, magazine, sword hilt) lie on the floor as 3D shapes; medieval ones (potions, gems, books, staffs) as pixel-art sprites.

### Numbers (first pass, all in the JSON files)
- **Jumps:** each jump rises about 2 m (was 1.2 m); the double jump adds a second one of the same height.
- **Head / weak point bonus per weapon** (owner, 2026-11-12): pistol x2; shotgun, rifle, staffs and sword x1.5. **Perks can raise it**: the weapon tree node "Marksman" adds +0.25 per rank (3 ranks) to every weapon, staff and sword included (provisional numbers). Spell projectiles get no head bonus unless a spell defines one.
- **Fireball shockwave:** pushes enemies away from the blast (strength scaled by QTE power).
- **Player:** 100 health (regenerates 2/s after 5 s without damage), 100 mana (6/s after 1.5 s), speed 750, dash 7 m in 0.18 s every 1.2 s.
- **QTE:** 4 keys, 1 gesture, 4 s in real time (raised from 3 s by the owner), world at 10% speed.
- **Staff:** Apprentice staff 12 damage every 0.35 s, no mana cost. Ember staff 22 fire damage every 0.6 s and burns.
- **Guns:** pistol 18 damage, 12 rounds, 4 spare magazines; shotgun 8 pellets of 10, 6 rounds, strong up close; rifle automatic, 14 damage, 30 rounds, stronger far away. Headshots: pistol x2, shotgun and rifle x1.5.
- **Enemies:** past orc 60 health, melee 12; future orc 70 health, ranged 9; brute (sub-boss placeholder) 320 health, melee 28.
- **Status effects:** burn 32 damage over 4 s; slow 50% for 4 s; chill 30% for 3 s; shock 40% for 1.5 s; health potion 60 over 10 s; injection 35 at once; enchant +25% gun damage for 20 s.
- **Skill trees:** 6 nodes each, 1-2 points per rank, up to 3 ranks.
- Every spell number in `Spells.json`.
