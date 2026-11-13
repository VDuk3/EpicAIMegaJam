// Level plan source for Glyphbound (one site, two eras).
// This file is the source of truth for the layout numbers. Running it:
//   node Design/Levels/make_plans.js
// checks the layout and rewrites, next to itself:
//   world_layout.json   machine-readable copy (for the architect's build scripts)
//   plan_past.svg       top-down plan of the castle (past)
//   plan_future.svg     top-down plan of the base (future)
//   plan_overlay.svg    both eras on one sheet
//   section_side.svg    side sections where height matters
// Units: centimetres. World frame: +X = north, +Y = east, +Z = up. Origin = centre of the courtyard floor.
// Rectangles are [x0, x1, y0, y1]. ASCII only.

'use strict';
const fs = require('fs');
const path = require('path');
const OUT = __dirname;

// ------------------------------------------------------------------ meta
const meta = {
	title: 'Glyphbound world layout',
	version: '1.0 (2026-11-13, level planner)',
	units: 'cm',
	frame: '+X north, +Y east, +Z up; origin = centre of the courtyard floor, Z 0 = main floor of both eras',
	grids: { castle: 400, castleShell: 1600, base: 400, labKit: 350, labPilaster: 100 },
	reach: {
		note: 'From Player.json: JumpVelocity 800, GravityScale 1.6, MantleMaxReach 160, MantleMinHeight 50, step height 45 (engine default)',
		jumpApex: 204, stepUp: 45, mantleMin: 50,
		climbSingleMax: 364, climbSingleDesign: 320,
		climbDoubleMax: 568, climbDoubleDesign: 500,
		unclimbableMin: 650
	}
};

// ------------------------------------------------------------------ rooms
// eras: 'both' | 'past' | 'future'. futureRect = the part of the footprint that exists in the future, when smaller.
const rooms = [
	{ id: 'GATE', past: 'Gatehouse', future: 'Checkpoint', rect: [-4000, -2400, -800, 800], eras: 'both', ceilPast: 1500, ceilFuture: 600 },
	{ id: 'COURT', past: 'Courtyard', future: 'Yard', rect: [-2400, 2400, -2400, 2400], eras: 'both', ceilPast: 0, ceilFuture: 0 },
	{ id: 'HALL', past: 'Great Hall', future: 'Containment Hall', rect: [2400, 5600, -2400, 2400], eras: 'both', ceilPast: 1500, ceilFuture: 1200 },
	{ id: 'SEAL', past: 'Seal Room', future: 'Seal Vault', rect: [5600, 7200, -800, 800], eras: 'both', ceilPast: 1500, ceilFuture: 700 },
	{ id: 'EAST', past: 'Forge (Fire zone)', future: 'Generator Hall', rect: [-2400, 2400, 2400, 5600], eras: 'both', ceilPast: 1500, ceilFuture: 1200 },
	{ id: 'WEST', past: 'Stormworks (Lightning zone)', future: 'Server Hall', rect: [-2400, 2400, -5600, -2400], eras: 'both', ceilPast: 1500, ceilFuture: 1200 },
	{ id: 'NE', past: 'High Ward (Air zone)', future: 'Signal Corridor', rect: [2400, 5600, 2400, 5600], futureRect: [2400, 3100, 2400, 5600], eras: 'both', ceilPast: 0, ceilFuture: 700 },
	{ id: 'NW', past: 'Frozen Cistern (Ice zone)', future: 'Cryo Corridor', rect: [2400, 5600, -5600, -2400], futureRect: [2400, 3100, -5600, -2400], eras: 'both', ceilPast: 1500, ceilFuture: 700 },
	{ id: 'BREACH', past: '', future: 'Breach Corridor', rect: [-8050, -4000, -265, 265], eras: 'future', ceilFuture: 333 },
	{ id: 'ANTE', past: '', future: 'Antechamber', rect: [-8450, -8050, -1800, 1800], eras: 'future', ceilFuture: 600 },
	{ id: 'HANGAR', past: '', future: 'Hangar (Herald)', rect: [-13250, -8450, -2400, 2400], eras: 'future', ceilFuture: 2400 }
];

// ------------------------------------------------------------------ doors
// axis = the constant coordinate of the wall the door sits in ('X' = wall runs east-west at X = at; 'Y' = wall runs north-south at Y = at).
// pos = centre of the opening on the floor. w/h = clear opening.
// lock: list of barriers in the doorway; each one is an actor with a UGBEraComponent (RemovedByFlag). The door is passable when all are gone.
const F = {
	openingPast: 'Encounter.Opening_Past.Cleared', sealUsed: 'SealUsed', gotGun: 'GotFirstGun',
	fire: 'Encounter.Zone_Fire.Cleared', lightning: 'Encounter.Zone_Lightning.Cleared',
	ice: 'Encounter.Zone_Ice.Cleared', air: 'Encounter.Zone_Air.Cleared',
	yard: 'Encounter.Wave_Future_1.Cleared', gen: 'Encounter.Wave_Future_2.Cleared', server: 'Encounter.Wave_Future_3.Cleared',
	herald: 'Encounter.MiniBoss_Herald.Cleared', valkPast: 'Encounter.Valkyrie_Past.Cleared', valkFuture: 'Encounter.Valkyrie_Future.Cleared',
	finalPortal: 'FinalPortalOpen'
};
const CASTLE_DOOR = { w: 288, h: 492, piece: 'SM_Wall_Door_16x15' };
const BIG = { w: 850, h: 425, piece: 'HTB SM_Door_02_Side (frame) + SM_Door_02 (leaf)' };
const GATE = { w: 392, h: 303, piece: 'SciFi SM_MGate01 frame + GateL + GateR' };
function door(id, era, axis, at, along, kind, label, lock) {
	const pos = axis === 'X' ? [at, along] : [along, at];
	return { id, era, axis, at, along, pos, w: kind.w, h: kind.h, piece: kind.piece, label, lock: lock || [] };
}
const doors = [
	// past (castle)
	door('D0', 'past', 'X', -4000, 0, CASTLE_DOOR, 'Outer gate (shut for good: portcullis)', [{ kind: 'SM_Portcullis', removedBy: null }]),
	door('D1', 'past', 'X', -2400, 0, CASTLE_DOOR, 'Gatehouse -> Courtyard', []),
	door('D2', 'past', 'X', 2400, 0, CASTLE_DOOR, 'Courtyard -> Great Hall', [{ kind: 'rune ward', removedBy: F.openingPast }]),
	door('D3a', 'past', 'Y', 2400, -1600, CASTLE_DOOR, 'Courtyard -> Forge (south door)', [{ kind: 'rune ward', removedBy: F.sealUsed }]),
	door('D3b', 'past', 'Y', 2400, 1600, CASTLE_DOOR, 'Courtyard -> Forge (north door)', [{ kind: 'rune ward', removedBy: F.sealUsed }]),
	door('D4a', 'past', 'Y', -2400, -1600, CASTLE_DOOR, 'Courtyard -> Stormworks (south door)', [{ kind: 'rune ward', removedBy: F.sealUsed }]),
	door('D4b', 'past', 'Y', -2400, 1600, CASTLE_DOOR, 'Courtyard -> Stormworks (north door)', [{ kind: 'rune ward', removedBy: F.sealUsed }]),
	door('D5', 'past', 'X', 5600, 0, CASTLE_DOOR, 'Great Hall -> Seal Room', []),
	door('D6', 'past', 'Y', 2400, 3200, CASTLE_DOOR, 'Great Hall <- High Ward (barred from the Air side)', [{ kind: 'barred door (2 x SM_Castle_Door)', removedBy: F.air }]),
	door('D7', 'past', 'Y', -2400, 3200, CASTLE_DOOR, 'Great Hall <- Frozen Cistern (barred from the Ice side)', [{ kind: 'barred door (2 x SM_Castle_Door)', removedBy: F.ice }]),
	// future (base)
	door('G0', 'future', 'X', -4000, 0, GATE, 'Checkpoint -> Breach Corridor', [{ kind: 'blast gate', removedBy: F.gen }, { kind: 'blast gate (second lock)', removedBy: F.server }]),
	door('D1f', 'future', 'X', -2400, 0, BIG, 'Yard -> Checkpoint', [{ kind: 'door leaf', removedBy: F.yard }]),
	door('D2f', 'future', 'X', 2400, 0, BIG, 'Yard <-> Containment Hall (LOCKDOWN)', [{ kind: 'lockdown shutter', removedBy: F.herald }]),
	door('D3f', 'future', 'Y', 2400, 0, BIG, 'Generator Hall -> Yard', [{ kind: 'door leaf', removedBy: F.gen }]),
	door('D4f', 'future', 'Y', -2400, 0, BIG, 'Server Hall -> Yard', [{ kind: 'door leaf', removedBy: F.server }]),
	door('D5f', 'future', 'X', 5600, 0, GATE, 'Containment Hall <-> Seal Vault', []),
	door('D8f', 'future', 'X', 2400, 4800, GATE, 'Generator Hall -> Signal Corridor (cross-era unlock U1)', [{ kind: 'rune-sealed plug', removedBy: F.lightning }, { kind: 'door leaf', removedBy: F.gen }]),
	door('D9f', 'future', 'X', 2400, -4800, GATE, 'Server Hall -> Cryo Corridor (cross-era unlock U2)', [{ kind: 'rune-sealed plug', removedBy: F.fire }, { kind: 'door leaf', removedBy: F.server }]),
	door('B1', 'future', 'X', -8050, 0, { w: 530, h: 333, piece: 'open end of the corridor model' }, 'Breach Corridor -> Antechamber', []),
	door('H1', 'future', 'X', -8450, -1200, BIG, 'Antechamber -> Hangar (west door)', []),
	door('H2', 'future', 'X', -8450, 1200, BIG, 'Antechamber -> Hangar (east door)', [])
];

// ------------------------------------------------------------------ wall lines
// A wall line: constant coordinate "axis = at", running "from..to" on the other axis.
// Castle: built from SM_Wall_16x15 / SM_Wall_Door_16x15, 1600 each (see castleWallPieces below).
// Base: one box per stretch between door gaps (material MI_Scifi_Panels), plus HTB features.
const castleLines = [
	{ axis: 'X', at: -4000, from: -800, to: 800 },
	{ axis: 'X', at: -2400, from: -5600, to: 5600 },
	{ axis: 'X', at: 2400, from: -5600, to: 5600 },
	{ axis: 'X', at: 5600, from: -5600, to: 5600 },
	{ axis: 'X', at: 7200, from: -800, to: 800 },
	{ axis: 'Y', at: -5600, from: -2400, to: 5600 },
	{ axis: 'Y', at: -2400, from: -2400, to: 5600 },
	{ axis: 'Y', at: 2400, from: -2400, to: 5600 },
	{ axis: 'Y', at: 5600, from: -2400, to: 5600 },
	{ axis: 'Y', at: -800, from: -4000, to: -2400 },
	{ axis: 'Y', at: 800, from: -4000, to: -2400 },
	{ axis: 'Y', at: -800, from: 5600, to: 7200 },
	{ axis: 'Y', at: 800, from: 5600, to: 7200 }
];
const baseLines = castleLines.concat([
	{ axis: 'X', at: 3100, from: 2400, to: 5600, note: 'north wall of the Signal Corridor; everything north of it is sealed mass in the future' },
	{ axis: 'X', at: 3100, from: -5600, to: -2400, note: 'north wall of the Cryo Corridor' },
	{ axis: 'Y', at: -265, from: -8050, to: -4000, note: 'Breach Corridor (the imported model has its own walls)' },
	{ axis: 'Y', at: 265, from: -8050, to: -4000 },
	{ axis: 'X', at: -8050, from: -1800, to: 1800 },
	{ axis: 'Y', at: -1800, from: -8450, to: -8050 },
	{ axis: 'Y', at: 1800, from: -8450, to: -8050 },
	{ axis: 'X', at: -8450, from: -2400, to: 2400 },
	{ axis: 'X', at: -13250, from: -2400, to: 2400 },
	{ axis: 'Y', at: -2400, from: -13250, to: -8450 },
	{ axis: 'Y', at: 2400, from: -13250, to: -8450 }
]);

// Castle shell pieces. E-W run (axis X): origin (at, yStart, 0), yaw 0, the 125 cm thickness goes south (-X).
// N-S run (axis Y): origin (xStart, at, 0), yaw -90, thickness goes east (+Y). The door of a door piece is centred 799 from the origin.
function buildCastlePieces() {
	const pieces = [];
	for (const line of castleLines) {
		for (let s = line.from; s < line.to; s += 1600) {
			const mid = s + 800;
			const d = doors.find(dd => dd.era === 'past' && dd.axis === line.axis && dd.at === line.at && Math.abs(dd.along - mid) < 10);
			const origin = line.axis === 'X' ? [line.at, s, 0] : [s, line.at, 0];
			pieces.push({
				mesh: d ? 'SM_Wall_Door_16x15' : 'SM_Wall_16x15',
				origin, yaw: line.axis === 'X' ? 0 : -90,
				line: line.axis + '=' + line.at, span: [s, s + 1600], door: d ? d.id : null
			});
		}
	}
	return pieces;
}
const castleWallPieces = buildCastlePieces();

