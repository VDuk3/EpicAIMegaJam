# Task: elemental build-up (Ignite, Shock, Frost) on enemies

Project: **Glyphbound** (Unreal Engine 5.9.0, C++), in `Glyphbound/`. Read `Design/ARCHITECTURE.md` and `Prompts/implementer_rules.md` first (you DO build and run things in this task; the rest of "Hard rules" applies). One build or run at a time. Do not open the Unreal Editor. Do not commit.

Owner's spec, GDD v0.22: read `Design/GDD.md` section 6 "Elemental build-up" and the v0.22 lines of section 18 "Numbers". The owner's original text (Spanish) is `Prompts/feedback8_elemental_spec_owner.md`; this brief is the architect's design for it. **Implement exactly this.** If something here contradicts the code you find, say so in the report and pick the option that keeps existing behavior.

Summary: every enemy gets three independent bars (fire, lightning, ice). Hits with a build-up value fill the bar of their element; a full bar starts a status (Ignite = damage over time, Shock = discharge + stun or interrupt, Frost = freeze or strong slow), then an immunity time. Everything is data. The old small per-hit effects (Burn, the old Shock slow, Chill) are removed.

---

## 1. Shared types and names

### `Core/GBTypes.h`
- `FGBDamageInfo`: add three fields (UPROPERTY like the others, with a one-line comment each):
  - `float BuildUp = 0.f;` raw elemental build-up this hit adds to the target's bar of `Element` (before the target's resistance and the weak point bonus).
  - `float WeakPointBuildUpMult = 1.f;` multiplies `BuildUp` once when `bHeadshot` is true (weapon hits only).
  - `int32 BuildUpEventId = 0;` hits that share an id add build-up to one target only once (shotgun pellets of one shot); 0 = no dedupe.
- `EGBModOp`: add `MultiplyMin` at the end. Meaning: like `Multiply`, but among all `MultiplyMin` modifiers on one stat **only the lowest counts** (the strongest slow wins); that one multiplies with the plain `Multiply` ones. Update the comment at the top of `Stats/GBStatsComponent.h`.

### `Stats/GBStatsComponent.cpp`
- `AddModifiers`: `MultiplyMin` is scaled around 1 exactly like `Multiply`.
- `ApplyModifiers`: keep the lowest `MultiplyMin` value (none if there is no such modifier) and multiply it into the product. Order and Override behavior unchanged.

### `Core/GBStatNames.h`
- `GBStat::BuildUp` ("BuildUp"): spell param and item stat, raw build-up per hit or per tick.
- `GBStat::WeakPointBuildUpMult` ("WeakPointBuildUpMult"): weapon stat; when a weapon does not have it, `Game.json` `WeakPointBuildUpMult` is the default.
- A small namespace `GBElemental` with:
  - `int32 ElementIndex(EGBElement Element)`: Fire 0, Lightning 1, Ice 2, anything else -1. `constexpr int32 NumElements = 3;`.
  - `EGBElement IndexToElement(int32 Index)`.
  - Per-element key tables (built once, `static const FName[3]` or a function returning a const FName from a static array; never `FName(*FString::Printf(...))` per call):
    `DamageTaken(i)` -> `FireDamageTaken`, `LightningDamageTaken`, `IceDamageTaken`;
    `BuildUpTaken(i)` -> `FireBuildUpTaken`...; `Threshold(i)` -> `FireBuildUpThreshold`...; `DecayDelay(i)` -> `FireBuildUpDecayDelay`...; `DecayRate(i)` -> `FireBuildUpDecayRate`...; `StatusDuration(i)` -> `FireStatusDuration`...; `StatusImmunity(i)` -> `FireStatusImmunity`...; `Reaction(i)` -> `FireStatusReaction`, `LightningStatusReaction`, `IceStatusReaction` (enemy `Strings`); `WeaponBuildUp(i)` -> `FireBuildUp`, `LightningBuildUp`, `IceBuildUp` (weapon stats).
  - Single keys: `IgniteDamageMult`, `ShockDischargeDamage`, `FrostSlow`.
  - Default prefix: the default of enemy key `K` is `Game.json` `"ElementalDefault.K"` (Values for numbers, Strings for reactions). Provide `FName DefaultKey(FName Key)` built once per key (cache in a small static TMap or per-table static arrays).
  - Reaction names: `Ignite`, `Stun`, `Interrupt`, `Freeze`, `Slow`, `None`; status effect id `Ignite`.

