#include "vox3d/map/vxmap_runtime_reader.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace vox3d {
namespace {

constexpr std::uint64_t kHeaderSize = 128;
constexpr std::uint64_t kSectionEntrySize = 64;
constexpr std::uint32_t kSupportedMajor = 1;
constexpr std::uint32_t kSupportedMinor = 2;
constexpr std::uint32_t kEndianMarker = 0x01020304U;
constexpr std::uint32_t kKnownHeaderFlags = 0x0000006FU;
constexpr std::uint32_t kExpectedHeaderFlags = 0x0000004FU;
constexpr std::uint16_t kCodecNone = 0;
constexpr std::uint32_t kSectionFlagRequired = 0x01U;
constexpr std::uint32_t kMaxSectionCount = 1'000'000U;
constexpr std::uint32_t kMaxTerrainCount = 65'535U;
constexpr std::uint64_t kMaxBinaryReadBytes = 512ULL * 1024ULL * 1024ULL;

constexpr std::uint32_t kTypeMetadata = 1;
constexpr std::uint32_t kTypeStringTable = 2;
constexpr std::uint32_t kTypeStringIdPool = 3;
constexpr std::uint32_t kTypeRegionIndex = 5;
constexpr std::uint32_t kTypeTerrainCatalog = 10;
constexpr std::uint32_t kTypeTerrainGrid = 20;
constexpr std::uint32_t kTypeElevationGrid = 21;
constexpr std::uint32_t kTypeMovementGrid = 22;
constexpr std::uint32_t kTypeCollisionBits = 23;
constexpr std::uint32_t kTypeProjectileBlockBits = 24;
constexpr std::uint32_t kTypeVisionBlockBits = 25;
constexpr std::uint32_t kTypeCoverGrid = 26;
constexpr std::uint32_t kTypeConcealmentGrid = 27;
constexpr std::uint32_t kTypeStructureHeightGrid = 28;
constexpr std::uint32_t kTypeStartGoal = 30;
constexpr std::uint32_t kTypeVegetationTypeGrid = 31;
constexpr std::uint32_t kTypeVegetationHeightGrid = 32;

constexpr std::array<std::uint32_t, 6> kRequiredGlobalSections{
    kTypeMetadata,
    kTypeStringTable,
    kTypeStringIdPool,
    kTypeRegionIndex,
    kTypeTerrainCatalog,
    kTypeStartGoal,
};

constexpr std::array<std::uint32_t, 8> kRequiredRegionSectionsV10{
    kTypeTerrainGrid,
    kTypeElevationGrid,
    kTypeMovementGrid,
    kTypeCollisionBits,
    kTypeProjectileBlockBits,
    kTypeVisionBlockBits,
    kTypeCoverGrid,
    kTypeConcealmentGrid,
};

constexpr std::array<std::uint32_t, 9> kRequiredRegionSectionsV11{
    kTypeTerrainGrid,
    kTypeElevationGrid,
    kTypeMovementGrid,
    kTypeCollisionBits,
    kTypeProjectileBlockBits,
    kTypeVisionBlockBits,
    kTypeCoverGrid,
    kTypeConcealmentGrid,
    kTypeStructureHeightGrid,
};

constexpr std::array<std::uint32_t, 11> kRequiredRegionSectionsV12{
    kTypeTerrainGrid,
    kTypeElevationGrid,
    kTypeMovementGrid,
    kTypeCollisionBits,
    kTypeProjectileBlockBits,
    kTypeVisionBlockBits,
    kTypeCoverGrid,
    kTypeConcealmentGrid,
    kTypeStructureHeightGrid,
    kTypeVegetationTypeGrid,
    kTypeVegetationHeightGrid,
};


struct Header {
    std::uint32_t format_major = 0;
    std::uint32_t format_minor = 0;
    std::uint32_t header_flags = 0;
    std::uint32_t section_count = 0;
    std::uint32_t width_tiles = 0;
    std::uint32_t height_tiles = 0;
    std::uint16_t tile_size_px = 0;
    std::int16_t min_elevation = 0;
    std::int16_t max_elevation = 0;
    std::uint16_t region_size_tiles = 0;
    std::uint64_t section_table_offset = 0;
    std::uint64_t section_table_size = 0;
    std::uint64_t file_size = 0;
    std::array<std::uint8_t, 16> build_id{};
    std::uint32_t header_crc32 = 0;
    std::uint32_t table_crc32 = 0;
};

[[nodiscard]] std::uint32_t RequiredRegionalSectionCount(const Header& header)
{
    if (header.format_minor >= 2U) {
        return static_cast<std::uint32_t>(kRequiredRegionSectionsV12.size());
    }
    if (header.format_minor >= 1U) {
        return static_cast<std::uint32_t>(kRequiredRegionSectionsV11.size());
    }
    return static_cast<std::uint32_t>(kRequiredRegionSectionsV10.size());
}

[[nodiscard]] bool IsKnownRequiredRegionalSection(const Header& header, std::uint32_t section_type)
{
    if (std::find(kRequiredRegionSectionsV10.begin(), kRequiredRegionSectionsV10.end(), section_type)
        != kRequiredRegionSectionsV10.end()) {
        return true;
    }
    if (header.format_minor >= 1U && section_type == kTypeStructureHeightGrid) {
        return true;
    }
    return header.format_minor >= 2U
        && (section_type == kTypeVegetationTypeGrid
            || section_type == kTypeVegetationHeightGrid);
}

struct SectionEntry {
    std::uint32_t section_type = 0;
    std::uint32_t section_flags = 0;
    std::uint32_t section_id = 0;
    std::uint32_t parent_id = 0;
    std::uint64_t offset = 0;
    std::uint64_t stored_size = 0;
    std::uint64_t unpacked_size = 0;
    std::uint32_t element_count = 0;
    std::uint32_t element_stride = 0;
    std::uint16_t codec = 0;
    std::uint16_t alignment = 0;
    std::uint32_t crc32 = 0;
    std::uint32_t aux_0 = 0;
    std::uint32_t aux_1 = 0;
};

struct ValidatedContainer {
    std::vector<std::uint8_t> data;
    Header header;
    std::vector<SectionEntry> entries;
    VxmapRuntimeValidationReport report;
};

struct RegionRecord {
    std::uint32_t region_id = 0;
    std::uint16_t region_x = 0;
    std::uint16_t region_y = 0;
    std::uint32_t origin_x = 0;
    std::uint32_t origin_y = 0;
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    std::uint32_t first_section_table_index = 0;
    std::uint16_t section_count = 0;
    std::uint16_t flags = 0;
    std::uint32_t tile_count = 0;
};

[[nodiscard]] int ElapsedMilliseconds(
    std::chrono::steady_clock::time_point start,
    std::chrono::steady_clock::time_point end)
{
    return static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());
}

