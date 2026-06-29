# Versions
 
## v0.1.0 -> v0.1.1

- Initial C++20 + raylib bootstrap.
- Added application window, main menu, confirm dialog, FPS display, debug overlay, logger, normalized input layer, and initial level data structures.

## v0.1.1 -> v0.1.2

- Added CMake and Makefile build entry points for the VoX3D executable.
- Implemented the first C++20/raylib application shell with a safe centered window, main menu, FPS counter, debug overlay, and exit confirmation dialog.
- Added a small terminal logger with levels, PID/TID/thread role, CLI log level parsing, color auto-detection, and `--no-color` support.
- Wired helper scripts `m`, `r`, and `c` to the new Make-based workflow.

## v0.1.2 -> v0.1.3

- Added `config/app.json` loading and moved window, UI font sizes, spacing, FPS, debug overlay, dialog, and log settings out of hard-coded runtime constants.
- Added `--config=<path>` and `--raylib-log-level=...` CLI overrides while keeping existing `--log-level`, `--debug-ui`, and `--no-color` behavior.
- Moved FetchContent dependencies to persistent `.deps/` and changed `./c` to clean build artifacts without deleting downloaded raylib by default.

## v0.1.3 -> v0.1.4

- Changed default clean to remove only VoX3D target artifacts, preserving cached raylib/glfw build outputs under `.deps/`.
- Added explicit `cmake-clean` / `./c --cmake` for full CMake target cleanup when it is intentionally needed.
- Removed an unused config loader helper that produced a compiler warning during Debug builds.

## v0.1.4 -> v0.1.5

- Added cached UI metrics/layout calculation so menu hitboxes, dialog geometry, text wrapping, FPS, and debug overlay are derived from one consistent scale model.
- Simplified `config/app.json` by removing per-widget font sizes, paddings, dialog dimensions, and other fragile manual UI constants.
- Switched debug overlay and FPS to compact default-font rendering while keeping the pixel font for the game-facing menu and dialog.

## v0.1.5 -> v0.1.6

- Replaced the single pixel-font UI model with a two-role font policy: title font and text font.
- Added `ui.font_scale`, `ui.title_font_path`, and `ui.text_font_path` as the only font controls in `config/app.json`.
- Switched menu, dialog, FPS, debug overlay, and placeholder screens to Noto Sans text rendering while keeping a separate title font role.
- Removed unused font files from the repository and kept only the required Noto Sans regular/bold files with license/readme.
- Removed the unused UI helper that produced a compiler warning.

## v0.1.6 -> v0.1.7

- Changed placeholder screens so Esc returns to the main menu instead of opening the exit dialog from the unfinished game placeholder.
- Guarded window close requests so cancelling the confirmation dialog does not immediately reopen it on the next frame.
- Moved monitor size detection after raylib window initialization to avoid pre-window GLFW monitor warnings.

## v0.1.7 -> v0.1.8

- Added UI label files for English and Ukrainian localization with English as the default language.
- Added `language.current`, `language.directory`, and `--language=...` support for selecting UI language.
- Moved main menu, placeholders, FPS prefix, debug field labels, and exit dialog text/buttons out of hard-coded strings.

## v0.1.8 -> v0.1.9

- Added localized placeholder-screen actions for returning to the main menu or opening exit confirmation.
- Added keyboard and mouse navigation for placeholder actions so temporary screens are no longer dead ends.
- Added debug hover reporting for menu, dialog, and placeholder hit-testing.

## v0.1.9 -> v0.2.0

- Added the first main workspace screen with an old-school editor layout: large viewport, right-side tool panel, bottom status bar, and reference-inspired color scheme.
- Added lightweight map package inspection from `map.path` / `--map=...` without loading heavy runtime grids or starting 3D rendering yet.
- Added workspace labels for English and Ukrainian UI text, workspace tool navigation, map status display, and debug overlay map/workspace diagnostics.

## v0.2.0 -> v0.2.1

- Added bounded map package reader support for terrain overview grids, tile size, elevation range, and runtime-grid availability flags.
- Replaced the workspace wire-only placeholder with a diagnostic 2D map overview when terrain grid data is available.
- Extended workspace status/tool panels and English/Ukrainian labels with map source, tile, terrain, elevation, collision, and overview fields.

## v0.2.1 -> v0.2.2

- Changed startup flow to open the MainRender/workspace screen immediately and load the configured map package without showing the preliminary main menu.
- Changed Esc on MainRender to open the exit confirmation dialog; Enter on Yes exits, while Esc/No cancels and returns to MainRender.
- Moved workspace map/debug diagnostics out of the top-left viewport area into the right-lower service panel and added FPS plus process RSS memory to the status bar.

## v0.2.2 -> v0.2.3

