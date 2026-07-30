## v0.5.82

- Fixed a shutdown crash introduced by the experimental tree instancing shader.
- All tree models and the shared instancing shader are now released before `CloseWindow()` destroys the OpenGL context.
- Added an idempotent `RaylibChunkMeshPreview::Shutdown()` path used by both application shutdown and the destructor.
- Suppressed noisy `-Wmissing-field-initializers` diagnostics originating from the third-party `raymath.h` header.

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

## v0.5.7 -> v0.5.8

- Reworked path endpoint selection so left click only selects a tile, while `S` assigns the selected tile as Start and `G` assigns it as Goal.
- Added right-panel Path Probe actions for Set Selected as Start and Set Selected as Goal, removing the unreliable Shift+left-click dependency.
- Added `X` as a Clear Path hotkey and updated Help text for the new endpoint workflow without changing the weighted A* pathfinding algorithm.

## v0.5.8 -> v0.5.9

- Replaced the conflicting `S`/`G` path endpoint hotkeys with a path pick tool mode driven by `1` for Pick Start and `2` for Pick Goal.
- Made left click apply the active path pick tool once and then return to normal Select mode, so endpoint picking no longer depends on Shift or WASD-adjacent keys.
- Updated Path Probe menu rows, Stats/Inspect tool reporting, Clear Path behaviour, and Help text without changing the weighted A* pathfinding algorithm or route overlay.

## v0.5.9 -> v0.5.10

- Replaced the fragile separate `1`/`2` endpoint workflow with a modal two-click path picking mode started by `P`.
- Made the first path-pick left click set Start, the second left click set Goal, and the path probe run automatically after Goal is selected.
- Disabled 3D camera mouse capture while path picking is active, with RMB/Escape cancelling the pick mode, so endpoint selection no longer conflicts with camera capture/release.

## v0.5.10 -> v0.5.11

- Reworked the right-side workspace Menu into semantic collapsible sections.
- Kept the View section focused on Reset View by dropping the redundant 3D Fit Map menu action.
- Added stable marker/label columns for aligned ASCII checkbox, radio, action, and group rows.

## v0.5.11 -> v0.5.12

- Started the workspace directly in 3D Preview mode when a mesh is available.
- Kept the Mode menu section expanded by default while all other 3D menu sections start collapsed.
- Tightened the initial 3D camera fit so the map starts centered and much closer to the viewport edges.

## v0.5.12 -> v0.5.13

- Replaced the right-panel button-like tab row with ASCII text tabs: `[View] [Stats] [Info]`.
- Drew inactive tabs with plain light text and the active tab with yellow pseudo-bold text.
- Removed Help from the visible tab cycle, keeping keyboard navigation limited to View, Stats, and Info.

## v0.5.13 -> v0.5.14

- Increased spacing between the right-panel ASCII tabs so `[View]`, `[Stats]`, and `[Info]` no longer visually stick together.
- Kept the existing text-tab rendering, active yellow pseudo-bold state, and tab click behaviour unchanged.

## v0.5.14 -> v0.5.15

- Reworked the Stats tab body into the same ASCII tree visual style as the View tab.
- Rendered Stats sections with marker and label columns using the workspace menu font size.
- Kept Stats content read-only while preserving existing View, Info, and tab navigation behavior.
## v0.5.15 -> v0.5.16

- Stats panel root rows now use the same collapsible `[-]` / `[+]` markers as View.
- Stats sections can be collapsed and expanded by clicking their headers.
- Stats layout uses the shared panel row hit-testing and scroll calculation.
## v0.5.16 -> v0.5.17

- Reworked the Info tab body into the same collapsible ASCII tree style used by View and Stats.
- Added runtime-only Info section collapse state for Selection, Transitions, Movement, Path, and Validation.
- Shared right-panel scrolling and group hit-testing across View, Stats, and Info without changing inspection data.

## v0.5.17 -> v0.5.18

- Set the startup workspace to 3D Preview with geographic coloring and frustum culling.
- Added a one-shot startup camera fly-in from an overhead frame into the default overview pose.
- Added `window.fullscreen` config support and displayed the application version in the status bar.

## v0.5.18 -> v0.5.19

- Tuned the startup cinematic final 3D pose toward the closer corner-focused overview.
- Delayed the fly-in briefly on the overhead frame and clamped its first-frame delta so slow startup loading no longer skips the animation.
- Let early camera input cancel the startup cinematic while keeping Reset View as an immediate jump to the final overview pose.

## v0.5.19 -> v0.5.20

- Added red V/S/I hotkey hints to the right-panel `[View]`, `[Stats]`, and `[Info]` tabs.
- Added direct V/S/I keyboard shortcuts for switching the right-panel tabs when the 3D camera cursor is not captured.
- Kept tab switching from resetting 2D/3D view state, collapsed groups, and scroll state.


## v0.5.20 -> v0.5.21

- Enlarged and centered the right-panel View/Stats/Info tab header while preserving red V/S/I hotkey hints.
- Reduced Stats and Info value-row indentation so values start closer to their section headers.
- Added hover tooltips for Stats/Info rows whose full text does not fit in the right panel.