---

## 2. Pure logic: `Combat/GBBuildUpMeter.h` / `.cpp` (new files, no UObjects)

```cpp
enum class EGBBuildUpPhase : uint8 { Building, Active, Immune };

struct FGBBuildUpConfig
{
	float Threshold = 0.f;        // <= 0: this bar never fills (element disabled for this enemy)
	float BuildUpTaken = 1.f;     // multiplier on incoming build-up (resistance < 1, vulnerability > 1, 0 = immune)
	float DecayDelay = 0.f;       // seconds without build-up before the bar drains
	float DecayRate = 0.f;        // points per second
	float StatusDuration = 0.f;
	float ImmunityDuration = 0.f;
};

struct FGBBuildUpMeter
{
	float Value = 0.f;
	EGBBuildUpPhase Phase = EGBBuildUpPhase::Building;
	float PhaseTimeRemaining = 0.f;
	float TimeSinceBuildUp = 0.f;

	/** Adds Amount * BuildUpTaken while Building. Returns true when this add filled the bar: Value goes to 0 and the phase to Active for StatusDuration. Ignored (false) when not Building, Amount <= 0, BuildUpTaken <= 0 or Threshold <= 0. Resets the decay delay. Full means Value >= Threshold - 1e-4 (float tolerance, so exactly the threshold counts). */
	bool Add(float Amount, const FGBBuildUpConfig& Config);

	/** Advances time; returns a bit mask of EGBBuildUpEvent (ValueChanged, StatusEnded, ImmunityEnded). */
	uint8 Advance(float DeltaTime, const FGBBuildUpConfig& Config);

	/** Building with an empty bar: nothing to tick. */
	bool IsIdle() const;
};
```
`Advance` rules (frame-rate independent: one step of 1.0 s must give the same result as ten steps of 0.1 s):
- **Building:** `Before = TimeSinceBuildUp; TimeSinceBuildUp += Dt;` if `Value > 0`: decay time this step = `max(0, TimeSinceBuildUp - max(Before, DecayDelay))`; `Value = max(0, Value - DecayRate * DecayTime)`; ValueChanged if it changed.
- **Active:** `PhaseTimeRemaining -= Dt`; when `<= 1e-4`: StatusEnded; if `ImmunityDuration > 0` -> Immune with `PhaseTimeRemaining = ImmunityDuration + PhaseTimeRemaining` (carry the overshoot) else Building. A status with `StatusDuration <= 0` ends on the next Advance.
- **Immune:** `PhaseTimeRemaining -= Dt`; when `<= 1e-4` -> Building, ImmunityEnded.

```cpp
/** Remembers the last 8 build-up events of one target, so one event (a shotgun shot) counts once: the biggest amount of that event wins. */
struct FGBBuildUpEventLog
{
	/** Returns how much of Amount to really add: all of it for a new id (or id 0); for a known id only the part above what that id already added (0 if smaller). */
	float Accept(int32 EventId, float Amount);
	// ring of 8 {Id, Amount}; a new id overwrites the oldest entry
};
```

## 3. Component: `Combat/GBElementalStatusComponent.h` / `.cpp` (new)

