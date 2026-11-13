# Builds the shotgun's one source .blend from the owner's Tripo GLB (SourceArt/Weapons/Shotgun/model.glb).
# The GLB is one mesh of 20,957 triangles turned 11.3 deg about the vertical axis, with one 4096 colour JPG that
# carries baked light, a frost-like wear and patches where projections meet, and no roughness, metallic, normal, AO or
# emissive map. Its cables have no colour of their own and their imprint is painted on the body behind them.
# This script rebuilds the look on the same shape (the rifle's method, Tools/Blender/rifle_build_blend.py):
#   frame     the gun is turned back and centred: muzzle -Y, up +Z, X = 0 on its centre line, metres
#   geometry  vertices welded, Tripo's normals dropped, hard edges above SHARP_ANGLE, face-area weighted normals
#   cables    split from the body, one clean colour per piece (flat blocks of the atlas), smooth, one subdivision
#             level so the 4 to 6 sided strands become round, fattened back to their thickness
#   BaseColor one navy, the gun's own (the median colour of its undisturbed texels). The brightness of each texel's
#             surroundings (a median on its UV island) has no separate tones on this gun, only one broad hump: cutting
#             it into tones gave patches. It is kept as a faint shade (SHADE) instead, so the frost, the baked light
#             and the cables' imprint are gone. The small drawn details (holes, panel lines) are re-cut crisp; where
#             they are dense, the surface is the carbon weave and gets a darker tone; deep recesses go near black.
#             The circuit board and the small coloured indicators keep their own colours
#   Emissive  cut from the colour: the saturated cyan and blue texels (strips, screens' glyphs, the cell)
#   ORM       R = ambient occlusion baked in Cycles (with the cables in place: their real contact shadow),
#             G = roughness and B = metallic by tone
#   Normal    baked in Cycles (OpenGL, +Y): rounded edges (Bevel node) and the drawn details pressed in (Bump)
# Modelled here, measured on the mesh with Tools/Blender/shotgun_measure.py (ray casts):
#   cell          see CELL_* below
#   energy bar    a dark bezel with a straight glowing strip on the flat top of the receiver block, behind the sight:
#                 the game cuts the strip into 13 cells that drain with the enchant. The painted strip under it goes
#   loading port  the shotgun loads shell by shell (owner): a framed dark opening on the sloped underside in front of
#                 the trigger guard
#   pump          the forend pumps after each shot (owner): a ribbed U-shaped sleeve around the forend, which has one
#                 section all along once it is seen along its own slope. It slides back PUMP_TRAVEL and returns
#   ammo display  a 2:1 hologram pane in the sight's rear window, which faces the player
#   weak-point light  a lens on the sight's rear lower panel, under the window
#   shell         one shell, at rest inside the loading port; its "out" vector is the way out of the port
# Objects saved: Shotgun_Body (with cables, cell, energy bar and port joined in), Shotgun_Trigger, Shotgun_Pump,
# Shotgun_AmmoDisplay, Shotgun_WeakPointLight, Shotgun_Magazine (the shell). Pivots and motion are custom properties.
# Usage: blender -b --factory-startup --python shotgun_build_blend.py -- <model.glb> <out.blend> [review_dir] [tex|full]
#        [bar_receiver|bar_sight]
#        "tex" stops after the textures are computed and dumped to review_dir (no bakes, nothing saved).
#        The last argument chooses where the energy bar goes (see ENERGY_BAR_ON_SIGHT).
import bpy, bmesh, sys, os, math
import numpy as np
from numpy.lib.stride_tricks import sliding_window_view
from mathutils import Vector

argv = sys.argv[sys.argv.index("--") + 1:]
glb, out_blend = argv[0], argv[1]
review_dir = argv[2] if len(argv) > 2 else None
stage = argv[3] if len(argv) > 3 else "full"
# Where the energy bar goes. On the receiver block's top (the place the owner approved in the renders) the game's
# camera sees it almost edge-on, and lowering the gun hides it behind the 2D hands: it cannot be read in play.
# The other place is the sight's rear lower panel, which faces the player, under the weak-point lens and the ammo
# hologram: there the bar runs across the gun and the painted strip on the receiver stays as plain glow.
ENERGY_BAR_ON_SIGHT = (argv[4] if len(argv) > 4 else "bar_receiver") == "bar_sight"

SHARP_ANGLE = 30.0
BEVEL_RADIUS = 0.0008      # metres: rounded-edge width baked into the normal map
DETAIL_DEPTH = 0.0004      # metres: how deep the drawn details are pressed in
AO_DISTANCE = 0.05
BAKE_SIZE = 4096
PAD = 12                   # texels of padding around every UV island
VOXEL = 0.004              # metres: the cell of space in which small islands look for their surroundings
MEDIAN_R = 5               # texels: half window of the median that gives each texel its surroundings' brightness
SHADE = 0.03               # how much of the surroundings' brightness stays in the navy (1 = all of it)
NAVY = (0.50, 0.20)        # roughness, metallic of the body
# The carbon panels do not differ from the body in brightness but in texture: the mean deviation of a texel's
# brightness from its surroundings, over CARBON_R texels, is about 0.03 on the plain body and 0.16 to 0.20 on the weave.
CARBON = (0.50, 0.36, 0.0)          # the weave's panels: (the navy times this, roughness, metallic)
CARBON_R, CARBON_AT, CARBON_BRIGHT = 5, 0.115, 0.31   # ...and never on bright surroundings (the frost on top faces)
WEAVE_PERIOD, WEAVE_LINE = 0.0030, 0.24   # metres between the weave's lines, drawn by position; share that is line
# Parts in another tone, so the gun is not one flat navy: (the navy times this, roughness, metallic)
PART_TONES = {2: (0.70, 0.42, 0.55),    # sight: darker satin metal
              3: (0.55, 0.38, 0.70),    # trigger: steel
              4: (0.45, 0.82, 0.00)}    # butt pad: rubber
BUTT_BOX = ((-0.060, 0.410, 0.090), (0.060, 0.520, 0.320))
# The handguard's strip is on its top face; the projection spilled it onto the sides as ragged blobs. Glow there only
# on faces that look up, within GLOW_STRIP_HALF of the centre line; none on the barrel's top (frost).
GLOW_UP_ONLY = ((-0.070, -0.520, 0.185), (0.070, -0.240, 0.240))
GLOW_STRIP_HALF = 0.013
GLOW_NONE = [((-0.060, -0.262, 0.185), (0.060, -0.110, 0.270))]
# The cell. In the GLB it is a lumpy cylinder with a ragged glow, fused into the body, in a bay under the barrel
# whose two walls lean forward (the forend's rear wall 14 deg, the receiver's front wall 20.5 deg: CELL_WALLS gives
# each wall's y at the axis height and its slope, y per z). The first modelled cell was a plain cylinder with square
# ends, fatter than the bay: it cut through both walls at a slant (owner: "it lacks form, looks out of place and
# crooked"). Now: the GLB's own cell is deleted (faces within CELL_GONE of the fitted axis that face away from it),
# and a canister takes its place on the same axis: a metal collar at each end, long enough to sink into its wall,
# a thinner glass body that glows evenly, two metal bands, and four guard rods from collar to collar.
CELL_BOX = ((-0.045, -0.236, 0.098), (0.045, -0.114, 0.178))
CELL_WALLS = ((-0.2415, -0.25), (-0.1100, -0.375))
CELL_GONE = 0.0315
CELL_COLLAR, CELL_STEP, CELL_GLASS_R, CELL_BAND_R = 0.0305, 0.0285, 0.0258, 0.0276
CELL_FRONT = (-0.262, -0.2285, -0.2225)      # y: the front collar's buried end, its step, where the glass starts
CELL_REAR = (-0.1295, -0.1235, -0.092)       # y: where the glass ends, the rear collar's step, its buried end
CELL_BANDS = [(-0.192, 0.0026), (-0.160, 0.0026)]     # y of each band's centre, half width
CELL_RODS = (0.0292, 0.0016, (150.0, 210.0, 30.0, 330.0))   # radius they sit at, their own radius, angles (deg from +X)
CELL_GLASS_GLOW = 0.36
CELL_GLASS_BASE = (0.020, 0.085, 0.105)      # dark teal under the glow, so the lights still shade the glass round
# What holds it: a saddle between the canister and the barrel's underside, sunk into both walls, and a foot under each
# band, down to the keel. (x half width, z from, z to); the saddle also has its y from and to.
CELL_SADDLE = (0.011, 0.156, 0.172, -0.256, -0.112)
CELL_FOOT = (0.012, 0.098, 0.1125, 0.0040)   # ...and each foot's half length along the gun
CELL_BAY_R = 0.0345        # every leftover of the GLB's cell within this of the axis goes too (not the barrel's underside)
CELL_SIDES = 40
CELL_SKIN = (-0.009, 0.004)   # texels this far from the fitted cylinder are the GLB's cell: no details there
SMALL_ISLAND = 3000        # texels: below this an island is too small to tell a weave on its own
GLOW_ISLAND = (1200, 0.05) # an island glows if it has this many glowing texels, or this share of them
CELL_Y = (-0.2295, -0.1235)   # y range of the GLB's own cell (its texels get no details)
DEEP = (0.30, 0.65, 0.0)            # deep recesses: (the navy times this, roughness, metallic)
DEEP_AT = (0.105, 0.065)            # surroundings' brightness where the recess tone starts and where it is full
DARK_DETAIL = (0.36, 0.80, 0.0)     # (the tone's colour times this, roughness, metallic)
EMISSION = 1.0                      # Blender review only
NEON_BASE = ((0.020, 0.024, 0.030), 0.30, 0.0)
KEEP = (0.60, 0.0)                  # roughness and metallic of the texels that keep their own colour
GLOW = (0.10, 0.85, 1.00)           # the gun's own cyan (Blender review; the game sets the element's colour)

# Cable colours (sRGB, roughness, metallic), given to the cable pieces in turn: the colours the texture hints at.
CABLE_COLOURS = [
    ((0.030, 0.032, 0.036), 0.50, 0.0),   # black rubber
    ((0.560, 0.180, 0.060), 0.45, 0.0),   # copper orange
    ((0.300, 0.045, 0.040), 0.45, 0.0),   # dark red
    ((0.060, 0.170, 0.420), 0.45, 0.0),   # blue
]
CABLE_STUB = 0.06          # metres: a cable piece shorter than this is a stub, and stubs are black (as coloured blobs they drew the eye)
CABLE_KEEP = 0.55          # share of the subdivided cables' faces kept by the decimation
CABLE_FATTEN = 0.00045     # metres: pushed out along the normals after the subdivision, which thins a strand
# Welded pieces fully inside one of these boxes and longer than CABLE_MIN are cables (straightened frame).
CABLE_BOXES = [
    ((0.012, 0.025, 0.205), (0.060, 0.225, 0.282)),      # left of the receiver
    ((0.012, -0.425, 0.104), (0.072, -0.295, 0.210)),    # left of the handguard
    ((-0.072, -0.425, 0.104), (-0.012, -0.295, 0.210)),  # right of the handguard
    ((0.010, -0.030, 0.195), (0.056, 0.025, 0.250)),     # loops beside the left screen
    ((-0.045, -0.030, 0.195), (-0.015, 0.025, 0.245)),   # loop beside the right screen
]
CABLE_MIN = 0.025
# The body's texels near the cables carry their imprint (orange smears, a painted shadow): no drawn details there.
IMPRINT_GROW = 0.022
# Islands that keep their own colours: the circuit board (left of the receiver) and what is green around it.
BOARD_BOX = ((0.008, -0.030, 0.190), (0.060, 0.230, 0.285))
SIGHT_BOX = ((-0.060, -0.200, 0.225), (0.060, 0.010, 0.400))
TRIGGER_BOX = ((-0.012, 0.060, 0.110), (0.012, 0.120, 0.180))

CHAMFER = 0.0004
HAND = Vector((0.0, 0.150, 0.165))             # top of the pistol grip: the body's origin in Unreal
TRIGGER_PIVOT = Vector((0.0, 0.092, 0.171))    # the trigger's hinge, at its top

