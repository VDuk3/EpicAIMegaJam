# review_enemies.py - builds a line-up of the future enemies in a review level (/Game/Glyphbound/Maps/Dev/Review_Enemies:
# floor, sun, sky light) and renders it offscreen. Poses come from the retargeted clips (override_animation_data at a
# fixed time), weapons are attached as the game does (hand_r + WeaponOffset). Run in the Editor through mcp_call.js py.
# Settings: Saved/review_enemies.json {"out": dir, "clip_time": 0.3, "weapon_offset": "x y z pitch yaw roll scale",
#                                      "shots": [[name, x, y, z, pitch, yaw, fov], ...], "rows": [...optional...]}
import unreal, json, os, base64

CFG = json.load(open(os.path.join(unreal.Paths.project_saved_dir(), "review_enemies.json")))
LEVEL = "/Game/Glyphbound/Maps/Dev/Review_Enemies"
LES = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
EAS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
C = "/Game/Glyphbound/Characters/"
RIFLE = "/Game/Glyphbound/Weapons/EnemyRifle/SM_EnemyRifle"
SWORD = "/Game/Glyphbound/Weapons/ValkyrieSword/SM_ValkyrieSword"


def ensure_level():
    if unreal.EditorAssetLibrary.does_asset_exist(LEVEL):
        cur = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_path_name()
        if not cur.startswith(LEVEL):
            LES.load_level(LEVEL)
    else:
        LES.new_level(LEVEL)
    for a in EAS.get_all_level_actors():
        if a.get_actor_label().startswith("RV_"):
            EAS.destroy_actor(a)
    floor = EAS.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, -5))
    floor.set_actor_label("RV_Floor")
    floor.static_mesh_component.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Plane"))
    floor.set_actor_scale3d(unreal.Vector(80, 80, 1))
    sun = EAS.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 500), unreal.Rotator(roll=0, pitch=-40, yaw=-50))
    sun.set_actor_label("RV_Sun")
    # a weaker light from the other side: the offscreen capture shows no sky light, and without this one the side
    # away from the sun is black (the first captures of the refitted orcs showed three silhouettes)
    fill = EAS.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 500), unreal.Rotator(roll=0, pitch=-25, yaw=130))
    fill.set_actor_label("RV_Fill")
    fill.light_component.set_editor_property("intensity", float(CFG.get("fill", 3.0)))
    fill.light_component.set_editor_property("cast_shadows", False)
    sky = EAS.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 300))
    sky.set_actor_label("RV_Sky")
    sky.light_component.set_editor_property("source_type", unreal.SkyLightSourceType.SLS_SPECIFIED_CUBEMAP)
    sky.light_component.set_editor_property("intensity", 1.5)
    pp = EAS.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector(0, 0, 0))
    pp.set_actor_label("RV_PP")
    pp.set_editor_property("unbound", True)
    s = pp.get_editor_property("settings")
    s.set_editor_property("override_auto_exposure_method", True)
    s.set_editor_property("auto_exposure_method", unreal.AutoExposureMethod.AEM_MANUAL)
    s.set_editor_property("override_auto_exposure_bias", True)
    s.set_editor_property("auto_exposure_bias", float(CFG.get("exposure", 11.0)))
    pp.set_editor_property("settings", s)


def parse_offset(txt):
    v = [float(x) for x in txt.split()]
    return unreal.Vector(v[0], v[1], v[2]), unreal.Rotator(roll=v[5], pitch=v[3], yaw=v[4]), v[6]


def skel(label, mesh, x, y, anim=None, armor=(), weapon=None, offset="0 0 0 0 0 0 1", scale=1.0, yaw=0.0, t=None):
    a = EAS.spawn_actor_from_class(unreal.SkeletalMeshActor, unreal.Vector(x, y, 0), unreal.Rotator(roll=0, pitch=0, yaw=yaw))
    a.set_actor_label("RV_" + label)
    a.set_actor_scale3d(unreal.Vector(scale, scale, scale))
    comp = a.skeletal_mesh_component
    comp.set_skinned_asset_and_update(unreal.load_asset(mesh))
    an = None
    if anim:
        an = unreal.load_asset(anim)
        if an:
            comp.override_animation_data(an, True, False, CFG.get("clip_time", 0.3) if t is None else t, 0.0)
    for i, p in enumerate(armor):
        sk = unreal.load_asset(p)
        if not sk:
            continue
        # editor preview: a separate actor per piece with the same clip and time (the game uses Leader Pose)
        pa = EAS.spawn_actor_from_class(unreal.SkeletalMeshActor, unreal.Vector(x, y, 0), unreal.Rotator(roll=0, pitch=0, yaw=yaw))
        pa.set_actor_label("RV_%s_A%d" % (label, i))
        pa.set_actor_scale3d(unreal.Vector(scale, scale, scale))
        pa.skeletal_mesh_component.set_skinned_asset_and_update(sk)
        if anim and an:
            pa.skeletal_mesh_component.override_animation_data(an, True, False, CFG.get("clip_time", 0.3) if t is None else t, 0.0)
    if weapon:
        w = EAS.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(x, y, 0))
        w.set_actor_label("RV_" + label + "_W")
        w.static_mesh_component.set_static_mesh(unreal.load_asset(weapon))
        w.set_mobility(unreal.ComponentMobility.MOVABLE)
        w.attach_to_component(comp, "hand_r", unreal.AttachmentRule.SNAP_TO_TARGET, unreal.AttachmentRule.SNAP_TO_TARGET,
                              unreal.AttachmentRule.KEEP_WORLD, False)
        loc, rot, sc = parse_offset(offset)
        w.root_component.set_editor_property("relative_location", loc)
        w.root_component.set_editor_property("relative_rotation", rot)
        w.root_component.set_editor_property("relative_scale3d", unreal.Vector(sc, sc, sc))
    return a


