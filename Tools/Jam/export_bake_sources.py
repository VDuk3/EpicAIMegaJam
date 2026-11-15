# export_bake_sources.py - exports what the pixel-art sprite baker (Tools/Blender/sprite_bake.py) renders: skeletal
# meshes, their clips (FBX, skeleton only), weapons (static FBX) and each material's base-colour texture (PNG), into
# Glyphbound/Saved/bake_src/<Character>/ with a manifest.json. Overnight point 5 (the castle's pixel-art enemies are
# rendered from animated 3D models, then reduced to the game's palette: the old way of Doom-era sprites).
# Run in the Editor: node Tools/Jam/mcp_call.js py Tools/Jam/export_bake_sources.py
import unreal, os, json

EAL = unreal.EditorAssetLibrary
OUT = os.path.join(unreal.Paths.project_saved_dir(), "bake_src")
G = "/Game/Glyphbound/Characters/"
Q = "/Game/QuadrapedCreatures/"
W = "/Game/Medieval_Weapons/Meshes/"
CHARS = {
    "Orc": {"meshes": [G + "Orc/SK_Orc"], "anim_dir": G + "Orc/Anims/", "clips": [
        "A_Orc_Idle", "A_Orc_Zombie_Idle_A", "A_Orc_Unarmed_Walk_Fwd", "A_Orc_Unarmed_Jog_Fwd", "A_Orc_Zombie_Walk_A",
        "A_Orc_StaffStrike", "A_Orc_Zombie_Attack_A", "A_Orc_Attack_01", "A_Orc_ChargedAttack", "A_Orc_StaffShot",
        "A_Orc_CastSpell", "A_Orc_MagicStrike", "A_Orc_HitReact_Front_Lgt_01", "A_Orc_Death_Front_01",
        "A_Orc_Death_Back_01", "A_Orc_Death_Left_01"]},
    "Valkyrie": {"meshes": [G + "Valkyrie/SK_Valkyrie"], "anim_dir": G + "Valkyrie/Anims/", "clips": [
        "A_Valkyrie_Idle", "A_Valkyrie_Unarmed_Walk_Fwd", "A_Valkyrie_Unarmed_Jog_Fwd", "A_Valkyrie_Attack_01",
        "A_Valkyrie_Attack_02", "A_Valkyrie_Attack_03", "A_Valkyrie_ChargedAttack", "A_Valkyrie_CastSpell",
        "A_Valkyrie_LevitatingMagicStrike", "A_Valkyrie_Levitating", "A_Valkyrie_MagicStrike",
        "A_Valkyrie_HitReact_Front_Lgt_01", "A_Valkyrie_Death_Back_01"]},
    "Barghest": {"meshes": [Q + "Barghest/Meshes/SK_BARGHEST", Q + "Barghest/Meshes/SK_BARGHEST_MANE",
                            Q + "Barghest/Meshes/SK_BARGHEST_TAIL"], "anim_dir": None, "clips": [
        "BARGHEST_idleAggressive", "BARGHEST_idleBreathe", "BARGHEST_walk", "BARGHEST_run", "BARGHEST_biteAggressive",
        "BARGHEST_jumpBiteAggressive", "BARGHEST_runBite", "BARGHEST_getHitAggressive", "BARGHEST_deathAggressive"]},
    "Centaur": {"meshes": [Q + "Centaur/Meshes/" + n for n in (
        "SK_Centaur", "SK_Body_Armor", "SK_Shoulder_Pads", "SK_Beard", "SK_Mane", "SK_Mustaches", "SK_Goaty",
        "SK_Pork_Chops", "SK_Sheath", "SK_Bow_idle", "SK_Bow_Action", "SK_Arrow_Action", "SK_Sword_Idle")],
        "anim_dir": None, "clips": [
        "ANIM_Centaur_IdleBow", "ANIM_Centaur_IdleAiming", "ANIM_Centaur_WalkBow", "ANIM_Centaur_TrotBow",
        "ANIM_Centaur_GallopBow", "ANIM_Centaur_ShootArrowToIdleAiming", "ANIM_Centaur_LegAttackBow",
        "ANIM_Centaur_GetHitBow", "ANIM_Centaur_DeathBow", "ANIM_Centaur_IdleNormalToIdleAiming"]},
}
LOG = []


def log(m):
    LOG.append(m)
    unreal.log("export_bake_sources: " + m)


def find_asset(name, cls, roots):
    reg = unreal.AssetRegistryHelpers.get_asset_registry()
    for r in roots:
        for a in reg.get_assets_by_path(r, recursive=True):
            if str(a.asset_name) == name and str(a.asset_class_path.asset_name) == cls:
                return str(a.package_name)
    return None


def export(obj, path, opts=None):
    t = unreal.AssetExportTask()
    t.set_editor_property("object", obj)
    t.set_editor_property("filename", path)
    t.set_editor_property("automated", True)
    t.set_editor_property("prompt", False)
    t.set_editor_property("replace_identical", True)
    if opts is not None:
        t.set_editor_property("options", opts)
    ok = unreal.Exporter.run_asset_export_task(t)
    if not ok:
        log("export FAILED %s" % path)
    return ok


