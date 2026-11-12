// balance_model.js - reads Glyphbound/Content/Data/*.json and prints the balance tables.
// Usage (from the repo root):  node Tools/Balance/balance_model.js
// It changes nothing: it is the calculator behind the numbers in Design/GDD.md section 19.
// Every assumption of the model is in the ASSUME and BUILDS blocks below; everything else comes from the data.
'use strict';
const fs = require('fs');
const path = require('path');

const dataDir = path.join(__dirname, '..', '..', 'Glyphbound', 'Content', 'Data');
const load = (name) => JSON.parse(fs.readFileSync(path.join(dataDir, name), 'utf8'));

const player = load('Player.json');
const weapons = load('Weapons.json').Weapons;
const spells = load('Spells.json').Spells;
const enemies = load('Enemies.json').Enemies;
const skills = load('Skills.json').Skills;
const encounters = load('Encounters.json').Encounters;
const effects = load('StatusEffects.json').StatusEffects;
const items = load('Items.json').Items;
const game = load('Game.json').Values;
const qte = load('QTE.json').Stats;
const ps = player.Stats;

// ---- Model assumptions (not game data) ----
const ASSUME = {
	Accuracy: 0.7,        // share of shots that hit (default; per weapon below)
	WeaponAccuracy: { Rifle: 0.6, Shotgun: 0.75, Pistol: 0.75 }, // automatic fire and recoil miss more
	Uptime: 0.7,          // share of the game time spent attacking (the rest: dodging, switching, looking for targets)
	HeadShare: 0.25,      // share of the hits that land on the head / weak point
	QteScore: 0.75,       // typical QTE score; the game maps 0 -> Min and 1 -> Max in a straight line (Base = QTE off)
	QteRealSeconds: 2,    // real seconds a player spends in one QTE (the limit is QTE.TimeLimit)
	SpellTargets: 1.8,    // enemies an average spell hits in a wave (1 against a boss)
	AvoidMelee: 0.85,     // share of melee damage a moving player avoids (the player is much faster than most enemies)
	AvoidRanged: 0.6,     // share of enemy shots a moving player dodges
	WaveOverhead: 6,      // game seconds lost per sub-wave: spawn delay, enemies walking in, finding targets
	SkillPointBudget: 36, // points the player should own before the final boss
	Targets: { Wave: [60, 90], SubBoss: [40, 60], Valkyrie: [105, 135], Entity: [160, 200], Opening: [15, 40] } // real seconds
};

