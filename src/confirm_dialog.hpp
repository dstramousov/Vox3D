#pragma once

#include <string_view>

namespace vox3d {

/**
 * @brief Modal dialog type displayed above the current screen.
 */
enum class ModalDialog {
    kNone,
    kExitConfirmation,
    kNewGameConfirmation,
};

/**
 * @brief Choice selected inside a confirmation dialog.
 */
enum class DialogChoice {
    kYes,
    kNo,
};

/**
 * @brief Runtime state for a modal confirmation dialog.
 */
struct ConfirmDialogState {
    ModalDialog type = ModalDialog::kNone;
    DialogChoice selected_choice = DialogChoice::kNo;
};

/**
 * @brief Converts a modal dialog type to a stable lowercase name.
 *
 * @param dialog Dialog type.
 * @return String representation.
 */
[[nodiscard]] std::string_view ToString(ModalDialog dialog);

/**
 * @brief Converts a dialog choice to a stable lowercase name.
 *
 * @param choice Dialog choice.
 * @return String representation.
 */
[[nodiscard]] std::string_view ToString(DialogChoice choice);

}  // namespace vox3d