# The energy bar, on the receiver block's top: a plane rising BAR_SLOPE (z per y) through BAR_C.
BAR_C, BAR_SLOPE = Vector((0.0, 0.112, 0.30374)), 0.22
BAR_ALONG = (-0.036, 0.034)        # metres from BAR_C along the plane: front and rear ends of the bezel
BAR_HALF_W, BAR_PROUD, BAR_DEEP = 0.0065, 0.0016, 0.0030
STRIP_HALF_W, STRIP_INSET, STRIP_LIFT = 0.0030, 0.0035, 0.0002
STRIP_CELLS, CELL_GAP = 13, 0.12   # as the game's material draws them; here only for the review material
BAR_CLEAR = ((-0.018, 0.066, 0.288), (0.018, 0.158, 0.318))   # the painted strip under the bar: its glow goes

# The loading port, on the sloped underside in front of the trigger guard: a plane rising PORT_SLOPE through PORT_C.
PORT_C, PORT_SLOPE = Vector((0.0, -0.030, 0.1025)), 0.39
PORT_HALF = (0.034, 0.0135)        # half length (along the slope) and half width of the frame
PORT_BAR, PORT_PROUD = 0.0030, 0.0014
# The shell: at rest inside the port, base to the rear. Its way out: back along the port and down out of it.
SHELL_R, SHELL_LEN, SHELL_BASE, SHELL_DEEP = 0.0095, 0.058, 0.011, 0.013
SHELL_OUT = (0.040, 0.035)         # metres: rearwards along the port's plane, and out of it

# The pump: the forend's section is the same from y = -0.385 to -0.265 when seen along its own slope (the underside
# rises PUMP_SLOPE). PUMP_SECTION is the forend's right half, (x, height above the underside's centre line), in metres.
PUMP_C, PUMP_SLOPE = Vector((0.0, -0.349, 0.06047)), 0.214
PUMP_SECTION = [(0.0, 0.0), (0.020, 0.0013), (0.030, 0.0037), (0.036, 0.0076), (0.040, 0.0112), (0.044, 0.0176),
                (0.046, 0.0276), (0.0495, 0.0376), (0.0495, 0.042)]
PUMP_HALF_LEN, PUMP_CLEAR, PUMP_THICK = 0.031, 0.0008, 0.0038
PUMP_RIBS, PUMP_RIB_LEN, PUMP_RIB_PITCH, PUMP_RIB_PROUD = 5, 0.0050, 0.0110, 0.0013
PUMP_TRAVEL = 0.042                # metres back along the slope, then forward again

# The ammo display: a 2:1 hologram pane in the sight's rear window (66 x 40 mm inside, its glass 4 to 10 mm behind
# the pane), tilted up towards the player's eye.
SCREEN_C = Vector((0.0085, -0.0385, 0.3695))
SCREEN_TILT = 8.0
SCREEN_HALF = (0.0250, 0.0125)
# The weak-point light: a lens on the sight's rear lower panel, a plane leaning LIGHT_LEAN (y per z) through LIGHT_C.
LIGHT_C, LIGHT_LEAN = Vector((0.0085, -0.0063, 0.329)), -0.3375
# With the energy bar on that panel: the lens moves up, and the bar lies under it, across the gun. Distances up the
# panel from LIGHT_C, half sizes (across the gun, up the panel), in metres.
PANEL_LENS = (0.0050, (0.0170, 0.0026))
PANEL_BAR = (-0.0065, (0.0240, 0.0036))
PANEL_STRIP_HALF = (0.0210, 0.0020)
LIGHT_HALF, LIGHT_SLAB = (0.017, 0.0035), (-0.0020, 0.0026)

# Found by the architect's own zone-by-zone review of the renders, before the owner saw them:
# - the carbon panels had ragged outlines, bites of navy inside them and scraps on the tubes. The weave is now kept
#   only on side faces inside CARBON_ZONES, and each panel's outline is tidied on a flat grid of the gun's side
#   (CARBON_GRID metres per cell): closed over CARBON_CLOSE, then opened over CARBON_OPEN;
# - the stock's comb strip had a torn blob at its front end, in the player's view: glow there only on faces that look
#   up, within the strip's own width (measured from its texels);
# - single dark specks on the sight: a drawn detail now needs neighbours;
# - the circuit board's own colours were a blurred smudge beside crisp parts: nothing keeps its own colours any more;
# - blotches on the lower tube's front end: no drawn details there.
CARBON_ZONES = [((0.015, -0.500, 0.135), (0.075, -0.255, 0.215)),    # handguard side panels (left; mirrored for the right)
                ((0.004, 0.190, 0.030), (0.075, 0.470, 0.330))]      # stock side panels
CARBON_GRID, CARBON_CLOSE, CARBON_OPEN = 0.00075, 0.0050, 0.0035
# ...and then each panel's outline is traced and simplified to a polygon whose corners keep within CARBON_STRAIGHT of
# it, so its edges are straight like the source's panels; panels under CARBON_MIN_AREA (square metres) are dropped.
CARBON_STRAIGHT, CARBON_MIN_AREA = 0.0050, 0.0004
# The polygons come from the left side, where the weave is found best, and the right side takes the same ones
# (the gun's sides mirror each other). What was found as weave but falls outside them gets no drawn details either:
# those were the scribbles left around the panels.
COMB_STRIP = ((-0.030, 0.190, 0.285), (0.030, 0.480, 0.335))
KEEP_OWN_COLOURS = False
NO_DETAIL_BOXES = [((-0.070, -0.520, 0.000), (0.070, -0.440, 0.120))]
# The small panel on the left of the receiver, near the stock, carried one more painted screen sign: it becomes a
# small plate with the border and the sigil's diamonds.
SMALL_PLATE_BOX = ((0.020, 0.070, 0.240), (0.050, 0.140, 0.295))

# The side plates. The GLB has a slab on each side of the receiver with a painted data screen (owner: "remove that
# panel that simulates a screen; put some design, something that is not electronic"). The slab stays; its face is
# redrawn by position, in the frame of the face's own four edges (the first version took one tilt for the whole face
# from its texels' spread, and its border sat crooked inside the slab: the face is not a rectangle, its front edge
# leans more than the others; owner: "straighten it, aligned with the rectangle around it"). Each edge is a line
# fitted to that side of the texels' outline, and the border keeps a constant distance from all four: an engraved
# border, a bolt in each corner, an engraved sigil inlaid with the glow (an abstract
# mark: a diamond, a smaller one inside, a bar on each side), and three slanted slots. PLATE_BOX holds the left face;
# the right one is its mirror. Lengths in metres on the plate, u along it towards the rear, v up.
PLATE_BOX = ((0.036, -0.135, 0.145), (0.050, 0.000, 0.232))
PLATE_BORDER, PLATE_LINE = 0.0042, 0.0010        # the border's distance from the plate's edge; engraved line width
PLATE_BOLT = (0.0085, 0.0024)                    # bolts: distance from the edges, radius
PLATE_SIGIL = (-0.016, 0.0, 0.0150, 0.0105, 0.0014)   # centre u, v; half height, half width of the diamond; line width
PLATE_SLOTS = (0.021, 0.0105, 0.0125, 0.0022, 3)      # first slot's u; pitch; half length; width; how many (parallel to the rear edge)
PLATE_INLAY = 0.80                               # how brightly the sigil's inlay glows (emissive mask)

# Flat tones of the modelled parts (sRGB, roughness, metallic), each on its own block of the atlas.
DARK_POLYMER = ((0.062, 0.100, 0.135), 0.75, 0.0)   # pump
DARK_METAL = ((0.085, 0.120, 0.160), 0.40, 0.60)    # bar bezel, port frame
BLACK = ((0.012, 0.014, 0.018), 0.60, 0.0)          # the port's opening

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=glb)
scene = bpy.context.scene
SRC = [o for o in scene.objects if o.type == "MESH"][0]
for o in scene.objects:
    o.select_set(False)
SRC.select_set(True)
bpy.context.view_layer.objects.active = SRC
bpy.ops.object.parent_clear(type="CLEAR_KEEP_TRANSFORM")
SRC.rotation_mode = "XYZ"
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
for o in [o for o in scene.objects if o.type != "MESH"]:
    bpy.data.objects.remove(o)
me = SRC.data
color_im = next(i for i in bpy.data.images if i.size[0] > 0)
UV_NAME = me.uv_layers[0].name

# ---------------------------------------------------------------- frame: turn the gun back, centre it
co = np.empty(len(me.vertices) * 3, np.float64)
me.vertices.foreach_get("co", co)
co = co.reshape(-1, 3)
width, yaw = min((np.ptp(co[:, 0] * math.cos(a) - co[:, 1] * math.sin(a)), a) for a in np.radians(np.arange(-25, 25, 0.05)))
x = co[:, 0] * math.cos(yaw) - co[:, 1] * math.sin(yaw)
y = co[:, 0] * math.sin(yaw) + co[:, 1] * math.cos(yaw)
co[:, 0], co[:, 1] = x - (x.min() + x.max()) / 2, y
me.vertices.foreach_set("co", co.ravel())
me.update()
print("FRAME yaw %.2f deg, width %.1f mm, y %.4f .. %.4f, z %.4f .. %.4f" %
      (math.degrees(yaw), width * 1000, co[:, 1].min(), co[:, 1].max(), co[:, 2].min(), co[:, 2].max()))


def load(im):
    w, h = im.size
    a = np.empty(w * h * 4, np.float32)
    im.pixels.foreach_get(a)
    return a.reshape(h, w, 4)


def new_image(name, rgb, srgb):
    h, w = rgb.shape[:2]
    im = bpy.data.images.get(name) or bpy.data.images.new(name, w, h, alpha=False)
    im.colorspace_settings.name = "sRGB" if srgb else "Non-Color"
    a = np.ones((h, w, 4), np.float32)
    a[..., :3] = rgb if rgb.ndim == 3 else rgb[..., None]
    im.pixels.foreach_set(np.ascontiguousarray(a).ravel())
    im.update()
    im.pack()
    return im