// Reference builds: what a player who mixes both trees owns at each point of the game (skill id -> rank).
const BUILDS = {
	Start: { points: 2, ranks: { Weapon_Damage: 2 } },
	Early: { points: 8, ranks: { Weapon_Damage: 3, Spell_FireballDamage: 3, Spell_LightningDamage: 2 } },
	Mid: { points: 16, ranks: { Weapon_Damage: 3, Weapon_Headshot: 2, Weapon_PistolHeadshot: 2, Weapon_FireRate: 1, Spell_FireballDamage: 3, Spell_LightningDamage: 3 } },
	ValkyriePast: { points: 24, ranks: { Weapon_Damage: 3, Weapon_FireRate: 3, Weapon_Headshot: 2, Weapon_PistolHeadshot: 2, Spell_FireballDamage: 3, Spell_LightningDamage: 3, Spell_ManaCost: 3 } },
	ValkyrieFuture: { points: 29, ranks: { Weapon_Damage: 3, Weapon_FireRate: 3, Weapon_Headshot: 3, Weapon_PistolHeadshot: 3, Spell_FireballDamage: 3, Spell_LightningDamage: 3, Spell_ManaCost: 3, Spell_FireballBlast: 2 } },
	Entity: { points: 34, sword: true, ranks: { Weapon_Damage: 3, Weapon_FireRate: 3, Weapon_ReloadSpeed: 3, Weapon_Headshot: 3, Weapon_PistolHeadshot: 3, Spell_FireballDamage: 3, Spell_LightningDamage: 3, Spell_ManaCost: 3, Spell_FireballBlast: 3, Spell_RuneRadius: 1 } },
	// The two extremes at the full budget, to see how far apart specialists land.
	GunSpecialist36: { points: 36, sword: true, ranks: { Weapon_Damage: 3, Weapon_FireRate: 3, Weapon_ReloadSpeed: 3, Weapon_Headshot: 3, Weapon_PistolHeadshot: 3, Weapon_PrecisionRecovery: 3, Weapon_EnchantBuildUp: 3, Weapon_ShotgunClose: 3, Weapon_Recoil: 3 } },
	PureMage36: { points: 36, sword: true, ranks: { Spell_FireballDamage: 3, Spell_FireballBlast: 3, Spell_LightningDamage: 3, Spell_ManaCost: 3, Spell_Cooldown: 3, Spell_FlamethrowerDamage: 3, Spell_RuneRadius: 3, Spell_OrbDuration: 3, Spell_FireWallDuration: 3, Spell_IceFloorSlow: 3 } }
};
// Which build an encounter is fought with (by id prefix, first match wins).
const STAGE = [['Opening', 'Start'], ['Test_', 'Mid'], ['Wave_Past_1', 'Early'], ['Wave_Future_1', 'Early'], ['Wave_Past_2', 'Mid'], ['Wave_Future_2', 'Mid'],
	['SubBoss', 'Mid'], ['Zone_', 'Mid'], ['Wave_', 'ValkyriePast'], ['Valkyrie_Past', 'ValkyriePast'], ['Valkyrie_Future', 'ValkyrieFuture'], ['Entity', 'Entity']];
const KIND = [['Opening', 'Opening'], ['SubBoss', 'SubBoss'], ['Valkyrie', 'Valkyrie'], ['Entity', 'Entity'], ['', 'Wave']];

const pad = (s, n) => String(s).padEnd(n);
const num = (v, d = 1) => (Math.round(v * Math.pow(10, d)) / Math.pow(10, d)).toString();
const line = (cols, widths) => console.log(cols.map((c, i) => pad(c, widths[i])).join(' '));
const title = (t) => console.log('\n=== ' + t + ' ===');
const skillById = {};
for (const s of skills) { skillById[s.Id] = s; }
const spellById = {};
for (const s of spells) { spellById[s.Id] = s; }
const enemyById = {};
for (const e of enemies) { enemyById[e.Id] = e; }

// ---- Modifiers: (base + adds) x product of multipliers; a perk's Multiply stacks in a straight line with its rank ----
function applyMods(base, keys, ranks) {
	let add = 0;
	let mult = 1;
	for (const id of Object.keys(ranks || {})) {
		const node = skillById[id];
		if (!node) { throw new Error('unknown skill ' + id); }
		const rank = Math.min(ranks[id], node.MaxRank);
		for (const m of node.ModifiersPerRank) {
			if (keys.indexOf(m.Stat) < 0) { continue; }
			if (m.Op === 'Add') { add += m.Value * rank; }
			else if (m.Op === 'Multiply') { mult *= 1 + (m.Value - 1) * rank; }
		}
	}
	return (base + add) * mult;
}
function buildCost(ranks) {
	return Object.keys(ranks).reduce((a, id) => a + skillById[id].Cost * ranks[id], 0);
}
const qteParam = (p) => p.ScaledBy === 'None' ? p.Base : p.Min + (p.Max - p.Min) * ASSUME.QteScore;

// ---- Weapon enchant at a typical QTE ----
const enchantSpell = spellById[player.EnchantSpell];
const enchantEffect = effects.find((f) => f.Id.indexOf('Enchant_') === 0);
const enchantBonus = enchantEffect ? enchantEffect.Modifiers[0].Value - 1 : 0;
function enchant(ranks) {
	if (!enchantSpell) { return { mult: 1, uptime: 0 }; }
	const p = enchantSpell.Params;
	const cooldown = applyMods(p.Cooldown.Base, ['Spell.All.Cooldown', 'Spell.WeaponEnchant.Cooldown'], ranks);
	const uptime = Math.min(1, qteParam(p.Duration) / cooldown);
	return { mult: 1 + enchantBonus * qteParam(p.EffectStrength), uptime, cooldown };
}