[[nodiscard]] bool CheckedAdd(std::uint64_t left, std::uint64_t right, std::uint64_t& result)
{
    if (left > std::numeric_limits<std::uint64_t>::max() - right) {
        return false;
    }
    result = left + right;
    return true;
}

[[nodiscard]] bool CheckedMultiply(std::uint64_t left, std::uint64_t right, std::uint64_t& result)
{
    if (left != 0 && right > std::numeric_limits<std::uint64_t>::max() / left) {
        return false;
    }
    result = left * right;
    return true;
}

[[nodiscard]] bool IsPowerOfTwo(std::uint64_t value)
{
    return value != 0 && (value & (value - 1U)) == 0;
}

[[nodiscard]] bool ReadU16Le(const std::vector<std::uint8_t>& data, std::uint64_t offset, std::uint16_t& value)
{
    if (offset > data.size() || data.size() - static_cast<std::size_t>(offset) < 2U) {
        return false;
    }
    const auto base = static_cast<std::size_t>(offset);
    value = static_cast<std::uint16_t>(data[base]) | static_cast<std::uint16_t>(data[base + 1U] << 8U);
    return true;
}

[[nodiscard]] bool ReadU32Le(const std::vector<std::uint8_t>& data, std::uint64_t offset, std::uint32_t& value)
{
    if (offset > data.size() || data.size() - static_cast<std::size_t>(offset) < 4U) {
        return false;
    }
    const auto base = static_cast<std::size_t>(offset);
    value = static_cast<std::uint32_t>(data[base])
        | (static_cast<std::uint32_t>(data[base + 1U]) << 8U)
        | (static_cast<std::uint32_t>(data[base + 2U]) << 16U)
        | (static_cast<std::uint32_t>(data[base + 3U]) << 24U);
    return true;
}

[[nodiscard]] bool ReadI16Le(const std::vector<std::uint8_t>& data, std::uint64_t offset, std::int16_t& value)
{
    std::uint16_t raw = 0;
    if (!ReadU16Le(data, offset, raw)) {
        return false;
    }
    value = static_cast<std::int16_t>(raw);
    return true;
}

[[nodiscard]] bool ReadI32Le(const std::vector<std::uint8_t>& data, std::uint64_t offset, std::int32_t& value)
{
    std::uint32_t raw = 0;
    if (!ReadU32Le(data, offset, raw)) {
        return false;
    }
    value = static_cast<std::int32_t>(raw);
    return true;
}

[[nodiscard]] bool ReadU64Le(const std::vector<std::uint8_t>& data, std::uint64_t offset, std::uint64_t& value)
{
    if (offset > data.size() || data.size() - static_cast<std::size_t>(offset) < 8U) {
        return false;
    }
    const auto base = static_cast<std::size_t>(offset);
    value = 0;
    for (unsigned i = 0; i < 8; ++i) {
        value |= static_cast<std::uint64_t>(data[base + i]) << (i * 8U);
    }
    return true;
}

[[nodiscard]] std::uint32_t Crc32IsoHdlc(const std::uint8_t* bytes, std::size_t size)
{
    std::uint32_t crc = 0xFFFFFFFFU;
    for (std::size_t i = 0; i < size; ++i) {
        crc ^= bytes[i];
        for (int bit = 0; bit < 8; ++bit) {
            const std::uint32_t mask = 0U - (crc & 1U);
            crc = (crc >> 1U) ^ (0xEDB88320U & mask);
        }
    }
    return ~crc;
}

[[nodiscard]] std::uint32_t Crc32Range(const std::vector<std::uint8_t>& data, std::uint64_t offset, std::uint64_t size)
{
    return Crc32IsoHdlc(data.data() + static_cast<std::size_t>(offset), static_cast<std::size_t>(size));
}

[[nodiscard]] std::string BuildIdToHex(const std::array<std::uint8_t, 16>& bytes)
{
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (std::uint8_t byte : bytes) {
        out << std::setw(2) << static_cast<unsigned>(byte);
    }
    return out.str();
}

[[nodiscard]] bool ReadFile(const std::filesystem::path& path, std::vector<std::uint8_t>& data, std::uint64_t& file_size, std::string& error)
{
    std::error_code size_error;
    const std::uintmax_t size = std::filesystem::file_size(path, size_error);
    if (size_error) {
        error = "stat_failed";
        return false;
    }
    if (size > kMaxBinaryReadBytes) {
        error = "resource_limit_exceeded";
        return false;
    }
    file_size = static_cast<std::uint64_t>(size);
    data.resize(static_cast<std::size_t>(file_size));
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        error = "open_failed";
        return false;
    }
    if (!data.empty()) {
        file.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()));
    }
    if (!file && !data.empty()) {
        error = "read_failed";
        return false;
    }
    return true;
}

void Fail(VxmapRuntimeValidationReport& report, std::string reason)
{
    report.valid = false;
    report.fallback_reason = std::move(reason);
}