## v0.5.21 -> v0.5.22

- Renamed the 3D color mode from Material to Traversal to match its actual terrain/traversal overlay role.
- Added terrain/traversal surface categories to terrain mesh vertices and faces so the overlay can color grass, slow terrain, blocked terrain, water/wet terrain, tree blockers, start, and goal separately.
- Prevented terrain-surface greedy merging from combining different traversal categories, preserving meaningful overlay colors across large top faces.

## v0.5.22 -> v0.5.23

- Added runtime object marker extraction from `objects/runtime_objects.json` and vegetation markers from `render/vegetation_visual.json`.
- Added a 3D `Object Markers` overlay that draws simple colored pillars for trees, reeds, bushes, ruins, cover, loot, structures, and trenches above the terrain.
- Kept the object markers separate from the terrain mesh and limited their draw pass to visible chunks so Traversal/Geographic modes can be combined with object diagnostics.

## v0.5.23 -> v0.5.24

- Replaced thin vegetation/object marker lines with uniform colored cube markers so map objects remain visible in 3D overview.
- Kept marker size identical across trees, reeds, bushes, ruins, cover, loot, structures, and unknown objects so color remains the primary type hint.
- Continued drawing object markers only for visible chunks and kept them separate from the terrain mesh.

## v0.5.24 -> v0.5.25

- Removed the always-visible Reset View action from the right-panel View section and made the Mode section collapsed by default.
- Moved object marker control out of Display into a separate Objects section with per-type filters.
- Made all object marker filters disabled by default; users can enable All Objects or individual Trees, Bushes, Reeds, Ruins, Cover, Loot/Cache, Structures, Trenches, and Unknown markers.

## v0.5.25 -> v0.5.26
- Fixed the Objects filter section so it is a normal top-level View panel group instead of a permanently expanded nested row.
- Changed initial panel state so all View, Stats, Info, and 2D menu sections start collapsed by default.
## v0.5.26 -> v0.5.27

- Removed unused Terrain and Probes groups from the 3D View panel.
- Reworked Path picking around F3: first click sets Start, second click sets Goal and runs pathfinding.
- Added Path status/start/goal rows in the Path panel and explicit status bar prompts while picking.
- Kept Path closed by default and preserved existing 2D/3D camera state behavior.

## v0.5.27 -> v0.5.28

- Added a vxmap-runtime-v1 binary reader skeleton that validates map_runtime.vxmap header, section table, section CRCs, required global sections, region index, terrain catalog, and map.json build_id metadata.
- Added runtime_binary discovery from map.json and package logging for the binary fast path.
- Kept RuntimeMap construction on the existing JSON loader for now; binary validation only records whether the fast path is usable and falls back safely to JSON on any mismatch.

## v0.5.28 -> v0.5.29

- Added vxmap-runtime-v1 binary core loading into RuntimeMap for terrain, elevation, collision, and start/goal.
- Kept JSON loader as a safe fallback and continued loading high-level objects/vegetation from JSON.
- Runtime map log now reports runtime_binary=loaded when the binary core path is used.
- Fixed WorkspacePanelItem switch warnings for Objects filter entries.
- Made label JSON parser tolerate an optional UTF-8 BOM before the root object.

## v0.5.29 -> v0.5.30

- Fixed label JSON loading by keeping the parsed file buffer alive for the flat string JSON parser.
- Removed the startup `labels: parse failed ... reason="expected object"` warning for valid `res/lang/en.json` files.
- Kept label loading behavior fallback-safe: malformed language files still keep built-in defaults and report diagnostics.

## v0.5.30 -> v0.5.31

- Added optional binary-vs-JSON runtime core verification controlled by `VOX3D_VERIFY_BINARY_JSON=1`.
- The verifier compares terrain, collision, elevation, start, and goal tile-by-tile after a successful `.vxmap` load.
- Runtime map logging now reports `binary_vs_json=ok` or mismatch counters plus JSON load/compare timings when verification is enabled.
- If verification detects a mismatch or JSON core is unavailable, the loader rejects the binary fast path and falls back to JSON safely.


## v0.5.31 -> v0.5.32

- Added startup profiling logs for map package load, RuntimeMap build, chunk pipeline, font load, layout, camera setup, and total startup time.
- Added chunk pipeline profiling logs for chunk grid, voxel world, face visibility, simple mesh, greedy mesh, terrain mesh, transitions, render upload, and total pipeline time.
- Kept loading/rendering behavior unchanged; this patch only adds timing diagnostics for the next performance pass.


## v0.5.32 -> v0.5.33

- Added an initial visible-mesh-only startup path for large maps.
- Large maps now build the first mesh subset around the start/focus chunk and leave far chunks empty for follow-up streaming work.
- Added `VOX3D_INITIAL_CHUNK_RADIUS` to force or disable the initial chunk build radius for diagnostics.
- Extended chunk pipeline profiling with initial/full mesh mode, initial chunk count, and pending chunk count.

## v0.5.33 -> v0.5.34