// ---- Weapons ----
function weaponRow(w, ranks, useEnchant) {
	const stat = (name) => applyMods(w.Stats[name] === undefined ? (name.indexOf('Mult') >= 0 ? 1 : 0) : w.Stats[name],
		['Weapon.All.' + name, 'Weapon.' + w.Slot + '.' + name, 'Weapon.' + w.Id + '.' + name], ranks);
	const pellets = w.Stats.Pellets || 1;
	const near = w.Stats.FalloffNearMult === undefined ? 1 : stat('FalloffNearMult');
	const far = w.Stats.FalloffFarMult === undefined ? 1 : stat('FalloffFarMult');
	const best = Math.max(near, far);
	const hit = stat('Damage') * pellets * best;
	const interval = stat('FireInterval');
	const burst = hit / interval;
	let sustained = burst;
	let rounds = Infinity;
	const mag = w.Stats.MagazineSize || 0;
	if (mag) {
		sustained = hit * mag / (mag * interval + stat('ReloadTime'));
		rounds = mag * (1 + (w.Stats.SpareMagazines || 0));
	}
	const head = stat('HeadshotMult') || 1;
	const accuracy = ASSUME.WeaponAccuracy[w.Id] || ASSUME.Accuracy;
	const headFactor = 1 + ASSUME.HeadShare * (head - 1);
	const en = useEnchant && w.Slot === 'Gun' ? enchant(ranks) : { mult: 1, uptime: 0 };
	const enchantFactor = 1 + en.uptime * (en.mult - 1);
	const perShot = hit * accuracy * headFactor * enchantFactor; // expected damage of one trigger pull
	const buildUp = Math.max(w.Stats.FireBuildUp || 0, w.Stats.LightningBuildUp || 0, w.Stats.IceBuildUp || 0);
	return {
		id: w.Id, slot: w.Slot, hit, burst, sustained, head, accuracy, mag, perShot,
		nearSustained: sustained * near / best, farSustained: sustained * far / best,
		expected: sustained * accuracy * ASSUME.Uptime * headFactor * enchantFactor,
		carried: rounds === Infinity ? Infinity : rounds * hit,          // paper damage carried
		carriedExpected: rounds === Infinity ? Infinity : rounds * perShot, // damage it really deals before running dry
		magExpected: mag * perShot,
		buildUpPerSecond: buildUp / interval
	};
}
const baseRows = weapons.map((w) => weaponRow(w, {}, false));

title('Weapons (at their best range, no perks, no enchant)');
let W = [18, 8, 8, 10, 12, 10, 9, 6, 9];
line(['Weapon', 'Hit', 'Burst', 'Sustained', 'Near / far', 'Expected', 'Carried', 'Head', 'BuildUp/s'], W);
for (const r of baseRows) {
	line([r.id, num(r.hit), num(r.burst), num(r.sustained), num(r.nearSustained, 0) + ' / ' + num(r.farSustained, 0), num(r.expected), r.carried === Infinity ? 'free' : num(r.carried, 0), 'x' + r.head, num(r.buildUpPerSecond)], W);
}
console.log('Hit, Burst, Sustained: every shot hitting the body. Sustained includes reloads. Expected: with accuracy, uptime and head share.');