// ------------------------------------------------------------------ structures
// kind: block (solid box), stairs (wedge; asc = direction it climbs), cover, pillar, prop, feature.
// era: 'both' = one shared collision body with two looks (ARCHITECTURE.md convention), 'past', 'future'.
// z0/z1 = bottom / top. mesh = the kit piece(s) or BOX (engine cube, kit material).
const S = [];
function st(id, zone, era, kind, rect, z0, z1, mesh, note, extra) {
	S.push(Object.assign({ id, zone, era, kind, rect, z0, z1, mesh, note: note || '' }, extra || {}));
}
function sq(cx, cy, half) { return [cx - half, cx + half, cy - half, cy + half]; }
function run(cx, cy, len, thick, along) { // along = 'X' or 'Y' (the long axis)
	return along === 'X' ? [cx - len / 2, cx + len / 2, cy - thick / 2, cy + thick / 2] : [cx - thick / 2, cx + thick / 2, cy - len / 2, cy + len / 2];
}

// --- GATE
st('gate_keg_w', 'GATE', 'past', 'prop', sq(-3800, -600, 60), 0, 127, 'SM_Large_Keg', 'dressing');
st('gate_keg_e', 'GATE', 'past', 'prop', sq(-3800, 600, 60), 0, 127, 'SM_Large_Keg', 'dressing');
st('gate_console_w', 'GATE', 'future', 'cover', [-4000, -3820, -750, -350], 0, 240, 'HTB SM_Prop_03', 'guard console');
st('gate_console_e', 'GATE', 'future', 'cover', [-4000, -3820, 350, 750], 0, 240, 'HTB SM_Prop_03', 'guard console');

// --- COURT, past: Courtyard
st('court_stage', 'COURT', 'past', 'block', [-800, 800, 1800, 2400], 0, 210, 'BOX + 4x8 SM_CastleTower_Floor_A_01 on top', 'east stage between the two Forge doors');
st('court_stage_stairs', 'COURT', 'past', 'stairs', [-405, 405, 1500, 1800], 0, 210, 'SM_Stairs_4M', '', { asc: '+Y' });
st('court_perch', 'COURT', 'past', 'block', [-800, 800, -2400, -1800], 0, 410, 'BOX + SM_CastleTower_Floor_A_01 on top', 'west perch between the two Stormworks doors');
st('court_perch_stairs', 'COURT', 'past', 'stairs', [-405, 405, -1800, -1200], 0, 410, 'SM_Stairs_8M', '', { asc: '-Y' });
st('court_cover_a', 'COURT', 'past', 'cover', run(-1200, -600, 620, 70, 'Y'), 0, 203, '3 x SM_Short_Walls_Wall_2M + 2 x SM_Short_Walls_Post', 'low wall');
st('court_cover_b', 'COURT', 'past', 'cover', run(-1200, 600, 620, 70, 'Y'), 0, 203, '3 x SM_Short_Walls_Wall_2M + 2 x SM_Short_Walls_Post', 'low wall');
st('court_cover_c', 'COURT', 'past', 'cover', run(1000, -900, 620, 70, 'X'), 0, 203, '3 x SM_Short_Walls_Wall_2M + 2 x SM_Short_Walls_Post', 'low wall');
st('court_cover_d', 'COURT', 'past', 'cover', run(1000, 900, 620, 70, 'X'), 0, 203, '3 x SM_Short_Walls_Wall_2M + 2 x SM_Short_Walls_Post', 'low wall');
st('court_podium_ne', 'COURT', 'past', 'pillar', sq(1900, 1900, 110), 0, 507, 'SM_Podium', 'full-height cover; its top is meant for the double jump (bonus pickup)');
st('court_podium_nw', 'COURT', 'past', 'pillar', sq(1900, -1900, 110), 0, 507, 'SM_Podium', 'full-height cover');
st('court_podium_se', 'COURT', 'past', 'pillar', sq(-1900, 1900, 110), 0, 507, 'SM_Podium', 'full-height cover');
st('court_podium_sw', 'COURT', 'past', 'pillar', sq(-1900, -1900, 110), 0, 507, 'SM_Podium', 'full-height cover');
// --- COURT, future: Yard
st('yard_dock_w', 'COURT', 'future', 'block', [-1300, 1300, -1800, -1200], 0, 100, 'HTB SM_Hero_Floor_01', 'loading dock, 1 m high');
st('yard_dock_w_ramp_n', 'COURT', 'future', 'stairs', [1300, 1600, -1800, -1200], 0, 100, 'BOX wedge', 'ramp so the navmesh joins the dock', { asc: '-X' });
st('yard_dock_w_ramp_s', 'COURT', 'future', 'stairs', [-1600, -1300, -1800, -1200], 0, 100, 'BOX wedge', '', { asc: '+X' });
st('yard_dock_e', 'COURT', 'future', 'block', [-1300, 1300, 1200, 1800], 0, 100, 'HTB SM_Hero_Floor_01', 'loading dock, 1 m high');
st('yard_dock_e_ramp_n', 'COURT', 'future', 'stairs', [1300, 1600, 1200, 1800], 0, 100, 'BOX wedge', '', { asc: '-X' });
st('yard_dock_e_ramp_s', 'COURT', 'future', 'stairs', [-1600, -1300, 1200, 1800], 0, 100, 'BOX wedge', '', { asc: '+X' });
st('yard_container_nw', 'COURT', 'future', 'cover', [1575, 1825, -800, -200], 0, 260, 'BOX 250x600x260, MI_Scifi_Panels + HTB SM_Wall_Cargo face', 'container');
st('yard_container_ne', 'COURT', 'future', 'cover', [1575, 1825, 200, 800], 0, 260, 'BOX container', 'container');
st('yard_container_sw', 'COURT', 'future', 'cover', [-1825, -1575, -800, -200], 0, 260, 'BOX container', 'container');
st('yard_container_se', 'COURT', 'future', 'cover', [-1825, -1575, 200, 800], 0, 260, 'BOX container', 'container');
st('yard_cube_w1', 'COURT', 'future', 'cover', sq(-700, -1500, 110), 100, 320, 'HTB SM_Prop_HS', 'on the dock');
st('yard_cube_w2', 'COURT', 'future', 'cover', sq(700, -1500, 110), 100, 320, 'HTB SM_Prop_HS', 'on the dock');
st('yard_cube_e1', 'COURT', 'future', 'cover', sq(-700, 1500, 110), 100, 320, 'HTB SM_Prop_HS', 'on the dock');
st('yard_cube_e2', 'COURT', 'future', 'cover', sq(700, 1500, 110), 100, 320, 'HTB SM_Prop_HS', 'on the dock');

for (const c of [[2300, 2300], [2300, -2300], [-2300, 2300], [-2300, -2300]]) {
	st('yard_mast_' + (c[0] > 0 ? 'n' : 's') + (c[1] > 0 ? 'e' : 'w'), 'COURT', 'future', 'pillar', sq(c[0], c[1], 75), 0, 1200, '2 x HTB SM_Wall_03 stacked + flood light', 'floodlight mast');
}

// --- HALL (shared pillars and galleries)
for (const y of [-1200, 1200]) {
	for (const x of [2800, 3600, 4400, 5200]) {
		st('hall_pillar_' + x + '_' + (y < 0 ? 'w' : 'e'), 'HALL', 'both', 'pillar', sq(x, y, 75), 0, 1400,
			'past: SM_PillarStem_01 + SM_PillarTop_01; future: 2 x HTB SM_Wall_03 stacked', 'same spot in both eras');
	}
}
st('hall_gallery_w', 'HALL', 'both', 'block', [4000, 5600, -2400, -1800], 0, 210, 'past: BOX + floor tiles; future: BOX, MI_Scifi_Panels', 'west gallery');
st('hall_gallery_w_stairs', 'HALL', 'both', 'stairs', [4395, 5205, -1800, -1500], 0, 210, 'past: SM_Stairs_4M; future: BOX wedge', '', { asc: '-Y' });
st('hall_gallery_e', 'HALL', 'both', 'block', [4000, 5600, 1800, 2400], 0, 210, 'past: BOX + floor tiles; future: BOX, MI_Scifi_Panels', 'east gallery');
st('hall_gallery_e_stairs', 'HALL', 'both', 'stairs', [4395, 5205, 1500, 1800], 0, 210, 'past: SM_Stairs_4M; future: BOX wedge', '', { asc: '+Y' });
// past: the rune cage (4 pylons, each removed by its zone's flag)
st('hall_pylon_air', 'HALL', 'past', 'feature', sq(4600, 600, 110), 0, 507, 'SM_Podium + rune glow', 'Air pylon (north-east)', { removedBy: F.air });
st('hall_pylon_ice', 'HALL', 'past', 'feature', sq(4600, -600, 110), 0, 507, 'SM_Podium + rune glow', 'Ice pylon (north-west)', { removedBy: F.ice });
st('hall_pylon_fire', 'HALL', 'past', 'feature', sq(3400, 600, 110), 0, 507, 'SM_Podium + rune glow', 'Fire pylon (south-east)', { removedBy: F.fire });
st('hall_pylon_lightning', 'HALL', 'past', 'feature', sq(3400, -600, 110), 0, 507, 'SM_Podium + rune glow', 'Lightning pylon (south-west)', { removedBy: F.lightning });
st('hall_table_w', 'HALL', 'past', 'cover', [2750, 3050, -1870, -1730], 0, 99, 'SM_HallTable_01', 'low cover');
st('hall_table_e', 'HALL', 'past', 'cover', [2750, 3050, 1730, 1870], 0, 99, 'SM_HallTable_01', 'low cover');
st('hall_throne', 'HALL', 'past', 'prop', sq(4800, -2250, 60), 210, 495, 'SM_Throne', 'on the west gallery, facing east');
// future: the cryo tube and lab dressing
st('hall_cryo_tube', 'HALL', 'future', 'feature', sq(4000, 0, 210), 0, 455, 'SciFi SM_LabCapsule11_1_Capsule + _Glass, scale 1.3', 'the Valkyrie\'s cryogenic tube; stays as central cover');
st('hall_cube_w', 'HALL', 'future', 'cover', sq(3200, -1700, 110), 0, 228, 'HTB SM_Prop_HS', 'cover');
st('hall_cube_e', 'HALL', 'future', 'cover', sq(3200, 1700, 110), 0, 228, 'HTB SM_Prop_HS', 'cover');
for (const y of [-2050, -1600, 1600, 2050]) {
	st('hall_side_tube_' + (y < 0 ? 'w' : 'e') + Math.abs(y), 'HALL', 'future', 'prop', sq(2560, y, 125), 0, 328, 'SciFi SM_LabCapsule05_Capsule + _Glass', 'against the south wall');
}
st('hall_console_n1', 'HALL', 'future', 'prop', sq(4700, -350, 40), 0, 130, 'SciFi SM_ControlPanel01', 'cryo release console (interaction that starts Valkyrie_Future)');

// --- SEAL
st('seal_shelf_w', 'SEAL', 'past', 'prop', [6520, 6690, -760, -700], 0, 201, 'SM_Bookshelf_A_01', 'dressing');
st('seal_shelf_e', 'SEAL', 'past', 'prop', [6520, 6690, 700, 760], 0, 201, 'SM_Bookshelf_A_01', 'dressing');
st('seal_console_w', 'SEAL', 'future', 'prop', sq(6500, -600, 40), 0, 130, 'SciFi SM_ControlPanel01', 'dressing');
st('seal_console_e', 'SEAL', 'future', 'prop', sq(6500, 600, 40), 0, 130, 'SciFi SM_ControlPanel01', 'dressing');

// --- EAST and WEST wings (mirror in Y). side = +1 east, -1 west.
function wing(zone, side, pastName, futureName) {
	const m = y => side * y;
	const r = (x0, x1, ya, yb) => [x0, x1, Math.min(m(ya), m(yb)), Math.max(m(ya), m(yb))];
	const p = zone.toLowerCase();
	// shared galleries
	st(p + '_gallery_n', zone, 'both', 'block', r(1800, 2400, 2800, 4400), 0, 210, 'past: BOX + floor tiles; future: BOX, MI_Scifi_Panels', 'north gallery');
	st(p + '_gallery_n_stairs', zone, 'both', 'stairs', r(1500, 1800, 3195, 4005), 0, 210, 'past: SM_Stairs_4M; future: BOX wedge', '', { asc: '+X' });
	st(p + '_gallery_s', zone, 'both', 'block', r(-2400, -1800, 2800, 4400), 0, 210, 'past: BOX + floor tiles; future: BOX, MI_Scifi_Panels', 'south gallery');
	st(p + '_gallery_s_stairs', zone, 'both', 'stairs', r(-1800, -1500, 3195, 4005), 0, 210, 'past: SM_Stairs_4M; future: BOX wedge', '', { asc: '-X' });
	// centre piece
	if (side > 0) {
		st(p + '_hearth', zone, 'past', 'block', r(-300, 300, 3700, 4300), 0, 120, 'BOX + SM_ConicalCauldron_02 + SM_FirePit_01 on top', 'the great hearth');
	} else {
		st(p + '_orrery', zone, 'past', 'pillar', sq(0, m(4000), 110), 0, 507, 'SM_Podium + spark sprite', 'the storm rod');
	}
	st(p + '_core', zone, 'future', 'block', r(-300, 300, 3700, 4300), 0, 1200, 'BOX, emissive MI_Scifi_Panels', side > 0 ? 'generator core (hides the portal from the hall)' : 'server core');
	// cover: same centres in both eras
	const coverPast = side > 0 ? '3 x SM_Short_Walls_Wall_2M + 2 x SM_Short_Walls_Post' : '3 x SM_Bookshelf_A_01 back to back with 3 more';
	const hPast = side > 0 ? 203 : 201;
	const cov = [[-900, 3200, 'X'], [900, 3200, 'X'], [-1000, 4700, 'Y'], [1000, 4700, 'Y']];
	cov.forEach((c, i) => {
		st(p + '_cover_' + (i + 1), zone, 'past', 'cover', run(c[0], m(c[1]), side > 0 ? 620 : 510, side > 0 ? 70 : 120, c[2]), 0, hPast, coverPast, side > 0 ? 'low wall' : 'bookshelf row');
		if (side > 0) {
			st(p + '_cube_' + (i + 1), zone, 'future', 'cover', sq(c[0], m(c[1]), 110), 0, 228, 'HTB SM_Prop_HS', 'machine block');
		} else {
			st(p + '_rack_' + (i + 1), zone, 'future', 'cover', run(c[0], m(c[1]), 510, 120, c[2]), 0, 260, 'BOX 510x120x260, emissive MI_Scifi_Panels', 'server rack row (where the bookshelves were)');
		}
	});
	// apse pillars beside the portal
	st(p + '_apse_n', zone, 'past', 'pillar', sq(500, m(5490), 110), 0, 507, 'SM_Podium', 'frames the portal, against the end wall');
	st(p + '_apse_s', zone, 'past', 'pillar', sq(-500, m(5490), 110), 0, 507, 'SM_Podium', 'frames the portal, against the end wall');
}
wing('EAST', 1);
wing('WEST', -1);
st('east_anvil_1', 'EAST', 'past', 'prop', sq(-500, 3500, 40), 0, 40, 'SM_Anvil', 'dressing');
st('east_anvil_2', 'EAST', 'past', 'prop', sq(500, 3500, 40), 0, 40, 'SM_Anvil', 'dressing');