def smoothstep(e0, e1, x):
    t = np.clip((x - e0) / (e1 - e0), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def box_blur(a, r):
    p = np.pad(a, ((r + 1, r), (r + 1, r)), mode="edge").astype(np.float64)
    s = p.cumsum(0).cumsum(1)
    k = 2 * r + 1
    return ((s[k:, k:] - s[:-k, k:] - s[k:, :-k] + s[:-k, :-k]) / (k * k)).astype(np.float32)


def median_sep(a, r):
    """Median along the rows, then along the columns: close to a square median, at a fraction of the cost."""
    p = np.pad(a, ((0, 0), (r, r)), mode="edge")
    a = np.median(sliding_window_view(p, 2 * r + 1, axis=1), axis=-1)
    p = np.pad(a, ((r, r), (0, 0)), mode="edge")
    return np.median(sliding_window_view(p, 2 * r + 1, axis=0), axis=-1).astype(np.float32)


def shifted(a, dy, dx):
    out = np.zeros_like(a)
    h, w = a.shape[:2]
    out[max(dy, 0):h + min(dy, 0), max(dx, 0):w + min(dx, 0)] = a[max(-dy, 0):h + min(-dy, 0), max(-dx, 0):w + min(-dx, 0)]
    return out


def fill_from(valid, a, iters):
    """Grows the valid values of a into the texels around them."""
    a, v = a.copy(), valid.copy()
    for _ in range(iters):
        if v.all():
            break
        for dy, dx in ((0, 1), (0, -1), (1, 0), (-1, 0)):
            take = ~v & shifted(v, dy, dx)
            a[take] = shifted(a, dy, dx)[take]
            v |= take
    return a, v


def components(b):
    """Connected face groups of a bmesh, as lists of face indices."""
    b.faces.ensure_lookup_table()
    b.faces.index_update()
    owner = np.full(len(b.faces), -1, np.int32)
    out = []
    for f in b.faces:
        if owner[f.index] >= 0:
            continue
        cid = len(out)
        stack, comp = [f], []
        owner[f.index] = cid
        while stack:
            a = stack.pop()
            comp.append(a.index)
            for e in a.edges:
                for o in e.link_faces:
                    if owner[o.index] < 0:
                        owner[o.index] = cid
                        stack.append(o)
        out.append(comp)
    return out, owner


def inside(lo, hi, blo, bhi):
    return all(blo[i] <= lo[i] and hi[i] <= bhi[i] for i in range(3))


# ---------------------------------------------------------------- UV islands (loose parts of the raw mesh) and pieces
bm = bmesh.new()
bm.from_mesh(me)
islands, face_island = components(bm)
n_faces = len(bm.faces)
tag = bm.faces.layers.int.new("orig")
for f in bm.faces:
    f[tag] = f.index
welded = bm.copy()
bmesh.ops.remove_doubles(welded, verts=welded.verts[:], dist=1e-5)
wtag = welded.faces.layers.int["orig"]
pieces, _ = components(welded)

# Parts by welded piece: 0 body, 1 cable, 2 sight, 3 trigger, 4 butt pad. Cable pieces are numbered (strand) to take a colour each.
face_part = np.zeros(n_faces, np.int32)
face_strand = np.full(n_faces, -1, np.int32)
cable_bounds = []
order = sorted(range(len(pieces)), key=lambda i: -len(pieces[i]))
for rank, pi in enumerate(order):
    comp = pieces[pi]
    vs = {v for i in comp for v in welded.faces[i].verts}
    lo = [min(v.co[k] for v in vs) for k in range(3)]
    hi = [max(v.co[k] for v in vs) for k in range(3)]
    orig = [welded.faces[i][wtag] for i in comp]
    if rank == 0:
        continue
    if max(hi[k] - lo[k] for k in range(3)) > CABLE_MIN and any(inside(lo, hi, *b) for b in CABLE_BOXES):
        face_part[orig] = 1
        long_cables = sum(1 for b_ in cable_bounds if max(b_[1][k] - b_[0][k] for k in range(3)) >= CABLE_STUB)
        is_stub = max(hi[k] - lo[k] for k in range(3)) < CABLE_STUB
        face_strand[orig] = 0 if is_stub else long_cables          # the colour it takes: stubs black, the rest in turn
        cable_bounds.append((lo, hi))
    elif len(comp) > 500 and inside(lo, hi, *SIGHT_BOX):
        face_part[orig] = 2
    elif inside(lo, hi, *TRIGGER_BOX):
        face_part[orig] = 3
    elif len(comp) > 300 and inside(lo, hi, *BUTT_BOX):
        face_part[orig] = 4
welded.free()
bm.free()
print("ISLANDS", len(islands), "PIECES", len(pieces), "cable pieces", len(cable_bounds), "cable faces", int((face_part == 1).sum()),
      "sight faces", int((face_part == 2).sum()), "trigger faces", int((face_part == 3).sum()),
      "butt pad faces", int((face_part == 4).sum()))
if not (face_part == 1).any() or not (face_part == 2).any() or not (face_part == 3).any():
    raise SystemExit("shotgun_build_blend: cables, sight or trigger not found")

# ---------------------------------------------------------------- the atlas: which island, where on the gun, facing where
col = load(color_im)[..., :3]
H, W = col.shape[:2]
lum = col[..., 0] * 0.2126 + col[..., 1] * 0.7152 + col[..., 2] * 0.0722

n_loops = len(me.loops)
uv = np.empty(n_loops * 2, np.float32)
me.uv_layers[0].uv.foreach_get("vector", uv)
uv = uv.reshape(n_loops, 2)
loop_start = np.empty(len(me.polygons), np.int32)
me.polygons.foreach_get("loop_start", loop_start)
loop_vert = np.empty(n_loops, np.int32)
me.loops.foreach_get("vertex_index", loop_vert)
loop_co = co.astype(np.float32)[loop_vert]
face_n = np.empty(len(me.polygons) * 3, np.float32)
me.polygons.foreach_get("normal", face_n)
face_n = face_n.reshape(-1, 3)
face_dir = (np.abs(face_n).argmax(1) * 2 + (np.take_along_axis(face_n, np.abs(face_n).argmax(1)[:, None], 1)[:, 0] > 0)).astype(np.uint8)

ids = np.full((H, W), -1, np.int32)
pos = np.zeros((H, W, 3), np.float32)
part = np.zeros((H, W), np.uint8)
facing = np.zeros((H, W), np.uint8)
for f in range(len(me.polygons)):
    p = uv[loop_start[f]:loop_start[f] + 3] * (W, H)
    x0, x1 = max(int(math.floor(p[:, 0].min() - 1)), 0), min(int(math.ceil(p[:, 0].max() + 1)), W)
    y0, y1 = max(int(math.floor(p[:, 1].min() - 1)), 0), min(int(math.ceil(p[:, 1].max() + 1)), H)
    if x1 <= x0 or y1 <= y0:
        continue
    d = (p[1, 0] - p[0, 0]) * (p[2, 1] - p[0, 1]) - (p[2, 0] - p[0, 0]) * (p[1, 1] - p[0, 1])
    if abs(d) < 1e-9:
        continue
    px = (np.arange(x0, x1) + 0.5)[None, :]
    py = (np.arange(y0, y1) + 0.5)[:, None]
    w1 = ((px - p[0, 0]) * (p[2, 1] - p[0, 1]) - (p[2, 0] - p[0, 0]) * (py - p[0, 1])) / d
    w2 = ((p[1, 0] - p[0, 0]) * (py - p[0, 1]) - (px - p[0, 0]) * (p[1, 1] - p[0, 1])) / d
    m = (w1 >= 0) & (w2 >= 0) & (w1 + w2 <= 1)
    ids[y0:y1, x0:x1][m] = face_island[f]
    part[y0:y1, x0:x1][m] = face_part[f]
    facing[y0:y1, x0:x1][m] = face_dir[f]
    c = loop_co[loop_start[f]:loop_start[f] + 3]
    pos[y0:y1, x0:x1][m] = (c[0] + w1[..., None] * (c[1] - c[0]) + w2[..., None] * (c[2] - c[0]))[m]
covered = ids >= 0
print("ATLAS covered %.1f%%" % (100 * covered.mean()))


def in_box(lo, hi):
    return covered & np.all((pos >= np.asarray(lo, np.float32)) & (pos <= np.asarray(hi, np.float32)), axis=2)


# ---------------------------------------------------------------- what each texel is, from its colour
mx, mn = col.max(axis=2), col.min(axis=2)
sat = (mx - mn) / np.maximum(mx, 1e-3)
glow_raw = (covered & (part != 1) & (mx > 0.55) & (col[..., 0] < 0.52 * mx) & (sat > 0.50)).astype(np.float32)
small_sign = glow_raw.copy()
glow_raw *= box_blur(glow_raw, 2) > 0.42          # the frost has cyan specks: a glowing texel has glowing neighbours
glow_raw *= box_blur(glow_raw, 6) > 0.35          # ...and belongs to a shape of some size, not to a blob
# The handguard's strip is on its top face; the projection spilled it onto the sides as ragged blobs.
glow_raw[in_box(*GLOW_UP_ONLY) & ((facing != 5) | (np.abs(pos[..., 0]) > GLOW_STRIP_HALF))] = 0.0
for lo_, hi_ in GLOW_NONE + ([] if ENERGY_BAR_ON_SIGHT else [BAR_CLEAR]):
    glow_raw[in_box(lo_, hi_)] = 0.0
green = (col[..., 1] > col[..., 0] * 1.12) & (col[..., 1] > col[..., 2] * 1.08) & (sat > 0.2)
warm = (col[..., 0] > col[..., 2] * 1.10) & (sat > 0.2) & (mx > 0.12)

# Islands that keep their own colours: mostly green or warm, on the body, in the circuit board's zone; and on the
# sight, the small coloured indicators.
n_isl = len(islands)
flat_ids = ids[covered]


def share(mask):
    return np.bincount(flat_ids, weights=mask[covered].astype(np.float64), minlength=n_isl) / \
        np.maximum(np.bincount(flat_ids, minlength=n_isl), 1)


# Glow only on islands that really glow: a strip, a screen, the cell. A blob on a large plain island is frost.
isl_glow = np.bincount(flat_ids, weights=glow_raw[covered].astype(np.float64), minlength=n_isl)
isl_texels = np.bincount(flat_ids, minlength=n_isl)
glows = (isl_glow >= GLOW_ISLAND[0]) | (isl_glow / np.maximum(isl_texels, 1) >= GLOW_ISLAND[1])
glow_raw *= glows[np.where(covered, ids, 0)]
isl_colour = share(green | warm)
isl_green = share(green)
isl_board = share(in_box(*BOARD_BOX) & (part == 0))
isl_sight = share(part == 2)
keep_isl = ((isl_board > 0.9) & (isl_green > 0.06)) | ((isl_sight > 0.9) & (isl_colour > 0.35))
if not KEEP_OWN_COLOURS:
    keep_isl[:] = False
keep = covered & keep_isl[np.where(covered, ids, 0)]
# The comb strip: its own centre and width from its texels, then nothing glows outside that band or on other faces.
on_comb = in_box(*COMB_STRIP) & (facing == 5) & (glow_raw > 0.5)
if on_comb.sum() > 2000:
    cx_ = pos[..., 0][on_comb]
    comb_c = float(np.median(cx_))
    comb_w = float(np.percentile(cx_, 80) - np.percentile(cx_, 20)) / 0.6
    glow_raw[in_box(*COMB_STRIP) & ((facing != 5) | (np.abs(pos[..., 0] - comb_c) > comb_w / 2 + 0.0004))] = 0.0
    print("COMB strip centre x %.1f mm, width %.1f mm" % (comb_c * 1000, comb_w * 1000))
print("KEEP islands", int(keep_isl.sum()), "texels", int(keep.sum()), "GLOW texels", int(glow_raw.sum()))

# The cables' imprint on the body: no drawn details near the cables.
plain = np.zeros((H, W), bool)
g = IMPRINT_GROW
for lo, hi in cable_bounds:
    plain |= in_box([v - g for v in lo], [v + g for v in hi])
for lo, hi in NO_DETAIL_BOXES:
    plain |= in_box(lo, hi)
plain &= part != 1

# ---------------------------------------------------------------- each texel's surroundings: a median on its island
neutral = covered & (glow_raw < 0.5) & ~keep
ys, xs = np.nonzero(covered)
y0s, y1s = np.full(n_isl, H, np.int64), np.zeros(n_isl, np.int64)
x0s, x1s = np.full(n_isl, W, np.int64), np.zeros(n_isl, np.int64)
np.minimum.at(y0s, flat_ids, ys)
np.maximum.at(y1s, flat_ids, ys)
np.minimum.at(x0s, flat_ids, xs)
np.maximum.at(x1s, flat_ids, xs)
around = np.zeros((H, W), np.float32)
weave_isl = np.zeros((H, W), np.float32)
mgn = MEDIAN_R + 2
for i in range(n_isl):
    if y1s[i] < y0s[i]:
        continue
    sl = (slice(max(y0s[i] - mgn, 0), min(y1s[i] + mgn + 1, H)), slice(max(x0s[i] - mgn, 0), min(x1s[i] + mgn + 1, W)))
    mask = ids[sl] == i
    valid = mask & neutral[sl]
    if valid.sum() < 4:
        around[sl][mask] = np.median(lum[sl][mask])
        continue
    filled, got = fill_from(valid, np.where(valid, lum[sl], 0.0), mgn + 8)
    filled[~got] = np.median(lum[sl][valid])
    ar = median_sep(filled, MEDIAN_R)
    around[sl][mask] = ar[mask]
    # The weave, decided on the island alone (a blur across islands would mix unrelated neighbours of the atlas),
    # then smoothed twice so a panel has an even outline and no holes.
    wv = (box_blur(np.abs(filled / np.maximum(ar, 0.03) - 1.0), CARBON_R) > CARBON_AT) & (ar < CARBON_BRIGHT)
    wv = box_blur(wv.astype(np.float32), 6) > 0.5
    wv &= box_blur(wv.astype(np.float32), 20) > 0.35          # a panel, not a patch
    weave_isl[sl][mask] = box_blur(wv.astype(np.float32), 8)[mask]
print("AROUND done")

sel = neutral & (part != 1)
# Small islands take the weave of the surface around them: the mean over the texels of large islands in the same small
# cell of space (VOXEL) that face the same way.
big = isl_texels[np.where(covered, ids, 0)] >= SMALL_ISLAND
v_ = np.floor(pos[sel] / VOXEL).astype(np.int64) + 512
key = ((v_[:, 0] * 1024 + v_[:, 1]) * 1024 + v_[:, 2]) * 6 + facing[sel]
_, inv = np.unique(key, return_inverse=True)
w_big = big[sel].astype(np.float64)
v_sum = np.bincount(inv, weights=weave_isl[sel] * w_big)
v_cnt = np.bincount(inv, weights=w_big)
around_weave = np.where(v_cnt > 0, v_sum / np.maximum(v_cnt, 1), -1.0)[inv]
own_weave = weave_isl[sel]
weave_isl[sel] = np.where((w_big > 0) | (around_weave < 0), own_weave, around_weave)
print("WEAVE small-island texels that took their surroundings' value: %d" % int(((w_big == 0) & (around_weave >= 0)).sum()))
hist, edges = np.histogram(around[sel], bins=40, range=(0.0, 0.8))
print("BRIGHTNESS of the surroundings (share of the body's texels per bin):")
for h_, e in zip(hist, edges):
    print("   %.2f %5.1f%% %s" % (e, 100 * h_ / sel.sum(), "#" * int(200 * h_ / sel.sum())))
undisturbed = sel & (np.abs(lum / np.maximum(around, 0.02) - 1.0) < 0.10)
navy = np.median(col[undisturbed], axis=0)
mid = float(np.median(around[sel]))
print("NAVY colour %.3f %.3f %.3f, median brightness of the surroundings %.3f" % (*navy, mid))

# ---------------------------------------------------------------- drawn details, re-cut crisp; the weave; recesses
ratio = lum / np.maximum(around, 0.03)
# Details from a slightly blurred brightness: a line survives it, the frost's single dark specks do not.
dark_all = smoothstep(0.74, 0.56, box_blur(lum, 1) / np.maximum(around, 0.03)) * neutral * (part != 1)
dark_all *= box_blur(dark_all, 2) > 0.20          # a line survives this, a single speck does not


def grey(a, r, fn):
    """Minimum or maximum over a square of half size r cells."""
    p_ = np.pad(a, ((r, r), (0, 0)), mode="edge")
    a = fn(sliding_window_view(p_, 2 * r + 1, axis=0), axis=-1)
    p_ = np.pad(a, ((0, 0), (r, r)), mode="edge")
    return fn(sliding_window_view(p_, 2 * r + 1, axis=1), axis=-1)


def simplify(chain, tol):
    """Douglas-Peucker on an open chain of 2D points."""
    if len(chain) < 3:
        return chain
    a, b = chain[0], chain[-1]
    pts = np.array(chain[1:-1], np.float64)
    ab = np.array(b, np.float64) - np.array(a, np.float64)
    length = math.hypot(ab[0], ab[1])
    rel = pts - np.array(a, np.float64)
    d = np.abs(rel[:, 0] * ab[1] - rel[:, 1] * ab[0]) / length if length > 1e-9 else np.hypot(rel[:, 0], rel[:, 1])
    k = int(d.argmax())
    if d[k] <= tol:
        return [a, b]
    return simplify(chain[:k + 2], tol)[:-1] + simplify(chain[k + 1:], tol)


def outlines(mask, tol, min_cells):
    """The mask's shapes as polygons with straight edges: lists of (x, y) corners in cells. Cells are unit squares."""
    m = np.pad(mask, 1)
    inner = m[1:-1, 1:-1]
    edges = {}

    def add(a, b):
        edges.setdefault(a, []).append(b)

    rim = inner & ~(m[:-2, 1:-1] & m[2:, 1:-1] & m[1:-1, :-2] & m[1:-1, 2:])
    for i, j in zip(*np.nonzero(rim)):
        i, j = int(i), int(j)
        if not m[i + 1, j]:
            add((i, j), (i + 1, j))                 # the side below: the inside stays on the left of every edge
        if not m[i + 2, j + 1]:
            add((i + 1, j), (i + 1, j + 1))
        if not m[i + 1, j + 2]:
            add((i + 1, j + 1), (i, j + 1))
        if not m[i, j + 1]:
            add((i, j + 1), (i, j))
    polys = []
    while edges:
        start = next(iter(edges))
        loop, cur = [start], start
        while True:
            nxt = edges[cur].pop()
            if not edges[cur]:
                del edges[cur]
            if nxt == start or nxt not in edges:
                break
            loop.append(nxt)
            cur = nxt
        xs_, ys_ = np.array([q[0] for q in loop], np.float64), np.array([q[1] for q in loop], np.float64)
        area = 0.5 * float(np.sum(xs_ * np.roll(ys_, -1) - np.roll(xs_, -1) * ys_))
        if area < min_cells:                         # holes run the other way (negative), scraps are small
            continue
        far = int(((xs_ - xs_[0]) ** 2 + (ys_ - ys_[0]) ** 2).argmax())
        polys.append(simplify(loop[:far + 1], tol)[:-1] + simplify(loop[far:] + [loop[0]], tol)[:-1])
    return polys


def draw_polygons(polys, ny, nz):
    out = np.zeros((ny, nz), bool)
    gx, gy = np.arange(ny)[:, None] + 0.5, np.arange(nz)[None, :] + 0.5
    for poly in polys:
        inside_ = np.zeros((ny, nz), bool)
        for (x1, y1), (x2, y2) in zip(poly, poly[1:] + poly[:1]):
            if y1 != y2:
                inside_ ^= ((y1 > gy) != (y2 > gy)) & (gx < (x2 - x1) * (gy - y1) / (y2 - y1) + x1)
        out |= inside_
    return out


carbon = np.zeros((H, W), np.float32)
panel_sides = np.zeros((H, W), bool)          # the faces that carry panels: no "deep recess" tone on them
r_close, r_open = int(round(CARBON_CLOSE / CARBON_GRID)), int(round(CARBON_OPEN / CARBON_GRID))
for zlo, zhi in CARBON_ZONES:
    ny, nz = int((zhi[1] - zlo[1]) / CARBON_GRID) + 2, int((zhi[2] - zlo[2]) / CARBON_GRID) + 2
    polys = None
    for side, facing_bin in ((1.0, 1), (-1.0, 0)):           # the left side first: its polygons serve both
        lo_ = (min(side * zlo[0], side * zhi[0]), zlo[1], zlo[2])
        hi_ = (max(side * zlo[0], side * zhi[0]), zhi[1], zhi[2])
        zone = in_box(lo_, hi_) & sel & (part == 0) & ~plain
        found = zone & (facing == facing_bin)
        gy = ((pos[..., 1][found] - lo_[1]) / CARBON_GRID).astype(np.int64)
        gz = ((pos[..., 2][found] - lo_[2]) / CARBON_GRID).astype(np.int64)
        flat = gy * nz + gz
        count = np.bincount(flat, minlength=ny * nz).reshape(ny, nz)
        total = np.bincount(flat, weights=(weave_isl[found] > 0.5).astype(np.float64), minlength=ny * nz).reshape(ny, nz)
        known = count > 0
        grid, _ = fill_from(known, np.where(known, total / np.maximum(count, 1), 0.0).astype(np.float32), 12)
        seen = grey((grid > 0.3).astype(np.float32), 4, np.max) > 0.5      # everything that looked like weave, grown a little
        if polys is None:
            panel = (grid > 0.5).astype(np.float32)
            panel = grey(grey(panel, r_close, np.max), r_close, np.min)       # closed: bites and holes filled
            panel = grey(grey(panel, r_open, np.min), r_open, np.max)         # opened: spurs and scraps gone
            polys = outlines(panel > 0.5, CARBON_STRAIGHT / CARBON_GRID, CARBON_MIN_AREA / CARBON_GRID ** 2)
            print("CARBON zone y %.2f..%.2f: %d panels, corners %s" % (zlo[1], zhi[1], len(polys), [len(q) for q in polys]))
        straight = draw_polygons(polys, ny, nz)
        soft = box_blur(straight.astype(np.float32), 1)
        # every face of this side's skin takes the panel, whichever way it looks, except those that look inwards
        target = zone & (facing != (1 - facing_bin))
        ty = np.clip(((pos[..., 1][target] - lo_[1]) / CARBON_GRID).astype(np.int64), 0, ny - 1)
        tz = np.clip(((pos[..., 2][target] - lo_[2]) / CARBON_GRID).astype(np.int64), 0, nz - 1)
        carbon[target] = soft[ty, tz]
        panel_sides |= target & (facing == facing_bin)
        scraps = np.zeros((H, W), bool)
        scraps[target] = (seen & ~straight)[ty, tz]
        dark_all[scraps] = 0.0
        print("CARBON side %+d: %.0f cm2 of weave, %d texels of scraps cleared" %
              (side, soft.sum() * CARBON_GRID ** 2 * 1e4, int(scraps.sum())))
# The weave, drawn by where each texel is on the gun: two sets of diagonal lines on the plane the face looks along.
axis = facing // 2
pu = np.where(axis == 0, pos[..., 1], pos[..., 0])
pv = np.where(axis == 2, pos[..., 1], pos[..., 2])


def lines(t):
    d = np.abs((t / WEAVE_PERIOD) % 1.0 - 0.5) * 2.0          # 0 on a line's centre, 1 midway between two lines
    return 1.0 - smoothstep(WEAVE_LINE * 0.6, WEAVE_LINE * 1.4, d)


weave = np.maximum(lines((pu + pv) * 0.7071), lines((pu - pv) * 0.7071)) * carbon
dark = np.maximum(dark_all * ~plain * (1.0 - carbon), weave)
deep = smoothstep(DEEP_AT[0], DEEP_AT[1], around) * neutral * (part != 1) * (1.0 - carbon) * ~panel_sides
neon_a = smoothstep(0.35, 0.65, box_blur(glow_raw, 1)) * covered
# The cell, by position.
in_cell_box = in_box(*CELL_BOX) & (part == 0)
lit = in_cell_box & (glow_raw > 0.5)
if lit.sum() < 1000:
    raise SystemExit("shotgun_build_blend: the cell's glow was not found")
# A circle through the glowing texels, seen along the gun (x, z): least squares, twice, dropping what is off it.
cx_, cz_ = pos[..., 0][lit].astype(np.float64), pos[..., 2][lit].astype(np.float64)
for _ in range(3):
    sol = np.linalg.lstsq(np.stack([2 * cx_, 2 * cz_, np.ones_like(cx_)], axis=1), cx_ ** 2 + cz_ ** 2, rcond=None)[0]
    cell_r = math.sqrt(sol[2] + sol[0] ** 2 + sol[1] ** 2)
    off = np.abs(np.sqrt((cx_ - sol[0]) ** 2 + (cz_ - sol[1]) ** 2) - cell_r)
    cx_, cz_ = cx_[off < 0.004], cz_[off < 0.004]
print("CELL axis x %.1f mm z %.1f mm, radius %.1f mm, %d of %d glowing texels on it, y %s" %
      (sol[0] * 1000, sol[1] * 1000, cell_r * 1000, len(cx_), lit.sum(),
       " ".join("%.4f" % v_ for v_ in np.percentile(pos[..., 1][lit], (2, 98)))))
radial = np.sqrt((pos[..., 0] - sol[0]) ** 2 + (pos[..., 2] - sol[1]) ** 2)
cell = in_cell_box & (radial - cell_r > CELL_SKIN[0]) & (radial - cell_r < CELL_SKIN[1]) & \
    (pos[..., 1] > CELL_Y[0]) & (pos[..., 1] < CELL_Y[1]) & (box_blur(lit.astype(np.float32), 12) > 0.05)
neon_a[in_cell_box] = 0.0
dark[cell] = 0.0
deep[cell] = 1.0          # the GLB's own cell, under the modelled one
CELL_AXIS, CELL_R = (float(sol[0]), float(sol[1])), float(cell_r)
print("CELL texels under the modelled cylinder", int(cell.sum()))
print("CARBON %.1f%% of the body's texels, DEEP %.1f%%, DARK details %.1f%%" %
      (100 * carbon[sel].mean(), 100 * deep[sel].mean(), 100 * dark[sel].mean()))


def layer(base, value, alpha):
    return base + (np.asarray(value, np.float32) - base) * (alpha[..., None] if base.ndim == 3 else alpha)


def band(d, width):
    """1 on a line of this width around d = 0, 0 away from it, a quarter millimetre of soft edge."""
    return 1.0 - smoothstep(width / 2 - 0.00025, width / 2 + 0.00025, np.abs(d))


def segment(u, v, a, b):
    """Distance from (u, v) to the segment a-b."""
    au, av, bu, bv = a[0], a[1], b[0], b[1]
    t = np.clip(((u - au) * (bu - au) + (v - av) * (bv - av)) / ((bu - au) ** 2 + (bv - av) ** 2), 0.0, 1.0)
    return np.sqrt((u - au - t * (bu - au)) ** 2 + (v - av - t * (bv - av)) ** 2)


def convex_hull(pts):
    """The outline of a set of 2D points, counter-clockwise (Andrew's monotone chain)."""
    p_ = pts[np.lexsort((pts[:, 1], pts[:, 0]))]
    p_ = [(float(a), float(b)) for a, b in p_]

    def turn(o, a, b):
        return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])

    lower, upper = [], []
    for q in p_:
        while len(lower) >= 2 and turn(lower[-2], lower[-1], q) <= 0:
            lower.pop()
        lower.append(q)
    for q in reversed(p_):
        while len(upper) >= 2 and turn(upper[-2], upper[-1], q) <= 0:
            upper.pop()
        upper.append(q)
    return np.array(lower[:-1] + upper[:-1], np.float64)


