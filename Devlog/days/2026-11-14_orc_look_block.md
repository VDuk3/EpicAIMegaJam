# 2026-11-14, block: the orcs' look, analysed before any change

Architect: Claude Code (Fable 5.2) on CENTRAL. No implementer: this block only looks and measures. Nothing was changed in the game.

## What the owner asked

> hay que arreglar los orcos tanto en funcionamiento como apariencia. Primero apariencia. Analiza la forma actual de los orcos y sus armaduras, hay que hacer ajustes.

Translation: The orcs need fixing, both how they work and how they look. Looks first. Analyse the current shape of the orcs and their armor; adjustments are needed.

## How it was done

- The Editor was in use by another chat (the castle map), so everything was done in Blender, headless, from the delivered files: `enemies_export_fbx.py <stage> armor --blend` rebuilt the fitted armor in a temporary folder, `armor_review_render.py` rendered close-ups of every zone of the four sets (torso, head, arm, hips, leg) and two poses, and `enemies_render.py` rendered the bare body and the two armor line-ups as delivered.
- New: `Tools/Blender/orc_measure.py` measures the body's proportions and, for each set, the share of the body's surface that has armor within 12 cm along the skin's normal. The measure is generous (a plate flaring away from a leg still counts), so the real picture is worse than the numbers.

## What was found

**The body**
- A human bodybuilder painted green: 210 cm, 5.65 heads tall, legs half the height, hands at hip level, shoulders 61 cm between joints. Nothing in the proportions says orc except the head.
- 4,147 triangles, smooth plastic skin, and a pair of dark green shorts painted into its texture. Each armor set is about 50,000 triangles, twelve times the body it sits on.

**The armor against the concept**
- The concept image generated on ASSETS (kept in `SourceArt/Enemies/Orc/provenance`) is one sealed suit: collar, big shoulder guards, a cuirass down to the belt, a backpack, full legs.
- What the game has is four sets of loose pieces generated for no body in particular, then stretched onto this one. Measured cover of the body: 66 to 71 % overall, but belly 13 %, pelvis 24 to 30 %, neck 16 to 31 %, forearm 34 to 48 %, hand 28 to 48 %.
- So every orc shows a bare belly and lower back, the painted shorts between belt and thigh plates, bare elbows and bare hands. They read as orcs in underwear with plates tied on, not as armored soldiers.