// --- NE, past: High Ward (Air)
st('ne_terrace', 'NE', 'past', 'block', [3600, 5600, 2400, 5600], 0, 410, 'BOX; south face 8 x SM_CastleTower_Wall_A_01; top 5x8 SM_CastleTower_Floor_A_01 (or one tiled box)', 'the terrace, 4.1 m up');
st('ne_stairs', 'NE', 'past', 'stairs', [3000, 3600, 3595, 4405], 0, 410, 'SM_Stairs_8M', 'main way up (navmesh)', { asc: '+X' });
st('ne_step', 'NE', 'past', 'block', [3400, 3600, 5100, 5300], 0, 200, 'BOX', 'second way up for the player: hop on, then ledge climb');
st('ne_crenel_1', 'NE', 'past', 'cover', run(3650, 2700, 416, 85, 'Y'), 410, 641, 'SM_Crenel_A_01', 'parapet on the terrace edge');
st('ne_crenel_2', 'NE', 'past', 'cover', run(3650, 3116, 416, 85, 'Y'), 410, 641, 'SM_Crenel_A_01', 'parapet');
st('ne_crenel_3', 'NE', 'past', 'cover', run(3650, 4650, 416, 85, 'Y'), 410, 641, 'SM_Crenel_A_01', 'parapet');
st('ne_tower_1', 'NE', 'past', 'block', [4000, 4400, 2800, 3200], 410, 610, 'BOX', 'wind tower, 2 m above the terrace');
st('ne_tower_2', 'NE', 'past', 'block', [4800, 5200, 3800, 4200], 410, 610, 'BOX', 'wind tower, 2 m above the terrace');
st('ne_aerie', 'NE', 'past', 'block', [5200, 5600, 4800, 5600], 410, 700, 'BOX', 'the aerie, 2.9 m above the terrace: the Air chest');
st('ne_spire', 'NE', 'past', 'pillar', sq(4300, 5000, 110), 410, 860, 'SM_Podium, sunk 57 cm into the terrace', '4.5 m above the terrace: meant for the double jump (bonus)');
// --- NE, future: Signal Corridor
st('ne_sealed', 'NE', 'future', 'block', [3100, 5600, 2400, 5600], 0, 1200, 'no geometry needed behind the corridor wall', 'sealed mass (not reachable in the future)', { sealed: true });
st('ne_crate', 'NE', 'future', 'cover', sq(2980, 3900, 60), 0, 112, 'SciFi SM_LabBarrels01_1 x3', 'low cover against the north wall of the corridor');

// --- NW, past: Frozen Cistern (Ice)
for (const x of [3700, 4400]) {
	for (const y of [-5000, -4000, -3000]) {
		st('nw_pillar_' + x + '_' + (-y), 'NW', 'past', 'pillar', sq(x, y, 75), 0, 1400, 'SM_PillarStem_01 + SM_PillarTop_01', 'frozen pillar');
	}
}
st('nw_gallery', 'NW', 'past', 'block', [5000, 5600, -5600, -2400], 0, 210, 'BOX + floor tiles', 'north gallery: the Ice chest');
st('nw_gallery_stairs', 'NW', 'past', 'stairs', [4700, 5000, -4405, -3595], 0, 210, 'SM_Stairs_4M', '', { asc: '+X' });
st('nw_ice_1', 'NW', 'past', 'cover', [3950, 4150, -4700, -4300], 0, 200, 'BOX, ice tile', 'ice block');
st('nw_ice_2', 'NW', 'past', 'cover', [3950, 4150, -3700, -3300], 0, 200, 'BOX, ice tile', 'ice block');
// --- NW, future: Cryo Corridor
st('nw_sealed', 'NW', 'future', 'block', [3100, 5600, -5600, -2400], 0, 1200, 'no geometry needed behind the corridor wall', 'sealed mass (not reachable in the future)', { sealed: true });

// --- HANGAR (future only)
st('hangar_dock_w', 'HANGAR', 'future', 'block', [-12150, -9550, -2400, -1800], 0, 100, 'HTB SM_Hero_Floor_01', 'dock, 1 m high');
st('hangar_dock_w_ramp_n', 'HANGAR', 'future', 'stairs', [-9550, -9250, -2400, -1800], 0, 100, 'BOX wedge', '', { asc: '-X' });
st('hangar_dock_w_ramp_s', 'HANGAR', 'future', 'stairs', [-12450, -12150, -2400, -1800], 0, 100, 'BOX wedge', '', { asc: '+X' });
st('hangar_dock_e', 'HANGAR', 'future', 'block', [-12150, -9550, 1800, 2400], 0, 100, 'HTB SM_Hero_Floor_01', 'dock, 1 m high');
st('hangar_dock_e_ramp_n', 'HANGAR', 'future', 'stairs', [-9550, -9250, 1800, 2400], 0, 100, 'BOX wedge', '', { asc: '-X' });
st('hangar_dock_e_ramp_s', 'HANGAR', 'future', 'stairs', [-12450, -12150, 1800, 2400], 0, 100, 'BOX wedge', '', { asc: '+X' });
st('hangar_container_nw', 'HANGAR', 'future', 'cover', [-10300, -9100, -1450, -1150], 0, 300, 'BOX 1200x300x300 container', 'big cover');
st('hangar_container_ne', 'HANGAR', 'future', 'cover', [-10300, -9100, 1150, 1450], 0, 300, 'BOX container', 'big cover');
st('hangar_container_sw', 'HANGAR', 'future', 'cover', [-12600, -11400, -1450, -1150], 0, 300, 'BOX container', 'big cover');
st('hangar_container_se', 'HANGAR', 'future', 'cover', [-12600, -11400, 1150, 1450], 0, 300, 'BOX container', 'big cover');
st('hangar_catwalk', 'HANGAR', 'future', 'block', [-13250, -13050, -2090, 2090], 262, 282, '10 x HTB SM_Bridge_01 (deck at +282) + 4 x SM_Railing_02 on the wall side', 'gantry along the south wall: a deck box in the shell (the kit piece keeps no collision; its own hull is solid underneath)');
st('hangar_catwalk_ramp_w', 'HANGAR', 'future', 'stairs', [-13050, -12450, -2090, -1890], 0, 282, 'BOX wedge', 'ramp to the gantry', { asc: '-X' });
st('hangar_catwalk_ramp_e', 'HANGAR', 'future', 'stairs', [-13050, -12450, 1890, 2090], 0, 282, 'BOX wedge', 'ramp to the gantry', { asc: '-X' });
for (const y of [-1300, 1300]) {
	for (const x of [-11300, -10400]) {
		st('hangar_cube_' + (y < 0 ? 'w' : 'e') + '_' + (x === -11300 ? 's' : 'n'), 'HANGAR', 'future', 'cover', sq(x, y < 0 ? -2100 : 2100, 110), 100, 328, 'HTB SM_Prop_HS', 'on the dock');
	}
}

// ------------------------------------------------------------------ portals
// All portals stand on the main floor (Z 0), which is one shared slab in both eras.
// requiredFlag: the portal actor gets a UGBEraComponent (Presence Both, RequiredFlag).
const portals = [
	{ id: 'P1', name: 'Seal portal', pos: [6150, 0, 0], yaw: 0, zone: 'SEAL', requiredFlag: F.gotGun, note: 'the way back after the first gun' },
	{ id: 'P0', name: 'Hub portal', pos: [0, 0, 0], yaw: 0, zone: 'COURT', requiredFlag: F.yard, note: 'Courtyard <-> Yard, lit once the Yard wave is cleared' },
	{ id: 'P2', name: 'Forge portal', pos: [0, 5300, 0], yaw: 90, zone: 'EAST', requiredFlag: F.fire, note: 'Forge <-> Generator Hall' },
	{ id: 'P3', name: 'Stormworks portal', pos: [0, -5300, 0], yaw: -90, zone: 'WEST', requiredFlag: F.lightning, note: 'Stormworks <-> Server Hall' },
	{ id: 'P4', name: 'Cryo portal', pos: [2750, -3200, 0], yaw: -90, zone: 'NW', requiredFlag: null, note: 'Cryo Corridor <-> Frozen Cistern; only reachable from the future first' },
	{ id: 'P5', name: 'Signal portal', pos: [2750, 3200, 0], yaw: 90, zone: 'NE', requiredFlag: null, note: 'Signal Corridor <-> High Ward; only reachable from the future first' }
];
const special = [
	{ id: 'START', name: 'Player start', era: 'past', pos: [-3300, 0, 0], yaw: 0 },
	{ id: 'SEAL', name: 'The seal', era: 'past', pos: [6850, 0, 0], note: 'interact: cutscene 1, sets SealUsed, travel to the future in place' },
	{ id: 'FINAL', name: 'Final portal', era: 'future', pos: [6850, 0, 0], requiredFlag: F.finalPortal, note: 'opens on the seal when the two quest items are combined; leads to the Entity map' },
	{ id: 'CAGE', name: 'Rune cage (Valkyrie)', era: 'past', pos: [4000, 0, 0], note: 'interact when the four pylons are gone: starts Valkyrie_Past' },
	{ id: 'TUBE', name: 'Cryo tube console', era: 'future', pos: [4700, -350, 0], note: 'interact: starts Valkyrie_Future; needs Valkyrie_Past and MiniBoss_Herald cleared' }
];

