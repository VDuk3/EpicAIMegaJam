// One-off patch used on 2026-11-15 to hook GBAudio into the game's sources (each hook is one line; see the commit). Idempotent.
const fs = require('fs'), path = require('path');
const SRC = path.resolve(__dirname, '..', '..', 'Glyphbound', 'Source', 'Glyphbound');
const DECL = 'namespace GBAudio { bool Play(const UObject* Ctx, FName Key, const FVector* Loc = nullptr, float PitchMul = 1.f, float VolMul = 1.f); void SpawnLoop(const UObject* Ctx, FName Key, const FVector& Loc); bool IsFutureMap(const UObject* Ctx); void MenuPing(const UObject* Ctx); void UpdateMusic(UWorld* World); }\n';
function patch(file, edits, decl = true) {
  const p = path.join(SRC, file);
  let t = fs.readFileSync(p, 'utf8');
  const crlf = t.includes('\r\n');
  if (crlf) t = t.replace(/\r\n/g, '\n');
  if (t.includes('GBAudio')) { console.log('skip (already)', file); return; }
  for (const [a, b] of edits) {
    if (!t.includes(a)) throw new Error(file + ': anchor not found: ' + a.slice(0, 60));
    if (t.split(a).length !== 2) throw new Error(file + ': anchor not unique: ' + a.slice(0, 60));
    t = t.split(a).join(b);
  }
  if (decl) {
    // declaration after the last #include line
    const lines = t.split('\n'); let last = 0;
    lines.forEach((l, i) => { if (/^#include/.test(l)) last = i; });
    lines.splice(last + 1, 0, DECL.trimEnd());
    t = lines.join('\n');
  }
  if (crlf) t = t.replace(/\n/g, '\r\n');
  fs.writeFileSync(p, t);
  console.log('patched', file);
}

// explosions (radial damage) -- inside GBCombatLibrary.cpp (GBAudio is defined there)
{
  const p = path.join(SRC, 'Combat/GBCombatLibrary.cpp');
  let t = fs.readFileSync(p, 'utf8');
  const crlf = t.includes('\r\n'); if (crlf) t = t.replace(/\r\n/g, '\n');
  if (!t.includes('FName(TEXT("explosion"))')) {
    const a = '\tUWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);\n';
    const i = t.indexOf('int32 UGBCombatLibrary::DealRadialDamage');
    const j = t.indexOf(a, i);
    if (j < 0) throw new Error('radial anchor');
    const k = j + a.length;
    t = t.slice(0, k) + '\tif (World && Radius >= 120.f)\n\t{\n\t\tGBAudio::Play(WorldContextObject, FName(TEXT("explosion")), &Origin, 1.f, 1.f);\n\t}\n' + t.slice(k);
    if (crlf) t = t.replace(/\n/g, '\r\n');
    fs.writeFileSync(p, t); console.log('patched radial');
  }
}

// weapon attacks
patch('Weapons/GBHeldItem.cpp', [[
  '\tif (PerformAttack())\n\t{\n',
  '\tif (PerformAttack())\n\t{\n\t\tif (!GBAudio::Play(this, FName(*(TEXT("staff_") + WeaponId.ToString())), nullptr, 1.f, 1.f)\n\t\t\t&& !GBAudio::Play(this, FName(*(TEXT("gun_") + WeaponId.ToString())), nullptr, 1.f, 1.f))\n\t\t{\n\t\t\tGBAudio::Play(this, FName(TEXT("sword_swing")), nullptr, 1.f, 1.f);\n\t\t}\n']]);
patch('Weapons/GBGun.cpp', [[
  '\tbReloading = true;\n\tReloadElapsed = 0.f;\n\tReloadDuration = FMath::Max(0.f, GetStat(GBStat::ReloadTime, 1.f));\n\tResetPump();\n',
  '\tbReloading = true;\n\tGBAudio::Play(this, FName(TEXT("gun_reload")), nullptr, 1.f, 1.f);\n\tReloadElapsed = 0.f;\n\tReloadDuration = FMath::Max(0.f, GetStat(GBStat::ReloadTime, 1.f));\n\tResetPump();\n']]);
patch('Spells/GBSpellbookComponent.cpp', [
  ['\tPendingSpellId = SpellId;\n\n\tUGBQTEComponent* QTE = GetOwnerQTE();',
   '\tPendingSpellId = SpellId;\n\tGBAudio::Play(this, FName(*(TEXT("spell_") + SpellId.ToString())), nullptr, 1.f, 1.f);\n\n\tUGBQTEComponent* QTE = GetOwnerQTE();'],
  ['\tif (Reason != EGBCastFailReason::None)\n\t{\n\t\tOnCastFailed.Broadcast(SpellId, Reason);',
   '\tif (Reason != EGBCastFailReason::None)\n\t{\n\t\tif (Reason == EGBCastFailReason::NotEnoughMana)\n\t\t{\n\t\t\tGBAudio::Play(this, FName(TEXT("spell_fail")), nullptr, 1.f, 1.f);\n\t\t}\n\t\tOnCastFailed.Broadcast(SpellId, Reason);']]);
patch('AI/GBEnemyCharacter.cpp', [
  ['\tSetState(EGBEnemyState::Dead);\n\tControlLockTime = 0.f;\n',
   '\tSetState(EGBEnemyState::Dead);\n\tControlLockTime = 0.f;\n\t{\n\t\tconst FString ClassName = GetClass()->GetName();\n\t\tconst bool bBoss = ClassName.Contains(TEXT("Valkyrie")) || ClassName.Contains(TEXT("Entity"));\n\t\tconst FVector DeathAt = GetActorLocation();\n\t\tGBAudio::Play(this, FName(bBoss ? TEXT("death_boss") : (GBAudio::IsFutureMap(this) ? TEXT("death_future") : TEXT("death_past"))), &DeathAt, 1.f, 1.f);\n\t}\n'],
  ['\tState = NewState;\n\tStateTime = 0.f;\n',
   '\tif (NewState != State && State != EGBEnemyState::Dead)\n\t{\n\t\tconst FVector SoundAt = GetActorLocation();\n\t\tif (NewState == EGBEnemyState::Windup)\n\t\t{\n\t\t\tGBAudio::Play(this, FName(GBAudio::IsFutureMap(this) ? TEXT("enemy_attack_future") : TEXT("enemy_attack_past")), &SoundAt, 1.f, 1.f);\n\t\t}\n\t\telse if (NewState == EGBEnemyState::Chase && State == EGBEnemyState::Idle)\n\t\t{\n\t\t\tGBAudio::Play(this, FName(GBAudio::IsFutureMap(this) ? TEXT("enemy_alert_future") : TEXT("enemy_alert_past")), &SoundAt, 1.f, 1.f);\n\t\t}\n\t}\n\tState = NewState;\n\tStateTime = 0.f;\n']]);
patch('World/GBPickup.cpp', [[
  '\tif (!FlagOnPickup.IsNone())\n\t{\n\t\tif (UGBProgressSubsystem* Progress = UGBProgressSubsystem::Get(this))\n\t\t{\n\t\t\tProgress->SetFlag(FlagOnPickup);\n\t\t}\n\t}\n',
  '\tGBAudio::Play(this, FName(TEXT("pickup")), nullptr, 1.f, 1.f);\n\tif (!FlagOnPickup.IsNone())\n\t{\n\t\tif (UGBProgressSubsystem* Progress = UGBProgressSubsystem::Get(this))\n\t\t{\n\t\t\tProgress->SetFlag(FlagOnPickup);\n\t\t}\n\t}\n']]);
patch('World/GBChest.cpp', [[
  'void AGBChest::Interact(AGBPlayerCharacter* Player)\n{\n\tif (!CanInteract(Player))\n\t{\n\t\treturn;\n\t}\n',
  'void AGBChest::Interact(AGBPlayerCharacter* Player)\n{\n\tif (!CanInteract(Player))\n\t{\n\t\treturn;\n\t}\n\t{\n\t\tconst FVector SoundAt = GetActorLocation();\n\t\tGBAudio::Play(this, FName(TEXT("chest_open")), &SoundAt, 1.f, 1.f);\n\t}\n']]);
patch('World/GBPortal.cpp', [[
  'void AGBPortal::Interact(AGBPlayerCharacter* Player)\n{\n\tif (!CanInteract(Player))\n\t{\n\t\treturn;\n\t}\n',
  'void AGBPortal::Interact(AGBPlayerCharacter* Player)\n{\n\tif (!CanInteract(Player))\n\t{\n\t\treturn;\n\t}\n\tGBAudio::Play(this, FName(TEXT("portal")), nullptr, 1.f, 1.f);\n']]);
patch('World/GBBreakable.cpp', [[
  'void AGBBreakable::Break()\n{\n\tbBroken = true;\n',
  'void AGBBreakable::Break()\n{\n\tbBroken = true;\n\t{\n\t\tconst FVector SoundAt = GetActorLocation();\n\t\tGBAudio::Play(this, FName(TEXT("breakable")), &SoundAt, 1.f, 1.f);\n\t}\n']]);
patch('World/GBTorch.cpp', [[
  '\tSuper::BeginPlay();\n',
  '\tSuper::BeginPlay();\n\tGBAudio::SpawnLoop(this, FName(TEXT("torch_loop")), GetActorLocation());\n']]);
patch('World/GBDoor.cpp', [[
  '\tconst float Step = OpenSeconds > KINDA_SMALL_NUMBER ? DeltaSeconds / OpenSeconds : 1.f;\n',
  '\tif (bWantOpen && OpenAlpha <= 0.f)\n\t{\n\t\tconst FVector SoundAt = GetActorLocation();\n\t\tGBAudio::Play(this, FName(GBAudio::IsFutureMap(this) ? TEXT("door_future") : TEXT("door_past")), &SoundAt, 1.f, 1.f);\n\t}\n\tconst float Step = OpenSeconds > KINDA_SMALL_NUMBER ? DeltaSeconds / OpenSeconds : 1.f;\n']]);
patch('UI/GBHUD_MainMenu.cpp', [
  ['void AGBHUD::MainMenuNavigate(FIntPoint Direction)\n{\n', 'void AGBHUD::MainMenuNavigate(FIntPoint Direction)\n{\n\tGBAudio::Play(this, FName(TEXT("ui_move")), nullptr, 1.f, 1.f);\n'],
  ['void AGBHUD::MainMenuConfirm()\n{\n', 'void AGBHUD::MainMenuConfirm()\n{\n\tGBAudio::Play(this, FName(TEXT("ui_select")), nullptr, 1.f, 1.f);\n'],
  ['bool AGBHUD::MainMenuBack()\n{\n', 'bool AGBHUD::MainMenuBack()\n{\n\tGBAudio::Play(this, FName(TEXT("ui_back")), nullptr, 1.f, 1.f);\n'],
  ['void AGBHUD::DrawMainMenu(EGBHudSkin Skin)\n{\n', 'void AGBHUD::DrawMainMenu(EGBHudSkin Skin)\n{\n\tGBAudio::MenuPing(this);\n']]);
patch('UI/GBHUD_Menus.cpp', [
  ['void AGBHUD::MenuNavigate(FIntPoint Direction)\n{\n', 'void AGBHUD::MenuNavigate(FIntPoint Direction)\n{\n\tGBAudio::Play(this, FName(TEXT("ui_move")), nullptr, 1.f, 1.f);\n'],
  ['void AGBHUD::MenuConfirm()\n{\n', 'void AGBHUD::MenuConfirm()\n{\n\tGBAudio::Play(this, FName(TEXT("ui_select")), nullptr, 1.f, 1.f);\n']]);
patch('Core/GBGameMode.cpp', [
  ['void AGBGameMode::HandlePlayerDeath(AGBPlayerCharacter* Player)\n{\n', 'void AGBGameMode::HandlePlayerDeath(AGBPlayerCharacter* Player)\n{\n\tGBAudio::Play(this, FName(TEXT("player_death")), nullptr, 1.f, 1.f);\n'],
  ['void AGBGameMode::Tick(float DeltaSeconds)\n{\n\tSuper::Tick(DeltaSeconds);\n', 'void AGBGameMode::Tick(float DeltaSeconds)\n{\n\tSuper::Tick(DeltaSeconds);\n\t{\n\t\tstatic float MusicClock = 0.f;\n\t\tMusicClock += FApp::GetDeltaTime();\n\t\tif (MusicClock >= 0.4f)\n\t\t{\n\t\t\tMusicClock = 0.f;\n\t\t\tGBAudio::UpdateMusic(GetWorld());\n\t\t}\n\t}\n']]);
patch('Player/GBPlayerCharacter.cpp', [[
  '\tSuper::Tick(DeltaSeconds);\n\n\tUpdateFocusedInteractable();\n',
  '\tSuper::Tick(DeltaSeconds);\n\n\tUpdateFocusedInteractable();\n\t{\n\t\t// Footsteps: one every ~2.3 m walked on the ground; the surface follows the map era.\n\t\tstatic float Walked = 0.f;\n\t\tconst UCharacterMovementComponent* Move = GetCharacterMovement();\n\t\tif (Move && Move->IsMovingOnGround() && !IsDead() && GetVelocity().Size2D() > 60.f)\n\t\t{\n\t\t\tWalked += GetVelocity().Size2D() * DeltaSeconds;\n\t\t\tif (Walked >= 230.f)\n\t\t\t{\n\t\t\t\tWalked = 0.f;\n\t\t\t\tGBAudio::Play(this, FName(GBAudio::IsFutureMap(this) ? TEXT("footstep_future") : TEXT("footstep_past")), nullptr, 1.f, 1.f);\n\t\t\t}\n\t\t}\n\t}\n']]);
