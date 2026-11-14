# Code review, second pass: the fixes to your nine findings, the vault, the sealed Valkyrie, the shattering glass (Glyphbound, UE 5.9 C++)

You are reviewing uncommitted work in `C:\GameJam` (read-only). You reviewed the first version of this block earlier
today (`Prompts/enemy_tactics_review_sol.md`; your nine findings are in
`Devlog/raw/CENTRAL/2026-11-14/agents/141613-codex-ec6dc5/output.txt`, at the end). Context: `CLAUDE.md`,
`Design/GDD.md` (v0.58: section 10 "How the future's enemies fight", the Vault paragraph before it), the briefs
`Prompts/enemy_tactics_ai.md` and `Prompts/vault_over_railings.md`. Run `git status` and `git diff` to see the change.

What is new since your first pass:
1. The fixes to your nine findings: zone enforced on the movement itself (`MoveToZonePoint`, `IsDirectionBlocked`,
   `TickLunge` / `StartLunge`, the put-back in `TickAwareness`), `MayStartMelee`, reinforcements at hidden points with
   a held wave (`AGBEncounter`, `EncounterReinforceMaxWait`), the stimulus position in `AlertSquad` and
   `LastKnown` apart from `LastSeenTime`, `GetEyeHeightAboveFoot`, the return to the post (`bReturningToPost`),
   `GBTactics::AddBlockedHit`, the throttled flyer separation and orbit slots, real traces in the shield's smoke check.
   Check each fix does what it says and did not open a new hole.
2. The Valkyrie sealed in her tube: `FGBEncounterDefinition::WakeFlag`, `AGBEnemyCharacter::SetSealed`, the
   encounter's poll and `BeginWake` with a leap (`AI/GBEncounter.*`, `AI/GBEnemyCharacter.*`).
3. The chief's shield carried on the capsule between two places, the bash knockback (`AI/GBShieldEnemy.*`).
4. The player's vault over thin obstacles: pure rules in `Player/GBMantle.*` (`CanVault`, `VaultEndFeetZ`,
   `EvaluateVaultPath`, `EvaluateVaultPose`), `FindVault` / `TryStartVault` / `StartVault` / `TickVaultPush` in
   `Player/GBPlayerCharacter.*` (it runs on the mantle's state), tests in `Tests/GBPlayerTests.cpp`.
5. `World/GBBreakable.*`: pieces thrown and moved by hand when it breaks (`Shatter`, `Tick`), `bMeshCollision`.
6. The map tour removes prespawned enemies; a `vaulttest` tour spot; `-AllWeapons` in `Player/GBPlayerCharacter.cpp`.

Look for real defects, most severe first:
- Crashes and lifetime: a sealed enemy destroyed or reset while sealed (is invulnerability always restored, can the
  encounter be left Waiting for ever, what if the flag is set while the player is dead or in the other era); the leap
  landing outside the navmesh or inside geometry; shard components after the actor's lifespan; the vault's state shared
  with the mantle (can one corrupt the other, can a vault start during a ladder, a dash, a QTE, death, or end with the
  camera or the held item left posed); the actor moved without sweeping during the vault (can he end inside geometry
  or outside the world when something moves in the way, when the frame rate is very low, or when the landing floor
  is a moving or breakable thing).
- Behaviour the owner would see: a vault into a place he cannot leave or through a wall thinner than 60 cm that should
  stop him (doors, the energy gate, breakable glass panes, the Valkyrie's tube: those must NOT be vaulted through or
  over when they reach the ceiling; what stops it?); the push trigger firing while he simply leans on cover in a
  fight; enemies clamped back into the zone visibly teleporting; reinforcements never arriving; the Valkyrie waking
  for a reason other than the flag, or never; the chief's shield mesh and hit box disagreeing after scale; knockback
  throwing the player through geometry or out of the map.
- Cost per tick; data keys read but not set and the other way round.
- Tests and smoke checks that wrong code would still pass.
Ignore style. For each finding: file:line, what goes wrong, a concrete scenario, the smallest fix. End with a one-line
verdict.