// ---- Spells ----
title('Spells (damage on ONE target; Min / Base / Max by QTE; damage per mana at Base)');
W = [16, 6, 9, 22, 10, 22];
line(['Spell', 'Mana', 'Cooldown', 'Damage', 'Dmg/mana', 'Notes'], W);
const allRanks = {};
for (const s of skills) { allRanks[s.Id] = s.MaxRank; }
for (const sp of spells) {
	const p = sp.Params;
	if (sp.bPassive || !p.ManaCost) { continue; }
	const mana = p.ManaCost.Base;
	let dmg = null;
	let note = '';
	let cost = mana;
	if (p.Damage) {
		dmg = [p.Damage.Min, p.Damage.Base, p.Damage.Max];
	} else if (p.DamagePerSecond && p.DamagePerSecond.Base > 0 && p.ManaPerSecond) {
		const seconds = (ps.MaxMana - mana) / p.ManaPerSecond.Base;
		dmg = [p.DamagePerSecond.Min, p.DamagePerSecond.Base, p.DamagePerSecond.Max].map((d) => d * seconds);
		cost = ps.MaxMana;
		note = 'full mana bar, ' + num(seconds) + ' s';
	} else if (p.DamagePerSecond && p.DamagePerSecond.Base > 0 && p.Duration) {
		dmg = [p.DamagePerSecond.Min * p.Duration.Min, p.DamagePerSecond.Base * p.Duration.Base, p.DamagePerSecond.Max * p.Duration.Max];
		note = 'if the enemy stays inside';
	}
	if (p.Duration && p.Duration.Base > 0 && p.Cooldown.Base > 0 && sp.Id !== 'LightningRune') {
		// Worst case for "always up": every duration and cooldown perk at rank 3.
		const durKeys = ['Spell.' + sp.Id + '.Duration'];
		const maxDur = applyMods(p.Duration.Max, durKeys, allRanks);
		const typDur = applyMods(qteParam(p.Duration), durKeys, allRanks);
		const minCd = applyMods(p.Cooldown.Base, ['Spell.All.Cooldown', 'Spell.' + sp.Id + '.Cooldown'], allRanks);
		note += (note ? '; ' : '') + 'up ' + num(100 * p.Duration.Base / p.Cooldown.Base, 0) + '% of the time (every perk: ' + num(100 * typDur / minCd, 0) + '% typical QTE, ' + num(100 * maxDur / minCd, 0) + '% perfect)';
	}
	line([sp.Id, mana, p.Cooldown.Base, dmg ? dmg.map((d) => num(d, 0)).join(' / ') : '-', dmg ? num(dmg[1] / cost, 2) : '-', note], W);
}

// ---- Casting schedule: mana only comes back after ManaRegenDelay without spending ----
// A player who casts as often as the mana allows: one cast of cost C every C / regen + delay game seconds.
function casting(ranks, fightSeconds, targets) {
	const direct = ['Fireball', 'LightningBolt', 'LightningRune'].map((id) => spellById[id]).filter(Boolean);
	let cost = 0;
	let damage = 0;
	for (const sp of direct) {
		cost += applyMods(sp.Params.ManaCost.Base, ['Spell.All.ManaCost', 'Spell.' + sp.Id + '.ManaCost'], ranks);
		damage += applyMods(qteParam(sp.Params.Damage), ['Spell.All.Damage', 'Spell.' + sp.Id + '.Damage'], ranks);
	}
	cost /= direct.length;
	damage /= direct.length;
	const period = cost / ps.ManaRegen + ps.ManaRegenDelay;     // game seconds between casts once the bar is empty
	const castsPerSecond = 1 / period + (ps.MaxMana / cost) / Math.max(fightSeconds, 1); // plus the starting bar
	return { cost, damage, period, castsPerSecond, dps: damage * castsPerSecond * (targets || 1) };
}
// Real seconds each QTE adds: it runs in real time while the world crawls.
const qteExtra = ASSUME.QteRealSeconds * (1 - (qte['QTE.TimeDilation'] || 0.1));

title('Casting');
const baseCast = casting({}, 75, 1);
console.log('Mana ' + ps.MaxMana + ', +' + ps.ManaRegen + '/s after ' + ps.ManaRegenDelay + ' s without spending. Average direct spell: ' + num(baseCast.cost) + ' mana, ' + num(baseCast.damage) + ' damage at a typical QTE.');
console.log('Sustained: one cast every ' + num(baseCast.period) + ' game s; with the starting bar over a 75 s fight, about ' + num(baseCast.dps) + ' damage/s on one target (' + num(baseCast.dps * ASSUME.SpellTargets) + ' in a wave).');
console.log('Each QTE adds about ' + num(qteExtra) + ' real seconds on top of the game time.');

