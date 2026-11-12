# Game Design Document (working draft)

> Epic AI MegaJam 2026 · Theme: **DIRECTOR'S CUT** · Unreal Engine 5.9.0
> Status: **v0.10, 2026-11-12** — owner's vision plus nine rounds of answers. Anything marked **TBD** is not decided yet; see "Open questions" at the end.
> Audience: **the architect** = whichever Claude Code chat is currently implementing the game. There is normally only one at a time, and it is replaced when its context fills up or its tasks are done. This document is the source of truth that every new architect chat reads first. Implementation is delegated as `CLAUDE.md` says ("Implementador").
> Project code name: **Glyphbound** (provisional; the game title is still TBD). Written in English because it is part of the public dev log (Glass Box) and the jam requires English.

## Changelog
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
- **Code:** C++ suggested for the core systems (AI agents handle code better than Blueprint graphs), with data assets / data tables for tuning. Final call: the architect.
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
- **Headshots on sprites:** the head is the top part of the sprite.

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
  - **Weapon tree:** damage, fire rate, **recoil** (better aim), **damage by distance** (shotgun stronger up close, rifle stronger far away), **headshot damage**.
- **One shared pool of skill points** for both trees.
- **Skill points** are given at specific points, mainly for defeating enemies:
  - defeating each **sub-boss**;
  - defeating each **boss**;
  - **clearing a wave** of enemies.

## 6. Magic (4 elements + weapon enchant)
Each element has 3 entries. Air is passives only; the other three are active spells.

| Element | 1 | 2 | 3 |
|---|---|---|---|
| **Fire** | Fireball (projectile) | Fire wall | Flamethrower (sustained DPS) |
| **Lightning** | Lightning bolt (projectile) | Lightning rune trap on the ground; triggers when an enemy walks over it | Static-electricity ball: moves and damages every enemy around it |
| **Ice** | Ice shield (held block, like a first-person RPG shield, but with a cooldown and limits) | Ice wall | Ice floor: slows enemies, light crowd control |
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
- **QTE time limit: 3 seconds** (provisional value in data; assumed real time for the whole QTE, both parts).

## 8. Weapons and items

### Held slots
- The player carries **one gun and one staff.** Never two guns or two staffs.
- Once obtained, the **elemental sword is a special third slot.**
- Switch with the **mouse wheel or Alt**; on gamepad, with a **D-pad arrow** (which one: **TBD**).
- Staff in hand: basic magic attack + the 5 spells. Gun in hand: shoot + aim + reload + the enchant.

### Staffs (medieval, pixel art)
- With a staff in hand, **primary fire = basic magic attack**. The attack **depends on which staff** the player holds, so there is more than one staff.
- Staffs are found in **chests** around the map.
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
| QTE part 1 | W A S D | Face buttons |
| QTE part 2 | Mouse gesture | Right stick |
| Inventory | Tab | Options / View |
| Skill trees | K (also a tab inside the inventory) | From the inventory |
| Pause | Esc | Start / Menu |

Gun controls on gamepad follow the standard shooter layout.

Right click / L1 change meaning with the held weapon and never clash: staff = spell slot 1; gun = aim (right click) and enchant (L1). L2 means "spell modifier" with the staff and "aim" with a gun.

No sprint (the air speed passive covers movement speed).

## 10. Enemies and encounters
- **Enemy waves (hordes).** Clearing a wave gives skill points.
- **A couple of sub-bosses.**
- **Two bosses** (the Valkyrie in each era, §11).
- **The true final boss.**
- Known enemies: **orcs** (past) and **armed orcs** (future). Other types and behavior: **TBD**. Their art follows §3 (medieval = pixel art, cyberpunk = 3D).
- Enemies **drop ammo** on the floor.
- **Enemies do not respawn.** A cleared zone stays cleared, also after era travel.

### Death and difficulty
- On death the player **returns to the last checkpoint**. Checkpoints are **automatic**.
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
2. **QTE time:** are the 3 seconds for the whole QTE or for each part, and are they real time (not slowed)?

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
