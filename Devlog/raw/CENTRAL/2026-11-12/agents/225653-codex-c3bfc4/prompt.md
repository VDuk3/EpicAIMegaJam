# Review: balance pass v0.26 and "no hardcoded values" (read-only)

You are a second opinion for the architect of **Glyphbound**, a first-person shooter / RPG hybrid made in a 4-day game jam (Unreal Engine 5.9.0, C++, in `Glyphbound/`). **Do not edit any file, do not build, do not run Unreal.** You may read files and run `git diff` and `node Tools/Balance/balance_model.js`. Answer in English, short and concrete.

Nothing of this is compiled or play-tested yet: the architect wants your notes **before** the single build, so that everything can be changed at once.

## The owner's brief (translated from Spanish)
"Balance, with maths, the numbers of the whole game, except the exact percentages of the skill-tree upgrades (leave those, suggestions are welcome): the player's attributes (health, mana), spell costs, spell damage, cooldowns, weapon and bullet damage, resistances and weaknesses. Not everything is implemented, so use placeholders; no enemy is final yet. We want fights a little long but not too long. The difficulty of a fast-paced shooter: in the waves enemies are always dying. No bullet-sponge enemies, unless it is an enemy with low damage that is slow. There are two bosses (the medieval Valkyrie and the cyberpunk Valkyrie) and then a final boss, where the player has every tool. There are no levels. The player must NOT get enough skill points for the whole tree: they pick branches for a build, but enough to be well armed as a mage with the equipped spells, and to have at least one gun and a half well upgraded."
Later the owner approved: waves of 60-90 s, Valkyries of about 2 min, final boss about 3 min, 36 skill points in total, and asked: "not a single hardcoded value; everything must be easy to change while testing".

## What to read
1. `Design/GDD.md`, section 19 "Balance" (the method and every table) and section 6 "Elemental build-up".
2. The data: `Glyphbound/Content/Data/Player.json`, `Weapons.json`, `Spells.json`, `Enemies.json`, `Encounters.json`, `StatusEffects.json`, `Items.json`, `Game.json`, `Skills.json`, `QTE.json`. `git diff HEAD -- Glyphbound/Content/Data` shows what changed.
3. `node Tools/Balance/balance_model.js` prints the tables (its assumptions are in the `ASSUME` block at the top of the script).
4. `git diff HEAD -- Glyphbound/Source` is the uncommitted C++ change (an implementer moved the last hardcoded gameplay numbers into the data files; its brief is `Prompts/balance1_no_hardcoded.md`). It must keep today's behavior exactly.

Facts about the game code that matter for the maths: the QTE maps its score linearly from a param's `Min` (score 0) to `Max` (score 1), `Base` is used only with the QTE off; a skill perk with `Multiply` stacks linearly with its rank (1.1 at rank 3 = x1.30); area spells tick with one timer per spell actor (an enemy must be inside at the tick); modifiers combine as (base + adds) x product of multipliers; a gun's enchant multiplies its damage by 1 + 0.25 x strength (strength 0.6-1.4); enemy stat blocks marked sub-boss / Valkyrie / Entity run on the generic melee or ranged enemy AI for now.

## What I want from you
**Part A, balance (most important).** Check the numbers against the brief and against each other. Look for:
- arithmetic or modelling mistakes in the method (GDD 19 "Method") or in `balance_model.js`, and assumptions that are clearly off for this kind of game;
- outliers: a weapon, spell, enemy or perk combination that dominates or is useless (work out the strongest builds reachable with 36 points and say what they do to the fight lengths; check the multiplicative stacking of Heavy Rounds, Quick Trigger, Marksman, Deadeye, the enchant and weak points);
- degenerate play: infinite loops of control (stun / freeze / slow / ice wall / ice shield uptime with the cooldown and cost perks), unlimited sustain (regeneration, Precision Recovery, potions), ammo or mana that never runs out or always runs out, a boss that cannot be finished with the resources a player carries;
- whether fight lengths, time-to-kill per enemy template and the time the player survives fit "fast shooter, hard, enemies always dying, no sponges";
- whether resistances and weaknesses make any enemy unfair or trivial;
- whether the 36-point budget really buys "a well-armed mage plus one gun and a half".
For every problem give the concrete number you would change, from what to what, and one line of reason. Do not rewrite the design.

**Part B, code diff.** Only real problems: anything that will not compile on UE 5.9, any change of behavior with the default values, a key read under one name in C++ and written under another in JSON, a value read once that `gb.ReloadData` will not refresh, a literal from the brief that was not moved.

## Format
A prioritized list, at most 15 items in total, each: **[A or B] [high / medium / low]** what, where (file and key or line), the change you propose, why. Then one line: what you checked and found fine. No preamble.
