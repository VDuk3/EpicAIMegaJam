# review_valkyrie.py - the 3D Valkyrie in a row of poses from her clips, with the sword on hand_r at the WeaponOffset of
# Enemies.json, rendered offscreen from the front, three quarters and the back (overnight point 1: stable wings, the
# sword held right in every pose). Uses the Review_Enemies level. Settings: Saved/review_valkyrie.json
#   {"out": dir, "offset": "x y z pitch yaw roll scale", "poses": [[clip, time], ...], "mode": "Lit"}
import unreal, json, os, base64

CFG = json.load(open(os.path.join(unreal.Paths.project_saved_dir(), "review_valkyrie.json")))
LEVEL = "/Game/Glyphbound/Maps/Dev/Review_Enemies"
LES = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
EAS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
V = "/Game/Glyphbound/Characters/Valkyrie/"
SWORD = "/Game/Glyphbound/Weapons/ValkyrieSword/SM_ValkyrieSword"
SPACING = 260.0

cur = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_path_name()
if not cur.startswith(LEVEL):
    LES.load_level(LEVEL)
def is_lineup(a):
    # the line-up's characters and their parts; the floor and the lights stay
    return a.get_actor_label().startswith("RV_") and isinstance(a, (unreal.SkeletalMeshActor, unreal.StaticMeshActor))         and "Floor" not in a.get_actor_label()


for a in EAS.get_all_level_actors():
    if is_lineup(a):
        a.set_is_temporarily_hidden_in_editor(True)
        a.set_actor_hidden_in_game(True)
    if a.get_actor_label().startswith("RVV_"):
        EAS.destroy_actor(a)

v = [float(x) for x in CFG["offset"].split()]
loc, rot, sc = unreal.Vector(v[0], v[1], v[2]), unreal.Rotator(roll=v[5], pitch=v[3], yaw=v[4]), v[6]
poses = CFG["poses"]
for i, (clip, t) in enumerate(poses):
    x = 3000.0
    y = (i - (len(poses) - 1) / 2.0) * SPACING
    a = EAS.spawn_actor_from_class(unreal.SkeletalMeshActor, unreal.Vector(x, y, 0), unreal.Rotator(roll=0, pitch=0, yaw=CFG.get("yaw", -90.0) + 180.0))
    a.set_actor_label("RVV_%d" % i)
    comp = a.skeletal_mesh_component
    comp.set_skinned_asset_and_update(unreal.load_asset(V + "SK_Valkyrie"))
    an = unreal.load_asset(V + "Anims/A_Valkyrie_" + clip)
    if an:
        comp.override_animation_data(an, True, False, float(t), 0.0)
    w = EAS.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(x, y, 0))
    w.set_actor_label("RVV_%d_W" % i)
    w.static_mesh_component.set_static_mesh(unreal.load_asset(SWORD))
    w.set_mobility(unreal.ComponentMobility.MOVABLE)
    w.attach_to_component(comp, "hand_r", unreal.AttachmentRule.SNAP_TO_TARGET, unreal.AttachmentRule.SNAP_TO_TARGET,
                          unreal.AttachmentRule.KEEP_WORLD, False)
    w.root_component.set_editor_property("relative_location", loc)
    w.root_component.set_editor_property("relative_rotation", rot)
    w.root_component.set_editor_property("relative_scale3d", unreal.Vector(sc, sc, sc))

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
out = []
span = (len(poses) - 1) * SPACING
dist = max(900.0, span * 0.75)
shots = CFG.get("shots") or [["front", 3000 - dist, 0, 140, -3, 0], ["back", 3000 + dist, 0, 140, -3, 180],
                             ["side34", 3000 - dist * 0.7, -dist * 0.7, 160, -5, 45]]
for name, x, y, z, pitch, yaw in shots:
    cam = {"location": {"x": x, "y": y, "z": z}, "rotation": {"pitch": pitch, "yaw": yaw, "roll": 0},
           "projectionMode": "Perspective", "fOV": CFG.get("fov", 70)}
    r = await call_tool("SceneCaptureToolset.SceneCaptureToolset.Capture", world={"refPath": world.get_path_name()},
                        camera=cam, mode=CFG.get("mode", "Lit"),
                        options={"width": CFG.get("width", 1800), "height": CFG.get("height", 700),
                                 "screenPercentage": 1.0, "visualizationSubMode": ""})
    img = r["image"]
    data = img["data"] if isinstance(img, dict) else img
    with open(os.path.join(CFG["out"], "valk_" + name + ".png"), "wb") as f:
        f.write(base64.b64decode(data))
    out.append(name)
for a in EAS.get_all_level_actors():
    if is_lineup(a):
        a.set_is_temporarily_hidden_in_editor(False)
        a.set_actor_hidden_in_game(False)
out