// ---- Builds ----
function evaluateBuild(name) {
	const b = BUILDS[name];
	const rows = weapons.map((w) => weaponRow(w, b.ranks, true));
	const guns = rows.filter((r) => r.slot === 'Gun');
	const gun = guns.reduce((a, r) => (r.expected > a.expected ? r : a), guns[0]);
	const free = rows.filter((r) => r.slot === 'Staff' || (b.sword && r.slot === 'Sword')).reduce((a, r) => (!a || r.expected > a.expected ? r : a), null);
	return { name, points: b.points, cost: buildCost(b.ranks), rows, gun, free, ranks: b.ranks };
}
const builds = {};
for (const name of Object.keys(BUILDS)) { builds[name] = evaluateBuild(name); }

title('Reference builds (best gun with its enchant, free weapon, spells on one target)');
W = [18, 8, 10, 12, 14, 18, 10];
line(['Build', 'Points', 'Best gun', 'Gun dmg/s', 'Ammo damage', 'Free weapon', 'Spells'], W);
for (const name of Object.keys(builds)) {
	const b = builds[name];
	if (b.cost > b.points) { console.log('  BUILD ' + name + ' costs ' + b.cost + ' points, more than its ' + b.points); }
	line([name, b.cost + '/' + b.points, b.gun.id, num(b.gun.expected, 0), num(b.gun.carriedExpected, 0), b.free.id.replace('Staff_', '') + ' ' + num(b.free.expected, 0), num(casting(b.ranks, 120, 1).dps, 0)], W);
}
console.log('Gun dmg/s is expected damage (accuracy, uptime, ' + num(ASSUME.HeadShare * 100, 0) + '% of hits on the head). Ammo damage: what the full load really deals before the gun runs dry.');

// ---- Fight simulation ----
// The gun fires until its ammo (carried + what the enemies drop) is gone, then the free weapon takes over. Spells run all the time.
function simulate(health, build, opts) {
	const cast = casting(build.ranks, opts.guessSeconds || 90, opts.targets || 1);
	const gunDps = build.gun.expected;
	const freeDps = build.free.expected;
	const ammo = build.gun.carriedExpected + (opts.dropMagazines || 0) * build.gun.magExpected;
	const gunSeconds = ammo / gunDps;
	let seconds;
	let ammoShare;
	if ((gunDps + cast.dps) * gunSeconds >= health) {
		seconds = health / (gunDps + cast.dps);
		ammoShare = (gunDps * seconds) / ammo;
	} else {
		seconds = gunSeconds + (health - (gunDps + cast.dps) * gunSeconds) / (freeDps + cast.dps);
		ammoShare = 1;
	}
	const real = seconds * (1 + qteExtra * cast.castsPerSecond) + (opts.subWaves || 0) * ASSUME.WaveOverhead;
	return { game: seconds + (opts.subWaves || 0) * ASSUME.WaveOverhead, real, ammoShare };
}

// ---- Enemies ----
const fighters = enemies.filter((e) => e.AttackMode !== 'None');
const cycle = (e) => (e.Stats.AttackInterval || 1) + (e.Stats.AttackWindup || 0); // windup, hit, then the interval
const enemyDps = (e) => (e.Stats.AttackDamage || 0) / cycle(e);
const baseGunDps = baseRows.filter((r) => r.slot === 'Gun').reduce((a, r) => Math.max(a, r.expected), 0);
title('Enemies (one attack every AttackWindup + AttackInterval)');
W = [22, 8, 7, 5, 7, 7, 19, 30];
line(['Enemy', 'Health', 'Speed', 'Hit', 'Cycle', 'Dmg/s', 'Hit = % of player', 'Seconds to kill: base gun / Mid build'], W);
for (const e of fighters) {
	const s = e.Stats;
	const share = 100 * s.AttackDamage / ps.MaxHealth;
	line([e.Id, s.MaxHealth, s.MoveSpeed, s.AttackDamage, num(cycle(e)), num(enemyDps(e)), num(share, 0) + '%' + (share >= 30 ? ' (strong hit)' : ''),
		num(s.MaxHealth / baseGunDps) + ' / ' + num(s.MaxHealth / (builds.Mid.gun.expected + casting(builds.Mid.ranks, 75, 1).dps))], W);
}

