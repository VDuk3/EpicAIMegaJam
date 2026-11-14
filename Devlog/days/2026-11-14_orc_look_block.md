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
