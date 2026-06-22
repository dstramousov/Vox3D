#pragma once

#include <cstdint>

namespace vox3d {

/**
 * @brief Process memory usage snapshot.
 */
struct ProcessMemoryInfo {
    bool available = false;
    std::uint64_t resident_bytes = 0;
};

/**
 * @brief Queries current process resident memory usage.
 *
 * The value is best-effort and may be unavailable on unsupported platforms.
 *
 * @return Current process memory snapshot.
 */
[[nodiscard]] ProcessMemoryInfo QueryProcessMemoryInfo();

}  // namespace vox3d