title('Hits to kill, body / head (no perks, best range)');
W = [22].concat(baseRows.map(() => 18));
line(['Enemy'].concat(baseRows.map((r) => r.id)), W);
for (const e of fighters.filter((x) => x.Stats.MaxHealth < 1000)) {
	const hp = e.Stats.MaxHealth;
	line([e.Id].concat(baseRows.map((r) => Math.ceil(hp / r.hit) + ' / ' + Math.ceil(hp / (r.hit * r.head)))), W);
}

// ---- Ammo economy ----
title('Ammo economy: rounds a kill gives back / rounds it costs (1 = the gun never runs dry). Base gun / Mid build');
const ammoItem = items.find((i) => i.Id === 'Ammo_Magazine');
const magsPerPickup = ammoItem && ammoItem.Stats ? ammoItem.Stats.SpareMagazines : 1;
const dropMags = (e) => (e.Drops || []).filter((d) => d.ItemId === 'Ammo_Magazine').reduce((a, d) => a + d.Count * d.Chance, 0) * magsPerPickup;
const baseGuns = baseRows.filter((r) => r.mag > 0);
W = [22].concat(baseGuns.map(() => 16));
line(['Enemy'].concat(baseGuns.map((g) => g.id)), W);
for (const e of fighters.filter((x) => x.Stats.MaxHealth < 4000)) {
	line([e.Id].concat(baseGuns.map((g) => {
		const withBuild = builds.Mid.rows.find((r) => r.id === g.id);
		// Whole shots: a kill never costs less than one trigger pull that hits.
		const ratio = (row) => dropMags(e) * row.mag / Math.max(1 / row.accuracy, e.Stats.MaxHealth / row.perShot);
		return num(ratio(g), 2) + ' / ' + num(ratio(withBuild), 2);
	})), W);
}

// ---- Elemental ----
title('Elemental per enemy: threshold, x build-up taken, x damage taken; Ignite as a share of its health');
const ignite = effects.find((f) => f.Id === 'Ignite');
const igniteTotal = ignite ? -ignite.HealthPerTick * (ignite.Duration / ignite.TickInterval) : 0;
for (const e of fighters) {
	const parts = ['Fire', 'Lightning', 'Ice'].map((el) => {
		const get = (k) => e.Stats[el + k] === undefined ? game['ElementalDefault.' + el + k] : e.Stats[el + k];
		return el + ' ' + get('BuildUpThreshold') + ' x' + get('BuildUpTaken') + ' x' + get('DamageTaken');
	});
	const mult = e.Stats.IgniteDamageMult === undefined ? game['ElementalDefault.IgniteDamageMult'] : e.Stats.IgniteDamageMult;
	console.log(pad(e.Id, 22) + pad(parts.join(' | '), 66) + ' Ignite ' + num(100 * igniteTotal * mult / e.Stats.MaxHealth, 1) + '%' + (e.Stats.SlowTaken !== undefined ? ', slow taken x' + e.Stats.SlowTaken : ''));
}