// ------------------------------------------------------------------ encounters
// trigger: centre + half extents. spawns: world positions on the navmesh. start: 'trigger' or 'interact'.
const encounters = [
	{ id: 'Opening_Past', zone: 'COURT', era: 'past', start: 'trigger', exists: true, trigger: { c: [0, 0, 300], e: [1600, 1900, 400] },
		spawns: [[1700, -1300, 0], [1700, 1300, 0], [2000, -600, 0], [2000, 600, 0], [1200, -1900, 0], [1200, 1900, 0], [600, -1500, 0], [600, 1500, 0]],
		size: '2 sub-waves, 8 enemies (imps and orcs)', seconds: 20, ignorePortals: ['P0'] /* P0 only lights much later */ },
	{ id: 'Opening_Future', zone: 'HALL', era: 'future', start: 'trigger', exists: true, trigger: { c: [3900, 0, 300], e: [1400, 2300, 600] },
		spawns: [[2900, -400, 0], [2900, 400, 0], [3050, -2000, 0], [3050, 2000, 0], [3600, -2100, 0], [3600, 2100, 0]],
		size: '2 sub-waves, 8 enemies; the first one spawned (first spawn point) carries the pistol', seconds: 25 },
	{ id: 'Zone_Fire', zone: 'EAST', era: 'past', start: 'trigger', exists: true, trigger: { c: [0, 4000, 300], e: [2000, 1300, 600] },
		spawns: [[-1500, 5200, 0], [1500, 5200, 0], [-2000, 4750, 0], [2000, 4750, 0], [-600, 4650, 0], [600, 4650, 0]],
		size: '5 sub-waves, 35-45 enemies', seconds: 72 },
	{ id: 'Zone_Lightning', zone: 'WEST', era: 'past', start: 'trigger', exists: true, trigger: { c: [0, -4000, 300], e: [2000, 1300, 600] },
		spawns: [[-1500, -5200, 0], [1500, -5200, 0], [-2000, -4750, 0], [2000, -4750, 0], [-600, -4650, 0], [600, -4650, 0]],
		size: '5 sub-waves, 35-45 enemies', seconds: 72 },
	{ id: 'Zone_Ice', zone: 'NW', era: 'past', start: 'trigger', exists: true, trigger: { c: [4500, -4000, 300], e: [900, 1600, 600] },
		spawns: [[4550, -5300, 0], [4550, -2700, 0], [4000, -5300, 0], [4000, -2700, 0], [4500, -4600, 0], [4500, -3400, 0]],
		size: '5 sub-waves, 35-45 enemies', seconds: 72 },
	{ id: 'Zone_Air', zone: 'NE', era: 'past', start: 'trigger', exists: true, trigger: { c: [4600, 4000, 710], e: [1000, 1600, 400] },
		spawns: [[5300, 2800, 410], [5300, 3400, 410], [5000, 4500, 410], [4500, 5300, 410], [4000, 3600, 410], [4700, 2700, 410]],
		size: '5 sub-waves, 35-45 enemies (imps and shamans first)', seconds: 72 },
	{ id: 'Valkyrie_Past', zone: 'HALL', era: 'past', start: 'interact', exists: true, trigger: null,
		spawns: [[4000, 0, 0]], size: 'boss', seconds: 120, requires: [F.fire, F.lightning, F.ice, F.air] },
	{ id: 'Wave_Future_2', zone: 'EAST', era: 'future', start: 'trigger', exists: true, trigger: { c: [0, 3550, 300], e: [2000, 1050, 600] },
		spawns: [[-1400, 2750, 0], [1400, 2750, 0], [-700, 2700, 0], [700, 2700, 0], [-1200, 3800, 0], [1200, 3800, 0]],
		size: '5 sub-waves, 35-40 enemies', seconds: 67 },
	{ id: 'Wave_Future_3', zone: 'WEST', era: 'future', start: 'trigger', exists: true, trigger: { c: [0, -3550, 300], e: [2000, 1050, 600] },
		spawns: [[-1400, -2750, 0], [1400, -2750, 0], [-700, -2700, 0], [700, -2700, 0], [-1200, -3800, 0], [1200, -3800, 0]],
		size: '5 sub-waves, 35-40 enemies', seconds: 74 },
	{ id: 'Wave_Future_1', zone: 'COURT', era: 'future', start: 'trigger', exists: true, trigger: { c: [0, 0, 300], e: [1900, 1900, 600] },
		spawns: [[2100, -1700, 0], [2100, 1700, 0], [-2100, -1700, 0], [-2100, 1700, 0], [600, -2000, 0], [600, 2000, 0], [-600, -2000, 0], [-600, 2000, 0]],
		size: '5 sub-waves, 35-40 enemies', seconds: 60 },
	{ id: 'Skirmish_Signal', zone: 'NE', era: 'future', start: 'trigger', exists: false, trigger: { c: [2750, 4800, 200], e: [350, 300, 200] },
		spawns: [[2750, 4100, 0], [2600, 3600, 0], [2900, 3600, 0], [2750, 4400, 0]],
		size: 'NEW, corridor fight: 3 Drone_Future + 2 Orc_Future, one wave', seconds: 15 },
	{ id: 'Skirmish_Breach', zone: 'BREACH', era: 'future', start: 'trigger', exists: false, trigger: { c: [-4700, 0, 160], e: [300, 265, 170] },
		spawns: [[-7000, 0, 0], [-7200, -120, 0], [-7200, 120, 0], [-7600, 0, 0], [-7700, -130, 0], [-7700, 130, 0]],
		size: 'NEW, corridor fight: 4 Drone_Future + 2 Orc_Future, one wave', seconds: 20 },
	{ id: 'MiniBoss_Herald', zone: 'HANGAR', era: 'future', start: 'trigger', exists: false, trigger: { c: [-10850, 0, 500], e: [2000, 2000, 1000] },
		spawns: [[-10850, 0, 0]], size: 'NEW, mini-boss: the tentacle machine (actor class GBEntity), one wave', seconds: 75 },
	{ id: 'Valkyrie_Future', zone: 'HALL', era: 'future', start: 'interact', exists: true, trigger: null,
		spawns: [[3650, 0, 0]], size: 'boss', seconds: 120, requires: [F.valkPast, F.herald] }
];

// ------------------------------------------------------------------ chests, pickups, checkpoints
const chests = [
	{ id: 'chest_fire', zone: 'EAST', era: 'past', pos: [0, 4900, 0], requiredFlag: F.fire, contents: 'Fire knowledge (fire wall + flamethrower); Ember staff (provisional)' },
	{ id: 'chest_lightning', zone: 'WEST', era: 'past', pos: [0, -4900, 0], requiredFlag: F.lightning, contents: 'Lightning knowledge (rune trap + static ball)' },
	{ id: 'chest_ice', zone: 'NW', era: 'past', pos: [5300, -4000, 210], requiredFlag: F.ice, contents: 'Ice knowledge (ice shield + ice floor)' },
	{ id: 'chest_air', zone: 'NE', era: 'past', pos: [5400, 5200, 700], requiredFlag: F.air, contents: 'Air knowledge (double jump, improved dash, speed)' },
	{ id: 'crate_herald', zone: 'HANGAR', era: 'future', pos: [-12800, 0, 0], requiredFlag: F.herald, contents: 'Rifle (provisional) + 2 Ammo_Magazine' },
	{ id: 'crate_yard', zone: 'GATE', era: 'future', pos: [-3200, 600, 0], requiredFlag: null, contents: 'Shotgun (provisional) + 1 HealthInjection' }
];
const pickups = [
	{ id: 'pk_court_stage', era: 'past', pos: [0, 2100, 210], item: 'HealthPotion' },
	{ id: 'pk_court_perch', era: 'past', pos: [0, -2100, 410], item: 'ManaPotion' },
	{ id: 'pk_court_podium', era: 'past', pos: [1900, 1900, 507], item: 'ManaPotion x2 (meant for the double jump)' },
	{ id: 'pk_hall_gallery_w', era: 'past', pos: [5200, -2100, 210], item: 'ManaPotion' },
	{ id: 'pk_hall_gallery_e', era: 'past', pos: [5200, 2100, 210], item: 'HealthPotion' },
	{ id: 'pk_ne_tower', era: 'past', pos: [4200, 3000, 610], item: 'ManaPotion' },
	{ id: 'pk_ne_spire', era: 'past', pos: [4300, 5000, 860], item: 'HealthPotion x2 (meant for the double jump)' },
	{ id: 'pk_nw_gallery', era: 'past', pos: [5300, -2700, 210], item: 'HealthPotion' },
	{ id: 'pk_east_gallery', era: 'past', pos: [2100, 3000, 210], item: 'ManaPotion' },
	{ id: 'pk_west_gallery', era: 'past', pos: [2100, -3000, 210], item: 'ManaPotion' },
	{ id: 'pk_hall_f_w', era: 'future', pos: [5200, -2100, 210], item: 'Ammo_Magazine' },
	{ id: 'pk_hall_f_e', era: 'future', pos: [5200, 2100, 210], item: 'Ammo_Magazine' },
	{ id: 'pk_yard_dock_w', era: 'future', pos: [0, -1500, 100], item: 'Ammo_Magazine' },
	{ id: 'pk_yard_dock_e', era: 'future', pos: [0, 1500, 100], item: 'Ammo_Magazine' },
	{ id: 'pk_gate_ammo', era: 'future', pos: [-3000, -500, 0], item: 'Ammo_Magazine x2' },
	{ id: 'pk_east_f_gallery', era: 'future', pos: [-2100, 3600, 210], item: 'Ammo_Magazine' },
	{ id: 'pk_east_f_gallery2', era: 'future', pos: [2100, 3600, 210], item: 'HealthInjection' },
	{ id: 'pk_west_f_gallery', era: 'future', pos: [-2100, -3600, 210], item: 'Ammo_Magazine' },
	{ id: 'pk_west_f_gallery2', era: 'future', pos: [2100, -3600, 210], item: 'HealthInjection' },
	{ id: 'pk_signal_end', era: 'future', pos: [2750, 5400, 0], item: 'Ammo_Magazine + HealthInjection' },
	{ id: 'pk_cryo_end', era: 'future', pos: [2750, -5400, 0], item: 'Ammo_Magazine + Grenade (provisional)' },
	{ id: 'pk_ante', era: 'future', pos: [-8250, -1500, 0], item: 'Ammo_Magazine x2 + HealthInjection (before the Herald)' },
	{ id: 'pk_hangar_dock_w', era: 'future', pos: [-10850, -2100, 100], item: 'Ammo_Magazine' },
	{ id: 'pk_hangar_dock_e', era: 'future', pos: [-10850, 2100, 100], item: 'Ammo_Magazine' },
	{ id: 'pk_hangar_catwalk', era: 'future', pos: [-13150, 0, 282], item: 'HealthInjection' }
];
const checkpoints = [
	{ id: 'cp_start', era: 'past', pos: [-3300, 0, 0], extent: [300, 300, 200] },
	{ id: 'cp_hall', era: 'past', pos: [2750, 0, 0], extent: [200, 300, 200] },
	{ id: 'cp_seal', era: 'past', pos: [5900, 0, 0], extent: [200, 300, 200] },
	{ id: 'cp_forge_door', era: 'past', pos: [1600, 2150, 0], extent: [250, 200, 200] },
	{ id: 'cp_storm_door', era: 'past', pos: [1600, -2150, 0], extent: [250, 200, 200] },
	{ id: 'cp_checkpoint', era: 'future', pos: [-3300, 0, 0], extent: [300, 300, 200] },
	{ id: 'cp_ante', era: 'future', pos: [-8250, 0, 0], extent: [150, 300, 200] }
];

// ------------------------------------------------------------------ critical path (recommended order; the wings can be swapped)
const stations = [
	{ n: 1, era: 'past', pos: [-3300, 0], text: 'Start in the gatehouse' },
	{ n: 2, era: 'past', pos: [300, -250], text: 'Opening_Past in the courtyard' },
	{ n: 3, era: 'past', pos: [6850, 0], text: 'The seal: cutscene 1, thrown to the future' },
	{ n: 4, era: 'future', pos: [3300, -350], text: 'Opening_Future, first gun, cutscene 2' },
	{ n: 5, era: 'future', pos: [6150, 0], text: 'Portal P1 back to the castle' },
	{ n: 6, era: 'past', pos: [600, 3900], text: 'Zone_Fire in the Forge, chest' },
	{ n: 7, era: 'past', pos: [0, 5300], text: 'Portal P2' },
	{ n: 8, era: 'future', pos: [700, 3300], text: 'Wave_Future_2 in the Generator Hall' },
	{ n: 9, era: 'future', pos: [0, 0], text: 'Wave_Future_1 in the Yard, portal P0' },
	{ n: 10, era: 'past', pos: [600, -3900], text: 'Zone_Lightning in the Stormworks, chest' },
	{ n: 11, era: 'past', pos: [0, -5300], text: 'Portal P3' },
	{ n: 12, era: 'future', pos: [700, -3300], text: 'Wave_Future_3 in the Server Hall' },
	{ n: 13, era: 'future', pos: [-10850, 0], text: 'Breach skirmish, then the Herald in the Hangar' },
	{ n: 14, era: 'future', pos: [2750, -3200], text: 'Cryo Corridor (door opened by Fire), portal P4' },
	{ n: 15, era: 'past', pos: [4300, -4000], text: 'Zone_Ice in the Frozen Cistern, chest, door D7' },
	{ n: 16, era: 'future', pos: [2750, 3200], text: 'Signal Corridor (door opened by Lightning), skirmish, portal P5' },
	{ n: 17, era: 'past', pos: [4600, 4000], text: 'Zone_Air on the High Ward, chest, door D6' },
	{ n: 18, era: 'past', pos: [4000, 0], text: 'Valkyrie_Past in the Great Hall' },
	{ n: 19, era: 'past', pos: [6150, 0], text: 'Portal P1 to the future' },
	{ n: 20, era: 'future', pos: [4000, -450], text: 'Valkyrie_Future in the Containment Hall' },
	{ n: 21, era: 'future', pos: [6850, 0], text: 'Final portal on the seal' }
];
// walking legs, drawn as lines (each inside one era)
const legs = [
	{ era: 'past', pts: [[-3300, 0], [-2400, 0], [300, -250], [2400, 0], [3000, 0], [3400, -920], [4600, -920], [5100, 0], [5600, 0], [6850, 0]] },
	{ era: 'future', pts: [[6850, 0], [5600, 0], [5000, -350], [3300, -350], [5000, 250], [5600, 0], [6150, 0]] },
	{ era: 'past', pts: [[6150, 0], [5600, 0], [5100, 0], [4600, 920], [3400, 920], [3000, 0], [2400, 0], [1900, 1300], [1600, 2400], [1500, 2900], [600, 3900], [0, 4900], [0, 5300]] },
	{ era: 'future', pts: [[0, 5300], [600, 4700], [700, 3300], [0, 2400], [0, 0]] },
	{ era: 'past', pts: [[0, 0], [1300, -1500], [1600, -2400], [1500, -2900], [600, -3900], [0, -4900], [0, -5300]] },
	{ era: 'future', pts: [[0, -5300], [600, -4700], [700, -3300], [0, -2400], [-900, -400], [-2400, 0], [-4000, 0], [-8050, 0], [-8250, 600], [-8450, 1200], [-10850, 0]] },
	{ era: 'future', pts: [[-10850, 0], [-8450, -1200], [-8250, -600], [-8050, 0], [-4000, 0], [-2400, 0], [-500, -700], [0, -2400], [1300, -3000], [2000, -4800], [2400, -4800], [2750, -4800], [2750, -3200]] },
	{ era: 'past', pts: [[2750, -3200], [3300, -4000], [4300, -4000], [4850, -4000], [5300, -4000], [4850, -3800], [3600, -3000], [3200, -2400], [3200, -1500], [3000, -500], [2400, 0], [0, 0]] },
	{ era: 'future', pts: [[0, 0], [0, 2400], [1300, 3000], [2000, 4800], [2400, 4800], [2750, 4800], [2750, 3200]] },
	{ era: 'past', pts: [[2750, 3200], [2800, 4000], [3600, 4000], [4600, 4000], [5400, 5200], [4600, 4400], [3600, 4000], [2900, 3600], [3200, 2400], [3200, 1500], [4000, 0], [5100, 0], [5600, 0], [6150, 0]] },
	{ era: 'future', pts: [[6150, 0], [5600, 0], [4700, -350], [4000, -450], [5600, 0], [6850, 0]] }
];

