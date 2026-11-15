# future_gate_fight.py - the energy-gate room's fight (owner, 2026-11-15): places Opening_Future in FutureBase's lab room,
# the room before the Valkyrie where the energy gate is. Run inside the Editor with FutureBase open. Everything runs
# inside functions so no Python reference to the map's actors outlives the call (a lingering one crashed the Editor
# once on a package reload). Idempotent: an actor labelled "GateRoomFight" is reused. It does not save the map.
#   gate_fight_explore()            -> text: floor height and wall distances around the room's centre
#   gate_fight_place(cx, cy, fz, points, extent)  -> places / updates the encounter
import unreal

GATE = unreal.Vector(1550.0, 4954.0, 610.0)   # GBEnergyGate_1; the room is on its -Y side, the Valkyrie on +Y
LABEL = 'GateRoomFight'


def _world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def _hit(w, a, b):
    h = unreal.SystemLibrary.line_trace_single(w, a, b, unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
                                               unreal.DrawDebugTrace.NONE, True)
    t = h.to_tuple()
    return (t[4], t[9].get_actor_label() if t[9] else None) if t[0] else (None, None)


def gate_fight_explore(cy_offsets=(300, 700, 1100, 1500)):
    w = _world()
    assert w.get_name() == 'FutureBase'
    out = []
    for off in cy_offsets:
        c = unreal.Vector(GATE.x, GATE.y - off, GATE.z + 150.0)
        floor, fl = _hit(w, c, c - unreal.Vector(0, 0, 600))
        row = ['y=%d floor=%s(%s)' % (c.y, round(floor.z) if floor else None, fl)]
        for name, d in (('+x', unreal.Vector(1, 0, 0)), ('-x', unreal.Vector(-1, 0, 0)), ('-y', unreal.Vector(0, -1, 0))):
            p, lab = _hit(w, c, c + d * 3000.0)
            row.append('%s=%s(%s)' % (name, round((p - c).length()) if p else None, lab))
        out.append(' '.join(row))
    return '\n'.join(out)


def gate_fight_place(cx, cy, fz, points, extent):
    w = _world()
    assert w.get_name() == 'FutureBase'
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    loc = unreal.Vector(cx, cy, fz + 150.0)
    enc = None
    ref = None
    for a in eas.get_all_level_actors():
        if a.get_actor_label() == LABEL:
            enc = a
        elif a.get_class().get_name() == 'GBEncounter' and str(a.get_editor_property('encounter_id')) == 'Future_Bay':
            ref = a
    if enc is None:
        enc = eas.spawn_actor_from_class(unreal.load_class(None, '/Script/Glyphbound.GBEncounter'), loc, unreal.Rotator(0, 0, 0))
        enc.set_actor_label(LABEL)
    enc.set_actor_location(loc, False, False)
    enc.set_editor_property('encounter_id', 'Opening_Future')
    if ref is not None:
        enc.set_editor_property('presence', ref.get_editor_property('presence'))
    enc.set_editor_property('spawn_points', [unreal.Vector(x, y, -50.0) for (x, y) in points])
    enc.set_editor_property('trigger_extent', unreal.Vector(*extent))
    r = '%s at %s presence=%s points=%d extent=%s' % (enc.get_actor_label(), enc.get_actor_location(),
                                                      enc.get_editor_property('presence'), len(points), extent)
    enc = None
    ref = None
    return r
