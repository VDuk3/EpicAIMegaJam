# Gameplay video: script and shot list

> For the Epic AI MegaJam 2026 submission. Written 2026-11-15 by the architect from `Design/GDD.md` v0.68 and the commits up to `f4dc179`. Everything shown below exists in the current build. A person records it from the **packaged** game (agents do not run the game).

## Requirements (from the jam rules in `CLAUDE.md`)
- **30 to 60 seconds.** Target: 55 s.
- **1920 x 1080 or more.**
- **English** only (captions, any voice-over, any text on screen).
- **Gameplay in the first 5 seconds**: the video opens on play, not on a title card.
- **No third-party brands or logos.** The only logo allowed is the Unreal Engine logo, used as Epic's Unreal Engine brand guidelines say.
- Teen (13+) content.

## The idea in one line
The theme is DIRECTOR'S CUT ("your vision, your edits, your final cut"). Our cut is a strict rule that never breaks (GDD §3, §14): **magic is pixel art, the future is 3D**, and the player cuts between the two in one fight. The video is edited like a film's cut: each shot is a "scene" with a short slate caption, and the edit itself jumps between the pixel past and the 3D future.

## Before recording
- Unzip the latest `TeamTBD_Glyphbound.zip` to a fresh folder. Use the packaged build, not the Editor.
- Starts beside the exe:
  - `Play-Castle.bat`: the castle, where the game starts (staff, Fireball, Lightning Bolt, Ice Wall).
  - `Play_AllWeapons.bat`: every weapon from the start (needed for the guns in the castle).
  - `Play-FutureBase.bat`: straight to the future base.
  - The seal's travel to the future base carries only the progress flags, not the weapons (GDD §4). For the future shots with guns, make a copy of `Play-FutureBase.bat` and add `-AllWeapons` after the map path. Shot 5 (the seal) and shot 6 are recorded as separate takes.
