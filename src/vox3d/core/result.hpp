#pragma once

#include <string>
#include <utility>
#include <vector>

namespace vox3d {

/**
 * @brief Minimal diagnostic container for recoverable library operations.
 */
struct Diagnostics {
    std::vector<std::string> warnings;

    /**
     * @brief Adds a non-fatal warning message.
     *
     * @param message Human-readable warning text.
     */
    void AddWarning(std::string message) { warnings.push_back(std::move(message)); }

    /**
     * @brief Returns true when no warnings were recorded.
     *
     * @return True if the diagnostics container is empty.
     */
    [[nodiscard]] bool Empty() const { return warnings.empty(); }
};

}  // namespace vox3d