- Replaced the aggressive start-chunk partial mesh path with a safer 256x256-tile initial build window for maps larger than 256x256 tiles.
- The initial window is centered on the map so the startup camera has drawable terrain immediately after upload.
- Added `VOX3D_INITIAL_TILE_WINDOW` to override the initial tile window size; `0` disables the limit and forces a full mesh build.
- Updated chunk pipeline logs to report `initial_area`, `initial_window`, selected initial chunks, and skipped chunks.

## v0.5.34 -> v0.5.35

- Limited initial face-visibility analysis to the same 256x256-tile startup window used by partial mesh builds.
- Kept neighbor checks against the full voxel world so window-edge meshes preserve the same culling semantics as regular chunk builds.
- Reduced large-map startup work in `initial_area=window` mode without changing full-build behavior.


## v0.5.35 -> v0.5.36

- Added a fast map-package inspection path when `map.json` declares `runtime_binary=map_runtime.vxmap`.
- Skipped diagnostic reads of heavy core JSON grid files on the `.vxmap` fast path: runtime grids, terrain/tile/elevation/collision layers, and elevation model.
- Marked terrain, elevation, collision, and runtime grids as available through the binary runtime package metadata instead of probing large JSON files.
- Extended the map-package log with `fast_vxmap=yes` and the skipped core JSON file list for startup diagnostics.

## v0.5.36 -> v0.5.37

- Added a startup window-corner camera pose for large maps that use the 256x256 initial tile window.
- The initial camera now targets the built startup window instead of framing the full large map, so the first visible view is guaranteed to contain rendered chunks.
- Added `VOX3D_STARTUP_CORNER=nw|ne|sw|se` to choose the startup window corner; the default is `se`.
- Added `VOX3D_STARTUP_VIEW=map` or `VOX3D_STARTUP_VIEW=flyin` to force the older full-map startup fly-in for diagnostics.
- Added `startup_view` logging with corner, initial window, camera position, and target diagnostics.

## v0.5.37 -> v0.5.38

- Added progressive chunk build after the initial 256x256 startup window is shown.
- Pending chunks are queued by distance from the startup window and built in small per-frame batches.
- Added VOX3D_CHUNK_BUILDS_PER_FRAME and VOX3D_DISABLE_PROGRESSIVE_BUILD controls.
- Added incremental raylib chunk upload for progressive builds without unloading existing models.
- Status bar now shows built/total chunk progress.

## v0.5.38 -> v0.5.39

- Changed progressive chunk building to prioritize chunks near the current camera target/look direction instead of consuming the original startup-window ring order.
- Added a camera lookahead score so newly built chunks tend to appear where the user is looking first.
- Reduced the default progressive build rate to 1 chunk per frame; `VOX3D_CHUNK_BUILDS_PER_FRAME` still overrides it.
- Extended progressive build logs with camera/map priority, target tile, and lookahead tile diagnostics.

## v0.5.39 -> v0.5.40

- Replaced progressive full-map chunk build with an interest-based render chunk cache.
- Progressive startup now builds only chunks inside the camera interest radius and idles when the nearby area is ready.
- Added render chunk budget, hard limit, keep radius, and per-frame eviction controls for large maps.
- Distant resident chunks can be evicted from GPU/render mesh while CPU runtime map data remains loaded.
- Status bar now reports resident chunks instead of implying the whole map must be built.

## v0.5.40 -> v0.5.41

- Changed progressive eviction to use the hard render chunk limit instead of evicting immediately above the soft budget.
- Added a progressive build time budget with cooldown to reduce repeated heavy chunk-build spikes.
- Added `VOX3D_PROGRESSIVE_TIME_BUDGET_MS` for tuning the per-frame progressive build budget.
- Kept `versions.md` updates appended to the end of the file.


## v0.5.41 -> v0.5.42

- Reworked the VXMAP fast path to read the container once and decode from the same validated memory buffer.
- Removed the second header, section-table, required-section, and CRC validation pass during runtime core loading.
- Added VXMAP read, validation, decode, and total load timings to runtime-map diagnostics.

## v0.5.42 -> v0.5.43

- Removed the permanent Info tab from the workspace side panel.
- Tile selection no longer changes the currently active workspace panel.
- Added a modal selection information overlay opened with `I` and closed with `Esc`.
- Added keyboard and mouse-wheel scrolling for long selection reports while the overlay blocks map input.

## v0.5.43 -> v0.5.44

- Removed the obsolete 3D View section and its Reset View action from the workspace side panel.
- Removed unreachable legacy 3D panel item identifiers and handlers while preserving camera hotkeys and startup camera behavior.

## v0.5.44 -> v0.5.45

- Replaced the fixed 2D thumbnail renderer with an interactive Camera2D-based map view.
- Added cursor-anchored mouse-wheel zoom, middle-button panning, fit, reset, and panel zoom actions.
- Added a one-pixel-per-tile nearest-neighbour terrain texture instead of drawing thousands of rectangles every frame.
- Added zoom-dependent tile/chunk grid rendering and shared 2D/3D tile selection with the existing `I` information overlay.
- Added 2D hover tile coordinates and zoom level to the workspace status bar.