- For clean takes, the Development build has console commands: `gb.God` (player cannot die), `gb.OpenValkyrie` (opens the energy gate to the Valkyrie's room), `gb.OpenDoors`. Open the console only between takes and cut that footage: the console must never appear in the video.
- Do not show the skill tree's "+1 skill point" test button or "Reset skills" hint if they are still in the build (they are test tools). If the skill tree shot cannot avoid them, drop that shot.

## Shot list (55 s)
Timings are for the final edit; record each shot several times, longer than needed.

| # | Time | Map / zone | What happens on screen | Caption (on screen) |
|---|---|---|---|---|
| 1 | 0:00 - 0:04 | `PastCastle`, lower ward, the orcs' camp (`Opening_Past`) | Straight into play: staff in hand, cast a **Fireball** (QTE skipped by the cut: show it landing) into the pixel-art orcs by the camp fire; the shockwave knocks them back. Gameplay from frame one. | none for the first 2 s, then small, lower third: **SCENE 1: THE PAST** |
| 2 | 0:04 - 0:09 | Same ward, close fight | Cast the **spell QTE** on camera: slowed time, the key sequence (W A S D) and the mouse gesture, then a **Lightning Bolt** through a line of orcs. The 90s RPG stone HUD is visible. | **Magic is performed, not triggered.** |
| 3 | 0:09 - 0:13 | Castle, Main Hall or the gate passage with **torches** | A short walk-and-turn through the plain-block pixel castle, the torch flames lighting the walls. Raise an **Ice Wall** in a doorway. | **Every wall cut in pixels.** |
| 4 | 0:13 - 0:18 | Castle (`Play_AllWeapons.bat`) | Switch staff to **shotgun** (mouse wheel or Alt): the HUD changes from stone to the red / cyan cyberpunk skin with its boot flicker. Fire, pump, shell-by-shell reload. A 3D gun in a pixel-art world. | **Hard cut: a 3D gun in a pixel world.** |
| 5 | 0:18 - 0:22 | The crypt: the seal's hall | Walk up to the **seal**, press G: its caption plays ("The castle tears away."). | **CUT TO: THE FUTURE** |
| 6 | 0:22 - 0:28 | `FutureBase`, transfer bay (`Future_Bay`) | Pistol or rifle in hand; the 3D enemies are on patrol, notice the player, take cover and flank (robots, armored orcs, a drone overhead). Aim, headshot, damage numbers. | **SCENE 2: THE FUTURE** |
| 7 | 0:28 - 0:33 | Same fight | **Weapon enchant** on the gun (T): the enchant QTE's element choice (A fire, W lightning, D ice), the gun's glow and energy bar change; an enchanted burst fills an enemy's elemental bar. | **Magic on a gun. Both eras in one fight.** |
| 8 | 0:33 - 0:38 | Transfer bay's east wall | Shoot the **glass bulkhead**: its panes break one by one into pieces while the bars stay; drop through the lower pane. | none (let the glass sell it) |
| 9 | 0:38 - 0:44 | Sub-boss hall (`SubBoss_Future`) | The **horde chief** behind his raised shield walks at the player while the enforcers flank from the galleries; the player switches to the staff and lands a **Fireball** (baked fluid fire in flight) at his side. | **The director's choice: spell or gun, every second.** |
| 10 | 0:44 - 0:50 | Medium room, the Valkyrie's room (after `gb.OpenValkyrie` off camera) | The **Valkyrie** sealed in her cryogenic tube; the player breaks the glass (four hits), she wakes and leaps out with her sword. Cut on her landing. | **SCENE 3: SHE WAKES.** |
| 11 | 0:50 - 0:55 | End card (black) | Game title, then the dev log line, then the Unreal Engine logo. | **[GAME TITLE]** / *Directed by people. Built with AI agents. Every cut is in the dev log.* / Unreal Engine logo |

Optional replacement shots (if one of the above does not record well):
- Ledge climb onto a castle wall, or a vault over a railing in the future base.
- The flamethrower held down, or the ice floor slowing a group (ground figure drawn flat on the floor).
- The weapon skill hub (Heavy Rounds, then the Pistol / Rifle / Shotgun pages) and the stone spell tablet, cut with a menu transition (only if no test button is visible).
- The Entity's phase 1 (3D floating shell, hologram tentacles, energy bolts): it is not placed on a map, it is spawned with `gb.SpawnEntity` in the test arena, so use it only if the owner wants the final boss teased.

## Captions
- One line each, 3 to 8 words, in English. Large enough to read on a phone; lower third, never over the crosshair or the HUD's numbers.
- Style: white condensed capitals with a thin black outline, or a film slate look ("SCENE 1"). No fonts or marks of other brands.
- The first caption appears no earlier than 2 s in, so the first 5 s are clearly gameplay.
- **[GAME TITLE]**: the title is still TBD (GDD §17). Use the final title the owner gives; until then the code name is Glyphbound.

## Recording tips
- **Resolution:** 1920 x 1080 minimum (2560 x 1440 or 3840 x 2160 if the machine holds 60 fps; export the final at 1920 x 1080 or the higher size). Full screen, 16:9. If the game does not start at that size, add `-ResX=1920 -ResY=1080` to the command line of a copy of the `.bat` file.
- **Frame rate:** record and export at 60 fps (30 fps at least). Constant frame rate in the recorder.
- **Capture:** any screen recorder on CENTRAL (RTX 4080 Laptop): record the game window or the display at native size, high bit rate (about 40 to 50 Mbit/s for 1080p60), H.264 or H.265, MP4.
- **Turn off every overlay** before recording: recorder, GPU driver, chat and launcher notifications, FPS counters, the Windows taskbar. Nothing but the game on screen.
- **Mouse:** move slower than in normal play; short, smooth turns read better on video. Hold the gun aimed for a moment after each kill.
- **Sound:** keep the game's own sound if the build has any. Music added in the edit must be free to use and is a third-party asset: log its name, source and licence in the day entry. A voice-over, if any, is in English.
- **No third-party logos:** no other games, brands, store pages, recorder watermarks or desktop in any frame. The Unreal Engine logo only on the end card, as Epic's brand guidelines say (official logo files, unaltered, with clear space).
- **Edit:** hard cuts on action (a shot, an explosion, a hit) between the shots; no long fades. Keep the total between 30 and 60 s; if it runs long, drop shot 8 or shot 3 first.
- Keep the raw takes: a frame or two of them can go into `Devlog/media/2026-11-15/` with an English caption for the Glass Box dev log.
