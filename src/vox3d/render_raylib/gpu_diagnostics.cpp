#include "vox3d/render_raylib/gpu_diagnostics.hpp"

#include <raylib.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <cstring>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace vox3d {
namespace {

using GlBoolean = unsigned char;
using GlEnum = unsigned int;
using GlInt = int;
using GlSizei = int;
using GlUint = unsigned int;
using GlUint64 = std::uint64_t;
using GlUbyte = unsigned char;

constexpr GlEnum kGlVendor = 0x1F00;
constexpr GlEnum kGlRenderer = 0x1F01;
constexpr GlEnum kGlVersion = 0x1F02;
constexpr GlEnum kGlExtensions = 0x1F03;
constexpr GlEnum kGlShadingLanguageVersion = 0x8B8C;
constexpr GlEnum kGlNumExtensions = 0x821D;
constexpr GlEnum kGlTimeElapsed = 0x88BF;
constexpr GlEnum kGlQueryResult = 0x8866;
constexpr GlEnum kGlQueryResultAvailable = 0x8867;

constexpr GlEnum kGlGpuMemoryInfoDedicatedVidmemNvx = 0x9047;
constexpr GlEnum kGlGpuMemoryInfoTotalAvailableMemoryNvx = 0x9048;
constexpr GlEnum kGlGpuMemoryInfoCurrentAvailableVidmemNvx = 0x9049;
constexpr GlEnum kGlGpuMemoryInfoEvictionCountNvx = 0x904A;
constexpr GlEnum kGlGpuMemoryInfoEvictedMemoryNvx = 0x904B;
constexpr GlEnum kGlVboFreeMemoryAti = 0x87FB;

constexpr double kDisplayIntervalSeconds = 1.0;
constexpr double kDriverSampleIntervalSeconds = 1.0;
constexpr std::uint64_t kBytesPerKiB = 1024ULL;

using GlGetStringFn = const GlUbyte* (*)(GlEnum name);
using GlGetStringiFn = const GlUbyte* (*)(GlEnum name, GlUint index);
using GlGetIntegervFn = void (*)(GlEnum name, GlInt* values);
using GlGenQueriesFn = void (*)(GlSizei count, GlUint* queries);
using GlDeleteQueriesFn = void (*)(GlSizei count, const GlUint* queries);
using GlBeginQueryFn = void (*)(GlEnum target, GlUint query);
using GlEndQueryFn = void (*)(GlEnum target);
using GlGetQueryObjectivFn = void (*)(GlUint query, GlEnum name, GlInt* value);
using GlGetQueryObjectuivFn = void (*)(GlUint query, GlEnum name, GlUint* value);
using GlGetQueryObjectui64vFn = void (*)(GlUint query, GlEnum name, GlUint64* value);

[[nodiscard]] std::string GlString(GlGetStringFn get_string, GlEnum name)
{
    if (get_string == nullptr) {
        return "unavailable";
    }
    const GlUbyte* value = get_string(name);
    return value != nullptr ? reinterpret_cast<const char*>(value) : "unavailable";
}

[[nodiscard]] bool ContainsExtension(const std::vector<std::string>& extensions, std::string_view name)
{
    return std::find(extensions.begin(), extensions.end(), name) != extensions.end();
}

[[nodiscard]] std::vector<std::string> SplitExtensions(std::string_view text)
{
    std::vector<std::string> result;
    std::size_t begin = 0;
    while (begin < text.size()) {
        while (begin < text.size() && text[begin] == ' ') {
            ++begin;
        }
        if (begin >= text.size()) {
            break;
        }
        const std::size_t end = text.find(' ', begin);
        result.emplace_back(text.substr(begin, end == std::string_view::npos ? text.size() - begin : end - begin));
        if (end == std::string_view::npos) {
            break;
        }
        begin = end + 1;
    }
    return result;
}

[[nodiscard]] std::uint64_t NonNegativeKiBToBytes(GlInt value)
{
    return value > 0 ? static_cast<std::uint64_t>(value) * kBytesPerKiB : 0ULL;
}

}  // namespace

