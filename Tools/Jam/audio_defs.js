// Audio definitions for Glyphbound. Run:  node Tools/Jam/audio_defs.js
//  - converts the source sounds (Incoming\ASSETS\...Bibliotecas_Sonidos + audio\*.mp3) to 44.1 kHz 16-bit WAV in Glyphbound/SourceArt/Audio/<cat>/<name>.wav
//  - writes Glyphbound/Content/Data/Audio.json (game data: key -> sound assets and play settings)
//  - writes Tools/Jam/audio_import_list.json (read by import_audio.py inside the Editor)
// Sound keys are played from C++ by GBAudio::Play(Key). Edit this file and rerun to retune (no recompile needed).
const fs = require('fs'), path = require('path'), cp = require('child_process');
const ROOT = path.resolve(__dirname, '..', '..');
const LIB = path.join(ROOT, 'Incoming', 'ASSETS', '20261116-002002_Bibliotecas_Sonidos');
const MB = 'Medie bits/', SC = 'SciFiCoreSounds/';
const MUS = path.join(ROOT, 'Incoming', 'ASSETS', 'audio');
const OUT = path.join(ROOT, 'Glyphbound', 'SourceArt', 'Audio');

// key: { cat, src:[relative paths], vol, pmin, pmax, range (cm, 0 = 2D), cd (cooldown s), dur (max seconds), loop, stereo, music }
const D = {};
const S = (key, cat, src, o = {}) => { D[key] = Object.assign({ cat, src, vol: 1, pmin: 0.94, pmax: 1.06, range: 3500, cd: 0.04, dur: 3 }, o); };
const mb = a => a.map(x => MB + x), sc = a => a.map(x => SC + x);

// ---- player attacks ----
S('staff_Staff_Apprentice', 'Player', mb(['ataque con magia 2.wav']), { vol: 0.8, dur: 1.5, range: 0 });
S('staff_Staff_Ember', 'Player', mb(['ataque con magia, bola de fuego/ataque con magia.wav', 'ataque con magia, bola de fuego/ataque con magia 2.wav']), { vol: 0.8, dur: 1.5, range: 0 });
S('staff_Staff_Frost', 'Player', mb(['explosion/magic 3.wav', 'explosion/magig.wav']), { vol: 0.7, dur: 1.5, range: 0 });
S('staff_Staff_Storm', 'Player', sc(['electricidad/electric 1.wav', 'electricidad/electric 2.wav', 'electricidad/electric 3.wav']), { vol: 0.7, dur: 1.2, range: 0 });
S('gun_Pistol', 'Player', sc(['pistola/pistola 1.wav', 'pistola/pistola 2.wav', 'pistola/pistola 3.wav', 'pistola/pistola 4.wav']), { vol: 0.8, dur: 1.2, range: 0, pmin: 0.96, pmax: 1.04 });
S('gun_Shotgun', 'Player', sc(['escopeta/escopeta 1.wav', 'escopeta/escopeta 2.wav', 'escopeta/escopeta 3.wav']), { vol: 0.9, dur: 1.8, range: 0 });
S('gun_Rifle', 'Player', sc(['rifle/111.wav', 'rifle/112.wav', 'rifle/113.wav', 'rifle/114.wav']), { vol: 0.7, dur: 0.9, range: 0, cd: 0.02, pmin: 0.97, pmax: 1.05 });
S('gun_reload', 'Player', sc(['recarga/arma cargando para disparar.wav', 'recarga/arma cargando para disparar 2.wav']), { vol: 0.7, dur: 2.5, range: 0 });
S('sword_swing', 'Player', mb(['espada/espada esquivada.wav', 'esquivar/esquivar 1.wav', 'esquivar/esquivar 2.wav', 'esquivar/esquivar 3.wav']), { vol: 0.8, dur: 1.2, range: 0 });
S('hit_sword', 'Hits', mb(['espada/espada 1.wav', 'espada/espada 2.wav', 'espada/espada 3.wav', 'espada/espada 4.wav', 'espada/espada 5.wav']), { vol: 0.9, dur: 1.2 });
S('player_dash', 'Player', mb(['esquivar/esquivar 1.wav', 'esquivar/esquivar 2.wav']), { vol: 0.7, dur: 1, range: 0 });
S('footstep_past', 'Player', mb(['pasos/pasos 2.wav', 'pasos/pasos 3.wav', 'pasos/pasos sobre tierra.wav', 'pasos/pasos sobre grava o tierra.wav']), { vol: 0.5, dur: 0.45, range: 0, pmin: 0.9, pmax: 1.1, cd: 0.15 });
S('footstep_future', 'Player', sc(['pasos en metal/Metal_Footstep_01.wav', 'pasos en metal/Metal_Footstep_02.wav', 'pasos en metal/Metal_Footstep_03.wav']), { vol: 0.5, dur: 0.6, range: 0, pmin: 0.9, pmax: 1.1, cd: 0.15 });
S('player_hurt', 'Player', mb(['golpe.wav']), { vol: 0.9, dur: 0.7, range: 0, pmin: 0.75, pmax: 0.88, cd: 0.1 });
S('player_death', 'Player', mb(['perder algo.wav']), { vol: 0.8, dur: 3, range: 0, cd: 1 });

