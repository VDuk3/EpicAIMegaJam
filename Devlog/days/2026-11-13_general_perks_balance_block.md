# Block: the three general weapon perks come back; every weapon perk measured and the new ones tuned (Nov 13, CENTRAL)

> Written by the architect chat of this block (Claude Code, Claude Opus 5.5). To be merged into the day entry by whoever closes the day. Times in UTC.

## The owner's message
> *"Todas las armas deberian tener Marksman, Precision Recovery y Arcane Conduit, como estaba originalmente. Si crees conveniente agregar algun perk nuevo para alguna arma en especifico me puedes preguntar por mi opinion para ver si se implementa. Tambien quisiera que veas como estan balanceados todos, y balancees los nuevo acorde a eso."*

Translation: "All weapons should have Marksman, Precision Recovery and Arcane Conduit, as it was originally. If you think a new perk for a specific weapon is worth adding, you can ask for my opinion before it is implemented. I would also like you to look at how they are all balanced, and balance the new ones accordingly."

The first sentence can be read two ways, so the architect asked four questions in one multiple-choice window. The owner answered:
- **The three perks:** "Generales, como al principio" ("General, as at first"). They work on every gun, Marksman also on the staff and the sword, and they sit on the weapon hub.
- **Deadeye**, which would otherwise duplicate the pistol's weak-point bonus: "Dejar los dos" ("Keep both"). They stack, as they always did.
- **Tight Choke**, added by the architect without asking in the previous block: "Dejarlo" ("Keep it").
- **New perks:** "Quiero que analices si vale la pena incorporar alguno nuevo tomando en cuenta las decisiones anteriores. Tambien puedes dar nuevas opciones o cambios." ("Analyse whether any new one is worth adding given the decisions above. You may also give new options or changes.")

## What changed
- **`Skills.json`.**
  - **The hub:** Heavy Rounds sits in the middle. Marksman -> Precision Recovery are on the left, and Arcane Conduit is on the right, back to its all-guns key. Arcane Conduit now needs only Heavy Rounds.
  - **Each gun page:** its signature perk (Deadeye, Long Shot, or Point Blank -> Tight Choke) and its own Quick Trigger -> Rapid Reload and Steady Grip.
- **`UI/GBHUD_Menus.cpp`.**
  - The gun emblems hang right under Heavy Rounds, in the free columns nearest to it. After Sol's note, a fourth gun can no longer land on a general perk.
  - The grid's height counts the tall emblems.
  - The emblems are numbered by their order (01-03), not by their column.
- **Smoke.** The Arcane Conduit check runs with the pistol again (15 -> 19.5, x1.3). Rapid Reload expects 0.55 x base at rank 3.

## How every weapon perk is balanced (the owner's second request)
`Tools/Balance/balance_model.js` gained a table, "what one point buys". It takes each perk alone at full rank, on top of Heavy Rounds 3, and gives the gain in its gun's expected damage per second (accuracy, uptime, head share, best range).

The tool could not see two kinds of perk before, so two things were added:
- **A provisional accuracy model.** Part of each gun's misses comes from recoil (pistol 30%, rifle 50%, shotgun 15%) and part from spread (15%, 20%, 50%). Recoil and spread perks shrink those parts.
- **Pellets and magazine size** now go through the perk modifiers, as the game's `GetStat` does.

Final table:

```
Perk                  Page     Points  Pistol   Rifle    Shotgun  Best / point  Other effect
Headshot (Marksman)   hub      6       +15%     +16.7%   +16.7%   +2.8%         also staff and sword
PrecisionRecovery     hub      6       -        -        -        -             heal 3% on weak-point kills
EnchantBuildUp        hub      3       -        -        -        -             build-up +30%
PistolHeadshot        Pistol   3       +15%     -        -        +5%
Pistol_FireRate       Pistol   3       +22.2%   -        -        +7.4%
Pistol_Reload         Pistol   3       +12.2%   -        -        +4.1%
Pistol_Recoil         Pistol   3       +6%      -        -        +2%
RifleFar              Rifle    3       -        +52.2%   -        +17.4%
Rifle_FireRate        Rifle    3       -        +17.6%   -        +5.9%
Rifle_Reload          Rifle    3       -        +20.3%   -        +6.8%
Rifle_Recoil          Rifle    3       -        +20%     -        +6.7%
ShotgunClose          Shotgun  3       -        -        +60%     +20%
Shotgun_Spread        Shotgun  3       -        -        +7.5%    +2.5%
Shotgun_FireRate      Shotgun  3       -        -        +20.4%   +6.8%
Shotgun_Reload        Shotgun  3       -        -        +15.3%   +5.1%
Shotgun_Recoil        Shotgun  3       -        -        +3%      +1%
Heavy Rounds itself: +30% on every gun for 3 points = +10% per point (on all three guns at once).
```

**Reading it:**
- The unconditional one-gun perks the owner already had buy +5% (Deadeye) to +7% (Quick Trigger) per point. That is the band the new ones were tuned to.
- Heavy Rounds buys +10% per point, on three guns at once.
- Long Shot and Point Blank buy far more (+17% and +20%), but only at their range.
- Marksman's +2.8% per gun is spread over five weapons.

**The tuning:**
- **Rapid Reload:** each gun's version bought only +2.6% to +4.2% at -10%, because it now helps one gun instead of three. It goes to **-15% per rank**: +4.1% pistol, +6.8% rifle, +5.1% shotgun.
- **Tight Choke:** goes to **-15% per rank** (+2.5%).
- **Weak spots left for the owner to decide:**
  - Steady Grip is right for the rifle (+6.7%) but weak on the pistol (+2%) and very weak on the shotgun (+1%), whose recoil barely matters at one shot every 0.8 s.
  - Tight Choke is still low under this model.

**Balance after the change:**
- The Entity fight is 190 s (target 160-200). The 7,300 health per phase from the previous block stays.
- The Valkyries take 128 and 129 s.

**Candidate new perks, measured with the same table** (temporary rows, removed after measuring):

| Candidate | Value per point |
|---|---|
| Rifle Extended Mag (+5 rounds per rank) | +4.8% |
| Shotgun +2 shells per rank | +5.7% |
| Pistol +3 rounds per rank | +3.3% |
| Shotgun Buckshot (+1 pellet per rank), 1 point per rank | +12.5% (too strong) |
| Shotgun Buckshot, 2 points per rank | +6.2% |

These go to the owner as proposals, as the owner asked.

## Review: GPT Sol 6.1 (Codex CLI, medium, read-only, logged through `Invoke-LoggedAgent.ps1`)
Prompt: `Prompts/general_perks_balance_review_sol.md`. **Two minor findings, both fixed.**
1. The tool's summary line said Heavy Rounds gives +33.1% (+11% per point). Ranks stack in a straight line, so it is +30% (+10% per point). The line is now computed from the data, and the GDD figure was corrected.
2. With four or more gun branches, an emblem would have landed on Arcane Conduit's cell. The emblems now skip occupied columns.

Sol also confirmed:
- Every node is reachable, the hub cells and links are clear, and §18 matches the data.
- The smoke's arithmetic is right.
- Accuracy stays between its base and 1.
- The pellets and magazine change leaves the earlier tables identical.

## Checks
- **Full smoke:** `done=True crashed=False LogGB errors=0`, 60 screenshots. The pistol reloads in 0.66 s (0.55 x 1.2), and the Arcane Conduit check gives x1.3 on the pistol.
- **Tests:** 44 run, 0 failed.
- **Builds:** Editor and game. After Sol's two fixes, the Editor build, the UI smoke and the game build ran again.
- **Not packaged in this block.**
- **Implementer:** the architect (data, tool and a small layout change, judged against the tool's numbers and screenshots).

## Screenshot
- `media/2026-11-13/generalperks_hub_1080.png`: the weapon hub. Marksman -> Precision Recovery on the left, Heavy Rounds and the three guns in the middle, Arcane Conduit on the right.