[[nodiscard]] bool ParseHeader(const std::vector<std::uint8_t>& data, Header& header)
{
    if (data.size() < kHeaderSize) {
        return false;
    }
    std::uint16_t header_size = 0;
    std::uint16_t major = 0;
    std::uint16_t minor = 0;
    std::uint16_t entry_size = 0;
    std::uint32_t endian_marker = 0;
    std::uint16_t tile_size = 0;
    std::uint16_t region_size = 0;
    if (!ReadU16Le(data, 8, header_size) || !ReadU16Le(data, 10, major) || !ReadU16Le(data, 12, minor)
        || !ReadU16Le(data, 14, entry_size) || !ReadU32Le(data, 16, endian_marker)) {
        return false;
    }
    if (std::string(reinterpret_cast<const char*>(data.data()), 8) != "VXMAPBIN") {
        return false;
    }
    if (header_size != kHeaderSize || entry_size != kSectionEntrySize || endian_marker != kEndianMarker) {
        return false;
    }
    header.format_major = major;
    header.format_minor = minor;
    if (!ReadU32Le(data, 20, header.header_flags) || !ReadU32Le(data, 24, header.section_count)
        || !ReadU32Le(data, 28, header.width_tiles) || !ReadU32Le(data, 32, header.height_tiles)
        || !ReadU16Le(data, 36, tile_size) || !ReadI16Le(data, 40, header.min_elevation)
        || !ReadI16Le(data, 42, header.max_elevation) || !ReadU16Le(data, 44, region_size)
        || !ReadU64Le(data, 56, header.section_table_offset) || !ReadU64Le(data, 64, header.section_table_size)
        || !ReadU64Le(data, 72, header.file_size) || !ReadU32Le(data, 104, header.header_crc32)
        || !ReadU32Le(data, 108, header.table_crc32)) {
        return false;
    }
    header.tile_size_px = tile_size;
    header.region_size_tiles = region_size;
    for (std::size_t i = 0; i < header.build_id.size(); ++i) {
        header.build_id[i] = data[88U + i];
    }
    return true;
}

[[nodiscard]] bool ValidateHeaderCrc(const std::vector<std::uint8_t>& data, const Header& header)
{
    std::array<std::uint8_t, kHeaderSize> copy{};
    std::copy_n(data.begin(), copy.size(), copy.begin());
    copy[104] = 0;
    copy[105] = 0;
    copy[106] = 0;
    copy[107] = 0;
    return Crc32IsoHdlc(copy.data(), copy.size()) == header.header_crc32;
}

[[nodiscard]] std::optional<SectionEntry> ParseSectionEntry(const std::vector<std::uint8_t>& data, std::uint64_t offset)
{
    SectionEntry entry;
    if (!ReadU32Le(data, offset + 0, entry.section_type) || !ReadU32Le(data, offset + 4, entry.section_flags)
        || !ReadU32Le(data, offset + 8, entry.section_id) || !ReadU32Le(data, offset + 12, entry.parent_id)
        || !ReadU64Le(data, offset + 16, entry.offset) || !ReadU64Le(data, offset + 24, entry.stored_size)
        || !ReadU64Le(data, offset + 32, entry.unpacked_size) || !ReadU32Le(data, offset + 40, entry.element_count)
        || !ReadU32Le(data, offset + 44, entry.element_stride) || !ReadU16Le(data, offset + 48, entry.codec)
        || !ReadU16Le(data, offset + 50, entry.alignment) || !ReadU32Le(data, offset + 52, entry.crc32)
        || !ReadU32Le(data, offset + 56, entry.aux_0) || !ReadU32Le(data, offset + 60, entry.aux_1)) {
        return std::nullopt;
    }
    return entry;
}

[[nodiscard]] bool ValidateTableAndSections(
    const std::vector<std::uint8_t>& data,
    const Header& header,
    std::vector<SectionEntry>& entries,
    VxmapRuntimeValidationReport& report)
{
    if (header.section_count == 0 || header.section_count > kMaxSectionCount) {
        Fail(report, "invalid_section_count");
        return false;
    }
    std::uint64_t expected_table_size = 0;
    if (!CheckedMultiply(header.section_count, kSectionEntrySize, expected_table_size)
        || expected_table_size != header.section_table_size) {
        Fail(report, "table_size_mismatch");
        return false;
    }
    std::uint64_t table_end = 0;
    if (!CheckedAdd(header.section_table_offset, header.section_table_size, table_end) || table_end > data.size()) {
        Fail(report, "section_table_out_of_bounds");
        return false;
    }
    if (Crc32Range(data, header.section_table_offset, header.section_table_size) != header.table_crc32) {
        Fail(report, "bad_table_crc");
        return false;
    }

    entries.reserve(header.section_count);
    std::set<std::uint32_t> section_ids;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> ranges;
    for (std::uint32_t i = 0; i < header.section_count; ++i) {
        const std::uint64_t entry_offset = header.section_table_offset + static_cast<std::uint64_t>(i) * kSectionEntrySize;
        std::optional<SectionEntry> parsed = ParseSectionEntry(data, entry_offset);
        if (!parsed.has_value()) {
            Fail(report, "bad_section_entry");
            return false;
        }
        SectionEntry entry = *parsed;
        if (entry.section_id == 0 || !section_ids.insert(entry.section_id).second) {
            Fail(report, "duplicate_section_id");
            return false;
        }
        if (entry.codec != kCodecNone || entry.stored_size != entry.unpacked_size) {
            Fail(report, "unsupported_codec");
            return false;
        }
        if (!IsPowerOfTwo(entry.alignment)) {
            Fail(report, "invalid_alignment");
            return false;
        }
        std::uint64_t section_end = 0;
        if (!CheckedAdd(entry.offset, entry.stored_size, section_end) || section_end > data.size()) {
            Fail(report, "section_out_of_bounds");
            return false;
        }
        if (entry.stored_size > 0) {
            ranges.emplace_back(entry.offset, section_end);
            if (Crc32Range(data, entry.offset, entry.stored_size) != entry.crc32) {
                Fail(report, "bad_section_crc");
                return false;
            }
        }
        entries.push_back(entry);
    }
    std::sort(ranges.begin(), ranges.end());
    for (std::size_t i = 1; i < ranges.size(); ++i) {
        if (ranges[i - 1].second > ranges[i].first) {
            Fail(report, "overlapping_sections");
            return false;
        }
    }
    return true;
}