## v0.5.45 -> v0.5.46

- Built the 2D terrain overview from the already-loaded runtime terrain grid when the lightweight package overview is absent.
- Restored 2D terrain rendering for the VXMAP fast path without re-reading skipped JSON terrain files.
- Reused one terrain-token classifier for JSON and VXMAP terrain names, including current bush and decorative terrain tokens.
- Added explicit 2D texture failure reasons and logged the selected overview source.

## v0.5.46 -> v0.5.47

- Added selectable Terrain, Elevation, and Collision base textures to the interactive 2D map view.
- Added a semantic elevation palette for levels `-5..20` and a high-contrast passability palette for collision cells.
- Changed 2D base-layer controls to exclusive radio selection and enabled them from loaded runtime-grid availability.
- Added the active 2D base-layer name to the workspace status bar.


## v0.5.47 -> v0.5.48

- Decoded movement cost, projectile blocking, vision blocking, cover, and concealment grids from the existing validated VXMAP memory buffer.
- Added selectable 2D diagnostic maps for all five runtime grids without additional JSON reads on the VXMAP fast path.
- Added per-layer color legends and hovered-tile values to the 2D workspace.
- Extended the `I` selection overlay and inspection logs with movement, blocking, cover, and concealment values.

## v0.5.48 -> v0.5.49

- Added independent 2D overlays for render chunks, VXMAP regions, and runtime Start/Goal points.
- Added zoom-dependent chunk and region labels while keeping the tile grid separate from partition overlays.
- Added `Home` and `End` navigation to center the 2D map on Start and Goal at inspection scale.
- Extended the `I` selection overlay with VXMAP region coordinates and clipped region bounds.

## v0.5.49 -> v0.5.50

- Loaded runtime objects, places, and semantic markers from the real map-package JSON files into normalized runtime data.
- Enabled independent 2D overlays for Objects, Places, and Markers with viewport culling and zoom-dependent detail.
- Added object footprints, collision footprints, orientation hints, place bounds, entrances, marker glyphs, and labels at inspection zoom.
- Extended the `I` selection overlay with objects, places, and markers touching the selected tile.


## v0.5.50 -> v0.5.51

- Changed 2D map panning to left-button dragging while preserving cursor-anchored wheel zoom.
- Changed 2D tile selection to right-click so selection no longer conflicts with map dragging.
- Added middle-click tile inspection that selects the clicked tile and opens the existing `I` information overlay immediately.
- Updated workspace help text for the new 2D mouse controls.

## v0.5.51 -> v0.5.52

- Removed the persistent Stats tab from the workspace side panel.
- Added a live mode-specific statistics overlay opened with `S` and closed with `Esc` or `S`.
- Reorganized 2D and 3D diagnostics into compact two-column table cards with scrolling.

## v0.5.52 -> v0.5.53

- Fixed Stats overlay compilation against the actual `LevelRange` and `ChunkGridInfo` field names.
- Removed unused workspace-layout parameter warnings after the persistent Stats tab was deleted.
- Preserved the dynamic 2D/3D Stats overlay behavior introduced in v0.5.52.

## v0.5.53 -> v0.5.54

- Restored the `camera_status` and `labels` parameter names in the legacy Stats panel builder so its remaining call sites compile.
- Marked the now-unused workspace-layout camera and label parameters unnamed after the persistent Stats tab removal.
- Fixed the full application build regression introduced by the previous warning cleanup.

## v0.5.54 -> v0.5.55

- Resized the dynamic Stats overlay to exactly match the Selection Info overlay dimensions.
- Removed the bright per-section cards and borders from the Stats overlay.
- Kept the two-column table layout with compact section headings, aligned values, and scrolling when required.

## v0.5.55 -> v0.5.56

- Added an `F1` Help overlay using the same size, styling, scrolling, and modal input behavior as Selection Info and Stats.
- Separated all documented workspace controls into a complete 2D section followed by a complete 3D section.
- Made Help mutually exclusive with Selection Info and Stats, with `Esc` or `F1` closing the overlay.
- Established Help content as part of the hotkey contract so future control changes must update it in the same patch.

## v0.5.56 -> v0.5.57

- Aligned Help overlay actions to one fixed table column instead of spacing rows manually.
- Highlighted the hotkey column in orange while keeping descriptions in the normal text color.
- Preserved separate 2D and 3D control sections, modal sizing, and scrolling behavior.

## v0.5.57 -> v0.5.58

- Removed the duplicated 2D Navigation section from the side menu while keeping all navigation hotkeys documented in Help.
- Replaced the collapsible Mode menu with a fixed full-width `2D Map` / `3D View` switch at the top of the side panel.
- Removed the obsolete `View` header and centered both equal-width mode buttons within the side column.
- Updated the 3D `V` hotkey documentation after removing the old View-panel shortcut.

## v0.5.58 -> v0.5.59