// ------------------------------------------------------------------ checks
const problems = [];
function inRect(p, r, margin) { margin = margin || 0; return p[0] >= r[0] - margin && p[0] <= r[1] + margin && p[1] >= r[2] - margin && p[1] <= r[3] + margin; }
function roomOf(id) { return rooms.find(r => r.id === id); }
function structuresAt(era) { return S.filter(s => s.era === 'both' || s.era === era); }
function accessibleRect(room, era) { return era === 'future' && room.futureRect ? room.futureRect : room.rect; }
function topAt(p, era) { // highest walkable top under a point (0 = floor)
	let z = 0;
	for (const s of structuresAt(era)) { if (!s.sealed && inRect(p, s.rect) && (s.kind === 'block' || s.kind === 'stairs')) z = Math.max(z, s.z1); }
	return z;
}
for (const e of encounters) {
	const room = roomOf(e.zone);
	const rr = accessibleRect(room, e.era);
	for (const sp of e.spawns) {
		if (!inRect(sp, rr, -100)) problems.push(e.id + ': spawn ' + sp + ' is outside ' + e.zone + ' or within 1 m of its walls');
		for (const s of structuresAt(e.era)) {
			if (s.kind === 'stairs' && inRect(sp, s.rect, 60)) problems.push(e.id + ': spawn ' + sp + ' is on stairs ' + s.id);
			const solid = s.kind !== 'stairs' && (s.z0 <= sp[2] + 1) && (s.z1 > sp[2] + 1);
			if (solid && inRect(sp, s.rect, 100) && !(s.removedBy && e.start === 'interact')) problems.push(e.id + ': spawn ' + sp + ' is inside or within 1 m of ' + s.id);
		}
		const floor = topAt(sp, e.era);
		if (Math.abs(floor - sp[2]) > 1) problems.push(e.id + ': spawn ' + sp + ' has Z ' + sp[2] + ' but the floor there is at ' + floor);
	}
}
for (const p of portals.concat(special.filter(s => s.id === 'SEAL' || s.id === 'FINAL' || s.id === 'START'))) {
	for (const era of ['past', 'future']) {
		if (p.era && p.era !== era && p.id !== 'SEAL') continue;
		for (const s of structuresAt(era)) {
			if (inRect(p.pos, s.rect, 150)) problems.push(p.id + ': not clear in the ' + era + ' (structure ' + s.id + ' within 1.5 m)');
		}
		if (p.zone) {
			const rr = accessibleRect(roomOf(p.zone), era);
			if (!inRect(p.pos, rr, -200)) problems.push(p.id + ': outside the walkable part of ' + p.zone + ' in the ' + era);
		}
	}
}
for (const d of doors) {
	const half = d.w / 2;
	const front = d.axis === 'X' ? [d.at - 300, d.at + 300, d.along - half, d.along + half] : [d.along - half, d.along + half, d.at - 300, d.at + 300];
	for (const s of structuresAt(d.era)) {
		if (s.sealed) continue;
		const r = s.rect;
		const overlap = r[0] < front[1] && r[1] > front[0] && r[2] < front[3] && r[3] > front[2];
		if (overlap) problems.push(d.id + ': structure ' + s.id + ' is within 3 m of the doorway');
	}
}
for (const e of encounters) {
	if (!e.trigger) continue;
	const t = e.trigger; const tr = [t.c[0] - t.e[0], t.c[0] + t.e[0], t.c[1] - t.e[1], t.c[1] + t.e[1]];
	for (const p of portals) {
		if (p.zone === e.zone && inRect(p.pos, tr) && p.requiredFlag !== 'Encounter.' + e.id + '.Cleared' && !(e.ignorePortals || []).includes(p.id)) problems.push(e.id + ': trigger contains portal ' + p.id + ' (an arrival would start the fight at once)');
	}
}
// structures of one era must not overlap each other in plan and height (stairs may touch their block)
for (const era of ['past', 'future']) {
	const list = structuresAt(era).filter(s => !s.sealed);
	for (let i = 0; i < list.length; i++) {
		for (let j = i + 1; j < list.length; j++) {
			const a = list[i], b = list[j];
			const ov = a.rect[0] < b.rect[1] - 1 && a.rect[1] > b.rect[0] + 1 && a.rect[2] < b.rect[3] - 1 && a.rect[3] > b.rect[2] + 1 && a.z0 < b.z1 - 1 && a.z1 > b.z0 + 1;
			if (ov) problems.push(era + ': ' + a.id + ' overlaps ' + b.id);
		}
	}
}

// free-standing obstacles of one era keep at least 2 m between them (the widest enemy capsule is 1.2 m), or they touch
for (const era of ['past', 'future']) {
	const list = structuresAt(era).filter(s => !s.sealed && s.kind !== 'prop');
	for (let i = 0; i < list.length; i++) {
		for (let j = i + 1; j < list.length; j++) {
			const a = list[i], b = list[j];
			if (!(a.z0 < b.z1 - 1 && b.z0 < a.z1 - 1)) continue;
			const dx = Math.max(0, Math.max(a.rect[0], b.rect[0]) - Math.min(a.rect[1], b.rect[1]));
			const dy = Math.max(0, Math.max(a.rect[2], b.rect[2]) - Math.min(a.rect[3], b.rect[3]));
			const gap = Math.hypot(dx, dy);
			if (gap > 1 && gap < 200) problems.push(era + ': only ' + Math.round(gap) + ' cm between ' + a.id + ' and ' + b.id);
		}
	}
}

// free-standing obstacles on the main floor are at least 3 m from the room's walls, or they are built against one
for (const s of S) {
	if (s.sealed || s.kind === 'prop' || s.kind === 'stairs' || s.z0 > 0) continue;
	for (const era of (s.era === 'both' ? ['past', 'future'] : [s.era])) {
		const rr = accessibleRect(roomOf(s.zone), era);
		const d = [s.rect[0] - rr[0], rr[1] - s.rect[1], s.rect[2] - rr[2], rr[3] - s.rect[3]];
		for (const v of d) { if (v > 60 && v < 300) problems.push(era + ': ' + s.id + ' stands ' + Math.round(v) + ' cm from a wall of ' + s.zone + ' (3 m or against it)'); }
		if (Math.min.apply(null, d) < -1) problems.push(era + ': ' + s.id + ' is outside ' + s.zone);
	}
}

// open-sky rooms: nothing standable within 600 of the wall top (double jump 408 + ledge climb 160 = 568)
for (const s of S) {
	if (s.sealed || s.kind === 'prop') continue;
	const room = roomOf(s.zone);
	for (const era of (s.era === 'both' ? ['past', 'future'] : [s.era])) {
		const ceil = era === 'past' ? room.ceilPast : room.ceilFuture;
		const wallTop = era === 'past' ? 1500 : 1200;
		if (ceil === 0 && s.z1 > wallTop - 600 && s.z1 < wallTop) problems.push(era + ': ' + s.id + ' (top ' + s.z1 + ') is within 600 of the wall top in open-sky ' + s.zone);
	}
}

// ------------------------------------------------------------------ collision shell (greybox)
// Boxes that carry the collision of the site, so the kits can be placed as looks only (see WORLD_PLAN.md, 'Collision shell').
// wall: a box from z0 to z1, 'thick' wide, centred on 'centre' (the coordinate across the wall). lintel: the wall above a door opening.
// Future: 100 thick, centred on the line (these boxes are also the visible walls).
// Past: 125 thick and placed where the kit wall is: SM_Wall_16x15 reaches 8 cm to one side of its line and 117 to the other
// (south of an X line, east of a Y line), so the collision and the look agree.
function wallHeight(era, l) {
	if (era === 'past') return 1500;
	if (l.axis === 'Y' && Math.abs(l.at) === 265) return 333;
	if (l.at === -13250 || (l.axis === 'Y' && l.from === -13250) || l.at === -8450) return 2400;
	if (l.at === -8050 || (l.axis === 'Y' && l.from === -8450)) return 600;
	return 1200;
}
function buildShell(era) {
	const lines = era === 'past' ? castleLines : baseLines;
	const walls = [], lintels = [];
	for (const l of lines) {
		const h = wallHeight(era, l);
		const thick = era === 'past' ? 125 : 100;
		const centre = era === 'past' ? l.at + (l.axis === 'X' ? -54.5 : 54.5) : l.at;
		const ds = doors.filter(d => d.era === era && d.axis === l.axis && d.at === l.at && d.along > l.from && d.along < l.to).sort((a, b) => a.along - b.along);
		let cur = l.from;
		for (const d of ds) {
			if (d.along - d.w / 2 > cur) walls.push({ axis: l.axis, at: l.at, centre, from: cur, to: d.along - d.w / 2, z0: 0, z1: h, thick });
			if (d.h < h) lintels.push({ axis: l.axis, at: l.at, centre, from: d.along - d.w / 2, to: d.along + d.w / 2, z0: d.h, z1: h, thick, door: d.id });
			cur = d.along + d.w / 2;
		}
		if (cur < l.to) walls.push({ axis: l.axis, at: l.at, centre, from: cur, to: l.to, z0: 0, z1: h, thick });
	}
	return { walls, lintels };
}
const shell = {
	floor: { rect: [-13650, 7600, -6000, 6000], z0: -100, z1: 0, era: 'both', note: 'one slab under the whole site, in the persistent level: the footing never changes on era travel' },
	past: buildShell('past'), future: buildShell('future')
};

// ------------------------------------------------------------------ JSON
const layout = {
	meta, flags: F, rooms, doors, castleLines, baseLines, castleWallPieces, shell,
	structures: S, portals, special, encounters, chests, pickups, checkpoints, stations, legs
};
fs.writeFileSync(path.join(OUT, 'world_layout.json'), JSON.stringify(layout, null, '\t') + '\n');

// ------------------------------------------------------------------ SVG helpers
const XMIN = -13700, XMAX = 7700, YMIN = -6100, YMAX = 6100;
const SC = 0.08; // px per cm: 1 m = 8 px
const MX = 70, MY = 90, LEG = 330; // margins and legend width
const W = Math.round((YMAX - YMIN) * SC) + MX * 2 + LEG;
const H = Math.round((XMAX - XMIN) * SC) + MY * 2;
const sx = y => +(MX + (y - YMIN) * SC).toFixed(1);
const sy = x => +(MY + (XMAX - x) * SC).toFixed(1);
function esc(t) { return String(t).split('&').join('&amp;').split('<').join('&lt;').split('>').join('&gt;'); }
function rectEl(r, attrs) { return '<rect x="' + sx(r[2]) + '" y="' + sy(r[1]) + '" width="' + ((r[3] - r[2]) * SC).toFixed(1) + '" height="' + ((r[1] - r[0]) * SC).toFixed(1) + '" ' + attrs + '/>'; }
function lineEl(a, b, attrs) { return '<line x1="' + sx(a[1]) + '" y1="' + sy(a[0]) + '" x2="' + sx(b[1]) + '" y2="' + sy(b[0]) + '" ' + attrs + '/>'; }
function textEl(p, t, attrs) { return '<text x="' + sx(p[1]) + '" y="' + sy(p[0]) + '" ' + (attrs || '') + '>' + esc(t) + '</text>'; }
function wallSegments(lines, era) {
	const segs = [];
	for (const l of lines) {
		const gaps = doors.filter(d => d.era === era && d.axis === l.axis && d.at === l.at && d.along > l.from && d.along < l.to)
			.map(d => [d.along - d.w / 2, d.along + d.w / 2]).sort((a, b) => a[0] - b[0]);
		let cur = l.from;
		for (const g of gaps) { if (g[0] > cur) segs.push({ axis: l.axis, at: l.at, from: cur, to: g[0] }); cur = g[1]; }
		if (cur < l.to) segs.push({ axis: l.axis, at: l.at, from: cur, to: l.to });
	}
	return segs;
}
function segPts(s) { return s.axis === 'X' ? [[s.at, s.from], [s.at, s.to]] : [[s.from, s.at], [s.to, s.at]]; }

const PAL = {
	past: { bg: '#f4efe2', room: '#e6dcc3', open: '#d9e3c8', wall: '#4a3b2a', block: '#b9a682', stairs: '#cdbf9f', cover: '#7a6548', pillar: '#5d4b36', text: '#2b2118', grid: '#d8cfba', grid2: '#bfb499' },
	future: { bg: '#eef2f6', room: '#dbe3ec', open: '#cfdbe6', wall: '#1f2e3d', block: '#9db0c4', stairs: '#b9c8d8', cover: '#3f5873', pillar: '#2c4258', text: '#16222e', grid: '#d3dbe4', grid2: '#b3c0ce' }
};