[[nodiscard]] const SectionEntry* FindSingleton(const std::vector<SectionEntry>& entries, std::uint32_t section_type)
{
    const SectionEntry* found = nullptr;
    for (const SectionEntry& entry : entries) {
        if (entry.section_type != section_type) {
            continue;
        }
        if (found != nullptr) {
            return nullptr;
        }
        found = &entry;
    }
    return found;
}

[[nodiscard]] bool ValidateRequiredSections(
    const std::vector<std::uint8_t>& data,
    const Header& header,
    const std::vector<SectionEntry>& entries,
    VxmapRuntimeValidationReport& report)
{
    for (std::uint32_t type : kRequiredGlobalSections) {
        if (FindSingleton(entries, type) == nullptr) {
            Fail(report, "missing_required_section");
            return false;
        }
    }

    const SectionEntry* terrain_catalog = FindSingleton(entries, kTypeTerrainCatalog);
    if (terrain_catalog == nullptr || terrain_catalog->stored_size < 8U) {
        Fail(report, "bad_terrain_catalog");
        return false;
    }
    std::uint32_t terrain_count = 0;
    std::uint16_t terrain_record_size = 0;
    if (!ReadU32Le(data, terrain_catalog->offset, terrain_count)
        || !ReadU16Le(data, terrain_catalog->offset + 4U, terrain_record_size)) {
        Fail(report, "bad_terrain_catalog");
        return false;
    }
    if (terrain_count > kMaxTerrainCount || terrain_record_size != 32U) {
        Fail(report, "bad_terrain_catalog");
        return false;
    }
    report.terrain_count = terrain_count;

    const SectionEntry* region_index = FindSingleton(entries, kTypeRegionIndex);
    if (region_index == nullptr || region_index->stored_size < 8U) {
        Fail(report, "bad_region_index");
        return false;
    }
    std::uint32_t region_count = 0;
    std::uint16_t region_size = 0;
    std::uint16_t region_record_size = 0;
    if (!ReadU32Le(data, region_index->offset, region_count) || !ReadU16Le(data, region_index->offset + 4U, region_size)
        || !ReadU16Le(data, region_index->offset + 6U, region_record_size)) {
        Fail(report, "bad_region_index");
        return false;
    }
    if (region_size != header.region_size_tiles || region_record_size != 32U) {
        Fail(report, "bad_region_index");
        return false;
    }
    const std::uint32_t expected_regions_x = (header.width_tiles + header.region_size_tiles - 1U) / header.region_size_tiles;
    const std::uint32_t expected_regions_y = (header.height_tiles + header.region_size_tiles - 1U) / header.region_size_tiles;
    const std::uint32_t expected_regions = expected_regions_x * expected_regions_y;
    const std::uint32_t regional_section_count = RequiredRegionalSectionCount(header);
    if (region_count != expected_regions
        || header.section_count != 6U + region_count * regional_section_count) {
        Fail(report, "invalid_region_index");
        return false;
    }
    report.region_count = region_count;

    for (const SectionEntry& entry : entries) {
        if ((entry.section_flags & kSectionFlagRequired) != 0U) {
            const bool known_global = std::find(kRequiredGlobalSections.begin(), kRequiredGlobalSections.end(), entry.section_type)
                != kRequiredGlobalSections.end();
            const bool known_region = IsKnownRequiredRegionalSection(header, entry.section_type);
            if (!known_global && !known_region) {
                Fail(report, "unknown_required_section");
                return false;
            }
        }
    }
    return true;
}

[[nodiscard]] bool ManifestMatches(
    const VxmapRuntimeManifest& manifest,
    const Header& header,
    const VxmapRuntimeValidationReport& report,
    std::string& reason)
{
    if (!manifest.declared) {
        reason = "map_index_missing_runtime_binary";
        return false;
    }
    if (manifest.format != "vxmap-runtime-v1") {
        reason = "unsupported_format";
        return false;
    }
    if (manifest.format_major != static_cast<int>(header.format_major) || manifest.format_minor != static_cast<int>(header.format_minor)) {
        reason = "version_mismatch";
        return false;
    }
    if (manifest.file_size != 0 && manifest.file_size != report.file_size) {
        reason = "declared_file_size_mismatch";
        return false;
    }
    if (manifest.section_count != 0 && manifest.section_count != header.section_count) {
        reason = "declared_section_count_mismatch";
        return false;
    }
    if (!manifest.build_id_hex.empty() && manifest.build_id_hex != report.build_id_hex) {
        reason = "build_id_mismatch";
        return false;
    }
    return true;
}


[[nodiscard]] bool ValidateContainerData(
    const std::vector<std::uint8_t>& data,
    const VxmapRuntimeManifest& manifest,
    Header& header,
    std::vector<SectionEntry>& entries,
    VxmapRuntimeValidationReport& report)
{
    if (data.size() < kHeaderSize) {
        Fail(report, "truncated_header");
        return false;
    }
    if (!ParseHeader(data, header)) {
        Fail(report, "bad_header");
        return false;
    }

    report.format_major = header.format_major;
    report.format_minor = header.format_minor;
    report.section_count = header.section_count;
    report.width_tiles = header.width_tiles;
    report.height_tiles = header.height_tiles;
    report.tile_size_px = header.tile_size_px;
    report.min_elevation = header.min_elevation;
    report.max_elevation = header.max_elevation;
    report.build_id_hex = BuildIdToHex(header.build_id);

    if (header.format_major != kSupportedMajor || header.format_minor > kSupportedMinor) {
        Fail(report, "unsupported_version");
        return false;
    }
    if (header.header_flags != kExpectedHeaderFlags || (header.header_flags & ~kKnownHeaderFlags) != 0U) {
        Fail(report, "bad_header_flags");
        return false;
    }
    if (header.file_size != report.file_size) {
        Fail(report, "declared_file_size_mismatch");
        return false;
    }
    if (header.width_tiles == 0 || header.height_tiles == 0 || header.tile_size_px == 0
        || header.region_size_tiles == 0) {
        Fail(report, "invalid_dimensions");
        return false;
    }
    if (!ValidateHeaderCrc(data, header)) {
        Fail(report, "bad_header_crc");
        return false;
    }

    std::string manifest_reason;
    if (!ManifestMatches(manifest, header, report, manifest_reason)) {
        Fail(report, manifest_reason);
        return false;
    }
    if (!ValidateTableAndSections(data, header, entries, report)) {
        return false;
    }
    if (!ValidateRequiredSections(data, header, entries, report)) {
        return false;
    }

    report.valid = true;
    report.fallback_reason.clear();
    return true;
}

