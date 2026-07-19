#include "vox3d/map/map_package.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <optional>
#include <regex>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace vox3d {
namespace {

constexpr std::uintmax_t kMaxMetadataReadBytes = 1024U * 1024U;
constexpr std::uintmax_t kMaxGridReadBytes = 32U * 1024U * 1024U;
constexpr std::size_t kMaxOverviewCells = 512U * 512U;

[[nodiscard]] bool IsBlankPath(const std::filesystem::path& path)
{
    return path.empty() || path.string().empty();
}

[[nodiscard]] std::string ReadTextFileLimited(
    const std::filesystem::path& path,
    std::uintmax_t max_bytes,
    std::string& warning)
{
    std::error_code size_error;
    const std::uintmax_t file_size = std::filesystem::file_size(path, size_error);
    if (size_error) {
        warning = "failed to stat file path=\"" + path.string() + "\" reason=\"" + size_error.message() + "\"";
        return {};
    }
    if (file_size > max_bytes) {
        warning = "file too large for lightweight read path=\"" + path.string() + "\" size=" + std::to_string(file_size);
        return {};
    }

    std::ifstream file(path);
    if (!file) {
        warning = "failed to read file path=\"" + path.string() + "\"";
        return {};
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

[[nodiscard]] std::optional<int> ExtractIntByKeys(const std::string& text, std::initializer_list<const char*> keys)
{
    for (const char* key : keys) {
        const std::regex pattern("\\\"" + std::string(key) + "\\\"\\s*:\\s*(-?[0-9]+)");
        std::smatch match;
        if (std::regex_search(text, match, pattern) && match.size() > 1) {
            try {
                return std::stoi(match[1].str());
            } catch (...) {
                return std::nullopt;
            }
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<long long> ExtractLongLongByKeys(const std::string& text, std::initializer_list<const char*> keys)
{
    for (const char* key : keys) {
        const std::regex pattern("\\\"" + std::string(key) + "\\\"\\s*:\\s*(-?[0-9]+)");
        std::smatch match;
        if (std::regex_search(text, match, pattern) && match.size() > 1) {
            try {
                return std::stoll(match[1].str());
            } catch (...) {
                return std::nullopt;
            }
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::string ExtractStringByKeys(const std::string& text, std::initializer_list<const char*> keys)
{
    for (const char* key : keys) {
        const std::regex pattern("\\\"" + std::string(key) + "\\\"\\s*:\\s*\\\"([^\\\"]*)\\\"");
        std::smatch match;
        if (std::regex_search(text, match, pattern) && match.size() > 1) {
            return match[1].str();
        }
    }
    return {};
}

[[nodiscard]] std::filesystem::path RelativeToPackage(const std::filesystem::path& package_path, const std::filesystem::path& path)
{
    std::error_code error;
    const std::filesystem::path relative = std::filesystem::relative(path, package_path, error);
    if (error) {
        return path.filename();
    }
    return relative;
}

void ExtractMetadata(MapPackageInfo& info, const std::filesystem::path& source_path, const std::string& text)
{
    if (!info.width.has_value()) {
        info.width = ExtractIntByKeys(text, {"width_tiles", "width", "map_width", "cols", "columns"});
    }
    if (!info.height.has_value()) {
        info.height = ExtractIntByKeys(text, {"height_tiles", "height", "map_height", "rows"});
    }
    if (!info.tile_size.has_value()) {
        info.tile_size = ExtractIntByKeys(text, {"tile_size_px", "tile_size", "tile_px"});
    }
    if (!info.min_level.has_value()) {
        info.min_level = ExtractIntByKeys(text, {"min_level", "min_elevation", "level_min", "z_min"});
    }
    if (!info.max_level.has_value()) {
        info.max_level = ExtractIntByKeys(text, {"max_level", "max_elevation", "level_max", "z_max"});
    }
    if (!info.resolved_seed.has_value()) {
        info.resolved_seed = ExtractLongLongByKeys(text, {"resolved_seed"});
    }
    if (info.schema_version.empty()) {
        info.schema_version = ExtractStringByKeys(text, {"schema_version"});
    }
    if (info.package_schema_version.empty()) {
        info.package_schema_version = ExtractStringByKeys(text, {"package_schema_version"});
    }
    if (info.generator_version.empty()) {
        info.generator_version = ExtractStringByKeys(text, {"generator_version"});
    }
    if (info.pipeline_version.empty()) {
        info.pipeline_version = ExtractStringByKeys(text, {"pipeline_version"});
    }
    if (info.profile.empty()) {
        info.profile = ExtractStringByKeys(text, {"profile"});
    }
    info.metadata_available = info.width.has_value() || info.height.has_value() || info.tile_size.has_value()
        || info.min_level.has_value() || info.max_level.has_value() || !info.schema_version.empty()
        || !info.generator_version.empty();
    const std::string relative_source = RelativeToPackage(info.path, source_path).string();
    if (info.source_file.empty() || relative_source == "map.json") {
        info.source_file = relative_source;
    }
}

[[nodiscard]] bool Exists(const std::filesystem::path& path)
{
    std::error_code error;
    return std::filesystem::exists(path, error) && !error;
}

[[nodiscard]] std::string FileStatus(const std::filesystem::path& package_path, std::string_view relative)
{
    if (Exists(package_path / relative)) {
        return std::string(relative);
    }
    return {};
}

void DiscoverKnownFiles(MapPackageInfo& info)
{
    constexpr std::array<std::string_view, 23> known_files{
        "map.json",
        "runtime_grids.json",
        "layers/tile_grid.json",
        "layers/terrain.json",
        "layers/elevation.json",
        "layers/collision.json",
        "layers/movement_costs.json",
        "layers/start_goal.json",
        "catalogs/tile_types.json",
        "catalogs/object_types.json",
        "render/tile_render_hints.json",
        "render/object_render_hints.json",
        "objects/runtime_objects.json",
        "objects/places.json",
        "markers.json",
        "world_graph.json",
        "routes.json",
        "gameplay_zones.json",
        "elevation_model.json",
        "elevation_features.json",
        "elevation_transitions.json",
        "gameplay/spawn_rules.json",
        "gameplay/object_rules.json",
    };

    for (std::string_view relative : known_files) {
        const std::string present = FileStatus(info.path, relative);
        if (!present.empty()) {
            info.present_files.push_back(present);
        }
    }

    info.runtime_grids_available = std::any_of(info.present_files.begin(), info.present_files.end(), [](const std::string& file) {
        return file == "runtime_grids.json";
    });
    info.terrain_available = std::any_of(info.present_files.begin(), info.present_files.end(), [](const std::string& file) {
        return file == "layers/terrain.json" || file == "layers/tile_grid.json";
    });
    info.elevation_available = std::any_of(info.present_files.begin(), info.present_files.end(), [](const std::string& file) {
        return file == "layers/elevation.json" || file == "elevation_model.json";
    });
    info.collision_available = std::any_of(info.present_files.begin(), info.present_files.end(), [](const std::string& file) {
        return file == "layers/collision.json" || file == "runtime_grids.json";
    });
}

void TryReadMetadata(MapPackageInfo& info)
{
    constexpr std::array<std::string_view, 3> metadata_candidates{
        "map.json",
        "runtime_grids.json",
        "layers/terrain.json",
    };

    for (std::string_view relative : metadata_candidates) {
        const std::filesystem::path candidate = info.path / relative;
        if (!Exists(candidate)) {
            continue;
        }

        std::string warning;
        const std::string text = ReadTextFileLimited(candidate, kMaxMetadataReadBytes, warning);
        if (!warning.empty()) {
            info.warnings.push_back(warning);
            continue;
        }
        ExtractMetadata(info, candidate, text);
        if (relative == std::string_view{"map.json"}) {
            return;
        }
    }
}

[[nodiscard]] std::string NormalizeToken(std::string_view value)
{
    std::string normalized;
    normalized.reserve(value.size());
    for (unsigned char c : value) {
        if (std::isalnum(c) != 0) {
            normalized.push_back(static_cast<char>(std::tolower(c)));
        }
    }
    return normalized;
}

[[nodiscard]] MapCellKind ClassifyTerrainToken(std::string_view raw_value)
{
    const std::string value = NormalizeToken(raw_value);
    if (value.empty()) {
        return MapCellKind::kUnknown;
    }
    if (value.find("start") != std::string::npos || value.find("spawn") != std::string::npos) {
        return MapCellKind::kStart;
    }
    if (value.find("goal") != std::string::npos || value.find("exit") != std::string::npos) {
        return MapCellKind::kGoal;
    }
    if (value.find("water") != std::string::npos || value.find("river") != std::string::npos || value.find("lake") != std::string::npos) {
        return MapCellKind::kWater;
    }
    if (value.find("swamp") != std::string::npos || value.find("marsh") != std::string::npos) {
        return MapCellKind::kSwamp;
    }
    if (value.find("forest") != std::string::npos || value.find("tree") != std::string::npos || value.find("woods") != std::string::npos) {
        return MapCellKind::kForest;
    }
    if (value.find("road") != std::string::npos || value.find("path") != std::string::npos) {
        return MapCellKind::kRoad;
    }
    if (value.find("ruin") != std::string::npos) {
        return MapCellKind::kRuins;
    }
    if (value.find("building") != std::string::npos || value.find("bunker") != std::string::npos || value.find("house") != std::string::npos || value.find("wall") != std::string::npos) {
        return MapCellKind::kBuilding;
    }
    if (value.find("block") != std::string::npos || value.find("solid") != std::string::npos || value.find("collision") != std::string::npos) {
        return MapCellKind::kBlocked;
    }
    if (value.find("open") != std::string::npos || value.find("grass") != std::string::npos || value.find("ground") != std::string::npos || value.find("plain") != std::string::npos || value.find("floor") != std::string::npos || value.find("sand") != std::string::npos || value.find("dirt") != std::string::npos) {
        return MapCellKind::kOpen;
    }
    return MapCellKind::kUnknown;
}

[[nodiscard]] bool IsStringTokenInteresting(std::string_view raw_value)
{
    return ClassifyTerrainToken(raw_value) != MapCellKind::kUnknown;
}

[[nodiscard]] std::optional<std::string> ExtractArrayAfterKey(const std::string& text, std::string_view key)
{
    const std::string pattern = "\"" + std::string(key) + "\"";
    std::size_t key_pos = 0;
    while ((key_pos = text.find(pattern, key_pos)) != std::string::npos) {
        std::size_t pos = text.find('[', key_pos + pattern.size());
        if (pos == std::string::npos) {
            return std::nullopt;
        }

        int depth = 0;
        bool in_string = false;
        bool escaped = false;
        for (std::size_t i = pos; i < text.size(); ++i) {
            const char c = text[i];
            if (in_string) {
                if (escaped) {
                    escaped = false;
                } else if (c == '\\') {
                    escaped = true;
                } else if (c == '"') {
                    in_string = false;
                }
                continue;
            }

            if (c == '"') {
                in_string = true;
            } else if (c == '[') {
                ++depth;
            } else if (c == ']') {
                --depth;
                if (depth == 0) {
                    return text.substr(pos, i - pos + 1);
                }
            }
        }
        key_pos += pattern.size();
    }
    return std::nullopt;
}

[[nodiscard]] std::vector<std::string> ExtractQuotedStrings(const std::string& text)
{
    std::vector<std::string> values;
    bool in_string = false;
    bool escaped = false;
    std::string current;

    for (const char c : text) {
        if (!in_string) {
            if (c == '"') {
                in_string = true;
                current.clear();
            }
            continue;
        }

        if (escaped) {
            switch (c) {
                case 'n':
                    current.push_back('\n');
                    break;
                case 'r':
                    current.push_back('\r');
                    break;
                case 't':
                    current.push_back('\t');
                    break;
                default:
                    current.push_back(c);
                    break;
            }
            escaped = false;
            continue;
        }
        if (c == '\\') {
            escaped = true;
            continue;
        }
        if (c == '"') {
            values.push_back(current);
            in_string = false;
            continue;
        }
        current.push_back(c);
    }
    return values;
}

[[nodiscard]] std::optional<std::pair<int, int>> InferNestedStringGridShape(const std::string& array_text)
{
    std::vector<int> row_counts;
    int depth = 0;
    int current_row_count = 0;
    bool in_string = false;
    bool escaped = false;
    std::string current;

    for (const char c : array_text) {
        if (in_string) {
            if (escaped) {
                current.push_back(c);
                escaped = false;
            } else if (c == '\\') {
                escaped = true;
            } else if (c == '"') {
                if (depth >= 2 && IsStringTokenInteresting(current)) {
                    ++current_row_count;
                }
                current.clear();
                in_string = false;
            } else {
                current.push_back(c);
            }
            continue;
        }

        if (c == '"') {
            in_string = true;
            current.clear();
        } else if (c == '[') {
            ++depth;
            if (depth == 2) {
                current_row_count = 0;
            }
        } else if (c == ']') {
            if (depth == 2 && current_row_count > 0) {
                row_counts.push_back(current_row_count);
            }
            --depth;
        }
    }

    if (row_counts.empty()) {
        return std::nullopt;
    }
    const int width = row_counts.front();
    if (width <= 0) {
        return std::nullopt;
    }
    const bool consistent = std::all_of(row_counts.begin(), row_counts.end(), [width](int value) { return value == width; });
    if (!consistent) {
        return std::nullopt;
    }
    return std::pair<int, int>{width, static_cast<int>(row_counts.size())};
}

[[nodiscard]] std::vector<MapCellKind> ExtractTerrainCells(const std::string& array_text)
{
    const std::vector<std::string> strings = ExtractQuotedStrings(array_text);
    std::vector<MapCellKind> cells;
    cells.reserve(strings.size());
    for (const auto& value : strings) {
        cells.push_back(ClassifyTerrainToken(value));
    }
    return cells;
}

[[nodiscard]] std::optional<std::string> FindBestGridArray(const std::string& text)
{
    constexpr std::array<std::string_view, 9> keys{
        "rows",
        "grid",
        "data",
        "tiles",
        "tile_grid",
        "terrain_grid",
        "terrain",
        "runtime_grid",
        "values",
    };

    for (std::string_view key : keys) {
        auto section = ExtractArrayAfterKey(text, key);
        if (section.has_value() && section->find('"') != std::string::npos) {
            return section;
        }
    }
    return std::nullopt;
}

void TryReadOverview(MapPackageInfo& info)
{
    if (info.width.has_value() && info.height.has_value()
        && *info.width > 0 && *info.height > 0) {
        const std::size_t cell_count = static_cast<std::size_t>(*info.width)
            * static_cast<std::size_t>(*info.height);
        if (cell_count > kMaxOverviewCells) {
            info.warnings.push_back(
                "map overview skipped: map too large for diagnostic preview cells="
                + std::to_string(cell_count));
            return;
        }
    }

    constexpr std::array<std::string_view, 3> grid_candidates{
        "layers/terrain.json",
        "layers/tile_grid.json",
        "runtime_grids.json",
    };

    for (std::string_view relative : grid_candidates) {
        const std::filesystem::path candidate = info.path / relative;
        if (!Exists(candidate)) {
            continue;
        }

        std::string warning;
        const std::string text = ReadTextFileLimited(candidate, kMaxGridReadBytes, warning);
        if (!warning.empty()) {
            info.warnings.push_back(warning);
            continue;
        }

        ExtractMetadata(info, candidate, text);
        std::optional<std::string> array_section = FindBestGridArray(text);
        if (!array_section.has_value()) {
            continue;
        }

        std::optional<std::pair<int, int>> inferred_shape = InferNestedStringGridShape(*array_section);
        if ((!info.width.has_value() || !info.height.has_value()) && inferred_shape.has_value()) {
            info.width = inferred_shape->first;
            info.height = inferred_shape->second;
        }
        if (!info.width.has_value() || !info.height.has_value() || *info.width <= 0 || *info.height <= 0) {
            info.warnings.push_back("map overview skipped: grid dimensions unavailable source=\"" + std::string(relative) + "\"");
            continue;
        }

        const std::size_t cell_count = static_cast<std::size_t>(*info.width) * static_cast<std::size_t>(*info.height);
        if (cell_count > kMaxOverviewCells) {
            info.warnings.push_back(
                "map overview skipped: map too large for diagnostic preview cells="
                + std::to_string(cell_count));
            return;
        }

        std::vector<MapCellKind> cells = ExtractTerrainCells(*array_section);
        if (cells.size() < cell_count) {
            info.warnings.push_back(
                "map overview skipped: not enough terrain cells source=\"" + std::string(relative) + "\" cells="
                + std::to_string(cells.size()) + " expected=" + std::to_string(cell_count));
            continue;
        }
        cells.resize(cell_count);

        info.overview.width = *info.width;
        info.overview.height = *info.height;
        info.overview.cells = std::move(cells);
        info.overview.source_file = std::string(relative);
        info.overview.terrain_loaded = true;
        info.terrain_available = true;
        info.metadata_available = true;
        return;
    }
}

void TryReadElevationRange(MapPackageInfo& info)
{
    if (info.min_level.has_value() && info.max_level.has_value()) {
        return;
    }

    constexpr std::array<std::string_view, 3> elevation_candidates{
        "layers/elevation.json",
        "runtime_grids.json",
        "elevation_model.json",
    };

    for (std::string_view relative : elevation_candidates) {
        const std::filesystem::path candidate = info.path / relative;
        if (!Exists(candidate)) {
            continue;
        }

        std::string warning;
        const std::string text = ReadTextFileLimited(candidate, kMaxGridReadBytes, warning);
        if (!warning.empty()) {
            info.warnings.push_back(warning);
            continue;
        }

        ExtractMetadata(info, candidate, text);
        if (info.min_level.has_value() && info.max_level.has_value()) {
            info.elevation_available = true;
            return;
        }

        bool found_any = false;
        int min_value = 0;
        int max_value = 0;
        auto add_value = [&](int value) {
            if (!found_any) {
                min_value = value;
                max_value = value;
                found_any = true;
            } else {
                min_value = std::min(min_value, value);
                max_value = std::max(max_value, value);
            }
        };

        if (const std::optional<int> default_level = ExtractIntByKeys(text, {"default"}); default_level.has_value()) {
            add_value(*default_level);
        }

        const std::regex level_pattern("\"level\"\\s*:\\s*(-?[0-9]+)");
        for (auto it = std::sregex_iterator(text.begin(), text.end(), level_pattern); it != std::sregex_iterator(); ++it) {
            add_value(std::stoi((*it)[1].str()));
        }

        if (!found_any) {
            auto array_section = ExtractArrayAfterKey(text, "height_grid");
            if (!array_section.has_value()) {
                array_section = ExtractArrayAfterKey(text, "grid");
            }
            if (array_section.has_value()) {
                const std::regex integer_pattern("-?[0-9]+");
                for (auto it = std::sregex_iterator(array_section->begin(), array_section->end(), integer_pattern);
                     it != std::sregex_iterator();
                     ++it) {
                    add_value(std::stoi(it->str()));
                }
            }
        }

        if (found_any) {
            info.min_level = min_value;
            info.max_level = max_value;
            info.elevation_available = true;
            return;
        }
    }
}

[[nodiscard]] int CountTopLevelObjectsInArray(std::string_view array_text)
{
    int depth = 0;
    int count = 0;
    bool in_string = false;
    bool escaped = false;
    for (const char c : array_text) {
        if (in_string) {
            if (escaped) {
                escaped = false;
            } else if (c == '\\') {
                escaped = true;
            } else if (c == '"') {
                in_string = false;
            }
            continue;
        }
        if (c == '"') {
            in_string = true;
        } else if (c == '{') {
            if (depth == 0) {
                ++count;
            }
            ++depth;
        } else if (c == '}') {
            --depth;
        }
    }
    return count;
}

void TryReadItemCounts(MapPackageInfo& info)
{
    struct CountCandidate {
        std::string_view path;
        std::optional<int> MapPackageInfo::*field;
    };
    constexpr std::array<CountCandidate, 3> candidates{
        CountCandidate{"objects/runtime_objects.json", &MapPackageInfo::object_count},
        CountCandidate{"objects/places.json", &MapPackageInfo::place_count},
        CountCandidate{"markers.json", &MapPackageInfo::marker_count},
    };

    for (const CountCandidate& candidate : candidates) {
        const std::filesystem::path path = info.path / candidate.path;
        if (!Exists(path)) {
            continue;
        }
        std::string warning;
        const std::string text = ReadTextFileLimited(path, kMaxMetadataReadBytes, warning);
        if (!warning.empty()) {
            info.warnings.push_back(warning);
            continue;
        }
        auto array = ExtractArrayAfterKey(text, "items");
        if (array.has_value()) {
            info.*(candidate.field) = CountTopLevelObjectsInArray(*array);
        }
    }
}

[[nodiscard]] std::string BuildStatus(const MapPackageInfo& info)
{
    if (!info.configured) {
        return "not_configured";
    }
    if (!info.exists) {
        return "missing";
    }
    if (!info.is_directory) {
        return "not_directory";
    }
    if (!info.metadata_available) {
        return "package_found_metadata_unavailable";
    }
    return "loaded";
}

}  // namespace

bool MapOverview::IsValid() const
{
    return terrain_loaded && width > 0 && height > 0 && cells.size() == static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
}

MapPackageInfo LoadMapPackageInfo(const std::filesystem::path& package_path)
{
    MapPackageInfo info;
    info.path = package_path;
    info.configured = !IsBlankPath(package_path);

    if (!info.configured) {
        info.status = BuildStatus(info);
        return info;
    }

    std::error_code error;
    info.exists = std::filesystem::exists(info.path, error);
    if (error) {
        info.warnings.push_back("failed to check map package path=\"" + info.path.string() + "\" reason=\"" + error.message() + "\"");
        info.status = BuildStatus(info);
        return info;
    }

    if (!info.exists) {
        info.status = BuildStatus(info);
        return info;
    }

    info.is_directory = std::filesystem::is_directory(info.path, error);
    if (error) {
        info.warnings.push_back("failed to inspect map package path=\"" + info.path.string() + "\" reason=\"" + error.message() + "\"");
        info.status = BuildStatus(info);
        return info;
    }

    if (!info.is_directory) {
        info.status = BuildStatus(info);
        return info;
    }

    DiscoverKnownFiles(info);
    TryReadMetadata(info);
    TryReadOverview(info);
    TryReadElevationRange(info);
    TryReadItemCounts(info);
    info.loaded = true;
    info.status = BuildStatus(info);
    return info;
}

std::string ToLogString(const MapPackageInfo& info)
{
    std::ostringstream out;
    out << "path=\"" << info.path.string() << "\" status=" << info.status;
    if (info.width.has_value() && info.height.has_value()) {
        out << " size=" << *info.width << 'x' << *info.height;
    }
    if (info.tile_size.has_value()) {
        out << " tile=" << *info.tile_size;
    }
    if (info.min_level.has_value() && info.max_level.has_value()) {
        out << " levels=" << *info.min_level << ".." << *info.max_level;
    }
    if (!info.generator_version.empty()) {
        out << " generator=" << info.generator_version;
    }
    if (!info.schema_version.empty()) {
        out << " schema=" << info.schema_version;
    }
    if (!info.package_schema_version.empty()) {
        out << " package_schema=" << info.package_schema_version;
    }
    if (info.resolved_seed.has_value()) {
        out << " seed=" << *info.resolved_seed;
    }
    if (!info.source_file.empty()) {
        out << " metadata=" << info.source_file;
    }
    if (info.overview.IsValid()) {
        out << " overview=" << info.overview.source_file;
    }
    if (info.object_count.has_value()) {
        out << " objects=" << *info.object_count;
    }
    if (info.place_count.has_value()) {
        out << " places=" << *info.place_count;
    }
    if (info.marker_count.has_value()) {
        out << " markers=" << *info.marker_count;
    }
    out << " files=" << info.present_files.size();
    return out.str();
}

}  // namespace vox3d
