# Block: a transition when the menu changes look, and a confirmation for every skill point (Nov 13, CENTRAL)

> Written by the architect chat of this block (Claude Code, Claude Opus 5.5). To be merged into the day entry by whoever closes the day. Times in UTC.

## The owner's request
> *"Quiero que haya una transicion de estilo de menu cuando estoy en el mundo medieval viendo el skill tree de armas (menu futurista) y al cambiar de pestaña (no de skill tree de spells) hace un cambio bruzco en el menu. Hacer lo mismo cuando se esta en el mundo futurista con el skill tree de spells. Tambien agregar una confirmacion cuando se use cada skill point."*

Translation: "I want a menu style transition when I am in the medieval world looking at the weapon skill tree (futuristic menu) and change tab (not to the spell skill tree): the menu changes abruptly. Do the same in the futuristic world with the spell skill tree. Also add a confirmation each time a skill point is used."

## What was built (`UI/GBHUD_Menus.cpp`, `UI/GBHUD.h`)
- **The transition.** `DrawMenus` remembers the look it last drew the open menu in. When the look changes while the menu stays open, a 0.42 s transition plays over it. That happens, for example, on an inventory tab from or to the skills page in the other era, or on a skill tree tab.
  - The old look's material covers the screen, and the new look's signature uncovers it.
  - **Into cyberpunk:** a cyan scan line sweeps down over dark carved stone, with red and cyan glitch slices trailing above it and the scanlines fading. The title's chromatic glitch plays again.
  - **Into the 90s RPG:** the near-black breaks into large pixels that vanish in a scattered order, each flashing gold just before it goes.
  - Opening a menu never animates. After Sol's review, closing or switching menus also clears any transition still playing.
  - It applies to every look change inside an open menu, including the Skill Trees tabs, so all the jumps behave the same.
- **The confirmation.**
  - Enter or a click on a perk that can be bought now opens a dialog in the screen's look: "Confirm upgrade" in cyberpunk, "Spend a skill point?" in the RPG look. It names the perk, the cost, the rank before and after, and the points left.
  - Enter or the Confirm button spends. Esc, gamepad B or Cancel closes it without spending.
  - While it is up, it is modal: the arrows, R and the page under it do not answer, and a tab key cancels it.
  - A perk that cannot be bought still just says why, with no dialog.
  - In the RPG look the first try overflowed the box on long perk names, so the box width now follows the text.

## Smoke (UI block)
The UI block gained four screenshots: the transition caught mid-way in both directions, and the dialog in both looks. It also gained a check that can fail:
1. Esc on the dialog spends nothing.
2. A first Enter only asks.
3. Enter on the dialog spends exactly one point and one rank: 5 -> 5 -> 5 -> 4, rank 0 -> 1.

The cyber death screenshot and `quit` moved 4 s later to make room.

## Review: GPT Sol 6.1 (Codex CLI, medium, read-only, logged through `Invoke-LoggedAgent.ps1`)
Prompt: `Prompts/menu_transition_confirm_review_sol.md`. **One finding, fixed:** a transition still playing carried over into the next menu when the player switched menus or closed and reopened within 0.42 s. Opening and closing a menu now clear it.

Sol found no way to spend a point without the dialog, to spend twice, or to buy a different node from the one the dialog names, and no way to leave the dialog stuck.

## Checks
- Iteration: Editor build plus the filtered UI smoke at 1080p and 720p.
- Final: full smoke `done=True crashed=False LogGB errors=0`, 64 screenshots; tests 44 run, 0 failed; Editor and game builds.
- After Sol's fix: Editor build, UI smoke (the confirm check again 5 -> 5 -> 5 -> 4) and game build.
- **Not packaged in this block.**
- **Implementer:** the architect (UI work judged from screenshots).

## Screenshots (`media/2026-11-13/`)
- `menutransition_into_cyber_1080.png`: a third of the way into the transition to cyberpunk. The scan line has revealed the Spells page; the stone below it is still the old look.
- `menutransition_into_rpg_1080.png`: the pixel dissolve into the RPG Skills page, mid-way.
- `skillconfirm_cyber_1080.png`, `skillconfirm_rpg_720.png`: the dialog in both looks.

## Follow-up: shorter button labels
Owner: *"En los botones de confirmacion de uso de skill point, que solo diga Confirm y Cancel, no es necesario que diga Enter y Esc"* ("On the skill point confirmation buttons, just Confirm and Cancel; there is no need to say Enter and Esc").

The buttons now read **Confirm** and **Cancel**; Enter and Esc still work. The change was checked with the UI smoke at 1080p (`media/2026-11-13/skillconfirm_rpg_short_labels_1080.png`), and the Editor and game builds both succeeded.
