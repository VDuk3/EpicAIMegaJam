# Game Design Document (working draft)

> Epic AI MegaJam 2026 · Theme: **DIRECTOR'S CUT** · Unreal Engine 5.9.0
> Status: **v0.2, 2026-11-12** — owner's vision plus the first round of answers. Anything marked **TBD** is not decided yet; see "Open questions" at the end.
> Audience: **the architect** = whichever Claude Code chat is currently implementing the game. There is normally only one at a time, and it is replaced when its context fills up or its tasks are done. This document is the source of truth that every new architect chat reads first. Written in English because it is part of the public dev log (Glass Box) and the jam requires English.

## Changelog
- v0.2 (2026-11-12): theme reading, art rule, shared map with era-gated zones, cross-era style mixing, QTE = power + reach, weapon enchant spell, skill points from encounters, enemy waves and sub-bosses, no early melee, architect definition.
- v0.1 (2026-11-12): first dump of the owner's pitch.

## 1. One-line pitch
A fast first-person **shooter/RPG hybrid** where the player jumps through fixed portals between a **pixel-art medieval past** full of magic and a **3D cyberpunk future** full of guns, on the same map in two times, and wins by **mixing spells and firearms** rather than sticking to one style.

Working title: **TBD**.

## 2. Pillars
1. **Two eras, one fight.** Past = magic, pixel art. Future = guns, 3D cyberpunk. Both feed the same character.
2. **Combos over specialization.** Spells and guns can be used in both eras, and the game rewards combining them. Weapon and spell mechanics have synergies.
3. **Fast and dynamic combat:** spells, ranged spells, shooting, and melee once the elemental sword is unlocked.
4. **Magic feels crafted, not triggered:** casting uses a two-part slowed-time quick-time event that scales the spell (see §7).

## 3. Art rule (never broken)
- **Everything medieval, wizards and magic is pixel art.**
- **Everything cyberpunk and futuristic is 3D.**
- This rule has **no exceptions**. Open point for the architect and art: how it applies when the two meet on screen (a 3D gun in the past, a magic spell in the future). See open question 1.

## 4. World and travel
- **One map, two times** (past and future). The player travels between them **only through predefined portals** placed on the map.
- **Era-gated zones:** some zones are reachable in one era and not in the other.
- **Cross-era unlocks:** some zones must be unlocked in the past so they open in the future (cause in the past, effect in the future).
- These interactions are few (short jam) but they exist and are part of level design.
- A third, separate map unlocks late: a **dimension between space and time** where the true final boss lives (see §10).

## 5. Player progression
- **No character levels / no XP leveling.**
- The only progression is a **skill tree** that upgrades spells.
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

### Weapon enchant (always equipped)
- A spell that **cannot be unequipped**.
- Animation: the **left hand draws a magic glyph over the weapon held in the right hand**.
- Effect: the weapon is **temporarily buffed with magic**.
- This is the core bridge between the two styles (magic on top of guns).
- Element, duration, cost and exact buff: **TBD**.

### General
- Spells cost **mana** (implied by the mana potion).
- Numbers for each spell (damage, cost, cooldowns, upgrade tiers): **TBD**, given later.
- The ice shield's exact limits (duration, damage absorbed, break behavior): **TBD**.

## 7. Spell-casting quick-time event
**Status:** wanted for every spell; still a trial for the 4-day schedule, so it must be possible to switch it off (spells then cast at full strength or a fixed default).

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

### What each part scales
| Spell | Part 1 (power) | Part 2 (reach) |
|---|---|---|
| Fireball | Damage | Blast radius / range |
| Ice wall | Duration and wall health | Width and height |
| Others | **TBD**, following the same idea: part 1 = strength, part 2 = size/area/range |

### Requirements for the architect
- **Every spell parameter is data, not code**, and every one can be tagged as scaled by part 1, part 2, or neither. Each spell defines its own min/max per parameter.
- The **time-slow amount is configurable** and can be modified by gear (e.g. some armor). Armor/gear as a system is otherwise **TBD**.
- Still open: whether each part is graded (each missed input lowers the result) or pass/fail, time limit, sequence length per spell, whether the camera also slows (see open questions).

## 8. Weapons and items

### Future weapons (cyberpunk)
- Pistol
- Shotgun
- Assault rifle
- **Electric grenade** (throwable)

### Melee
- **No melee before the elemental sword** (§10).

### Consumables
| Item | Era | Effect |
|---|---|---|
| Health potion | Past | Heals a set amount **over time** |
| Mana potion | Past | Restores a set amount of mana **over time** |
| Electric grenade | Future | Throwable, electric damage |
| Health injection | Future | Heals **instantly**, but less than the potion |

### Synergies
Weapon mechanics and spell mechanics combine. The weapon enchant (§6) is the first confirmed one. Other synergies (e.g. shooting through a fire wall, electric grenade on an ice floor) are **TBD**.