[[nodiscard]] ValidatedContainer LoadValidatedContainer(
    const std::filesystem::path& package_path,
    const VxmapRuntimeManifest& manifest)
{
    const auto total_start = std::chrono::steady_clock::now();
    ValidatedContainer container;
    VxmapRuntimeValidationReport& report = container.report;

    if (!manifest.declared) {
        Fail(report, "map_index_missing_runtime_binary");
        report.total_ms = ElapsedMilliseconds(total_start, std::chrono::steady_clock::now());
        return container;
    }

    report.path = package_path / manifest.relative_path;
    report.present = std::filesystem::exists(report.path);
    if (!report.present) {
        Fail(report, "binary_missing");
        report.total_ms = ElapsedMilliseconds(total_start, std::chrono::steady_clock::now());
        return container;
    }

    const auto read_start = std::chrono::steady_clock::now();
    std::string read_error;
    const bool read_ok = ReadFile(report.path, container.data, report.file_size, read_error);
    const auto read_end = std::chrono::steady_clock::now();
    report.read_ms = ElapsedMilliseconds(read_start, read_end);
    if (!read_ok) {
        Fail(report, read_error);
        report.total_ms = ElapsedMilliseconds(total_start, read_end);
        return container;
    }

    const auto validate_start = std::chrono::steady_clock::now();
    report.valid = ValidateContainerData(
        container.data,
        manifest,
        container.header,
        container.entries,
        report);
    const auto validate_end = std::chrono::steady_clock::now();
    report.validate_ms = ElapsedMilliseconds(validate_start, validate_end);
    report.total_ms = ElapsedMilliseconds(total_start, validate_end);
    return container;
}

[[nodiscard]] bool ReadStringTable(
    const std::vector<std::uint8_t>& data,
    const SectionEntry& section,
    std::vector<std::string>& strings,
    std::string& reason)
{
    if (section.stored_size < 8U) {
        reason = "bad_string_table";
        return false;
    }
    std::uint32_t string_count = 0;
    std::uint32_t bytes_size = 0;
    if (!ReadU32Le(data, section.offset, string_count) || !ReadU32Le(data, section.offset + 4U, bytes_size)) {
        reason = "bad_string_table";
        return false;
    }
    std::uint64_t offsets_bytes = 0;
    if (!CheckedMultiply(static_cast<std::uint64_t>(string_count) + 1U, 4U, offsets_bytes)) {
        reason = "bad_string_table";
        return false;
    }
    std::uint64_t header_and_offsets = 0;
    if (!CheckedAdd(8U, offsets_bytes, header_and_offsets)) {
        reason = "bad_string_table";
        return false;
    }
    std::uint64_t expected_size = 0;
    if (!CheckedAdd(header_and_offsets, bytes_size, expected_size) || expected_size != section.stored_size) {
        reason = "bad_string_table";
        return false;
    }

    std::vector<std::uint32_t> offsets;
    offsets.reserve(static_cast<std::size_t>(string_count) + 1U);
    for (std::uint32_t i = 0; i <= string_count; ++i) {
        std::uint32_t value = 0;
        if (!ReadU32Le(data, section.offset + 8U + static_cast<std::uint64_t>(i) * 4U, value)) {
            reason = "bad_string_table";
            return false;
        }
        offsets.push_back(value);
    }
    if (offsets.empty() || offsets.front() != 0U || offsets.back() != bytes_size) {
        reason = "bad_string_table";
        return false;
    }
    for (std::size_t i = 1; i < offsets.size(); ++i) {
        if (offsets[i - 1U] > offsets[i]) {
            reason = "bad_string_table";
            return false;
        }
    }

    const std::uint64_t bytes_offset = section.offset + header_and_offsets;
    strings.clear();
    strings.reserve(string_count);
    for (std::uint32_t i = 0; i < string_count; ++i) {
        const std::uint32_t begin = offsets[i];
        const std::uint32_t end = offsets[i + 1U];
        strings.emplace_back(
            reinterpret_cast<const char*>(data.data() + static_cast<std::size_t>(bytes_offset + begin)),
            static_cast<std::size_t>(end - begin));
    }
    return true;
}

[[nodiscard]] bool ReadTerrainNames(
    const std::vector<std::uint8_t>& data,
    const SectionEntry& section,
    const std::vector<std::string>& strings,
    std::vector<std::string>& terrain_names,
    std::string& reason)
{
    if (section.stored_size < 8U) {
        reason = "bad_terrain_catalog";
        return false;
    }
    std::uint32_t terrain_count = 0;
    std::uint16_t record_size = 0;
    if (!ReadU32Le(data, section.offset, terrain_count) || !ReadU16Le(data, section.offset + 4U, record_size)) {
        reason = "bad_terrain_catalog";
        return false;
    }
    std::uint64_t records_size = 0;
    if (terrain_count > kMaxTerrainCount || record_size != 32U
        || !CheckedMultiply(terrain_count, record_size, records_size)
        || 8U + records_size != section.stored_size) {
        reason = "bad_terrain_catalog";
        return false;
    }
    terrain_names.clear();
    terrain_names.reserve(terrain_count);
    for (std::uint32_t i = 0; i < terrain_count; ++i) {
        const std::uint64_t record_offset = section.offset + 8U + static_cast<std::uint64_t>(i) * record_size;
        std::uint32_t name_id = 0;
        if (!ReadU32Le(data, record_offset, name_id) || name_id >= strings.size()) {
            reason = "bad_terrain_catalog";
            return false;
        }
        terrain_names.push_back(strings[name_id]);
    }
    return true;
}

