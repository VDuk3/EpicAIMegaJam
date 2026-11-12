# Game Design Document (working draft)

> Epic AI MegaJam 2026 · Theme: **DIRECTOR'S CUT** · Unreal Engine 5.9.0
> Status: **v0.1, 2026-11-12** — first dump of the owner's vision, written down as dictated. Anything marked **TBD** is not decided yet; see "Open questions" at the end.
> Audience: the code architect (first reader), then the whole team. Written in English because it is part of the public dev log (Glass Box) and the jam requires English.

## 1. One-line pitch
A fast first-person **shooter/RPG hybrid** where the player jumps through fixed portals between a **pixel-art medieval past** full of magic and a **3D cyberpunk future** full of guns, and wins by **mixing spells and firearms** rather than sticking to one style.

Working title: **TBD**.

## 2. Pillars
1. **Two eras, one fight.** Past = magic, pixel art. Future = guns, 3D cyberpunk. Both feed the same character.
2. **Combos over specialization.** The game rewards switching between spells, guns and melee. Weapon and spell mechanics have synergies.
3. **Fast and dynamic combat**, in the spirit of modern first-person action RPGs: spells, some melee, ranged spells and shooting.
4. **Magic feels crafted, not triggered** (experimental, see §7): casting uses a short slowed-time quick-time event.

## 3. World and travel
- Two versions of the map/world:
  - **Past:** medieval, magical, **pixel-art** style. Contains the medieval castle.
  - **Future:** cyberpunk, futuristic, **3D** style.
- The player travels between past and future **only through predefined portals** placed on the map.
- Whether both eras share the same layout (same place, different time) is **TBD**.
- A third, separate map unlocks late: a **dimension between space and time** where the true final boss lives (see §9).

## 4. Player progression
- **No character levels / no XP leveling.**
- The only progression is a **skill tree** that upgrades spells.
- How skill points are earned is **TBD**.

## 5. Magic (4 elements)
Each element has 3 entries. Air is passives only; the other three are active spells.

| Element | 1 | 2 | 3 |
|---|---|---|---|
| **Fire** | Fireball (projectile) | Fire wall | Flamethrower (sustained DPS) |
| **Lightning** | Lightning bolt (projectile) | Lightning rune trap on the ground; triggers when an enemy walks over it | Static-electricity ball: moves and damages every enemy around it |
| **Ice** | Ice shield (held block, like a first-person RPG shield, but with a cooldown and limits) | Ice wall | Ice floor: slows enemies, light crowd control |
| **Air** (passives) | Double jump | Improved dash | Increased movement speed |

- Spells cost **mana** (implied by the mana potion).
- Details of each spell (damage, cost, cooldowns, upgrade tiers) will come later: **TBD**.
- The ice shield's exact limits (duration, damage absorbed, break behavior) are **TBD**.

## 6. Weapons and items

### Future weapons (cyberpunk)
- Pistol
- Shotgun
- Assault rifle
- **Electric grenade** (throwable)

### Melee
- Some melee is part of the combat (§2). The **elemental sword** (§9) is the confirmed melee weapon; whether there is earlier melee is **TBD**.

### Consumables
| Item | Era | Effect |
|---|---|---|
| Health potion | Past | Heals a set amount **over time** |
| Mana potion | Past | Restores a set amount of mana **over time** |
| Electric grenade | Future | Throwable, electric damage |
| Health injection | Future | Heals **instantly**, but less than the potion |

### Synergies
Weapon mechanics and spell mechanics combine (example ideas, not decided: shooting into a fire wall, electric grenade on an ice floor). The actual synergy list is **TBD**.

## 7. Spell-casting quick-time event (experimental)
**Status: trial.** Built only if the 4-day schedule allows it; it must be possible to switch it off.

Purpose: make magic feel more elaborate than pulling a trigger.

Flow when the player casts a spell:
1. **Time slows** by about **90%** (world runs at ~10% speed). Inspired by the slowed time some games use while the player picks combat hacks.
2. **Part 1 — button sequence:** a random sequence of buttons.
   - Keyboard & mouse: the movement keys (W, A, S, D).
   - Gamepad: the four face buttons (A/B/X/Y or Cross/Circle/Square/Triangle).