// ---- spells (key = spell_<SpellId>) ----
S('spell_Fireball', 'Spells', mb(['ataque con magia, bola de fuego/ataque con magia 3.wav', 'ataque con magia, bola de fuego/ataque con magia 2.wav']), { vol: 0.9, dur: 2, range: 0 });
S('spell_FireWall', 'Spells', mb(['madera/fogata o antorcha.wav']), { vol: 0.6, dur: 3, range: 0 });
S('spell_Flamethrower', 'Spells', sc(['vapor/vapor 1.wav']), { vol: 0.7, dur: 2.5, range: 0, cd: 0.6 });
S('spell_LightningBolt', 'Spells', sc(['electricidad/electric 4.wav', 'electricidad/electric 5.wav']), { vol: 0.9, dur: 2, range: 0 });
S('spell_LightningRune', 'Spells', sc(['electricidad/electricidad 6.wav']), { vol: 0.8, dur: 2, range: 0 });
S('spell_StaticOrb', 'Spells', sc(['computos/computos 1.wav']), { vol: 0.7, dur: 2, range: 0 });
S('spell_IceWall', 'Spells', mb(['stone/explosion de piedras.wav']), { vol: 0.8, dur: 2, range: 0 });
S('spell_IceShield', 'Spells', sc(['recarga/dispositivo cargando.wav']), { vol: 0.7, dur: 2, range: 0 });
S('spell_IceFloor', 'Spells', sc(['vapor/vapor 3.wav']), { vol: 0.7, dur: 2, range: 0 });
S('spell_Air_Dash', 'Spells', mb(['esquivar/esquivar 3.wav']), { vol: 0.7, dur: 1, range: 0 });
S('spell_Air_DoubleJump', 'Spells', mb(['esquivar/esquivar 1.wav']), { vol: 0.6, dur: 1, range: 0 });
S('spell_WeaponEnchant', 'Spells', mb(['ganar algo.wav']), { vol: 0.8, dur: 2.5, range: 0 });
S('spell_fail', 'UI', sc(['error/error 1.wav', 'error/error 2.wav']), { vol: 0.6, dur: 1, range: 0, cd: 0.25 });
S('explosion', 'Hits', mb(['explosion/252692-SW001-8-Bit-Games-024-Explosion.wav', 'explosion/511979-8_Bit_Large_Destroyed_02_-GAMEVideo-8_bit-impact-hit-damaged-destroyed.wav', 'stone/explosion.wav']), { vol: 0.9, dur: 2.5, range: 6000, cd: 0.12 });