def plate_edges(plate):
    """The face's texels in a rough frame of their own (u along the face, v up), and its four edges: for each of
    bottom, rear, top and front, a point on the edge, its direction and its normal towards the inside."""
    yz = np.stack([pos[..., 1][plate], pos[..., 2][plate]], axis=1).astype(np.float64)
    centre = yz.mean(axis=0)
    _, vecs = np.linalg.eigh(np.cov((yz - centre).T))
    ax_u = vecs[:, 1] * (1.0 if vecs[0, 1] > 0 else -1.0)
    ax_v = np.array([-ax_u[1], ax_u[0]])
    P = np.stack([(yz - centre) @ ax_u, (yz - centre) @ ax_v], axis=1)
    hull = convex_hull(P)
    nxt = np.roll(hull, -1, axis=0)
    d = nxt - hull
    length = np.hypot(d[:, 0], d[:, 1])
    n_out = np.stack([d[:, 1], -d[:, 0]], axis=1) / np.maximum(length, 1e-12)[:, None]
    edges = {}
    for name, want in (("bottom", (0.0, -1.0)), ("rear", (1.0, 0.0)), ("top", (0.0, 1.0)), ("front", (-1.0, 0.0))):
        pick = (n_out @ np.array(want) > 0.75) & (length > 1e-5)      # the corners' chamfers face neither way enough
        q = np.concatenate([hull[pick], nxt[pick]])
        w = np.concatenate([length[pick], length[pick]])
        c = (q * w[:, None]).sum(axis=0) / w.sum()
        _, ev = np.linalg.eigh(((q - c) * w[:, None]).T @ (q - c))
        direction = ev[:, 1]
        normal = np.array([-direction[1], direction[0]])
        if normal @ (-c) < 0:                                         # the texels' centre is the origin: inside
            normal = -normal
        world = ax_u * direction[0] + ax_v * direction[1]
        angle = math.degrees(math.atan2(world[1], world[0])) % 180.0
        edges[name] = (c, direction, normal, angle if angle < 90 else angle - 180.0)
    return P, edges