`UGBElementalStatusComponent : UActorComponent` (ClassGroup Glyphbound, BlueprintSpawnableComponent). Combat logic only, no visuals. Public API:
```cpp
static UGBElementalStatusComponent* Find(const AActor* Actor);
/** Raw build-up of Element (Fire / Lightning / Ice; others ignored), before the owner's BuildUpTaken. EventId != 0 is deduped through FGBBuildUpEventLog. Returns true if it started the status. */
bool AddBuildUp(EGBElement Element, float Amount, int32 EventId, AActor* Instigator);
float GetBuildUp(EGBElement Element) const;
float GetThreshold(EGBElement Element) const;
EGBBuildUpPhase GetPhase(EGBElement Element) const;
float GetPhaseTimeRemaining(EGBElement Element) const;
bool IsStatusActive(EGBElement Element) const;
FName GetReaction(EGBElement Element) const;   // see below
void ClearAll();

// Events for the look of each enemy (Niagara, shaders... later). No visuals here.
FGBOnBuildUpChanged OnBuildUpChanged;          // (EGBElement, float Value, float Threshold)
FGBOnElementalStatusStarted OnStatusStarted;   // (EGBElement, FName Reaction, float Duration)
FGBOnElementalStatusEnded OnStatusEnded;       // (EGBElement)
FGBOnElementalStatusEnded OnImmunityEnded;     // (EGBElement)
```
Declare the delegates (native multicast) in the header.

**Config per element** (read when needed, never cached across a data reload): numbers from the owner's `UGBStatsComponent::GetStat(Key, Default)` where `Default = Data->GetGameValue(GBElemental::DefaultKey(Key), 0)` (so an enemy's `Stats` value wins, and modifiers can change it later); the reaction from the owner's enemy definition `Strings` (`AGBEnemyCharacter::GetDefinition()`), else `Data->GetGameString(DefaultKey, "None")`. Valid reactions: Fire `Ignite` / `None`; Lightning `Stun` / `Interrupt` / `None`; Ice `Freeze` / `Slow` / `None`. An unknown text logs one warning (once per enemy id and element) and counts as `None`. Reaction `None` = immune to that status: no build-up is added (the damage itself is untouched). A missing default (threshold 0) disables that bar; log one warning per key, once.

**Tick:** `PrimaryComponentTick.TickInterval = Game.json ElementalTickInterval` (set in BeginPlay; default 0.1). Tick is enabled only while some meter is not idle (enable in AddBuildUp / StartStatus, disable when all three are idle). Each tick: `Advance` each meter with DeltaTime (the engine passes the real elapsed time), broadcast `OnBuildUpChanged` on ValueChanged, and on StatusEnded / ImmunityEnded run the end logic and broadcast.

**Status start** (when `Add` returns true), with `Duration = <E>StatusDuration`, `Instigator` = the one whose hit filled the bar:
- **Fire, `Ignite`:** `UGBStatusEffectComponent::ApplyEffect("Ignite", Instigator, DurationScale = Duration / IgniteDefinition.Duration (guard against 0), StrengthScale = IgniteDamageMult)`. The Ignite effect is data in `StatusEffects.json` (ticks of fire damage through `DealDamage`, so FireDamageTaken applies and it never adds build-up).
- **Lightning:** first the discharge: if `ShockDischargeDamage > 0`, `UGBCombatLibrary::DealDamage(Owner, Info)` with that amount, Element Lightning, the instigator, `SourceId "Shock"`, `BuildUp 0`. If that killed the owner, stop here. The reaction (`Stun` / `Interrupt`) is done by the enemy through `OnStatusStarted` (below).
- **Ice, `Slow`:** `Stats->AddModifiers("Elemental.Frost", { MoveSpeed, MultiplyMin, 1 - clamp(FrostSlow, 0, 0.95) })`. `Freeze` is done by the enemy through `OnStatusStarted`.
- Then broadcast `OnStatusStarted(Element, Reaction, Duration)` (also for Ignite and Slow) and `OnBuildUpChanged` (value 0).

**Status end:** Fire: `RemoveEffect("Ignite")` (in case a data reload changed durations); Ice: `RemoveModifiers("Elemental.Frost")`; broadcast `OnStatusEnded`.