- Added a compact right-click context menu for valid tiles in the 2D map with `Tile info` and `Go to 3D view` actions.
- Added a contextual 2D-to-3D camera transfer that targets the selected tile and derives camera distance from the current 2D viewport scale.
- Kept the normal global `3D View` switch behavior unchanged while preserving the current tile selection across the contextual transition.
- Updated Help so the 2D right mouse action documents both tile selection and context-menu opening.

## v0.5.59 -> v0.5.60

- Added a separate `Vegetation` overlay to the 2D map instead of mixing render-only vegetation markers with runtime objects.
- Displayed the exact vegetation-marker count in the 2D side menu and Stats overlay.
- Added vegetation entries to Selection Info for the selected tile while preserving runtime objects as a separate data source.
- Rendered vegetation markers by type above the current 2D base layer, with labels available at close zoom levels.

## v0.5.60 -> v0.5.61

- Added full `structure_height` runtime support for `map-package-map-v12`, including JSON loading and legacy zero-filled fallback for older maps.
- Added VXMAP 1.1 decoding for required regional section type 28 while preserving VXMAP 1.0 compatibility.
- Added binary-versus-JSON parity checks and runtime statistics for ruin heights 1, 2, and 3.
- Added a 2D `Ruin Height` base layer and exposed ground top, structure height, and structure top in Selection Info.
- Kept 3D terrain geometry unchanged so the new format can be verified independently before ruin mesh generation.

## v0.5.61 -> v0.5.62

- Built `structure_height` tiles as real ruin geometry in simple, greedy, and terrain-surface mesh modes.
- Extended voxel-world vertical bounds and block classification so ruin levels participate in face culling, chunk meshing, streaming, and visibility statistics.
- Added exposed ruin tops and side walls with cross-chunk neighbor checks and a dedicated brown-gray ruin color in traversal and geographic modes.
- Moved 3D picking, selection markers, collision markers, object markers, and contextual 2D-to-3D camera focus to the visible top of structure tiles.
- Added ruin top/wall mesh counters to logs and the Stats overlay while preserving zero-height behavior for legacy maps.

## v0.5.62 -> v0.5.63

- Added `map-package-map-v13` / VXMAP 1.2 fast-path support for required regional vegetation type and height sections.
- Added exact JSON fallback loading and optional binary-versus-JSON parity checks for both vegetation grids.
- Rebuilt 2D vegetation markers from map-provided type and height data while preserving the legacy visual-file fallback for older maps.
- Exposed vegetation type, height, top level, type counts, and height distributions in Selection Info, Stats, and runtime logs.
- Kept 3D vegetation rendering unchanged so the new format can be verified independently before geometry changes.

## v0.5.63 -> v0.5.64

- Rendered map-provided tree and bush vegetation as separate centered vertical pillars using each marker's stored height.
- Limited vegetation pillars to a 0.50 x 0.50 tile footprint so neighboring plants remain visually separate.
- Added a very dark green tree color and a light green bush color while leaving reeds and non-vegetation runtime-object markers unchanged.
- Kept the existing 3D Trees and Bushes filters, chunk visibility checks, terrain-relative placement, and legacy vegetation-height fallback behavior.

## v0.5.64 -> v0.5.65

- Replaced broad Geographic elevation bands with one explicit terrain color for every supported level from -5 through 20.
- Preserved the geographic water-to-lowland-to-highland progression while making adjacent terrain terraces visually distinguishable.
- Kept ruin, tree, bush, traversal, chunk, and face-type colors unchanged.

## v0.5.65 -> v0.5.66

- Replaced per-frame `DrawCubeV` calls for map-provided trees and bushes with static per-chunk GPU meshes.
- Kept each 0.50 x 0.50 vegetation pillar visually separate while preserving map-provided heights and existing colors.
- Reused the existing chunk visibility, progressive residency, and eviction pipeline for vegetation mesh upload, draw, and unload.
- Skipped the 53k vegetation-marker scan when only tree and bush filters are enabled, while preserving legacy maps and reed/runtime-object drawing.
- Added vegetation mesh upload and last-draw diagnostics for model, pillar, face, and draw-call counts.

## v0.5.66 -> v0.5.67

- Moved shore and puddle reed markers into the same static per-chunk vegetation mesh path used by trees and bushes.
- Preserved the existing compact yellow-green reed marker appearance while removing per-frame `DrawCubeV` calls for new vegetation-grid maps.
- Added reed mesh and pillar counters, visible vegetation chunk counts, and vegetation draw-call diagnostics to Stats and renderer logs.
- Kept legacy vegetation visuals and the small runtime-object set on the existing marker draw path.


## v0.5.67 -> v0.5.68

- Added non-blocking OpenGL timer-query diagnostics for averaged and peak 3D GPU frame time.
- Added GPU device, tracked mesh memory, driver VRAM, render workload, and streaming/upload sections to Stats - 3D.
- Added a compact GPU frame-time value to the lower-right status bar and highlighted version, FPS, GPU, and memory values in orange.
- Added logical GPU buffer accounting for terrain, ruins, trees, bushes, and reeds with current and peak usage.

## v0.5.68 -> v0.5.69