def corner_inset(e1, e2, inset):
    """The point this far inside both edges."""
    a_ = np.array([e1[2], e2[2]])
    return np.linalg.solve(a_, np.array([e1[0] @ e1[2] + inset, e2[0] @ e2[2] + inset]))


def draw_plate(plate, sigil, slots, bolts, name):
    """Redraws a plate's face: an engraved border at a constant distance from the face's four edges, bolts in its
    corners, the sigil (centre u, v, half height, half width, line; with or without its side bars) inlaid with the
    glow, slots. The sigil and the slots stand on the bottom edge's direction."""
    P, edges = plate_edges(plate)
    order = ("bottom", "rear", "top", "front")
    corners = [corner_inset(edges[a_], edges[b_], 0.0) for a_, b_ in zip(order, order[1:] + order[:1])]
    origin = np.mean(corners, axis=0)
    dir_u = edges["bottom"][1] * (1.0 if edges["bottom"][1][0] > 0 else -1.0)
    dir_v = np.array([-dir_u[1], dir_u[0]])
    U, V = (P - origin) @ dir_u, (P - origin) @ dir_v
    rear_dir = edges["rear"][1] * (1.0 if edges["rear"][1] @ dir_v > 0 else -1.0)
    rear_uv = np.array([rear_dir @ dir_u, rear_dir @ dir_v])
    print("PLATE %s: %d texels; edges' angles to the gun's axis: bottom %.1f, top %.1f, rear %.1f, front %.1f deg; "
          "bottom %.1f mm long, rear %.1f mm" %
          (name, plate.sum(), edges["bottom"][3], edges["top"][3], edges["rear"][3], edges["front"][3],
           np.linalg.norm(corners[0] - corners[3]) * 1000, np.linalg.norm(corners[1] - corners[0]) * 1000))
    inside_ = np.minimum.reduce([(P - edges[k][0]) @ edges[k][2] for k in order])     # distance to the nearest edge
    engraved = band(inside_ - PLATE_BORDER, PLATE_LINE)
    if bolts:
        for a_, b_ in zip(order, order[1:] + order[:1]):
            at = corner_inset(edges[a_], edges[b_], PLATE_BOLT[0]) - origin
            bu, bv = at @ dir_u, at @ dir_v
            r_ = np.sqrt((U - bu) ** 2 + (V - bv) ** 2)
            engraved = np.maximum(engraved, band(r_ - PLATE_BOLT[1], PLATE_LINE))
            engraved = np.maximum(engraved, band(segment(U, V, (bu - 0.0015, bv), (bu + 0.0015, bv)), 0.0007))
    if slots:
        first, pitch, half_len, width, count = PLATE_SLOTS
        for k in range(count):
            cu = first + k * pitch
            engraved = np.maximum(engraved, band(segment(U, V, (cu - rear_uv[0] * half_len, -rear_uv[1] * half_len),
                                                         (cu + rear_uv[0] * half_len, rear_uv[1] * half_len)), width))
    su_, sv_, sh, sw, line, bars = sigil
    scale = sh * sw / math.hypot(sh, sw)                        # turns the diamond's |u|/w + |v|/h - 1 into a distance
    d_outer = (np.abs(U - su_) / sw + np.abs(V - sv_) / sh - 1.0) * scale
    d_inner = (np.abs(U - su_) / (sw * 0.36) + np.abs(V - sv_) / (sh * 0.36) - 1.0) * scale * 0.36
    inlay = np.maximum(band(d_outer, line), 1.0 - smoothstep(-0.00025, 0.00025, d_inner))
    if bars:
        for sgn in (-1, 1):
            inlay = np.maximum(inlay, band(segment(U, V, (su_ + sgn * (sw + 0.0030), sv_), (su_ + sgn * (sw + 0.0095), sv_)), line))
    dark[plate] = engraved
    carbon[plate] = 0.0
    deep[plate] = 0.0
    neon_a[plate] = inlay * PLATE_INLAY
    keep[plate] = False


for side, facing_bin in ((1.0, 1), (-1.0, 0)):
    lo_ = (min(side * PLATE_BOX[0][0], side * PLATE_BOX[1][0]), PLATE_BOX[0][1], PLATE_BOX[0][2])
    hi_ = (max(side * PLATE_BOX[0][0], side * PLATE_BOX[1][0]), PLATE_BOX[1][1], PLATE_BOX[1][2])
    cand = in_box(lo_, hi_) & (part == 0) & (facing == facing_bin)
    if cand.sum() < 5000:
        raise SystemExit("shotgun_build_blend: side plate not found on side %+d" % side)
    plane = float(np.median(pos[..., 0][cand]))
    draw_plate(cand & (np.abs(pos[..., 0] - plane) < 0.0012), PLATE_SIGIL + (True,), True, True, "side %+d (plane x %.1f mm)" % (side, plane * 1000))

# The small plate: the face that carried the painted sign (its plane is where that sign's glow was).
cand = in_box(*SMALL_PLATE_BOX) & (part == 0) & (facing == 1)
seed = cand & (small_sign > 0.5)
if seed.sum() < 100:
    raise SystemExit("shotgun_build_blend: the small panel's sign was not found")
plane = float(np.median(pos[..., 0][seed]))
small = cand & (np.abs(pos[..., 0] - plane) < 0.0012)
draw_plate(small, (0.0, 0.0, 0.0085, 0.0060, 0.0011, False), False, False, "small (plane x %.1f mm)" % (plane * 1000))

shade = 1.0 + SHADE * np.clip(around / mid - 1.0, -1.0, 1.0)
base = navy[None, None, :] * shade[..., None]
rough = np.full((H, W), NAVY[0], np.float32)
metal = np.full((H, W), NAVY[1], np.float32)
for code, (k_, r_, m_) in PART_TONES.items():
    here = part == code
    base[here] *= k_
    rough[here] = r_
    metal[here] = m_
for (k_, r_, m_), a in ((CARBON, carbon), (DEEP, deep), (DARK_DETAIL, dark)):
    base = layer(base, base * k_, a)
    rough = layer(rough, r_, a)
    metal = layer(metal, m_, a)
own_colour = np.stack([box_blur(col[..., k], 1) for k in range(3)], axis=2)
base[keep] = own_colour[keep]
rough[keep] = KEEP[0]
metal[keep] = KEEP[1]
base = layer(base, NEON_BASE[0], neon_a)
rough = layer(rough, NEON_BASE[1], neon_a)
metal = layer(metal, NEON_BASE[2], neon_a)
height = 1.0 - np.maximum(dark, neon_a * 0.6)

# ---------------------------------------------------------------- padding, and flat blocks for the cables
src = np.where(covered, np.arange(H * W, dtype=np.int64).reshape(H, W), -1)
got = covered.copy()
for _ in range(PAD):
    for dy, dx in ((0, 1), (0, -1), (1, 0), (-1, 0)):
        take = ~got & shifted(got, dy, dx)
        src[take] = shifted(src, dy, dx)[take]
        got |= take
flat_src = src[got]


def padded(a, empty):
    out = np.full(a.shape, empty, np.float32) if a.ndim == 2 else np.full(a.shape, empty, np.float32)
    out[got] = a.reshape(H * W, -1)[flat_src].reshape((-1,) + a.shape[2:]) if a.ndim == 3 else a.ravel()[flat_src]
    return out


base, rough, metal = padded(base, 0.1), padded(rough, 0.6), padded(metal, 0.0)
height, neon_a = padded(height, 1.0), padded(neon_a, 0.0)
used = got.copy()
BLOCKS = []


def claim_block(what):
    for reach in (26, 20, 14):
        free = box_blur((~used).astype(np.float32), reach) > 0.9999
        free[:48, :] = free[-48:, :] = False
        free[:, :48] = free[:, -48:] = False
        if free.any():
            break
    else:
        raise SystemExit("shotgun_build_blend: ERROR no free block in the atlas for " + what)
    fy, fx = np.argwhere(free)[0]
    half = reach - 6
    block = (slice(fy - half, fy + half + 1), slice(fx - half, fx + half + 1))
    used[fy - reach:fy + reach + 1, fx - reach:fx + reach + 1] = True
    BLOCKS.append(block)
    print("BLOCK for", what, "at texel", int(fx), int(fy), "half size", half)
    return block, ((fx + 0.5) / W, (fy + 0.5) / H)


_, CELL_GLASS_UV = claim_block("the cell's glass")
neon_a[BLOCKS[-1]] = CELL_GLASS_GLOW
base[BLOCKS[-1]], rough[BLOCKS[-1]], metal[BLOCKS[-1]], height[BLOCKS[-1]] = CELL_GLASS_BASE, 0.12, 0.0, 1.0
_, STRIP_UV = claim_block("the energy strip")
neon_a[BLOCKS[-1]] = 1.0
base[BLOCKS[-1]], rough[BLOCKS[-1]], metal[BLOCKS[-1]], height[BLOCKS[-1]] = NEON_BASE[0], NEON_BASE[1], 0.0, 1.0
FLAT_UV = {}
for name_, (rgb, r_, m_) in (("polymer", DARK_POLYMER), ("metal", DARK_METAL), ("black", BLACK)):
    block, FLAT_UV[name_] = claim_block("dark " + name_)
    base[block], rough[block], metal[block], height[block], neon_a[block] = rgb, r_, m_, 1.0, 0.0