**Death and cleanup:** bind the owner's attribute `OnDeath` in BeginPlay -> `ClearAll()` (meters reset, Frost modifier removed, tick off). EndPlay: unbind and remove the Frost modifier. No timers survive the owner.

## 4. Damage path: `Combat/GBCombatLibrary.h/.cpp`
- `static int32 NewBuildUpEventId();` global counter, never returns 0.
- `static bool IsWeakPointHit(const AActor* Target, const FVector& HitLocation);` today: `AGBEnemyCharacter::IsHeadshotLocation`. The one place to add other weak points later; a head that is also a weak point is still one hit, so the bonus can never apply twice. Update the header comment.
- `ApplyWeakPointBonus`: `Info.bHeadshot = IsWeakPointHit(...)`; only if `bHeadshot && WeakPointMult > 1` multiply `Amount`. Returns `bHeadshot`. (Today all weapons have mult >= 1.5, so nothing visible changes; it decouples the build-up bonus from the damage bonus.)
- `static float ApplyBuildUp(AActor* Target, const FGBDamageInfo& Info);` returns the raw amount sent. Does nothing unless `Info.BuildUp > 0`, the element is Fire / Lightning / Ice, `CanDamage(Info.Instigator, Target)`, and the target has a `UGBElementalStatusComponent`. Amount = `BuildUp * (bHeadshot ? max(1, WeakPointBuildUpMult) : 1)`; calls `AddBuildUp(Element, Amount, BuildUpEventId, Instigator)`.
- `DealDamage`: right after a hit that dealt damage (`Dealt > 0`, where `OnDamageDealt` is broadcast), call `ApplyBuildUp(Target, FinalInfo)`. Leave the generic `OnHitEffect` code as it is (the ice floor's slow still uses on-hit effects).
- `Stats/GBAttributeComponent.cpp` `ApplyDamage`: multiply by the element's `<E>DamageTaken` stat (default 1) next to `DamageTaken`.

## 5. Sources

### Spells (`Spells/GBSpellTypes.cpp`, `GBSpellActor`, `GBSpell_Zone`)
- `FGBSpellCast::MakeDamageInfo`: `Info.BuildUp = Get(GBStat::BuildUp, 0)` and `Info.BuildUpEventId = UGBCombatLibrary::NewBuildUpEventId()`. Every behavior already builds its hits with it: a projectile gets one id for its whole flight (a lightning bolt passing through three enemies gives one build-up to each, never two to one; check `AGBProjectile::DamagedActors` still prevents a second hit on the same actor), an explosion or rune blast one id (each target in the radius once), and every tick of a wall, orb, flamethrower or zone builds a new info (one build-up per target per tick). Verify these call sites and say in the report if any of them builds the info differently.
- Flamethrower: build-up rides on its existing 0.3 s damage ticks (`AGBSpell_Beam::DamageTick` -> `DamageTarget`). Do not add another timer. Mana, QTE, hold-to-sustain and the strong-hit interrupt must stay exactly as they are.
- Ice floor deals no damage (`DamagePerSecond` 0) so `DealDamage` never runs: in `AGBSpell_Zone::ZoneTick`, the branch that only applies the on-hit effect also calls `UGBCombatLibrary::ApplyBuildUp(Target, Info)` with `Info = SpellCast.MakeDamageInfo(NAME_None, this)` (one info per tick, HitLocation = target location). Its slow, `EffectStrength`, Deep Freeze and the 80% cap are untouched.
- Ice wall and ice shield have no `BuildUp` param: they build up nothing.

### Weapons (`Weapons/GBHeldItem.cpp`, `GBGun.cpp`)
- `AGBHeldItem::MakeDamageInfo`: after the element is final (weapon element, or the gun enchant's element): if `GBElemental::ElementIndex(Info.Element) >= 0`, `Info.BuildUp = GetStat(GBElemental::WeaponBuildUp(i), 0)`; `Info.WeakPointBuildUpMult = GetStat(GBStat::WeakPointBuildUpMult, Data->GetGameValue("WeakPointBuildUpMult", 1))`; `Info.BuildUpEventId = NewBuildUpEventId()`. Use `GetStat` so `Weapon.*` modifiers reach these values. An unenchanted gun is Physical, the apprentice staff Arcane: no build-up. The sword uses its current gem element.
- Check that `MakeDamageInfo` runs once per trigger pull: `AGBGun` builds `BaseInfo` once per shot and passes it to `FireHitscan` / `FireProjectile`, so all pellets share one `BuildUpEventId` and the event log keeps only the best pellet (a head pellet after a body pellet adds only the difference). Same for the sword (one id per swing) and the staff (one per bolt). If any of them calls it per pellet, fix that.
- Weapon damage, falloff, the head damage bonus and everything else stay the same.

### Electric grenade (`Items/GBInventoryComponent.cpp`, throw path around `Config.Damage`)
- `Config.Damage.BuildUp = ReadItemStat(Def, GBStat::BuildUp, 0)` and a new event id. The grenade no longer uses an on-hit effect (data below).

## 6. Enemies (`AI/GBEnemyCharacter.h/.cpp`)
- Create a `UGBElementalStatusComponent` in the constructor (`ElementalStatus`, UPROPERTY VisibleAnywhere, getter `GetElementalStatus()`).
- BeginPlay: bind `ElementalStatus->OnStatusStarted` to `HandleElementalStatusStarted(EGBElement, FName Reaction, float Duration)`: Lightning `Stun` -> `ApplyControlLock(Duration)`; Lightning `Interrupt` -> `InterruptAttack()`; Ice `Freeze` -> `ApplyControlLock(Duration)`. Everything else: nothing here.
- `void InterruptAttack()`: if the state is Windup, `SetState(Recover)` (the telegraph goes off in SetState; the attack is lost and the interval starts again).
- `void ApplyControlLock(float Seconds)`: ignored when dead; `ControlLockTime = max(ControlLockTime, Seconds)`; `InterruptAttack()`; `StopMoving()` once.
- `Tick`: right after the dead / era-presence early return, if `ControlLockTime > 0`: subtract DeltaSeconds, `ResetStuck()`, and return **before** `StateTime` and the other timers advance (state timers pause while locked, so the attack interval runs after the lock). Gravity and knockback still work (character movement).
- `float GetControlLockTime() const` (for the debug view). Death sets it to 0.

## 7. Data (all values provisional, to balance; they match GDD 18)

### `Game.json` Values (keep the existing keys)
`"ElementalTickInterval": 0.1`, `"WeakPointBuildUpMult": 1.5`, and for each element E in Fire, Lightning, Ice:
`"ElementalDefault.<E>DamageTaken": 1`, `"ElementalDefault.<E>BuildUpTaken": 1`, `"ElementalDefault.<E>BuildUpThreshold": 100`, `"ElementalDefault.<E>BuildUpDecayDelay": 2`, `"ElementalDefault.<E>BuildUpDecayRate": 20`;
`"ElementalDefault.FireStatusDuration": 4`, `"ElementalDefault.LightningStatusDuration": 1.2`, `"ElementalDefault.IceStatusDuration": 2.5`;
`"ElementalDefault.FireStatusImmunity": 3`, `"ElementalDefault.LightningStatusImmunity": 4`, `"ElementalDefault.IceStatusImmunity": 5`;
`"ElementalDefault.IgniteDamageMult": 1`, `"ElementalDefault.ShockDischargeDamage": 10`, `"ElementalDefault.FrostSlow": 0.6`.
`Game.json` Strings: `"ElementalDefault.FireStatusReaction": "Ignite"`, `"ElementalDefault.LightningStatusReaction": "Stun"`, `"ElementalDefault.IceStatusReaction": "Freeze"`.

### `Enemies.json`
- `Orc_Past`, `TrainingDummy`: no new keys (defaults).
- `Orc_Past_Brute` Stats: `FireDamageTaken 0.8`, `FireBuildUpTaken 0.75`, `FireBuildUpThreshold 160`, `LightningBuildUpThreshold 200`, `IceBuildUpThreshold 180`, `FrostSlow 0.5`; Strings: `"LightningStatusReaction": "Interrupt"`, `"IceStatusReaction": "Slow"`.
- `Orc_Future` Stats: `LightningDamageTaken 1.25`, `LightningBuildUpTaken 1.25`, `FireBuildUpTaken 0.75`.

### `StatusEffects.json`
- Remove `Burn`, `Shock` and `Chill` (grep the code and every JSON for those ids first: the smoke script, the HUD, comments, `ARCHITECTURE.md` excepted (the architect updates docs). Replace or remove each reference; nothing may still apply them).
- `Slow`: its modifier op becomes `"MultiplyMin"` (value 0.5 unchanged).
- Add `Ignite`: `"DisplayName": "Ignited", "Duration": 4.0, "TickInterval": 0.5, "HealthPerTick": -6.0, "Element": "Fire", "bRefreshOnReapply": true, "MaxStacks": 1, "Strings": { "Color": "#FF6A10" }`.
- `Enchant_Fire`, `Enchant_Lightning`, `Enchant_Ice`: remove `WeaponOnHitEffect` from their Strings (keep `WeaponElement`, `Color`, the damage modifier). The code that reads it must keep working when it is absent.

### `Spells.json`
- Remove `"OnHitEffect"` from Fireball, FireWall, Flamethrower, LightningBolt, LightningRune, StaticOrb. Remove their `EffectDuration` param too, unless a modifier key anywhere references it (the validator would warn). IceFloor keeps `"OnHitEffect": "Slow"` and all its params.
- Add a `BuildUp` param (same shape as the others): Fireball Base 40 / Min 25 / Max 55 ScaledBy Power; FireWall 10 / 10 / 10 None; Flamethrower 8 / 8 / 8 None; LightningBolt 35 / 20 / 50 Power; LightningRune 60 / 35 / 85 Power; StaticOrb 8 / 8 / 8 None; IceFloor 5 / 5 / 5 None.
- Descriptions (game text, English, short): drop "sets enemies alight", "burns", "shocks"; say what the spell does and, in a few words, the element it builds up. Example: "A ball of fire that explodes on impact. Builds up fire."

### `Weapons.json` (Stats)
- Pistol: `FireBuildUp 15`, `LightningBuildUp 15`, `IceBuildUp 15`. Shotgun: 25 each (per shot, see section 5). Rifle: 8 each. ElementalSword: 20 each. Staff_Ember: `FireBuildUp 10` only, and remove its `"OnHitEffect": "Burn"` string. Staff_Apprentice: nothing.

### `Items.json`
- ElectricGrenade: remove `"OnHitEffect": "Shock"`; add Stat `"BuildUp": 60`; description: "Bounces, then explodes in a burst of lightning that builds up shock."

## 8. Debug view and console command (testing aid, not the game's indicator)
- Console variable `gb.ShowElemental` (int, default 0) declared in `UI/GBHUD.cpp`. When 1, `AGBHUD::DrawHUD` (right after `DrawDamageNumbers`) draws, for each living, present `AGBEnemyCharacter` within 4000 cm of the player and in front of the camera, small text over its head: one line per element that is not idle, e.g. `Fire 40/100`, `Lightning SHOCK Stun 0.8s`, `Ice IMMUNE 3.2s`, plus `LOCK 0.6s` when `GetControlLockTime() > 0`. Use the HUD's existing small-text drawing. Cheap: one `TActorIterator` pass per frame only while the variable is 1.
- Console command `gb.BuildUp <Fire|Lightning|Ice> <Amount>` in `Core/GBDevCommands.cpp`: adds that raw build-up (through `UGBCombatLibrary::ApplyBuildUp`, player as instigator, a new event id) to every living enemy within 1500 cm of the player, and logs per enemy: id, value / threshold, phase.

## 9. Smoke script (`Core/GBDevCommands.cpp`, near the end, where it does not disturb later steps)
On the training dummy (spawned by the arena; find it by enemy id `TrainingDummy`):
- First, dedupe: on the empty fire bar, two adds of 20 with the SAME event id must leave it at 20 (times the dummy's BuildUpTaken, 1 by default): log `SMOKE elemental dedupe ok=%d`.
- Then for each element: send build-up of 30 with new event ids (through `UGBCombatLibrary::ApplyBuildUp`, player as instigator) until it starts the status (at most 10 adds). Log `SMOKE elemental <E> triggered=%d adds=%d`. For Fire also log `ignite=%d` (`UGBStatusEffectComponent::HasEffect("Ignite")`). Right after, one more add must be refused: log `blockedWhileActive=%d`. Any failure -> `UE_LOG(LogGB, Error, ...)`.
- Leave a second of game time after these steps before the next screenshot.

## 10. Tests (pure logic, new file `Tests/GBElementalTests.cpp`, plus one in `Tests/GBStatsTests.cpp`)
Each test must be able to fail. Check that no test name is a prefix of another.
- `Glyphbound.Elemental.MeterThreshold`: below the threshold no trigger; reaching it exactly triggers, the value is 0, the phase Active; adds during Active and during Immune are ignored; after StatusDuration the phase is Immune, after ImmunityDuration Building again and adds count; ImmunityDuration 0 goes straight back to Building; two configs (threshold 100 vs 160 with BuildUpTaken 0.75) need different numbers of the same add to trigger; BuildUpTaken 0 never fills.
- `Glyphbound.Elemental.MeterDecay`: nothing drains during the delay; after it the bar drains at DecayRate; one Advance of 1.0 s equals ten of 0.1 s (within 1e-3) both for a run that crosses the delay boundary and one fully after it; never below 0; an add resets the delay.
- `Glyphbound.Elemental.EventDedupe`: the same id twice adds once; the same id with a bigger amount adds only the difference; id 0 is never deduped; after 8 other ids the first one is forgotten (adds again).
- `Glyphbound.Stats.StrongestSlow`: with the existing stats API: a `MultiplyMin` 0.5 scaled by strength 1.1 gives 0.45 (Deep Freeze rank 1); add a `MultiplyMin` 0.4 -> the result is 0.4, not 0.18; add a plain `Multiply` 0.8 -> 0.32; remove the 0.4 source -> 0.36.

## Verification (all must be true when you finish)
1. `powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Jam/Build-Jam.ps1` -> `Result: Succeeded`.
2. `powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Jam/Build-Jam.ps1 -Target Glyphbound` -> `Result: Succeeded`.
3. `powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Jam/Test-Jam.ps1 -MinTests 34` passes.
4. `powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Jam/Smoke-Jam.ps1` ends with `Smoke: done=True crashed=False LogGB errors=0 timedOut=False`, with `SMOKE path check passed`, the flamethrower lines as before, the new elemental SMOKE lines, and no `LogGB` warning.
5. `grep` for `"Burn"`, `"Chill"`, `WeaponOnHitEffect` values and `OnHitEffect": "Shock"` in `Glyphbound/Content/Data` finds nothing.
Never make an error disappear by deleting behavior. All JSON files stay valid. No gameplay numbers in code.

## Final report (plain text)
1. The result lines of the four runs verbatim, plus every new SMOKE line.
2. What you changed, file by file.
3. The call-site checks of section 5 (projectile pierce, explosions, ticks, one `MakeDamageInfo` per shot / swing / bolt): what you found.
4. Every reference to Burn / Shock / Chill you removed or changed.
5. Anything you could not do as described, and why.
6. What a person should check by playing.
