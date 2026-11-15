# Skill point inventory (2026-11-15)

Counted from `Content/Data/Encounters.json`, `Player.json`, `Skills.json`, the code that grants points
(`AGBEncounter::Clear` is the only gameplay call to `UGBSkillTreeComponent::AddPoints`) and the encounter
ids found in the saved map files (`PastCastle.umap`, `FutureBase.umap`, `EntityArena.umap`; read only).

## Earnable in one full playthrough: 29 points

There is no XP, no levels and no skill-point pickup. Points come from the starting value and from clearing
encounters. Each encounter pays once: clearing sets its cleared flag, and a cleared encounter never
re-arms (not after death, respawn or era travel), so points cannot be farmed.

| Source | Map | Points |
|---|---|---|
| Starting points (`Player.json StartingSkillPoints`) | - | 2 |
| Opening_Past | PastCastle | 1 |
| Castle_Yard | PastCastle | 1 |
| Castle_FrontHall | PastCastle | 1 |
| Castle_MainHall | PastCastle | 2 |
| Zone_Fire | PastCastle | 3 |
| Zone_Ice | PastCastle | 3 |
| Zone_Air | PastCastle | 3 |
| Valkyrie_Past (boss) | PastCastle | 5 |
| **PastCastle total** | | **19** |
| Future_Bay | FutureBase | 1 |
| Future_Spiders | FutureBase | 1 |
| Future_DeepWing | FutureBase | 1 |
| SubBoss_Future | FutureBase | 2 |
| Boss_Valkyrie_Future (final boss) | FutureBase | 3 |
| **FutureBase total** | | **8** |
| Entity | EntityArena | 0 |
| **Total** | | **29** |

By kind: start 2, regular encounters 8, zones 9, sub-boss 2, bosses 8.

Encounters defined in data but not placed in any shipped map (they pay nothing in a real run):
Test_PastWave, Test_FutureWave, Opening_Future, Wave_Past_1-3, Wave_Future_1-3, SubBoss_Past,
Zone_Lightning, Valkyrie_Future, Skirmish_Breach, Skirmish_Signal, MiniBoss_Herald (they add up to 24 more).

Limits of the count:
- Placement was read from the encounter ids stored in each `.umap`. A map's name table holds each id once,
  so two actors with the same id would show as one; that does not change the total, because both share
  one cleared flag and only the first clear pays.
- Not every encounter may be mandatory. If a zone (Fire / Ice / Air, 3 points each) can be skipped, a
  minimal run earns less than 29.
- The skill menu shows a "+1 skill point" test button with no build check, so in the current build the
  player can add unlimited points. It should be hidden in the shipped build.

## Spendable: 114 points (every skill at max rank)

All 32 skills have 3 ranks.

| Tree / branch | Skills | Points to max |
|---|---|---|
| Spell (incl. the new Vitality 6 and Deep Reserves 6) | 14 | 54 |
| Weapon core | 4 | 18 |
| Weapon / Pistol | 4 | 12 |
| Weapon / Rifle | 5 | 15 |
| Weapon / Shotgun | 5 | 15 |
| **Total** | **32** | **114** |

## Verdict

Earnable 29 vs spendable 114: the player can buy about a quarter of the tree (85 points short) and must
choose. Maxing both new perks alone costs 12 of the 29 points. The free reset button lets the player
re-spend everything at any time.
