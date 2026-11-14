# Game Design Document (working draft)

> Epic AI MegaJam 2026 · Theme: **DIRECTOR'S CUT** · Unreal Engine 5.9.0
> Status: **v0.60, 2026-11-14** — owner's vision plus twenty-two rounds of answers. Anything marked **TBD** is not decided yet; see "Open questions" at the end.
> Audience: **the architect** = whichever Claude Code chat is currently implementing the game. There is normally only one at a time, and it is replaced when its context fills up or its tasks are done. This document is the source of truth that every new architect chat reads first. Implementation is delegated as `CLAUDE.md` says ("Implementador").
> Project code name: **Glyphbound** (provisional; the game title is still TBD). Written in English because it is part of the public dev log (Glass Box) and the jam requires English.

## Changelog
- v0.60 (2026-11-14): owner: "yes, go ahead" on the castle's plan. **The castle pack's castle is back in the project and measured**, and the plan (version 1) is drawn to scale over **the keep as its author assembled it**: a T of five sections, 92 x 59 m and 55 m high, storeys 9 m apart, six spiral stairs, a stair hall, two great halls with galleries that have no stair of their own, roof walks. Slice 1 (the opening): gatehouse, two wards, Front Hall, Main Hall, a crypt stair in its rear corner, cells, the seal's hall in the rock behind the keep. Nothing built. §4 "The site".
- v0.59 (2026-11-14): owner: **the castle's map starts, with the plan.** It carries the future base's style to a castle (ways up and down, fights on two levels, obstacles, variety) with what only a castle has: a spiral stair in a tower, a floor above another that do not connect, a long wall between two towers. Plan version 0 in `Design/Levels/PAST_CASTLE_PLAN.md`, a proposal on paper: a keep over the seal's crypt, two wards, towers joined by a wall, orcs camped in it; slice 1 is the game's opening, from the gate to the seal. Nothing built or measured yet. §4 "The site".
- v0.58 (2026-11-14): the owner's notes on the first test package. **The bulkhead's eight panes break one by one and its bars stay.** **The Valkyrie waits inside her tube**, sealed, until the player breaks its glass; then she wakes and leaps out. **Vault**: a thin obstacle the jump cannot clear (a railing under a low ceiling) is climbed over, on jump or by pushing against it. The Valkyrie's room steps can be walked; chairs collide. The review's nine findings fixed (the zone holds the movement itself, melee turns, reinforcements out of sight, alerts carry where the noise was, guards walk back to their posts). §4 "The site", §9, §10.
- v0.57 (2026-11-14): owner's review of the future's enemies after playing. **Squad tactics**: enemies are already in their zone on patrol, notice the player, take cover, spread out, flank, fire in turns (attack tokens) and never leave their zone; clips play in place (the "teleporting" robots); drones slower; orcs in every fight; **the horde chief**, a bigger orc with a shield that blocks shots and a heavy pistol; the Valkyrie stands in her room. **The glass wall that breaks** is a modelled bulkhead of thick dirty glass in the manner of the deep wing's windows, and it shatters into pieces. Console: `gb.God`, `gb.OpenValkyrie`, `gb.OpenDoors`. §4 "The site", §10.
- v0.56 (2026-11-14): **the future's enemies in 3D** from the ASSETS deliveries: modular armored orcs, robot soldier, drone, sentinel, robot spider, the 3D Valkyrie with her sword; a basic first-person-shooter behaviour (notice, alert, keep distance, strafe, bursts); encounters in the future base and the two Power Cells' sources. §10, §19.
- v0.55 (2026-11-14): owner's sixth walk of the future base, four corrections: **the Valkyrie's room keeps only the holograms** of the figures (the solid ones are out, their tubes stay empty); the glass wall is in panes, clouded; the fallen deck tiles stay tilted and can be stood on; the ladder's walkway is entered from its open end. §4 "The site".
- v0.54 (2026-11-14): owner's fifth walk of the future base. **Hand ladders**: a new way to move, used with the Interact key from either end, with a simple climb (hands on the rungs); the deep wing's stairwell goes down by one instead of a stair. The breakable wall is **frosted glass**. The tentacle boss's hall is darker. Corners, collisions and platforms fixed. §4 "The site", §11.
- v0.53 (2026-11-14): owner's fourth walk of the future base. **A wall that breaks in four hits** in the transfer bay leads down to **a wing under water**: stairs, a worn corridor with failing light, a small room to be infested, a corridor in complete darkness (smoke later) and **the tentacle boss's hall, a dead end**. In the medium room, **an energy gate** that goes out when **two Power Cells dropped by enemies** are set in its two devices; behind it **the Valkyrie's room**, after a battle, with her tube in sight through the gate from the start. Crates stand flush; the lab's steps are walkable; the hall's corners are closed. §4 "The site", §11.
- v0.52 (2026-11-14): owner's third walk of the future base. (1) **The medium room is the lab room again, without figures, holograms and tubes**: only those are kept for the Valkyrie's fight, not the whole kit (corrects v0.50). (2) **A sub-boss hall** behind the long corridor: wide, 12 m high, railed galleries for enemies along both sides, crates as the only way up, an exit door on the far side. (3) **A spell always appears**, whatever its size and whatever the room: the ice wall at full size did not appear in the low corridor. §4 "The site", §11 "Spell walls".
- v0.51 (2026-11-13): the long corridor of the future base is now **the Spaceship Corridor model** (the asset the owner meant), imported through Blender with the look he approved from renders: plain painted metal, dark windows, lit screens. The screens show our own picture, the containment seal's status board, instead of the model's stock infographic (it carries a third party's watermark). The muster hall has no fight for now (owner). §4 "The site".
- v0.50 (2026-11-13): owner's second walk of the future base. **The lab kit (Sci-Fi Creatures Research Lab) is kept for the Valkyrie's fight**: nothing else uses it. The medium combat room stays where it is but is now a muster hall built from the base's own kit. After the second freight door comes **a long corridor with cables on the floor and pipes under the ceiling**. The east stair of the bay gets the same room as the west one. A wall that trapped the player behind the freight door is gone (the builder's own mistake). §4 "The site", §11.
- v0.49 (2026-11-13): owner's first walk of the future base, five notes: no holes where rooms meet corridors; the bay's stair must not be blocked; **a door from the air plant's gallery to a medium room with some fighting** (the specimen lab, with the encounter `Opening_Future`); **the freight door works and the way goes on** (one more corridor bay, to a door that stays shut for now); **any pit a player can fall into has a way out without the double jump**. A map can now start in the future (provisional: the base has no castle to arrive from yet). §4 "The site".
- v0.48 (2026-11-13): owner: **the future base, slice 1.** "A kind of military base, mostly corridors that lead to small rooms and then to large halls", with Halo and Cyberpunk 2077 as references; coherent floors, level changes, stairs where fights happen; "the structure of the kits' own demo maps is your reference". Built as map `FutureBase`: the room where the player arrives, a corridor with two side rooms, and a combat hall (plan and what version 1 really builds: `Design/Levels/FUTURE_BASE_SLICE1.md`). The owner accepted the approach ("Sí, sigue") and asked for a package that starts on it. Everything about it is provisional until he has walked it. §4 "The site".
- v0.47 (2026-11-13): owner: **the first maps of both eras are rejected and deleted.** "Neither is of any use"; the assets "were used indiscriminately and at random, not with sense nor looking for coherence in the world". Deleted: the maps `Site`, `Site_Past` and `Site_Future`, their build and dressing scripts, the world plan of v0.46 and the smoke block that checked the map against it. The owner's direction of v0.46 stands (castle in the past, military base on the same site in the future, wide rooms, wave arenas, the tentacle mini-boss); the plan's layout, route and names do not. The game starts on the test arena again. §4 "The site".
- v0.46 (2026-11-13): owner: **the real map**, the same site in two times. The past is the **medieval castle**; the future is a **futuristic military base** on the same site. Besides the intro and the Valkyrie, the future has **several wave arenas** and a **mini-boss: the tentacle machine** already in the project. Combat is **fast and wide** (Cyberpunk 2077 style): the main fights happen in **large rooms**, only a few in corridors. The map is a bit bigger than "compact"; the Blender spaceship corridor is prepared and used. Spaces are built from the **kits' modular pieces**; only the castle of the medieval pack is kept (it will be converted to pixel art). The plan, drawn by a Claude Fable 5.2 planner, is in `Design/Levels/WORLD_PLAN.md` (§4 "The site"); every zone name, lock rule and reward in it is provisional.
- v0.45 (2026-11-13): owner: **climbing does not need the view on the wall** ("with your back to it, for example"). The climb now follows the player's intent: the held direction on the ground, the jump's direction in the air, and all around when there is none. When the wall is behind or beside the view, the view turns to it during the climb (architect, provisional; §18 "Ledge climb").
- v0.44 (2026-11-13): owner: **the player climbs ledges**. A jump that ends at the edge of a structure climbs onto it, and pressing jump right against a block climbs it instead of jumping when one jump can reach its top. In first person the climb is the camera dipping over the edge, the held item dropping out of view and the two pixel hands gripping the ledge. Architect, provisional: the reach, durations and the "facing the wall" rule (§18 "Ledge climb"); the test arena's gates were re-sized for the extra reach.
- v0.43 (2026-11-13): owner: **the weapon enchant's element is chosen in its QTE**, always in the same places (never shuffled): a first step shows fire (A / Square), lightning (W / Triangle) and ice (D / Circle); the key picks it, then the usual key sequence and gesture follow. Also from the owner the same day: menus play a transition when their look changes, every skill point asks for confirmation, and the weapon tree is a hub with gun pages (v0.39 to v0.42).
- v0.42 (2026-11-13): owner chose three perk changes the architect proposed from the balance table: the pistol's Steady Grip becomes **Quick Draw** (aim 20% faster and move 10% faster while aiming, per rank), the shotgun's Steady Grip becomes **Extended Tube** (more shells), and the rifle gets **Extended Mag** (+5 rounds per magazine per rank). Measuring them showed that a bigger magazine also carries more ammo (spare magazines hold the bigger magazine; the shotgun's reserve is magazines x tube size), so the architect set Extended Tube to **+1 shell per rank** instead of the +2 shown to the owner: +2 doubled the shells carried, twice any other perk. Details in §18 "Weapon tree".
- v0.41 (2026-11-13): owner: **Marksman, Precision Recovery and Arcane Conduit are general again, as at first** (they work on every gun, Marksman also on the staff and the sword) and sit on the weapon hub with Heavy Rounds; Deadeye stays next to Marksman (they stack, as before); Tight Choke stays. The architect measured every weapon perk with the balance tool (new table "what one point buys") and tuned the new per-gun perks to the existing ones: **Rapid Reload -15% per rank** on each gun (was -10% on all guns at once), **Tight Choke -15%** per rank. Arcane Conduit needs only Heavy Rounds now. Details in §18 "Weapon tree".
- v0.40 (2026-11-13): owner chose **perks per gun**: each gun page has its own perks, and the shared ones ("All guns") are gone. Every gun gets its own Quick Trigger, Rapid Reload and Steady Grip; the pistol keeps Deadeye -> Marksman -> Precision Recovery, the rifle Long Shot -> Arcane Conduit (now rifle-only), the shotgun Point Blank -> a new Tight Choke (tighter pellet spread). The weapon tree goes from 36 to 57 points. Side effect found with the balance tool: the old "All" fire-rate and reload perks also sped up the staff and the sword, so the Entity fight grew from about 191 to 214 s; **each Entity phase goes from 8,000 to 7,300 health** (about 193 s again). Details in §18 "Weapon tree" and §19.
- v0.39 (2026-11-13): owner, after playing the package: **the skill trees are two pages** (tabs; Q / E, L1 / L2 / R1), and each page takes its tree's whole look (spells = RPG, weapons = cyberpunk); the other menus both eras share take **the current era's look** (past = RPG, future = cyberpunk) instead of the held item's; I opens the inventory; the spell page gets a calmer, darker background. **The weapon tree becomes a hub with three gun branches, Cyberpunk style:** Heavy Rounds on top, then Pistol, Rifle and Shotgun; choosing one opens that gun's page, with a Back button. Architect, provisional: the nine existing perks are regrouped by theme with their effects unchanged (§18 "Weapon tree").
- v0.38 (2026-11-13): owner, after seeing it in the arena: **no baked explosion on the Fireball's impact**. The Fireball keeps the baked fire in flight; where it lands it shows the shockwave alone, as before. The explosion clip stays in the plugin for whoever wants it later (§18 "Baked fluid effects").
- v0.37 (2026-11-13): owner: a complete rework of the HUD and menus ("it works but it is basic and ugly"), each skin true to its style, Cyberpunk 2077 as the reference for the cyberpunk one. Both skins are built (§13 "As built"): a carved stone, iron and gilt status bar in pixel art with the game's own pixel font, and a red / cyan cyberpunk HUD with cut corners and condensed type; menus, QTE, death screen, toasts, crosshair and damage numbers follow the held item's skin; ten pixel spell icons. Architect, provisional: the inventory follows the held item's skin; death headlines "YOU HAVE FALLEN" and "FLATLINED".
- v0.36 (2026-11-13): owner: **the Fireball uses the baked fire**. In flight it is the baked fire clip instead of its small sprite; where it explodes the baked explosion plays on top of the shockwave (the explosion is the architect's addition, provisional). Sizes and speed are data in `Spells.json` (§18 "Baked fluid effects"). No other spell is changed.
- v0.35 (2026-11-13): a tool for the game's magic effects, the **GBPixelVfx** plugin (owner's idea; the architect built the minimal version): it bakes a realistic Niagara Fluids effect (fire, smoke, explosion) into a pixel-art flipbook in the master palette and plays it in the game as a camera-facing sprite with a real light. Four clips are baked. **None is used by a spell yet: that waits for the owner's OK.** Rules in §3 "The pixel-art look" and §18.
- v0.34 (2026-11-13): the shotgun is in the game with its 3D model, its pump and its shell-by-shell reload (§18 "Reload"): firing stops the reload at once, a tap on an empty gun fires when the first shell lands, the reserve reads "x24 shells". Owner: its energy bar stays on top of the receiver.
- v0.34 (2026-11-13): the Entity's phase 1 as built (2.5x, a 3D floating boss). Owner: its combat is **pending** polish, including **tentacle melee**. Added to §11 and §17.
- v0.33 (2026-11-12): owner, on the shotgun: it loads **shell by shell** (not a magazine swap), and its **forend pumps** after every shot. Its model is a Tripo science-fiction shotgun. Rules and the architect's provisional numbers in §18 "Reload".
- v0.32 (2026-11-12): the pixel-art line is defined: "dark fantasy hi-bit pixel art, 16-bit inspired", from 9 sprites drawn by hand on the LAPTOP (Valki, axe, sword, two shields, four staffs). Rules, master palette and checklist in `Design/VisualStyleGuide.md`, linked from §3.
- v0.31 (2026-11-12): owner picked the Entity's first model (Tripo, variant #26): a metal shell with electronic-light eyes, and hologram tentacles. Phase 1 art in §11; its size is provisional.
- v0.30 (2026-11-12): owner: the pistol's energy bar must match the enchant's duration; the weak-point light lasts half as long.
- v0.29 (2026-11-12): owner, after reading the balance suggestions: Long Shot goes from +0.15x to +0.2x per rank (the rifle perk now matches Point Blank); the free skill reset will not be in the final version.
- v0.28 (2026-11-12): owner asked the architect to balance every number of the game with maths (the skill-tree percentages stay as they are) and to leave no hardcoded value. New §19 "Balance": the method, the enemy templates (swarm, soldier, elite, slow tank, sub-boss, the two Valkyries, the Entity), resistances and weaknesses, the ammo and mana economy, and a budget of 36 skill points. The numbers in §18 are updated. The last gameplay numbers that lived in code moved to the data files. A calculator, `Tools/Balance/balance_model.js`, prints every table from the data. Reviewed by a second model before the first build.
- v0.27 (2026-11-12): owner: the spell tree is reorganized in four columns (fire, lightning, ice, casting) with only two links kept; Precision Recovery costs 2 points per rank. See §18 "Spell tree".
- v0.25 (2026-11-12): owner approved the first real gun model: the pistol replaces its placeholder (§8 "Guns"). Architect, provisional: its glow follows the enchant element and its energy bar shows the enchant time left.
- v0.24 (2026-11-12): owner: the weapon tree is reorganized (Heavy Rounds on top; a handling branch and a precision branch; three weapon specializations on one row) and gets four new perks: Rapid Reload, Precision Recovery, Arcane Conduit, Deadeye. See §18 "Weapon tree".
- v0.23 (2026-11-12): owner: guns are less accurate while the player moves than while standing still.
- v0.22 (2026-11-12): owner's spec: **elemental build-up** (fire, lightning, ice) on every enemy, with three status effects (Ignite, Shock, Frost), per-enemy resistances and reactions, a weak-point bonus for weapon hits, and every number in data. Owner's answers: the old small effects on every hit (burn, the lightning slow, the enchant's chill) are **removed**; the status effect at the full bar is the only elemental effect (the ice floor keeps its zone slow). The elemental sword and the ember staff build up their element too. See §6 "Elemental build-up".
- v0.21 (2026-11-12): owner: a strong hit (30% of health or more) interrupts the flamethrower, with a camera shake; the spell description says "a strong hit" without the number.
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
- **Style guide:** `Design/VisualStyleGuide.md` ("dark fantasy hi-bit pixel art, 16-bit inspired"): canvas and scale, outline, light from the top left, a master palette of about 50 colors in ramps per material and element, color budget per sprite, the staff variant system and a checklist. Built from the first hand-made sprites (2026-11-12).
- **Pixel-art environments are 3D geometry with low-resolution pixel textures** (nearest filtering, no smoothing), not flat 2D.
- **Pixel-art characters are sprites that always face the camera.** Target: **8 view angles**; fewer if time runs short.
- **Headshots on sprites:** the head is the top part of the sprite. The head / weak point bonus applies to **every aimed weapon attack** (owner, 2026-11-12): guns, the staff's basic attack and the sword. Each weapon has its own multiplier.
- **Ground spells are drawn on the ground** (owner, 2026-11-12): a spell that marks an area of the floor (lightning rune trap, ice floor) shows its pixel-art figure lying flat on the ground and stretched over the whole area it affects, so its size on screen is its real reach. It is not a small camera-facing sprite in the middle of a colored disc.
- **Fluid effects can be baked from a simulation** (owner's idea, 2026-11-13; plugin `Glyphbound/Plugins/GBPixelVfx`, see its `README.md`). Fire, smoke and explosions are hard to draw by hand, so a Niagara Fluids simulation is rendered once on a development machine and converted to a pixel-art flipbook: master-palette ramps, hard alpha, held frames, no screen filter. The game ships only the flipbook; the sprite carries a real point light that follows the effect's brightness, so it lights the scene, casts shadows and shows in reflections. A baked clip is still a sprite and follows this section's rules (it faces the camera; a ground effect lies on the ground).
- **Structures are never flat sprites** (owner, 2026-11-12). Anything that is a structure, the castle and also what magic builds (the ice wall), is a 3D object with 2D pixel textures. Camera-facing sprites are for characters, items, projectiles and effects (flames, bolts).

## 4. World and travel
- **One map, two times** (past and future). The player travels between them **only through predefined portals** placed on the map.
- **Era-gated zones:** some zones are reachable in one era and not in the other.
- **Cross-era unlocks:** some zones must be unlocked in the past so they open in the future (cause in the past, effect in the future).
- These interactions are few (short jam) but they exist and are part of level design.
- **Nothing is lost when changing era.** Health, mana, ammo, inventory, spells and weapons all carry over. Mana exists and works in the future; guns work in the past.
- **Era travel restores everything** (health, mana, ammo), a bit like the rest points of classic action RPGs. Portals are the safe points of the map.

- A third, separate map unlocks late: a **dimension between space and time** where the true final boss lives (see §11).

### The site (owner, 2026-11-13)
- **Past:** a medieval castle. **Future:** a futuristic military base built on the same site, on the castle's wall lines, so the player recognises it.
- **The first attempt (owner, 2026-11-13, v0.47).** The first attempt at both eras was rejected whole and deleted: the three maps (`Site`, `Site_Past`, `Site_Future`), the scripts that built and dressed them, and the world plan they came from (`Design/Levels/WORLD_PLAN.md`, `world_layout.json`, the drawings; all still in git history, commit `ac54fbc` onward). The owner's reason: the kit assets were placed indiscriminately and at random, with no sense and no search for coherence in the world. The game runs on the code-built test arena again.
- **What stands:** the line above (the owner's direction) and the rest of §4. **What does not:** the room layout, the route, the zone names, the lock rules and the module lists of the deleted plan. Nothing of them is to be reused as a starting point.
- **The future base, slice 1 (v0.48, provisional).** Map `/Game/Glyphbound/Maps/FutureBase`, built by `Tools/Jam/Build-FutureBase.py`; plan in `Design/Levels/FUTURE_BASE_SLICE1.md`. The anomaly's control room where the player arrives (the seal on a plinth among the watch consoles), a 10 m wide corridor with an air plant (two floors, its own stair) and a store room off it, and the transfer bay as combat hall (catwalk 3 m up with a stair at each end, a bridge to a lookout, crates and machine blocks as cover). No enemies, portal or pickups yet. From the air plant's gallery a door leads to a medium room, the lab kit's own room made generic (no figures, holograms or tubes: owner, v0.52), where the first fight of the future will start (`Opening_Future`, switched off for now); the bay's freight doors open on approach and the way goes on through a long service corridor (40 m; cables on its grid floor, pipes under its ceiling, wall screens with the seal's status) to its far door, which opens onto the **sub-boss hall** (v0.52): 28 x 36 m, 12 m high, galleries 4 m up along both sides and over the entry for enemies that walk and shoot, crates on the floor (some stacked) that are also the only way up to the galleries, every step within the player's single jump, and a freight door on the far side that opens when the sub-boss is beaten. The sub-boss itself is **TBD**. **Behind the transfer bay's east wall, a bulkhead of thick dirty glass in a heavy steel frame (v0.57; it was a shutter, then a sheet of frosted glass), whose eight panes break one by one in four hits each, each into pieces, while frame and bars stay; a lower pane is the way through (v0.58)** (v0.53), a wing 6 m down, under water: a stairwell that is gone down by a hand ladder and a dock (v0.54), a worn corridor with one or two failing lamps, a small room that will be infested, a corridor in complete darkness (to be filled with smoke) and, at the end, the hall of **the tentacle boss**: large, with galleries that do not go all the way round, wrecked, and a dead end. Which boss the tentacle boss is, and its fight, are **TBD**. **In the medium room, an energy gate** in the dais's end wall goes out when two **Power Cells**, dropped by enemies (which ones: **TBD**), are set in the two devices beside it; behind it is **the Valkyrie's room** (the lab with its tubes and the holograms of its figures, after a battle; the solid figures are not used: owner, v0.55), and her cryogenic tube is in sight through the gate from the first time the player enters the medium room. From the lab kit, the figures, their holograms and their tubes are the look of the Valkyrie's fight and appear nowhere else (owner, v0.50, made precise in v0.52); its rooms and plain furniture can be used. **The game starts on this map, in the future, for now**, so the owner can walk it; the smoke runs still use the test arena. The past (castle) has no map.
- **The castle, plan version 0 (v0.59, provisional, not built).** `Design/Levels/PAST_CASTLE_PLAN.md`. The owner's direction: the future base's style (ways up and down, fights on two levels, obstacles, variety) with what a castle has of its own: a spiral stair in a tower, a floor above another that do not connect, a long passage between two towers; built from the castle pack's modular pieces and on the structure its author already assembled. The architect's proposal, waiting for the owner: **Blackwall Castle** (provisional name), a keep standing over the crypt that holds the seal, with orcs camped in it; a gatehouse, a lower and an upper ward, the keep's Front Hall and King's Hall, towers with spiral stairs joined by a long wall walk, an upper floor reached only along that wall, cellars with the well. **Slice 1 is the opening sequence:** gate, wards (the first fight), Front Hall with two small rooms, the crypt stair past the cells, the seal's hall. Where each element lives and where the medieval Valkyrie is bound: **TBD** (a starting proposal is in the plan). The castle pack's assembled castle and most of its kit were removed on 2026-11-13 and are in the Recycle Bin; they are restored and measured before anything is drawn to scale. **Version 1 (v0.60, to scale, not built):** the castle part of the pack is restored and measured, and the plan now stands on the keep as its author assembled it, placed whole: Front Hall between two entrance towers with spiral stairs, Main Hall with a grand stair of 25 m, King's Hall and Side Hall as wings (each with a gallery 9 m up that has no stair of its own), and a Top Level of three floors with four spiral turrets. Heights above the lower ward: crypt -9.8 m, upper ward +2.1, keep floor +2.4, wall walk +10, galleries +11.7, roof walk +20.7, Top Level from +26.5. What the plan adds: the gatehouse, wall towers and wall stair (the author's own sections), two wards 52 m wide between curtain walls, and the crypt from the pack's plain second interior set (stair of four flights from a rear corner of the Main Hall, cells, the seal's hall). Tower and room doors are about 1.3 m wide, so fights stay in wards and halls. How the castle's photographic stone becomes pixel art (§3) is **TBD**: a test on one section goes to the owner first. Drawing: `Devlog/media/2026-11-14/castle_plan_v1.png`.
- **Level rule (owner, v0.49):** a player who falls into any pit must be able to get out on foot with a single jump; the double jump is learned later.
- **Rule for map work (architect, from the owner's reason):** a place is decided before a piece is placed. Each space is first described as a place in its world (what it is, who built it, what it is for, what it looks like), and an asset goes in only because that description asks for it. The next map is planned step by step with the owner.

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
  - **Weapon tree:** damage, fire rate, **recoil** (better aim), **damage by distance** (shotgun stronger up close, rifle stronger far away), **headshot damage**, reload speed, healing on precise kills, enchant build-up. Layout in §18 "Weapon tree".
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
| **Fire** | Fireball (projectile; **always explodes with a shockwave**, on an enemy, on the world or at the end of its range) | Fire wall | Flamethrower (sustained DPS; **runs while the spell key is held**, until it is released or the mana runs out; **a strong hit interrupts it**) |
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
- **Element:** chosen by the player **in the enchant's QTE** (owner, v0.43). Its first step shows the three elements, always in the same places: **A / Square = fire, W / Triangle = lightning, D / Circle = ice** (W sits a little higher, as on the keyboard), with the current element marked. The key picks it and the usual part 1 and part 2 follow. The step has its own time (`QTE.ChoiceTimeLimit` in `QTE.json`, 3 s, provisional) so it never eats the QTE's own time; if it runs out the element stays as it was. With the QTE switched off, the element is the one set in the inventory's Spells page (that picker stays).
- **Uses the QTE** like every other spell (§7).
- **Does not work on the elemental sword.**
- Duration, cost and exact buff per element: **TBD**.

### Elemental build-up (owner's spec, 2026-11-12)
Every enemy has three independent bars: **fire, lightning, ice**. A valid hit of an element fills its bar; a full bar starts that element's status effect.
- **Rules for each bar:** its own threshold, duration, immunity time and decay. After a while without new build-up the bar drains little by little. When the status starts the bar goes back to zero; while the status runs that element does not build up again; after it ends there can be an immunity time before it builds up again. The three bars never affect each other. No reactions between elements yet.
- **Ignite (fire, sustained damage):** fire damage at fixed time steps for the status duration. Sources: fireball, fire wall, flamethrower (on its 0.3 s damage ticks), the ember staff, the elemental sword and a fire-enchanted gun.
- **Shock (lightning, interruption):** an electric discharge (a small hit of lightning damage) and the enemy's reaction: **stun** (stops moving and attacking for the duration, its attack is cancelled), **interrupt** (only cancels the attack it was winding up) or **none**. Sources: lightning bolt (every enemy it passes through, once each), lightning rune (enemies in its blast radius, so Expanded Sigil applies), static orb, the elemental sword and a lightning-enchanted gun.
- **Frost (ice, slow and control):** the enemy's reaction: **freeze** (cannot move or attack for the duration), **slow** (a strong slow) or **none**. Sources: the **ice floor only** among spells (low build-up on each of its ticks, so standing on it fills the bar slowly and an ice-enchanted gun finishes it faster), the elemental sword and an ice-enchanted gun. The ice wall and the ice shield build up nothing. The ice floor's own slow (50%, Deep Freeze 55/60/65%, cap 80%, QTE) is unchanged. **Slows never stack:** when the ice floor and Frost both slow an enemy, only the strongest counts.
- **Per enemy (no global multipliers by enemy type):** each enemy can set, per element, its damage taken, its build-up taken (separately, so they balance apart), its threshold, the status duration, the immunity time, the decay, and its reaction (or immunity to the status: an enemy can be immune to Shock and still take lightning damage). Values not set by an enemy fall back to defaults in `Game.json`.
- **Per source:** each spell has its own build-up per hit or per tick (`BuildUp` in `Spells.json`); each weapon has its own build-up per element (`FireBuildUp`, `LightningBuildUp`, `IceBuildUp` in `Weapons.json`). An unenchanted gun builds up nothing; the apprentice staff (arcane) builds up nothing. One shot counts once per enemy: the eight pellets of a shotgun shot give one build-up to each enemy they hit.
- **Weak point bonus:** a weapon hit on the head or on a weak point multiplies its build-up (`WeakPointBuildUpMult`), only once even if the head is also a weak point. It does not change the head damage bonus.
- **Visuals:** not decided yet (Niagara, shaders or something per enemy). The code only sends events (build-up changed, status started, status ended) that each enemy's look can use later. A debug view (`gb.ShowElemental 1` in the console) prints the bars over enemies for testing; it is not the game's indicator.
- **Removed by the owner (2026-11-12):** the small effects every elemental hit used to apply (burn on fire hits, the 40% slow on lightning hits, the enchant's chill). The bar's status effect is now the only elemental effect. The electric grenade builds up lightning instead of applying the old shock (architect).

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
- **Moving costs accuracy (owner, 2026-11-12):** shots spread more while the player moves than while standing still; the crosshair opens to show it. See §18.
- **Magazines and reload**, like any shooter.
- **Gun models (v0.25):** the pistol is the first real 3D model (made with Tripo on ASSETS, finished in Blender on CENTRAL); the shotgun and rifle keep placeholder shapes until their models exist. The pistol's trigger moves on each shot and its magazine slides out and back in during the reload. **Glow (provisional, architect):** the gun's glowing parts show its own green when not enchanted; while the enchant runs they take the element's colour, and the energy bar on the side shows the enchant time left (full when cast, empty when it ends). **Owner, v0.30:** the bar's 13 cells show the time left rounded up, so the last cell goes dark exactly when the enchant ends (before, it went dark half a cell early); the weak-point light is half as long as before: full for 0.06 s, then a 0.15 s fade (`PistolWeakPointFlashHold` / `PistolWeakPointFlashFade` in `Game.json`).
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
| Jump (against a reachable ledge: climb it) | Space | Cross (A) |
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

- **Vault (v0.58, owner):** a thin obstacle between 50 and 150 cm high with room and a floor beyond it (down to 4.5 m lower) is crossed in about half a second, on Jump or by pushing against it for a third of a second; the view tips, rolls and dips over the top and the held weapon lowers. The ledge climb is tried first. Made for the corridor's railing: 127 cm high under a 3 m ceiling, where the 180 cm player's jump tops out 7 cm short. Numbers in `Player.json` (provisional).

## 10. Enemies and encounters
- **Enemy waves (hordes).** Clearing a wave gives skill points.
- **A couple of sub-bosses.**
- **Two bosses** (the Valkyrie in each era, §11).
- **The true final boss.**
- **Enemies must get around obstacles** to reach the player (owner, 2026-11-12): never stand stuck against a wall.
- Known enemies: **orcs** (past) and **armed orcs** (future). Other types and behavior: **TBD**. Their art follows §3 (medieval = pixel art, cyberpunk = 3D).
- **The future's 3D enemies (v0.56, 2026-11-14, from the ASSETS deliveries; provisional).** All data-driven from `Enemies.json` (meshes, clips, weapon, VFX and tuning by key).
  - **Armed orcs** (`Orc_Future`, `_GunCarrier`, `_Enforcer`; melee `_Siege`, sub-boss `_Warlord`): one orc body, 2.10 m, with **modular armor**: four sets (A plain, B riveted, C bone and gold, D green scales) of up to nine slots (helmet, chest, belt, thigh plates, greaves, upper-arm plates, shoulder pads, gauntlets, boots) worn over the body; each enemy id lists its pieces, so a new orc type is a line of data. Boots were left out at first (v0.56); from v0.57 every set has them.
  - **Robot soldier** (`Robot_Future`, 1.70 m) with the robots' rifle, like the gun orcs.
  - **Scrap Drone** (`Drone_Future`): flies 2.5 m up, circles the player, its red eye tracks him; short laser bursts.
  - **Sentinel** (`Sentinel_Future`, elite): a floating sphere whose blue core is an eye that follows the player; it charges a beam with a visible aiming line that lags behind a moving player, then fires. The core is its weak point.
  - **Robot spider** (`Spider_Future`, swarm): fast, leaps at the player.
  - **Shooter behaviour (owner: "basic first-person-shooter AI")**: an enemy notices the player in a sight cone or when hit, alerts the others nearby, keeps a preferred distance band (closes in, backs off, strafes sideways between bursts, always facing the player), fires **bursts of hitscan shots** with spread (more spread against a moving player) and needs line of sight. Tracers, muzzle flashes and impacts come from the project's weapon VFX packs. Corpses of humanoids fall as ragdolls.
  - Encounters in the future base (provisional, v0.56): the transfer bay, a spider ambush in the long corridor, the sub-boss hall (Warlord; opens its freight door and gives the first **Power Cell**), the deep wing (spiders as the infestation, a sentinel guarding the second Power Cell). Which enemies infest the deep wing is the owner's call.
- **How the future's enemies fight (v0.57, 2026-11-14, owner's review after playing; replaces the "shooter behaviour" line above; numbers provisional, all in `Enemies.json` and `Game.json`).** The owner's words: they must be intelligent, take cover, use different parts of the arena, not stand together, split up and try to surround the player ("shooters already have techniques for that, use them"); they stay in their zones; they are already there, patrolling, and never appear from nowhere; "we do not want enemies that look like a cheap game's".
  - **Already there.** An encounter's first wave stands in its zone from the moment the map loads: some walk a patrol loop, some stand guard and look around, some are dormant (the corridor's spiders, the Valkyrie before her tube). Nothing starts because the player crossed a line: the fight starts when one of them notices him (sight cone held for a moment, his gunfire heard, damage, a squadmate's alert). The one who notices stops and turns before acting, and the squad is alerted one after another, not in the same frame. Later waves are reinforcements: they come in from spawn points the player cannot see, already alerted.
  - **Zones.** Every encounter has a zone (its trigger box and spawn points, with a margin) and its enemies never leave it. A player who walks out is shot at from inside by those who see him; nobody follows. If nobody has seen him for a while they go back to their posts and patrol again.
  - **The squad** (one per encounter): shares the player's **last known position** (an enemy that cannot see him does not know where he is), gives each ranged member a **role** (base of fire, left flank, right flank, assault), reserves a **position** for each so no two stand together, and hands out **attack tokens**: only two or three may be firing at once, and the others use that time to move. That rhythm of fire and movement is the look of the fight.
  - **Positions** are chosen by scoring points sampled on the navmesh inside the zone: cover from the player with a firing spot beside it is best, then an open spot with a line of fire; inside the role's distance band; at an angle from the squadmates as seen from the player (flankers go 60 to 120 degrees round him); a short path that does not cross his line of fire. A member sits in cover, steps out (or stands up from low cover) to fire a burst or two, goes back, and moves on when its cover is flanked, the player gets too close, it has been there long enough, or it is hurt badly. When the player hides, the base of fire shoots at where he was and the flankers and assaulters close in by bounds.
  - **Temperament** (`Aggression`): robots are disciplined (hold cover, flank wide); orcs push (closer range, shorter stays in cover, more assaults).
  - **Movement**: three gaits (patrol walk, combat walk, run), acceleration instead of instant direction changes, facing the way they run and facing the player when they walk in combat or fire. The clips are played in place (they carried their travel in the pelvis and the mesh used to leave its capsule and snap back: the "robots that teleport through everything" of the owner's review).
  - **Melee** enemies take slots on a circle round the player and only two attack at once; **flyers** take orbit slots, keep apart and fly slower (drone 600 to 400).
  - **The horde chief** (`Orc_Future_Warlord`, the sub-boss hall): a bigger orc (1.65) with a **shield** and a **heavy pistol**. The shield blocks everything that comes from his front while it is raised (its hit box is as big as it looks); he walks at the player behind it, turning slowly, lowers it to aim one slow, heavy shot, and raises it again. He is hurt from the sides and the back, while he aims, and when the shield is knocked aside (enough damage on it in a short time, a lightning stun, a freeze). Up close he bashes with the shield. The others flank while he holds the front. His shield and pistol art have not arrived yet (placeholder shield, the rifle for now).
  - **The Valkyrie** stands **inside her tube**, in sight through the energy gate from the start. While its glass is whole she is sealed: she notices nothing and cannot be hurt. The player breaks the glass (four hits); she wakes, leaps out into the room and the fight starts (v0.58, owner). If the player dies and comes back, the glass stays broken and she wakes when he comes near. She fights with her sword (bolts at range, a swing up close) until her own fight is designed.
  - Encounters (v0.57): the transfer bay has robots and orcs on patrol with a drone, then orcs and spiders, then a sentinel and an enforcer; the deep wing has orcs, a robot and spiders, then the sentinel and drones; the sub-boss hall has the chief with two enforcers and two drones.
  - Console, for testing the map: `gb.God` (player invulnerable), `gb.OpenValkyrie` (the energy gate goes out), `gb.OpenDoors`. A test start: `-AllWeapons` on the command line gives every weapon (the package carries `Play_AllWeapons.bat`).
  - After the code review (v0.58): the zone is enforced on the movement itself (paths that leave it are thrown away, no lunge at a player outside); only two melee enemies wind up or lunge at once; reinforcements use only spawn points the player cannot see and wait up to 6 s for one; an alert from a noise or a hit tells the squad where the noise or the hit came from, not where the player is; enemies that lose the player walk back to their posts. The chief's bash throws the player back.
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
  - **Phase 1 art, "the machine" (owner, 2026-11-12):** a floating metal shell with a big central eye and smaller eyes that glow like electronic lights, and about eleven tentacles made of light (holograms) that sway. The central eye is a separate mesh: the weak point. Size **about 2.9 m tall (provisional, architect)**. Assets in `/Game/Glyphbound/Enemies/Entity`; the source and how it was split are described in `SourceArt/Enemies/Entity/README.md`. The hologram tentacles already lean towards the ghost half.
  - **Phase 1 as built (2026-11-13):**
    - the size is now **2.5x, about 9.4 m** (`EntityScale`);
    - it floats and banks, turns with weight and strafes;
    - it fires lit energy bolts from a tentacle tip;
    - the eye is the weak point;
    - the tentacles move with one coordinated stroke.

    The owner played it: "it looks much better".
  - **PENDING (owner, 2026-11-13): polish and improve the combat. The Entity will also fight in melee with its tentacles.** Still to design and build:
    - tentacle strikes and sweeps at close range, with a readable windup;
    - the hit volumes along the tentacles;
    - how the player dodges or blocks them;
    - when it chooses melee or ranged;
    - its balance in §19.

    Today it only has the ranged bolt, and the tentacles cannot be hit.
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
- The HUD switches when the player switches weapon. **The menus (pause, inventory, death screen) follow the held item's skin too** (architect, provisional, v0.37: it answers the inventory's look, which was TBD).
- **As built (v0.37, 2026-11-13).** The owner asked for a complete rework ("it works but it is basic and ugly"), each skin true to its style, with Cyberpunk 2077 as the reference for the cyberpunk one. Everything is still drawn by `AGBHUD` on the canvas (no widget assets).
  - **90s RPG skin** (staff, sword): a status bar of carved dark stone along the bottom with a worn iron rim, a gilt trim line and rivets. A raised crest in the middle holds the five spell sockets: gold bezels, the spell's pixel icon, its mana cost, a cooldown shade with the seconds left, a cold shade when there is not enough mana, the key under each. Life and mana are glass tubes with a liquid ramp and the number beside them (a pale trail shows the life just lost; the heart glows under 30 %); dash is a thin gilt tube. The quick-use item sits in a gold socket at the right, with the skill points and a gem. Active effects and the summoned shield are small stone plaques above the bar. The hands stand on the bar, as in the 90s games. Everything is laid out on a grid of whole art pixels (4 screen pixels at 1080p, 3 at 720p) in the master palette of `Design/VisualStyleGuide.md` (obsidian, iron, bone, blood, ice; a gilt ramp made from the wood and bone ramps), with **the game's own 5 x 7 pixel font**, drawn for the game during the jam and baked into gold, bone, blood, ice and ember inks.
  - **Cyberpunk skin** (gun): coral red lines and condensed capitals on near-black, cyan for values and keys, yellow for alerts. Top left: the health number in a cut-corner box and a slanted, segmented health bar (a pale trail for the health just lost; yellow and "CRITICAL" under 30 %, the screen edges pulse red), mana and dash under it, effects as chips below. Bottom right, in a narrow column clear of the gun model: the enchant module (key T, element icon, time left or cost), the element tag and the weapon name over a rule, the magazine count large with the reserve in cyan, one slanted pip per round or the reload bar. Quick-use item bottom left; skill points as a yellow notice top right. Drawing the gun boots the HUD with a short flicker and a chromatic split; a hit tears the screen with scan bands. Type: the engine's own Roboto (condensed bold for headings and numbers) and Droid Sans Mono for small technical labels.
  - **Menus**, in both skins with one layout: a list at the left, a detail panel at the right, key and action hints in the footer. RPG: an iron-framed stone window with a gold title plaque, stone tabs and a gold arrow cursor. Cyberpunk: full screen, a large title at the top left with the tabs beside it, a ruler under them, cut-corner rows, bracketed panels, faint scanlines.
  - **Skill trees** (v0.39): two pages behind tabs, and each page takes its tree's look for the whole screen, whatever the era or the held item. The other menus both eras share (inventory, pause, death) take the current era's look. The spell tree is a tablet with gold-framed nodes, gem pips for ranks and its four element columns named; the weapon tree is a cut-corner circuit with right-angle links and a pulse running down the owned ones. The details of the selected node take its tree's skin.
  - **QTE** in both skins: RPG keys are stone tablets under a gold title plaque with a fuse-like time tube; cyberpunk keys are cut-corner boxes under a "sync sequence" header with a segmented timer.
  - **Death screen headlines (provisional, architect):** "YOU HAVE FALLEN" (RPG) and "FLATLINED" (cyberpunk). Toasts, the interact prompt, the crosshair and the damage numbers follow the skin too.
  - **Spell icons:** ten 16 x 16 pixel icons in `Sprites.json` (`icon_` + the spell id in lower case), drawn by the architect in the master palette; used by the sockets, the spells page and the enchant module.
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
- **Entity combat (pending, owner 2026-11-13):** polish the fight and add tentacle melee. The details are in §11.

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
- **Ledge climb (owner, 2026-11-13; details provisional, architect):** the player climbs onto a structure in two cases. (1) **In the air**, rising or falling, when a wall is right next to the player in the jump's direction (or anywhere around, if the jump is nearly vertical or was stopped by the wall), and its top is between 15 cm and 1.6 m above the feet (the hands' reach): the climb starts on its own. (2) **Jump pressed on the ground** right against a wall in the held movement direction (or anywhere around when no direction is held: back to the wall works; holding a direction away from a wall behind is a normal jump), whose top is between 0.5 m and one jump's rise plus the 1.6 m reach above the feet: the climb starts instead of the jump; a higher wall is a normal jump (and the double jump can still reach a climb in the air). The wall must be met roughly head-on (within 60° of its face). The view no longer has to face it (owner, v0.45): if the wall is more than 60° off the view, the view turns to face it during the rise, so the player ends on top looking across it; the mouse still works during the turn. There must be room for the player on top and above the head, and floor under the landing point. The climb rises straight up, then moves forward onto the top, in 0.22 s plus 0.18 s per metre of rise; it cannot be steered and blocks attacks, spells, dash and jump while it lasts (looking stays free). Enemies are never climbed. The first-person animation: the held item (staff, sword, gun) drops out of view, both pixel hands come up from the bottom of the screen with their sleeves and grip the ledge where it is on screen, the camera pulls down a little and dips 10° (with a slight roll) as it goes over the edge, then the hands let go and the held item comes back. Numbers in `Player.json` (`Mantle*`). Side effect handled in the test arena: the reach makes things higher reachable, so the last course step now needs the double jump even with a climb, the outer walls went from 11 m to 14 m, and the arena warns if the future-only platform (6 m) could be climbed in the past.
- **Double jump on day 1:** the player starts knowing the air passive "Double jump" so it can be tested now. In the real game it is learned in the air zone (§6); remove it from `Player.json` `StartingSpells` then.
- **Dash:** available from the start (the air passive only improves it). It goes in the movement direction (forward when standing still), works in the air, gives no invulnerability, and does not refill jumps.
- **Owning weapons:** decided by the owner for staffs (section 8): picked-up weapons go to the inventory and are equipped by hand. Provisional part: guns and the sword follow the same rule; each owned gun keeps its own ammo while stored.
- **Reload:** the bullets left in the old magazine are lost (classic tactical style). A gun reloads by itself when its magazine runs dry. Ammo pickups give one spare magazine, up to the gun's maximum.
  - **The shotgun loads shell by shell (owner, 2026-11-12).** One shell goes in at a time; nothing is lost by reloading early, and the reload can stop part-way. Provisional (architect): its reserve is counted in shells (4 magazines x 6 = 24, the same total as before); each shell takes `ReloadTime / MagazineSize` (0.33 s), so a full reload from empty still takes the 2 s the balance tables use and Rapid Reload speeds it the same way; pressing fire during the reload stops it and shoots as soon as a shell is in (the shell on its way in is not lost: it stays in the reserve); an ammo pickup gives one magazine's worth of shells (6); an empty shotgun still starts reloading by itself. **As built (v0.34):** pressing fire stops the reload at that moment, even inside the time between shots, and the shot follows when the fire rate allows; on an empty gun the press (a tap is enough) is kept, the first shell lands, the reload stops and the gun fires; a weapon switch, a death or an ammo refill drops that kept shot. The gun's data says it reloads per round (`ReloadPerRound` 1 in `Weapons.json`); the HUD shows the reserve as "x24 shells".
  - **The shotgun's forend pumps after every shot (owner, 2026-11-12).** Provisional (architect): it is only the look; the time between shots stays `FireInterval` (0.8 s) and the pump plays inside it. **As built (v0.34), provisional art values:** it starts back at 22% of the interval, is fully back at 45% and home at 72%; the gun dips 0.5 cm and rolls 1.5° with it and the 2D left hand rides it. From the first-person camera the pump itself is under the gun and hardly seen.
  - **The shotgun's energy bar (owner, 2026-11-13):** it stays on top of the receiver, where the approved model has it, although the camera sees it almost edge-on; the architect offered to move it to the sight's rear panel and the owner declined.
- **Sustained spells (flamethrower), changed by the owner (2026-11-12):** after the QTE the flamethrower runs **while its spell key is held**, until the key is released or the mana runs out (also on weapon switch or death). No time limit. Provisional (architect): it pays a smaller cost at the start (10 mana, always spent, like every QTE spell) and then drains 15 mana per second; if the key was already released when the QTE ends, it still burns for a short minimum (0.75 s) so the cast is never wasted; the cooldown starts at the cast. All values in `Spells.json` (`ManaCost`, `ManaPerSecond`, `MinDuration`).
- **Strong hit interrupts the flamethrower (owner, 2026-11-12):** a single hit that takes **30% or more of the player's maximum health** stops it at once, and the camera shakes. The in-game description says "a strong hit", never the number. Provisional (architect): measured against maximum health (not current health), on the damage actually taken after the ice shield and armor; the threshold is a spell parameter (`InterruptHitFraction`, so any sustained spell can use it); the shake is short (about 0.35 s, a few degrees, values in `Game.json`), only moves the view, never the aim; the mana already spent is not refunded and the cooldown is unchanged.
- **Spread while moving (owner, 2026-11-12; numbers by the architect, provisional):** each gun adds `MoveSpread` (hip fire) or `AimMoveSpread` (aiming) degrees to its spread, scaled by the player's horizontal speed over the current walk speed (0 standing still, full bonus at full walk speed or faster; aiming already lowers the walk speed, so a slow aimed walk counts as full). Pistol 1.2 -> 3.7 hip, 0.25 -> 1.25 aimed; shotgun 6 -> 9, 4.5 -> 6.5; rifle 1.8 -> 4.8, 0.35 -> 1.55 (`Weapons.json`, modifiable by perks as `Weapon.<Id>.MoveSpread`). Jumping counts by its horizontal speed only.
- **Spell tree (owner, v0.27):** four columns, top to bottom. **Fire:** Searing Fireball -> Wide Blast, Lingering Flames, Intense Heat. **Lightning:** Overcharge, Expanded Sigil, Persistent Charge. **Ice:** Glacier Wall, Enduring Guard, Deep Freeze. **Casting:** Frugal Casting -> Quick Recall. Only those two links stay; every other spell perk has no prerequisite (Frugal Casting no longer needs Overcharge). Costs unchanged (Frugal Casting and Quick Recall 2 points, the rest 1).
- **Weapon tree, v0.42 (owner's perk choices):** as v0.41 below, with three changes. **Pistol:** Steady Grip is replaced by **Quick Draw** (aim time x0.8 and +0.1 move speed while aiming, per rank: aiming takes 60% less time and no longer slows the player at rank 3). **Rifle:** new **Extended Mag** under Long Shot (+5 rounds per magazine per rank, 30 -> 45; spare magazines are the bigger size too): +4.8% damage per second per point and +50% damage of the full load at rank 3, the same per point as Long Shot's load gain. **Shotgun:** Steady Grip is replaced by **Extended Tube** (+1 shell per rank, 6 -> 9; the reserve grows with it): +3.6% per point and +50% load, like Extended Mag. +2 shells per rank (the number the owner saw when choosing) gave +100% load, double any other perk; **the owner confirmed +1** ("Dejarlo en +1 si es lo que consideras ideal", "Keep it at +1 if that is what you consider ideal"). All 1 point per rank, 3 ranks.
- **Weapon tree, v0.41 (owner: the three general perks stay general; partly superseded by v0.42 above):** **Hub page:** Heavy Rounds (+10% gun damage, all guns) in the middle; Marksman (+0.25x head / weak point damage with every weapon, staff and sword included, 2 points per rank) -> Precision Recovery (2 points per rank) on the left; Arcane Conduit (+10% elemental build-up from enchanted weapons per rank, needs Heavy Rounds) on the right; the three gun emblems hang from Heavy Rounds. **Every gun page:** its own Quick Trigger (-8% time between shots of that gun per rank) -> Rapid Reload (**-15%** reload time per rank, -45% at 3) and Steady Grip (-20% recoil). **Pistol:** Deadeye (+0.25x pistol head damage; it stacks with Marksman as it always did). **Rifle:** Long Shot. **Shotgun:** Point Blank -> Tight Choke (**-15%** pellet spread, hip and aimed, per rank). All 3 ranks, 1 point per rank unless said.
  - **Balance (architect, `Tools/Balance/balance_model.js`, table "what one point buys"):** each perk alone at full rank on top of Heavy Rounds 3, as expected damage per second of its gun. The unconditional one-gun perks the owner already had buy +5% (Deadeye) to +7% (Quick Trigger) per point; Heavy Rounds +10% per point on all three guns at once; the range perks +17% (Long Shot) and +20% (Point Blank), but only at their best range. The new per-gun Rapid Reload at -10% bought only +2.6% to +4.2%, so it goes to -15% (+4.1% pistol, +6.8% rifle, +5.1% shotgun). The model now also counts accuracy: a provisional share of each gun's misses comes from recoil (pistol 30%, rifle 50%, shotgun 15%) and from spread (15%, 20%, 50%). With it, Steady Grip buys +6.7% on the rifle but only +2% on the pistol and +1% on the shotgun, and Tight Choke at -15% buys +2.5%. Those are the weak spots; replacing them is the owner's call.
- **Weapon tree, v0.40 (superseded by v0.41 above; owner: "perks per gun"):** the hub of v0.39 stays (Heavy Rounds, then the Pistol, Rifle and Shotgun pages), and every perk now belongs to one gun. Each gun page: **Quick Trigger** (-8% time between shots of that gun per rank) -> **Rapid Reload** (-10% reload time, -30% at 3) and **Steady Grip** (-20% recoil); plus its signature chain. **Pistol:** Deadeye (+0.25x pistol head / weak point damage) -> Marksman (+0.25x head / weak point damage with every weapon, staff and sword included, 2 points per rank) -> Precision Recovery (2 points per rank). **Rifle:** Long Shot (+0.2x damage far) -> Arcane Conduit (+10% elemental build-up of the enchanted rifle per rank). **Shotgun:** Point Blank (+0.2x damage up close) -> **Tight Choke** (new, provisional: -10% pellet spread, hip and aimed, per rank). All 3 ranks, 1 point per rank unless said. The first perk of each chain needs Heavy Rounds. Marksman stays global on purpose: it is the one weak-point perk of the staff and the sword. The weapon tree costs 57 points (was 36), so a player can finish about one gun and a half, as the owner wanted.
- **Weapon tree, v0.39 (superseded by v0.40 above; owner: a hub with gun branches, "similar to Cyberpunk"):** Heavy Rounds stays the first perk, on the hub page. Under it, three gun emblems: **Pistol**, **Rifle**, **Shotgun**. Choosing one (Enter or a click) opens that gun's page; a Back button (and Esc / B) returns to the hub. **Provisional (architect):** the nine perks below keep their effects, costs and ranks exactly (so §19's balance and the 36-point budget are unchanged) and are regrouped by theme: **Pistol** (precision) = Deadeye, Marksman -> Precision Recovery; **Rifle** (sustained fire) = Long Shot, Quick Trigger -> Steady Grip; **Shotgun** (close range) = Point Blank -> Arcane Conduit, Rapid Reload. The first perks of every branch need only Heavy Rounds. Two prerequisites changed so each branch stands alone: Rapid Reload now needs Heavy Rounds (was Quick Trigger) and Arcane Conduit needs Point Blank (was Marksman). The perks that work on every gun say "All guns" in their description. Gun-specific versions of the shared perks are a possible next step if the owner wants each branch to touch only its gun.
- **Weapon tree (owner, v0.24, superseded by v0.39 above):** Heavy Rounds (+10% gun damage) on top. **Handling** branch (left): Quick Trigger (-8% time between shots) -> Rapid Reload (-10% reload time per rank, -30% at 3) and Steady Grip (-20% recoil per rank, now pitch and yaw). **Precision** branch (right): Marksman (+0.25x head / weak point damage) -> Precision Recovery and Arcane Conduit (+10% elemental build-up from enchanted weapons per rank, +30% at 3; the sword's own element does not count). **Specializations** (bottom row, all needing only Heavy Rounds, so no gun is favored): Deadeye (pistol, +0.25x head / weak point damage per rank: 2.0 -> 2.25 / 2.5 / 2.75), Point Blank (shotgun, +0.2x up close), Long Shot (rifle, +0.2x far; owner, v0.29, was +0.15x). Every perk: 3 ranks, 1 point per rank (Marksman 2, Precision Recovery 2 since v0.27). **Precision Recovery:** killing an enemy with a head / weak point hit heals a fixed **3% of max health**, with an internal cooldown of **6 / 5 / 4 s** by rank; once per enemy death (a shotgun's pellets never heal more than once), never for a head hit that does not kill (in a horde game that would be near-unlimited healing). Provisional (architect): the prerequisites inside each branch (the owner's sketch only shows the arrows Heavy Rounds -> both branches, Quick Trigger -> Rapid Reload, Marksman -> Precision Recovery; Steady Grip hangs from Quick Trigger and Arcane Conduit from Marksman); only the killing blow counts, from any weapon, spells excluded (they never flag weak points).
- **Flamethrower damage ticks (owner, 2026-11-12):** damage is dealt in fixed time steps, never per frame, so it does not depend on the frame rate: one tick every **0.3 s** (`TickInterval`), each dealing `DamagePerSecond x 0.3` (the mana drain follows the same ticks). If a frame is longer than a tick, the missed ticks still happen.
- **New spell perks (owner's list, placement by the architect):** in the spell tree, each under its element's column (fire, lightning, ice), with no prerequisite so they can be tested right away. Deep Freeze: the ice floor slows by 50% at normal QTE power (the slow effect, `StatusEffects.json`), so +5 points per rank makes 55 / 60 / 65%; the cap is **80%** (also reached by a perfect QTE with 3 ranks). The cap is a general rule: any spell parameter can declare a `Cap` in `Spells.json`, applied after QTE scaling and perks.
- **Ice shield QTE:** part 1 (power) scales its duration and health; part 2 (reach) scales how much frontal damage it blocks.
- **Modifier keys by weapon type:** besides one weapon (`Weapon.Pistol.Damage`) and all weapons (`Weapon.All.Damage`), a modifier can target a slot: `Weapon.Gun.*`, `Weapon.Staff.*`, `Weapon.Sword.*`. The enchant uses `Weapon.Gun.Damage`, so it never buffs the staff or the sword.
- **Explosions and enemy melee need a clear line** to their target: no damage through walls or through the ice wall.
- **Enemies and cover (after the first play-test):** enemies walk around obstacles. A melee enemy only starts its attack when it sees the player; a ranged enemy that loses sight stops aiming and moves until it sees the player again. Enemies do not walk off ledges on their own; one that falls out of the map dies without drops.
- **Projectile explosions push enemies back** (knockback is a spell parameter, scaled by QTE power); they do not push the player.
- **Falling:** the kill height is 2,500 cm below the floor of the level (`Game.json` `KillZ`).
- **Weapon enchant QTE:** part 1 scales how long the buff lasts; part 2 scales how strong it is. Only one enchant at a time. The enchanted gun deals the element's damage type and builds up that element's bar on every hit (§6 "Elemental build-up"; changed by the owner on 2026-11-12, it used to apply a small effect on each hit).
- **Hand ladders** (owner, 2026-11-14): the player stands at a ladder's foot or head and presses Interact ("Climb up" / "Climb down"); the climb takes the ladder's height at 3 m/s plus half a second to step on or off, the held item is lowered and the view turns to the ladder; nothing else can be done during it. A ladder is used only from its own end and floor. Enemies do not use ladders.
- **Things that break** (owner, 2026-11-14): a breakable wall counts hits, not damage (four by default), so any weapon opens it in the same number of blows; a shotgun blast or a flame counts once. It cracks a little more with each hit and stays broken.
- **A spell always appears** (owner, 2026-11-14): no result of the quick-time event and no room may make a spell not show. A spell that stands on the ground (wall, zone, trap) looks for its ground from where the caster is, never through a ceiling; a wall that does not fit at its full distance stands in front of whatever is in its way. It may poke through a ceiling or a side wall that is smaller than it.
- **Spell walls:** the fire wall does not block; it damages enemies that touch it and builds up fire. The ice wall blocks movement and shots and has health; anyone's shots can break it, the player's included. Enemies do not attack walls on purpose yet.
- **QTE:** a wrong key consumes its step. A gesture is judged when the hand stops for a moment or the stroke is clearly complete. If both parts are fully missed the spell fizzles, and the mana is still spent and the cooldown still starts.
- **New spells** go to the first free loadout slot.
- **Ground spell figures (architect, after the owner's note):** the figure is the spell's sprite stretched to the diameter of its area, a few cm above the floor, keeping the pixels sharp; transparent pixels show the floor. It does not turn with the camera; it keeps the yaw the player was facing when casting. The rune shows dimmer until it is armed. The old colored disc is gone.
- **Overlapping ground spells (owner, 2026-11-12):** the figure placed last is drawn on top of every figure it overlaps, completely (an ice floor cast over a rune hides the rune where they overlap; a rune cast over an ice floor shows over it), and overlapping figures never flicker. Provisional (architect): each new figure sits a little higher than the highest figure it overlaps (1.5 cm per layer, values in `Game.json`); this is only how it looks, the spells' areas and effects do not change.
- **Skill reset (owner asked for it, details provisional):** a "Reset skills" button on the skill tree screen (K, and the Skills page of the inventory), also the R key (Y / Triangle on a gamepad) while that screen is open. Free, any time, no confirmation, refunds exactly the points that were spent (even if a node's cost changed in `Skills.json` since). **Owner (2026-11-12): the free reset will not be in the final version**; it stays for testing and is removed or hidden before the submission, like the "+1 skill point" button.
- **"+1 skill point" button (owner asked for it, to test perks):** on the same screen, left of the skill points; each click adds one free point to the pool (no key, no limit). A reset refunds only what was paid, so free points stay. It is a test option: it must be removed or hidden before the jam submission (provisional, architect).
- **Elemental sword:** the hilt and the gems are not consumed when forging, so the player re-forges from the inventory to switch element.
- **Enemies:** once hit, an enemy chases the player from any distance. Enemies of the other era are frozen and hidden, not simulated.
- **Encounters:** if the player dies during one, its living enemies are removed and it starts again from wave 1 on return. Progress flags are never rolled back.
- **Checkpoints:** saved at the start, on every era travel, on entering a checkpoint zone and when an encounter is cleared. Death sends the player back to the checkpoint's place and era with full health, mana and ammo, and **keeps everything collected since** (items, weapons, spells, skill points): pickups and chests do not come back, so rolling the inventory back would lose rewards for good (found by the code review).
- **Consumables** can be used at full health or mana (they are wasted).
- **Baked fluid effects (GBPixelVfx, architect, 2026-11-13; all provisional):**
  - Clip frames are 48 x 64 or 64 x 64 pixels at **12 fps**, larger and faster than the style guide's sprites (6 fps): fluid motion reads badly below that. Colors come only from the master palette's fire, bone, smoke and arcane ramps.
  - The owner asked for "ray tracing behaviors" without detail. The architect read it as: the baked effect must still behave as a light source in a Lumen or ray-traced scene (lights the scene, casts shadows, shows in reflections). **To be confirmed by the owner.** Hardware ray tracing stays off in the project; the plugin's showcase can run with it through a command-line override.
  - The four clips (fire, smoke column with fire, explosion, arcane fire) are a demonstration. Which spell uses which clip, at what size and light strength, is the owner's call.
  - **Fireball (owner, 2026-11-13: "Fireball with fire").** Any projectile spell can name two clips in `Spells.json`: Strings `FlightVfx` (shown instead of the sprite while it flies, centred on the flame's bright core, always facing the camera) and `ImpactVfx` (a one-shot played where it explodes; the shockwave that shows the real blast radius stays). The Fireball uses `FluidFire` in flight and **no impact clip** (owner, 2026-11-13: the architect had added `FluidExplosion` on impact as a provisional extra; the owner saw it and removed it). Provisional number (Params): `FlightVfxHeight` 110 cm for the clip's whole frame. A spell that does name an `ImpactVfx` can size and time it with `ImpactVfxSize` (frame width as a multiple of the blast diameter, default 1.4) and `ImpactVfxRate` (playback speed, default 1.5). These lights do not cast shadows (too costly on a fast projectile). Removing the string gives back the old look. The staff's basic attack keeps its sprite.
- **Art rule on pickups:** cyberpunk items (injection, grenade, magazine, sword hilt) lie on the floor as 3D shapes; medieval ones (potions, gems, books, staffs) as pixel-art sprites.

### Numbers (first pass, all in the JSON files)
- **Jumps:** each jump rises about 2 m (was 1.2 m); the double jump adds a second one of the same height.
- **Head / weak point bonus per weapon** (owner, 2026-11-12): pistol x2; shotgun, rifle, staffs and sword x1.5. **Perks can raise it**: the weapon tree node "Marksman" adds +0.25 per rank (3 ranks) to every weapon, staff and sword included (provisional numbers). Spell projectiles get no head bonus unless a spell defines one.
- **Fireball shockwave:** pushes enemies away from the blast (strength scaled by QTE power).
- **Player:** 100 health (regenerates 3/s after 6 s without damage), 100 mana (6/s after 1.5 s), speed 750, dash 7 m in 0.18 s every 1.2 s. Health and mana stay at 100 for the whole game (no levels), so every hit reads as a percentage (§19).
- **QTE:** 4 keys, 1 gesture, 4 s in real time (raised from 3 s by the owner), world at 10% speed.
- **Staff:** Apprentice staff 12 damage every 0.35 s, no mana cost. Ember staff 22 fire damage every 0.6 s and builds up fire.
- **Guns (v0.28):** pistol 20 damage every 0.25 s, 15 rounds, 6 spare magazines; shotgun 8 pellets of 10 every 0.8 s, 6 rounds, 4 spare, strong up close; rifle automatic, 12 damage every 0.1 s, 30 rounds, 5 spare, stronger far away. Headshots: pistol x2, shotgun and rifle x1.5. Elemental sword 50 every 0.5 s. Why: §19.
- **Enemies:** see the templates in §19 (v0.28).
- **Status effects:** slow 50% for 4 s (ice floor); health potion 60 over 10 s; injection 35 at once; enchant +25% gun damage for 20 s. Burn, chill and the old shock were removed (v0.22).
- **Elemental build-up (first pass, to balance; defaults in `Game.json`, per enemy in `Enemies.json`):** threshold 100 for each bar; decay starts 2 s after the last build-up and drains 20 per second. Ignite 4 s, 5 fire damage every 0.5 s (40 in total, times each enemy's `IgniteDamageMult`), then 3 s immune. Shock: a 15 lightning damage discharge and a 1.2 s stun, then 4 s immune. Frost: 2.5 s freeze (or a 60% slow for enemies that resist freezing), then 5 s immune.
- **Build-up per source (v0.28):** fireball 40 (25-55 with QTE power), fire wall 15 per 0.25 s tick, flamethrower 8 per 0.3 s tick, lightning bolt 35 (20-50), lightning rune 60 (35-85), static orb 12 per tick, ice floor 5 per 0.5 s tick (60 at most over its 6 s, so it never fills the bar alone), electric grenade 60. Weapons per hit: pistol 15, shotgun 40 per shot, rifle 6, elemental sword 25, ember staff 18 (fire): every weapon fills about 50-60 per second of fire. Weak point bonus x1.5.
- **Enemies:** resistances, weaknesses and thresholds per enemy are in §19 (v0.28).
- **Skill trees:** 6 nodes each, 1-2 points per rank, up to 3 ranks.
- Every spell number in `Spells.json`; the v0.28 values and their reasons are in §19.

## 19. Balance (v0.28, architect, 2026-11-12)
The owner's brief: fights a little long but not too long; a fast shooter where enemies die all the time in the waves; no bullet sponges, except an enemy that is slow and hits softly; two Valkyries and then a final boss fought with every tool; no levels; not enough skill points for the whole tree, but enough for a well-armed mage plus one gun and a half well upgraded. Approved by the owner: waves of 60-90 s, Valkyries of about 2 min, the Entity about 3 min, 36 skill points.

**Every number here is provisional and lives in `Glyphbound/Content/Data/*.json`.** Nothing needs a recompile: edit the file and run `gb.ReloadData` in the console (it also works in the packaged game). `node Tools/Balance/balance_model.js` prints every table below from the data, so after changing a number the tables can be checked again in a second. The numbers were reviewed by a second model (GPT-6-Astra, medium effort) before the first build; what it found is at the end.

### Method
- **Three anchors.** The player has 100 health and 100 mana for the whole game. The reference damage is the base pistol on the body: 80 per second. An enemy's health is "seconds it should last x the damage a player really deals".
- **Real damage is about half the paper damage:** the model takes 70% of the time attacking, 60-75% of the shots hitting and a quarter of the hits on the head. Each base gun then deals about 40 per second.
- **Builds are calculated, not guessed.** The calculator applies the perks of six reference builds (one per point of the game, mixing both trees) to the weapon and spell data: about 65 real damage per second from the gun at the start, 86 at 16 points, 99-107 at the Valkyries, 117 at the Entity. A perk's percentage stacks in a straight line with its rank (+10% per rank is +30% at rank 3).
- **Ammo runs out.** A fight uses the gun until the ammo carried and dropped is gone, then the staff or the sword. Spells run all the time, as often as the mana allows: mana only comes back after 1.5 s without spending, so the steady rhythm is one 15-mana spell every 4 seconds.
- **The QTE costs real time.** Each cast is about 2 real seconds while the world runs at 10%, so a fight with many spells lasts longer on the clock than in game time. Lengths below are real seconds.
- **The QTE maps its score in a straight line from Min (score 0) to Max (perfect).** `Base` is the value with the QTE off. For damage, Min is half of Base and Max one and a half. The model uses a score of 0.75.

### Player
| | Value | Why |
|---|---|---|
| Health | 100, +3/s after 6 s without damage | No regeneration inside a fight; a clean disengage (dash, ice wall) is rewarded |
| Mana | 100, +6/s after 1.5 s without spending | Six or seven spells in a burst, then one every 4 s |
| Largest single hit | 32 (sub-bosses) | Never more than 45% of the bar: no one-shots. 30% or more is a "strong hit" and stops the flamethrower (the old brute hit 28 and never did) |

### Weapons (no perks)
| Weapon | Hit | Sustained damage/s (with reloads) | Role |
|---|---|---|---|
| Apprentice staff | 12 every 0.35 s | 34, free | The fallback that never runs out: about half a gun |
| Ember staff | 22 every 0.6 s | 37 + Ignite, free | Slower, sets enemies on fire |
| Pistol | 20 every 0.25 s, 15 rounds, 6 spare magazines | 61 (121 on the head) | Precision: the best head bonus (x2) |
| Shotgun | 8 x 10 every 0.8 s, 6 shells, 4 spare | 71 up close, 14 far | Risk: one shell kills a soldier at arm's length |
| Rifle | 12 every 0.1 s, 30 rounds, 5 spare | 60 near, 86 far | Range and volume; less accurate, weak up close |
| Elemental sword | 50 every 0.5 s | 100, free | Late game, melee risk, hits a 90 degree arc |

The three guns come out even (about 40 real damage per second each). The rifle was the outlier before (140 burst against 82 and 94).

**Ammo.** A full load really deals 2,600 to 4,300 damage depending on the build: about one long wave shooting only. Magazine drops are set so that a base gun gets back about half of the rounds it spends in the future and a quarter in the past, and a gun with 16 points in it gets back almost all of them in the future and half in the past. Drop chance of a magazine in the future: swarm 6%, soldier 12%, elite 30%, tank 70%, sub-boss 3 magazines; in the past: 3%, 6%, 15%, 35%, sub-boss 2. The past pushes the player to the staff and the spells; nobody is ever stuck, because the staff is free.

### Spells (Base; damage Min/Max is 50% / 150%)
| Spell | Mana | Cooldown | Effect | Notes |
|---|---|---|---|---|
| Fireball | 15 | 1.5 s | 40 in a 2 m blast | 2.7 damage per mana, times the enemies caught |
| Lightning bolt | 10 | 0.8 s | 30, goes through enemies | 3.0 per mana |
| Lightning rune | 20 | 6 s | 80 in a 3 m blast | 4.0 per mana, the best: it needs setting up |
| Flamethrower | 10 + 15/s | 6 s | 50/s in a short cone | 3.0 per mana; a full bar is 300 per enemy |
| Fire wall | 25 | 10 s | 60/s for 6 s, ticks every 0.25 s | A soldier crossing it loses a quarter to a half of its health |
| Static orb | 25 | 7 s | 35/s within 2.5 m, travels at 3 m/s for 6 s, up to 27 m | About 60 to each enemy it passes |
| Ice wall | 20 | 16 s | Blocks for 8 s (5-11) | Up half of the time; 98% with every perk and a perfect QTE |
| Ice shield | 20 | 24 s | 12 s (8-16), 80 health (40-120), blocks 75% from the front | Up half of the time. Its health is the real limit; with 15 points in it (Quick Recall, Enduring Guard) it can always be up |
| Ice floor | 20 | 12 s | 50% slow for 6 s; the slow lingers 1 s after leaving it | Up half of the time |
| Weapon enchant | 20 | 18 s | +25% gun damage (15-35%) for 15 s (8-22), element build-up | About a fifth of the mana regeneration |

Rules behind them: a direct spell gives about 3 damage per mana; a spell that needs setup gives more; area spells give less per target because they hit several. **A control spell is up about half of the time; keeping it up all the time has to be bought with perks** (before, the ice wall and the ice shield had a cooldown shorter than their duration). Fixed with data: the fire wall ticked every 0.5 s and an orc crossed it in 0.4 s untouched (now 0.25 s and a little thicker); the ice floor's slow lasted 4 s after leaving the floor (now 1 s); the static orb reached the end of its range before its duration perk could matter.

### Enemy templates
| Template | Past (pixel art) | Future (3D) | Health | Speed | Hit | Lasts (base gun -> 16-point build) |
|---|---|---|---|---|---|---|
| Swarm | Castle Imp | Scrap Drone, Robot Spider | 30 | 600 | 6 | 2 pistol shots |
| Soldier | Castle Orc (melee), Orc Shaman (ranged) | Armored Orc, Robot Soldier (ranged) | 50-70 | 380-450 | 8-10 | 3-4 pistol shots, 1 shell |
| Elite | Orc Champion | Orc Enforcer, Sentinel (ranged) | 180 | 380-400 | 16-18 | 4 s -> 1.7 s |
| Slow tank (the one sponge) | Orc Juggernaut | Siege Orc | 480 | 260 | 20 every 3.1 s | 12 s -> 4.5 s |
| Sub-boss | Orc Brute | Orc Warlord | 2,700 / 3,000 | 340-360 | 32 | 40-45 s |
| Boss | Valkyrie (medieval) | Valkyrie (cyberpunk) | 7,200 / 7,600 | 480 | 18-22 | about 2 min |
| Final boss | The Entity, two phases of 7,300 (8,000 before v0.40) | | 14,600 | 420-520 | 20-26 | about 3 min |

**Bursts (v0.56):** the future's gun enemies fire bursts of `BurstCount` hitscan shots; `AttackDamage` is per shot and a burst that all lands is about one hit of the table (3 x 3 for a soldier, 4 x 4 for an enforcer). An enemy attacks once every `AttackWindup + AttackInterval`. The player (750) is faster than everything, so melee enemies only hit a player who stops; the pressure comes from numbers, ranged enemies and the swarm. The tank is the only sponge and it is the slowest and softest hitter for its size. Bosses and sub-bosses are stat blocks for the generic enemy until their own behavior exists; their heavy attacks, when written, should stay at 40 or less.

**Boss health follows the ammo.** A build of that point of the game empties its gun in about 40 seconds (about 4,000 damage, a bit more than half a Valkyrie); the spells give another quarter and the staff or the sword the rest. That is the "combine both eras" pillar in numbers, and it is why a Valkyrie has 7,200 and not 10,000: with more health the second half of the fight is a staff-only chore. By build, against the medieval Valkyrie: the reference build 120 s, a gun specialist 57 s, a pure mage 146 s. A player who lands half of the shots on the weak point is about 1.5 times faster. Boss arenas can still offer a magazine now and then.

### Resistances and weaknesses
- **Weakness:** +25% damage and +50% build-up of that element. **Resistance:** -25% damage and -50% build-up. An enemy never resists two elements; bosses change between phases.
- **Past (flesh): weak to fire.** **Future (armor, electronics): weak to lightning, resists fire.** The swarm of the past is weak to ice (a freeze stops a rush). The Orc Champion also resists ice; the Juggernaut resists lightning.
- **Orc Brute:** resists fire, weak to ice. **Valkyrie, medieval (bound by runes):** resists lightning, weak to ice. **Valkyrie, cyberpunk (cryogenic tube):** resists ice, weak to fire. **Entity:** phase 1 (the machine) weak to lightning and resists fire; phase 2 (the ghost) weak to fire and resists ice.
- **Build-up threshold by template:** swarm 60, soldier 100, elite 150, tank 200, sub-boss 250, boss 400. Tanks, sub-bosses and bosses are never stunned or frozen: Shock only interrupts their attack and Frost only slows them (40% tanks and sub-bosses, 25% bosses), with longer immunity after each status.
- **Slow resistance (new, `SlowTaken`):** tanks and sub-bosses take 60% of the ice floor's slow and bosses 40%, so a 50% slow is 30% on a tank and 20% on a boss. It is general: any enemy can have `<EffectId>Taken` for any status effect.
- **Ignite scales with the template** (`IgniteDamageMult`: tank x2, sub-boss x4, boss x8): about 17-22% of an elite or a tank and 4% of a boss per proc.

### Encounters and length
Sub-waves of 5 to 10 enemies, so something dies every 2 seconds. With every enemy of a sub-wave attacking, a player who avoids most of it (85% of melee, 60% of shots) takes 6 to 14 damage per second: 7 to 18 seconds to die without healing or defending. That is the "hard" setting; the levers are the counts in `Encounters.json`.

| Encounter | Enemies | Health | Length (real s) |
|---|---|---|---|
| Opening fights | 8 | 400-500 | about 20 |
| Waves 1 to 3 of each era | 35-40 in five sub-waves | 2,200 -> 3,900 | 60 -> 74 |
| Elemental zones | 35-45 in five sub-waves | 3,300-3,500 | about 72 |
| Sub-bosses (with a small escort) | 4-5 | 2,900-3,200 | 41-45 |
| Valkyries | 1 | 7,200 / 7,600 | about 120 |
| Entity | 2 phases | 14,600 | about 193 (it has the sword; v0.40) |

`Encounters.json` holds one ready-made encounter of each kind (placeholders for the level designers: `Opening_*`, `Wave_Past_1..3`, `Wave_Future_1..3`, `SubBoss_*`, `Zone_Fire/Lightning/Ice/Air`, `Valkyrie_*`, `Entity`), besides the two shorter test waves of the arena.

### Skill points
Both trees cost 99 points (weapons 57, spells 42; v0.40, was 78). **Budget: 36 points before the Entity (36%).** Example: Heavy Rounds, the pistol's Quick Trigger and Rapid Reload, Deadeye and Marksman at rank 3 (18 points, a finished pistol), Point Blank 3 (the shotgun half-way), Frugal Casting 3 (6 points) and three spell upgrades at rank 3 (9 points): a build, not the whole tree.

| Source | Points |
|---|---|
| Start | 2 |
| Each of the two opening fights | 1 |
| Each wave (6) | 1 |
| Each sub-boss (2) | 2 |
| Each elemental zone (4) | 3 |
| Each Valkyrie | 5 |
| **Total** | **36** |

If the number of encounters changes, the points per encounter are adjusted to keep the total (the calculator prints the sum and flags it).

### Suggestions for the owner (the tree's percentages are the owner's)
- **Marksman + Deadeye is the strongest thing in the tree:** the pistol's head bonus goes from x2 to x3.5, and with Heavy Rounds and Quick Trigger the pistol triples its damage for 18 points. It is aim-gated, so it may be exactly what is wanted; if the pistol dominates in play-tests, Deadeye at +0.15x per rank is the smallest fix.
- **Long Shot: applied by the owner (v0.29).** It was +0.15x per rank (+39% at long range against +60% for Point Blank); at +0.2x per rank the rifle gets +52%.
- **Free skill reset:** answered by the owner, it will not be in the final version, so the 36-point budget is a real build choice.
- **Chest contents** of the test arena are still written in code (`GBTestArena.cpp`); they move to data when real levels exist.

### Values moved from code to data in v0.28
Enemy AI fractions, projectile lifetime and knockback lift, grenade blast falloff and bounce, the fire wall's touch margin, the ice shield's block arc, player acceleration, friction and look speed, what a respawn and an era travel restore, and what a failed QTE costs. All keep their old value; the list is in the day entry. A spell already in the world keeps the numbers it was cast with: cast it again after `gb.ReloadData`.

### Second opinion (GPT-6-Astra, before the first build)
Taken: builds calculated from the perks instead of a flat multiplier (the old guess was half of the real damage); the ammo limit inside the boss maths (the Valkyries went from 9,000-10,000 to 7,200-7,600); the real time of the QTE; mana that only regenerates between casts; enemy attack cycles with their windup; lower magazine drops (a mid-game pistol was getting back more rounds than it spent); longer cooldowns for the ice wall, shield and floor; the lingering ice floor slow; the orb's range; a fifth sub-wave; and two code notes (the staff ignored the new projectile lifetime key). Not taken: an ice shield cooldown of 34 s (too harsh for a player without perks; its health already limits it) and a magazine drop of 8% for the armored orc (a base gun would starve; 12% keeps a specialized pistol just under break-even).
