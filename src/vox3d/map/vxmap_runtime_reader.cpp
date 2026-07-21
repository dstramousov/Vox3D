#include "vox3d/map/vxmap_runtime_reader.hpp"

#include <algorithm>
#include <array>
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
constexpr std::uint32_t kSupportedMinor = 0;
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
constexpr std::uint32_t kTypeStartGoal = 30;

constexpr std::array<std::uint32_t, 6> kRequiredGlobalSections{
    kTypeMetadata,
    kTypeStringTable,
    kTypeStringIdPool,
    kTypeRegionIndex,
    kTypeTerrainCatalog,
    kTypeStartGoal,
};

constexpr std::array<std::uint32_t, 8> kRequiredRegionSections{
    20,
    21,
    22,
    23,
    24,
    25,
    26,
    27,
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
    if (region_count != expected_regions || header.section_count != 6U + region_count * 8U) {
        Fail(report, "invalid_region_index");
        return false;
    }
    report.region_count = region_count;

    for (const SectionEntry& entry : entries) {
        if ((entry.section_flags & kSectionFlagRequired) != 0U) {
            const bool known_global = std::find(kRequiredGlobalSections.begin(), kRequiredGlobalSections.end(), entry.section_type)
                != kRequiredGlobalSections.end();
            const bool known_region = std::find(kRequiredRegionSections.begin(), kRequiredRegionSections.end(), entry.section_type)
                != kRequiredRegionSections.end();
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

}  // namespace

VxmapRuntimeValidationReport ValidateVxmapRuntimeBinary(
    const std::filesystem::path& package_path,
    const VxmapRuntimeManifest& manifest)
{
    VxmapRuntimeValidationReport report;
    if (!manifest.declared) {
        Fail(report, "map_index_missing_runtime_binary");
        return report;
    }

    report.path = package_path / manifest.relative_path;
    report.present = std::filesystem::exists(report.path);
    if (!report.present) {
        Fail(report, "binary_missing");
        return report;
    }

    std::vector<std::uint8_t> data;
    std::string read_error;
    if (!ReadFile(report.path, data, report.file_size, read_error)) {
        Fail(report, read_error);
        return report;
    }
    if (data.size() < kHeaderSize) {
        Fail(report, "truncated_header");
        return report;
    }

    Header header;
    if (!ParseHeader(data, header)) {
        Fail(report, "bad_header");
        return report;
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
        return report;
    }
    if (header.header_flags != kExpectedHeaderFlags || (header.header_flags & ~kKnownHeaderFlags) != 0U) {
        Fail(report, "bad_header_flags");
        return report;
    }
    if (header.file_size != report.file_size) {
        Fail(report, "declared_file_size_mismatch");
        return report;
    }
    if (header.width_tiles == 0 || header.height_tiles == 0 || header.tile_size_px == 0 || header.region_size_tiles == 0) {
        Fail(report, "invalid_dimensions");
        return report;
    }
    if (!ValidateHeaderCrc(data, header)) {
        Fail(report, "bad_header_crc");
        return report;
    }

    std::string manifest_reason;
    if (!ManifestMatches(manifest, header, report, manifest_reason)) {
        Fail(report, manifest_reason);
        return report;
    }

    std::vector<SectionEntry> entries;
    if (!ValidateTableAndSections(data, header, entries, report)) {
        return report;
    }
    if (!ValidateRequiredSections(data, header, entries, report)) {
        return report;
    }

    report.valid = true;
    report.fallback_reason.clear();
    return report;
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
    return out.str();
}

}  // namespace vox3d
