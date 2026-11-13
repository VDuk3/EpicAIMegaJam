# Sprite animations on the LAPTOP (SpriteLab) -> CENTRAL

Pipeline agreed 2026-11-13 (architect). The LAPTOP draws and checks the animations; CENTRAL wires them into Glyphbound.

## Rules
- The pixels are drawn by hand in Pixelorama. The AI on the LAPTOP (Sonnet 5.5 in Cursor) writes only tooling (import, check, preview); it never generates or edits pixels.
- SpriteLab is a throwaway UE project on the LAPTOP (5.8 is fine: nothing built in it reaches the game, so the 5.9.0 pin does not apply), only to preview timing, silhouette and angles. Its .uasset files are NOT sent: CENTRAL rebuilds everything from the PNG + JSON.
- Cursor on the LAPTOP is logged by the user-level Cursor hooks (Glass Box). Check they are installed before starting.

## Delivery contract (what arrives in `\\CENTRAL\Incoming\LAPTOP\anims\<sprite_id>\`)
- `<sprite_id>.pxo`: the Pixelorama source.
- One PNG per animation and angle: `<sprite_id>__<anim>__a<angle>.png` (double underscore), a horizontal strip, frames left to right, all frames the same size, transparent background, 1:1 (no scaling).
- `<sprite_id>.anim.json`:

```json
{
  "Id": "valki",
  "FrameWidth": 100,
  "FrameHeight": 100,
  "Pivot": [50, 98],
  "Angles": 5,
  "Animations": [
    { "Name": "idle",   "Fps": 6,  "Loop": true,  "Frames": 4 },
    { "Name": "walk",   "Fps": 8,  "Loop": true,  "Frames": 6 },
    { "Name": "attack", "Fps": 10, "Loop": false, "Frames": 5, "Events": [ { "Frame": 3, "Name": "hit" } ] },
    { "Name": "death",  "Fps": 6,  "Loop": false, "Frames": 6 }
  ]
}
```

- `Pivot`: the pixel in the frame that touches the ground (feet, centered).
- `Angles`: 1 (always faces the camera), 5 or 8. Index = where the character looks on screen: 0 front (looks at the player), 1 front-right, 2 right (looks to the right edge of the screen), 3 back-right, 4 back, 5 back-left, 6 left, 7 front-left. With 5 angles the game mirrors 1-3 to get 7-5.
- `Events`: optional; the frame where something happens (`hit`, `cast`, `step`), so the game syncs damage and sound.
- Style guide rules apply (Design/VisualStyleGuide.md): 100x100 canvas, 2 px margin, black 1 px outline, master palette, base 6 fps (faster only where motion needs it).
- Names in English, lower case, no spaces.