- Fixed exit confirmation input so the same Esc/window-close event that opens the dialog cannot immediately cancel it in the same frame.
- Preserved MainRender as the startup screen while keeping Esc/close-button exit confirmation modal and cancel/accept behavior stable.

## v0.2.3 -> v0.2.4

- Restyled the right workspace panel after the retro 3D-editor reference without adding new renderer behavior.
- Added a header, compact command list, contextual subitems, and a bottom pseudo-button block to the tool panel.
- Added localized labels for the new panel subitems and footer button captions.
## v0.2.4 -> v0.2.5

- Removed the decorative bottom pseudo-button block from the right workspace panel.
- Changed the right panel into a click-driven accordion menu: clicking a section opens or closes its subitems, while mouse hover only reports hover state.
- Added initial 2D layer toggles for terrain, elevation, collision, and grid through the accordion subitems.

## v0.2.5 -> v0.2.6

- Added the first `vox3d_core` static library target and moved map package loading out of the editor executable.
- Reworked map package inspection around the real TopDownMapGen v0.0.68 package layout, starting from `map.json` and known layer/catalog/render/object files.
- Added a lightweight `RuntimeMap` shell for future voxel/chunk/mesh builders while keeping the editor UI behavior unchanged.


## v0.2.6 -> v0.2.7

- Expanded `RuntimeMap` into a real editor-independent data layer with dense terrain, collision, and height grids.
- Added runtime map validation, start/goal extraction, blocked-cell counts, and height-range calculation from loaded grid data.
- Wired the editor startup to build and log the new runtime map while keeping UI and renderer behavior unchanged.

## v0.2.7 -> v0.2.8

- Added the first `ChunkGrid` foundation in `vox3d_core`, splitting runtime maps into fixed tile chunks.
- Added per-chunk bounds, blocked-cell counts, height ranges, dirty flags, validation, and compact chunk-grid logging.
- Wired editor startup to build and log the chunk grid while keeping 3D, voxel meshes, and rendering behavior unchanged.
- Fixed the unused `MapTileText` warning by showing tile size in the workspace info panel.

## v0.2.8 -> v0.2.9

- Added the first `VoxelWorld` foundation in `vox3d_core`, building compact implicit voxel columns from runtime map height, terrain, and collision grids.
- Added block types, voxel block lookup helpers, world validation, and voxel-world statistics for columns, solid blocks, empty blocks, and blocked columns.
- Wired editor startup to build and log the voxel world while keeping 3D rendering, face culling, greedy meshing, and block destruction unchanged.

## v0.2.9 -> v0.3.0

- Added voxel face-visibility analysis in `vox3d_core`, counting visible and culled faces without creating renderer resources.
- Added per-direction visible face counters, cull-ratio diagnostics, and compact face-visibility logging.
- Wired editor startup to build and log face-visibility statistics while keeping 3D rendering, mesh buffers, greedy meshing, and block destruction unchanged.

## v0.3.0 -> v0.3.1

- Added renderer-independent per-chunk mesh data generation in `vox3d_core`, emitting visible voxel faces as indexed quads without creating raylib resources.
- Added chunk mesh summaries for generated faces, vertices, indices, non-empty chunks, and validation diagnostics.
- Wired editor startup to build and log chunk mesh data while keeping 3D rendering, camera controls, greedy meshing, and dirty-chunk rebuilds unchanged.

## v0.3.1 -> v0.3.2

- Added the first raylib-backed 3D preview path as a separate `vox3d_raylib` layer on top of renderer-independent chunk mesh data.
- Uploaded non-empty chunk meshes into per-chunk raylib models, preserving `vox3d_core` as renderer-independent logic.
- Added F3 and workspace View panel switching between the existing 2D map overview and the new 3D mesh preview.

## v0.3.2 -> v0.3.3

- Added a raylib free-fly 3D preview camera with WASD movement, Q/E vertical movement, Shift/Ctrl speed modifiers, RMB mouse look, velocity smoothing, reset, and fit-to-map controls.
- Wired the workspace View panel and footer hints to the new 3D camera controls while keeping `vox3d_core` renderer-independent.
- Fixed the workspace info panel to report loaded runtime terrain/elevation/collision data instead of layer visibility toggles.

## v0.3.3 -> v0.3.4

- Added 3D debug overlay toggles for chunk bounds, world grid, collision cells, and sampled height markers in the raylib preview path.
- Wired the Render workspace panel and hotkeys F4/F5/F6/F7 to the new overlay flags without changing `vox3d_core` mesh generation.
- Extended the workspace info panel and footer hints with chunk/face diagnostics and 3D overlay controls.

## v0.3.4 -> v0.3.5

