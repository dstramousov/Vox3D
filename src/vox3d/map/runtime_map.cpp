#include "vox3d/map/runtime_map.hpp"
#include "vox3d/map/vxmap_runtime_reader.hpp"

#include <algorithm>
#include <bit>
#include <chrono>
#include <cstdlib>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <limits>
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

constexpr std::uintmax_t kMaxRuntimeGridReadBytes = 64U * 1024U * 1024U;

[[nodiscard]] bool Exists(const std::filesystem::path& path)
{
    std::error_code error;
    return std::filesystem::exists(path, error) && !error;
}

[[nodiscard]] std::string ReadTextFileLimited(
    const std::filesystem::path& path,
    std::uintmax_t max_bytes,
    Diagnostics& diagnostics)
{
    std::error_code size_error;
    const std::uintmax_t file_size = std::filesystem::file_size(path, size_error);
    if (size_error) {
        diagnostics.AddWarning("failed to stat runtime map file path=\"" + path.string() + "\" reason=\"" + size_error.message() + "\"");
        return {};
    }
    if (file_size > max_bytes) {
        diagnostics.AddWarning(
            "runtime map file too large path=\"" + path.string() + "\" size=" + std::to_string(file_size)
            + " limit=" + std::to_string(max_bytes));
        return {};
    }

    std::ifstream file(path);
    if (!file) {
        diagnostics.AddWarning("failed to read runtime map file path=\"" + path.string() + "\"");
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

[[nodiscard]] std::optional<std::string> ExtractStringByKeys(const std::string& text, std::initializer_list<const char*> keys)
{
    for (const char* key : keys) {
        const std::regex pattern("\\\"" + std::string(key) + "\\\"\\s*:\\s*\\\"([^\\\"]*)\\\"");
        std::smatch match;
        if (std::regex_search(text, match, pattern) && match.size() > 1) {
            return match[1].str();
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<bool> ExtractBoolByKeys(const std::string& text, std::initializer_list<const char*> keys)
{
    for (const char* key : keys) {
        const std::regex pattern("\\\"" + std::string(key) + "\\\"\\s*:\\s*(true|false)");
        std::smatch match;
        if (std::regex_search(text, match, pattern) && match.size() > 1) {
            return match[1].str() == "true";
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::string ToLowerAscii(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

[[nodiscard]] bool ContainsToken(std::string_view text, std::string_view token)
{
    return text.find(token) != std::string_view::npos;
}

[[nodiscard]] std::optional<std::string> ExtractBalancedAfterKey(
    const std::string& text,
    std::string_view key,
    char open_char,
    char close_char)
{
    const std::string pattern = "\"" + std::string(key) + "\"";
    std::size_t key_pos = 0;
    while ((key_pos = text.find(pattern, key_pos)) != std::string::npos) {
        const std::size_t pos = text.find(open_char, key_pos + pattern.size());
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
            } else if (c == open_char) {
                ++depth;
            } else if (c == close_char) {
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

[[nodiscard]] std::optional<std::string> ExtractArrayAfterKey(const std::string& text, std::string_view key)
{
    return ExtractBalancedAfterKey(text, key, '[', ']');
}

[[nodiscard]] std::optional<std::string> ExtractObjectAfterKey(const std::string& text, std::string_view key)
{
    return ExtractBalancedAfterKey(text, key, '{', '}');
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

[[nodiscard]] std::vector<int> ExtractIntegers(const std::string& text)
{
    std::vector<int> values;
    const std::regex integer_pattern("-?[0-9]+");
    for (auto it = std::sregex_iterator(text.begin(), text.end(), integer_pattern); it != std::sregex_iterator(); ++it) {
        try {
            values.push_back(std::stoi(it->str()));
        } catch (...) {
            return {};
        }
    }
    return values;
}

[[nodiscard]] std::vector<std::string> ExtractTopLevelObjectsFromArray(std::string_view array_text)
{
    std::vector<std::string> objects;
    int object_depth = 0;
    bool in_string = false;
    bool escaped = false;
    std::size_t object_start = std::string_view::npos;

    for (std::size_t i = 0; i < array_text.size(); ++i) {
        const char c = array_text[i];
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
            if (object_depth == 0) {
                object_start = i;
            }
            ++object_depth;
        } else if (c == '}') {
            --object_depth;
            if (object_depth == 0 && object_start != std::string_view::npos) {
                objects.emplace_back(array_text.substr(object_start, i - object_start + 1));
                object_start = std::string_view::npos;
            }
        }
    }
    return objects;
}

[[nodiscard]] std::size_t ExpectedCellCount(int width, int height)
{
    if (width <= 0 || height <= 0) {
        return 0;
    }
    return static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
}

[[nodiscard]] std::string GridName(std::string_view file, std::string_view key)
{
    return std::string(file) + ":" + std::string(key);
}

[[nodiscard]] RuntimeGrid<std::string> ParseStringGrid(
    const std::string& text,
    int width,
    int height,
    std::string_view file,
    Diagnostics& diagnostics)
{
    RuntimeGrid<std::string> grid;
    grid.width = width;
    grid.height = height;

    const std::optional<std::string> rows = ExtractArrayAfterKey(text, "rows");
    if (!rows.has_value()) {
        diagnostics.AddWarning("runtime terrain grid missing rows source=" + std::string(file));
        return grid;
    }

    const std::vector<std::string> strings = ExtractQuotedStrings(*rows);
    const std::size_t expected = ExpectedCellCount(width, height);
    if (strings.size() == expected) {
        grid.cells = strings;
        return grid;
    }

    if (strings.size() == static_cast<std::size_t>(height)
        && std::all_of(strings.begin(), strings.end(), [width](const std::string& row) {
               return row.size() == static_cast<std::size_t>(width);
           })) {
        grid.cells.reserve(expected);
        for (const std::string& row : strings) {
            for (const char value : row) {
                grid.cells.emplace_back(1, value);
            }
        }
        return grid;
    }

    diagnostics.AddWarning(
        "runtime terrain grid shape mismatch source=" + std::string(file) + " values=" + std::to_string(strings.size())
        + " expected=" + std::to_string(expected));
    grid.cells.clear();
    return grid;
}

[[nodiscard]] RuntimeGrid<std::uint8_t> ParseBooleanRowsGrid(
    const std::string& text,
    int width,
    int height,
    std::string_view file,
    Diagnostics& diagnostics)
{
    RuntimeGrid<std::uint8_t> grid;
    grid.width = width;
    grid.height = height;

    const std::optional<std::string> rows = ExtractArrayAfterKey(text, "rows");
    if (!rows.has_value()) {
        diagnostics.AddWarning("runtime boolean grid missing rows source=" + std::string(file));
        return grid;
    }

    const std::vector<std::string> strings = ExtractQuotedStrings(*rows);
    const std::size_t expected = ExpectedCellCount(width, height);
    if (strings.size() == static_cast<std::size_t>(height)
        && std::all_of(strings.begin(), strings.end(), [width](const std::string& row) {
               return row.size() == static_cast<std::size_t>(width);
           })) {
        grid.cells.reserve(expected);
        for (const std::string& row : strings) {
            for (const char value : row) {
                grid.cells.push_back(value == '1' ? std::uint8_t{1} : std::uint8_t{0});
            }
        }
        return grid;
    }

    const std::vector<int> numbers = ExtractIntegers(*rows);
    if (numbers.size() == expected) {
        grid.cells.reserve(expected);
        for (int value : numbers) {
            grid.cells.push_back(value != 0 ? std::uint8_t{1} : std::uint8_t{0});
        }
        return grid;
    }

    diagnostics.AddWarning(
        "runtime boolean grid shape mismatch source=" + std::string(file) + " values="
        + std::to_string(std::max(strings.size(), numbers.size())) + " expected=" + std::to_string(expected));
    return grid;
}

[[nodiscard]] RuntimeGrid<int> ParseIntegerRowsGrid(
    const std::string& text,
    int width,
    int height,
    std::string_view file,
    std::string_view key,
    Diagnostics& diagnostics)
{
    RuntimeGrid<int> grid;
    grid.width = width;
    grid.height = height;

    std::string section = text;
    if (!key.empty()) {
        const std::optional<std::string> object = ExtractObjectAfterKey(text, key);
        if (!object.has_value()) {
            diagnostics.AddWarning("runtime integer grid object missing source=" + GridName(file, key));
            return grid;
        }
        section = *object;
    }

    const std::optional<std::string> rows = ExtractArrayAfterKey(section, "rows");
    if (!rows.has_value()) {
        diagnostics.AddWarning("runtime integer grid missing rows source=" + GridName(file, key));
        return grid;
    }

    std::vector<int> values = ExtractIntegers(*rows);
    const std::size_t expected = ExpectedCellCount(width, height);
    if (values.size() != expected) {
        diagnostics.AddWarning(
            "runtime integer grid shape mismatch source=" + GridName(file, key) + " values=" + std::to_string(values.size())
            + " expected=" + std::to_string(expected));
        return grid;
    }

    grid.cells = std::move(values);
    return grid;
}

[[nodiscard]] RuntimeGrid<std::string> ReadTerrainGrid(const MapPackageInfo& package, Diagnostics& diagnostics)
{
    constexpr std::string_view kTerrainFile = "layers/terrain.json";
    const std::filesystem::path terrain_path = package.path / kTerrainFile;
    if (Exists(terrain_path)) {
        const std::string text = ReadTextFileLimited(terrain_path, kMaxRuntimeGridReadBytes, diagnostics);
        if (!text.empty()) {
            return ParseStringGrid(text, package.width.value_or(0), package.height.value_or(0), kTerrainFile, diagnostics);
        }
    }

    constexpr std::string_view kTileFile = "layers/tile_grid.json";
    const std::filesystem::path tile_path = package.path / kTileFile;
    if (Exists(tile_path)) {
        const std::string text = ReadTextFileLimited(tile_path, kMaxRuntimeGridReadBytes, diagnostics);
        if (!text.empty()) {
            return ParseStringGrid(text, package.width.value_or(0), package.height.value_or(0), kTileFile, diagnostics);
        }
    }

    RuntimeGrid<std::string> grid;
    grid.width = package.width.value_or(0);
    grid.height = package.height.value_or(0);
    diagnostics.AddWarning("runtime terrain grid unavailable");
    return grid;
}

[[nodiscard]] RuntimeGrid<std::uint8_t> ReadCollisionGrid(const MapPackageInfo& package, Diagnostics& diagnostics)
{
    constexpr std::string_view kCollisionFile = "layers/collision.json";
    const std::filesystem::path collision_path = package.path / kCollisionFile;
    if (Exists(collision_path)) {
        const std::string text = ReadTextFileLimited(collision_path, kMaxRuntimeGridReadBytes, diagnostics);
        if (!text.empty()) {
            return ParseBooleanRowsGrid(text, package.width.value_or(0), package.height.value_or(0), kCollisionFile, diagnostics);
        }
    }

    constexpr std::string_view kRuntimeFile = "runtime_grids.json";
    const std::filesystem::path runtime_path = package.path / kRuntimeFile;
    if (Exists(runtime_path)) {
        const std::string text = ReadTextFileLimited(runtime_path, kMaxRuntimeGridReadBytes, diagnostics);
        const std::optional<std::string> object = ExtractObjectAfterKey(text, "collision_grid");
        if (object.has_value()) {
            return ParseBooleanRowsGrid(*object, package.width.value_or(0), package.height.value_or(0), kRuntimeFile, diagnostics);
        }
    }

    RuntimeGrid<std::uint8_t> grid;
    grid.width = package.width.value_or(0);
    grid.height = package.height.value_or(0);
    diagnostics.AddWarning("runtime collision grid unavailable");
    return grid;
}

[[nodiscard]] RuntimeGrid<int> ReadSparseElevationLayer(const MapPackageInfo& package, Diagnostics& diagnostics)
{
    RuntimeGrid<int> grid;
    grid.width = package.width.value_or(0);
    grid.height = package.height.value_or(0);

    constexpr std::string_view kElevationFile = "layers/elevation.json";
    const std::filesystem::path elevation_path = package.path / kElevationFile;
    if (!Exists(elevation_path)) {
        diagnostics.AddWarning("runtime height grid unavailable");
        return grid;
    }

    const std::string text = ReadTextFileLimited(elevation_path, kMaxRuntimeGridReadBytes, diagnostics);
    if (text.empty()) {
        return grid;
    }

    const std::optional<std::string> elevation = ExtractObjectAfterKey(text, "elevation");
    if (!elevation.has_value()) {
        diagnostics.AddWarning("runtime sparse elevation object missing source=" + std::string(kElevationFile));
        return grid;
    }

    const int default_level = ExtractIntByKeys(*elevation, {"default"}).value_or(0);
    grid.cells.assign(ExpectedCellCount(grid.width, grid.height), default_level);

    const std::optional<std::string> cells = ExtractArrayAfterKey(*elevation, "cells");
    if (!cells.has_value()) {
        return grid;
    }

    int skipped = 0;
    for (const std::string& object : ExtractTopLevelObjectsFromArray(*cells)) {
        const std::optional<int> x = ExtractIntByKeys(object, {"x"});
        const std::optional<int> y = ExtractIntByKeys(object, {"y"});
        const std::optional<int> level = ExtractIntByKeys(object, {"level"});
        if (!x.has_value() || !y.has_value() || !level.has_value()) {
            ++skipped;
            continue;
        }
        const TileCoord coord{*x, *y};
        if (!grid.Contains(coord)) {
            ++skipped;
            continue;
        }
        const std::size_t index = static_cast<std::size_t>(*y) * static_cast<std::size_t>(grid.width)
            + static_cast<std::size_t>(*x);
        grid.cells[index] = *level;
    }

    if (skipped > 0) {
        diagnostics.AddWarning("runtime sparse elevation skipped invalid cells count=" + std::to_string(skipped));
    }
    return grid;
}

[[nodiscard]] RuntimeGrid<std::uint8_t> ReadStructureHeightGrid(
    const MapPackageInfo& package,
    bool& source_present,
    Diagnostics& diagnostics)
{
    RuntimeGrid<std::uint8_t> grid;
    grid.width = package.width.value_or(0);
    grid.height = package.height.value_or(0);

    constexpr std::string_view kStructureHeightFile = "layers/structure_height.json";
    const std::filesystem::path path = package.path / kStructureHeightFile;
    source_present = Exists(path);
    if (!source_present) {
        grid.cells.assign(ExpectedCellCount(grid.width, grid.height), std::uint8_t{0});
        return grid;
    }

    const std::string text = ReadTextFileLimited(path, kMaxRuntimeGridReadBytes, diagnostics);
    if (text.empty()) {
        return grid;
    }

    RuntimeGrid<int> parsed = ParseIntegerRowsGrid(
        text,
        grid.width,
        grid.height,
        kStructureHeightFile,
        "",
        diagnostics);
    if (!parsed.IsValid()) {
        return grid;
    }

    constexpr int kMaximumStructureHeight =
        static_cast<int>(std::numeric_limits<std::uint8_t>::max());

    grid.cells.reserve(parsed.cells.size());
    for (int value : parsed.cells) {
        if (value < 0 || value > kMaximumStructureHeight) {
            diagnostics.AddWarning(
                "runtime structure height value outside supported range source="
                + std::string(kStructureHeightFile) + " value=" + std::to_string(value)
                + " supported=0.." + std::to_string(kMaximumStructureHeight));
            grid.cells.clear();
            return grid;
        }
        grid.cells.push_back(static_cast<std::uint8_t>(value));
    }
    return grid;
}

[[nodiscard]] RuntimeGrid<std::uint8_t> ReadByteRowsGrid(
    const MapPackageInfo& package,
    std::string_view relative_path,
    int maximum_value,
    bool& source_present,
    Diagnostics& diagnostics)
{
    RuntimeGrid<std::uint8_t> grid;
    grid.width = package.width.value_or(0);
    grid.height = package.height.value_or(0);

    const std::filesystem::path path = package.path / relative_path;
    source_present = Exists(path);
    if (!source_present) {
        grid.cells.assign(ExpectedCellCount(grid.width, grid.height), std::uint8_t{0});
        return grid;
    }

    const std::string text = ReadTextFileLimited(path, kMaxRuntimeGridReadBytes, diagnostics);
    if (text.empty()) {
        return grid;
    }

    RuntimeGrid<int> parsed = ParseIntegerRowsGrid(
        text,
        grid.width,
        grid.height,
        relative_path,
        "",
        diagnostics);
    if (!parsed.IsValid()) {
        return grid;
    }

    grid.cells.reserve(parsed.cells.size());
    for (int value : parsed.cells) {
        if (value < 0 || value > maximum_value) {
            diagnostics.AddWarning(
                "runtime byte grid value outside supported range source="
                + std::string(relative_path) + " value=" + std::to_string(value));
            grid.cells.clear();
            return grid;
        }
        grid.cells.push_back(static_cast<std::uint8_t>(value));
    }
    return grid;
}

[[nodiscard]] RuntimeGrid<std::uint8_t> ReadStructureTypeGrid(
    const MapPackageInfo& package,
    bool& source_present,
    Diagnostics& diagnostics)
{
    constexpr std::string_view kStructureTypeFile = "layers/structure_type.json";
    RuntimeGrid<std::uint8_t> grid = ReadByteRowsGrid(
        package,
        kStructureTypeFile,
        static_cast<int>(std::numeric_limits<std::uint8_t>::max()),
        source_present,
        diagnostics);
    if (!source_present || !grid.IsValid()) {
        return grid;
    }

    for (const std::uint8_t value : grid.cells) {
        if (!IsKnownStructureType(static_cast<RuntimeStructureType>(value))) {
            diagnostics.AddWarning(
                "runtime structure type value is unsupported source="
                + std::string(kStructureTypeFile) + " value="
                + std::to_string(static_cast<int>(value)));
            grid.cells.clear();
            return grid;
        }
    }
    return grid;
}

[[nodiscard]] RuntimeGrid<std::uint16_t> ReadStructureMicroGeometryGrid(
    const MapPackageInfo& package,
    bool& source_present,
    int& division,
    Diagnostics& diagnostics)
{
    RuntimeGrid<std::uint16_t> grid;
    grid.width = package.width.value_or(0);
    grid.height = package.height.value_or(0);
    division = 0;

    constexpr std::string_view kMicroGeometryFile =
        "layers/structure_micro_geometry.json";
    const std::filesystem::path path = package.path / kMicroGeometryFile;
    source_present = Exists(path);
    if (!source_present) {
        grid.cells.assign(ExpectedCellCount(grid.width, grid.height), std::uint16_t{0});
        return grid;
    }

    const std::string text = ReadTextFileLimited(
        path,
        kMaxRuntimeGridReadBytes,
        diagnostics);
    if (text.empty()) {
        return grid;
    }

    const auto fail = [&diagnostics, &grid](std::string reason) {
        diagnostics.AddWarning(
            "runtime structure micro geometry is invalid source="
            "layers/structure_micro_geometry.json reason=" + std::move(reason));
        grid.cells.clear();
    };

    const std::optional<std::string> schema = ExtractStringByKeys(text, {"schema_version"});
    const std::optional<std::string> kind = ExtractStringByKeys(text, {"kind"});
    const std::optional<std::string> format = ExtractStringByKeys(text, {"format"});
    const std::optional<std::string> bit_order = ExtractStringByKeys(text, {"bit_order"});
    const std::optional<std::string> occupancy_rule = ExtractStringByKeys(text, {"occupancy_rule"});
    const std::optional<std::string> floor_rule = ExtractStringByKeys(text, {"floor_rule"});
    const std::optional<int> width = ExtractIntByKeys(text, {"width"});
    const std::optional<int> height = ExtractIntByKeys(text, {"height"});
    const std::optional<int> parsed_division = ExtractIntByKeys(text, {"division"});
    const std::optional<int> subtile_size_px = ExtractIntByKeys(text, {"subtile_size_px"});
    const std::optional<int> default_mask = ExtractIntByKeys(text, {"default_mask"});
    const std::optional<int> full_mask = ExtractIntByKeys(text, {"full_mask"});

    constexpr int kRequiredDivision = 4;
    constexpr int kFullMask = static_cast<int>(std::numeric_limits<std::uint16_t>::max());
    const int tile_size_px = package.tile_size.value_or(0);
    const int required_subtile_size_px = tile_size_px / kRequiredDivision;
    const bool schema_supported = schema == "structure-micro-geometry-layer-v3"
        || schema == "structure-micro-geometry-layer-v4"
        || schema == "structure-micro-geometry-layer-v5"
        || schema == "structure-micro-geometry-layer-v6"
        || schema == "structure-micro-geometry-layer-v7"
        || schema == "structure-micro-geometry-layer-v8";
    if (!schema_supported
        || kind != "structure_micro_geometry"
        || format != "sparse_uint16_masks"
        || bit_order != "row_major_top_left_lsb"
        || occupancy_rule != "bit_set_means_vertical_structure_solid"
        || floor_rule != "floor_structure_types_use_zero_mask_and_render_as_tile_surfaces"
        || width != grid.width || height != grid.height
        || parsed_division != kRequiredDivision
        || tile_size_px <= 0 || tile_size_px % kRequiredDivision != 0
        || subtile_size_px != required_subtile_size_px
        || default_mask != 0 || full_mask != kFullMask) {
        fail("metadata_mismatch");
        return grid;
    }

    const std::optional<std::string> cells = ExtractArrayAfterKey(text, "cells");
    if (!cells.has_value()) {
        fail("cells_missing");
        return grid;
    }

    grid.cells.assign(ExpectedCellCount(grid.width, grid.height), std::uint16_t{0});
    std::vector<std::uint8_t> seen(grid.cells.size(), std::uint8_t{0});
    for (const std::string& object : ExtractTopLevelObjectsFromArray(*cells)) {
        const std::optional<int> x = ExtractIntByKeys(object, {"x"});
        const std::optional<int> y = ExtractIntByKeys(object, {"y"});
        const std::optional<int> mask = ExtractIntByKeys(object, {"mask"});
        if (!x.has_value() || !y.has_value() || !mask.has_value()
            || *mask <= 0 || *mask > kFullMask) {
            fail("bad_cell");
            return grid;
        }
        const TileCoord tile{*x, *y};
        if (!grid.Contains(tile)) {
            fail("cell_out_of_bounds");
            return grid;
        }
        const std::size_t index = static_cast<std::size_t>(tile.y)
            * static_cast<std::size_t>(grid.width)
            + static_cast<std::size_t>(tile.x);
        if (seen[index] != 0U) {
            fail("duplicate_cell");
            return grid;
        }
        seen[index] = 1U;
        grid.cells[index] = static_cast<std::uint16_t>(*mask);
    }

    division = kRequiredDivision;
    return grid;
}

struct StructureTopGeometryGrids {
    RuntimeGrid<std::uint16_t> walkway;
    RuntimeGrid<std::uint16_t> parapet;
    RuntimeGrid<std::uint16_t> crenellation;
    bool source_present = false;
    int division = 0;
};

[[nodiscard]] StructureTopGeometryGrids ReadStructureTopGeometryGrids(
    const MapPackageInfo& package,
    Diagnostics& diagnostics)
{
    StructureTopGeometryGrids result;
    result.walkway.width = package.width.value_or(0);
    result.walkway.height = package.height.value_or(0);
    result.parapet.width = result.walkway.width;
    result.parapet.height = result.walkway.height;
    result.crenellation.width = result.walkway.width;
    result.crenellation.height = result.walkway.height;

    constexpr std::string_view kTopGeometryFile =
        "layers/structure_top_geometry.json";
    const std::filesystem::path path = package.path / kTopGeometryFile;
    result.source_present = Exists(path);
    const std::size_t cell_count = ExpectedCellCount(
        result.walkway.width,
        result.walkway.height);
    if (!result.source_present) {
        result.walkway.cells.assign(cell_count, std::uint16_t{0});
        result.parapet.cells.assign(cell_count, std::uint16_t{0});
        result.crenellation.cells.assign(cell_count, std::uint16_t{0});
        return result;
    }

    const std::string text = ReadTextFileLimited(
        path,
        kMaxRuntimeGridReadBytes,
        diagnostics);
    if (text.empty()) {
        return result;
    }

    const auto fail = [&diagnostics, &result](std::string reason) {
        diagnostics.AddWarning(
            "runtime structure top geometry is invalid source="
            "layers/structure_top_geometry.json reason=" + std::move(reason));
        result.walkway.cells.clear();
        result.parapet.cells.clear();
        result.crenellation.cells.clear();
        result.division = 0;
    };

    const std::optional<std::string> schema = ExtractStringByKeys(text, {"schema_version"});
    const std::optional<std::string> kind = ExtractStringByKeys(text, {"kind"});
    const std::optional<std::string> format = ExtractStringByKeys(text, {"format"});
    const std::optional<std::string> bit_order = ExtractStringByKeys(text, {"bit_order"});
    const std::optional<std::string> height_reference = ExtractStringByKeys(text, {"height_reference"});
    const std::optional<std::string> walkway_rule = ExtractStringByKeys(text, {"walkway_rule"});
    const std::optional<std::string> parapet_rule = ExtractStringByKeys(text, {"parapet_rule"});
    const std::optional<std::string> crenellation_rule = ExtractStringByKeys(text, {"crenellation_rule"});
    const std::optional<std::string> overlay_rule = ExtractStringByKeys(text, {"overlay_rule"});
    const std::optional<int> width = ExtractIntByKeys(text, {"width"});
    const std::optional<int> height = ExtractIntByKeys(text, {"height"});
    const std::optional<int> parsed_division = ExtractIntByKeys(text, {"division"});
    const std::optional<int> subtile_size_px = ExtractIntByKeys(text, {"subtile_size_px"});

    constexpr int kRequiredDivision = 4;
    constexpr int kFullMask = static_cast<int>(std::numeric_limits<std::uint16_t>::max());
    const int tile_size_px = package.tile_size.value_or(0);
    const int required_subtile_size_px = tile_size_px / kRequiredDivision;
    if (schema != "structure-top-geometry-layer-v3"
        || kind != "structure_top_geometry"
        || format != "sparse_triple_uint16_masks"
        || bit_order != "row_major_top_left_lsb"
        || height_reference != "top_of_structure_height"
        || walkway_rule != "walkway_mask_is_flat_roof_or_combat_walkway"
        || parapet_rule != "parapet_mask_is_inner_raised_edge"
        || crenellation_rule
            != "crenellation_mask_is_ordered_outer_ring_alternating_merlon_and_gap"
        || overlay_rule != "raised_masks_overlay_walkway_mask"
        || width != result.walkway.width || height != result.walkway.height
        || parsed_division != kRequiredDivision
        || tile_size_px <= 0 || tile_size_px % kRequiredDivision != 0
        || subtile_size_px != required_subtile_size_px) {
        fail("metadata_mismatch");
        return result;
    }

    const std::optional<std::string> cells = ExtractArrayAfterKey(text, "cells");
    if (!cells.has_value()) {
        fail("cells_missing");
        return result;
    }

    result.walkway.cells.assign(cell_count, std::uint16_t{0});
    result.parapet.cells.assign(cell_count, std::uint16_t{0});
    result.crenellation.cells.assign(cell_count, std::uint16_t{0});
    std::vector<std::uint8_t> seen(cell_count, std::uint8_t{0});
    for (const std::string& object : ExtractTopLevelObjectsFromArray(*cells)) {
        const std::optional<int> x = ExtractIntByKeys(object, {"x"});
        const std::optional<int> y = ExtractIntByKeys(object, {"y"});
        const std::optional<int> walkway_mask = ExtractIntByKeys(object, {"walkway_mask"});
        const std::optional<int> parapet_mask = ExtractIntByKeys(object, {"parapet_mask"});
        const std::optional<int> crenellation_mask = ExtractIntByKeys(object, {"crenellation_mask"});
        if (!x.has_value() || !y.has_value() || !walkway_mask.has_value()
            || !parapet_mask.has_value() || !crenellation_mask.has_value()
            || *walkway_mask < 0 || *walkway_mask > kFullMask
            || *parapet_mask < 0 || *parapet_mask > kFullMask
            || *crenellation_mask < 0 || *crenellation_mask > kFullMask
            || (*walkway_mask == 0 && *parapet_mask == 0
                && *crenellation_mask == 0)) {
            fail("bad_cell");
            return result;
        }
        const TileCoord tile{*x, *y};
        if (!result.walkway.Contains(tile)) {
            fail("cell_out_of_bounds");
            return result;
        }
        const std::size_t index = static_cast<std::size_t>(tile.y)
            * static_cast<std::size_t>(result.walkway.width)
            + static_cast<std::size_t>(tile.x);
        if (seen[index] != 0U) {
            fail("duplicate_cell");
            return result;
        }
        seen[index] = 1U;
        result.walkway.cells[index] = static_cast<std::uint16_t>(*walkway_mask);
        result.parapet.cells[index] = static_cast<std::uint16_t>(*parapet_mask);
        result.crenellation.cells[index] = static_cast<std::uint16_t>(*crenellation_mask);
    }

    result.division = kRequiredDivision;
    return result;
}

[[nodiscard]] bool ValidateStructureGeometryGrids(
    const RuntimeGrid<std::uint8_t>& type_grid,
    const RuntimeGrid<std::uint8_t>& height_grid,
    const RuntimeGrid<std::uint16_t>& micro_mask_grid,
    const RuntimeGrid<std::uint16_t>& walkway_grid,
    const RuntimeGrid<std::uint16_t>& parapet_grid,
    const RuntimeGrid<std::uint16_t>& crenellation_grid,
    bool top_geometry_present,
    Diagnostics& diagnostics)
{
    if (!type_grid.IsValid() || !height_grid.IsValid() || !micro_mask_grid.IsValid()
        || type_grid.width != height_grid.width || type_grid.height != height_grid.height
        || type_grid.width != micro_mask_grid.width
        || type_grid.height != micro_mask_grid.height) {
        diagnostics.AddWarning(
            "runtime structure geometry grids are unavailable or dimensionally inconsistent");
        return false;
    }
    if (top_geometry_present
        && (!walkway_grid.IsValid() || !parapet_grid.IsValid()
            || !crenellation_grid.IsValid()
            || walkway_grid.width != type_grid.width
            || walkway_grid.height != type_grid.height
            || parapet_grid.width != type_grid.width
            || parapet_grid.height != type_grid.height
            || crenellation_grid.width != type_grid.width
            || crenellation_grid.height != type_grid.height)) {
        diagnostics.AddWarning(
            "runtime structure top geometry grids are dimensionally inconsistent");
        return false;
    }

    for (std::size_t index = 0; index < type_grid.cells.size(); ++index) {
        const auto type = static_cast<RuntimeStructureType>(type_grid.cells[index]);
        const std::uint8_t height = height_grid.cells[index];
        const std::uint16_t micro_mask = micro_mask_grid.cells[index];
        const std::uint16_t walkway_mask = top_geometry_present
            ? walkway_grid.cells[index]
            : std::uint16_t{0};
        const std::uint16_t parapet_mask = top_geometry_present
            ? parapet_grid.cells[index]
            : std::uint16_t{0};
        const std::uint16_t crenellation_mask = top_geometry_present
            ? crenellation_grid.cells[index]
            : std::uint16_t{0};
        const bool raised_masks_inside_walkway =
            (parapet_mask & static_cast<std::uint16_t>(~walkway_mask)) == 0U
            && (crenellation_mask & static_cast<std::uint16_t>(~walkway_mask)) == 0U;
        const bool no_top_geometry = walkway_mask == 0U
            && parapet_mask == 0U && crenellation_mask == 0U;
        const bool valid = IsKnownStructureType(type)
            && raised_masks_inside_walkway
            && ((type == RuntimeStructureType::kNone && height == 0U
                    && micro_mask == 0U && no_top_geometry)
                || (IsVerticalStructureType(type) && height > 0U)
                || (IsFloorStructureType(type) && height == 0U
                    && micro_mask == 0U && parapet_mask == 0U
                    && crenellation_mask == 0U));
        if (!valid) {
            diagnostics.AddWarning(
                "runtime structure type/height/mask mismatch index="
                + std::to_string(index) + " type="
                + std::to_string(static_cast<int>(type_grid.cells[index]))
                + " height=" + std::to_string(static_cast<int>(height))
                + " micro=" + std::to_string(micro_mask)
                + " walkway=" + std::to_string(walkway_mask)
                + " parapet=" + std::to_string(parapet_mask)
                + " crenellation=" + std::to_string(crenellation_mask));
            return false;
        }
    }
    return true;
}

[[nodiscard]] RuntimeGrid<std::uint8_t> ReadVegetationTypeGrid(
    const MapPackageInfo& package,
    bool& source_present,
    Diagnostics& diagnostics)
{
    return ReadByteRowsGrid(
        package,
        "layers/vegetation_type.json",
        4,
        source_present,
        diagnostics);
}

[[nodiscard]] RuntimeGrid<std::uint8_t> ReadVegetationHeightGrid(
    const MapPackageInfo& package,
    bool& source_present,
    Diagnostics& diagnostics)
{
    return ReadByteRowsGrid(
        package,
        "layers/vegetation_height.json",
        5,
        source_present,
        diagnostics);
}

[[nodiscard]] bool IsValidVegetationPair(std::uint8_t type, std::uint8_t height)
{
    return (type == 0U && height == 0U)
        || (type == 1U && height >= 2U && height <= 5U)
        || (type == 2U && height >= 1U && height <= 2U)
        || ((type == 3U || type == 4U) && height == 1U);
}

[[nodiscard]] bool ValidateVegetationGrids(
    const RuntimeGrid<std::uint8_t>& type_grid,
    const RuntimeGrid<std::uint8_t>& height_grid,
    Diagnostics& diagnostics)
{
    if (!type_grid.IsValid() || !height_grid.IsValid()
        || type_grid.width != height_grid.width
        || type_grid.height != height_grid.height) {
        diagnostics.AddWarning("runtime vegetation grids are unavailable or dimensionally inconsistent");
        return false;
    }

    for (std::size_t index = 0; index < type_grid.cells.size(); ++index) {
        if (!IsValidVegetationPair(type_grid.cells[index], height_grid.cells[index])) {
            diagnostics.AddWarning(
                "runtime vegetation type/height mismatch index=" + std::to_string(index)
                + " type=" + std::to_string(static_cast<int>(type_grid.cells[index]))
                + " height=" + std::to_string(static_cast<int>(height_grid.cells[index])));
            return false;
        }
    }
    return true;
}

[[nodiscard]] RuntimeGrid<int> ReadHeightGrid(const MapPackageInfo& package, Diagnostics& diagnostics)
{
    constexpr std::string_view kRuntimeFile = "runtime_grids.json";
    const std::filesystem::path runtime_path = package.path / kRuntimeFile;
    if (Exists(runtime_path)) {
        const std::string text = ReadTextFileLimited(runtime_path, kMaxRuntimeGridReadBytes, diagnostics);
        if (!text.empty()) {
            RuntimeGrid<int> grid = ParseIntegerRowsGrid(
                text,
                package.width.value_or(0),
                package.height.value_or(0),
                kRuntimeFile,
                "height_grid",
                diagnostics);
            if (grid.IsValid()) {
                return grid;
            }
        }
    }

    return ReadSparseElevationLayer(package, diagnostics);
}

[[nodiscard]] std::optional<TileCoord> ReadPointFromStartGoal(
    const std::string& text,
    std::string_view key,
    int width,
    int height,
    Diagnostics& diagnostics)
{
    const std::optional<std::string> object = ExtractObjectAfterKey(text, key);
    if (!object.has_value()) {
        return std::nullopt;
    }

    const std::optional<int> x = ExtractIntByKeys(*object, {"x"});
    const std::optional<int> y = ExtractIntByKeys(*object, {"y"});
    if (!x.has_value() || !y.has_value()) {
        diagnostics.AddWarning("runtime point missing coordinate key=" + std::string(key));
        return std::nullopt;
    }

    const TileCoord coord{*x, *y};
    if (coord.x < 0 || coord.y < 0 || coord.x >= width || coord.y >= height) {
        diagnostics.AddWarning(
            "runtime point outside map key=" + std::string(key) + " x=" + std::to_string(coord.x)
            + " y=" + std::to_string(coord.y));
        return std::nullopt;
    }
    return coord;
}

void ReadStartGoal(RuntimeMap& runtime, const MapPackageInfo& package)
{
    constexpr std::string_view kStartGoalFile = "layers/start_goal.json";
    const std::filesystem::path start_goal_path = package.path / kStartGoalFile;
    if (!Exists(start_goal_path)) {
        runtime.diagnostics.AddWarning("runtime start/goal layer unavailable");
        return;
    }

    const std::string text = ReadTextFileLimited(start_goal_path, kMaxRuntimeGridReadBytes, runtime.diagnostics);
    if (text.empty()) {
        return;
    }

    runtime.info.start = ReadPointFromStartGoal(
        text,
        "start",
        runtime.info.width,
        runtime.info.height,
        runtime.diagnostics);
    runtime.info.goal = ReadPointFromStartGoal(
        text,
        "goal",
        runtime.info.width,
        runtime.info.height,
        runtime.diagnostics);
    runtime.info.start_goal_loaded = runtime.info.start.has_value() && runtime.info.goal.has_value();
}

[[nodiscard]] RuntimeObjectMarkerKind ClassifyObjectMarker(std::string type, std::string role)
{
    const std::string key = ToLowerAscii(type + " " + role);
    if (ContainsToken(key, "reed") || ContainsToken(key, "rush") || ContainsToken(key, "cane")) {
        return RuntimeObjectMarkerKind::kReed;
    }
    if (ContainsToken(key, "bush") || ContainsToken(key, "shrub")) {
        return RuntimeObjectMarkerKind::kBush;
    }
    if (ContainsToken(key, "tree") || ContainsToken(key, "forest")) {
        return RuntimeObjectMarkerKind::kTree;
    }
    if (ContainsToken(key, "ruin") || ContainsToken(key, "wall") || ContainsToken(key, "stone")) {
        return RuntimeObjectMarkerKind::kRuin;
    }
    if (ContainsToken(key, "trench")) {
        return RuntimeObjectMarkerKind::kTrench;
    }
    if (ContainsToken(key, "loot") || ContainsToken(key, "cache") || ContainsToken(key, "backpack")) {
        return RuntimeObjectMarkerKind::kLoot;
    }
    if (ContainsToken(key, "cover") || ContainsToken(key, "barricade")) {
        return RuntimeObjectMarkerKind::kCover;
    }
    if (ContainsToken(key, "building") || ContainsToken(key, "bunker") || ContainsToken(key, "mast")
        || ContainsToken(key, "beacon") || ContainsToken(key, "generator")) {
        return RuntimeObjectMarkerKind::kStructure;
    }
    return RuntimeObjectMarkerKind::kUnknown;
}

[[nodiscard]] RuntimeObjectMarkerKind VegetationMarkerKind(RuntimeVegetationType type)
{
    switch (type) {
        case RuntimeVegetationType::kTree:
            return RuntimeObjectMarkerKind::kTree;
        case RuntimeVegetationType::kBush:
            return RuntimeObjectMarkerKind::kBush;
        case RuntimeVegetationType::kShoreReed:
        case RuntimeVegetationType::kPuddleReed:
            return RuntimeObjectMarkerKind::kReed;
        case RuntimeVegetationType::kNone:
            break;
    }
    return RuntimeObjectMarkerKind::kUnknown;
}

[[nodiscard]] bool RuntimeContainsTile(
    const RuntimeMap& runtime,
    TileCoord tile)
{
    return runtime.info.IsValid()
        && tile.x >= 0 && tile.y >= 0
        && tile.x < runtime.info.width && tile.y < runtime.info.height;
}

void AddRuntimeObjectMarker(RuntimeMap& runtime, RuntimeObjectMarker marker)
{
    if (!runtime.info.IsValid() || !runtime.height.Contains(marker.tile)) {
        return;
    }
    marker.height = std::max(0, marker.height);
    runtime.object_markers.push_back(std::move(marker));
}

[[nodiscard]] std::optional<TileCoord> ParseTileCoordObject(
    const std::string& text,
    std::string_view key)
{
    const std::optional<std::string> object = ExtractObjectAfterKey(text, key);
    if (!object.has_value()) {
        return std::nullopt;
    }
    const std::optional<int> x = ExtractIntByKeys(*object, {"x"});
    const std::optional<int> y = ExtractIntByKeys(*object, {"y"});
    if (!x.has_value() || !y.has_value()) {
        return std::nullopt;
    }
    return TileCoord{*x, *y};
}

[[nodiscard]] std::vector<TileCoord> ParseTileCoordArray(
    const std::string& text,
    std::string_view key)
{
    const std::optional<std::string> array = ExtractArrayAfterKey(text, key);
    if (!array.has_value()) {
        return {};
    }

    const std::vector<int> values = ExtractIntegers(*array);
    if (values.size() % 2U != 0U) {
        return {};
    }

    std::vector<TileCoord> tiles;
    tiles.reserve(values.size() / 2U);
    for (std::size_t index = 0; index < values.size(); index += 2U) {
        tiles.push_back(TileCoord{values[index], values[index + 1U]});
    }
    return tiles;
}

[[nodiscard]] RuntimeTileBounds ParseInclusiveBounds(
    const std::string& text,
    std::string_view key)
{
    RuntimeTileBounds bounds;
    const std::optional<std::string> object = ExtractObjectAfterKey(text, key);
    if (!object.has_value()) {
        return bounds;
    }
    bounds.min_x = ExtractIntByKeys(*object, {"min_x"}).value_or(0);
    bounds.min_y = ExtractIntByKeys(*object, {"min_y"}).value_or(0);
    bounds.max_x = ExtractIntByKeys(*object, {"max_x"}).value_or(-1);
    bounds.max_y = ExtractIntByKeys(*object, {"max_y"}).value_or(-1);
    return bounds;
}

[[nodiscard]] RuntimeTileBounds ParseVisualBounds(const std::string& text)
{
    RuntimeTileBounds bounds;
    const std::optional<std::string> object = ExtractObjectAfterKey(text, "visual_bounds");
    if (!object.has_value()) {
        return bounds;
    }

    const std::optional<int> x = ExtractIntByKeys(*object, {"x"});
    const std::optional<int> y = ExtractIntByKeys(*object, {"y"});
    const std::optional<int> width = ExtractIntByKeys(*object, {"width"});
    const std::optional<int> height = ExtractIntByKeys(*object, {"height"});
    if (!x.has_value() || !y.has_value() || !width.has_value()
        || !height.has_value() || *width <= 0 || *height <= 0) {
        return bounds;
    }

    bounds.min_x = *x;
    bounds.min_y = *y;
    bounds.max_x = *x + *width - 1;
    bounds.max_y = *y + *height - 1;
    return bounds;
}

void ReadRuntimeObjects(RuntimeMap& runtime, const MapPackageInfo& package)
{
    constexpr std::string_view kObjectsFile = "objects/runtime_objects.json";
    const std::filesystem::path objects_path = package.path / kObjectsFile;
    if (!Exists(objects_path)) {
        return;
    }

    const std::string text = ReadTextFileLimited(
        objects_path,
        kMaxRuntimeGridReadBytes,
        runtime.diagnostics);
    if (text.empty()) {
        return;
    }

    const std::optional<std::string> items = ExtractArrayAfterKey(text, "items");
    if (!items.has_value()) {
        runtime.diagnostics.AddWarning(
            "runtime objects missing items source=" + std::string(kObjectsFile));
        return;
    }

    int skipped = 0;
    for (const std::string& object_text : ExtractTopLevelObjectsFromArray(*items)) {
        const std::optional<int> x = ExtractIntByKeys(object_text, {"x"});
        const std::optional<int> y = ExtractIntByKeys(object_text, {"y"});
        if (!x.has_value() || !y.has_value()) {
            ++skipped;
            continue;
        }

        RuntimeMapObject object;
        object.id = ExtractStringByKeys(object_text, {"id"}).value_or("object");
        object.type = ExtractStringByKeys(object_text, {"type"}).value_or("object");
        object.role = ExtractStringByKeys(object_text, {"role"}).value_or("");
        object.anchor = TileCoord{*x, *y};
        if (!RuntimeContainsTile(runtime, object.anchor)) {
            ++skipped;
            continue;
        }
        object.kind = ClassifyObjectMarker(object.type, object.role);
        object.orientation = ExtractStringByKeys(object_text, {"orientation"}).value_or("");
        object.footprint = ParseTileCoordArray(object_text, "footprint");
        object.collision_footprint = ParseTileCoordArray(
            object_text,
            "collision_footprint");
        object.visual_bounds = ParseVisualBounds(object_text);
        object.elevation = ExtractIntByKeys(object_text, {"elevation"}).value_or(0);
        object.height = std::max(0, ExtractIntByKeys(object_text, {"height"}).value_or(0));
        object.blocks_movement = ExtractBoolByKeys(
            object_text,
            {"blocks_movement"}).value_or(false);
        object.blocks_projectiles = ExtractBoolByKeys(
            object_text,
            {"blocks_projectiles"}).value_or(false);
        object.blocks_vision = ExtractBoolByKeys(
            object_text,
            {"blocks_vision"}).value_or(false);
        object.interactive = ExtractBoolByKeys(
            object_text,
            {"interactive"}).value_or(false);
        RuntimeObjectMarker marker;
        marker.tile = object.anchor;
        marker.kind = object.kind;
        marker.type = object.type;
        marker.role = object.role;
        marker.height = object.height;
        marker.blocks_movement = object.blocks_movement;
        marker.visual_only = false;
        AddRuntimeObjectMarker(runtime, std::move(marker));
        runtime.runtime_objects.push_back(std::move(object));
    }

    runtime.info.runtime_objects = static_cast<int>(runtime.runtime_objects.size());
    runtime.info.runtime_objects_loaded = !runtime.runtime_objects.empty();
    if (skipped > 0) {
        runtime.diagnostics.AddWarning(
            "runtime objects skipped invalid items count=" + std::to_string(skipped));
    }
}

void ReadPlaces(RuntimeMap& runtime, const MapPackageInfo& package)
{
    constexpr std::string_view kPlacesFile = "objects/places.json";
    const std::filesystem::path places_path = package.path / kPlacesFile;
    if (!Exists(places_path)) {
        return;
    }

    const std::string text = ReadTextFileLimited(
        places_path,
        kMaxRuntimeGridReadBytes,
        runtime.diagnostics);
    if (text.empty()) {
        return;
    }

    const std::optional<std::string> items = ExtractArrayAfterKey(text, "items");
    if (!items.has_value()) {
        runtime.diagnostics.AddWarning(
            "runtime places missing items source=" + std::string(kPlacesFile));
        return;
    }

    int skipped = 0;
    for (const std::string& place_text : ExtractTopLevelObjectsFromArray(*items)) {
        const std::optional<TileCoord> center = ParseTileCoordObject(
            place_text,
            "center");
        if (!center.has_value() || !RuntimeContainsTile(runtime, *center)) {
            ++skipped;
            continue;
        }

        RuntimePlace place;
        place.id = ExtractStringByKeys(place_text, {"id"}).value_or("place");
        place.type = ExtractStringByKeys(place_text, {"type"}).value_or("place");
        place.role = ExtractStringByKeys(place_text, {"role"}).value_or("");
        place.center = *center;
        place.radius = std::max(0, ExtractIntByKeys(place_text, {"radius"}).value_or(0));
        place.bounds = ParseInclusiveBounds(place_text, "bounds");

        const std::optional<std::string> entrances = ExtractArrayAfterKey(
            place_text,
            "entrances");
        if (entrances.has_value()) {
            for (const std::string& entrance_text
                 : ExtractTopLevelObjectsFromArray(*entrances)) {
                const std::optional<TileCoord> position = ParseTileCoordObject(
                    entrance_text,
                    "position");
                if (!position.has_value() || !RuntimeContainsTile(runtime, *position)) {
                    continue;
                }
                RuntimePlaceEntrance entrance;
                entrance.id = ExtractStringByKeys(
                    entrance_text,
                    {"id"}).value_or("entrance");
                entrance.side = ExtractStringByKeys(
                    entrance_text,
                    {"side"}).value_or("");
                entrance.tile = *position;
                place.entrances.push_back(std::move(entrance));
            }
        }
        runtime.places.push_back(std::move(place));
    }

    runtime.info.places = static_cast<int>(runtime.places.size());
    runtime.info.places_loaded = !runtime.places.empty();
    if (skipped > 0) {
        runtime.diagnostics.AddWarning(
            "runtime places skipped invalid items count=" + std::to_string(skipped));
    }
}

void ReadMarkers(RuntimeMap& runtime, const MapPackageInfo& package)
{
    constexpr std::string_view kMarkersFile = "markers.json";
    const std::filesystem::path markers_path = package.path / kMarkersFile;
    if (!Exists(markers_path)) {
        return;
    }

    const std::string text = ReadTextFileLimited(
        markers_path,
        kMaxRuntimeGridReadBytes,
        runtime.diagnostics);
    if (text.empty()) {
        return;
    }

    const std::optional<std::string> items = ExtractArrayAfterKey(text, "items");
    if (!items.has_value()) {
        runtime.diagnostics.AddWarning(
            "runtime markers missing items source=" + std::string(kMarkersFile));
        return;
    }

    int skipped = 0;
    for (const std::string& marker_text : ExtractTopLevelObjectsFromArray(*items)) {
        const std::optional<TileCoord> position = ParseTileCoordObject(
            marker_text,
            "position");
        if (!position.has_value() || !RuntimeContainsTile(runtime, *position)) {
            ++skipped;
            continue;
        }

        RuntimeMapMarker marker;
        marker.id = ExtractStringByKeys(marker_text, {"id"}).value_or("marker");
        marker.type = ExtractStringByKeys(marker_text, {"type"}).value_or("marker");
        marker.source = ExtractStringByKeys(marker_text, {"source"}).value_or("");
        marker.tile = *position;
        runtime.markers.push_back(std::move(marker));
    }

    runtime.info.markers = static_cast<int>(runtime.markers.size());
    runtime.info.markers_loaded = !runtime.markers.empty();
    if (skipped > 0) {
        runtime.diagnostics.AddWarning(
            "runtime markers skipped invalid items count=" + std::to_string(skipped));
    }
}

[[nodiscard]] bool ReadVegetationGridMarkers(RuntimeMap& runtime)
{
    if (!runtime.info.vegetation_type_loaded || !runtime.info.vegetation_height_loaded
        || !runtime.vegetation_type.IsValid() || !runtime.vegetation_height.IsValid()) {
        return false;
    }

    for (int y = 0; y < runtime.info.height; ++y) {
        for (int x = 0; x < runtime.info.width; ++x) {
            const std::size_t index = static_cast<std::size_t>(y) * static_cast<std::size_t>(runtime.info.width)
                + static_cast<std::size_t>(x);
            const auto type = static_cast<RuntimeVegetationType>(runtime.vegetation_type.cells[index]);
            if (type == RuntimeVegetationType::kNone) {
                continue;
            }

            RuntimeObjectMarker marker;
            marker.tile = TileCoord{x, y};
            marker.kind = VegetationMarkerKind(type);
            marker.type = std::string(ToString(type));
            marker.role = "vegetation";
            marker.height = static_cast<int>(runtime.vegetation_height.cells[index]);
            marker.visual_only = true;
            marker.blocks_movement = false;
            AddRuntimeObjectMarker(runtime, std::move(marker));
        }
    }
    return true;
}

void ReadLegacyVegetationVisualMarkers(RuntimeMap& runtime, const MapPackageInfo& package)
{
    constexpr std::string_view kVegetationFile = "render/vegetation_visual.json";
    const std::filesystem::path vegetation_path = package.path / kVegetationFile;
    if (!Exists(vegetation_path)) {
        return;
    }

    const std::string text = ReadTextFileLimited(vegetation_path, kMaxRuntimeGridReadBytes, runtime.diagnostics);
    if (text.empty()) {
        return;
    }

    const std::optional<std::string> rows = ExtractArrayAfterKey(text, "rows");
    if (!rows.has_value()) {
        runtime.diagnostics.AddWarning("runtime vegetation visual markers missing rows source=" + std::string(kVegetationFile));
        return;
    }

    const std::vector<std::string> row_values = ExtractQuotedStrings(*rows);
    if (static_cast<int>(row_values.size()) != runtime.info.height) {
        runtime.diagnostics.AddWarning(
            "runtime vegetation visual row count mismatch rows=" + std::to_string(row_values.size())
            + " expected=" + std::to_string(runtime.info.height));
        return;
    }

    int skipped = 0;
    for (int y = 0; y < runtime.info.height; ++y) {
        const std::string& row = row_values[static_cast<std::size_t>(y)];
        if (static_cast<int>(row.size()) < runtime.info.width) {
            ++skipped;
            continue;
        }
        for (int x = 0; x < runtime.info.width; ++x) {
            RuntimeObjectMarker marker;
            marker.tile = TileCoord{x, y};
            marker.visual_only = true;
            marker.blocks_movement = false;
            switch (row[static_cast<std::size_t>(x)]) {
                case 'T':
                    marker.kind = RuntimeObjectMarkerKind::kTree;
                    marker.type = "visible_tree";
                    marker.role = "vegetation";
                    marker.height = 2;
                    break;
                case 'R':
                    marker.kind = RuntimeObjectMarkerKind::kReed;
                    marker.type = "shore_reed";
                    marker.role = "vegetation";
                    marker.height = 1;
                    break;
                case 'P':
                    marker.kind = RuntimeObjectMarkerKind::kReed;
                    marker.type = "puddle_reed";
                    marker.role = "vegetation";
                    marker.height = 1;
                    break;
                case 'B':
                    marker.kind = RuntimeObjectMarkerKind::kBush;
                    marker.type = "reclaimed_bush";
                    marker.role = "vegetation";
                    marker.height = 1;
                    break;
                default:
                    continue;
            }
            AddRuntimeObjectMarker(runtime, std::move(marker));
        }
    }

    if (skipped > 0) {
        runtime.diagnostics.AddWarning("runtime vegetation visual skipped short rows count=" + std::to_string(skipped));
    }
}

void ReadWorldOverlayData(RuntimeMap& runtime, const MapPackageInfo& package)
{
    ReadRuntimeObjects(runtime, package);
    ReadPlaces(runtime, package);
    ReadMarkers(runtime, package);
    if (!ReadVegetationGridMarkers(runtime)) {
        ReadLegacyVegetationVisualMarkers(runtime, package);
    }
    runtime.info.object_markers = static_cast<int>(runtime.object_markers.size());
    runtime.info.object_markers_loaded = runtime.info.object_markers > 0;
    runtime.info.vegetation_markers = static_cast<int>(std::count_if(
        runtime.object_markers.begin(),
        runtime.object_markers.end(),
        [](const RuntimeObjectMarker& marker) {
            return marker.visual_only && marker.role == "vegetation";
        }));
    runtime.info.vegetation_markers_loaded = runtime.info.vegetation_markers > 0;
}

void UpdateHeightRange(RuntimeMap& runtime)
{
    if (!runtime.height.IsValid()) {
        return;
    }

    auto [min_it, max_it] = std::minmax_element(runtime.height.cells.begin(), runtime.height.cells.end());
    if (min_it != runtime.height.cells.end() && max_it != runtime.height.cells.end()) {
        runtime.info.levels = LevelRange{*min_it, *max_it};
    }
}

void ValidateRuntimeMap(RuntimeMap& runtime)
{
    if (!runtime.info.IsValid()) {
        runtime.diagnostics.AddWarning("runtime map dimensions are invalid");
        return;
    }

    if (!runtime.terrain.IsValid()) {
        runtime.diagnostics.AddWarning("runtime terrain grid is not loaded");
    }
    if (!runtime.collision.IsValid()) {
        runtime.diagnostics.AddWarning("runtime collision grid is not loaded");
    }
    if (!runtime.height.IsValid()) {
        runtime.diagnostics.AddWarning("runtime height grid is not loaded");
    }
    if (runtime.info.structure_height_loaded && !runtime.structure_height.IsValid()) {
        runtime.diagnostics.AddWarning("runtime structure height grid is not loaded");
    }
    if (runtime.info.structure_type_loaded && !runtime.structure_type.IsValid()) {
        runtime.diagnostics.AddWarning("runtime structure type grid is not loaded");
    }
    if (runtime.info.structure_micro_geometry_loaded
        && !runtime.structure_micro_mask.IsValid()) {
        runtime.diagnostics.AddWarning(
            "runtime structure micro geometry grid is not loaded");
    }
    if (runtime.info.structure_type_loaded
        != runtime.info.structure_micro_geometry_loaded) {
        runtime.diagnostics.AddWarning(
            "runtime structure type and micro geometry sources are incomplete");
    }
    if (runtime.info.structure_micro_geometry_loaded
        && runtime.info.structure_micro_division != 4) {
        runtime.diagnostics.AddWarning(
            "runtime structure micro geometry division is unsupported");
    }
    if (runtime.info.structure_top_geometry_loaded
        && (!runtime.structure_walkway_mask.IsValid()
            || !runtime.structure_parapet_mask.IsValid()
            || !runtime.structure_crenellation_mask.IsValid())) {
        runtime.diagnostics.AddWarning(
            "runtime structure top geometry grids are not loaded");
    }
    if (runtime.info.structure_top_geometry_loaded
        && runtime.info.structure_top_division != 4) {
        runtime.diagnostics.AddWarning(
            "runtime structure top geometry division is unsupported");
    }
    if (runtime.info.structure_top_geometry_loaded
        && (!runtime.info.structure_type_loaded
            || !runtime.info.structure_micro_geometry_loaded)) {
        runtime.diagnostics.AddWarning(
            "runtime structure top geometry sources are incomplete");
    }
    if (runtime.info.vegetation_type_loaded && !runtime.vegetation_type.IsValid()) {
        runtime.diagnostics.AddWarning("runtime vegetation type grid is not loaded");
    }
    if (runtime.info.vegetation_height_loaded && !runtime.vegetation_height.IsValid()) {
        runtime.diagnostics.AddWarning("runtime vegetation height grid is not loaded");
    }
    if (runtime.info.vegetation_type_loaded != runtime.info.vegetation_height_loaded) {
        runtime.diagnostics.AddWarning("runtime vegetation type and height sources are incomplete");
    }
    if (!runtime.info.start_goal_loaded) {
        runtime.diagnostics.AddWarning("runtime start/goal points are not fully loaded");
    }
}

[[nodiscard]] int CountBlockedCells(const RuntimeGrid<std::uint8_t>& grid)
{
    if (!grid.IsValid()) {
        return 0;
    }
    return static_cast<int>(std::count_if(grid.cells.begin(), grid.cells.end(), [](std::uint8_t value) {
        return value != 0;
    }));
}

void UpdateStructureHeightStats(RuntimeMap& runtime)
{
    runtime.info.structure_tiles = 0;
    runtime.info.structure_blocks = 0;
    runtime.info.structure_min_height = 0;
    runtime.info.structure_max_height = 0;
    if (!runtime.structure_height.IsValid()) {
        return;
    }

    for (const std::uint8_t value : runtime.structure_height.cells) {
        if (value == 0U) {
            continue;
        }

        const int height = static_cast<int>(value);
        ++runtime.info.structure_tiles;
        runtime.info.structure_blocks += value;
        if (runtime.info.structure_min_height == 0) {
            runtime.info.structure_min_height = height;
        } else {
            runtime.info.structure_min_height = std::min(
                runtime.info.structure_min_height,
                height);
        }
        runtime.info.structure_max_height = std::max(
            runtime.info.structure_max_height,
            height);
    }
}

void UpdateStructureGeometryStats(RuntimeMap& runtime)
{
    runtime.info.structure_type_tiles = 0;
    runtime.info.structure_micro_tiles = 0;
    runtime.info.structure_micro_full_masks = 0;
    runtime.info.structure_micro_partial_masks = 0;
    runtime.info.structure_micro_solid_subtiles = 0;
    runtime.info.structure_walkway_tiles = 0;
    runtime.info.structure_walkway_subtiles = 0;
    runtime.info.structure_parapet_tiles = 0;
    runtime.info.structure_parapet_subtiles = 0;
    runtime.info.structure_crenellation_tiles = 0;
    runtime.info.structure_crenellation_subtiles = 0;
    if (!runtime.structure_type.IsValid() || !runtime.structure_micro_mask.IsValid()) {
        return;
    }

    const bool top_valid = runtime.structure_walkway_mask.IsValid()
        && runtime.structure_parapet_mask.IsValid()
        && runtime.structure_crenellation_mask.IsValid();
    constexpr std::uint16_t kFullMask =
        std::numeric_limits<std::uint16_t>::max();
    for (std::size_t index = 0; index < runtime.structure_type.cells.size(); ++index) {
        if (runtime.structure_type.cells[index]
            != static_cast<std::uint8_t>(RuntimeStructureType::kNone)) {
            ++runtime.info.structure_type_tiles;
        }
        const std::uint16_t mask = runtime.structure_micro_mask.cells[index];
        if (mask != 0U) {
            ++runtime.info.structure_micro_tiles;
            runtime.info.structure_micro_solid_subtiles += std::popcount(mask);
            if (mask == kFullMask) {
                ++runtime.info.structure_micro_full_masks;
            } else {
                ++runtime.info.structure_micro_partial_masks;
            }
        }
        if (!top_valid) {
            continue;
        }
        const std::uint16_t walkway = runtime.structure_walkway_mask.cells[index];
        const std::uint16_t parapet = runtime.structure_parapet_mask.cells[index];
        const std::uint16_t crenellation =
            runtime.structure_crenellation_mask.cells[index];
        if (walkway != 0U) {
            ++runtime.info.structure_walkway_tiles;
            runtime.info.structure_walkway_subtiles += std::popcount(walkway);
        }
        if (parapet != 0U) {
            ++runtime.info.structure_parapet_tiles;
            runtime.info.structure_parapet_subtiles += std::popcount(parapet);
        }
        if (crenellation != 0U) {
            ++runtime.info.structure_crenellation_tiles;
            runtime.info.structure_crenellation_subtiles +=
                std::popcount(crenellation);
        }
    }
}

void UpdateVegetationStats(RuntimeMap& runtime)
{
    runtime.info.vegetation_trees = 0;
    runtime.info.vegetation_bushes = 0;
    runtime.info.vegetation_shore_reeds = 0;
    runtime.info.vegetation_puddle_reeds = 0;
    runtime.info.vegetation_tree_height_2 = 0;
    runtime.info.vegetation_tree_height_3 = 0;
    runtime.info.vegetation_tree_height_4 = 0;
    runtime.info.vegetation_tree_height_5 = 0;
    runtime.info.vegetation_bush_height_1 = 0;
    runtime.info.vegetation_bush_height_2 = 0;

    if (runtime.info.vegetation_type_loaded && runtime.info.vegetation_height_loaded
        && runtime.vegetation_type.IsValid() && runtime.vegetation_height.IsValid()) {
        for (std::size_t index = 0; index < runtime.vegetation_type.cells.size(); ++index) {
            const auto type = static_cast<RuntimeVegetationType>(runtime.vegetation_type.cells[index]);
            const std::uint8_t height = runtime.vegetation_height.cells[index];
            switch (type) {
                case RuntimeVegetationType::kNone:
                    break;
                case RuntimeVegetationType::kTree:
                    ++runtime.info.vegetation_trees;
                    switch (height) {
                        case 2U:
                            ++runtime.info.vegetation_tree_height_2;
                            break;
                        case 3U:
                            ++runtime.info.vegetation_tree_height_3;
                            break;
                        case 4U:
                            ++runtime.info.vegetation_tree_height_4;
                            break;
                        case 5U:
                            ++runtime.info.vegetation_tree_height_5;
                            break;
                        default:
                            break;
                    }
                    break;
                case RuntimeVegetationType::kBush:
                    ++runtime.info.vegetation_bushes;
                    if (height == 1U) {
                        ++runtime.info.vegetation_bush_height_1;
                    } else if (height == 2U) {
                        ++runtime.info.vegetation_bush_height_2;
                    }
                    break;
                case RuntimeVegetationType::kShoreReed:
                    ++runtime.info.vegetation_shore_reeds;
                    break;
                case RuntimeVegetationType::kPuddleReed:
                    ++runtime.info.vegetation_puddle_reeds;
                    break;
            }
        }
        return;
    }

    for (const RuntimeObjectMarker& marker : runtime.object_markers) {
        if (!marker.visual_only || marker.role != "vegetation") {
            continue;
        }
        switch (marker.kind) {
            case RuntimeObjectMarkerKind::kTree:
                ++runtime.info.vegetation_trees;
                break;
            case RuntimeObjectMarkerKind::kBush:
                ++runtime.info.vegetation_bushes;
                break;
            case RuntimeObjectMarkerKind::kReed:
                if (marker.type == "puddle_reed") {
                    ++runtime.info.vegetation_puddle_reeds;
                } else {
                    ++runtime.info.vegetation_shore_reeds;
                }
                break;
            default:
                break;
        }
    }
}

struct JsonRuntimeCore {
    RuntimeGrid<std::string> terrain;
    RuntimeGrid<std::uint8_t> collision;
    RuntimeGrid<int> height;
    RuntimeGrid<std::uint8_t> structure_height;
    bool structure_height_present = false;
    RuntimeGrid<std::uint8_t> structure_type;
    bool structure_type_present = false;
    RuntimeGrid<std::uint16_t> structure_micro_mask;
    bool structure_micro_geometry_present = false;
    int structure_micro_division = 0;
    RuntimeGrid<std::uint16_t> structure_walkway_mask;
    RuntimeGrid<std::uint16_t> structure_parapet_mask;
    RuntimeGrid<std::uint16_t> structure_crenellation_mask;
    bool structure_top_geometry_present = false;
    int structure_top_division = 0;
    RuntimeGrid<std::uint8_t> vegetation_type;
    RuntimeGrid<std::uint8_t> vegetation_height;
    bool vegetation_type_present = false;
    bool vegetation_height_present = false;
    std::optional<TileCoord> start;
    std::optional<TileCoord> goal;
    bool start_goal_loaded = false;
    Diagnostics diagnostics;
};

[[nodiscard]] bool RuntimeBinaryJsonVerificationEnabled()
{
    const char* value = std::getenv("VOX3D_VERIFY_BINARY_JSON");
    if (value == nullptr) {
        return false;
    }

    const std::string normalized = ToLowerAscii(value);
    return normalized == "1" || normalized == "true" || normalized == "yes" || normalized == "on";
}

[[nodiscard]] int ElapsedMillis(std::chrono::steady_clock::time_point start, std::chrono::steady_clock::time_point end)
{
    return static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());
}

[[nodiscard]] JsonRuntimeCore ReadJsonRuntimeCore(const MapPackageInfo& package, const RuntimeMapInfo& info)
{
    JsonRuntimeCore core;
    core.terrain = ReadTerrainGrid(package, core.diagnostics);
    core.collision = ReadCollisionGrid(package, core.diagnostics);
    core.height = ReadHeightGrid(package, core.diagnostics);
    core.structure_height = ReadStructureHeightGrid(
        package,
        core.structure_height_present,
        core.diagnostics);
    core.structure_type = ReadStructureTypeGrid(
        package,
        core.structure_type_present,
        core.diagnostics);
    core.structure_micro_mask = ReadStructureMicroGeometryGrid(
        package,
        core.structure_micro_geometry_present,
        core.structure_micro_division,
        core.diagnostics);
    StructureTopGeometryGrids top_geometry = ReadStructureTopGeometryGrids(
        package,
        core.diagnostics);
    core.structure_walkway_mask = std::move(top_geometry.walkway);
    core.structure_parapet_mask = std::move(top_geometry.parapet);
    core.structure_crenellation_mask = std::move(top_geometry.crenellation);
    core.structure_top_geometry_present = top_geometry.source_present;
    core.structure_top_division = top_geometry.division;
    const bool structure_geometry_present = core.structure_type_present
        || core.structure_micro_geometry_present
        || core.structure_top_geometry_present;
    if (core.structure_type_present != core.structure_micro_geometry_present) {
        core.diagnostics.AddWarning(
            "runtime structure type and micro geometry sources are incomplete");
    } else if (core.structure_top_geometry_present
               && (!core.structure_type_present
                   || !core.structure_micro_geometry_present)) {
        core.diagnostics.AddWarning(
            "runtime structure top geometry requires structure type and micro geometry");
    } else if (structure_geometry_present && !core.structure_height_present) {
        core.diagnostics.AddWarning(
            "runtime structure geometry requires structure height source");
    }
    const bool structure_geometry_valid = core.structure_type_present
        == core.structure_micro_geometry_present
        && (!core.structure_top_geometry_present
            || (core.structure_type_present
                && core.structure_micro_geometry_present))
        && (!structure_geometry_present
            || (core.structure_height_present
                && core.structure_micro_division == 4
                && (!core.structure_top_geometry_present
                    || core.structure_top_division == 4)
                && ValidateStructureGeometryGrids(
                    core.structure_type,
                    core.structure_height,
                    core.structure_micro_mask,
                    core.structure_walkway_mask,
                    core.structure_parapet_mask,
                    core.structure_crenellation_mask,
                    core.structure_top_geometry_present,
                    core.diagnostics)));
    if (!structure_geometry_valid) {
        core.structure_type.cells.clear();
        core.structure_micro_mask.cells.clear();
        core.structure_walkway_mask.cells.clear();
        core.structure_parapet_mask.cells.clear();
        core.structure_crenellation_mask.cells.clear();
    }
    core.vegetation_type = ReadVegetationTypeGrid(
        package,
        core.vegetation_type_present,
        core.diagnostics);
    core.vegetation_height = ReadVegetationHeightGrid(
        package,
        core.vegetation_height_present,
        core.diagnostics);
    if (core.vegetation_type_present != core.vegetation_height_present
        || ((core.vegetation_type_present || core.vegetation_height_present)
            && !ValidateVegetationGrids(
                core.vegetation_type,
                core.vegetation_height,
                core.diagnostics))) {
        core.vegetation_type.cells.clear();
        core.vegetation_height.cells.clear();
    }

    RuntimeMap start_goal_runtime;
    start_goal_runtime.info.width = info.width;
    start_goal_runtime.info.height = info.height;
    start_goal_runtime.info.tile_size_px = info.tile_size_px;
    ReadStartGoal(start_goal_runtime, package);
    core.start = start_goal_runtime.info.start;
    core.goal = start_goal_runtime.info.goal;
    core.start_goal_loaded = start_goal_runtime.info.start_goal_loaded;
    for (const std::string& warning : start_goal_runtime.diagnostics.warnings) {
        core.diagnostics.AddWarning(warning);
    }
    return core;
}

void ApplyJsonRuntimeCore(RuntimeMap& runtime, JsonRuntimeCore&& core)
{
    runtime.terrain = std::move(core.terrain);
    runtime.collision = std::move(core.collision);
    runtime.height = std::move(core.height);
    runtime.structure_height = std::move(core.structure_height);
    runtime.info.structure_height_loaded = core.structure_height_present
        && runtime.structure_height.IsValid();
    runtime.structure_type = std::move(core.structure_type);
    runtime.info.structure_type_loaded = core.structure_type_present
        && runtime.structure_type.IsValid();
    runtime.structure_micro_mask = std::move(core.structure_micro_mask);
    runtime.info.structure_micro_geometry_loaded =
        core.structure_micro_geometry_present
        && runtime.structure_micro_mask.IsValid();
    runtime.info.structure_micro_division =
        runtime.info.structure_micro_geometry_loaded
        ? core.structure_micro_division
        : 0;
    runtime.structure_walkway_mask = std::move(core.structure_walkway_mask);
    runtime.structure_parapet_mask = std::move(core.structure_parapet_mask);
    runtime.structure_crenellation_mask = std::move(core.structure_crenellation_mask);
    runtime.info.structure_top_geometry_loaded =
        core.structure_top_geometry_present
        && runtime.structure_walkway_mask.IsValid()
        && runtime.structure_parapet_mask.IsValid()
        && runtime.structure_crenellation_mask.IsValid();
    runtime.info.structure_top_division =
        runtime.info.structure_top_geometry_loaded
        ? core.structure_top_division
        : 0;
    runtime.vegetation_type = std::move(core.vegetation_type);
    runtime.vegetation_height = std::move(core.vegetation_height);
    runtime.info.vegetation_type_loaded = core.vegetation_type_present
        && runtime.vegetation_type.IsValid();
    runtime.info.vegetation_height_loaded = core.vegetation_height_present
        && runtime.vegetation_height.IsValid();
    runtime.movement_cost = RuntimeGrid<int>{};
    runtime.projectile_block = RuntimeGrid<std::uint8_t>{};
    runtime.vision_block = RuntimeGrid<std::uint8_t>{};
    runtime.cover = RuntimeGrid<std::uint8_t>{};
    runtime.concealment = RuntimeGrid<std::uint8_t>{};
    runtime.info.start = core.start;
    runtime.info.goal = core.goal;
    runtime.info.start_goal_loaded = core.start_goal_loaded;
    for (const std::string& warning : core.diagnostics.warnings) {
        runtime.diagnostics.AddWarning(warning);
    }
}

template <typename T>
[[nodiscard]] std::size_t CountGridMismatches(const RuntimeGrid<T>& left, const RuntimeGrid<T>& right)
{
    if (left.width != right.width || left.height != right.height || left.cells.size() != right.cells.size()) {
        return std::max(left.cells.size(), right.cells.size());
    }

    std::size_t mismatches = 0;
    for (std::size_t index = 0; index < left.cells.size(); ++index) {
        if (left.cells[index] != right.cells[index]) {
            ++mismatches;
        }
    }
    return mismatches;
}

[[nodiscard]] bool SamePoint(const std::optional<TileCoord>& left, const std::optional<TileCoord>& right)
{
    if (left.has_value() != right.has_value()) {
        return false;
    }
    if (!left.has_value()) {
        return true;
    }
    return left->x == right->x && left->y == right->y;
}

void CompareRuntimeBinaryWithJson(RuntimeMap& runtime, const JsonRuntimeCore& json_core)
{
    RuntimeMapInfo& info = runtime.info;
    info.runtime_binary_json_compare_checked = true;

    if (!json_core.terrain.IsValid() || !json_core.collision.IsValid()
        || !json_core.height.IsValid() || !json_core.structure_height.IsValid()
        || !json_core.structure_type.IsValid()
        || !json_core.structure_micro_mask.IsValid()
        || !json_core.structure_walkway_mask.IsValid()
        || !json_core.structure_parapet_mask.IsValid()
        || !json_core.structure_crenellation_mask.IsValid()
        || !json_core.vegetation_type.IsValid()
        || !json_core.vegetation_height.IsValid()) {
        info.runtime_binary_json_compare_ok = false;
        info.runtime_binary_json_compare_reason = "json_core_unavailable";
        return;
    }

    const bool binary_structure_geometry_present = info.structure_type_loaded
        && info.structure_micro_geometry_loaded;
    const bool json_structure_geometry_present = json_core.structure_type_present
        && json_core.structure_micro_geometry_present;
    const bool binary_top_geometry_present = info.structure_top_geometry_loaded;
    const bool json_top_geometry_present = json_core.structure_top_geometry_present;
    if (binary_structure_geometry_present != json_structure_geometry_present
        || binary_top_geometry_present != json_top_geometry_present
        || (binary_structure_geometry_present
            && info.structure_micro_division != json_core.structure_micro_division)
        || (binary_top_geometry_present
            && info.structure_top_division != json_core.structure_top_division)) {
        info.runtime_binary_json_compare_ok = false;
        info.runtime_binary_json_compare_reason =
            "structure_geometry_presence_mismatch";
        return;
    }

    info.runtime_binary_json_terrain_mismatches = CountGridMismatches(runtime.terrain, json_core.terrain);
    info.runtime_binary_json_collision_mismatches = CountGridMismatches(runtime.collision, json_core.collision);
    info.runtime_binary_json_height_mismatches = CountGridMismatches(runtime.height, json_core.height);
    info.runtime_binary_json_structure_height_mismatches = CountGridMismatches(
        runtime.structure_height,
        json_core.structure_height);
    info.runtime_binary_json_structure_type_mismatches = CountGridMismatches(
        runtime.structure_type,
        json_core.structure_type);
    info.runtime_binary_json_structure_micro_geometry_mismatches =
        CountGridMismatches(
            runtime.structure_micro_mask,
            json_core.structure_micro_mask);
    info.runtime_binary_json_structure_walkway_mismatches = CountGridMismatches(
        runtime.structure_walkway_mask,
        json_core.structure_walkway_mask);
    info.runtime_binary_json_structure_parapet_mismatches = CountGridMismatches(
        runtime.structure_parapet_mask,
        json_core.structure_parapet_mask);
    info.runtime_binary_json_structure_crenellation_mismatches = CountGridMismatches(
        runtime.structure_crenellation_mask,
        json_core.structure_crenellation_mask);
    info.runtime_binary_json_vegetation_type_mismatches = CountGridMismatches(
        runtime.vegetation_type,
        json_core.vegetation_type);
    info.runtime_binary_json_vegetation_height_mismatches = CountGridMismatches(
        runtime.vegetation_height,
        json_core.vegetation_height);
    info.runtime_binary_json_point_mismatches = 0;
    if (!SamePoint(runtime.info.start, json_core.start)) {
        ++info.runtime_binary_json_point_mismatches;
    }
    if (!SamePoint(runtime.info.goal, json_core.goal)) {
        ++info.runtime_binary_json_point_mismatches;
    }

    const std::size_t total = info.runtime_binary_json_terrain_mismatches
        + info.runtime_binary_json_collision_mismatches
        + info.runtime_binary_json_height_mismatches
        + info.runtime_binary_json_structure_height_mismatches
        + info.runtime_binary_json_structure_type_mismatches
        + info.runtime_binary_json_structure_micro_geometry_mismatches
        + info.runtime_binary_json_structure_walkway_mismatches
        + info.runtime_binary_json_structure_parapet_mismatches
        + info.runtime_binary_json_structure_crenellation_mismatches
        + info.runtime_binary_json_vegetation_type_mismatches
        + info.runtime_binary_json_vegetation_height_mismatches
        + info.runtime_binary_json_point_mismatches;
    info.runtime_binary_json_compare_ok = total == 0;
    info.runtime_binary_json_compare_reason = info.runtime_binary_json_compare_ok ? "ok" : "binary_json_mismatch";
}


[[nodiscard]] VxmapRuntimeManifest ToVxmapManifest(const RuntimeBinaryInfo& info)
{
    VxmapRuntimeManifest manifest;
    manifest.declared = info.declared;
    manifest.relative_path = info.relative_path;
    manifest.format = info.format;
    manifest.format_major = info.format_major;
    manifest.format_minor = info.format_minor;
    manifest.build_id_hex = info.build_id_hex;
    manifest.file_size = info.file_size;
    manifest.section_count = info.section_count;
    manifest.region_size_tiles = info.region_size_tiles;
    manifest.regions_x = info.regions_x;
    manifest.regions_y = info.regions_y;
    manifest.regions_total = info.regions_total;
    return manifest;
}

bool TryLoadRuntimeBinaryCore(RuntimeMap& runtime, const MapPackageInfo& package)
{
    if (!package.runtime_binary.declared) {
        runtime.info.runtime_binary_checked = false;
        return false;
    }

    VxmapRuntimeCore core = LoadVxmapRuntimeCore(package.path, ToVxmapManifest(package.runtime_binary));
    runtime.info.runtime_binary_checked = true;
    runtime.info.runtime_binary_valid = core.validation.valid;
    runtime.info.runtime_binary_fallback_reason = core.fallback_reason;
    runtime.info.runtime_binary_read_ms = core.validation.read_ms;
    runtime.info.runtime_binary_validate_ms = core.validation.validate_ms;
    runtime.info.runtime_binary_decode_ms = core.decode_ms;
    runtime.info.runtime_binary_total_ms = core.total_ms;

    if (!core.loaded) {
        runtime.info.runtime_binary_loaded = false;
        runtime.diagnostics.AddWarning("runtime binary fast path unavailable reason=" + core.fallback_reason);
        return false;
    }

    if (static_cast<int>(core.width_tiles) != runtime.info.width || static_cast<int>(core.height_tiles) != runtime.info.height
        || static_cast<int>(core.tile_size_px) != runtime.info.tile_size_px) {
        runtime.info.runtime_binary_valid = false;
        runtime.info.runtime_binary_loaded = false;
        runtime.info.runtime_binary_fallback_reason = "binary_dimensions_mismatch";
        runtime.diagnostics.AddWarning("runtime binary fast path unavailable reason=binary_dimensions_mismatch");
        return false;
    }

    runtime.terrain.width = runtime.info.width;
    runtime.terrain.height = runtime.info.height;
    runtime.terrain.cells = std::move(core.terrain);

    runtime.collision.width = runtime.info.width;
    runtime.collision.height = runtime.info.height;
    runtime.collision.cells = std::move(core.collision);

    runtime.height.width = runtime.info.width;
    runtime.height.height = runtime.info.height;
    runtime.height.cells.assign(core.elevation.begin(), core.elevation.end());

    runtime.structure_height.width = runtime.info.width;
    runtime.structure_height.height = runtime.info.height;
    runtime.structure_height.cells = std::move(core.structure_height);
    runtime.info.structure_height_loaded = core.structure_height_present
        && runtime.structure_height.IsValid();

    runtime.structure_type.width = runtime.info.width;
    runtime.structure_type.height = runtime.info.height;
    runtime.structure_type.cells = std::move(core.structure_type);
    runtime.info.structure_type_loaded = core.structure_type_present
        && runtime.structure_type.IsValid();

    runtime.structure_micro_mask.width = runtime.info.width;
    runtime.structure_micro_mask.height = runtime.info.height;
    runtime.structure_micro_mask.cells = std::move(core.structure_micro_mask);
    runtime.info.structure_micro_geometry_loaded =
        core.structure_micro_geometry_present
        && runtime.structure_micro_mask.IsValid();
    runtime.info.structure_micro_division =
        runtime.info.structure_micro_geometry_loaded
        ? core.structure_micro_division
        : 0;

    runtime.structure_walkway_mask.width = runtime.info.width;
    runtime.structure_walkway_mask.height = runtime.info.height;
    runtime.structure_walkway_mask.cells = std::move(core.structure_walkway_mask);
    runtime.structure_parapet_mask.width = runtime.info.width;
    runtime.structure_parapet_mask.height = runtime.info.height;
    runtime.structure_parapet_mask.cells = std::move(core.structure_parapet_mask);
    runtime.structure_crenellation_mask.width = runtime.info.width;
    runtime.structure_crenellation_mask.height = runtime.info.height;
    runtime.structure_crenellation_mask.cells = std::move(core.structure_crenellation_mask);
    runtime.info.structure_top_geometry_loaded =
        core.structure_top_geometry_present
        && runtime.structure_walkway_mask.IsValid()
        && runtime.structure_parapet_mask.IsValid()
        && runtime.structure_crenellation_mask.IsValid();
    runtime.info.structure_top_division =
        runtime.info.structure_top_geometry_loaded
        ? core.structure_top_division
        : 0;

    runtime.vegetation_type.width = runtime.info.width;
    runtime.vegetation_type.height = runtime.info.height;
    runtime.vegetation_type.cells = std::move(core.vegetation_type);
    runtime.info.vegetation_type_loaded = core.vegetation_type_present
        && runtime.vegetation_type.IsValid();

    runtime.vegetation_height.width = runtime.info.width;
    runtime.vegetation_height.height = runtime.info.height;
    runtime.vegetation_height.cells = std::move(core.vegetation_height);
    runtime.info.vegetation_height_loaded = core.vegetation_height_present
        && runtime.vegetation_height.IsValid();

    runtime.movement_cost.width = runtime.info.width;
    runtime.movement_cost.height = runtime.info.height;
    runtime.movement_cost.cells.assign(core.movement_cost.begin(), core.movement_cost.end());

    runtime.projectile_block.width = runtime.info.width;
    runtime.projectile_block.height = runtime.info.height;
    runtime.projectile_block.cells = std::move(core.projectile_block);

    runtime.vision_block.width = runtime.info.width;
    runtime.vision_block.height = runtime.info.height;
    runtime.vision_block.cells = std::move(core.vision_block);

    runtime.cover.width = runtime.info.width;
    runtime.cover.height = runtime.info.height;
    runtime.cover.cells = std::move(core.cover);

    runtime.concealment.width = runtime.info.width;
    runtime.concealment.height = runtime.info.height;
    runtime.concealment.cells = std::move(core.concealment);

    runtime.info.start = core.start;
    runtime.info.goal = core.goal;
    runtime.info.start_goal_loaded = runtime.info.start.has_value() && runtime.info.goal.has_value();
    runtime.info.runtime_binary_loaded = true;
    runtime.info.runtime_binary_fallback_reason.clear();
    return true;
}

void BuildRuntimeTerrainOverview(RuntimeMap& runtime)
{
    if (runtime.overview.IsValid() || !runtime.terrain.IsValid()) {
        return;
    }

    MapOverview overview;
    overview.width = runtime.terrain.width;
    overview.height = runtime.terrain.height;
    overview.cells.reserve(runtime.terrain.cells.size());
    for (const std::string& terrain : runtime.terrain.cells) {
        overview.cells.push_back(ClassifyTerrainCell(terrain));
    }
    overview.source_file = runtime.info.runtime_binary_loaded
        ? "map_runtime.vxmap:terrain"
        : "runtime_terrain";
    overview.terrain_loaded = true;
    runtime.overview = std::move(overview);
}

[[nodiscard]] std::string FormatPoint(const std::optional<TileCoord>& coord)
{
    if (!coord.has_value()) {
        return "none";
    }
    return std::to_string(coord->x) + "," + std::to_string(coord->y);
}

}  // namespace

std::string_view ToString(RuntimeStructureType type)
{
    switch (type) {
        case RuntimeStructureType::kNone:
            return "none";
        case RuntimeStructureType::kRuinWall:
            return "ruin_wall";
        case RuntimeStructureType::kRuinFloor:
            return "ruin_floor";
        case RuntimeStructureType::kFortressWall:
            return "fortress_wall";
        case RuntimeStructureType::kFortressTower:
            return "fortress_tower";
        case RuntimeStructureType::kFortressGate:
            return "fortress_gate";
        case RuntimeStructureType::kFortressKeep:
            return "fortress_keep";
        case RuntimeStructureType::kFortressBuilding:
            return "fortress_building";
        case RuntimeStructureType::kFortressFloor:
            return "fortress_floor";
        case RuntimeStructureType::kBuildingWall:
            return "building_wall";
        case RuntimeStructureType::kBuildingFloor:
            return "building_floor";
    }
    return "unknown";
}

bool IsKnownStructureType(RuntimeStructureType type)
{
    return ToString(type) != "unknown";
}

bool IsVerticalStructureType(RuntimeStructureType type)
{
    switch (type) {
        case RuntimeStructureType::kRuinWall:
        case RuntimeStructureType::kFortressWall:
        case RuntimeStructureType::kFortressTower:
        case RuntimeStructureType::kFortressKeep:
        case RuntimeStructureType::kFortressBuilding:
        case RuntimeStructureType::kBuildingWall:
            return true;
        case RuntimeStructureType::kNone:
        case RuntimeStructureType::kRuinFloor:
        case RuntimeStructureType::kFortressGate:
        case RuntimeStructureType::kFortressFloor:
        case RuntimeStructureType::kBuildingFloor:
            return false;
    }
    return false;
}

bool IsFloorStructureType(RuntimeStructureType type)
{
    switch (type) {
        case RuntimeStructureType::kRuinFloor:
        case RuntimeStructureType::kFortressGate:
        case RuntimeStructureType::kFortressFloor:
        case RuntimeStructureType::kBuildingFloor:
            return true;
        case RuntimeStructureType::kNone:
        case RuntimeStructureType::kRuinWall:
        case RuntimeStructureType::kFortressWall:
        case RuntimeStructureType::kFortressTower:
        case RuntimeStructureType::kFortressKeep:
        case RuntimeStructureType::kFortressBuilding:
        case RuntimeStructureType::kBuildingWall:
            return false;
    }
    return false;
}

std::string_view ToString(RuntimeVegetationType type)
{
    switch (type) {
        case RuntimeVegetationType::kNone:
            return "none";
        case RuntimeVegetationType::kTree:
            return "tree";
        case RuntimeVegetationType::kBush:
            return "bush";
        case RuntimeVegetationType::kShoreReed:
            return "shore_reed";
        case RuntimeVegetationType::kPuddleReed:
            return "puddle_reed";
    }
    return "unknown";
}

bool RuntimeTileBounds::IsValid() const
{
    return min_x <= max_x && min_y <= max_y;
}

bool RuntimeTileBounds::Contains(TileCoord tile) const
{
    return IsValid() && tile.x >= min_x && tile.x <= max_x
        && tile.y >= min_y && tile.y <= max_y;
}

bool RuntimeMapInfo::IsValid() const
{
    return width > 0 && height > 0 && tile_size_px > 0;
}

bool RuntimeMap::IsValid() const
{
    return info.IsValid();
}

bool RuntimeMap::HasCoreGrids() const
{
    return terrain.IsValid() && collision.IsValid() && height.IsValid();
}

RuntimeMap BuildRuntimeMap(const MapPackageInfo& package)
{
    RuntimeMap runtime;
    runtime.info.width = package.width.value_or(0);
    runtime.info.height = package.height.value_or(0);
    runtime.info.tile_size_px = package.tile_size.value_or(0);
    if (package.min_level.has_value() && package.max_level.has_value()) {
        runtime.info.levels = LevelRange{*package.min_level, *package.max_level};
    }
    runtime.info.generator_version = package.generator_version;
    runtime.info.schema_version = package.schema_version;
    runtime.overview = package.overview;

    if (!package.loaded || !runtime.info.IsValid()) {
        ValidateRuntimeMap(runtime);
        return runtime;
    }

    bool json_core_applied = false;
    const bool loaded_from_binary = TryLoadRuntimeBinaryCore(runtime, package);
    if (loaded_from_binary && RuntimeBinaryJsonVerificationEnabled()) {
        const auto json_load_start = std::chrono::steady_clock::now();
        JsonRuntimeCore json_core = ReadJsonRuntimeCore(package, runtime.info);
        const auto json_load_end = std::chrono::steady_clock::now();

        const auto compare_start = std::chrono::steady_clock::now();
        CompareRuntimeBinaryWithJson(runtime, json_core);
        const auto compare_end = std::chrono::steady_clock::now();
        runtime.info.runtime_binary_json_load_ms = ElapsedMillis(json_load_start, json_load_end);
        runtime.info.runtime_binary_json_compare_ms = ElapsedMillis(compare_start, compare_end);

        if (!runtime.info.runtime_binary_json_compare_ok) {
            runtime.info.runtime_binary_valid = false;
            runtime.info.runtime_binary_loaded = false;
            runtime.info.runtime_binary_fallback_reason = runtime.info.runtime_binary_json_compare_reason;
            runtime.diagnostics.AddWarning(
                "runtime binary fast path rejected reason=" + runtime.info.runtime_binary_json_compare_reason);
            ApplyJsonRuntimeCore(runtime, std::move(json_core));
            json_core_applied = true;
        }
    }

    if (!runtime.info.runtime_binary_loaded && !json_core_applied) {
        runtime.terrain = ReadTerrainGrid(package, runtime.diagnostics);
        runtime.collision = ReadCollisionGrid(package, runtime.diagnostics);
        runtime.height = ReadHeightGrid(package, runtime.diagnostics);
        bool structure_height_present = false;
        runtime.structure_height = ReadStructureHeightGrid(
            package,
            structure_height_present,
            runtime.diagnostics);
        runtime.info.structure_height_loaded = structure_height_present
            && runtime.structure_height.IsValid();
        bool structure_type_present = false;
        bool structure_micro_geometry_present = false;
        int structure_micro_division = 0;
        runtime.structure_type = ReadStructureTypeGrid(
            package,
            structure_type_present,
            runtime.diagnostics);
        runtime.structure_micro_mask = ReadStructureMicroGeometryGrid(
            package,
            structure_micro_geometry_present,
            structure_micro_division,
            runtime.diagnostics);
        StructureTopGeometryGrids top_geometry = ReadStructureTopGeometryGrids(
            package,
            runtime.diagnostics);
        runtime.structure_walkway_mask = std::move(top_geometry.walkway);
        runtime.structure_parapet_mask = std::move(top_geometry.parapet);
        runtime.structure_crenellation_mask = std::move(top_geometry.crenellation);
        const bool structure_top_geometry_present = top_geometry.source_present;
        const int structure_top_division = top_geometry.division;
        const bool structure_geometry_present = structure_type_present
            || structure_micro_geometry_present
            || structure_top_geometry_present;
        if (structure_type_present != structure_micro_geometry_present) {
            runtime.diagnostics.AddWarning(
                "runtime structure type and micro geometry sources are incomplete");
        } else if (structure_top_geometry_present
                   && (!structure_type_present
                       || !structure_micro_geometry_present)) {
            runtime.diagnostics.AddWarning(
                "runtime structure top geometry requires structure type and micro geometry");
        } else if (structure_geometry_present && !structure_height_present) {
            runtime.diagnostics.AddWarning(
                "runtime structure geometry requires structure height source");
        }
        const bool structure_geometry_valid = structure_type_present
            == structure_micro_geometry_present
            && (!structure_top_geometry_present
                || (structure_type_present && structure_micro_geometry_present))
            && (!structure_geometry_present
                || (structure_height_present
                    && structure_micro_division == 4
                    && (!structure_top_geometry_present
                        || structure_top_division == 4)
                    && ValidateStructureGeometryGrids(
                        runtime.structure_type,
                        runtime.structure_height,
                        runtime.structure_micro_mask,
                        runtime.structure_walkway_mask,
                        runtime.structure_parapet_mask,
                        runtime.structure_crenellation_mask,
                        structure_top_geometry_present,
                        runtime.diagnostics)));
        runtime.info.structure_type_loaded = structure_geometry_valid
            && structure_type_present && runtime.structure_type.IsValid();
        runtime.info.structure_micro_geometry_loaded = structure_geometry_valid
            && structure_micro_geometry_present
            && runtime.structure_micro_mask.IsValid();
        runtime.info.structure_micro_division =
            runtime.info.structure_micro_geometry_loaded
            ? structure_micro_division
            : 0;
        runtime.info.structure_top_geometry_loaded = structure_geometry_valid
            && structure_top_geometry_present
            && runtime.structure_walkway_mask.IsValid()
            && runtime.structure_parapet_mask.IsValid()
            && runtime.structure_crenellation_mask.IsValid();
        runtime.info.structure_top_division =
            runtime.info.structure_top_geometry_loaded
            ? structure_top_division
            : 0;
        if (!structure_geometry_valid) {
            runtime.structure_type.cells.clear();
            runtime.structure_micro_mask.cells.clear();
            runtime.structure_walkway_mask.cells.clear();
            runtime.structure_parapet_mask.cells.clear();
            runtime.structure_crenellation_mask.cells.clear();
        }
        bool vegetation_type_present = false;
        bool vegetation_height_present = false;
        runtime.vegetation_type = ReadVegetationTypeGrid(
            package,
            vegetation_type_present,
            runtime.diagnostics);
        runtime.vegetation_height = ReadVegetationHeightGrid(
            package,
            vegetation_height_present,
            runtime.diagnostics);
        const bool vegetation_valid = vegetation_type_present == vegetation_height_present
            && (!vegetation_type_present
                || ValidateVegetationGrids(
                    runtime.vegetation_type,
                    runtime.vegetation_height,
                    runtime.diagnostics));
        runtime.info.vegetation_type_loaded = vegetation_valid
            && vegetation_type_present && runtime.vegetation_type.IsValid();
        runtime.info.vegetation_height_loaded = vegetation_valid
            && vegetation_height_present && runtime.vegetation_height.IsValid();
        ReadStartGoal(runtime, package);
    }
    runtime.info.terrain_loaded = runtime.terrain.IsValid();
    runtime.info.collision_loaded = runtime.collision.IsValid();
    runtime.info.elevation_loaded = runtime.height.IsValid();
    if (!runtime.structure_height.IsValid()) {
        runtime.structure_height.width = runtime.info.width;
        runtime.structure_height.height = runtime.info.height;
        runtime.structure_height.cells.assign(
            ExpectedCellCount(runtime.info.width, runtime.info.height),
            std::uint8_t{0});
    }
    if (!runtime.structure_type.IsValid()) {
        runtime.structure_type.width = runtime.info.width;
        runtime.structure_type.height = runtime.info.height;
        runtime.structure_type.cells.assign(
            ExpectedCellCount(runtime.info.width, runtime.info.height),
            static_cast<std::uint8_t>(RuntimeStructureType::kNone));
        runtime.info.structure_type_loaded = false;
    }
    if (!runtime.structure_micro_mask.IsValid()) {
        runtime.structure_micro_mask.width = runtime.info.width;
        runtime.structure_micro_mask.height = runtime.info.height;
        runtime.structure_micro_mask.cells.assign(
            ExpectedCellCount(runtime.info.width, runtime.info.height),
            std::uint16_t{0});
        runtime.info.structure_micro_geometry_loaded = false;
        runtime.info.structure_micro_division = 0;
    }
    if (!runtime.structure_walkway_mask.IsValid()
        || !runtime.structure_parapet_mask.IsValid()
        || !runtime.structure_crenellation_mask.IsValid()) {
        runtime.structure_walkway_mask.width = runtime.info.width;
        runtime.structure_walkway_mask.height = runtime.info.height;
        runtime.structure_walkway_mask.cells.assign(
            ExpectedCellCount(runtime.info.width, runtime.info.height),
            std::uint16_t{0});
        runtime.structure_parapet_mask.width = runtime.info.width;
        runtime.structure_parapet_mask.height = runtime.info.height;
        runtime.structure_parapet_mask.cells.assign(
            ExpectedCellCount(runtime.info.width, runtime.info.height),
            std::uint16_t{0});
        runtime.structure_crenellation_mask.width = runtime.info.width;
        runtime.structure_crenellation_mask.height = runtime.info.height;
        runtime.structure_crenellation_mask.cells.assign(
            ExpectedCellCount(runtime.info.width, runtime.info.height),
            std::uint16_t{0});
        runtime.info.structure_top_geometry_loaded = false;
        runtime.info.structure_top_division = 0;
    }
    if (!runtime.vegetation_type.IsValid()) {
        runtime.vegetation_type.width = runtime.info.width;
        runtime.vegetation_type.height = runtime.info.height;
        runtime.vegetation_type.cells.assign(
            ExpectedCellCount(runtime.info.width, runtime.info.height),
            std::uint8_t{0});
    }
    if (!runtime.vegetation_height.IsValid()) {
        runtime.vegetation_height.width = runtime.info.width;
        runtime.vegetation_height.height = runtime.info.height;
        runtime.vegetation_height.cells.assign(
            ExpectedCellCount(runtime.info.width, runtime.info.height),
            std::uint8_t{0});
    }
    runtime.info.movement_cost_loaded = runtime.movement_cost.IsValid();
    runtime.info.projectile_block_loaded = runtime.projectile_block.IsValid();
    runtime.info.vision_block_loaded = runtime.vision_block.IsValid();
    runtime.info.cover_loaded = runtime.cover.IsValid();
    runtime.info.concealment_loaded = runtime.concealment.IsValid();
    BuildRuntimeTerrainOverview(runtime);
    runtime.info.blocked_cells = CountBlockedCells(runtime.collision);
    UpdateStructureHeightStats(runtime);
    UpdateStructureGeometryStats(runtime);
    UpdateHeightRange(runtime);
    ReadWorldOverlayData(runtime, package);
    UpdateVegetationStats(runtime);
    ValidateRuntimeMap(runtime);
    return runtime;
}

std::string ToLogString(const RuntimeMap& map)
{
    std::ostringstream out;
    out << "status=" << (map.IsValid() ? "loaded" : "invalid");
    if (map.info.IsValid()) {
        out << " size=" << map.info.width << 'x' << map.info.height << " tile=" << map.info.tile_size_px;
    }
    if (map.info.levels.has_value()) {
        out << " levels=" << map.info.levels->min << ".." << map.info.levels->max;
    }
    out << " terrain=" << (map.info.terrain_loaded ? "loaded" : "missing");
    out << " collision=" << (map.info.collision_loaded ? "loaded" : "missing");
    if (map.info.collision_loaded) {
        out << " blocked=" << map.info.blocked_cells;
    }
    out << " height=" << (map.info.elevation_loaded ? "loaded" : "missing");
    out << " structure_height=" << (map.info.structure_height_loaded ? "loaded" : "default_zero");
    if (map.info.structure_height_loaded) {
        out << " structure_tiles=" << map.info.structure_tiles;
        out << " structure_blocks=" << map.info.structure_blocks;
        out << " structure_range=" << map.info.structure_min_height
            << ".." << map.info.structure_max_height;
    }
    out << " structure_type="
        << (map.info.structure_type_loaded ? "loaded" : "legacy");
    out << " structure_micro="
        << (map.info.structure_micro_geometry_loaded ? "loaded" : "legacy");
    if (map.info.structure_micro_geometry_loaded) {
        out << " micro_division=" << map.info.structure_micro_division;
        out << " structure_type_tiles=" << map.info.structure_type_tiles;
        out << " micro_tiles=" << map.info.structure_micro_tiles;
        out << " micro_full=" << map.info.structure_micro_full_masks;
        out << " micro_partial=" << map.info.structure_micro_partial_masks;
        out << " micro_subtiles=" << map.info.structure_micro_solid_subtiles;
    }
    out << " structure_top="
        << (map.info.structure_top_geometry_loaded ? "loaded" : "missing");
    if (map.info.structure_top_geometry_loaded) {
        out << " top_division=" << map.info.structure_top_division;
        out << " walkway=" << map.info.structure_walkway_tiles
            << '/' << map.info.structure_walkway_subtiles;
        out << " parapet=" << map.info.structure_parapet_tiles
            << '/' << map.info.structure_parapet_subtiles;
        out << " crenellation=" << map.info.structure_crenellation_tiles
            << '/' << map.info.structure_crenellation_subtiles;
    }
    out << " vegetation_type=" << (map.info.vegetation_type_loaded ? "loaded" : "legacy");
    out << " vegetation_height=" << (map.info.vegetation_height_loaded ? "loaded" : "legacy");
    out << " movement=" << (map.info.movement_cost_loaded ? "loaded" : "missing");
    out << " projectile_block=" << (map.info.projectile_block_loaded ? "loaded" : "missing");
    out << " vision_block=" << (map.info.vision_block_loaded ? "loaded" : "missing");
    out << " cover=" << (map.info.cover_loaded ? "loaded" : "missing");
    out << " concealment=" << (map.info.concealment_loaded ? "loaded" : "missing");
    out << " start=" << FormatPoint(map.info.start);
    out << " goal=" << FormatPoint(map.info.goal);
    out << " object_markers=" << map.info.object_markers;
    out << " runtime_objects=" << map.info.runtime_objects;
    out << " vegetation_markers=" << map.info.vegetation_markers;
    out << " vegetation_trees=" << map.info.vegetation_trees;
    out << " vegetation_bushes=" << map.info.vegetation_bushes;
    out << " vegetation_shore_reeds=" << map.info.vegetation_shore_reeds;
    out << " vegetation_puddle_reeds=" << map.info.vegetation_puddle_reeds;
    out << " places=" << map.info.places;
    out << " markers=" << map.info.markers;
    if (map.info.runtime_binary_checked) {
        out << " runtime_binary=";
        if (map.info.runtime_binary_loaded) {
            out << "loaded";
        } else {
            out << (map.info.runtime_binary_valid ? "validated" : "fallback");
        }
        if (!map.info.runtime_binary_valid && !map.info.runtime_binary_fallback_reason.empty()) {
            out << " reason=" << map.info.runtime_binary_fallback_reason;
        }
        out << " vxmap_read_ms=" << map.info.runtime_binary_read_ms;
        out << " vxmap_validate_ms=" << map.info.runtime_binary_validate_ms;
        out << " vxmap_decode_ms=" << map.info.runtime_binary_decode_ms;
        out << " vxmap_total_ms=" << map.info.runtime_binary_total_ms;
    }
    if (map.info.runtime_binary_json_compare_checked) {
        out << " binary_vs_json=" << (map.info.runtime_binary_json_compare_ok ? "ok" : "mismatch");
        if (!map.info.runtime_binary_json_compare_ok) {
            out << " terrain_mismatch=" << map.info.runtime_binary_json_terrain_mismatches;
            out << " collision_mismatch=" << map.info.runtime_binary_json_collision_mismatches;
            out << " height_mismatch=" << map.info.runtime_binary_json_height_mismatches;
            out << " structure_height_mismatch="
                << map.info.runtime_binary_json_structure_height_mismatches;
            out << " structure_type_mismatch="
                << map.info.runtime_binary_json_structure_type_mismatches;
            out << " structure_micro_mismatch="
                << map.info.runtime_binary_json_structure_micro_geometry_mismatches;
            out << " vegetation_type_mismatch="
                << map.info.runtime_binary_json_vegetation_type_mismatches;
            out << " vegetation_height_mismatch="
                << map.info.runtime_binary_json_vegetation_height_mismatches;
            out << " point_mismatch=" << map.info.runtime_binary_json_point_mismatches;
        }
        out << " json_load_ms=" << map.info.runtime_binary_json_load_ms;
        out << " compare_ms=" << map.info.runtime_binary_json_compare_ms;
    }
    if (!map.info.generator_version.empty()) {
        out << " generator=" << map.info.generator_version;
    }
    if (!map.info.schema_version.empty()) {
        out << " schema=" << map.info.schema_version;
    }
    if (!map.diagnostics.warnings.empty()) {
        out << " warnings=" << map.diagnostics.warnings.size();
    }
    return out.str();
}

}  // namespace vox3d