class GpuDiagnostics::Impl {
public:
    Impl() = default;
    ~Impl()
    {
        CloseLibrary();
    }

    void OpenLibrary()
    {
#if defined(_WIN32)
        library_ = LoadLibraryA("opengl32.dll");
        if (library_ != nullptr) {
            wgl_get_proc_address_ = reinterpret_cast<WglGetProcAddressFn>(
                GetProcAddress(static_cast<HMODULE>(library_), "wglGetProcAddress"));
        }
#elif defined(__APPLE__)
        library_ = dlopen("/System/Library/Frameworks/OpenGL.framework/OpenGL", RTLD_LAZY | RTLD_LOCAL);
#else
        library_ = dlopen("libGL.so.1", RTLD_LAZY | RTLD_LOCAL);
        if (library_ == nullptr) {
            library_ = dlopen("libOpenGL.so.0", RTLD_LAZY | RTLD_LOCAL);
        }
        if (library_ != nullptr) {
            glx_get_proc_address_ = reinterpret_cast<GlxGetProcAddressFn>(dlsym(library_, "glXGetProcAddressARB"));
            if (glx_get_proc_address_ == nullptr) {
                glx_get_proc_address_ = reinterpret_cast<GlxGetProcAddressFn>(dlsym(library_, "glXGetProcAddress"));
            }
        }
#endif
    }

    void CloseLibrary()
    {
#if defined(_WIN32)
        if (library_ != nullptr) {
            FreeLibrary(static_cast<HMODULE>(library_));
            library_ = nullptr;
        }
#else
        if (library_ != nullptr) {
            dlclose(library_);
            library_ = nullptr;
        }
#endif
    }

    template <typename Function>
    [[nodiscard]] Function Load(const char* name) const
    {
        return reinterpret_cast<Function>(LoadRaw(name));
    }

    [[nodiscard]] void* LoadRaw(const char* name) const
    {
        if (name == nullptr) {
            return nullptr;
        }
#if defined(_WIN32)
        void* result = nullptr;
        if (wgl_get_proc_address_ != nullptr) {
            result = reinterpret_cast<void*>(wgl_get_proc_address_(name));
            const auto raw = reinterpret_cast<std::uintptr_t>(result);
            if (raw <= 3U || raw == static_cast<std::uintptr_t>(-1)) {
                result = nullptr;
            }
        }
        if (result == nullptr && library_ != nullptr) {
            result = reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(library_), name));
        }
        return result;
#else
        void* result = dlsym(RTLD_DEFAULT, name);
        if (result == nullptr && library_ != nullptr) {
            result = dlsym(library_, name);
        }
#if !defined(__APPLE__)
        if (result == nullptr && glx_get_proc_address_ != nullptr) {
            result = glx_get_proc_address_(reinterpret_cast<const unsigned char*>(name));
        }
#endif
        return result;
#endif
    }

    GlGetStringFn get_string = nullptr;
    GlGetStringiFn get_string_i = nullptr;
    GlGetIntegervFn get_integer_v = nullptr;
    GlGenQueriesFn gen_queries = nullptr;
    GlDeleteQueriesFn delete_queries = nullptr;
    GlBeginQueryFn begin_query = nullptr;
    GlEndQueryFn end_query = nullptr;
    GlGetQueryObjectivFn get_query_object_iv = nullptr;
    GlGetQueryObjectuivFn get_query_object_uiv = nullptr;
    GlGetQueryObjectui64vFn get_query_object_ui64v = nullptr;
    bool has_nvx_memory = false;
    bool has_ati_memory = false;

private:
#if defined(_WIN32)
    using WglGetProcAddressFn = PROC(WINAPI*)(LPCSTR name);
    void* library_ = nullptr;
    WglGetProcAddressFn wgl_get_proc_address_ = nullptr;
#else
#if !defined(__APPLE__)
    using GlxGetProcAddressFn = void* (*)(const unsigned char* name);
    GlxGetProcAddressFn glx_get_proc_address_ = nullptr;
#endif
    void* library_ = nullptr;
#endif
};

