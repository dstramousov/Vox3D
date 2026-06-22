#include "menu.hpp"

#include <ostream>

namespace vox3d {

MainMenu::MainMenu(const UiLabels& labels)
{
    state_.items = {
        {MenuItemId::kNewGame, labels.menu_new_game, true},
        {MenuItemId::kLoadGame, labels.menu_load_game, false},
        {MenuItemId::kSettings, labels.menu_settings, true},
        {MenuItemId::kExit, labels.menu_exit, true},
    };
    state_.selected_index = FindNextEnabled(0, 1);
}

void MainMenu::ApplyLabels(const UiLabels& labels)
{
    for (auto& item : state_.items) {
        switch (item.id) {
            case MenuItemId::kNewGame:
                item.title = labels.menu_new_game;
                break;
            case MenuItemId::kLoadGame:
                item.title = labels.menu_load_game;
                break;
            case MenuItemId::kSettings:
                item.title = labels.menu_settings;
                break;
            case MenuItemId::kExit:
                item.title = labels.menu_exit;
                break;
        }
    }
}


void MainMenu::SetItemEnabled(MenuItemId id, bool enabled)
{
    for (auto& item : state_.items) {
        if (item.id != id) {
            continue;
        }
        item.enabled = enabled;
        break;
    }

    if (state_.selected_index < 0 || state_.selected_index >= static_cast<int>(state_.items.size())
        || !state_.items[static_cast<std::size_t>(state_.selected_index)].enabled) {
        state_.selected_index = FindNextEnabled(0, 1);
    }
}

bool MainMenu::SelectPrevious()
{
    const int next = FindNextEnabled(state_.selected_index - 1, -1);
    if (next < 0 || next == state_.selected_index) {
        return false;
    }
    state_.selected_index = next;
    return true;
}

bool MainMenu::SelectNext()
{
    const int next = FindNextEnabled(state_.selected_index + 1, 1);
    if (next < 0 || next == state_.selected_index) {
        return false;
    }
    state_.selected_index = next;
    return true;
}

bool MainMenu::SelectByIndex(int index)
{
    if (index < 0 || index >= static_cast<int>(state_.items.size())) {
        return false;
    }
    if (!state_.items[static_cast<std::size_t>(index)].enabled) {
        return false;
    }
    if (state_.selected_index == index) {
        return false;
    }
    state_.selected_index = index;
    return true;
}

const MenuItem* MainMenu::SelectedItem() const
{
    if (state_.selected_index < 0 || state_.selected_index >= static_cast<int>(state_.items.size())) {
        return nullptr;
    }
    const auto& item = state_.items[static_cast<std::size_t>(state_.selected_index)];
    if (!item.enabled) {
        return nullptr;
    }
    return &item;
}

const MenuState& MainMenu::State() const
{
    return state_;
}

int MainMenu::FindNextEnabled(int start_index, int direction) const
{
    const int count = static_cast<int>(state_.items.size());
    if (count == 0 || EnabledCount() == 0) {
        return -1;
    }

    int index = start_index;
    for (int step = 0; step < count; ++step) {
        if (index < 0) {
            index = count - 1;
        } else if (index >= count) {
            index = 0;
        }

        if (state_.items[static_cast<std::size_t>(index)].enabled) {
            return index;
        }
        index += direction;
    }

    return -1;
}

int MainMenu::EnabledCount() const
{
    int count = 0;
    for (const auto& item : state_.items) {
        if (item.enabled) {
            ++count;
        }
    }
    return count;
}

std::string_view ToString(MenuItemId id)
{
    switch (id) {
        case MenuItemId::kNewGame:
            return "new_game";
        case MenuItemId::kLoadGame:
            return "load_game";
        case MenuItemId::kSettings:
            return "settings";
        case MenuItemId::kExit:
            return "exit";
    }
    return "unknown";
}

std::ostream& operator<<(std::ostream& os, const MenuState& state)
{
    int enabled_count = 0;
    for (const auto& item : state.items) {
        if (item.enabled) {
            ++enabled_count;
        }
    }

    os << "MenuState { selected_index=" << state.selected_index << " item_count=" << state.items.size()
       << " enabled_count=" << enabled_count;
    if (state.selected_index >= 0 && state.selected_index < static_cast<int>(state.items.size())) {
        os << " selected_id=" << ToString(state.items[static_cast<std::size_t>(state.selected_index)].id);
    }
    os << " }";
    return os;
}

}  // namespace vox3d
