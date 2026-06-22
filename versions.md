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