GpuDiagnostics::~GpuDiagnostics()
{
    delete impl_;
    impl_ = nullptr;
}

void GpuDiagnostics::Initialize()
{
    Shutdown();
    ResetState();

    impl_ = new Impl();
    impl_->OpenLibrary();
    impl_->get_string = impl_->Load<GlGetStringFn>("glGetString");
    impl_->get_string_i = impl_->Load<GlGetStringiFn>("glGetStringi");
    impl_->get_integer_v = impl_->Load<GlGetIntegervFn>("glGetIntegerv");
    impl_->gen_queries = impl_->Load<GlGenQueriesFn>("glGenQueries");
    impl_->delete_queries = impl_->Load<GlDeleteQueriesFn>("glDeleteQueries");
    impl_->begin_query = impl_->Load<GlBeginQueryFn>("glBeginQuery");
    impl_->end_query = impl_->Load<GlEndQueryFn>("glEndQuery");
    impl_->get_query_object_iv = impl_->Load<GlGetQueryObjectivFn>("glGetQueryObjectiv");
    impl_->get_query_object_uiv = impl_->Load<GlGetQueryObjectuivFn>("glGetQueryObjectuiv");
    impl_->get_query_object_ui64v = impl_->Load<GlGetQueryObjectui64vFn>("glGetQueryObjectui64v");
    if (impl_->get_query_object_ui64v == nullptr) {
        impl_->get_query_object_ui64v = impl_->Load<GlGetQueryObjectui64vFn>("glGetQueryObjectui64vEXT");
    }

    snapshot_.vendor = GlString(impl_->get_string, kGlVendor);
    snapshot_.renderer = GlString(impl_->get_string, kGlRenderer);
    snapshot_.opengl_version = GlString(impl_->get_string, kGlVersion);
    snapshot_.glsl_version = GlString(impl_->get_string, kGlShadingLanguageVersion);

    std::vector<std::string> extensions;
    if (impl_->get_integer_v != nullptr && impl_->get_string_i != nullptr) {
        GlInt count = 0;
        impl_->get_integer_v(kGlNumExtensions, &count);
        if (count > 0 && count < 65536) {
            extensions.reserve(static_cast<std::size_t>(count));
            for (GlInt index = 0; index < count; ++index) {
                const GlUbyte* extension = impl_->get_string_i(kGlExtensions, static_cast<GlUint>(index));
                if (extension != nullptr) {
                    extensions.emplace_back(reinterpret_cast<const char*>(extension));
                }
            }
        }
    }
    if (extensions.empty() && impl_->get_string != nullptr) {
        const GlUbyte* extension_text = impl_->get_string(kGlExtensions);
        if (extension_text != nullptr) {
            extensions = SplitExtensions(reinterpret_cast<const char*>(extension_text));
        }
    }

    impl_->has_nvx_memory = ContainsExtension(extensions, "GL_NVX_gpu_memory_info");
    impl_->has_ati_memory = ContainsExtension(extensions, "GL_ATI_meminfo");
    if (impl_->has_nvx_memory) {
        snapshot_.driver_memory.source = "NVX_gpu_memory_info";
    } else if (impl_->has_ati_memory) {
        snapshot_.driver_memory.source = "ATI_meminfo";
    }

    const bool timer_functions = impl_->gen_queries != nullptr
        && impl_->delete_queries != nullptr
        && impl_->begin_query != nullptr
        && impl_->end_query != nullptr
        && impl_->get_query_object_iv != nullptr
        && (impl_->get_query_object_ui64v != nullptr || impl_->get_query_object_uiv != nullptr);
    const bool timer_extension = ContainsExtension(extensions, "GL_ARB_timer_query")
        || ContainsExtension(extensions, "GL_EXT_timer_query")
        || snapshot_.opengl_version.find("3.3") != std::string::npos
        || snapshot_.opengl_version.find("4.") != std::string::npos;
    snapshot_.timer_query_supported = timer_functions && timer_extension;

    if (snapshot_.timer_query_supported) {
        std::array<GlUint, 4> query_ids{};
        impl_->gen_queries(static_cast<GlSizei>(query_ids.size()), query_ids.data());
        for (std::size_t index = 0; index < queries_.size(); ++index) {
            queries_[index].id = query_ids[index];
        }
        snapshot_.timer_query_supported = std::all_of(
            queries_.begin(),
            queries_.end(),
            [](const QuerySlot& query) { return query.id != 0; });
    }

    snapshot_.initialized = impl_->get_string != nullptr;
    sample_window_start_seconds_ = GetTime();
    driver_sample_time_seconds_ = sample_window_start_seconds_ - kDriverSampleIntervalSeconds;
    SampleDriverMemory();
}