def fbx_opts(preview_mesh=False):
    o = unreal.FbxExportOption()
    o.set_editor_property("ascii", False)
    o.set_editor_property("export_morph_targets", False)
    o.set_editor_property("export_preview_mesh", preview_mesh)
    o.set_editor_property("level_of_detail", False)
    o.set_editor_property("collision", False)
    return o


def base_color_texture(mi):
    """The texture behind a material's base colour: the first texture parameter whose name says so, else any."""
    best = None
    try:
        for p in unreal.MaterialEditingLibrary.get_texture_parameter_names(mi) if isinstance(mi, unreal.MaterialInstance) else []:
            t = unreal.MaterialEditingLibrary.get_material_instance_texture_parameter_value(mi, p)
            if t is None:
                continue
            n = (str(p) + " " + t.get_name()).lower()
            if any(k in n for k in ("basecolor", "base color", "albedo", "diffuse", "_bc", "_d.", "color")):
                return t
            best = best or t
    except Exception as e:
        log("texture params of %s: %s" % (mi.get_name(), e))
    if best is None:
        reg = unreal.AssetRegistryHelpers.get_asset_registry()
        for dep in reg.get_dependencies(mi.get_outermost().get_name(), unreal.AssetRegistryDependencyOptions()) or []:
            o = unreal.load_asset(str(dep))
            if isinstance(o, unreal.Texture2D):
                n = o.get_name().lower()
                if any(k in n for k in ("basecolor", "albedo", "diffuse", "_bc", "_d", "color")):
                    return o
                best = best or o
    return best


def main():
    man = {}
    for char, cfg in CHARS.items():
        d = os.path.join(OUT, char)
        os.makedirs(d, exist_ok=True)
        entry = {"meshes": [], "clips": {}, "textures": {}}
        for mp in cfg["meshes"]:
            m = unreal.load_asset(mp)
            if m is None:
                log("%s: no mesh %s" % (char, mp))
                continue
            fn = os.path.join(d, m.get_name() + ".fbx")
            export(m, fn, fbx_opts())
            mats = []
            for slot in m.get_editor_property("materials"):
                mi = slot.get_editor_property("material_interface")
                sname = str(slot.get_editor_property("material_slot_name"))
                tex = base_color_texture(mi) if mi else None
                tfile = None
                if tex is not None:
                    tfile = tex.get_name() + ".png"
                    if not os.path.isfile(os.path.join(d, tfile)):
                        export(tex, os.path.join(d, tfile))
                mats.append({"slot": sname, "material": mi.get_name() if mi else None, "texture": tfile})
            entry["meshes"].append({"name": m.get_name(), "fbx": os.path.basename(fn), "materials": mats})
        roots = [cfg["anim_dir"]] if cfg["anim_dir"] else [Q]
        for c in cfg["clips"]:
            p = (cfg["anim_dir"] + c) if cfg["anim_dir"] else find_asset(c, "AnimSequence", roots)
            a = unreal.load_asset(p) if p else None
            if a is None:
                log("%s: no clip %s" % (char, c))
                continue
            fn = os.path.join(d, c + ".fbx")
            export(a, fn, fbx_opts())
            entry["clips"][c] = {"fbx": os.path.basename(fn), "length": a.get_play_length()}
        man[char] = entry
        log("%s: %d meshes, %d clips" % (char, len(entry["meshes"]), len(entry["clips"])))
    wd = os.path.join(OUT, "Weapons")
    os.makedirs(wd, exist_ok=True)
    wl = []
    for name in ("SM_Mace", "SM_Axe", "SM_Staff", "SM_Sword_1", "SM_Shield_1", "SM_Spear", "SM_Bow", "SM_ProjectileArrowMesh"):
        p = find_asset(name, "StaticMesh", ["/Game/Medieval_Weapons"])
        m = unreal.load_asset(p) if p else None
        if m is None:
            log("no weapon %s" % name)
            continue
        export(m, os.path.join(wd, name + ".fbx"), fbx_opts())
        mats = []
        for slot in m.get_editor_property("static_materials"):
            mi = slot.get_editor_property("material_interface")
            tex = base_color_texture(mi) if mi else None
            tfile = None
            if tex is not None:
                tfile = tex.get_name() + ".png"
                if not os.path.isfile(os.path.join(wd, tfile)):
                    export(tex, os.path.join(wd, tfile))
            mats.append({"slot": str(slot.get_editor_property("material_slot_name")), "texture": tfile})
        wl.append({"name": name, "fbx": name + ".fbx", "materials": mats})
    sw = unreal.load_asset("/Game/Glyphbound/Weapons/ValkyrieSword/SM_ValkyrieSword")
    if sw:
        export(sw, os.path.join(wd, "SM_ValkyrieSword.fbx"), fbx_opts())
        wl.append({"name": "SM_ValkyrieSword", "fbx": "SM_ValkyrieSword.fbx", "materials": []})
    man["Weapons"] = wl
    json.dump(man, open(os.path.join(OUT, "manifest.json"), "w"), indent=1)


main()
"\n".join(LOG)