- Reworked the 3D preview camera into click-to-capture mouse-look: clicking the 3D viewport captures the cursor, while Esc releases it before any exit dialog is opened.
- Fixed free-fly strafe direction so A moves left and D moves right relative to the current view direction.
- Changed 3D fit-view to use the actual viewport aspect ratio when entering 3D mode or pressing F, so the whole map starts framed inside the canvas.
- Added compact camera diagnostics for capture state, yaw/pitch, and position in the workspace info panel and camera log output.

## v0.3.5 -> v0.3.6

- Reworked the right workspace panel into a mode-oriented tree with top-level 2D map, 3D world, selection, package data, debug, and settings sections.
- Added nested row metadata for groups, actions, checkboxes, radios, and value rows so future selection/picking data has a stable place in the UI.
- Removed the decorative VoX3D title from the top of the right panel and starts the menu directly from the tree rows.

## v0.3.6 -> v0.3.7

- Added greedy chunk meshing in `vox3d_core` while keeping the previous simple visible-face mesh builder as a selectable baseline.
- Added mesh optimization diagnostics for naive faces, culled faces, simple faces, greedy faces, active vertices/indices, draw models, and saved-face ratios.
- Wired the workspace 3D mesh panel, F8 hotkey, status bar, info panel, and logs to report measurable render/mesh profit when switching mesh modes or debug overlays.

## v0.3.7 -> v0.3.8

- Added selectable 16x16 and 32x32 chunk-size rebuild modes while preserving the active simple/greedy mesh mode.
- Rebuilt ChunkGrid, VoxelWorld, face visibility, simple meshes, greedy meshes, and raylib preview upload when chunk size changes.
- Added chunk-size comparison diagnostics for total chunks, draw models, active faces, and before/after percentage deltas in logs, status, and workspace info.

## v0.3.8 -> v0.3.9

- Added renderer-independent chunk mesh cache data in `vox3d_core` with per-chunk dirty flags.
- Added dirty chunk rebuild reports for rebuilt/reused chunks, saved rebuild work, and face/vertex/index deltas.
- Wired the editor to keep simple/greedy mesh caches, expose a dirty rebuild probe, and log measurable cache rebuild profit.

## v0.3.9 -> v0.4.0

- Added a renderer-independent stepped terrain mesh builder in `vox3d_core` that emits tile top surfaces and only the required cliff/wall faces instead of full voxel columns.
- Added `Terrain Surface` as a selectable 3D mesh mode alongside the existing simple and greedy voxel mesh baselines.
- Added terrain mesh diagnostics for top faces, wall faces, total terrain faces, and terrain-vs-greedy deltas in the workspace panel, status/info output, and logs.

## v0.4.0 -> v0.4.1

- Replaced the overloaded right-side workspace panel with explicit `Menu`, `Stats`, `Inspect`, and `Help` tabs.
- Moved noisy map, mesh, dirty-cache, and camera diagnostics out of the main menu into the dedicated `Stats` tab.
- Shortened the bottom status bar and preserved the 3D camera view when switching mesh mode, chunk size, overlays, or 2D/3D mode.

## v0.4.1 -> v0.4.2

- Fixed mouse-capture release so Esc is consumed before any exit confirmation can be opened while the 3D camera owns the cursor.
- Added F2 as an explicit mouse-release hotkey for the 3D camera capture mode.
- Updated the workspace Help tab to document the mouse-release behavior without changing the rendering or mesh pipeline.

## v0.4.2 -> v0.4.3

- Added span merging for stepped terrain top surfaces so adjacent tiles with the same height and block type are emitted as one larger quad.
- Added one-dimensional span merging for terrain wall/cliff faces along chunk rows or columns while preserving the existing terrain surface mesh mode.
- Added terrain raw-vs-merged diagnostics for top faces, wall faces, total terrain faces, and merge-saved ratios in logs and the workspace Stats tab.

## v0.4.3 -> v0.4.4

- Added 3D diagnostic color modes for material, geographic elevation, chunk id, and face type previews without changing mesh geometry.
- Added deterministic geographic elevation colors for map levels and reuploads active chunk meshes when switching color mode.
- Wired the workspace menu, F11 hotkey, status bar, Stats tab, and render logs to expose the active color visualization mode.


## v0.4.4 -> v0.4.5

- Added render visibility modes for all chunks, soft radius fade, and hard chunk culling in the 3D preview.
- Added per-frame visibility diagnostics for resident, visible, fade, hidden, drawn, and culled chunks plus drawn/skipped faces.
- Wired the workspace menu, F12 hotkey, Stats tab, status bar, and render logs to show measurable visibility/culling profit without changing mesh generation.

## v0.4.5 -> v0.4.6