3. **Part 2 — gesture:** a movement.
   - Mouse: left-to-right, top-to-bottom, a circle, etc.
   - Gamepad: the right stick.
4. Spell fires; time returns to normal.

Requirements:
- The slow-down amount is a **configurable value**, because gear (e.g. some armor) may modify it. Armor/gear as a system is otherwise **TBD**.
- Undecided: what failing the QTE does, whether there is a time limit, whether sequence length depends on the spell, whether the player camera also slows.

## 8. Enemies
**TBD** (types per era, behavior, how pixel-art enemies are rendered).

## 9. Bosses and endgame
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

- **Elemental sword:** in the inventory, the player combines the hilt with a gem and gets an elemental sword that comes out of the hilt. The player can switch the sword to the other elements from the inventory.
- **Final portal:** combining the two quest items opens a portal to a separate map, a dimension between space and time, where the player fights the **true final boss** (**TBD**).
- This happens in mid/late game.

## 10. Inventory
Needed for: consumables, combining hilt + gem, switching the sword's element, combining the two quest items. Layout and limits **TBD**.

## 11. Theme fit (DIRECTOR'S CUT)
**TBD — needs a decision.** The theme is scored. The concept above does not yet say how it shows "your vision, your edits, your final cut" (a scrapped mechanic brought back, the player editing/cutting, etc.). The dev log will also show how we direct the AI agents and what we cut.

## 12. Scope for the jam (proposal, to discuss)
The jam is 4 days and we package a playable build every day. Suggested order so that every day ends with something playable; anything not reached becomes our own documented "cut":

| Day | Target |
|---|---|
| 1 (Nov 12) | First-person controller (move, dash, double jump), one gun, one spell, mana/health, one portal swapping eras in a test map. Packaged. |
| 2 (Nov 13) | Rest of guns + spells, basic enemies in both eras, consumables, a first synergy. |
| 3 (Nov 14) | Valkyrie fights, inventory + elemental sword, skill tree, QTE trial. |
| 4 (Nov 15) | Final boss arena, polish, audio, video, submission (deadline 20:59 EST). |

## 13. For the architect: systems to design
Not decisions, just the list of systems the design implies:
- Player controller: movement, double jump, dash, speed modifiers (air passives).
- Health, mana, damage, status effects (slow, heal/mana over time, instant heal).
- Ability system shared by spells, gun abilities and grenade (cooldowns, costs, upgrades from the skill tree). Unreal's Gameplay Ability System is one candidate; the architect decides.
- Weapons: hitscan/projectile guns, melee sword with swappable element.
- Spawned spell actors: walls, floor zone, ground trap, moving damage ball, sustained flamethrower.
- Era/portal system: switching between past and future maps or layers, keeping player state.
- Two visual pipelines: pixel-art past vs. 3D future.
- Spell QTE with global time dilation as a configurable, gear-modifiable value; can be disabled.
- Inventory with item combining.
- Skill tree.
- Boss encounters (free + fight + drops), quest items, final portal.
- Input for both keyboard & mouse and gamepad.

## 14. Open questions
Answers get folded into the sections above as they come.

**Blocking (needed before the architect starts):**
1. **Theme:** how does the game show DIRECTOR'S CUT?
2. **Pixel art in a first-person 3D game:** 2D sprites/billboards in a 3D world (classic retro shooter look), a pixelation post-process over 3D, or both?
3. **Eras:** same map in two times (portal swaps the layer in place) or two separate maps?
4. **Spells and guns across eras:** can the player cast in the future and shoot in the past, or is each era limited to its own style? (This decides how the "mix both styles" pillar works.)
5. **Team and roles:** who codes, who does art (pixel and 3D), audio; and who/what is "the architect" (person or AI agent in the chat).

**Can wait:**
6. How skill points are earned (no XP).
7. Enemy types per era.
8. The two quest items and the true final boss.
9. QTE: fail result, time limit, sequence length per spell, does the camera slow too.
10. Armor/gear system: in scope or only the time-slow modifier?
11. Melee before the elemental sword?
12. Game and team name.
