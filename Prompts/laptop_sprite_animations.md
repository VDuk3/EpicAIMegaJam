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

## One-time setup on the LAPTOP (by hand)
Status reported by the LAPTOP agent (`_de_LAPTOP\20261113-1948_setup-spritelab-anims.md`): steps 1, 3, 4 and 5 done; the project already exists at the long path below (there is no `C:\SpriteLab`, and no junction is wanted).
1. UE 5.8 project **SpriteLab** (Blank, Blueprint, no Starter Content): `C:\Users\Romelt\Documents\PixeloramaAIWorkspace\SpriteLab`.
2. Edit > Plugins: enable **Paper2D** and **Python Editor Script Plugin**; restart the Editor.
3. Cursor devlog hooks: installed (`C:\JamLog\bin\`); do not reinstall.
4. `\\192.168.31.188\Incoming\LAPTOP\anims\` (animations) and `...\arte\` (other art): created.
5. Art goes in `C:\Users\Romelt\Documents\PixeloramaAIWorkspace\SpriteLab\Art\<sprite_id>\`. Pixelorama export: File > Export > Spritesheet, 1 row, only the frames of one animation and one angle (tags help).

## Prompts for Sonnet 5.5 in Cursor (folder `C:\Users\Romelt\Documents\PixeloramaAIWorkspace\SpriteLab` open). Kept in Spanish as given to the team.

### Prompt 1: build the tool (once)

```
Proyecto: SpriteLab, un proyecto vacío de Unreal Engine 5.8 con Paper2D y el Python Editor Script Plugin activados. Sirve solo para previsualizar animaciones de pixel art antes de enviarlas a otra máquina (el juego final está en UE 5.9, pero de aquí solo salen PNG y JSON, nunca .uasset). Tú escribes herramientas; NUNCA generes ni modifiques píxeles: el arte lo dibuja a mano una persona en Pixelorama.

Formato de entrada (contrato fijo, no lo cambies), en C:\Users\Romelt\Documents\PixeloramaAIWorkspace\SpriteLab\Art\<sprite_id>\:
- <sprite_id>.anim.json con: Id, FrameWidth, FrameHeight, Pivot [x,y] (píxel que toca el suelo, origen arriba a la izquierda), Angles (1, 5 u 8) y Animations [{Name, Fps, Loop, Frames, Events opcional [{Frame, Name}]}].
- Un PNG por animación y ángulo: <sprite_id>__<anim>__a<angle>.png (doble guion bajo), tira horizontal de frames de izquierda a derecha, fondo transparente, escala 1:1.
- Índice de ángulo = hacia dónde mira el personaje en pantalla: 0 frente, 1 frente-derecha, 2 derecha, 3 espalda-derecha, 4 espalda, 5 espalda-izquierda, 6 izquierda, 7 frente-izquierda. Con Angles=5 solo existen los PNG 0 a 4; los ángulos 5, 6 y 7 se muestran reflejando el 3, el 2 y el 1. Con Angles=1 solo existe a0.

Escribe C:\Users\Romelt\Documents\PixeloramaAIWorkspace\SpriteLab\Scripts\spritelab.py (Python dentro de Unreal 5.8; no instales nada con pip) con dos modos:

1) check: no crea assets. Por cada sprite_id comprueba y lista OK/ERROR por archivo:
   - JSON válido y todos los PNG esperados presentes (y avisa de PNG que sobran);
   - ancho del PNG = Frames × FrameWidth y alto = FrameHeight;
   - alfa solo 0 o 255 (nada semitransparente);
   - 2 px vacíos en el borde de cada frame;
   - Pivot dentro del frame; Events apuntando a frames que existen.
   Para leer los PNG usa un lector mínimo en Python puro (zlib + struct, PNG RGBA/RGB/paleta de 8 bits) para no depender de la API de imágenes de Unreal.

2) import: corre check y, si pasa, por cada sprite_id:
   - importa los PNG en /Game/Lab/<sprite_id>/ con ajustes de pixel art: filtro Nearest, sin mipmaps, compresión UserInterface2D, sin streaming;
   - corta cada tira en Paper2D Sprites de FrameWidth × FrameHeight con el pivot del JSON (pivot personalizado) y 1 píxel = 1 unidad (PixelsPerUnrealUnit = 1);
   - crea un PaperFlipbook por animación y ángulo con el Fps del JSON;
   - abre o crea el nivel /Game/Lab/Preview y coloca, en fila, un PaperFlipbookActor por ángulo (0 a 7; los reflejados con escala X = -1), con un TextRenderActor encima que diga "<anim> a<angle>". Una fila por animación, separadas para que no se pisen, y una luz y una cámara que lo encuadren.
   Volver a correrlo reemplaza lo anterior sin duplicar assets ni actores.

Uso desde el Output Log del Editor (cambia el desplegable de Cmd a Python o usa el comando py):
  py "C:/Users/Romelt/Documents/PixeloramaAIWorkspace/SpriteLab/Scripts/spritelab.py" check [sprite_id]
  py "C:/Users/Romelt/Documents/PixeloramaAIWorkspace/SpriteLab/Scripts/spritelab.py" import [sprite_id]
Sin sprite_id procesa todos. Escribe un README.md corto en Scripts con estos comandos.

Si una llamada de la API de Python de Unreal no existe en la 5.8, busca la equivalente en la 5.8; no inventes nombres. Itera barato: prueba primero el modo check, y la importación una vez al final. Cuando acabes, dime paso a paso qué hago en el Editor para probarlo.
```

### Prompt 2: check new art (every time)

```
Hay arte nuevo o cambiado en C:\Users\Romelt\Documents\PixeloramaAIWorkspace\SpriteLab\Art\<sprite_id>\. Dime el comando de check de spritelab.py para correrlo en el Output Log; cuando te pegue el resultado, explícame cada ERROR en palabras sencillas (qué archivo, qué frame, qué está mal y cómo se arregla en Pixelorama). No toques los PNG. Cuando todo esté OK, dame el comando de import para verlo en el nivel Preview.
```

### Prompt 3: send to CENTRAL

```
Copia la carpeta C:\Users\Romelt\Documents\PixeloramaAIWorkspace\SpriteLab\Art\<sprite_id>\ (el .pxo, los PNG y el .anim.json; nada de .uasset) a \\192.168.31.188\Incoming\LAPTOP\anims\<sprite_id>\. Si ya existe, crea dentro una subcarpeta con fecha y hora (en ese recurso no se puede borrar ni sobrescribir con seguridad). Luego escribe en la carpeta copiada un NOTES.md en inglés con: animaciones, frames y fps de cada una, ángulos, qué cambió respecto a la versión anterior y que el arte se dibujó a mano en Pixelorama. Copia también C:\Users\Romelt\Documents\PixeloramaAIWorkspace\SpriteLab\Scripts\ a \\192.168.31.188\Incoming\LAPTOP\spritelab_tools\ (con subcarpeta de fecha si ya existe) para la bitácora.
```

## After sending
Tell CENTRAL the sprite id and what it is for (enemy, player hands, ...). CENTRAL imports it from the PNG + JSON, wires it in and sends back a build to check the final look (lighting and size in first person are only right in the game).