**The fit**
- Helmets follow the neck bone, which leans forward: every helmet looks at the floor. Set A's face mask sits on the chin and the mouth shows above it; the scalp comes out through the top of sets B and C.
- Shoulder guards were fitted to the upper arm and came out as flat sleeves; the round guards of the line-up are gone, and with them the silhouette.
- Gauntlets were fitted to forearm and hand together: they open like funnels round the wrist and their fingers are lost.
- Thigh plates and greaves flare away from the leg like skirts and cones; the push that keeps plates outside the skin left torn edges on almost every piece, and textures are stretched and blotched (set A's belly plate, the boots of B, C and D).
- In a pose the plates lift and cross each other (the aiming and walking renders).

**The four sets as designs**
- A (plain military) is the only one that belongs to the future's sci-fi orc of GDD section 3. B is medieval iron with spikes, C is bone and gold with a halo, D is an insect or dragon-scale suit with an insect head.
- All four are dark green or black on green skin: armor and body do not separate, and nothing glows except D's gem.

## Media

- `media/2026-11-14/orc_look_body_front.png`, `orc_look_body_side.png`: the bare body as delivered.
- `media/2026-11-14/orc_look_lineup_1.png`, `orc_look_lineup_2.png`: the armor pieces as delivered, before any fit.
- `media/2026-11-14/orc_look_<A..D>_upper.png`: torso front, torso side, head, arm. `orc_look_<A..D>_lower.png`: hips front, hips side, leg, aiming pose.
- `media/2026-11-14/orc_look_A_posed.png`: set A aiming and walking.

## Open

- The owner decides the direction of the adjustments; nothing is rebuilt or imported before he sees renders of it.

## Refit, work in progress (owner: "yes, start, and the gauntlets are on wrong: put them on right")

Architect's own work in Blender (exception to the Muse rule: the job is a loop of fit, render, look, correct). Nothing imported into the game yet.

- **Why the first fit looked wrong**, read off contact sheets of every delivered piece (`Tools/Blender/orc_armor_pieces.py`): thigh, upper-arm and shin pieces are closed tubes, not open plates, and the old code turned every limb piece 180 degrees (top down, left for right). The gauntlets went on backwards for the same reason: fingers up the forearm, the cuff's mouth round the hand. The "greaves" are tall boots with a foot; the cuirasses reach the belt but had been squashed onto the upper chest.
- **New fit** (`Tools/Blender/orc_armor_fit.py`): each piece keeps its shape, is turned by the shortest rotation onto its bone, scaled as a whole and bound to one bone (cuirass: spine bones by height). Helmets stand upright on the skull. The gauntlet's hand is registered onto the orc's own hand (closest points, best rigid motion), its cuff laid along the forearm and shortened to 80 % of it; under a gauntlet the body is worn without hands (`SK_Orc_NoHands`).
- **Undersuit** painted per texel into the body's texture: dark ribbed fabric below a collar line, gloves included; only the head stays skin.
- **State after three passes:** set A reads as an armored soldier, its gauntlets are right. Still wrong: set B's gauntlet (the registration picks a wrong turn), thigh and upper-arm tubes too wide, set C's helmet too low, set C's knee skulls float, set B's greaves. Emissive accents not started.
- Media: `media/2026-11-14/orc_refit_wip_A.png`, `orc_refit_wip_C.png`, `orc_refit_wip_A_gauntlet.png`.

## Refit, ready for the owner's eye (not imported)

Fourteen rounds of fit, render, look, correct. What changed after the work-in-progress note above, each change with the render that asked for it:

- **Gauntlets.** A glove's "wrist" (its narrowest section) could not be found from its vertices (a slice of a 900-vertex glove holds five of them): sections are now measured on 20,000 points spread over the surface. The hand part is registered onto the orc's hand from four starting turns, and the winner must also leave the wrist at the wrist and the cuff along the forearm; judged on surface distance alone, set B's mitten was laid across the hand. Cuffs are shortened to 80 % of the forearm (they were drawn elbow-long for a much smaller hand).
- **The body worn under armor, `SK_Orc_Armored`:** the same mesh and skin, 2.5 cm slimmer on the limbs and 1.5 cm on the chest (by its own bone weights, so it fades out at the joints), no hands, ears laid flat. Rigid plates sized to the bare body either stood off it like barrels or let it through in ragged patches; both were tried.
- **Tubes** (thigh, upper arm, greave, knee) are widened only where the limb still comes through, in five stations along their length, measured on face points. Growing a whole tube until it cleared made barrels; a greave now keeps its width at the knee and opens at the ankle.
- **Cuirass:** front to back it follows the torso's profile in four bands, and its back corners are tucked in. With one depth scale its arm-hole rims stood behind the shoulders like hoops.
- **Shoulder caps** sit on the side of the deltoid; the strap behind the dome is cut off (at four times its size it was a hoop on the back). Left and right pieces take the same sizes (the right is fitted first).
- **Helmets** placed by hand per set (scale and two offsets) until no tusk or ear came through.
- **Undersuit:** warm charcoal (a cool one merged with set B's steel), and light lines 7 mm wide in orange: a ring inside the collar, one round each elbow and knee, one down the spine. `import_enemies.py` gives the orc's material a glow on that orange; the review renders show the lines as paint, not as light.
- **Cost:** 14,500 to 19,400 triangles per set (the first fit cut every plate into 3 cm pieces to wrap it: about 50,000).

Still imperfect, and said to the owner: boots keep some blotched texture; set B's greaves are open shin plates with pistons and stand wide; rigid plates cross each other when a knee or elbow bends far; sets B, C and D are the designs as delivered (iron, bone, scales), not sci-fi.

Media: `media/2026-11-14/orc_refit_front.png`, `_side`, `_back`, `_walk`, `_aim` (four sets side by side), `orc_refit_gauntlets.png`, `_heads`, `_torsos`, `_arms`, `_legs` (the same zone of each set).

Next, only on the owner's yes: import (`SK_Orc`, `SK_Orc_Armored`, 36 armor meshes, the body's textures), point the five armed orcs at `SK_Orc_Armored`, look at them in the game.

## The owner's review of the renders: three corrections

> 1. Esta el pulgar del lado que no es. 2. Esta torcido. 3. Quita la parte superior de las botas

Translation: (1, on set B's gauntlet) the thumb is on the wrong side; (2, on set B's helmet) it is crooked; (3) take the upper part of the boots off.

- **Thumb.** Set B's glove is a left one in the line-up (the other three are right ones), and it had been used for the right hand, its mirror for the left. Two fixes failed before one worked: mirroring the piece before registering it (it was laid palm outwards, cuff twisted 46 degrees) and letting the registration choose between the piece and its mirror (right for A, C and D by 3.5 against 7.5, wrong for B by 5.7 against 7.1; a mitten is nearly its own mirror image). What works: register the piece as it is, which lays it well, then reflect it side to side in the hand's own frame, which moves the thumb across and nothing else. The registration also got a thumb term (distance from the orc's thumb to the glove's surface).
- **Helmet.** Set B's helmet stands 13.5 degrees off the front in the line-up. Every helmet is now turned to its own plane of symmetry before it is fitted (the turn that makes it most alike to its mirror image): B -13.5, D -5, C -2.5, A 0.
- **Boots.** Cut with a plane at 18.5 cm (the ankle); the greaves slide down and stretch up to 20 % to end there. Cuts are real cuts through the faces now; deleting vertices had left ragged edges.

Media, replaced: `orc_refit_front.png`, `_side`, `_back`, `_walk`, `_aim`, `_gauntlets`, `_heads`, `_legs`; new: `orc_refit_B_gauntlet.png` (set B's right hand from outside, front and inside, and its left hand).

## The owner's second look: wrists and boot shafts

> 1. La muñeca esta torcida hacia el lado del meñique. 2. Enderezala. 3. Remueve completamente la parte superior de las botas.

Translation: (1, on the gauntlets of sets B and D) the wrist is bent towards the little finger's side; (2) straighten it; (3) remove the upper part of the boots completely (his picture shows the four tall shafts over the calves).

- **Wrists.** Registered onto the orc's hand, a glove's hand took that hand's own bend plus its sculpt's: measured, 21 to 35 degrees to the little finger's side on sets A, C and D, and 7 to 10 the other way on B. The line of the glove's hand is now measured where it ended up (the middle of its surface, side to side, at six stations from wrist to knuckles; a median, so the thumb does not pull it) and the hand part is turned about the wrist, in the plane of the palm, until that line runs along the forearm. The registered pose is kept for the skin, so each glove finger is still bound to the orc finger it was laid on. Done on all four sets, not only the two in the pictures.
- **Boot shafts.** The last row of Armor 2, which the first fit called greaves, are tall boot shafts; cutting the boots at the ankle and bringing those shafts down to them (the first reading of "take the upper part of the boots off") left the upper part of a boot in place. They are no longer fitted or exported (`SKIP_SLOTS`); the lower leg is the suit from knee to boot, with its orange ring at the knee. Eight slots per set now. This is the architect's reading of the owner's picture, said to him as such.

Media, replaced: `orc_refit_front.png`, `_side`, `_back`, `_walk`, `_aim`, `_gauntlets`, `_legs`, `orc_refit_B_gauntlet.png`; new: `orc_refit_gauntlets_front.png`.
