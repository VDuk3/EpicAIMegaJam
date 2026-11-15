# Skill point inventory (2026-11-15)

Counted from `Content/Data/Encounters.json`, `Player.json`, `Skills.json`, the code that grants points
(`AGBEncounter::Clear` is the only gameplay call to `UGBSkillTreeComponent::AddPoints`) and the encounter
ids found in the saved map files (`PastCastle.umap`, `FutureBase.umap`, `EntityArena.umap`; read only).

## Earnable in one full playthrough: 43 points

Update (owner, same day): every real source now pays 1 point more (starting points 2 -> 3 and each of the
13 placed encounters +1; Entity stays at 0, unplaced encounters unchanged). The tables below show the new values.

There is no XP, no levels and no skill-point pickup. Points come from the starting value and from clearing
encounters. Each encounter pays once: clearing sets its cleared flag, and a cleared encounter never
re-arms (not after death, respawn or era travel), so points cannot be farmed.

| Source | Map | Points |
|---|---|---|
| Starting points (`Player.json StartingSkillPoints`) | - | 3 |
| Opening_Past | PastCastle | 2 |
| Castle_Yard | PastCastle | 2 |
| Castle_FrontHall | PastCastle | 2 |
| Castle_MainHall | PastCastle | 3 |
| Zone_Fire | PastCastle | 4 |
| Zone_Ice | PastCastle | 4 |
| Zone_Air | PastCastle | 4 |
| Valkyrie_Past (boss) | PastCastle | 6 |
| **PastCastle total** | | **27** |
| Future_Bay | FutureBase | 2 |
| Future_Spiders | FutureBase | 2 |
| Future_DeepWing | FutureBase | 2 |
| SubBoss_Future | FutureBase | 3 |
| Boss_Valkyrie_Future (final boss) | FutureBase | 4 |
| **FutureBase total** | | **13** |
| Entity | EntityArena | 0 |
| **Total** | | **43** |

By kind: start 3, regular encounters 15, zones 12, sub-boss 3, bosses 10.

Encounters defined in data but not placed in any shipped map (they pay nothing in a real run):
Test_PastWave, Test_FutureWave, Opening_Future, Wave_Past_1-3, Wave_Future_1-3, SubBoss_Past,
Zone_Lightning, Valkyrie_Future, Skirmish_Breach, Skirmish_Signal, MiniBoss_Herald (they add up to 24 more).

Limits of the count:
- Placement was read from the encounter ids stored in each `.umap`. A map's name table holds each id once,
  so two actors with the same id would show as one; that does not change the total, because both share
  one cleared flag and only the first clear pays.
- Not every encounter may be mandatory. If a zone (Fire / Ice / Air, 4 points each) can be skipped, a
  minimal run earns less than 43.
- The skill menu shows a "+1 skill point" test button with no build check, so in the current build the
  player can add unlimited points. It should be hidden in the shipped build.

## Spendable: 124 points (every skill at max rank)

All 36 skills have 3 ranks, except the four potion perks (1 rank each).

| Tree / branch | Skills | Points to max |
|---|---|---|
| Spell (incl. Vitality 6, Deep Reserves 6, Potent Remedy 2, Potent Elixir 2, Emergency Draught 3, Emergency Focus 3) | 18 | 64 |
| Weapon core | 4 | 18 |
| Weapon / Pistol | 4 | 12 |
| Weapon / Rifle | 5 | 15 |
| Weapon / Shotgun | 5 | 15 |
| **Total** | **36** | **124** |

## Verdict

Earnable 43 vs spendable 124: the player can buy about a third of the tree (81 points short) and
must choose. The six health / mana perks together cost 22 of the 43 points. The free reset button lets the player
re-spend everything at any time.
