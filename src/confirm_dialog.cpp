#include "confirm_dialog.hpp"

namespace vox3d {

std::string_view ToString(ModalDialog dialog)
{
    switch (dialog) {
        case ModalDialog::kNone:
            return "none";
        case ModalDialog::kExitConfirmation:
            return "exit_confirmation";
        case ModalDialog::kNewGameConfirmation:
            return "new_game_confirmation";
    }
    return "unknown";
}

std::string_view ToString(DialogChoice choice)
{
    switch (choice) {
        case DialogChoice::kYes:
            return "yes";
        case DialogChoice::kNo:
            return "no";
    }
    return "unknown";
}

}  // namespace vox3d
