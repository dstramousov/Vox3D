#include "process_metrics.hpp"

#if defined(__linux__)
#include <fstream>
#include <sstream>
#include <string>
#endif

namespace vox3d {

ProcessMemoryInfo QueryProcessMemoryInfo()
{
#if defined(__linux__)
    std::ifstream status("/proc/self/status");
    if (!status) {
        return {};
    }

    std::string line;
    while (std::getline(status, line)) {
        if (line.rfind("VmRSS:", 0) != 0) {
            continue;
        }

        std::istringstream input(line.substr(6));
        std::uint64_t value_kb = 0;
        std::string unit;
        input >> value_kb >> unit;
        if (value_kb == 0 || unit != "kB") {
            return {};
        }
        return ProcessMemoryInfo{true, value_kb * 1024ULL};
    }
#endif
    return {};
}

}  // namespace vox3d