void GpuDiagnostics::Shutdown()
{
    if (impl_ != nullptr && impl_->delete_queries != nullptr) {
        std::array<GlUint, 4> ids{};
        GlSizei count = 0;
        for (const QuerySlot& query : queries_) {
            if (query.id != 0) {
                ids[static_cast<std::size_t>(count++)] = query.id;
            }
        }
        if (count > 0) {
            impl_->delete_queries(count, ids.data());
        }
    }
    delete impl_;
    impl_ = nullptr;
    queries_ = {};
    query_active_ = false;
}

void GpuDiagnostics::BeginApplicationFrame(float cpu_frame_seconds)
{
    if (!snapshot_.initialized) {
        return;
    }
    snapshot_.render_active = false;
    if (cpu_frame_seconds > 0.0F && cpu_frame_seconds < 10.0F) {
        cpu_sample_sum_ms_ += static_cast<double>(cpu_frame_seconds) * 1000.0;
        ++cpu_sample_count_;
    }
    PollTimerQueries();
    UpdateDisplayAverages();
    SampleDriverMemory();
}

void GpuDiagnostics::SetRenderActive(bool active)
{
    snapshot_.render_active = active;
}

void GpuDiagnostics::BeginWorldRender()
{
    if (!snapshot_.timer_query_supported || impl_ == nullptr || query_active_) {
        return;
    }
    QuerySlot& slot = queries_[next_query_];
    if (slot.id == 0 || slot.pending) {
        return;
    }
    active_query_ = next_query_;
    impl_->begin_query(kGlTimeElapsed, slot.id);
    query_active_ = true;
}

void GpuDiagnostics::EndWorldRender()
{
    if (!query_active_ || impl_ == nullptr || impl_->end_query == nullptr) {
        return;
    }
    impl_->end_query(kGlTimeElapsed);
    queries_[active_query_].pending = true;
    next_query_ = (active_query_ + 1U) % queries_.size();
    query_active_ = false;
}

const GpuDiagnosticsSnapshot& GpuDiagnostics::Snapshot() const
{
    return snapshot_;
}

void GpuDiagnostics::PollTimerQueries()
{
    if (!snapshot_.timer_query_supported || impl_ == nullptr) {
        return;
    }
    for (QuerySlot& slot : queries_) {
        if (!slot.pending || slot.id == 0) {
            continue;
        }
        GlInt available = 0;
        impl_->get_query_object_iv(slot.id, kGlQueryResultAvailable, &available);
        if (available == 0) {
            continue;
        }

        GlUint64 elapsed_nanoseconds = 0;
        if (impl_->get_query_object_ui64v != nullptr) {
            impl_->get_query_object_ui64v(slot.id, kGlQueryResult, &elapsed_nanoseconds);
        } else if (impl_->get_query_object_uiv != nullptr) {
            GlUint elapsed_32 = 0;
            impl_->get_query_object_uiv(slot.id, kGlQueryResult, &elapsed_32);
            elapsed_nanoseconds = elapsed_32;
        }
        slot.pending = false;
        RecordGpuSample(static_cast<double>(elapsed_nanoseconds) / 1'000'000.0);
    }
}