- Added a renderer-independent chunk visibility module in `vox3d_core` with radius, hard-cull, and frustum AABB classification.
- Added `Frustum Cull` as a selectable 3D visibility mode while keeping the existing soft radius fade and hard-cull debug modes.
- Wired visibility stats, logs, menu selection, F12 cycling, and raylib drawing through the shared engine visibility report instead of local UI-only classification.

## v0.4.6 -> v0.4.7

- Split terrain surface meshes into explicit render-pass metadata for tops, regular walls, and cliffs while keeping one renderer-independent terrain build result.
- Updated the raylib preview upload path to create separate terrain pass models per chunk, exposing the real draw-model cost of pass separation.
- Added workspace menu toggles and Stats diagnostics for terrain passes, including merged top/wall/cliff counts and active pass flags.

## v0.4.7 -> v0.4.8

- Added a renderer-independent transition feature foundation in `vox3d_core` that derives ramp, stair, bridge-reserved, and drop markers from runtime height/collision grids.
- Wired transition diagnostics into the chunk pipeline logs and workspace Stats tab with total, kind, passable, and blocked counters.
- Added 3D transition overlay toggles for ramps, stairs, bridges, and drops, drawing lightweight debug markers without changing terrain mesh, collision, pathfinding, or movement rules.

## v0.4.8 -> v0.4.9

- Fixed right Menu tab overflow by adding a scrollable workspace menu list.
- Moved transition controls above terrain passes so the new overlay controls are visible earlier.
- Added a `T` hotkey for toggling the transition overlay without using the right panel.

## v0.4.9 -> v0.5.0

- Added renderer-independent tile inspection in `vox3d_core`, combining runtime terrain/elevation/collision, chunk ownership, and transition counters for one selected tile.
- Added 3D viewport tile picking from a viewport-correct camera ray with heightfield ray marching and a plane fallback for maps without height data.
- Wired left-click selection into the workspace Inspect tab and highlighted the selected tile in the 3D preview without adding map editing, object picking, pathfinding, or movement changes.


## v0.5.0 -> v0.5.1

- Fixed 3D tile picking so the mouse ray mirrors raylib's actual full render-target projection while still rejecting clicks outside the 3D canvas.
- Aligned frustum visibility aspect-ratio calculations with the same render projection used by the 3D preview.
- Updated picking documentation to describe the current scissored-preview projection behavior without changing map inspection, terrain mesh generation, or selection UI.

## v0.5.1 -> v0.5.2

- Added a renderer-independent 4-way movement probe in `vox3d_core` that explains passable and blocked neighbour steps around the selected tile.
- Wired movement diagnostics into the Inspect and Stats tabs, including pass/block counts, height deltas, transition kind, and block reasons.
- Added a 3D movement probe overlay and `M` hotkey without adding pathfinding, player movement, animation, or map editing.

## v0.5.2 -> v0.5.3

- Added renderer-independent map-wide passability validation in `vox3d_core`, reusing movement rules to audit local edges across the whole runtime map.
- Added validation counters for checked, passable, blocked, invalid, suspicious drop, blocked ramp/stair, isolated-tile, and stored issue counts.
- Wired validation diagnostics into logs, workspace Menu, Stats, Inspect, 3D issue overlay, and `V` hotkey without adding pathfinding, player movement, or map auto-fixes.


## v0.5.3 -> v0.5.4

- Fixed the broken passability validation patch by adding the missing `passability_validator` core source and header files referenced by CMake and UI code.
- Kept map-wide passability validation renderer-independent, with checked/passable/blocked edge counters, issue storage, and compact logging.
- Preserved the existing validation menu, Stats, Inspect, and 3D overlay wiring without adding new gameplay behavior.

## v0.5.4 -> v0.5.5

- Added validation execution policy controls: Off, Manual, and On Load, with Manual as the default to keep map loading responsive.
- Added cached passability validation report state with explicit Run Validation and Clear Report actions, status text, last-run duration, and footer/Stats/Inspect reporting.
- Stopped running full-map passability validation unconditionally during chunk pipeline rebuilds; existing reports are reused until the user clears or reruns validation.

## v0.5.5 -> v0.5.6

- Fixed the unused `BoolText` helper warning in `ui_draw.cpp` so the normal `./r` build no longer reports that dead function.
- Added right mouse button release for captured 3D camera mouse state, matching the existing Escape/F2 release behaviour without opening the exit dialog.
- Updated Help text to document the new RMB release shortcut without changing camera capture, picking, validation, or pathfinding behaviour.

## v0.5.6 -> v0.5.7

- Added a renderer-independent weighted A* path probe in `vox3d_core` with Shortest and Safe cost profiles.
- Added start/goal path endpoint selection, Run Path, Clear Path, Show Path, and Show Visited workspace controls plus the `P` hotkey.
- Added 3D route and visited-node overlays with Stats/Inspect cost breakdowns for base, terrain, elevation, transition, and safety cost components.
