# Block: the weapon skill tree becomes a hub of gun branches, Cyberpunk style; skill pages take their tree's look (Nov 13, CENTRAL)

> Written by the architect chat of this block (Claude Code, Claude Opus 5.5). To be merged into the day entry by whoever closes the day. Times in UTC.

## The owner's notes
After the package `194e9ab`, the owner sent two screenshots (`media/2026-11-13/weaponhub_owner_report_weapons.webp`, `weaponhub_owner_report_spells.webp`) and wrote, in Spanish:

> *"al estar en skill - weapons deberia cambiarse toda la interfaz. Para el de spells hayy que cambiar el fondo para uno que haga mejor contraste con el contenido. Y vamos a hacerle una pequeña reforma del skill tree de armas. Vamos a hacerlo un poco estilo cyberpunk con las ramas. El primer perk sigue siendo el mismo, que es Heavy Rounds. Y de ese van a salir las 3 ramas. Sin embargo, esto va a ser con submenús, similar a Cyberpunk. De Heavy Rounds van a salir las 3 categorías, que son escopeta, rifle y pistola. Y al hacer clic en esas esferas, cuadros o símbolos, se abre el skill tree respectivo de esa arma. En la interfaz tiene que haber también un botón para que el jugador pueda volver, ya que esta es dentro de un submenú."*

Translation: "On Skills > Weapons the whole interface should change. For the spells one, the background must change to one that contrasts better with the content. And let's give the weapon skill tree a small reform, a bit Cyberpunk style with the branches. The first perk is still Heavy Rounds, and the 3 branches come out of it. But this will be with submenus, like Cyberpunk. Out of Heavy Rounds come the 3 categories: shotgun, rifle and pistol. Clicking those spheres, boxes or symbols opens that weapon's skill tree. The interface must also have a button for the player to go back, since this is inside a submenu."

## What changed
- **The whole screen follows the shown tree (`UI/GBHUD.cpp`, `GetMenuSkin`).** On the Skill Trees screen and on the inventory's Skills page, the weapons page is cyberpunk from title to footer and the spells page is RPG, whatever the era. The other shared menus keep following the era, as decided in the previous block.
- **The spell page's background.** It used to be bricks behind bricks, and the gold-framed nodes fought the pattern. Now the panel is near-black slate with a faint trace of the masonry and a low ember warmth in the middle (`weaponhub_spells_dark_background_1080.png`).
- **The weapon hub (`UI/GBHUD_Menus.cpp`).**
  - Heavy Rounds sits on top. Under it hang three emblems, numbered 01 to 03: **Pistol, Rifle, Shotgun**. Each has a pixel outline of its gun, its name and one segment per rank of its perks.
  - Enter or a click on an emblem opens that gun's page. The page has a **Back** button with the Esc key next to it, and the title "WEAPONS // RIFLE". Esc and gamepad B also go back to the hub, landing on that gun's emblem.
  - The emblems are not perks: `GetSkillNodes` builds them as pseudo-nodes (`Branch_<Gun>`) that never reach the skill component.
  - The page shown is always derived from the cursor. So the mouse, the keyboard, the tabs and the memory of the last node in each tree all work unchanged.
  - The weapon pages are centred vertically. The first try left the bottom half of the hub empty (`weaponhub_hub_first_try_1080.png`).
- **Data (`Skills.json`, new `Branch` field in `FGBSkillNodeDefinition`).** The owner named the categories but not their contents. The architect regrouped the nine existing perks by theme, **with their effects, costs and ranks unchanged**, so §19's balance and the 36-point budget still hold:
  - **Pistol** (precision): Deadeye; Marksman -> Precision Recovery.
  - **Rifle** (sustained fire): Long Shot; Quick Trigger -> Steady Grip.
  - **Shotgun** (close range): Point Blank -> Arcane Conduit; Rapid Reload.
  - Two prerequisites changed so each branch stands alone: Rapid Reload now needs Heavy Rounds (it needed Quick Trigger), and Arcane Conduit needs Point Blank (it needed Marksman).
  - The three perks that work on every gun now say "All guns:" in their description.
  - GDD v0.39 records this as provisional. Gun-specific versions of the shared perks are the obvious next step if the owner wants each branch to touch only its gun.
- **Smoke:** the Arcane Conduit step unlocks Point Blank instead of Marksman, because of the new prerequisite. The `12_weapon_tree` screenshot shows the hub. The UI block now captures the hub (`ui_21`, `ui_20`) and a gun page (`ui_09`, Rifle).

## Review: GPT Sol 6.1 (Codex CLI, medium, read-only, logged through `Invoke-LoggedAgent.ps1`)
Prompt: `Prompts/weapon_hub_review_sol.md`. **One finding, real, fixed.**
- **The problem:** inside menus, Tab was mapped to "back". On a gun page in the inventory, Tab would go back to the hub instead of closing the inventory, which is Tab's own job.
- **The fix:** Tab now has its own menu action (`MenuTabKey`). In the inventory it closes the inventory, like I. On every other screen it still acts as "back", as before.

Sol also confirmed:
- No pseudo-node reaches the progression code.
- Opening and going back both clear the stale mouse hits.
- The look always matches the page that is drawn.
- All 22 perks are still reachable.
- The balance tool still runs, and its builds satisfy the new prerequisites.

## Checks
- **Iteration:** an Editor-target build plus `Smoke-Jam.ps1 -Only ui -Offscreen`, once at 1080p, then once more at 1080p and once at 720p after the layout fix.
- **Final checks, once each:** Editor build; full smoke `done=True crashed=False LogGB errors=0`, 60 screenshots, 140 s; tests 44 run, 0 failed; game target build succeeded.
- **Not packaged in this block.**
- **Implementer:** the architect, not Muse Spark. This was screen layout judged from screenshots round by round, and a pass for each resolution would have cost more as a hand-off than as direct work.

## Screenshots (`media/2026-11-13/`, the game's smoke run, editor build)
- `weaponhub_hub_1080.png`: the weapon hub (Heavy Rounds and the three gun emblems), 1920x1080.
- `weaponhub_rifle_page_past_1080.png`: the Rifle page with Back. It was taken in the past, and the whole screen is still cyberpunk.
- `weaponhub_inventory_hub_720.png`: the hub inside the inventory's Skills page, 1280x720.
- `weaponhub_spells_dark_background_1080.png`: the spell page on its new dark slate.
- `weaponhub_hub_first_try_1080.png`: the first hub, before the grid was centred and the emblems made taller.
- `weaponhub_owner_report_weapons.webp`, `weaponhub_owner_report_spells.webp`: the owner's screenshots before the change.

## Needs a person playing
Opening and leaving each gun page with the mouse, Enter, Esc, gamepad B and Tab; whether the grouping of perks per gun makes sense to the owner.
