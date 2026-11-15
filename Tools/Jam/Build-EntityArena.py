# Build-EntityArena.py - the final boss's arena (owner, 2026-11-15): a circle 40 m across, completely black, whose floor is a
# white, faintly glowing Niagara Fluids liquid (NS_Arena_Liquid, tagged ArenaLiquid: the boss stirs it, GBValkyrieEnemy.cpp
# GBReaperFx). The plague reaper (encounter "Entity", prespawned) waits in the centre; the player arrives at the south edge
# through the final portal (Game.json EntityMapPath). No lights, no sky: the liquid is the only light, under a fixed exposure.
# Run in the Editor: node Tools/Jam/mcp_call.js py Tools/Jam/Build-EntityArena.py   (creates or rebuilds the map, saves it)
import unreal, math

MAP = '/Game/Glyphbound/Maps/EntityArena'
BLACK = '/Game/Glyphbound/Materials/M_ArenaBlack'
LIQUID = '/Game/Glyphbound/FX/Arena/NS_Arena_Liquid'
RADIUS = 2000.0                 # cm: 40 m across (owner)
WALL_H = 1500.0
WALL_SEGMENTS = 40
EXPOSURE_BIAS = 0.0             # manual exposure (EV100 0 + bias); tuned on the liquid's glow
eal = unreal.EditorAssetLibrary
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
MEL = unreal.MaterialEditingLibrary
log = []


def rot(yaw=0.0):
    return unreal.Rotator(0.0, 0.0, yaw)        # (roll, pitch, yaw)


def black_material():
    if eal.does_asset_exist(BLACK):
        return unreal.load_asset(BLACK)
    pkg, name = BLACK.rsplit('/', 1)
    m = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, pkg, unreal.Material, unreal.MaterialFactoryNew())
    c = MEL.create_material_expression(m, unreal.MaterialExpressionConstant3Vector, -300, 0)
    c.set_editor_property('constant', unreal.LinearColor(0, 0, 0, 1))
    MEL.connect_material_property(c, '', unreal.MaterialProperty.MP_BASE_COLOR)
    z = MEL.create_material_expression(m, unreal.MaterialExpressionConstant, -300, 150)
    z.set_editor_property('r', 0.0)
    MEL.connect_material_property(z, '', unreal.MaterialProperty.MP_SPECULAR)
    one = MEL.create_material_expression(m, unreal.MaterialExpressionConstant, -300, 250)
    one.set_editor_property('r', 1.0)
    MEL.connect_material_property(one, '', unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.recompile_material(m)
    eal.save_loaded_asset(m)
    return m


def shape(path, loc, scale, yaw, mat, label):
    a = eas.spawn_actor_from_class(unreal.StaticMeshActor, loc, rot(yaw))
    smc = a.static_mesh_component
    smc.set_static_mesh(unreal.load_asset(path))
    smc.set_material(0, mat)
    smc.set_collision_profile_name('BlockAll')
    a.set_actor_scale3d(scale)
    a.set_actor_label(label)
    a.tags = [unreal.Name('EntityArena')]
    return a


if not eal.does_asset_exist(MAP):
    les.new_level(MAP, False)
    les.save_current_level()
elif ues.get_editor_world().get_path_name().split('.')[0] != MAP:
    les.load_level(MAP)
for a in eas.get_all_level_actors():
    if a.actor_has_tag('EntityArena'):
        eas.destroy_actor(a)

mat = black_material()
# floor: the walk surface is an invisible slab with its top at z 0; the visible black floor lies 55 cm lower, under the
# liquid's wave troughs (NS_Arena_Liquid rests about 8 cm over the walk surface and dips below it in the waves)
walk = shape('/Engine/BasicShapes/Cylinder', unreal.Vector(0, 0, -20), unreal.Vector(RADIUS * 2.2 / 100.0, RADIUS * 2.2 / 100.0, 0.2), 0, mat, 'Arena_WalkFloor')
walk.set_actor_hidden_in_game(True)
walk.static_mesh_component.set_visibility(False)
deep = shape('/Engine/BasicShapes/Cylinder', unreal.Vector(0, 0, -75), unreal.Vector(RADIUS * 2.2 / 100.0, RADIUS * 2.2 / 100.0, 0.2), 0, mat, 'Arena_Floor')
deep.static_mesh_component.set_collision_profile_name('NoCollision')
# the rim: a ring of black wall segments (they vanish against the black void; they only stop the player)
seg_w = 2 * math.pi * RADIUS / WALL_SEGMENTS * 1.1
for k in range(WALL_SEGMENTS):
    ang = 360.0 * k / WALL_SEGMENTS
    x = math.cos(math.radians(ang)) * (RADIUS + 50)
    y = math.sin(math.radians(ang)) * (RADIUS + 50)
    shape('/Engine/BasicShapes/Cube', unreal.Vector(x, y, WALL_H / 2), unreal.Vector(1.0, seg_w / 100.0, WALL_H / 100.0), ang, mat, 'Arena_Wall_%02d' % k)

ppv = eas.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector(0, 0, 0), rot())
ppv.set_editor_property('unbound', True)
st = ppv.get_editor_property('settings')
for name, value in (('auto_exposure_method', unreal.AutoExposureMethod.AEM_MANUAL), ('auto_exposure_bias', EXPOSURE_BIAS),
                    ('auto_exposure_apply_physical_camera_exposure', False), ('bloom_intensity', 0.8)):
    st.set_editor_property('override_' + name, True)
    st.set_editor_property(name, value)
ppv.set_editor_property('settings', st)
ppv.set_actor_label('Arena_PostProcess')
ppv.tags = [unreal.Name('EntityArena')]

ps = eas.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(0, -RADIUS + 300, 110), rot(90))
ps.set_actor_label('Arena_PlayerStart')
ps.tags = [unreal.Name('EntityArena')]

enc = eas.spawn_actor_from_class(unreal.GBEncounter, unreal.Vector(0, 0, 0), rot())
enc.set_editor_property('encounter_id', unreal.Name('Entity'))
enc.set_editor_property('presence', unreal.GBEraPresence.BOTH)
enc.set_editor_property('trigger_extent', unreal.Vector(1100, 1100, 600))
enc.set_editor_property('spawn_points', [unreal.Vector(0, 0, 200)])
enc.set_actor_label('Arena_Encounter_Entity')
enc.tags = [unreal.Name('EntityArena')]

nav = eas.spawn_actor_from_class(unreal.NavMeshBoundsVolume, unreal.Vector(0, 0, 300), rot())
nav.set_actor_scale3d(unreal.Vector(RADIUS * 2.2 / 200.0, RADIUS * 2.2 / 200.0, 8.0))
nav.set_actor_label('Arena_Nav')
nav.tags = [unreal.Name('EntityArena')]

if eal.does_asset_exist(LIQUID):
    liq = eas.spawn_actor_from_class(unreal.NiagaraActor, unreal.Vector(0, 0, 8), rot())
    liq.niagara_component.set_asset(unreal.load_asset(LIQUID))
    liq.set_actor_label('Arena_Liquid')
    liq.tags = [unreal.Name('EntityArena'), unreal.Name('ArenaLiquid')]
    log.append('liquid placed')
else:
    log.append('NO LIQUID YET (%s missing)' % LIQUID)

ues.get_editor_world().get_world_settings().set_editor_property('kill_z', -2000.0)
les.save_current_level()
log.append('arena built: floor, %d wall segments, post process, player start, encounter Entity, nav' % WALL_SEGMENTS)
'\n'.join(log)
