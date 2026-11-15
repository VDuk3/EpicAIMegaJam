# 2026-11-15 — Pixel-art melee AI tuning (side session)

**Request (owner):** "adjust the melee enemies' AI of the pixel-art world a bit more" (translated from Spanish).

**Who did it:** the architect implemented directly (Claude Opus 5.5 side session). This is an exception to the Muse rule because of the deadline: the changes were a few lines of `.cpp` plus data, and Live Coding was faster than a brief.

**Reading the code first:** the architect listed four likely weaknesses and asked the owner which ones bother him:
1. After a swing, an enemy stood still for its whole attack interval (about 1 s), even while the player backed away, so it was easy to kite.
2. No reaction to hits beyond a flinch frame: no stagger, and the windup was never interrupted.
3. With three or more melee enemies, only two attack and the rest stand still in the wait ring.
4. The windup telegraph is barely visible.

Circling and flanking near the player were left out on purpose: the owner rejected them earlier today.

**Done while waiting for the answer (points 1 and 2, the architect's own reading of the code):**
- **Recovery chase.** This is opt-in through the `RecoverPause` stat, set for the barghest (0.25 s), the castle orc (0.3 s) and the champion (0.4 s). After the pause, an enemy beyond 0.8 of its reach chases again. It only does this on the ground, because a barghest lunge flies during Recover. The juggernaut, the brute and the boss's second phase keep their old behaviour.
- **Stagger.** A hit of at least `StaggerFraction` of max health locks the enemy for `StaggerTime` and breaks its windup, at most once per `StaggerCooldown`. Values: barghest 0.3 / 0.25 s / 1.2 s, orc 0.15 / 0.3 s / 1.5 s, champion 0.1 / 0.3 s / 2 s. Every weapon deals about 10 or more per hit, so in practice this is "flinch on almost any hit, limited by the cooldown". The stagger is skipped mid-air, so it never drops a lunge out of the air.
- **Advisor review** (Fable 5.2, before the PIE request) caught three problems in the first version, all fixed:
  - the recovery chase cut the barghest's lunge;
  - the chase also applied to the boss's pixel-art second phase;
  - a code comment credited the "easy to kite" finding to the owner instead of the architect.

**Checked:** Live Coding compile clean; human PIE test pending.
