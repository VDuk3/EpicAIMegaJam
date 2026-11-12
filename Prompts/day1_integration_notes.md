# Implementers' reports (input for the integration pass)

Condensed by the architect from each implementer's final report. "Requests" are things one module needs from another: check they are satisfied while integrating.

## QTE
- Added in `QTE/GBQTEComponent.h`: `namespace GBQTEStat` (QTE.GestureMinScore, QTE.GestureMinLength, QTE.GestureIdleSubmitTime, QTE.LineSubmitLengthMult, QTE.StickGestureScale), `GetLastResult()`, `GetStickGestureScale()`.
- Request to Player: right-stick gesture input must be `Stick * QTE->GetStickGestureScale() * FApp::GetDeltaTime()` (undilated: the world runs at 0.1 speed during the QTE). Mouse deltas go in as raw pixels, with +Y down.
- Request to Core: some .cpp must contain `DEFINE_LOG_CATEGORY(LogGB)` (expected in `Glyphbound.cpp`).
- Behavior: the component's tick is off until `StartQTE`; `Finish` always sets global time dilation back to 1; `CancelQTE` fires the delegate; with zero sequence steps and zero gestures the result is a success.
- Tests: `Glyphbound.QTE.GestureLine`, `GestureCircle`, `GestureShort`, `SessionScore`, `SessionAllMissed`, `SessionTimeout`.

## ITEMS (inventory + skill trees)
- Added in `Items/GBInventoryComponent.h`: `namespace GBItemStat { ThrowPitch }`. Only protected additions otherwise.
- Requests: sprite ids used by Items.json (potion_health, potion_mana, injection_health, grenade_electric, ammo_magazine, armor_chronoweave, gem_fire, gem_lightning, gem_ice, sword_hilt, quest_past, quest_future); weapon id `ElementalSword`; status effects `HealthPotion`, `ManaPotion`, `HealthInjection`, `Shock`; Skills.json uses the keys `Spell.Fireball.Damage`, `Spell.Fireball.ExplosionRadius`, `Spell.LightningBolt.Damage`, `Spell.IceWall.Health`, `Spell.All.ManaCost`, `Spell.All.Cooldown`, `Weapon.Shotgun.FalloffNearMult`, `Weapon.Rifle.FalloffFarMult`: check each against the param / stat names really used in Spells.json and Weapons.json and fix the key in Skills.json if a name differs.
- Risk to verify: JSON enum values are short names ("Consumable", "Multiply", "Lightning"). Confirm that `FJsonObjectConverter` resolves short names for `enum class` properties; a test or a log check after loading is enough.
- Risk: `TryCombine` sets the sword's element after `GiveWeapon`; the HUD is not notified (acceptable if the HUD reads it every frame).
- Test: `Glyphbound.Progression.UnlockRules`.

## FOUNDATION (data loader, stats, attributes, placeholders, progress)
- No public API changes. `DEFINE_LOG_CATEGORY(LogGB)` and `gb.ReloadData` are in `Glyphbound.cpp`.
- Behaviors other modules must respect:
  - `UGBAttributeComponent::BeginPlay` calls `InitializeFromStats()` only if the owner's stats already have `MaxHealth` and health is still 0; owners that set stats later must call it themselves.
  - `ApplyDamage` returns 0 and fires nothing when a hit is fully blocked; it returns damage capped at the remaining health.
  - `SetHealthAndMana` revives when health > 0. `SpendMana(<= 0)` returns true.
  - `GBPlaceholder::SetupShape` creates a dynamic material: never call it from a constructor (class default object); call it from BeginPlay / Init functions. CHECK every caller.
- Risk: `FJsonObjectConverter::JsonArrayToUStruct` fails the whole array when one element is bad (e.g. a misspelled enum): then the table is empty and a red error shows. After the build is green, validate every JSON file against its struct (enum spellings, field names).
- Tests: `Glyphbound.Stats.ModifierOrder`, `ModifierSources`, `Regen`, `ScaledParam`.