function drawPlan(era, opts) {
	opts = opts || {};
	const P = PAL[era];
	const o = [];
	const title = era === 'past' ? 'PAST: the castle (pixel-art medieval)' : 'FUTURE: the military base (3D)';
	o.push('<svg xmlns="http://www.w3.org/2000/svg" width="' + W + '" height="' + H + '" viewBox="0 0 ' + W + ' ' + H + '" font-family="Verdana, Arial, sans-serif" font-size="11">');
	o.push('<rect width="' + W + '" height="' + H + '" fill="' + P.bg + '"/>');
	o.push('<text x="' + MX + '" y="34" font-size="20" font-weight="bold" fill="' + P.text + '">Glyphbound world plan. ' + esc(title) + '</text>');
	o.push('<text x="' + MX + '" y="54" fill="' + P.text + '">Top-down, north up. World frame: +X north, +Y east, cm. Origin = centre of the courtyard. Scale 1 m = 8 px. Thin grid 4 m, bold grid 16 m. Same scale and origin as the other era\'s plan.</text>');
	// grid
	for (let y = -6000; y <= 6000; y += 400) {
		const bold = y % 1600 === 0;
		o.push(lineEl([XMIN, y], [XMAX, y], 'stroke="' + (bold ? P.grid2 : P.grid) + '" stroke-width="' + (bold ? 0.9 : 0.4) + '"'));
		if (bold) o.push('<text x="' + sx(y) + '" y="' + (MY - 8) + '" text-anchor="middle" font-size="9" fill="' + P.text + '">Y ' + y + '</text>');
	}
	for (let x = -13600; x <= 7600; x += 400) {
		const bold = x % 1600 === 0;
		o.push(lineEl([x, YMIN], [x, YMAX], 'stroke="' + (bold ? P.grid2 : P.grid) + '" stroke-width="' + (bold ? 0.9 : 0.4) + '"'));
		if (bold) o.push('<text x="' + (MX - 6) + '" y="' + (sy(x) + 3) + '" text-anchor="end" font-size="9" fill="' + P.text + '">X ' + x + '</text>');
	}
	// rooms
	for (const r of rooms) {
		if (r.eras !== 'both' && r.eras !== era) {
			if (era === 'past') {
				o.push(rectEl(r.rect, 'fill="none" stroke="' + P.grid2 + '" stroke-width="1" stroke-dasharray="6 5"'));
			}
			continue;
		}
		const ceil = era === 'past' ? r.ceilPast : r.ceilFuture;
		const rr = accessibleRect(r, era);
		o.push(rectEl(rr, 'fill="' + (ceil === 0 ? P.open : P.room) + '" stroke="none"'));
	}
	if (era === 'past') {
		o.push(textEl([-10800, 0], 'Beyond the shut gate: nothing is built here in the past.', 'text-anchor="middle" font-size="11" fill="#8a7d63" font-style="italic"'));
		o.push(textEl([-10400, 0], '(future only: Breach Corridor, Antechamber, Hangar)', 'text-anchor="middle" font-size="10" fill="#8a7d63" font-style="italic"'));
	}
	// structures
	for (const s of structuresAt(era)) {
		if (s.sealed) {
			o.push(rectEl(s.rect, 'fill="url(#hatch)" stroke="' + P.wall + '" stroke-width="0.6"'));
			o.push(textEl([(s.rect[0] + s.rect[1]) / 2, (s.rect[2] + s.rect[3]) / 2], 'sealed mass', 'text-anchor="middle" font-size="10" fill="' + P.text + '" font-style="italic"'));
			continue;
		}
		let fill = P.block, stroke = P.wall, sw = 0.7, extra = '';
		if (s.kind === 'stairs') { fill = P.stairs; extra = ' stroke-dasharray="2 2"'; }
		if (s.kind === 'cover') { fill = P.cover; }
		if (s.kind === 'pillar') { fill = P.pillar; }
		if (s.kind === 'prop') { fill = 'none'; stroke = P.cover; }
		if (s.kind === 'feature') { fill = era === 'past' ? '#b56ad1' : '#39b8c9'; }
		o.push(rectEl(s.rect, 'fill="' + fill + '" stroke="' + stroke + '" stroke-width="' + sw + '"' + extra));
		const cx = (s.rect[0] + s.rect[1]) / 2, cy = (s.rect[2] + s.rect[3]) / 2;
		if (s.kind === 'stairs') {
			const dir = { '+X': [1, 0], '-X': [-1, 0], '+Y': [0, 1], '-Y': [0, -1] }[s.asc];
			const hx = (s.rect[1] - s.rect[0]) / 2 - 30, hy = (s.rect[3] - s.rect[2]) / 2 - 30;
			const a = [cx - dir[0] * hx, cy - dir[1] * hy], b = [cx + dir[0] * hx, cy + dir[1] * hy];
			o.push(lineEl(a, b, 'stroke="' + P.wall + '" stroke-width="1.2" marker-end="url(#arr)"'));
		}
		const big = (s.rect[1] - s.rect[0]) >= 390 && (s.rect[3] - s.rect[2]) >= 390;
		if (s.kind === 'block' && big) {
			o.push(textEl([cx - 40, cy], '+' + s.z1, 'text-anchor="middle" font-size="9" fill="' + P.text + '"'));
		}
	}
	// walls
	const lines = era === 'past' ? castleLines : baseLines;
	for (const sg of wallSegments(lines, era)) {
		const p = segPts(sg);
		o.push(lineEl(p[0], p[1], 'stroke="' + P.wall + '" stroke-width="' + (era === 'past' ? 5 : 4) + '" stroke-linecap="butt"'));
	}
	// doors
	for (const d of doors.filter(dd => dd.era === era)) {
		const half = d.w / 2;
		const a = d.axis === 'X' ? [d.at, d.along - half] : [d.along - half, d.at];
		const b = d.axis === 'X' ? [d.at, d.along + half] : [d.along + half, d.at];
		const locked = d.lock.length > 0;
		o.push(lineEl(a, b, 'stroke="' + (locked ? '#d9822b' : '#3aa655') + '" stroke-width="3"'));
		const lp = d.axis === 'X' ? [d.at + 130, d.along + half + 40] : [d.along + half + 60, d.at + 60];
		o.push(textEl(lp, d.id, 'font-size="10" font-weight="bold" fill="' + (locked ? '#a85a0c' : '#1f7a38') + '"'));
	}
	// room names
	for (const r of rooms) {
		if (r.eras !== 'both' && r.eras !== era) continue;
		const rr = accessibleRect(r, era);
		const name = era === 'past' ? r.past : r.future;
		const ceil = era === 'past' ? r.ceilPast : r.ceilFuture;
		const narrow = (rr[1] - rr[0]) < 900 || (rr[3] - rr[2]) < 900;
		const px = rr[1] - 170, py = rr[2] + 90;
		if (r.id === 'BREACH') {
			o.push(textEl([-5900, 420], name, 'font-size="11" font-weight="bold" fill="' + P.text + '"'));
			o.push(textEl([-6080, 420], '40.5 x 5.3 x 3.3 m, the imported corridor model', 'font-size="9" fill="' + P.text + '"'));
		} else if (r.id === 'ANTE') {
			o.push(textEl([rr[0] + 130, rr[3] + 80], name, 'font-size="10" font-weight="bold" fill="' + P.text + '"'));
		} else if (narrow && r.futureRect && era === 'future') {
			o.push(textEl([rr[1] + 110, rr[2] + 80], name + ' (the strip below this line)', 'font-size="10" font-weight="bold" fill="' + P.text + '"'));
		} else {
			o.push(textEl([px, py], name, 'font-size="12" font-weight="bold" fill="' + P.text + '"'));
			o.push(textEl([px - 160, py], r.id + (ceil === 0 ? ', open sky' : ', ceiling ' + ceil), 'font-size="9" fill="' + P.text + '"'));
		}
	}
	// encounter triggers and spawns
	for (const e of encounters.filter(en => en.era === era)) {
		if (e.trigger) {
			const t = e.trigger; const tr = [t.c[0] - t.e[0], t.c[0] + t.e[0], t.c[1] - t.e[1], t.c[1] + t.e[1]];
			o.push(rectEl(tr, 'fill="none" stroke="#c0392b" stroke-width="1" stroke-dasharray="7 4"'));
			o.push(textEl([tr[0] + 50, tr[2] + 40], e.id, 'font-size="10" fill="#c0392b" font-weight="bold"'));
		} else {
			o.push(textEl([e.spawns[0][0] - 330, e.spawns[0][1]], e.id + ' (by interaction)', 'font-size="10" fill="#c0392b" font-weight="bold" text-anchor="middle"'));
		}
		for (const sp of e.spawns) {
			const x = sx(sp[1]), y = sy(sp[0]);
			o.push('<path d="M' + (x - 4) + ' ' + (y - 4) + ' L' + (x + 4) + ' ' + (y + 4) + ' M' + (x - 4) + ' ' + (y + 4) + ' L' + (x + 4) + ' ' + (y - 4) + '" stroke="#c0392b" stroke-width="1.6"/>');
		}
	}
	// chests, pickups, checkpoints
	for (const c of chests.filter(ch => ch.era === era)) {
		o.push('<rect x="' + (sx(c.pos[1]) - 5) + '" y="' + (sy(c.pos[0]) - 4) + '" width="10" height="8" fill="#f1c40f" stroke="#7d6608" stroke-width="1"/>');
	}
	for (const k of pickups.filter(pk => pk.era === era)) {
		o.push('<circle cx="' + sx(k.pos[1]) + '" cy="' + sy(k.pos[0]) + '" r="3" fill="#27ae60" stroke="#145a32" stroke-width="0.8"/>');
	}
	for (const c of checkpoints.filter(cp => cp.era === era)) {
		const r = [c.pos[0] - c.extent[0], c.pos[0] + c.extent[0], c.pos[1] - c.extent[1], c.pos[1] + c.extent[1]];
		o.push(rectEl(r, 'fill="none" stroke="#2980b9" stroke-width="0.9" stroke-dasharray="2 3"'));
	}
	// critical path legs
	for (const l of legs.filter(lg => lg.era === era)) {
		const pts = l.pts.map(p => sx(p[1]) + ',' + sy(p[0])).join(' ');
		o.push('<polyline points="' + pts + '" fill="none" stroke="#e67e22" stroke-width="1.6" stroke-opacity="0.75" stroke-dasharray="1 0" marker-end="url(#arrO)"/>');
	}
	// portals and specials
	for (const p of portals) {
		const x = sx(p.pos[1]), y = sy(p.pos[0]);
		o.push('<path d="M' + x + ' ' + (y - 9) + ' L' + (x + 9) + ' ' + y + ' L' + x + ' ' + (y + 9) + ' L' + (x - 9) + ' ' + y + ' Z" fill="#00bcd4" stroke="#00566b" stroke-width="1.4"/>');
		o.push('<text x="' + (x + 11) + '" y="' + (y - 7) + '" font-size="11" font-weight="bold" fill="#00566b">' + p.id + '</text>');
	}
	for (const s of special.filter(sp => sp.era === era)) {
		const x = sx(s.pos[1]), y = sy(s.pos[0]);
		if (s.id === 'START') {
			o.push('<path d="M' + x + ' ' + (y - 9) + ' L' + (x + 7) + ' ' + (y + 7) + ' L' + (x - 7) + ' ' + (y + 7) + ' Z" fill="#2ecc71" stroke="#145a32" stroke-width="1.2"/>');
			o.push('<text x="' + (x + 10) + '" y="' + (y + 16) + '" font-size="10" font-weight="bold" fill="#145a32">START</text>');
		} else if (s.id === 'SEAL' || s.id === 'FINAL') {
			o.push('<circle cx="' + x + '" cy="' + y + '" r="9" fill="' + (s.id === 'SEAL' ? '#b56ad1' : '#e74c3c') + '" stroke="#3d1452" stroke-width="1.4"/>');
			o.push('<text x="' + (x + 12) + '" y="' + (y - 8) + '" font-size="10" font-weight="bold" fill="#3d1452">' + (s.id === 'SEAL' ? 'SEAL' : 'FINAL PORTAL') + '</text>');
		} else {
			o.push('<circle cx="' + x + '" cy="' + y + '" r="5" fill="none" stroke="#8e44ad" stroke-width="1.6"/>');
		}
	}
	// stations
	for (const s of stations.filter(stn => stn.era === era)) {
		const x = sx(s.pos[1]) - 16, y = sy(s.pos[0]) + 16;
		o.push('<circle cx="' + x + '" cy="' + y + '" r="8.5" fill="#e67e22" stroke="#7e3f05" stroke-width="1"/>');
		o.push('<text x="' + x + '" y="' + (y + 3.5) + '" text-anchor="middle" font-size="9.5" font-weight="bold" fill="#fff">' + s.n + '</text>');
	}
	// defs
	o.push('<defs><marker id="arr" markerWidth="6" markerHeight="6" refX="5" refY="3" orient="auto"><path d="M0 0 L6 3 L0 6 Z" fill="' + P.wall + '"/></marker>' +
		'<marker id="arrO" markerWidth="7" markerHeight="7" refX="6" refY="3.5" orient="auto"><path d="M0 0 L7 3.5 L0 7 Z" fill="#e67e22"/></marker>' +
		'<pattern id="hatch" width="8" height="8" patternUnits="userSpaceOnUse" patternTransform="rotate(45)"><rect width="8" height="8" fill="' + P.room + '"/><line x1="0" y1="0" x2="0" y2="8" stroke="' + P.grid2 + '" stroke-width="2"/></pattern></defs>');
	// scale bar and north arrow
	const bx = MX + 10, by = H - 40;
	o.push('<rect x="' + bx + '" y="' + by + '" width="' + (1000 * SC) + '" height="6" fill="' + P.wall + '"/><rect x="' + (bx + 1000 * SC) + '" y="' + by + '" width="' + (1000 * SC) + '" height="6" fill="#fff" stroke="' + P.wall + '"/><rect x="' + (bx + 2000 * SC) + '" y="' + by + '" width="' + (3000 * SC) + '" height="6" fill="' + P.wall + '"/>');
	o.push('<text x="' + bx + '" y="' + (by + 20) + '" font-size="10" fill="' + P.text + '">0</text><text x="' + (bx + 1000 * SC) + '" y="' + (by + 20) + '" font-size="10" text-anchor="middle" fill="' + P.text + '">10 m</text><text x="' + (bx + 2000 * SC) + '" y="' + (by + 20) + '" font-size="10" text-anchor="middle" fill="' + P.text + '">20 m</text><text x="' + (bx + 5000 * SC) + '" y="' + (by + 20) + '" font-size="10" text-anchor="middle" fill="' + P.text + '">50 m</text>');
	const nx = MX + 30, ny = MY + 60;
	o.push('<path d="M' + nx + ' ' + (ny - 26) + ' L' + (nx + 9) + ' ' + ny + ' L' + nx + ' ' + (ny - 7) + ' L' + (nx - 9) + ' ' + ny + ' Z" fill="' + P.wall + '"/><text x="' + nx + '" y="' + (ny - 30) + '" text-anchor="middle" font-size="12" font-weight="bold" fill="' + P.text + '">N (+X)</text>');
	// legend
	const lx = W - LEG + 10; let ly = MY + 10;
	const L = (t, bold) => { o.push('<text x="' + lx + '" y="' + ly + '" font-size="' + (bold ? 12 : 10) + '"' + (bold ? ' font-weight="bold"' : '') + ' fill="' + P.text + '">' + esc(t) + '</text>'); ly += bold ? 18 : 14; };
	L('Legend', true);
	const sw = (draw, t) => { o.push(draw(lx, ly - 9)); o.push('<text x="' + (lx + 26) + '" y="' + ly + '" font-size="10" fill="' + P.text + '">' + esc(t) + '</text>'); ly += 16; };
	sw((x, y) => '<rect x="' + x + '" y="' + y + '" width="18" height="10" fill="' + P.room + '" stroke="' + P.wall + '"/>', 'room with a ceiling');
	sw((x, y) => '<rect x="' + x + '" y="' + y + '" width="18" height="10" fill="' + P.open + '" stroke="' + P.wall + '"/>', 'open to the sky');
	sw((x, y) => '<line x1="' + x + '" y1="' + (y + 5) + '" x2="' + (x + 18) + '" y2="' + (y + 5) + '" stroke="' + P.wall + '" stroke-width="5"/>', 'wall (' + (era === 'past' ? '15 m, SM_Wall_16x15' : '12 m box, sci-fi panels') + ')');
	sw((x, y) => '<line x1="' + x + '" y1="' + (y + 5) + '" x2="' + (x + 18) + '" y2="' + (y + 5) + '" stroke="#3aa655" stroke-width="3"/>', 'door, open');
	sw((x, y) => '<line x1="' + x + '" y1="' + (y + 5) + '" x2="' + (x + 18) + '" y2="' + (y + 5) + '" stroke="#d9822b" stroke-width="3"/>', 'door with a lock (flag), see the door table');
	sw((x, y) => '<rect x="' + x + '" y="' + y + '" width="18" height="10" fill="' + P.block + '" stroke="' + P.wall + '"/>', 'platform (+top height in cm)');
	sw((x, y) => '<rect x="' + x + '" y="' + y + '" width="18" height="10" fill="' + P.stairs + '" stroke="' + P.wall + '" stroke-dasharray="2 2"/>', 'stairs or ramp (arrow climbs)');
	sw((x, y) => '<rect x="' + x + '" y="' + (y + 2) + '" width="18" height="6" fill="' + P.cover + '"/>', 'cover (about 2 m high)');
	sw((x, y) => '<rect x="' + (x + 5) + '" y="' + y + '" width="9" height="9" fill="' + P.pillar + '"/>', 'pillar or tall block');
	sw((x, y) => '<rect x="' + (x + 5) + '" y="' + y + '" width="9" height="9" fill="' + (era === 'past' ? '#b56ad1' : '#39b8c9') + '"/>', era === 'past' ? 'rune pylon (gone when its zone is cleared)' : 'cryo tube');
	sw((x, y) => '<path d="M' + (x + 9) + ' ' + (y - 3) + ' L' + (x + 17) + ' ' + (y + 5) + ' L' + (x + 9) + ' ' + (y + 13) + ' L' + (x + 1) + ' ' + (y + 5) + ' Z" fill="#00bcd4" stroke="#00566b"/>', 'portal (same spot in both eras)');
	sw((x, y) => '<rect x="' + x + '" y="' + y + '" width="18" height="10" fill="none" stroke="#c0392b" stroke-dasharray="7 4"/>', 'encounter trigger box');
	sw((x, y) => '<path d="M' + (x + 5) + ' ' + (y + 1) + ' L' + (x + 13) + ' ' + (y + 9) + ' M' + (x + 5) + ' ' + (y + 9) + ' L' + (x + 13) + ' ' + (y + 1) + '" stroke="#c0392b" stroke-width="1.6"/>', 'enemy spawn point');
	sw((x, y) => '<rect x="' + (x + 4) + '" y="' + (y + 1) + '" width="10" height="8" fill="#f1c40f" stroke="#7d6608"/>', 'chest or crate');
	sw((x, y) => '<circle cx="' + (x + 9) + '" cy="' + (y + 5) + '" r="3" fill="#27ae60" stroke="#145a32"/>', 'pickup');
	sw((x, y) => '<rect x="' + x + '" y="' + y + '" width="18" height="10" fill="none" stroke="#2980b9" stroke-dasharray="2 3"/>', 'checkpoint volume');
	sw((x, y) => '<line x1="' + x + '" y1="' + (y + 5) + '" x2="' + (x + 18) + '" y2="' + (y + 5) + '" stroke="#e67e22" stroke-width="1.6"/>', 'critical path (walking legs)');
	sw((x, y) => '<circle cx="' + (x + 9) + '" cy="' + (y + 5) + '" r="7" fill="#e67e22" stroke="#7e3f05"/>', 'station number (order of play)');
	ly += 8;
	L('Critical path, in order', true);
	for (const s of stations) {
		const mine = s.era === era;
		o.push('<text x="' + lx + '" y="' + ly + '" font-size="9.5" fill="' + (mine ? P.text : '#8c8c8c') + '"' + (mine ? ' font-weight="bold"' : '') + '>' + s.n + '. ' + esc((mine ? '' : '[' + s.era + '] ') + s.text).slice(0, 62) + '</text>');
		ly += 13;
	}
	ly += 8;
	L('Doors with locks', true);
	for (const d of doors.filter(dd => dd.era === era && dd.lock.length)) {
		const fl = d.lock.map(k => k.removedBy ? k.removedBy.replace('Encounter.', '').replace('.Cleared', '') : 'never').join(' + ');
		o.push('<text x="' + lx + '" y="' + ly + '" font-size="9.5" fill="' + P.text + '">' + esc(d.id + ': opens on ' + fl) + '</text>');
		ly += 13;
	}
	o.push('</svg>');
	return o.join('\n');
}