// ---- hits and deaths ----
S('hit_flesh', 'Hits', mb(['golpe.wav', 'madera/golpe a madera.wav']), { vol: 0.8, dur: 0.7, range: 3000 });
S('hit_robot', 'Hits', sc(['corto circuito/corto circuito 1.wav', 'corto circuito/corto circuito 2.wav', 'corto circuito/corto circuito 3.wav']), { vol: 0.6, dur: 0.8, range: 3000 });
S('hit_Lightning', 'Hits', sc(['electricidad/electric 4.wav', 'electricidad/electric 5.wav']), { vol: 0.5, dur: 0.8, range: 3000 });
S('hit_Fire', 'Hits', mb(['explosion/511980-8_Bit_Small_Destroyed_01_-GAMEVideo-8_bit-impact-hit-damaged-destroyed.wav']), { vol: 0.5, dur: 0.8, range: 3000 });
S('hit_Ice', 'Hits', mb(['stone/pequeñas piedras cayendo.wav']), { vol: 0.5, dur: 0.8, range: 3000 });
S('death_past', 'Enemies', mb(['esqueleto/esqueleto.wav', 'esqueleto/esqueleto 2.wav', 'criaturas/403435-Fantasy_Orc_-Heeeh_-Soft_Snort_-07_-Humanoid_Monster_Voice_006536_.wav']), { vol: 0.9, dur: 2, range: 5000, cd: 0.1 });
S('death_future', 'Enemies', sc(['corto circuito/corto circuito 4.wav', 'corto circuito/corto circuito 5.wav', 'corto circuito que se va apagando.wav']), { vol: 0.8, dur: 2, range: 5000, cd: 0.1 });
S('death_boss', 'Enemies', mb(['stone/explosion de piedras.wav', 'explosion/511979-8_Bit_Large_Destroyed_02_-GAMEVideo-8_bit-impact-hit-damaged-destroyed.wav']), { vol: 1, dur: 3, range: 0, cd: 1 });
S('enemy_attack_past', 'Enemies', mb(['criaturas/527897-Monster_Breath_03.wav', 'criaturas/403435-Fantasy_Orc_-Heeeh_-Soft_Snort_-07_-Humanoid_Monster_Voice_006536_.wav']), { vol: 0.8, dur: 1.5, range: 4500, cd: 0.25 });
S('enemy_attack_future', 'Enemies', sc(['criaturas/Robots/Robots 01.wav', 'criaturas/Robots/Robots 02.wav', 'criaturas/Robots/Robots 03.wav', 'criaturas/Robots/Robots 04.wav']), { vol: 0.6, dur: 1.2, range: 4500, cd: 0.25 });
S('enemy_alert_past', 'Enemies', mb(['criaturas/403435-Fantasy_Orc_-Heeeh_-Soft_Snort_-07_-Humanoid_Monster_Voice_006536_.wav']), { vol: 0.7, dur: 1.5, range: 4500, cd: 0.4 });
S('enemy_alert_future', 'Enemies', sc(['criaturas/criatura tecnologica.wav', 'criaturas/criatura tipo drone.wav', 'criaturas/Organic/AlienChitter_01.wav']), { vol: 0.6, dur: 1.5, range: 4500, cd: 0.4 });
S('boss_roar', 'Enemies', sc(['criaturas/criatura gruñido.wav', 'criaturas/sonido gutural de criatura.wav', 'criaturas/criatura tecnologica 2.wav']), { vol: 1, dur: 3, range: 9000, cd: 1 });

// ---- world and UI ----
S('ui_move', 'UI', sc(['digital bleeps/digital bleeps 1.wav', 'digital bleeps/digital bleeps 2.wav', 'digital bleeps/digital bleeps 3.wav']), { vol: 0.4, dur: 0.5, range: 0, cd: 0.03, pmin: 0.98, pmax: 1.02 });
S('ui_select', 'UI', sc(['digital bleeps/digital bleeps 5.wav']), { vol: 0.5, dur: 0.8, range: 0, pmin: 1, pmax: 1 });
S('ui_back', 'UI', sc(['digital bleeps/digital bleeps 6.wav']), { vol: 0.5, dur: 0.8, range: 0, pmin: 1, pmax: 1 });
S('pickup', 'World', mb(['ganar algo.wav']), { vol: 0.5, dur: 1.5, range: 0, pmin: 1, pmax: 1.15, cd: 0.1 });
S('chest_open', 'World', mb(['madera/abrir cofre.wav', 'madera/cofre 1.wav']), { vol: 0.9, dur: 2.5, range: 4000 });
S('potion', 'World', sc(['digital bleeps/digital bleeps 9.wav']), { vol: 0.6, dur: 1, range: 0 });
S('victory', 'World', mb(['ganar batalla.wav']), { vol: 0.8, dur: 4, range: 0, cd: 3 });
S('door_past', 'World', mb(['puerta de madera/puerta 1.wav', 'puerta de madera/puerta 2.wav', 'puerta de madera/puerta 3.wav']), { vol: 0.8, dur: 3, range: 4000 });
S('door_future', 'World', sc(['puerta automatica/puerta automatica.wav', 'puerta automatica/puerta automatica pesada.wav']), { vol: 0.8, dur: 3, range: 4000 });
S('portal', 'World', sc(['preparacion de robot.wav', 'mecanismo.wav']), { vol: 0.8, dur: 3, range: 0, cd: 1 });
S('torch_loop', 'World', mb(['madera/fogata o antorcha.wav']), { vol: 0.35, dur: 30, range: 1200, loop: true });
S('breakable', 'World', mb(['stone/esplosion en piedras.wav', 'stone/piedras cayendo.wav']), { vol: 0.8, dur: 2, range: 4000 });
// ---- ambience (2D loops, per map) ----
S('amb_future', 'Ambience', sc(['ambientación/ambiente con ventilacion.wav']), { vol: 0.35, dur: 30, range: 0, loop: true, stereo: true });
S('amb_arena', 'Ambience', sc(['ambientación/ambiente tenso.wav']), { vol: 0.3, dur: 15, range: 0, loop: true, stereo: true });