void GpuDiagnostics::UpdateDisplayAverages()
{
    const double now = GetTime();
    if (now - sample_window_start_seconds_ < kDisplayIntervalSeconds) {
        return;
    }
    if (cpu_sample_count_ > 0) {
        snapshot_.cpu_frame_average_ms = cpu_sample_sum_ms_ / static_cast<double>(cpu_sample_count_);
    }
    if (gpu_sample_count_ > 0) {
        snapshot_.gpu_frame_average_ms = gpu_sample_sum_ms_ / static_cast<double>(gpu_sample_count_);
        snapshot_.gpu_sample_available = true;
    }
    cpu_sample_sum_ms_ = 0.0;
    cpu_sample_count_ = 0;
    gpu_sample_sum_ms_ = 0.0;
    gpu_sample_count_ = 0;
    sample_window_start_seconds_ = now;
}

void GpuDiagnostics::SampleDriverMemory()
{
    if (impl_ == nullptr || impl_->get_integer_v == nullptr) {
        return;
    }
    const double now = GetTime();
    if (now - driver_sample_time_seconds_ < kDriverSampleIntervalSeconds) {
        return;
    }
    driver_sample_time_seconds_ = now;

    GpuDriverMemoryStats result;
    if (impl_->has_nvx_memory) {
        result.source = "NVX_gpu_memory_info";
        GlInt dedicated_kib = 0;
        GlInt total_available_kib = 0;
        GlInt current_available_kib = 0;
        GlInt eviction_count = 0;
        GlInt evicted_kib = 0;
        impl_->get_integer_v(kGlGpuMemoryInfoDedicatedVidmemNvx, &dedicated_kib);
        impl_->get_integer_v(kGlGpuMemoryInfoTotalAvailableMemoryNvx, &total_available_kib);
        impl_->get_integer_v(kGlGpuMemoryInfoCurrentAvailableVidmemNvx, &current_available_kib);
        impl_->get_integer_v(kGlGpuMemoryInfoEvictionCountNvx, &eviction_count);
        impl_->get_integer_v(kGlGpuMemoryInfoEvictedMemoryNvx, &evicted_kib);

        result.total_bytes = NonNegativeKiBToBytes(dedicated_kib > 0 ? dedicated_kib : total_available_kib);
        result.free_bytes = NonNegativeKiBToBytes(current_available_kib);
        result.total_available = result.total_bytes > 0;
        result.free_available = current_available_kib >= 0;
        if (total_available_kib >= current_available_kib && current_available_kib >= 0) {
            result.used_bytes = static_cast<std::uint64_t>(total_available_kib - current_available_kib) * kBytesPerKiB;
            result.used_available = true;
        }
        result.eviction_count = eviction_count > 0 ? static_cast<std::uint64_t>(eviction_count) : 0ULL;
        result.evicted_bytes = NonNegativeKiBToBytes(evicted_kib);
        result.eviction_available = true;
    } else if (impl_->has_ati_memory) {
        result.source = "ATI_meminfo";
        std::array<GlInt, 4> values{};
        impl_->get_integer_v(kGlVboFreeMemoryAti, values.data());
        result.free_bytes = NonNegativeKiBToBytes(values[0]);
        result.free_available = values[0] >= 0;
    }
    snapshot_.driver_memory = std::move(result);
}

void GpuDiagnostics::RecordGpuSample(double milliseconds)
{
    if (!std::isfinite(milliseconds) || milliseconds < 0.0 || milliseconds > 10000.0) {
        return;
    }
    snapshot_.gpu_frame_last_ms = milliseconds;
    snapshot_.gpu_frame_peak_ms = std::max(snapshot_.gpu_frame_peak_ms, milliseconds);
    gpu_sample_sum_ms_ += milliseconds;
    ++gpu_sample_count_;
}

void GpuDiagnostics::ResetState()
{
    queries_ = {};
    next_query_ = 0;
    active_query_ = 0;
    query_active_ = false;
    sample_window_start_seconds_ = 0.0;
    driver_sample_time_seconds_ = 0.0;
    cpu_sample_sum_ms_ = 0.0;
    cpu_sample_count_ = 0;
    gpu_sample_sum_ms_ = 0.0;
    gpu_sample_count_ = 0;
    snapshot_ = GpuDiagnosticsSnapshot{};
}

}  // namespace vox3d