- Moved the Stats overlay hotkey from `S` to `F2`, restoring `S` exclusively to backward 3D camera movement.
- Removed the obsolete `F2` mouse-release shortcut while keeping right mouse and Escape release controls.
- Changed lower-right status values from orange to a high-contrast burgundy color.
- Ellipsized long Stats values to keep GPU renderer and other diagnostic strings inside their assigned column.

## v0.5.69 -> v0.5.70

- Added an experimental GLB tree renderer using three bundled low-poly tree models.
- Replaced procedural tree pillars with deterministic GLB instances when the experiment is enabled, while preserving bush and reed chunk meshes.
- Added stable per-tile model choice, rotation, scale, and small placement offsets with a configurable instance limit.
- Added configuration fields and Stats diagnostics for GLB asset, instance, and draw-call counts.


## v0.5.70 -> v0.5.71

- Fixed the experimental GLB tree renderer build by declaring the chunk visibility map helper before its first use.


## v0.5.71 -> v0.5.72

- Bundled the three experimental GLB tree assets in `assets/models/trees` so the runtime renderer can load them from the configured directory.
- Added explicit raylib log messages with full asset paths for successful and failed experimental tree model loads.
- Preserved the procedural tree fallback when any required GLB asset cannot be loaded.


## v0.5.72 -> v0.5.73

- Bundled the external `Textures/colormap.png` palette referenced by the experimental GLB tree models.
- Restored the authored tree materials instead of raylib's white missing-texture fallback.

## v0.5.73 -> v0.5.74

- Expanded the experimental forest renderer from three to eight bundled low-poly tree models.
- Added deterministic 12x12-tile forest profiles for broadleaf, conifer, mixed, young-growth, and irregular old forest patches.
- Added weighted per-tile model selection while preserving stable rotation, scale, and placement across runs and chunk reloads.


## v0.5.74 -> v0.5.75

- Added two original deterministic low-poly GLB tree assets generated specifically for VoX3D: a layered conifer and a broad spreading tree.
- Added the reproducible `tools/generate_vox3d_trees.py` asset generator with embedded PBR colors and ground-level pivots.
- Integrated the custom trees into weighted forest profiles while retaining the existing models as secondary variants.

## v0.5.75 -> v0.5.76

- Reworked the custom spreading tree branches into shorter, thinner, upward-curving segments contained inside the crown.
- Temporarily restricted experimental forests to the two original VoX3D tree models for clean visual evaluation.
- Simplified deterministic forest patches to broadleaf, conifer, and mixed profiles using only the custom assets.


## v0.5.76 -> v0.5.77

- Replaced the two provisional generated tree assets with three normalized bases extracted from the Blender forest scene.
- Added simple conifer, detailed conifer, and broadleaf models with embedded materials, ground-level pivots, and consistent runtime heights.
- Updated deterministic forest profiles to distribute the three new bases across broadleaf, conifer, and mixed patches.


## v0.5.77 -> v0.5.78

- Added six broadleaf and six conifer GLB silhouettes derived from the normalized Blender forest bases.
- Added a broadleaf variant with one large unified low-poly crown, plus tall, wide, young, crooked, detailed, and sparse silhouettes.
- Added two genuinely rare special trees: an ancient spreading broadleaf and a sparse dead conifer.
- Added deterministic weighted selection across broadleaf, conifer, and mixed 12x12-tile forest patches.
- Added `tools/generate_forest_tree_variants.py` to reproduce all fourteen runtime tree assets.


## v0.5.78 -> v0.5.79

- Removed the default 300-instance cap from the GLB tree renderer so every tree record in resident map chunks receives a model instance.
- Defined `vegetation_models.tree_limit = 0` as unlimited while retaining positive values as an optional diagnostic cap.
- Preserved existing chunk residency and frustum culling, so only trees in currently drawn chunks are submitted each frame.

## v0.5.79 -> v0.5.80

- Replaced one `DrawModelEx()` call per visible GLB tree with instanced batches.
- Tree transforms are grouped by model and visibility class, then submitted with `DrawMeshInstanced()`.
- Preserved chunk/frustum culling and fade tinting.
- Tree draw-call statistics now report actual instanced mesh submissions instead of tree count.

## v0.5.81

- Fixed invisible GLB trees after the instanced-rendering switch.
- Added the required OpenGL 3.3 instancing shader and bound `instanceTransform` to the raylib model-matrix attribute slot.
- Tree materials now use the shared instancing shader while preserving their diffuse colors.


## v0.5.82 -> v0.5.83

- Merged tree GLB parts by material so each tree variant uses only a few runtime meshes instead of dozens.
- Reduced instanced tree submissions without changing the existing silhouettes or placement logic.
- Re-encoded GLB index accessors as 16-bit where possible, removing raylib u32-to-u16 conversion warnings.
- Updated the forest variant generator to reproduce the optimized runtime assets.


## v0.5.83 -> v0.5.84

- Added distance-based instanced tree LOD with configurable near, far, and cull ranges.
- Added shared medium and far low-poly broadleaf/conifer proxy models.
- Preserved all fourteen detailed silhouettes in the near range while batching distant trees by species.
- Trees beyond the configured cull distance are no longer submitted.