[[nodiscard]] bool ReadRegionRecords(
    const std::vector<std::uint8_t>& data,
    const Header& header,
    const SectionEntry& section,
    std::vector<RegionRecord>& regions,
    std::string& reason)
{
    if (section.stored_size < 8U) {
        reason = "bad_region_index";
        return false;
    }
    std::uint32_t region_count = 0;
    std::uint16_t region_size = 0;
    std::uint16_t record_size = 0;
    if (!ReadU32Le(data, section.offset, region_count) || !ReadU16Le(data, section.offset + 4U, region_size)
        || !ReadU16Le(data, section.offset + 6U, record_size)) {
        reason = "bad_region_index";
        return false;
    }
    if (region_size != header.region_size_tiles || record_size != 32U) {
        reason = "bad_region_index";
        return false;
    }
    std::uint64_t records_size = 0;
    if (!CheckedMultiply(region_count, record_size, records_size) || 8U + records_size != section.stored_size) {
        reason = "bad_region_index";
        return false;
    }
    const std::uint32_t regions_x = (header.width_tiles + header.region_size_tiles - 1U) / header.region_size_tiles;
    const std::uint32_t regional_section_count = RequiredRegionalSectionCount(header);
    regions.clear();
    regions.reserve(region_count);
    for (std::uint32_t i = 0; i < region_count; ++i) {
        const std::uint64_t offset = section.offset + 8U + static_cast<std::uint64_t>(i) * record_size;
        RegionRecord region;
        if (!ReadU32Le(data, offset + 0U, region.region_id) || !ReadU16Le(data, offset + 4U, region.region_x)
            || !ReadU16Le(data, offset + 6U, region.region_y) || !ReadU32Le(data, offset + 8U, region.origin_x)
            || !ReadU32Le(data, offset + 12U, region.origin_y) || !ReadU16Le(data, offset + 16U, region.width)
            || !ReadU16Le(data, offset + 18U, region.height)
            || !ReadU32Le(data, offset + 20U, region.first_section_table_index)
            || !ReadU16Le(data, offset + 24U, region.section_count) || !ReadU16Le(data, offset + 26U, region.flags)
            || !ReadU32Le(data, offset + 28U, region.tile_count)) {
            reason = "bad_region_index";
            return false;
        }
        if (region.region_id != i || region.region_x != i % regions_x || region.region_y != i / regions_x
            || region.width == 0 || region.height == 0 || region.tile_count != static_cast<std::uint32_t>(region.width) * region.height
            || region.section_count != regional_section_count
            || region.first_section_table_index != 6U + i * regional_section_count) {
            reason = "invalid_region_index";
            return false;
        }
        regions.push_back(region);
    }
    return true;
}

[[nodiscard]] const SectionEntry* FindRegionalSection(
    const std::vector<SectionEntry>& entries,
    const RegionRecord& region,
    std::uint32_t section_type)
{
    const std::uint32_t begin = region.first_section_table_index;
    const std::uint32_t end = begin + region.section_count;
    if (end > entries.size()) {
        return nullptr;
    }
    for (std::uint32_t i = begin; i < end; ++i) {
        const SectionEntry& entry = entries[i];
        if (entry.section_type == section_type && entry.parent_id == region.region_id) {
            return &entry;
        }
    }
    return nullptr;
}

[[nodiscard]] bool BitsetValue(const std::vector<std::uint8_t>& data, const SectionEntry& section, std::uint32_t local_index)
{
    const std::uint64_t byte_offset = section.offset + (local_index >> 3U);
    const std::uint8_t bit = static_cast<std::uint8_t>(1U << (local_index & 7U));
    return (data[static_cast<std::size_t>(byte_offset)] & bit) != 0U;
}

