# retarget_deaths.py - the template mannequin's other four death clips (Front_02, Front_03, Left_01, Right_01) onto the
# Robot, Orc and Valkyrie, with the retargeters retarget_enemies.py already made (they are only read here, nothing in
# them changes), so each 3D enemy can fall its own way (overnight point 3: no ragdoll, one death clip per enemy).
# Clips that already exist are left alone. Run in the Editor: node Tools/Jam/mcp_call.js py Tools/Jam/retarget_deaths.py
import unreal

EAL = unreal.EditorAssetLibrary
MANNY = "/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple"
ANIMS = "/Game/Characters/Mannequins/Anims/Death/"
CLIPS = ["MM_Death_Front_02", "MM_Death_Front_03", "MM_Death_Left_01", "MM_Death_Right_01"]
CHARS = ["Robot", "Orc", "Valkyrie"]
LOG = []


def log(m):
    LOG.append(m)
    unreal.log("retarget_deaths: " + m)


def main():
    manny = unreal.load_asset(MANNY)
    for char in CHARS:
        base = "/Game/Glyphbound/Characters/%s" % char
        mesh = unreal.load_asset("%s/SK_%s" % (base, char))
        rtg = unreal.load_asset("%s/RTG_Manny_To_%s" % (base, char))
        if mesh is None or rtg is None:
            log("%s: mesh or retargeter missing, skipped" % char)
            continue
        dest = base + "/Anims"
        todo = [c for c in CLIPS if not EAL.does_asset_exist("%s/A_%s_%s" % (dest, char, c[3:]))]
        if not todo:
            log("%s: nothing to do" % char)
            continue
        assets = [EAL.find_asset_data(ANIMS + c) for c in todo]
        made = unreal.IKRetargetBatchOperation.duplicate_and_retarget(
            assets, manny, mesh, rtg, "", "", "RTG_%s_" % char, "", dest, False, False, True)
        for ad in made:
            p = str(ad.package_name)
            name = p.rsplit("/", 1)[1].replace("RTG_%s_" % char, "")
            if name.startswith("MM_"):
                name = name[3:]
            target = "%s/A_%s_%s" % (dest, char, name)
            EAL.rename_asset(p, target)
            seq = unreal.load_asset(target)
            log("  %s (%.2f s)" % (target, seq.get_play_length() if seq else -1))
    EAL.save_directory("/Game/Glyphbound/Characters", only_if_is_dirty=True, recursive=True)


main()
"\n".join(LOG)