// ---- Encounters ----
title('Encounters: length with the build of that point of the game, and the pressure on the player');
W = [16, 3, 6, 7, 15, 7, 11, 6, 50];
line(['Encounter', 'SP', 'Kills', 'Health', 'Build', 'Game s', 'Real s', 'Ammo', 'Per sub-wave: kills, expected damage taken per second (seconds to die)'], W);
for (const enc of encounters) {
	const stage = STAGE.find(([prefix]) => enc.Id.indexOf(prefix) === 0);
	const kind = KIND.find(([prefix]) => enc.Id.indexOf(prefix) === 0)[1];
	const build = builds[stage ? stage[1] : 'Mid'];
	let kills = 0;
	let health = 0;
	let mags = 0;
	const parts = [];
	for (const wave of enc.Waves) {
		let wk = 0;
		let exp = 0;
		for (const sp of wave.Spawns) {
			const e = enemyById[sp.EnemyId];
			if (!e) { console.log('  UNKNOWN ENEMY ' + sp.EnemyId + ' in ' + enc.Id); continue; }
			wk += sp.Count;
			health += sp.Count * e.Stats.MaxHealth;
			mags += sp.Count * dropMags(e);
			exp += sp.Count * enemyDps(e) * (1 - (e.AttackMode === 'Ranged' ? ASSUME.AvoidRanged : ASSUME.AvoidMelee));
		}
		kills += wk;
		parts.push(wk + 'k ' + num(exp, 1) + ' (' + num(ps.MaxHealth / Math.max(exp, 0.01), 0) + ' s)');
	}
	const boss = kills <= 2;
	const sim = simulate(health, build, { targets: boss ? 1 : ASSUME.SpellTargets, dropMagazines: mags, subWaves: boss ? 0 : enc.Waves.length, guessSeconds: boss ? 120 : 75 });
	const target = ASSUME.Targets[kind];
	const flag = sim.real < target[0] ? ' SHORT' : sim.real > target[1] ? ' LONG' : '';
	line([enc.Id, enc.SkillPoints, kills, health, build.name, num(sim.game, 0), num(sim.real, 0) + flag, num(sim.ammoShare * 100, 0) + '%', parts.join(' | ')], W);
}
console.log('Real s = game seconds plus the QTEs. Targets (real s): waves 60-90, sub-bosses 40-60, Valkyries about 120, Entity about 180.');
console.log('Ammo = share of the ammo (carried + dropped) the fight uses; 100% = the gun ran dry and the free weapon finished.');

title('Bosses against every build (real seconds, share of the ammo used)');
W = [20].concat(Object.keys(builds).map(() => 16));
line(['Boss'].concat(Object.keys(builds)), W);
for (const id of ['Orc_Past_Brute', 'Orc_Future_Warlord', 'Valkyrie_Past', 'Valkyrie_Future', 'Entity']) {
	const hp = id === 'Entity' ? enemies.filter((e) => e.Id.indexOf('Entity') === 0).reduce((a, e) => a + e.Stats.MaxHealth, 0) : enemyById[id].Stats.MaxHealth;
	line([id].concat(Object.keys(builds).map((name) => {
		const sim = simulate(hp, builds[name], { targets: 1, guessSeconds: 120 });
		return num(sim.real, 0) + ' s, ' + num(sim.ammoShare * 100, 0) + '%';
	})), W);
}

// ---- Skill points ----
title('Skill points');
const treeCost = (tree) => skills.filter((s) => s.Tree === tree).reduce((a, s) => a + s.Cost * s.MaxRank, 0);
const total = treeCost('Weapon') + treeCost('Spell');
console.log('Weapon tree: ' + treeCost('Weapon') + ' points. Spell tree: ' + treeCost('Spell') + ' points. Both: ' + total + '.');
const real = encounters.filter((e) => e.Id.indexOf('Test_') !== 0);
const fromEncounters = real.reduce((a, e) => a + (e.SkillPoints || 0), 0);
const sum = player.StartingSkillPoints + fromEncounters;
console.log('Starting points: ' + player.StartingSkillPoints + '. Encounters (without Test_*) give: ' + fromEncounters + '. Sum: ' + sum + ' (budget ' + ASSUME.SkillPointBudget + ' = ' + num(100 * ASSUME.SkillPointBudget / total, 0) + '% of both trees)' + (sum === ASSUME.SkillPointBudget ? ' OK' : '  <-- OFF BUDGET'));
