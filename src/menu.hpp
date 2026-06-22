#pragma once

#include "ui_labels.hpp"

#include <iosfwd>
#include <string>
#include <string_view>
#include <vector>

namespace vox3d {

/**
 * @brief Stable menu item identifier.
 */
enum class MenuItemId {
    kNewGame,
    kLoadGame,
    kSettings,
    kExit,
};

/**
 * @brief Main menu item state and metadata.
 */
struct MenuItem {
    MenuItemId id;
    std::string title;
    bool enabled = true;
};

/**
 * @brief Main menu runtime state.
 */
struct MenuState {
    std::vector<MenuItem> items;
    int selected_index = 0;
};

/**
 * @brief Handles main menu navigation and activation.
 */
class MainMenu {
public:
    /**
     * @brief Creates a menu with localized default items.
     *
     * @param labels Localized labels used for visible menu titles.
     */
    explicit MainMenu(const UiLabels& labels);

    /**
     * @brief Updates visible menu titles from localized labels.
     *
     * @param labels Localized labels used for visible menu titles.
     */
    void ApplyLabels(const UiLabels& labels);

    /**
     * @brief Enables or disables a menu item by stable identifier.
     *
     * If the selected item becomes disabled, selection moves to the next enabled item.
     *
     * @param id Item identifier.
     * @param enabled New enabled state.
     */
    void SetItemEnabled(MenuItemId id, bool enabled);

    /**
     * @brief Moves selection to the previous enabled item.
     *
     * @return True if selection changed.
     */
    [[nodiscard]] bool SelectPrevious();

    /**
     * @brief Moves selection to the next enabled item.
     *
     * @return True if selection changed.
     */
    [[nodiscard]] bool SelectNext();

    /**
     * @brief Selects an enabled item by index.
     *
     * @param index Item index.
     * @return True if selection changed.
     */
    [[nodiscard]] bool SelectByIndex(int index);

    /**
     * @brief Returns the currently selected menu item.
     *
     * @return Selected menu item, or nullptr if no enabled item exists.
     */
    [[nodiscard]] const MenuItem* SelectedItem() const;

    /**
     * @brief Returns read-only menu state.
     *
     * @return Menu state.
     */
    [[nodiscard]] const MenuState& State() const;

private:
    [[nodiscard]] int FindNextEnabled(int start_index, int direction) const;
    [[nodiscard]] int EnabledCount() const;

    MenuState state_;
};

/**
 * @brief Converts a menu item identifier to a stable lowercase name.
 *
 * @param id Menu item identifier.
 * @return String representation.
 */
[[nodiscard]] std::string_view ToString(MenuItemId id);

/**
 * @brief Writes a compact debug representation of the menu state.
 *
 * @param os Destination stream.
 * @param state Menu state to dump.
 * @return Destination stream.
 */
std::ostream& operator<<(std::ostream& os, const MenuState& state);

}  // namespace vox3d