## COMBAT (damage library, status effects, projectile)
- Added protected members and UFUNCTION handlers in `Combat/GBProjectile.h` and `Combat/GBStatusEffectComponent.h` (accepted by the architect). No public API change.
- Behaviors other modules must respect:
  - `DealDamage` returns 0 early when the final Amount is <= 0, so a 0-damage hit never applies `OnHitEffect`. Anything that only applies an effect (ice floor) must call `UGBStatusEffectComponent::ApplyEffect` directly. CHECK `Spells/GBSpell_Zone.cpp` and the other behaviors.
  - Projectiles ignore actors without attributes (they fly through pickups), dead actors and Neutral-team actors; a blocking hit on a non-target counts as hitting the world. Collision: object type WorldDynamic; overlap Pawn and WorldDynamic; block WorldStatic; ignore everything else. So a blocking spell wall must be WorldStatic (or block the projectile's WorldDynamic object type) to stop projectiles.
  - `ApplyEffect` returns false when the owner is dead. Instant health/mana is applied again on each re-apply.
  - `DealRadialDamage` with a null instigator damages everything with attributes.
- Not verified: the float overload of `TestEqual` with a tolerance argument (use `TestNearlyEqual` if it does not compile).
- Tests: `Glyphbound.Combat.DistanceFalloff`, `Headshot`, `TeamHostility`.

## WEAPONS (held items, equipment)
- Added in `Weapons/GBHeldItem.h`: `GBWeaponStat::AimTime`, `GBWeaponString::WeaponElement` / `WeaponOnHitEffect`, `GBWeapon::AimTraceChannel` (= ECC_Camera, because the engine Pawn profile ignores Visibility), public `IsAutomatic()`, `GetActiveEnchant(...)`, `Tick`; protected trigger helpers and `PerformAttack()`. `AGBGun` adds `GetMagazineSize()`, `GetMaxSpareMagazines()`.
- Requests: enemy capsules must block ECC_Camera (the default Pawn profile does; CHECK that `AI/GBEnemyCharacter.cpp` did not set a custom response that ignores Camera); the player should call `StopPrimary` / `StopSecondary` on the held item when control is disabled or on death; sprites `staff_apprentice`, `staff_ember`, `bolt_arcane`, `fireball`, `sword_blade`.
- The gun drives the camera FOV itself (`SetFieldOfView`) and restores it when put away.
- Viewmodel parts are created at runtime with `NewObject` + `RegisterComponent` in `InitFromDefinition`.
- Not verified: `DrawDebugLine` include (`DrawDebugHelpers.h`).

## AI (enemies, encounters)
- Added: `GBEnemyKey` (Game.json `CorpseTime`, `EnemyMeleeReachTolerance`; Enemies.json string `ProjectileColor`) and `GBEncounterKey::SpawnRingRadius` (Game.json `EncounterSpawnRingRadius`). Add these three values to `Game.json` if missing: CorpseTime 1.5, EnemyMeleeReachTolerance 1.2, EncounterSpawnRingRadius 400.
- Risk to verify: the enemy constructor sets the capsule's Visibility response to Block on top of the Pawn profile. Guns trace on ECC_Camera: make sure the capsule still blocks Camera.
- The encounter detects a dead player in its own Tick and resets itself (the game mode also resets active encounters on respawn: both are fine, make sure it is not done twice in a way that breaks).
- The windup telegraph multiplies the billboard's relative scale captured after `SetupBillboard`.
- Not verified: `UBoxComponent::InitBoxExtent`, `UPrimitiveComponent::IsOverlappingActor`.

## PLAYER (character, controller, input)
- Added: `UGBInputSet::LookStick` (right stick; `Look` is mouse only), `QTEUp/Left/Down/Right`; `AGBPlayerCharacter::ReleaseHeldInputs()`; `GBPlayerCharacter.h` includes `QTE/GBQTELogic.h`.
- Look changes the control rotation directly (not `AddControllerYawInput`), clamping pitch itself.
- Stick gesture input currently uses a code constant (900 px/s). REQUIRED by the QTE module: use `QTE->GetStickGestureScale()` and the undilated delta (`FApp::GetDeltaTime()`) instead of the constant.
- Mouse gesture: converts Mouse2D units to pixels by dividing by 0.07 (assumed legacy sensitivity). Leave as is; a human checks it while playing.
- Dash switches to `MOVE_Flying` for its duration and restores `JumpCurrentCount`.
- Expects the game mode to call `PC->SetMenu(EGBMenu::Death)` on death and `SetMenu(None)` after `RespawnPlayer`.
- Not compile-checked: `BindAction` with payloads (`int32`, `EGBQTEKey`), the input modifier members, `UWorld::DeltaRealTimeSeconds`.

## SPELLS (spellbook, cast resolution, 8 behaviors)
- Added: `AGBSpellActor::StartSpell()` (protected virtual run once after InitSpell and BeginPlay) and helper methods; `GBSpellParam` constants (SpriteSize, Distance, ExplosionRadius, ExplosionFalloff, Pierce, VisualScale, Blocking, ArmTime, TriggerRadius, BlastHeight, Falloff, ShowArea, ConeAngle, ManaPerSecond); `GBSpellString::EffectPrefix`; `AGBSpell_Shield::GetShieldHealth()`, `ComputeBlockedDamage`; `AGBSpell_Enchant::MakeEffectId`, `ComputeDurationScale`; spellbook `HandleQTEFinished(const FGBQTEResult&, FName)` (payload order of `CreateUObject`).
- The spellbook cancels its sustained spell itself on weapon switch and on death.
- A blocking ice wall is a Neutral actor with attributes using the `BlockAllDynamic` profile (object type WorldDynamic).
- Not verified: `IsValid(TObjectPtr)` implicit conversion.
- Tests: `Glyphbound.Spells.ResolveScaling`, `ResolveNoQTE`, `DamageInfo`.

### ARCHITECT DECISION (apply during integration): Neutral targets are damageable by everyone
The projectile overlaps WorldDynamic and ignores Neutral actors, so as written it would fly THROUGH the ice wall, and nothing could wear a wall down. Rule to implement:
1. `UGBCombatLibrary`: add `static bool CanDamage(const AActor* Instigator, const AActor* Target)`: true when the target has attributes and is alive and (the instigator is null, OR either team is Neutral, OR the teams are hostile). `DealDamage` uses it (still honoring `bAllowFriendlyFire`). `DealRadialDamage` uses it too. So Player and Enemy never hurt their own team, and Neutral things (spell walls, future destructibles) can be hurt by anyone. `AreHostile` keeps its meaning (Player vs Enemy only) for AI targeting and for spell area effects that should only touch enemies.
2. `AGBProjectile`: a valid target is anything `CanDamage` accepts except the instigator itself; a projectile that touches a Neutral actor with attributes damages it and stops (no pierce through walls).
3. Spell area behaviors (wall burn, zone, orb, beam, trap) keep using hostile-only gathering, so the player's fire wall does not burn the player's ice wall. Leave them as they are.

## UI (HUD, menus, pixel sprites)
- No public API change. New files `UI/GBHUD_Menus.cpp`, `UI/GBHUDShared.h`. `UI/GBHUD.h` now includes `Spells/GBSpellTypes.h`.
- `SetupBillboard` sets scale = 2 * WorldHeight / texture height and calls `MarkRenderStateDirty()`; in editor builds it divides out `AActor::SpriteScale` and `UBillboardComponent::EditorScale` (editor-only members: watch the guards).
- Watch on first compile: mixed float/double arguments to `FMath` calls; `TestEqual` overloads for `FColor` in `Tests/GBSpriteTests.cpp`.
- Sprites.json has 36 sprites, generated and validated by a script (every row the same length).
- Test: `Glyphbound.UI.SpritePixels`.

## WORLD (eras, portal, chest, pickups, checkpoint, game mode, test arena)
- Added: `GBGameValue` keys (EraFadeOutTime, EraHoldTime, EraFadeInTime in `World/GBEraSubsystem.h`; RespawnDelay in `Core/GBGameMode.h`): the same namespace is opened in two headers, which is fine as long as no name repeats. `UGBEraSubsystem::OnWorldBeginPlay`; `AGBInteractableActor::CanInteract` override; `AGBPickup::GiveDrop`, `GetDropDisplayName`, `IsDropValid`; `GBPickupString::PickupLook`; `AGBGameMode::Tick` (respawns in real time even if the game is paused). New classes `AGBPortal`, `AGBChest`, `AGBCheckpointVolume`, `AGBTestArena`.
- ARCHITECT DECISION (art rule, GDD 3: cyberpunk things are 3D): add `"PickupLook": "3D"` to the Strings of `HealthInjection`, `ElectricGrenade`, `Ammo_Magazine` and `SwordHilt` in `Items.json`.
- Gameplay actors of the arena are spawned in the arena's BeginPlay (relies on actors spawned during world BeginPlay also getting BeginPlay).
- Not verified: implicit `TArrayView` from C arrays in calls to `AGBChest::Configure` / `AGBEncounter::Configure` (wrap in `MakeArrayView` if needed).
- Tests: `Glyphbound.Era.PresenceRule`, `Glyphbound.Era.IsPresentIn`.

## Test names expected in total (20)
Stats: ModifierOrder, ModifierSources, Regen, ScaledParam. Combat: DistanceFalloff, Headshot, TeamHostility. Spells: ResolveScaling, ResolveNoQTE, DamageInfo. QTE: GestureLine, GestureCircle, GestureShort, SessionScore, SessionAllMissed, SessionTimeout. Progression: UnlockRules. UI: SpritePixels. Era: PresenceRule, IsPresentIn. (That is 20; run the test script with -MinTests 20.)