// ---- music (2D, looped, crossfaded by GBAudio) ----
const M = (key, file, vol, o = {}) => { D[key] = Object.assign({ cat: 'Music', src: [file], vol, pmin: 1, pmax: 1, range: 0, cd: 0, dur: 0, loop: true, stereo: true, music: true, abs: true }, o); };
M('music_menu', 'ABS:Glyphbound/SourceArt/Audio/Music_src/main_theme.mpeg', 0.8);
M('music_castle', 'ABS:Glyphbound/SourceArt/Audio/Music_src/main_theme.mpeg', 0.55);
M('music_future', 'MP3:tRgp4nnhYncK4Oyf6CwmB_music_generated.mp3', 0.5);
M('music_boss_past', 'MP3:24kp9fr4263ke1IrsJp6-_music_generated.mp3', 0.6);
M('music_boss_future', 'MP3:oWHG97rLWY-D5i7IN3K9M_music_generated.mp3', 0.6);
M('music_entity', 'MP3:Tentacle_Boss.mp3', 0.65);

// ---- conversion ----
const ff = a => cp.spawnSync('ffmpeg', ['-y', '-v', 'error', ...a], { stdio: 'inherit' });
const slug = s => path.basename(s).replace(/\.[^.]+$/, '').replace(/[^A-Za-z0-9]+/g, '_').replace(/^_|_$/g, '').slice(0, 40);
const list = [], json = { Sounds: {} };
for (const [key, d] of Object.entries(D)) {
  const assets = [];
  d.src.forEach((rel, i) => {
    const name = `${key}_${i + 1}`;
    const outDir = path.join(OUT, d.cat); fs.mkdirSync(outDir, { recursive: true });
    const out = path.join(outDir, name + '.wav');
    const src = rel.startsWith('ABS:') ? path.join(ROOT, rel.slice(4)) : rel.startsWith('MP3:') ? path.join(MUS, rel.slice(4)) : path.join(LIB, rel);
    if (!fs.existsSync(src)) { console.error('MISSING', src); return; }
    if (!fs.existsSync(out) || process.argv.includes('--force')) {
      const a = ['-i', src];
      if (d.dur) a.push('-t', String(d.dur), '-af', `afade=t=out:st=${Math.max(0, d.dur - 0.05)}:d=0.05`);
      a.push('-ar', '44100', '-ac', (d.stereo || d.range === 0 && d.cat !== 'Player' && d.cat !== 'Spells') ? '2' : '1', '-sample_fmt', 's16', out);
      ff(a);
    }
    list.push({ asset: `/Game/Glyphbound/Audio/${d.cat}/${name}`, file: out.replace(/\\/g, '/'), loop: !!d.loop, music: !!d.music, source: rel });
    assets.push(`/Game/Glyphbound/Audio/${d.cat}/${name}.${name}`);
  });
  json.Sounds[key] = { Files: assets, Volume: d.vol, PitchMin: d.pmin, PitchMax: d.pmax, Range: d.range, Cooldown: d.cd, Loop: !!d.loop, Music: !!d.music };
}
// Which music plays where (map name substring -> key); boss tracks are chosen by GBAudio from the fight.
json.Maps = { PastCastle: { Music: 'music_castle' }, FutureBase: { Music: 'music_future', Ambience: 'amb_future' }, EntityArena: { Music: 'music_entity', Ambience: 'amb_arena' } };
fs.writeFileSync(path.join(ROOT, 'Glyphbound', 'Content', 'Data', 'Audio.json'), JSON.stringify(json, null, '\t'));
fs.writeFileSync(path.join(__dirname, 'audio_import_list.json'), JSON.stringify(list, null, 1));
console.log('keys', Object.keys(D).length, 'files', list.length);
