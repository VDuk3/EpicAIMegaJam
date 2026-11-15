# castle_floor_level.py - owner's castle playtest, 2026-11-15, note 9 ("floor separation between the castle's rooms":
# his picture, a plank floor rising a hand or two at a room's edge). The author's halls were set at slightly different
# heights (castle_ledges.py measured plank floors at 250 next to 272, 2612 next to 2660). Every plank or stone floor
# tile that stands 15..45 cm above a neighbouring floor tile, and below none, is lowered onto it; repeated, so a raised
# patch of several tiles comes down together. The raised seams under the walls are left alone (a wall stands on them).
# Run by Build-PastCastle.py after the keep stage, or alone: node Tools/Jam/mcp_call.js py Tools/Jam/castle_floor_level.py
import unreal

# plank floors only: the stone floor (CastleTower_Floor_A, 222) lies under the halls' plank floors (250) by design, and
# lowering a hall onto it would cascade through the whole keep
FLOORS = ('SM_Px_Planks_4M', 'SM_Px_Square_Planks_4M', 'SM_Px_Herringbone_4M')
eas_ = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world_ = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
VIS = unreal.TraceTypeQuery.TRACE_TYPE_QUERY1


def _top(x, y, z, ignore):
    h = unreal.SystemLibrary.line_trace_single(world_, unreal.Vector(x, y, z + 60.0), unreal.Vector(x, y, z - 120.0), VIS, True, ignore,
                                               unreal.DrawDebugTrace.NONE, True)
    t = h.to_tuple() if h else None
    if not t or not t[0]:
        return None, None
    return t[4].z, t[9]


def level_floors():
    tiles = [a for a in eas_.get_all_level_actors() if isinstance(a, unreal.StaticMeshActor) and a.actor_has_tag('GBCastle')
             and a.static_mesh_component.static_mesh and a.static_mesh_component.static_mesh.get_name() in FLOORS]
    moved = {}
    props = 0
    others = [a for a in eas_.get_all_level_actors() if a.actor_has_tag('GBCastle') and isinstance(a, unreal.StaticMeshActor) and a not in tiles]
    for _ in range(6):
        changed = 0
        for a in tiles:
            o, e = a.get_actor_bounds(False)
            top, hit = _top(o.x, o.y, o.z + e.z, [])
            if top is None or hit != a:
                continue
            lower, higher = [], 0
            for dx, dy in ((e.x + 30.0, 0), (-e.x - 30.0, 0), (0, e.y + 30.0), (0, -e.y - 30.0)):
                for f in (-0.5, 0.0, 0.5):                       # three points along each edge
                    px = o.x + dx + (f * e.x * 2 if dy else 0)
                    py = o.y + dy + (f * e.y * 2 if dx else 0)
                    z, h = _top(px, py, top, [a])
                    if z is None or not isinstance(h, unreal.StaticMeshActor) or not h.static_mesh_component.static_mesh \
                            or h.static_mesh_component.static_mesh.get_name() not in FLOORS:
                        continue
                    if 15.0 <= top - z <= 45.0:
                        lower.append(z)
                    elif z - top > 5.0:
                        higher += 1
            if lower and not higher:
                d = top - max(lower)
                a.set_actor_location(a.get_actor_location() - unreal.Vector(0, 0, d), False, False)
                # what stands on the tile (a table, a bench, a torch stand) comes down with it
                for p in others:
                    po, pe = p.get_actor_bounds(False)
                    if abs(po.z - pe.z - top) < 4.0 and abs(po.x - o.x) < e.x and abs(po.y - o.y) < e.y:
                        p.set_actor_location(p.get_actor_location() - unreal.Vector(0, 0, d), False, False)
                        props += 1
                moved[a.get_name()] = moved.get(a.get_name(), 0) + d
                changed += 1
        if not changed:
            break
    return 'floors: %d of %d plank floor tiles lowered onto their neighbours (by %s cm), %d things on them with them' % (
        len(moved), len(tiles), ', '.join('%.0f' % v for v in sorted(set(round(v) for v in moved.values()))), props)


if __name__ == '__main__' or 'STAGES' not in globals():
    unreal.log('FLOORLEVEL ' + level_floors())