[[nodiscard]] bool DecodeBinaryCorePayloads(
    const std::vector<std::uint8_t>& data,
    const Header& header,
    const std::vector<SectionEntry>& entries,
    VxmapRuntimeCore& core)
{
    std::string reason;
    const SectionEntry* string_table = FindSingleton(entries, kTypeStringTable);
    const SectionEntry* terrain_catalog = FindSingleton(entries, kTypeTerrainCatalog);
    const SectionEntry* region_index = FindSingleton(entries, kTypeRegionIndex);
    const SectionEntry* start_goal = FindSingleton(entries, kTypeStartGoal);
    if (string_table == nullptr || terrain_catalog == nullptr || region_index == nullptr || start_goal == nullptr) {
        core.fallback_reason = "missing_required_section";
        return false;
    }

    std::vector<std::string> strings;
    std::vector<std::string> terrain_names;
    std::vector<RegionRecord> regions;
    if (!ReadStringTable(data, *string_table, strings, reason) || !ReadTerrainNames(data, *terrain_catalog, strings, terrain_names, reason)
        || !ReadRegionRecords(data, header, *region_index, regions, reason)) {
        core.fallback_reason = reason;
        return false;
    }

    const std::size_t tile_count = static_cast<std::size_t>(header.width_tiles) * static_cast<std::size_t>(header.height_tiles);
    core.terrain.assign(tile_count, std::string{});
    core.elevation.assign(tile_count, 0);
    core.collision.assign(tile_count, 0);
    core.movement_cost.assign(tile_count, 0);
    core.projectile_block.assign(tile_count, 0);
    core.vision_block.assign(tile_count, 0);
    core.cover.assign(tile_count, 0);
    core.concealment.assign(tile_count, 0);
    core.structure_height.assign(tile_count, 0);
    core.structure_height_present = header.format_minor >= 1U;
    core.vegetation_type.assign(tile_count, 0);
    core.vegetation_height.assign(tile_count, 0);
    core.vegetation_type_present = header.format_minor >= 2U;
    core.vegetation_height_present = header.format_minor >= 2U;

    for (const RegionRecord& region : regions) {
        const SectionEntry* terrain_grid = FindRegionalSection(entries, region, kTypeTerrainGrid);
        const SectionEntry* elevation_grid = FindRegionalSection(entries, region, kTypeElevationGrid);
        const SectionEntry* movement_grid = FindRegionalSection(entries, region, kTypeMovementGrid);
        const SectionEntry* collision_bits = FindRegionalSection(entries, region, kTypeCollisionBits);
        const SectionEntry* projectile_block_bits = FindRegionalSection(entries, region, kTypeProjectileBlockBits);
        const SectionEntry* vision_block_bits = FindRegionalSection(entries, region, kTypeVisionBlockBits);
        const SectionEntry* cover_grid = FindRegionalSection(entries, region, kTypeCoverGrid);
        const SectionEntry* concealment_grid = FindRegionalSection(entries, region, kTypeConcealmentGrid);
        const SectionEntry* structure_height_grid = header.format_minor >= 1U
            ? FindRegionalSection(entries, region, kTypeStructureHeightGrid)
            : nullptr;
        const SectionEntry* vegetation_type_grid = header.format_minor >= 2U
            ? FindRegionalSection(entries, region, kTypeVegetationTypeGrid)
            : nullptr;
        const SectionEntry* vegetation_height_grid = header.format_minor >= 2U
            ? FindRegionalSection(entries, region, kTypeVegetationHeightGrid)
            : nullptr;
        if (terrain_grid == nullptr || elevation_grid == nullptr || movement_grid == nullptr
            || collision_bits == nullptr || projectile_block_bits == nullptr || vision_block_bits == nullptr
            || cover_grid == nullptr || concealment_grid == nullptr
            || (header.format_minor >= 1U && structure_height_grid == nullptr)
            || (header.format_minor >= 2U
                && (vegetation_type_grid == nullptr || vegetation_height_grid == nullptr))) {
            core.fallback_reason = "missing_regional_grid";
            return false;
        }
        if (terrain_grid->element_stride != 2U || elevation_grid->element_stride != 2U
            || movement_grid->element_stride != 2U || cover_grid->element_stride != 1U
            || concealment_grid->element_stride != 1U || terrain_grid->element_count != region.tile_count
            || elevation_grid->element_count != region.tile_count || movement_grid->element_count != region.tile_count
            || collision_bits->element_count != region.tile_count
            || projectile_block_bits->element_count != region.tile_count
            || vision_block_bits->element_count != region.tile_count
            || cover_grid->element_count != region.tile_count
            || concealment_grid->element_count != region.tile_count
            || (structure_height_grid != nullptr
                && (structure_height_grid->element_stride != 1U
                    || structure_height_grid->element_count != region.tile_count))
            || (vegetation_type_grid != nullptr
                && (vegetation_type_grid->element_stride != 1U
                    || vegetation_type_grid->element_count != region.tile_count))
            || (vegetation_height_grid != nullptr
                && (vegetation_height_grid->element_stride != 1U
                    || vegetation_height_grid->element_count != region.tile_count))) {
            core.fallback_reason = "bad_regional_grid";
            return false;
        }
        const std::uint64_t expected_bitset_size = (static_cast<std::uint64_t>(region.tile_count) + 7U) / 8U;
        const std::uint64_t expected_i16_size = static_cast<std::uint64_t>(region.tile_count) * 2U;
        if (terrain_grid->stored_size != expected_i16_size || elevation_grid->stored_size != expected_i16_size
            || movement_grid->stored_size != expected_i16_size || collision_bits->stored_size != expected_bitset_size
            || projectile_block_bits->stored_size != expected_bitset_size
            || vision_block_bits->stored_size != expected_bitset_size
            || cover_grid->stored_size != region.tile_count || concealment_grid->stored_size != region.tile_count
            || (structure_height_grid != nullptr
                && structure_height_grid->stored_size != region.tile_count)
            || (vegetation_type_grid != nullptr
                && vegetation_type_grid->stored_size != region.tile_count)
            || (vegetation_height_grid != nullptr
                && vegetation_height_grid->stored_size != region.tile_count)) {
            core.fallback_reason = "bad_regional_grid_size";
            return false;
        }

        for (std::uint32_t local_y = 0; local_y < region.height; ++local_y) {
            for (std::uint32_t local_x = 0; local_x < region.width; ++local_x) {
                const std::uint32_t local_index = local_y * region.width + local_x;
                const std::uint32_t global_x = region.origin_x + local_x;
                const std::uint32_t global_y = region.origin_y + local_y;
                const std::size_t global_index = static_cast<std::size_t>(global_y) * header.width_tiles + global_x;
                std::uint16_t terrain_id = 0;
                std::int16_t elevation = 0;
                std::int16_t movement_cost = 0;
                if (!ReadU16Le(data, terrain_grid->offset + static_cast<std::uint64_t>(local_index) * 2U, terrain_id)
                    || terrain_id >= terrain_names.size()
                    || !ReadI16Le(data, elevation_grid->offset + static_cast<std::uint64_t>(local_index) * 2U, elevation)
                    || !ReadI16Le(data, movement_grid->offset + static_cast<std::uint64_t>(local_index) * 2U, movement_cost)) {
                    core.fallback_reason = "bad_regional_grid";
                    return false;
                }
                core.terrain[global_index] = terrain_names[terrain_id];
                core.elevation[global_index] = elevation;
                core.movement_cost[global_index] = movement_cost;
                core.collision[global_index] = BitsetValue(data, *collision_bits, local_index) ? 1U : 0U;
                core.projectile_block[global_index] = BitsetValue(data, *projectile_block_bits, local_index) ? 1U : 0U;
                core.vision_block[global_index] = BitsetValue(data, *vision_block_bits, local_index) ? 1U : 0U;
                core.cover[global_index] = data[static_cast<std::size_t>(cover_grid->offset + local_index)];
                core.concealment[global_index] = data[static_cast<std::size_t>(concealment_grid->offset + local_index)];
                if (structure_height_grid != nullptr) {
                    const std::uint8_t structure_height =
                        data[static_cast<std::size_t>(structure_height_grid->offset + local_index)];
                    if (structure_height > 3U) {
                        core.fallback_reason = "bad_structure_height_grid";
                        return false;
                    }
                    core.structure_height[global_index] = structure_height;
                }
                if (vegetation_type_grid != nullptr && vegetation_height_grid != nullptr) {
                    const std::uint8_t vegetation_type =
                        data[static_cast<std::size_t>(vegetation_type_grid->offset + local_index)];
                    const std::uint8_t vegetation_height =
                        data[static_cast<std::size_t>(vegetation_height_grid->offset + local_index)];
                    if (vegetation_type > 4U || vegetation_height > 5U) {
                        core.fallback_reason = "bad_vegetation_grid";
                        return false;
                    }
                    const bool valid_pair = (vegetation_type == 0U && vegetation_height == 0U)
                        || (vegetation_type == 1U && vegetation_height >= 2U && vegetation_height <= 5U)
                        || (vegetation_type == 2U && vegetation_height >= 1U && vegetation_height <= 2U)
                        || ((vegetation_type == 3U || vegetation_type == 4U) && vegetation_height == 1U);
                    if (!valid_pair) {
                        core.fallback_reason = "bad_vegetation_type_height_pair";
                        return false;
                    }
                    core.vegetation_type[global_index] = vegetation_type;
                    core.vegetation_height[global_index] = vegetation_height;
                }
            }
        }
    }

    if (start_goal->stored_size != 32U) {
        core.fallback_reason = "bad_start_goal";
        return false;
    }
    std::int32_t start_x = -1;
    std::int32_t start_y = -1;
    std::int32_t goal_x = -1;
    std::int32_t goal_y = -1;
    std::uint32_t flags = 0;
    if (!ReadI32Le(data, start_goal->offset + 0U, start_x) || !ReadI32Le(data, start_goal->offset + 4U, start_y)
        || !ReadI32Le(data, start_goal->offset + 8U, goal_x) || !ReadI32Le(data, start_goal->offset + 12U, goal_y)
        || !ReadU32Le(data, start_goal->offset + 16U, flags)) {
        core.fallback_reason = "bad_start_goal";
        return false;
    }
    if ((flags & 0x01U) != 0U) {
        if (start_x < 0 || start_y < 0 || static_cast<std::uint32_t>(start_x) >= header.width_tiles
            || static_cast<std::uint32_t>(start_y) >= header.height_tiles) {
            core.fallback_reason = "bad_start_goal";
            return false;
        }
        core.start = TileCoord{start_x, start_y};
    }
    if ((flags & 0x02U) != 0U) {
        if (goal_x < 0 || goal_y < 0 || static_cast<std::uint32_t>(goal_x) >= header.width_tiles
            || static_cast<std::uint32_t>(goal_y) >= header.height_tiles) {
            core.fallback_reason = "bad_start_goal";
            return false;
        }
        core.goal = TileCoord{goal_x, goal_y};
    }
    return true;
}

}  // namespace