## v0.5.84 -> v0.5.85

- Removed thirteen obsolete tree GLB assets that were no longer loaded by the runtime.
- Removed the two obsolete tree generators that recreated deleted experimental assets.
- Normalized the embedded colors of all eighteen runtime tree and LOD models so bark and foliage no longer collapse to near-black values.
- Added a deterministic four-variant foliage tint system while leaving trunks and dead materials unchanged.
- Added `tools/normalize_runtime_tree_palette.py` to reproduce and validate the runtime tree palette without regenerating geometry.

## v0.5.85 -> v0.5.86

- Fixed progressive darkening of instanced tree materials across frames.
- Tree tinting now uses a per-draw deep copy of the material-map array instead of mutating the model's shared material maps.
- Preserved deterministic foliage color variants, visibility fading, existing LOD selection, and tree batching.


## v0.5.86 -> v0.5.87

- Fixed the raylib 5.x build failure caused by the unavailable `MAX_MATERIAL_MAPS` macro.
- Tree tinting now temporarily changes only the diffuse map color for the draw call and restores the original color immediately afterward.
- Preserved deterministic foliage variants, visibility fading, LOD selection, and instanced batching without persistent material mutation.

## v0.5.87 -> v0.5.88

- Added deterministic altitude zoning for GLB trees.
- Tree species, density, scale, and foliage palette now change smoothly with elevation.
- High elevations favor smaller conifers and dead trees; summit trees are nearly eliminated.
- Added configurable altitude-zone control points under `vegetation_models.altitude_zoning`.

## v0.5.88 -> v0.5.89

- Added configurable directional Lambert lighting for instanced tree models.
- Added ambient and hemispheric terms so shaded tree faces remain readable.
- Added mild lower-crown darkening for foliage materials only.
- Preserved existing tree tinting, altitude zoning, LOD selection, and instancing.
- Added `vegetation_models.lighting` settings to `config/app.json`.

## v0.5.89 -> v0.5.90

- Added configurable flat-shading for foliage in the instanced tree shader.
- Reconstructed per-fragment face normals from world-position derivatives so low-poly crown facets remain visible even when source normals are smoothed.
- Kept trunk and branch shading on the original model normals.
- Added `vegetation_models.lighting.flat_foliage_shading` to `config/app.json`; `0.0` keeps smooth shading and `1.0` uses fully flat foliage facets.
- Preserved altitude zoning, foliage palettes, LOD selection, instancing, and tree culling.

## v0.5.90 -> v0.5.91

- Reworked the rounded deciduous near model into an asymmetric four-lobe canopy.
- Replaced rounded medium and far deciduous LOD silhouettes with angular asymmetric canopies.
- Reduced the single-crown selection weight while keeping the existing tree model set unchanged.

## v0.5.91 -> v0.5.92

- Added deterministic altitude-dependent foliage color zoning for deciduous and conifer trees.
- Replaced equal tint selection with configurable olive, ochre, cool, and brown probabilities.
- Blended adjacent altitude palettes spatially to remove visible horizontal color bands.
- Changed foliage tinting from channel multiplication to controlled target-color blending so yellow and ochre tones remain visible.
- Preserved the existing model set, LOD distances, instancing batches, flat shading, and tree density zoning.

## v0.5.92 -> v0.5.93

- Expanded JSON and VXMAP `structure_height` support from the legacy `0..3` range to the map-contract `0..255` range.
- Replaced height-1/2/3-only diagnostics with generic structure tile, block, minimum, maximum, and average statistics.
- Updated the 2D structure-height layer to visualize legal tall structures instead of marking values above 3 as errors.
- Preserved dynamic world-height, voxel, mesh, collision, and binary-versus-JSON behavior for existing maps.

## v0.5.93 -> v0.5.94

- Added `map-package-map-v16` runtime loading for `structure_type` and sparse 4x4 `structure_micro_geometry` layers.
- Added VXMAP runtime v1.3 support for regional structure-type and uint16 micro-mask sections while preserving v1.0-v1.2 compatibility.
- Added strict structure type/height/micro-mask validation, JSON-to-VXMAP parity checks, and legacy zero-grid fallback for older maps.
- Added structure micro-geometry diagnostics for typed tiles, full and partial masks, and occupied subtiles.
- Kept existing full-tile structure rendering unchanged; micro-mesh generation is reserved for the next iteration.

## v0.5.94 -> v0.5.95

- Added native 4x4 structure micro-mesh generation for `structure_micro_geometry` maps.
- Removed legacy full-tile structure columns when authoritative micro geometry is present.
- Integrated micro structures into simple, greedy, terrain, progressive, and dirty-rebuild mesh paths.
- Preserved structure type metadata on generated faces and vertices for future fortress materials.
- Kept legacy `structure_height` rendering unchanged for older maps without micro geometry.


## v0.5.95 -> v0.5.96