## 9. Enemies and encounters
- **Enemy waves (hordes).** Clearing a wave gives skill points.
- **A couple of sub-bosses.**
- **Two bosses** (the Valkyrie in each era, §10).
- **The true final boss.**
- Enemy types per era, behavior, and how pixel-art enemies are rendered: **TBD**.

## 10. Bosses and endgame
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

- **Elemental sword:** in the inventory, the player combines the hilt with a gem and gets an elemental sword that comes out of the hilt. The player can switch the sword to the other elements from the inventory. It is the only melee weapon.
- **Final portal:** combining the two quest items opens a portal to a separate map, a dimension between space and time, where the player fights the **true final boss** (**TBD**).
- This happens in mid/late game.

## 11. Inventory
Needed for: consumables, combining hilt + gem, switching the sword's element, combining the two quest items. Layout and limits **TBD**.

## 12. Theme fit (DIRECTOR'S CUT)
Our reading (owner, 2026-11-12): the theme is effectively **open**; what the organizers ask for is a game with an **author's signature**, a personal vision. We answer it with the game's own identity:
- the hard art split (pixel-art magic vs. 3D cyberpunk) that never breaks;
- magic that has to be performed (the QTE) rather than triggered;
- a combat style that rewards mixing the two eras.

The dev log shows the directing side too: how we direct the AI agents, and every scope decision ("what made the final cut"). Any feature we drop gets logged as a cut, with the reason.

## 13. Scope for the jam (proposal, to discuss)
The jam is 4 days and we package a playable build every day. Suggested order so that every day ends with something playable:

| Day | Target |
|---|---|
| 1 (Nov 12) | First-person controller (move, dash, double jump), one gun, one spell with data-driven parameters, mana/health, one portal swapping eras in a test map. Packaged. |
| 2 (Nov 13) | Rest of guns + spells, weapon enchant, basic enemies and one wave, consumables, QTE first version. |
| 3 (Nov 14) | Sub-bosses, Valkyrie fights, inventory + elemental sword, skill tree, era-gated zones. |
| 4 (Nov 15) | Final boss arena, polish, audio, video, submission (deadline 20:59 EST). |

## 14. For the architect: systems to design
Not decisions, just the list of systems the design implies:
- Player controller: movement, double jump, dash, speed modifiers (air passives).
- Health, mana, damage, status effects (slow, heal/mana over time, instant heal, temporary weapon buff).
- Ability system shared by spells, gun abilities and grenade (cooldowns, costs, upgrades from the skill tree). Unreal's Gameplay Ability System is one candidate; the architect decides.
- **Data-driven spell definitions:** every parameter configurable, with min/max and a tag saying whether QTE part 1 (power) or part 2 (reach) scales it.
- Spell QTE: input sequence (KB/M and gamepad), gesture recognition (mouse and right stick), global time dilation as a configurable, gear-modifiable value; can be disabled.
- Weapons: guns, electric grenade, enchant buff on the held weapon, melee sword with swappable element.
- Spawned spell actors: walls, floor zone, ground trap, moving damage ball, sustained flamethrower.
- Era system: same map in two times, portals, era-gated zones, past actions that open future zones, player state kept across eras.
- Rendering of the art rule: pixel-art medieval/magic vs. 3D cyberpunk.
- Encounters: waves, sub-bosses, bosses (free + fight + drops), skill point rewards.
- Inventory with item combining; skill tree.
- Final portal and separate final-boss map.
- Input for both keyboard & mouse and gamepad.

## 15. Open questions
Answers get folded into the sections above as they come.

**Blocking (needed before the architect starts):**
1. **Art rule where the eras meet:** does the rule follow the *object* or the *era*? E.g. in the past, is the player's 3D pistol still 3D inside a pixel-art world, and in the future is a fireball still pixel art inside the 3D world? Same question for the first-person hands.
2. **Spell loadout:** how many spells are equipped at once, how does the player pick and switch (wheel, slots, number keys), and are all spells available from the start or unlocked by the skill tree?
3. **Weapon enchant:** which element does it use (the selected one, a fixed one, the player chooses)? Does it have a QTE? Does it also work on the elemental sword?
4. **QTE grading:** is each part pass/fail, or does every missed input lower the result a bit? Is mana spent if the player fails everything?

**Can wait:**
5. How "combining styles" is rewarded beyond synergies (combo meter, bonus damage, more skill points?).
6. Do the ice shield and the air passives use the QTE? (Passives presumably not.)
7. Gun ammo: does it exist, and where does it come from?
8. Sub-bosses: how many, which era, what they are.
9. Examples of era-gated zones and past→future unlocks.
10. Enemy types per era.
11. The two quest items and the true final boss.
12. QTE: time limit, sequence length per spell, does the camera slow too.
13. Armor/gear system: in scope, or only the time-slow modifier?
14. Game and team name.
