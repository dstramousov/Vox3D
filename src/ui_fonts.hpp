#pragma once

#include <raylib.h>

namespace vox3d {

/**
 * @brief Fonts used by the user interface renderer.
 *
 * The title font is intended for decorative headings. The text font is used for
 * menu items, dialogs, placeholders, FPS, and debug overlay text.
 */
struct UiFontSet {
    Font title{};
    Font text{};
};

}  // namespace vox3d