def static(label, mesh, x, y, z, yaw=0.0, parent=None, socket=""):
    a = EAS.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(x, y, z), unreal.Rotator(roll=0, pitch=0, yaw=yaw))
    a.set_actor_label("RV_" + label)
    a.static_mesh_component.set_static_mesh(unreal.load_asset(mesh))
    a.set_mobility(unreal.ComponentMobility.MOVABLE)
    if parent is not None:
        a.attach_to_component(parent.static_mesh_component, socket, unreal.AttachmentRule.SNAP_TO_TARGET,
                              unreal.AttachmentRule.SNAP_TO_TARGET, unreal.AttachmentRule.KEEP_WORLD, False)
    return a


def lineup():
    off = CFG.get("weapon_offset", "0 0 0 0 0 0 1")
    clip = CFG.get("clip", "Rifle_Idle_ADS")
    yaw = CFG.get("yaw", -90.0)
    y = 0
    skel("Robot", C + "Robot/SK_Robot", 0, y, C + "Robot/Anims/A_Robot_" + clip, weapon=RIFLE, offset=off, yaw=yaw)
    # the armed orcs: the body worn under armor (slimmer, no hands) and the three sets left after 2026-11-14
    # (set B and the boot shafts were dropped by the owner)
    for i, st in enumerate("ACD"):
        armor = [C + "Orc/Armor/SK_OrcArmor_%s_%s" % (st, s) for s in
                 ("Helmet", "Chest", "Pelvis", "Thigh", "UpperArm", "Shoulder", "Hand", "Foot")]
        skel("Orc" + st, C + "Orc/SK_Orc_Armored", 0, 250 + 200 * i, C + "Orc/Anims/A_Orc_" + clip, armor=armor, weapon=RIFLE,
             offset=CFG.get("orc_weapon_offset", off), yaw=yaw)
    skel("OrcBare", C + "Orc/SK_Orc", 0, -250, C + "Orc/Anims/A_Orc_" + CFG.get("orc_bare_clip", clip), weapon=RIFLE,
         offset=CFG.get("orc_weapon_offset", off), yaw=yaw)
    skel("Valkyrie", C + "Valkyrie/SK_Valkyrie", 0, -500, C + "Valkyrie/Anims/A_Valkyrie_" + CFG.get("valk_clip", "Idle"),
         weapon=SWORD, offset=CFG.get("sword_offset", "0 0 0 0 0 0 1"), yaw=yaw)
    skel("Spider", C + "Spider/SK_Spider", 0, -750, C + "Spider/Anims/A_Spider_" + CFG.get("spider_clip", "Walk"), yaw=yaw)
    body = static("Drone", C + "Drone/SM_Drone_Body", 0, -950, 150, yaw=180 + yaw + 90)
    for k in range(4):
        static("Drone_R%d" % k, C + "Drone/SM_Drone_Rotor_%d" % k, 0, 0, 0, parent=body, socket="Rotor_%d" % k)
    static("Drone_Eye", C + "Drone/SM_Drone_Eye", 0, 0, 0, parent=body, socket="Eye")
    sh = static("Sentinel", C + "Sentinel/SM_Sentinel_Shell", 0, -1200, 150, yaw=180 + yaw + 90)
    static("Sentinel_Core", C + "Sentinel/SM_Sentinel_Core", 0, 0, 0, parent=sh, socket="Core")


if not CFG.get("capture_only"):
    ensure_level()
    lineup()
    LES.save_current_level()
if CFG.get("build_only"):
    CFG["shots"] = []
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
out = []
for shot in CFG["shots"]:
    name, x, y, z, pitch, yaw = shot[:6]
    fov = shot[6] if len(shot) > 6 else 60
    cam = {"location": {"x": x, "y": y, "z": z}, "rotation": {"pitch": pitch, "yaw": yaw, "roll": 0},
           "projectionMode": "Perspective", "fOV": fov}
    r = await call_tool("SceneCaptureToolset.SceneCaptureToolset.Capture", world={"refPath": world.get_path_name()},
                        camera=cam, mode=CFG.get("mode", "Lit"),
                        options={"width": CFG.get("width", 1600), "height": CFG.get("height", 900),
                                 "screenPercentage": 1.0, "visualizationSubMode": ""})
    img = r["image"]
    data = img["data"] if isinstance(img, dict) else img
    with open(os.path.join(CFG["out"], name + ".png"), "wb") as f:
        f.write(base64.b64decode(data))
    out.append(name)
written = out
written