- Removed the dark visibility-ring tint from terrain, structures, and vegetation.
- Kept geographic and model colors identical across visible and fade chunks.
- Preserved existing chunk residency, fade-ring classification, frustum culling, and hidden-chunk behavior.

## v0.5.96 -> v0.5.97

- Corrected the remaining broad green bands in geographic mode, which were elevation-palette bands rather than visibility tinting.
- Unified land elevations 0 through 4 to one lowland green so ordinary terrain terraces no longer resemble renderer LOD rings.
- Preserved water-depth colors and introduced a gradual high-altitude transition for hills, rock, and snow levels.
- Kept chunk visibility, frustum culling, vegetation LOD distances, and model selection unchanged.

## v0.5.97 -> v0.5.98

- Restored the original distinct geographic colors for every land elevation from 0 through 20.
- Replaced hard circular tree LOD boundaries with deterministic per-tree radial transition bands.
- Added configurable near and far LOD transition widths while preserving the existing nominal radii and cull distance.
- Kept tree placement, foliage zoning, chunk visibility, and model assets unchanged.

## v0.5.98 -> v0.5.99

- Corrected medium and far conifer LOD crown orientation so simplified trees preserve the upright conifer silhouette.
- Normalized medium and far LOD width, height, and ground contact against each tree's selected near model.
- Matched simplified LOD trunk and foliage base colors to the corresponding near-model species palettes.
- Preserved deterministic transition bands, nominal LOD radii, cull distance, tree placement, and altitude zoning.


## v0.5.99 -> v0.5.100

- Added explicit deciduous, conifer, and dead-conifer visual families for tree LOD selection.
- Kept dead conifers on their original detailed model at every visible distance instead of replacing them with a live green conifer LOD.
- Normalized detailed, medium, and far live-tree materials to the same family palette before applying each instance's stable foliage tint.
- Added per-frame near, medium, far, dead-original, and culled GLB tree diagnostics.
- Preserved existing LOD radii, transition bands, tree placement, altitude zoning, and cull distance.

## v0.5.100 -> v0.5.101

- Removed the two universal medium/far tree LOD model families from runtime loading and drawing.
- Kept each visible tree on its original detailed model so distance changes no longer alter species, silhouette, or palette.
- Added adaptive detailed-tree culling driven by stable GPU frame time with CPU fallback, hysteresis, cooldown, and asymmetric quality steps.
- Added deterministic per-tree cull-distance dispersion so the distant forest thins gradually instead of ending on a circular boundary.
- Added configurable adaptive-cull limits and timing thresholds plus HUD diagnostics for current radius and drawn/culled trees.

## v0.5.101 -> v0.5.102

- Simplified the 3D status bar to visible/resident chunks, progressive loading, and drawn terrain faces.
- Removed redundant ready, view, mesh, color, visibility-mode, and drawn-model labels from the 3D status line.
- Hid inactive path and validation placeholders while preserving their diagnostics when those tools are used.
- Added explicit adaptive-tree controller states (`AUTO WAIT`, `AUTO HOLD`, `AUTO UP`, `AUTO DOWN`, and `AUTO OFF`) next to the current radius and drawn/culled tree counts.
- Kept `UP` and `DOWN` visible during the controller cooldown instead of exposing them for only one frame.
- Limited the left status text width so it cannot run underneath the right-side performance diagnostics.

## v0.5.102 -> v0.5.103

- Condensed the 3D chunk status to `visible/resident/total` and removed the separate progressive-loaded field.
- Replaced internal adaptive-tree state and raw draw/cull counters with the current radius in tiles and the visible-tree percentage.
- Changed the version prefix to the readable `v 0.5.103` form while preserving FPS, GPU time, face count, and process-memory diagnostics.
- Kept adaptive tree visibility, culling decisions, rendering, and performance behavior unchanged.

## v0.5.103 -> v0.5.104

- Added VXMAP runtime 1.5 decoding for regional walkway, parapet, and crenellation uint16 mask sections.
- Added JSON loading and validation for `structure_top_geometry` schema v3 while preserving legacy structure-micro v8 fallback tiles.
- Rendered walkway masks as explicit 4x4 top surfaces and crenellation/parapet bits as separate raised 1x1-subtile geometry above `structure_height`.
- Added the `Structure Top` diagnostic color mode: walkway blue, parapet yellow, crenellation red, with structure bodies and terrain muted.
- Added binary/JSON parity counters and runtime statistics for all three structure-top mask channels.

## v0.5.104 -> v0.5.105

- Added JSON compatibility for `structure-micro-geometry-layer-v9` and `structure-top-geometry-layer-v6`, including the new two-subtile crenellation rule.
- Accepted VXMAP 1.5 refined fortress footprints where structure type and micro occupancy replace stale coarse height ownership outside the new body contour.
- Reconstructed expanded round tower cells to one connected top level when the v9 footprint contains missing or overlapped source heights.
- Treated an intentionally empty `structure_top_geometry` layer as valid, producing flat structure tops without walkway, parapet, or crenellation overlays.
- Preserved loading and rendering behavior for older structure-micro v3-v8 and structure-top v3 maps.