CABLE_UV = []
for k, (rgb, r_, m_) in enumerate(CABLE_COLOURS):
    block, uv_c = claim_block("cable colour %d" % k)
    base[block], rough[block], metal[block], height[block], neon_a[block] = rgb, r_, m_, 1.0, 0.0
    CABLE_UV.append(uv_c)

im_base = new_image("T_Shotgun_BaseColor", base, True)
im_height = new_image("Shotgun_Height", height, False)
im_mask = new_image("T_Shotgun_EmissiveMask", np.repeat(neon_a[..., None], 3, 2), False)


def dump(im_or_array, name, srgb=True):
    if not review_dir:
        return
    os.makedirs(review_dir, exist_ok=True)
    im = im_or_array if isinstance(im_or_array, bpy.types.Image) else new_image("dump_" + name, im_or_array, srgb)
    c = im.copy()
    c.scale(2048, 2048)
    c.filepath_raw = os.path.join(review_dir, name + ".png")
    c.file_format = "PNG"
    c.save()
    bpy.data.images.remove(c)
    if im is not im_or_array:
        bpy.data.images.remove(im)


dump(im_base, "T_Shotgun_BaseColor")
dump(im_mask, "T_Shotgun_EmissiveMask")
if stage == "tex":
    dump(np.repeat((around * covered)[..., None], 3, 2), "debug_around")
    dump(np.stack([dark, carbon, np.maximum(keep.astype(np.float32), deep)], axis=2), "debug_details", False)
    print("TEX stage done")
    sys.exit(0)

# ---------------------------------------------------------------- geometry
attr = me.attributes.new("part", "INT", "FACE")
attr.data.foreach_set("value", face_part)
attr = me.attributes.new("strand", "INT", "FACE")
attr.data.foreach_set("value", face_strand)
bpy.ops.object.mode_set(mode="EDIT")
bpy.ops.mesh.select_all(action="SELECT")
bpy.ops.mesh.remove_doubles(threshold=1e-5)
bpy.ops.mesh.select_all(action="DESELECT")
bpy.ops.object.mode_set(mode="OBJECT")
# The GLB's own cell goes: body faces in the bay, near the fitted axis, that face away from it (the barrel's underside
# and the walls face towards it and stay).
n_poly = len(me.polygons)
p_c = np.empty(n_poly * 3, np.float32)
me.polygons.foreach_get("center", p_c)
p_c = p_c.reshape(-1, 3)
p_n = np.empty(n_poly * 3, np.float32)
me.polygons.foreach_get("normal", p_n)
p_n = p_n.reshape(-1, 3)
p_part = np.empty(n_poly, np.int32)
me.attributes["part"].data.foreach_get("value", p_part)
rx, rz = p_c[:, 0] - CELL_AXIS[0], p_c[:, 2] - CELL_AXIS[1]
rr = np.sqrt(rx ** 2 + rz ** 2)
outward = (p_n[:, 0] * rx + p_n[:, 2] * rz) / np.maximum(rr, 1e-6)
wall_f = CELL_WALLS[0][0] + CELL_WALLS[0][1] * (p_c[:, 2] - CELL_AXIS[1])
wall_r = CELL_WALLS[1][0] + CELL_WALLS[1][1] * (p_c[:, 2] - CELL_AXIS[1])
in_bay = (p_part == 0) & (p_c[:, 1] > wall_f + 0.003) & (p_c[:, 1] < wall_r - 0.003)
gone = in_bay & (rr < CELL_GONE) & (outward > 0.15)
barrel_underside = (outward < -0.3) & (p_c[:, 2] > 0.160)
gone |= in_bay & (rr < CELL_BAY_R) & (p_c[:, 2] > 0.114) & ~barrel_underside
me.polygons.foreach_set("select", gone.tolist())
bpy.ops.object.mode_set(mode="EDIT")
bpy.ops.mesh.delete(type="FACE")
bpy.ops.object.mode_set(mode="OBJECT")
print("CELL faces of the GLB's own cell deleted:", int(gone.sum()))
bpy.ops.mesh.customdata_custom_splitnormals_clear()
me.polygons.foreach_set("use_smooth", [True] * len(me.polygons))
me.set_sharp_from_angle(angle=math.radians(SHARP_ANGLE))


def face_values(mesh, name):
    a = np.empty(len(mesh.polygons), np.int32)
    mesh.attributes[name].data.foreach_get("value", a)
    return a


def only(ob):
    for o in scene.objects:
        o.select_set(False)
    ob.select_set(True)
    bpy.context.view_layer.objects.active = ob


def separate(code, name):
    only(SRC)
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="DESELECT")
    bpy.ops.object.mode_set(mode="OBJECT")
    me.polygons.foreach_set("select", (face_values(me, "part") == code).tolist())
    bpy.ops.object.mode_set(mode="EDIT")
    before = set(scene.objects)
    bpy.ops.mesh.separate(type="SELECTED")
    bpy.ops.object.mode_set(mode="OBJECT")
    new = next(o for o in scene.objects if o not in before)
    new.name = name
    new.data.name = name
    return new


def weighted_normals(ob):
    only(ob)
    mod = ob.modifiers.new("WeightedNormal", "WEIGHTED_NORMAL")
    mod.mode = "FACE_AREA"
    mod.weight = 50
    mod.keep_sharp = True
    bpy.ops.object.modifier_apply(modifier=mod.name)


CABLES = separate(1, "Shotgun_Cables")
weighted_normals(SRC)

# Cables: one flat colour per piece, no hard edges, round.
cm = CABLES.data
strand = face_values(cm, "strand")
c_loop_total = np.empty(len(cm.polygons), np.int32)
cm.polygons.foreach_get("loop_total", c_loop_total)
c_uv = np.repeat(np.array(CABLE_UV, np.float32)[strand % len(CABLE_UV)], c_loop_total, axis=0)
cm.uv_layers[0].uv.foreach_set("vector", c_uv.ravel())
cm.set_sharp_from_angle(angle=math.radians(180.0))
only(CABLES)
bpy.ops.object.mode_set(mode="EDIT")
bpy.ops.mesh.select_all(action="SELECT")
bpy.ops.mesh.tris_convert_to_quads(face_threshold=math.radians(60), shape_threshold=math.radians(60))
bpy.ops.object.mode_set(mode="OBJECT")
sub = CABLES.modifiers.new("Round", "SUBSURF")
sub.levels = 1
sub.uv_smooth = "NONE"
bpy.ops.object.modifier_apply(modifier=sub.name)
dec = CABLES.modifiers.new("Lighter", "DECIMATE")
dec.ratio = CABLE_KEEP
bpy.ops.object.modifier_apply(modifier=dec.name)
dis = CABLES.modifiers.new("Fatten", "DISPLACE")
dis.strength = CABLE_FATTEN
dis.mid_level = 0.0
bpy.ops.object.modifier_apply(modifier=dis.name)
cm.polygons.foreach_set("use_smooth", [True] * len(cm.polygons))
print("CABLES tris", sum(len(p.vertices) - 2 for p in cm.polygons))


def build_cell():
    """The canister: rings along Y joined into tubes, each tube's UVs on its flat block; then the guard rods."""
    b = bmesh.new()
    ax, az = CELL_AXIS
    metal_uv, glass_uv = FLAT_UV["metal"], CELL_GLASS_UV
    c = 0.0007                                             # chamfer of the collars' and bands' corners
    # (y, radius) corners of the outline from front to rear, and what the stretch after each corner is made of
    outline = [(CELL_FRONT[0], CELL_COLLAR, "m"), (CELL_FRONT[1] - c, CELL_COLLAR, "m"), (CELL_FRONT[1], CELL_COLLAR - c, "m"),
               (CELL_FRONT[1], CELL_STEP, "m"), (CELL_FRONT[2] - c, CELL_STEP, "m"), (CELL_FRONT[2], CELL_STEP - c, "m"),
               (CELL_FRONT[2], CELL_GLASS_R, "g")]
    for centre, half in sorted(CELL_BANDS):
        outline += [(centre - half, CELL_GLASS_R, "m"), (centre - half, CELL_BAND_R - c, "m"), (centre - half + c, CELL_BAND_R, "m"),
                    (centre + half - c, CELL_BAND_R, "m"), (centre + half, CELL_BAND_R - c, "m"), (centre + half, CELL_GLASS_R, "g")]
    outline += [(CELL_REAR[0], CELL_GLASS_R, "m"), (CELL_REAR[0], CELL_STEP - c, "m"), (CELL_REAR[0] + c, CELL_STEP, "m"),
                (CELL_REAR[1], CELL_STEP, "m"), (CELL_REAR[1], CELL_COLLAR - c, "m"), (CELL_REAR[1] + c, CELL_COLLAR, "m"),
                (CELL_REAR[2], CELL_COLLAR, "m")]

    def loop_at(cx, cz, y_, r_, sides):
        return [b.verts.new((cx + r_ * math.cos(2 * math.pi * k / sides), y_, cz + r_ * math.sin(2 * math.pi * k / sides)))
                for k in range(sides)]

    def tube(a_, c_, uv_c):
        faces = []
        for k in range(len(a_)):
            j = (k + 1) % len(a_)
            faces.append(b.faces.new((a_[k], a_[j], c_[j], c_[k])))
        set_uv(b, faces, uv_c)

    rings = [loop_at(ax, az, y_, r_, CELL_SIDES) for y_, r_, _ in outline]
    for (a_, c_), (_, _, kind) in zip(zip(rings, rings[1:]), outline):
        tube(a_, c_, glass_uv if kind == "g" else metal_uv)
    set_uv(b, [b.faces.new(rings[0]), b.faces.new(list(reversed(rings[-1])))], metal_uv)
    rod_at, rod_r, angles = CELL_RODS
    for deg in angles:
        cx, cz = ax + rod_at * math.cos(math.radians(deg)), az + rod_at * math.sin(math.radians(deg))
        a_, c_ = loop_at(cx, cz, CELL_FRONT[1] - 0.002, rod_r, 8), loop_at(cx, cz, CELL_REAR[1] + 0.002, rod_r, 8)
        tube(a_, c_, metal_uv)
        set_uv(b, [b.faces.new(a_), b.faces.new(list(reversed(c_)))], metal_uv)
    world_axes = (Vector((1.0, 0.0, 0.0)), Vector((0.0, 1.0, 0.0)), Vector((0.0, 0.0, 1.0)))
    centre_line = Vector((ax, 0.0, 0.0))
    hw, z0, z1, y0, y1 = CELL_SADDLE
    box(b, centre_line, *world_axes, (-hw, y0, z0), (hw, y1, z1), metal_uv)
    hw, z0, z1, half = CELL_FOOT
    for centre, _ in CELL_BANDS:
        box(b, centre_line, *world_axes, (-hw, centre - half, z0), (hw, centre + half, z1), metal_uv)
    bmesh.ops.recalc_face_normals(b, faces=b.faces[:])
    bmesh.ops.triangulate(b, faces=b.faces[:])
    for f in b.faces:
        f.smooth = True
    new_me = bpy.data.meshes.new("Shotgun_Cell")
    b.to_mesh(new_me)
    b.free()
    new_me.uv_layers[0].name = UV_NAME
    new_me.materials.append(me.materials[0])
    ob = bpy.data.objects.new("Shotgun_Cell", new_me)
    scene.collection.objects.link(ob)
    new_me.set_sharp_from_angle(angle=math.radians(SHARP_ANGLE))
    print("MODELLED Shotgun_Cell tris", len(new_me.polygons), "glass radius %.1f mm, collars %.1f mm" %
          (CELL_GLASS_R * 1000, CELL_COLLAR * 1000))
    return ob