function drawOverlay() {
	const o = [];
	o.push('<svg xmlns="http://www.w3.org/2000/svg" width="' + W + '" height="' + H + '" viewBox="0 0 ' + W + ' ' + H + '" font-family="Verdana, Arial, sans-serif" font-size="11">');
	o.push('<rect width="' + W + '" height="' + H + '" fill="#fafafa"/>');
	o.push('<text x="' + MX + '" y="34" font-size="20" font-weight="bold" fill="#222">Glyphbound world plan. OVERLAY: past (brown) over future (blue)</text>');
	o.push('<text x="' + MX + '" y="54" fill="#222">Same frame and scale as the two era plans. Purple = one shared collision body in both eras. Cyan diamonds = portals: the floor is clear there in both eras.</text>');
	for (let y = -5600; y <= 5600; y += 1600) o.push(lineEl([XMIN, y], [XMAX, y], 'stroke="#ddd" stroke-width="0.8"'));
	for (let x = -13600; x <= 7600; x += 1600) o.push(lineEl([x, YMIN], [x, YMAX], 'stroke="#ddd" stroke-width="0.8"'));
	for (const s of S) {
		if (s.sealed) { o.push(rectEl(s.rect, 'fill="#3b6ea5" fill-opacity="0.12" stroke="#3b6ea5" stroke-width="0.5" stroke-dasharray="3 3"')); continue; }
		if (s.kind === 'prop') continue;
		const col = s.era === 'both' ? '#8e44ad' : (s.era === 'past' ? '#8a5a2b' : '#2f6fb0');
		o.push(rectEl(s.rect, 'fill="' + col + '" fill-opacity="0.35" stroke="' + col + '" stroke-width="0.8"'));
	}
	for (const sg of wallSegments(baseLines, 'future')) { const p = segPts(sg); o.push(lineEl(p[0], p[1], 'stroke="#2f6fb0" stroke-width="6" stroke-opacity="0.55"')); }
	for (const sg of wallSegments(castleLines, 'past')) { const p = segPts(sg); o.push(lineEl(p[0], p[1], 'stroke="#8a5a2b" stroke-width="2.5"')); }
	for (const r of rooms) {
		const c = [(r.rect[0] + r.rect[1]) / 2, (r.rect[2] + r.rect[3]) / 2];
		if (r.id === 'BREACH' || r.id === 'ANTE') continue;
		o.push(textEl([c[0] + 130, c[1]], r.past || '(not built)', 'text-anchor="middle" font-size="11" font-weight="bold" fill="#6b4420"'));
		o.push(textEl([c[0] - 60, c[1]], r.future, 'text-anchor="middle" font-size="11" font-weight="bold" fill="#1f4f80"'));
	}
	for (const d of doors) {
		const lp = d.axis === 'X' ? [d.at + (d.era === 'past' ? 130 : -260), d.along + d.w / 2 + 40] : [d.along + d.w / 2 + 60, d.at + (d.era === 'past' ? 60 : -230)];
		o.push(textEl(lp, d.id, 'font-size="9.5" font-weight="bold" fill="' + (d.era === 'past' ? '#6b4420' : '#1f4f80') + '"'));
	}
	for (const p of portals) {
		const x = sx(p.pos[1]), y = sy(p.pos[0]);
		o.push('<path d="M' + x + ' ' + (y - 9) + ' L' + (x + 9) + ' ' + y + ' L' + x + ' ' + (y + 9) + ' L' + (x - 9) + ' ' + y + ' Z" fill="#00bcd4" stroke="#00566b" stroke-width="1.4"/>');
		o.push('<text x="' + (x + 11) + '" y="' + (y - 7) + '" font-size="11" font-weight="bold" fill="#00566b">' + p.id + '</text>');
	}
	const x = sx(0), y = sy(6850);
	o.push('<circle cx="' + x + '" cy="' + y + '" r="9" fill="#e74c3c" stroke="#3d1452" stroke-width="1.4"/><text x="' + (x + 12) + '" y="' + (y - 8) + '" font-size="10" font-weight="bold" fill="#3d1452">SEAL (past) / FINAL PORTAL (future)</text>');
	const bx = MX + 10, by = H - 40;
	o.push('<rect x="' + bx + '" y="' + by + '" width="' + (5000 * SC) + '" height="6" fill="#222"/><text x="' + bx + '" y="' + (by + 20) + '" font-size="10">0</text><text x="' + (bx + 5000 * SC) + '" y="' + (by + 20) + '" font-size="10" text-anchor="middle">50 m</text>');
	o.push('</svg>');
	return o.join('\n');
}