VxmapRuntimeValidationReport ValidateVxmapRuntimeBinary(
    const std::filesystem::path& package_path,
    const VxmapRuntimeManifest& manifest)
{
    ValidatedContainer container = LoadValidatedContainer(package_path, manifest);
    return std::move(container.report);
}

VxmapRuntimeCore LoadVxmapRuntimeCore(
    const std::filesystem::path& package_path,
    const VxmapRuntimeManifest& manifest)
{
    const auto total_start = std::chrono::steady_clock::now();
    ValidatedContainer container = LoadValidatedContainer(package_path, manifest);

    VxmapRuntimeCore core;
    core.validation = std::move(container.report);
    core.path = core.validation.path;
    core.fallback_reason = core.validation.fallback_reason;
    if (!core.validation.valid) {
        core.total_ms = ElapsedMilliseconds(total_start, std::chrono::steady_clock::now());
        return core;
    }

    const Header& header = container.header;
    core.width_tiles = header.width_tiles;
    core.height_tiles = header.height_tiles;
    core.tile_size_px = header.tile_size_px;
    core.min_elevation = header.min_elevation;
    core.max_elevation = header.max_elevation;
    core.build_id_hex = BuildIdToHex(header.build_id);

    const auto decode_start = std::chrono::steady_clock::now();
    const bool decoded = DecodeBinaryCorePayloads(
        container.data,
        header,
        container.entries,
        core);
    const auto decode_end = std::chrono::steady_clock::now();
    core.decode_ms = ElapsedMilliseconds(decode_start, decode_end);
    core.total_ms = ElapsedMilliseconds(total_start, decode_end);
    if (!decoded) {
        return core;
    }

    core.loaded = true;
    core.fallback_reason.clear();
    return core;
}


std::string ToLogString(const VxmapRuntimeValidationReport& report)
{
    std::ostringstream out;
    out << "source=" << (report.valid ? "binary" : "json");
    if (!report.path.empty()) {
        out << " path=\"" << report.path.string() << "\"";
    }
    if (!report.valid && !report.fallback_reason.empty()) {
        out << " reason=" << report.fallback_reason;
    }
    if (report.present) {
        out << " size=" << report.file_size;
    }
    if (report.width_tiles > 0 && report.height_tiles > 0) {
        out << " map=" << report.width_tiles << 'x' << report.height_tiles;
    }
    if (report.section_count > 0) {
        out << " sections=" << report.section_count;
    }
    if (report.region_count > 0) {
        out << " regions=" << report.region_count;
    }
    if (report.terrain_count > 0) {
        out << " terrain_types=" << report.terrain_count;
    }
    if (!report.build_id_hex.empty()) {
        out << " build_id=" << report.build_id_hex;
    }
    out << " read_ms=" << report.read_ms;
    out << " validate_ms=" << report.validate_ms;
    out << " total_ms=" << report.total_ms;
    return out.str();
}

}  // namespace vox3d
