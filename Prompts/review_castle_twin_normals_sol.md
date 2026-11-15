# Review: castle twins imported from OBJ with normals (read-only)

Editor-side build tools for the castle map of an Unreal Engine 5.9 game (Glyphbound). Report only real defects (wrong
geometry, wrong normals or winding, crashes, a walk check that can pass on a broken map) with file, line and a concrete
failure scenario. No style notes. If nothing, say so.

Context: `Tools/Jam/castle_twins.js` turns each castle-pack mesh into a plain-block "twin" (JSON with sections of
vertices, normals, UVs, triangles, plus collision boxes). Static meshes built from those in Editor Python (MeshDescription)
had all-zero normals, so lighting depended on the view. The fix writes each twin as an OBJ and imports it with the
classic FBX/OBJ importer, keeping the normals.

The change (see `git diff HEAD -- Tools/Jam` and the new file `Tools/Jam/castle_twins_obj.js`):
- `castle_twins_obj.js` (new): writes `<out>/<name>.obj`. The importer reads OBJ as right-handed and negates Y; the
  writer negates Y and reverses each triangle's vertex order, so the imported mesh keeps the twin's coordinates and
  Unreal's winding. Check that the normals and the UV v flip are consistent with that.
- `castle_twins_import.py`: imports the OBJ (AssetImportTask + FbxImportUI, IMPORT_NORMALS) into
  `/Game/Glyphbound/Castle/Pixel/Twins`, renames slots, assigns material instances, Nanite off, then box collision or
  complex-as-simple, double-sided.
- `castle_twins.js`: the "field" mode (a floor or vault as a height field) keeps at least the end cell inside the
  piece's nominal box (`nom`), because the nominal-box clamp later cut away a stair's lowest treads (a 75 cm hole at the
  foot of each flight). Check `nom` matches the clamp in `build()` (the `nb` computation).
- `Build-PastCastle.py`: twins looked up in `/Twins`; `walk_flight` gives the crypt stair walks the twin's real profile
  (flat 1.2 m top, treads, flat 1.5 m bottom) in the pixel look, the plain slope in the pack look. Check the walk
  points still cover every flight and the reversed lists for the walk back up.