def flat_material(name, rgb_srgb, roughness, metallic, glow=None, alpha=1.0):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    b = next(n for n in m.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
    b.inputs["Base Color"].default_value = tuple(c ** 2.2 for c in rgb_srgb) + (1.0,)
    b.inputs["Roughness"].default_value = roughness
    b.inputs["Metallic"].default_value = metallic
    b.inputs["Alpha"].default_value = alpha
    if glow is not None:
        b.inputs["Emission Color"].default_value = tuple(glow[:3]) + (1.0,)
        b.inputs["Emission Strength"].default_value = glow[3]
    return m


def set_uv(b, faces, uv_c):
    layer_uv = b.loops.layers.uv.verify()
    for f in faces:
        for l_ in f.loops:
            l_[layer_uv].uv = uv_c


def box(b, origin, ax, ay, az, lo, hi, uv_c):
    """A closed box in the frame (ax, ay, az) at origin, from lo to hi along each axis; its UVs on one flat block."""
    vs = [b.verts.new(origin + ax * x + ay * y + az * z)
          for z in (lo[2], hi[2]) for y in (lo[1], hi[1]) for x in (lo[0], hi[0])]
    faces = [b.faces.new([vs[i] for i in q]) for q in
             ((0, 1, 3, 2), (4, 6, 7, 5), (0, 4, 5, 1), (2, 3, 7, 6), (0, 2, 6, 4), (1, 5, 7, 3))]
    set_uv(b, faces, uv_c)
    return faces


def quad(b, origin, ax, ay, az, lo, hi, lift, uv_rect):
    """A quad in the (ax, ay) plane, lifted along az; uv_rect = (u0, v0, u1, v1) from lo to hi."""
    layer_uv = b.loops.layers.uv.verify()
    pts = [(lo[0], lo[1]), (hi[0], lo[1]), (hi[0], hi[1]), (lo[0], hi[1])]
    f = b.faces.new([b.verts.new(origin + ax * x + ay * y + az * lift) for x, y in pts])
    f.normal_update()
    if f.normal.dot(az) < 0:
        f.normal_flip()
    for loop in f.loops:
        local = loop.vert.co - origin
        s_ = (local.dot(ax) - lo[0]) / (hi[0] - lo[0])
        t_ = (local.dot(ay) - lo[1]) / (hi[1] - lo[1])
        loop[layer_uv].uv = (uv_rect[0] + (uv_rect[2] - uv_rect[0]) * s_, uv_rect[1] + (uv_rect[3] - uv_rect[1]) * t_)
    return f


def extrude(b, outline, origin, ax, ay, az, z0, z1, uv_c):
    """A closed prism: the 2D outline (in the ax, ay plane) from z0 to z1 along az. The outline may be concave."""
    a_ = [b.verts.new(origin + ax * x + ay * y + az * z0) for x, y in outline]
    c_ = [b.verts.new(origin + ax * x + ay * y + az * z1) for x, y in outline]
    n = len(outline)
    faces = [b.faces.new((a_[i], a_[(i + 1) % n], c_[(i + 1) % n], c_[i])) for i in range(n)]
    faces += [b.faces.new(a_), b.faces.new(list(reversed(c_)))]
    set_uv(b, faces, uv_c)
    return faces


def finish(b, name, materials, solids, chamfer=True, smooth=True):
    """bmesh -> object. solids: faces of the closed boxes and prisms, whose edges get a real chamfer."""
    b.loops.layers.uv.verify()
    bmesh.ops.recalc_face_normals(b, faces=solids)
    if chamfer and solids:
        edges = list({e for f in solids for e in f.edges})
        bmesh.ops.bevel(b, geom=edges, offset=CHAMFER, offset_type="OFFSET", segments=2, profile=0.5,
                        affect="EDGES", clamp_overlap=True)
    bmesh.ops.triangulate(b, faces=b.faces[:])
    for f in b.faces:
        f.smooth = smooth
    new_me = bpy.data.meshes.new(name)
    b.to_mesh(new_me)
    b.free()
    new_me.uv_layers[0].name = UV_NAME
    for m in materials:
        new_me.materials.append(m)
    ob = bpy.data.objects.new(name, new_me)
    scene.collection.objects.link(ob)
    if smooth:
        weighted_normals(ob)
    else:
        new_me.set_sharp_from_angle(angle=math.radians(SHARP_ANGLE))
    print("MODELLED", name, "tris", len(new_me.polygons))
    return ob


def slope_frame(slope):
    """Axes of a plane that rises 'slope' (z per y): along it towards the rear, and out of it upwards."""
    along = Vector((0.0, 1.0, slope)).normalized()
    return along, Vector((0.0, -along.z, along.y))


X_AXIS = Vector((1.0, 0.0, 0.0))
ATLAS = me.materials[0]
BAR_AX, BAR_UP = slope_frame(BAR_SLOPE)
STRIP_S = (BAR_ALONG[0] + STRIP_INSET, BAR_ALONG[1] - STRIP_INSET)
PANEL_UP = Vector((0.0, LIGHT_LEAN, 1.0)).normalized()          # up the sight's rear lower panel
PANEL_OUT = Vector((0.0, PANEL_UP.z, -PANEL_UP.y))                # out of it, towards the player
PLAYER_RIGHT = Vector((-1.0, 0.0, 0.0))
# The strip as modelled: the middle of its two ends (the game drains it from the first towards the second), the
# direction out of its face, and its half width. The export derives the second UV map from these.
if ENERGY_BAR_ON_SIGHT:
    STRIP_MID = LIGHT_C + PANEL_UP * PANEL_BAR[0] + PANEL_OUT * (LIGHT_SLAB[1] + STRIP_LIFT)
    STRIP_FROM, STRIP_TO = STRIP_MID + PLAYER_RIGHT * PANEL_STRIP_HALF[0], STRIP_MID - PLAYER_RIGHT * PANEL_STRIP_HALF[0]
    STRIP_NORMAL, STRIP_HALF = PANEL_OUT, PANEL_STRIP_HALF[1]
else:
    STRIP_FROM, STRIP_TO = (BAR_C + BAR_AX * s_ + BAR_UP * (BAR_PROUD + STRIP_LIFT) for s_ in STRIP_S)
    STRIP_NORMAL, STRIP_HALF = BAR_UP, STRIP_HALF_W
PORT_AX, PORT_UP = slope_frame(PORT_SLOPE)
PORT_OUT = -PORT_UP                                  # the port is on the underside: out of it is downwards
PUMP_AX, PUMP_UP = slope_frame(PUMP_SLOPE)


def build_extras():
    """Energy bar and loading port: one object, joined into the body after the bakes."""
    b = bmesh.new()
    u, v = STRIP_UV
    h = 6.0 / W
    if ENERGY_BAR_ON_SIGHT:
        at = LIGHT_C + PANEL_UP * PANEL_BAR[0]
        solids = box(b, at, PLAYER_RIGHT, PANEL_UP, PANEL_OUT, (-PANEL_BAR[1][0], -PANEL_BAR[1][1], LIGHT_SLAB[0]),
                     (PANEL_BAR[1][0], PANEL_BAR[1][1], LIGHT_SLAB[1]), FLAT_UV["metal"])
        quad(b, at, PLAYER_RIGHT, PANEL_UP, PANEL_OUT, (-PANEL_STRIP_HALF[0], -PANEL_STRIP_HALF[1]),
             (PANEL_STRIP_HALF[0], PANEL_STRIP_HALF[1]), LIGHT_SLAB[1] + STRIP_LIFT, (u - h, v - h, u + h, v + h))
    else:
        solids = box(b, BAR_C, X_AXIS, BAR_AX, BAR_UP, (-BAR_HALF_W, BAR_ALONG[0], -BAR_DEEP),
                     (BAR_HALF_W, BAR_ALONG[1], BAR_PROUD), FLAT_UV["metal"])
        quad(b, BAR_C, X_AXIS, BAR_AX, BAR_UP, (-STRIP_HALF_W, STRIP_S[0]), (STRIP_HALF_W, STRIP_S[1]),
             BAR_PROUD + STRIP_LIFT, (u - h, v - h, u + h, v + h))
    # the port: four bars round a dark opening
    hl, hw, bar = PORT_HALF[0], PORT_HALF[1], PORT_BAR
    for lo, hi in (((-hw, -hl), (hw, -hl + bar)), ((-hw, hl - bar), (hw, hl)),
                   ((-hw, -hl + bar), (-hw + bar, hl - bar)), ((hw - bar, -hl + bar), (hw, hl - bar))):
        solids += box(b, PORT_C, X_AXIS, PORT_AX, PORT_OUT, (lo[0], lo[1], -0.003), (hi[0], hi[1], PORT_PROUD), FLAT_UV["metal"])
    ub, vb = FLAT_UV["black"]
    quad(b, PORT_C, X_AXIS, PORT_AX, PORT_OUT, (-hw + bar, -hl + bar), (hw - bar, hl - bar), 0.0004,
         (ub - h, vb - h, ub + h, vb + h))
    return finish(b, "Shotgun_Extras", [ATLAS], solids)


def offset_path(path, d):
    """The open path pushed out by d: each point along the mean of its two segments' outward normals."""
    out = []
    for i, pt in enumerate(path):
        n = Vector((0.0, 0.0))
        for a_, c_ in ((i - 1, i), (i, i + 1)):
            if 0 <= a_ and c_ < len(path):
                seg = (Vector(path[c_]) - Vector(path[a_])).normalized()
                n += Vector((seg.y, -seg.x))
        n.normalize()
        out.append((pt[0] + n.x * d, pt[1] + n.y * d))
    return out


def build_pump():
    """The pump: a U-shaped sleeve under and beside the forend, with ribs. Its origin is its rest place."""
    b = bmesh.new()
    path = [(-x, h_) for x, h_ in reversed(PUMP_SECTION[1:])] + list(PUMP_SECTION)      # left top, round, right top
    uv_c = FLAT_UV["polymer"]

    def sleeve(inner, outer, s0, s1):
        outline = offset_path(path, outer) + list(reversed(offset_path(path, inner)))
        return extrude(b, outline, PUMP_C, X_AXIS, PUMP_UP, PUMP_AX, s0, s1, uv_c)

    solids = sleeve(PUMP_CLEAR, PUMP_CLEAR + PUMP_THICK, -PUMP_HALF_LEN, PUMP_HALF_LEN)
    for k in range(PUMP_RIBS):
        centre = (k - (PUMP_RIBS - 1) / 2) * PUMP_RIB_PITCH
        solids += sleeve(PUMP_CLEAR + PUMP_THICK - 0.0004, PUMP_CLEAR + PUMP_THICK + PUMP_RIB_PROUD,
                         centre - PUMP_RIB_LEN / 2, centre + PUMP_RIB_LEN / 2)
    return finish(b, "Shotgun_Pump", [ATLAS], solids)


def build_display():
    """The hologram pane. UV: U to the viewer's right, V up in Blender (down in Unreal), so a texture reads upright."""
    b = bmesh.new()
    t = math.radians(SCREEN_TILT)
    right, s_up, s_out = Vector((-1.0, 0.0, 0.0)), Vector((0.0, -math.sin(t), math.cos(t))), Vector((0.0, math.cos(t), math.sin(t)))
    quad(b, SCREEN_C, right, s_up, s_out, (-SCREEN_HALF[0], -SCREEN_HALF[1]), (SCREEN_HALF[0], SCREEN_HALF[1]),
         0.0, (0.0, 0.0, 1.0, 1.0))
    holo = flat_material("Holo_AmmoDisplay", (0.02, 0.05, 0.05), 0.3, 0.0, glow=(GLOW[0], GLOW[1], GLOW[2], 0.25))
    return finish(b, "Shotgun_AmmoDisplay", [holo], [], chamfer=False, smooth=False)


def build_light():
    """The weak-point light: a lens on the sight's rear lower panel. Dark until the game flashes it."""
    b = bmesh.new()
    up = Vector((0.0, LIGHT_LEAN, 1.0)).normalized()
    out = Vector((0.0, up.z, -up.y))
    at, half = (LIGHT_C + up * PANEL_LENS[0], PANEL_LENS[1]) if ENERGY_BAR_ON_SIGHT else (LIGHT_C, LIGHT_HALF)
    solids = box(b, at, Vector((-1.0, 0.0, 0.0)), up, out, (-half[0], -half[1], LIGHT_SLAB[0]),
                 (half[0], half[1], LIGHT_SLAB[1]), (0.5, 0.5))
    lens = flat_material("Shotgun_Light", (0.05, 0.07, 0.08), 0.15, 0.0, glow=(GLOW[0], GLOW[1], GLOW[2], 0.0))
    return finish(b, "Shotgun_WeakPointLight", [lens], solids)


def build_shell():
    """One shell, base to the rear, at rest inside the port. Three slots: hull, brass base, glowing band."""
    b = bmesh.new()
    layer_uv = b.loops.layers.uv.verify()
    centre = PORT_C + PORT_OUT * (-SHELL_DEEP)
    side_a, side_b = X_AXIS, PORT_OUT
    sides = 20

    def ring(s_, r_):
        return [b.verts.new(centre + PORT_AX * s_ + (side_a * math.cos(2 * math.pi * k / sides) + side_b * math.sin(2 * math.pi * k / sides)) * r_)
                for k in range(sides)]

    half = SHELL_LEN / 2
    # from the crimped front to the rim at the rear: (place along the shell, radius, slot of the band that follows)
    stations = [(-half, SHELL_R * 0.80, 0), (-half + 0.004, SHELL_R, 0), (half - SHELL_BASE - 0.012, SHELL_R, 2),
                (half - SHELL_BASE - 0.007, SHELL_R, 0), (half - SHELL_BASE, SHELL_R, 1), (half - 0.0015, SHELL_R, 1),
                (half - 0.0015, SHELL_R * 1.08, 1), (half, SHELL_R * 1.08, 1)]
    rings = [ring(s_, r_) for s_, r_, _ in stations]
    faces = []
    for (a_, c_), (_, _, slot) in zip(zip(rings, rings[1:]), stations):
        for k in range(sides):
            j = (k + 1) % sides
            f = b.faces.new((a_[k], a_[j], c_[j], c_[k]))
            f.material_index = slot
            faces.append(f)
    f = b.faces.new(rings[0])
    f.material_index = 0
    faces.append(f)
    f = b.faces.new(list(reversed(rings[-1])))
    f.material_index = 1
    faces.append(f)
    for f in b.faces:
        for l_ in f.loops:
            l_[layer_uv].uv = (0.5, 0.5)
    mats = [flat_material("Shotgun_Shell_Hull", (0.10, 0.13, 0.17), 0.45, 0.1),
            flat_material("Shotgun_Shell_Base", (0.78, 0.60, 0.26), 0.30, 0.9),
            flat_material("Shotgun_Shell_Glow", (0.1, 0.1, 0.1), 0.5, 0.0, glow=(GLOW[0], GLOW[1], GLOW[2], 4.0))]
    ob = finish(b, "Shotgun_Magazine", mats, faces, chamfer=False, smooth=True)
    ob["pivot"] = centre[:]
    ob["out"] = (PORT_AX * SHELL_OUT[0] + PORT_OUT * SHELL_OUT[1])[:]
    return ob


EXTRAS = build_extras()  # in the scene before the bakes, so they shade what is around them
PUMP = build_pump()
CELL = build_cell()      # in the scene before the bakes, so it shades what is around it

bm = bmesh.new()
bm.from_mesh(me)
print("WELDED body verts", len(bm.verts), "tris", len(bm.faces), "open edges", sum(1 for e in bm.edges if len(e.link_faces) == 1),
      "sharp edges", sum(1 for e in bm.edges if not e.smooth), "of", len(bm.edges))
bm.free()

# ---------------------------------------------------------------- bake: normal (rounded edges + details) and AO
mat = me.materials[0]
mat.name = "Shotgun_Atlas"
scene.render.engine = "CYCLES"
prefs = bpy.context.preferences.addons["cycles"].preferences
for kind in ("OPTIX", "CUDA"):
    try:
        prefs.compute_device_type = kind
        prefs.get_devices()
        for dev in prefs.devices:
            dev.use = True
        scene.cycles.device = "GPU"
        break
    except Exception as e:
        print("no", kind, e)
world = bpy.data.worlds.new("BakeWorld")
world.light_settings.distance = AO_DISTANCE
scene.world = world

nt = mat.node_tree
nt.nodes.clear()
out = nt.nodes.new("ShaderNodeOutputMaterial")
bsdf = nt.nodes.new("ShaderNodeBsdfPrincipled")
nt.links.new(bsdf.outputs["BSDF"], out.inputs["Surface"])


def tex(image, x, y):
    n = nt.nodes.new("ShaderNodeTexImage")
    n.image = image
    n.location = (x, y)
    return n


t_height = tex(im_height, -900, -300)
bump = nt.nodes.new("ShaderNodeBump")
bump.inputs["Strength"].default_value = 1.0
bump.inputs["Distance"].default_value = DETAIL_DEPTH
nt.links.new(t_height.outputs["Color"], bump.inputs["Height"])
bevel = nt.nodes.new("ShaderNodeBevel")
bevel.samples = 12
bevel.inputs["Radius"].default_value = BEVEL_RADIUS
nt.links.new(bump.outputs["Normal"], bevel.inputs["Normal"])
nt.links.new(bevel.outputs["Normal"], bsdf.inputs["Normal"])


def bake(kind, name, samples, **kwargs):
    image = bpy.data.images.new(name, BAKE_SIZE, BAKE_SIZE, alpha=False)
    image.colorspace_settings.name = "Non-Color"
    node = tex(image, -600, 300)
    for n in nt.nodes:
        n.select = False
    node.select = True
    nt.nodes.active = node
    scene.cycles.samples = samples
    scene.render.bake.margin = PAD
    scene.render.bake.margin_type = "EXTEND"
    only(SRC)
    bpy.ops.object.bake(type=kind, use_clear=True, **kwargs)
    nt.nodes.remove(node)
    print("BAKED", name)
    return image


im_bake_n = bake("NORMAL", "Shotgun_NormalBake", 24, normal_space="TANGENT")
PUMP.hide_render = True          # it moves: its shadow must not be baked onto the forend
im_ao = bake("AO", "Shotgun_AO", 96)
PUMP.hide_render = False

normal = load(im_bake_n)[..., :3].copy()
for block in BLOCKS:
    normal[block] = (0.5, 0.5, 1.0)
im_normal = new_image("T_Shotgun_Normal", normal, False)
ao = np.clip(load(im_ao)[..., 0], 0.0, 1.0)
for block in BLOCKS:
    ao[block] = 1.0
im_orm = new_image("T_Shotgun_ORM", np.stack([ao, rough, metal], axis=2), False)
for im in (im_ao, im_bake_n):
    bpy.data.images.remove(im)

# ---------------------------------------------------------------- final material (Blender review; Unreal builds its own)
nt.nodes.remove(bump)
nt.nodes.remove(bevel)
nt.nodes.remove(t_height)
t_base, t_orm, t_nrm, t_mask = tex(im_base, -900, 400), tex(im_orm, -900, 100), tex(im_normal, -900, -200), tex(im_mask, -900, -500)
sep = nt.nodes.new("ShaderNodeSeparateColor")
nt.links.new(t_orm.outputs["Color"], sep.inputs["Color"])
mix = nt.nodes.new("ShaderNodeMix")
mix.data_type = "RGBA"
mix.blend_type = "MULTIPLY"
mix.inputs["Factor"].default_value = 1.0
nt.links.new(t_base.outputs["Color"], mix.inputs["A"])
nt.links.new(sep.outputs["Red"], mix.inputs["B"])
nt.links.new(mix.outputs["Result"], bsdf.inputs["Base Color"])
nt.links.new(sep.outputs["Green"], bsdf.inputs["Roughness"])
nt.links.new(sep.outputs["Blue"], bsdf.inputs["Metallic"])
nmap = nt.nodes.new("ShaderNodeNormalMap")
nt.links.new(t_nrm.outputs["Color"], nmap.inputs["Color"])
nt.links.new(nmap.outputs["Normal"], bsdf.inputs["Normal"])
glow = nt.nodes.new("ShaderNodeMix")
glow.data_type = "RGBA"
glow.blend_type = "MULTIPLY"
glow.inputs["Factor"].default_value = 1.0
glow.inputs["B"].default_value = GLOW + (1.0,)
nt.links.new(t_mask.outputs["Color"], glow.inputs["A"])
nt.links.new(glow.outputs["Result"], bsdf.inputs["Emission Color"])
# The 13 cells of the energy strip, as the game's material draws them: a dark gap at the start of each cell.
def math_node(op, a, b=None):
    n = nt.nodes.new("ShaderNodeMath")
    n.operation = op
    for i, v_ in enumerate((a, b)):
        if v_ is None:
            continue
        if isinstance(v_, (int, float)):
            n.inputs[i].default_value = v_
        else:
            nt.links.new(v_, n.inputs[i])
    return n.outputs[0]


coord = nt.nodes.new("ShaderNodeTexCoord")
xyz = nt.nodes.new("ShaderNodeSeparateXYZ")
nt.links.new(coord.outputs["Object"], xyz.inputs["Vector"])
def along(axis):
    """The shaded point's distance from the strip's first end along this axis."""
    rel = nt.nodes.new("ShaderNodeVectorMath")
    rel.operation = "SUBTRACT"
    nt.links.new(coord.outputs["Object"], rel.inputs[0])
    rel.inputs[1].default_value = STRIP_FROM[:]
    dot = nt.nodes.new("ShaderNodeVectorMath")
    dot.operation = "DOT_PRODUCT"
    nt.links.new(rel.outputs["Vector"], dot.inputs[0])
    dot.inputs[1].default_value = axis[:]
    return dot.outputs["Value"]


strip_dir = (STRIP_TO - STRIP_FROM).normalized()
strip_len = (STRIP_TO - STRIP_FROM).length
cell_pos = math_node("MULTIPLY", along(strip_dir), STRIP_CELLS / strip_len)
on_strip = math_node("MULTIPLY", math_node("GREATER_THAN", cell_pos, 0.0), math_node("LESS_THAN", cell_pos, float(STRIP_CELLS)))
on_strip = math_node("MULTIPLY", on_strip, math_node("LESS_THAN", math_node("ABSOLUTE", along(STRIP_NORMAL)), 0.0005))
on_strip = math_node("MULTIPLY", on_strip, math_node("LESS_THAN", math_node("ABSOLUTE", along(STRIP_NORMAL.cross(strip_dir))), STRIP_HALF + 0.001))
lit_cell = math_node("GREATER_THAN", math_node("FRACT", cell_pos), CELL_GAP)
cells = math_node("MAXIMUM", lit_cell, math_node("SUBTRACT", 1.0, on_strip))
nt.links.new(math_node("MULTIPLY", cells, EMISSION), bsdf.inputs["Emission Strength"])
bpy.data.images.remove(im_height)
bpy.data.images.remove(color_im)

# ---------------------------------------------------------------- join the cables back, save
TRIGGER = separate(3, "Shotgun_Trigger")
only(CABLES)
CELL.select_set(True)
EXTRAS.select_set(True)
SRC.select_set(True)
bpy.context.view_layer.objects.active = SRC
bpy.ops.object.join()
SRC.name = "Shotgun_Body"
me.name = "Shotgun_Body"
DISPLAY = build_display()
LIGHT = build_light()
SHELL = build_shell()

# What the export needs to know, kept with the objects.
SRC["pivot"] = HAND[:]
TRIGGER["pivot"] = TRIGGER_PIVOT[:]
DISPLAY["pivot"] = HAND[:]
LIGHT["pivot"] = HAND[:]
PUMP["pivot"] = PUMP_C[:]
PUMP["slide"] = (PUMP_AX * PUMP_TRAVEL)[:]
scene["strip_from"] = STRIP_FROM[:]
scene["strip_to"] = STRIP_TO[:]
scene["strip_normal"] = STRIP_NORMAL[:]
scene["strip_half_w"] = STRIP_HALF
scene["energy_bar_on_sight"] = ENERGY_BAR_ON_SIGHT
for o in scene.objects:
    if o.type == "MESH":
        print("OBJECT", o.name, "tris", sum(len(p.vertices) - 2 for p in o.data.polygons), "slots",
              [m.name for m in o.data.materials], "custom normals", o.data.has_custom_normals)
for im in (im_orm, im_normal):
    dump(im, im.name, False)
bpy.ops.wm.save_as_mainfile(filepath=out_blend, compress=True)
print("SAVED", out_blend)