// ------------------------------------------------------------------ side sections
function drawSections() {
	const s = 0.1; // px per cm: 1 m = 10 px, true scale (same horizontally and vertically)
	const o = [];
	const WW = 2300, HH = 1500;
	o.push('<svg xmlns="http://www.w3.org/2000/svg" width="' + WW + '" height="' + HH + '" viewBox="0 0 ' + WW + ' ' + HH + '" font-family="Verdana, Arial, sans-serif" font-size="11">');
	o.push('<rect width="' + WW + '" height="' + HH + '" fill="#fbfaf6"/>');
	o.push('<defs><marker id="arr" markerWidth="6" markerHeight="6" refX="5" refY="3" orient="auto"><path d="M0 0 L6 3 L0 6 Z" fill="#333"/></marker></defs>');
	o.push('<text x="40" y="34" font-size="20" font-weight="bold">Glyphbound world plan. Side sections (true scale, 1 m = 10 px)</text>');
	o.push('<text x="40" y="54">Heights in cm above the main floor (Z 0). Reach lines from Player.json: one jump 204; jump + ledge climb 364 (design limit 320); double jump + ledge climb 568 (design limit 500); nothing under 650 may be used as a barrier.</text>');

	function section(x0px, basePx, hMin, hMax, title, draw) {
		const X = h => +(x0px + (h - hMin) * s).toFixed(1);
		const Z = z => +(basePx - z * s).toFixed(1);
		o.push('<text x="' + x0px + '" y="' + (Z(2600) - 8) + '" font-size="13" font-weight="bold">' + esc(title) + '</text>');
		o.push('<line x1="' + X(hMin) + '" y1="' + Z(0) + '" x2="' + X(hMax) + '" y2="' + Z(0) + '" stroke="#333" stroke-width="2"/>');
		for (let z = 400; z <= 2400; z += 400) {
			o.push('<line x1="' + X(hMin) + '" y1="' + Z(z) + '" x2="' + X(hMax) + '" y2="' + Z(z) + '" stroke="#ddd" stroke-width="0.6"/>');
			o.push('<text x="' + (X(hMin) - 6) + '" y="' + (Z(z) + 3) + '" text-anchor="end" font-size="9" fill="#666">+' + z + '</text>');
		}
		draw({ X, Z,
			box: (a, b, z0, z1, fill, label) => {
				o.push('<rect x="' + X(a) + '" y="' + Z(z1) + '" width="' + ((b - a) * s).toFixed(1) + '" height="' + ((z1 - z0) * s).toFixed(1) + '" fill="' + fill + '" stroke="#333" stroke-width="0.8"/>');
				if (label) o.push('<text x="' + X((a + b) / 2) + '" y="' + (Z(z1) - 4) + '" text-anchor="middle" font-size="9">' + esc(label) + '</text>');
			},
			wall: (h, z1, col, gapTop) => {
				o.push('<line x1="' + X(h) + '" y1="' + Z(gapTop || 0) + '" x2="' + X(h) + '" y2="' + Z(z1) + '" stroke="' + col + '" stroke-width="5"/>');
			},
			ceil: (a, b, z, col) => { o.push('<line x1="' + X(a) + '" y1="' + Z(z) + '" x2="' + X(b) + '" y2="' + Z(z) + '" stroke="' + col + '" stroke-width="3"/>'); },
			ramp: (a, b, z0, z1, fill) => { o.push('<path d="M' + X(a) + ' ' + Z(z0) + ' L' + X(b) + ' ' + Z(z1) + ' L' + X(b) + ' ' + Z(z0) + ' Z" fill="' + fill + '" stroke="#333" stroke-width="0.8"/>'); },
			label: (h, z, t, anchor, size) => { o.push('<text x="' + X(h) + '" y="' + Z(z) + '" text-anchor="' + (anchor || 'middle') + '" font-size="' + (size || 10) + '">' + esc(t) + '</text>'); },
			player: (h, z) => { o.push('<rect x="' + (X(h) - 3.8) + '" y="' + Z(z + 180) + '" width="7.6" height="18" rx="3" fill="#e67e22" stroke="#7e3f05" stroke-width="0.6"/>'); },
			reach: (h, z) => {
				const w = 46;
				[[204, '#27ae60', 'jump 204'], [364, '#2980b9', '+climb 364'], [568, '#8e44ad', 'double +climb 568']].forEach(r => {
					o.push('<line x1="' + (X(h) - 6) + '" y1="' + Z(z + r[0]) + '" x2="' + (X(h) + w) + '" y2="' + Z(z + r[0]) + '" stroke="' + r[1] + '" stroke-width="1" stroke-dasharray="4 2"/>');
					o.push('<text x="' + (X(h) + w + 3) + '" y="' + (Z(z + r[0]) + 3) + '" font-size="8.5" fill="' + r[1] + '">' + r[2] + '</text>');
				});
			}
		});
	}

	// A. past, south-north through Y 0: gatehouse, courtyard, great hall, seal room
	section(70, 380, -4200, 7400, 'A. PAST, south to north along Y 0 (looking west): Gatehouse, Courtyard, Great Hall, Seal Room', g => {
		const brown = '#4a3b2a';
		g.wall(-4000, 1500, brown, 0); g.label(-4000, 1540, 'gate, shut');
		g.wall(-2400, 1500, brown, 492); g.label(-2400, 1540, 'D1');
		g.wall(2400, 1500, brown, 492); g.label(2400, 1540, 'D2');
		g.wall(5600, 1500, brown, 492); g.label(5600, 1540, 'D5');
		g.wall(7200, 1500, brown, 0);
		g.ceil(-4000, -2400, 1500, brown); g.ceil(2400, 7200, 1500, brown);
		g.label(-3200, 1560, 'Gatehouse'); g.label(0, 1560, 'Courtyard (open sky, walls 15 m)', 'middle', 11); g.label(4000, 1560, 'Great Hall (ceiling 15 m)', 'middle', 11); g.label(6400, 1560, 'Seal Room');
		g.box(-405, 405, 0, 210, '#b9a682', 'east stage +210 (beyond)');
		g.box(-800, -420, 0, 410, '#cdbf9f', 'west perch +410');
		g.box(1790, 2010, 0, 507, '#5d4b36', 'podium 507');
		g.box(690, 1310, 0, 203, '#7a6548', 'low wall 203');
		g.box(3290, 3510, 0, 507, '#b56ad1', 'pylon'); g.box(4490, 4710, 0, 507, '#b56ad1', 'pylon');
		g.box(2725, 2875, 0, 1400, '#5d4b36', ''); g.box(3525, 3675, 0, 1400, '#5d4b36', ''); g.box(4325, 4475, 0, 1400, '#5d4b36', 'pillars 14 m'); g.box(5125, 5275, 0, 1400, '#5d4b36', '');
		g.box(4000, 5600, 0, 210, '#b9a682', '');
		g.label(4800, 250, 'side gallery +210', 'middle', 9);
		g.label(4000, 640, 'Valkyrie in the rune cage', 'middle', 10);
		g.player(-3300, 0); g.reach(-1800, 0); g.player(-1800, 0);
		g.label(6850, 60, 'seal'); g.label(6150, 430, 'P1');
		g.box(6130, 6170, 0, 400, '#00bcd4', '');
	});

	// B. future, same line, plus breach and hangar
	section(70, 800, -13500, 7400, 'B. FUTURE, south to north along Y 0 (looking west): Hangar, Antechamber, Breach Corridor, Checkpoint, Yard, Containment Hall, Seal Vault', g => {
		const blue = '#1f2e3d';
		g.wall(-13250, 2400, blue, 0); g.wall(-8450, 2400, blue, 0); g.ceil(-13250, -8450, 2400, blue);
		g.label(-10850, 2460, 'Hangar (ceiling 24 m)', 'middle', 11);
		g.box(-13250, -13050, 262, 282, '#9db0c4', 'gantry +282');
		g.ramp(-12450, -13050, 0, 282, '#b9c8d8');
		g.box(-12600, -11400, 0, 300, '#3f5873', 'container 300'); g.box(-10300, -9100, 0, 300, '#3f5873', 'container 300');
		g.box(-12150, -9550, 0, 100, '#9db0c4', '');
		g.label(-10850, 130, 'docks +100 (along the side walls)', 'middle', 9);
		// the Herald, 9.4 m tall, floating
		o.push('<ellipse cx="' + g.X(-10850) + '" cy="' + g.Z(950) + '" rx="30" ry="20" fill="#3affb0" fill-opacity="0.35" stroke="#0b7a52" stroke-width="1.2"/>');
		o.push('<line x1="' + g.X(-10850) + '" y1="' + g.Z(750) + '" x2="' + g.X(-10850) + '" y2="' + g.Z(210) + '" stroke="#0b7a52" stroke-width="1" stroke-dasharray="3 2"/>');
		g.label(-10450, 1330, 'the Herald: about 9.4 m from shell top (+1150) to tentacle tips (+210), floats', 'start', 10);
		g.wall(-8050, 600, blue, 333); g.ceil(-8450, -8050, 600, blue); g.label(-8250, 640, 'Ante');
		g.ceil(-8050, -4000, 333, blue); g.label(-6000, 380, 'Breach Corridor (ceiling 3.33 m)', 'middle', 10);
		g.wall(-4000, 600, blue, 303); g.ceil(-4000, -2400, 600, blue); g.label(-3200, 640, 'Checkpoint');
		g.wall(-2400, 1200, blue, 425); g.label(-2400, 1240, 'D1f');
		g.wall(2400, 1200, blue, 425); g.label(2400, 1240, 'D2f');
		g.label(0, 1260, 'Yard (open sky, walls 12 m)', 'middle', 11);
		g.box(1575, 1825, 0, 260, '#3f5873', 'container 260'); g.box(-1825, -1575, 0, 260, '#3f5873', '');
		g.box(-1300, 1300, 0, 100, '#9db0c4', 'docks +100 (east and west of the centre lane)');
		g.ceil(2400, 5600, 1200, blue); g.label(4000, 1260, 'Containment Hall (ceiling 12 m)', 'middle', 11);
		g.box(3790, 4210, 0, 455, '#39b8c9', 'cryo tube 455');
		g.box(2725, 2875, 0, 1200, '#2c4258', ''); g.box(3525, 3675, 0, 1200, '#2c4258', ''); g.box(4325, 4475, 0, 1200, '#2c4258', ''); g.box(5125, 5275, 0, 1200, '#2c4258', '');
		g.box(4000, 5600, 0, 210, '#9db0c4', '');
		g.wall(5600, 1200, blue, 303); g.label(5600, 1240, 'D5f');
		g.ceil(5600, 7200, 700, blue); g.wall(7200, 700, blue, 0); g.label(6400, 740, 'Seal Vault');
		g.box(6130, 6170, 0, 400, '#00bcd4', ''); g.label(6150, 430, 'P1'); g.label(6850, 60, 'final portal');
		g.player(-9000, 0); g.reach(-9000, 0);
		g.player(0, 0);
	});

	// C. past, west-east through X 4600: frozen cistern, great hall, high ward
	section(70, 1260, -5800, 5800, 'C. PAST, west to east along X 4600 (looking north): Frozen Cistern, Great Hall, High Ward', g => {
		const brown = '#4a3b2a';
		g.wall(-5600, 1500, brown, 0); g.wall(-2400, 1500, brown, 0); g.wall(2400, 1500, brown, 0); g.wall(5600, 1500, brown, 0);
		g.ceil(-5600, 2400, 1500, brown);
		g.label(-4000, 1560, 'Frozen Cistern (ceiling 15 m)', 'middle', 11); g.label(0, 1560, 'Great Hall', 'middle', 11); g.label(4000, 1560, 'High Ward (open sky)', 'middle', 11);
		g.box(-5075, -4925, 0, 1400, '#5d4b36', ''); g.box(-4075, -3925, 0, 1400, '#5d4b36', 'pillars'); g.box(-3075, -2925, 0, 1400, '#5d4b36', '');
		g.box(-4700, -4300, 0, 200, '#9fd3e6', 'ice 200'); g.box(-3700, -3300, 0, 200, '#9fd3e6', '');
		g.box(-2400, -1800, 0, 210, '#b9a682', 'gallery +210'); g.box(1800, 2400, 0, 210, '#b9a682', 'gallery +210');
		g.box(-1275, -1125, 0, 1400, '#5d4b36', ''); g.box(1125, 1275, 0, 1400, '#5d4b36', '');
		g.box(-710, -490, 0, 507, '#b56ad1', 'pylon'); g.box(490, 710, 0, 507, '#b56ad1', 'pylon');
		g.box(2400, 5600, 0, 410, '#b9a682', '');
		g.label(3500, 180, 'terrace +410 (4.1 m above the strip)');
		g.box(2800, 3200, 410, 610, '#cdbf9f', 'tower +610'); g.box(3800, 4200, 410, 610, '#cdbf9f', 'tower +610');
		g.box(4800, 5600, 410, 700, '#cdbf9f', 'aerie +700 (chest)');
		g.label(4000, 900, 'the spire (+860) stands at X 4300, south of this cut', 'middle', 9);
		g.player(3500, 410);
		g.player(-2650, 0);
	});
	o.push('<text x="40" y="' + (HH - 20) + '" font-size="10" fill="#555">Orange figure = the player (capsule 180 cm). Open-sky rooms: nothing the player can stand on is higher than the wall top minus 600, so the wall tops stay out of reach.</text>');
	o.push('</svg>');
	return o.join('\n');
}

fs.writeFileSync(path.join(OUT, 'plan_past.svg'), drawPlan('past') + '\n');
fs.writeFileSync(path.join(OUT, 'plan_future.svg'), drawPlan('future') + '\n');
fs.writeFileSync(path.join(OUT, 'plan_overlay.svg'), drawOverlay() + '\n');
fs.writeFileSync(path.join(OUT, 'section_side.svg'), drawSections() + '\n');

// ------------------------------------------------------------------ report
const count = (era) => structuresAt(era).filter(s => !s.sealed).length;
console.log('castle shell pieces: ' + castleWallPieces.length + ' (' + castleWallPieces.filter(p => p.door).length + ' with a door)');
console.log('structures: past ' + count('past') + ', future ' + count('future') + ', shared ' + S.filter(s => s.era === 'both').length);
console.log('collision shell: past ' + shell.past.walls.length + ' walls + ' + shell.past.lintels.length + ' lintels, future ' + shell.future.walls.length + ' walls + ' + shell.future.lintels.length + ' lintels');
console.log('encounters: ' + encounters.length + ', portals: ' + portals.length + ', doors: ' + doors.length);
console.log(problems.length ? 'PROBLEMS:\n  ' + problems.join('\n  ') : 'layout checks: clean');
process.exitCode = problems.length ? 1 : 0;
