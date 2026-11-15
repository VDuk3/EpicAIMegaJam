# castle_ledges.py - owner's castle playtest, 2026-11-15, note 9: "floor separation between the castle's rooms" (his
# picture: a plank floor ending in a ledge some 40-60 cm above the next room's floor). Finds every such ledge in the
# keep: a 50 cm grid of downward traces on each walking level; a ledge is two neighbouring cells 20..110 cm apart in
# height with flat floor on both sides (so a stair, which climbs cell after cell, is not one).
# Writes <Saved>/castle_ledges.json: [{x, y, lo, hi, dir, lo_mesh, hi_mesh}], clustered.
# Run in the Editor: node Tools/Jam/mcp_call.js py Tools/Jam/castle_ledges.py
import json
import os

import unreal

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
STEP = 50.0
X0, X1, Y0, Y1 = 5600.0, 13400.0, -6600.0, 5000.0
STARTS = (236.0 + 160.0, 236.0 - 340.0, 236.0 + 2560.0, 236.0 + 1200.0)
nx, ny = int((X1 - X0) / STEP), int((Y1 - Y0) / STEP)
out = []
for z0 in STARTS:
    hz = {}
    for i in range(nx + 1):
        for j in range(ny + 1):
            x, y = X0 + i * STEP, Y0 + j * STEP
            hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x, y, z0), unreal.Vector(x, y, z0 - 320.0),
                                                         unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [], unreal.DrawDebugTrace.NONE, True)
            if hit and hit.to_tuple()[0]:
                t = hit.to_tuple()
                comp = t[10] if len(t) > 10 else None
                nm = comp.get_owner().get_actor_label() if comp else '?'
                mesh = ''
                try:
                    mesh = comp.static_mesh.get_name()
                except Exception:
                    pass
                hz[(i, j)] = (t[4].z, mesh or nm)

    def flat(c, d, z):
        n = (c[0] + d[0], c[1] + d[1])
        return n in hz and abs(hz[n][0] - z) < 6.0
    for (i, j), (z, m) in hz.items():
        for d in ((1, 0), (0, 1), (-1, 0), (0, -1)):
            nb = (i + d[0], j + d[1])
            if nb not in hz:
                continue
            z2, m2 = hz[nb]
            if not (20.0 < z2 - z < 110.0):
                continue
            back = (-d[0], -d[1])
            if flat((i, j), back, z) and flat(nb, d, z2):
                out.append(dict(x=X0 + i * STEP, y=Y0 + j * STEP, lo=round(z), hi=round(z2), dir=d, lo_mesh=m, hi_mesh=m2, level=z0))
# cluster: same level pair, direction, within 120 cm
clusters = []
for p in out:
    for c in clusters:
        if c['dir'] == p['dir'] and abs(c['lo'] - p['lo']) < 8 and abs(c['hi'] - p['hi']) < 8 and \
                min(abs(c['x0'] - p['x']), abs(c['x1'] - p['x'])) <= 120 and min(abs(c['y0'] - p['y']), abs(c['y1'] - p['y'])) <= 120:
            c['x0'], c['x1'] = min(c['x0'], p['x']), max(c['x1'], p['x'])
            c['y0'], c['y1'] = min(c['y0'], p['y']), max(c['y1'], p['y'])
            c['n'] += 1
            break
    else:
        clusters.append(dict(x0=p['x'], x1=p['x'], y0=p['y'], y1=p['y'], lo=p['lo'], hi=p['hi'], dir=p['dir'], n=1,
                             lo_mesh=p['lo_mesh'], hi_mesh=p['hi_mesh']))
clusters.sort(key=lambda c: -c['n'])
json.dump(clusters, open(os.path.join(unreal.Paths.project_saved_dir(), 'castle_ledges.json'), 'w'), indent=1)
print('%d ledge cells, %d ledges' % (len(out), len(clusters)))
for c in clusters[:40]:
    print(c)
