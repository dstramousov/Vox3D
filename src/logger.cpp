#include "logger.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>
#include <utility>

#if defined(_WIN32)
#include <process.h>
#include <io.h>
#else
#include <unistd.h>
#endif

namespace vox3d {
namespace {

[[nodiscard]] int CurrentProcessId()
{
#if defined(_WIN32)
    return _getpid();
#else
    return static_cast<int>(getpid());
#endif
}

[[nodiscard]] bool StdoutIsTty()
{
#if defined(_WIN32)
    return _isatty(_fileno(stdout)) != 0;
#else
    return isatty(fileno(stdout)) != 0;
#endif
}

[[nodiscard]] std::string Lowercase(std::string_view value)
{
    std::string result(value);
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return result;
}

[[nodiscard]] std::string CurrentTimestamp()
{
    using clock = std::chrono::system_clock;
    const auto now = clock::now();
    const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    const std::time_t time_now = clock::to_time_t(now);

    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &time_now);
#else
    localtime_r(&time_now, &tm);
#endif

    std::ostringstream out;
    out << std::put_time(&tm, "%H:%M:%S") << '.' << std::setw(3) << std::setfill('0') << millis.count();
    return out.str();
}

[[nodiscard]] const char* ColorForLevel(LogLevel level)
{
    switch (level) {
        case LogLevel::kTrace:
            return "\033[90m";
        case LogLevel::kDebug:
            return "\033[37m";
        case LogLevel::kInfo:
            return "\033[0m";
        case LogLevel::kWarn:
            return "\033[33m";
        case LogLevel::kError:
            return "\033[31m";
        case LogLevel::kFatal:
            return "\033[1;31m";
    }
    return "\033[0m";
}

}  // namespace

Logger::Logger(LoggerConfig config, std::ostream& output)
    : config_(std::move(config)), output_(output)
{
}

void Logger::Log(LogLevel level, std::string_view module, std::string_view message)
{
    if (!IsEnabled(level)) {
        return;
    }

    std::ostringstream tid;
    tid << std::this_thread::get_id();

    std::lock_guard<std::mutex> lock(mutex_);
    if (config_.color_enabled) {
        output_ << ColorForLevel(level);
    }

    output_ << CurrentTimestamp() << ' ';
    output_ << '[' << ToString(level) << "] ";
    output_ << "[pid=" << CurrentProcessId() << " tid=" << tid.str() << ' ' << config_.thread_name << "] ";
    output_ << module << ": " << message;

    if (config_.color_enabled) {
        output_ << "\033[0m";
    }
    output_ << '\n';
}

bool Logger::IsEnabled(LogLevel level) const
{
    return static_cast<int>(level) >= static_cast<int>(config_.minimum_level);
}

void Logger::SetThreadName(std::string thread_name)
{
    std::lock_guard<std::mutex> lock(mutex_);
    config_.thread_name = std::move(thread_name);
}

void Logger::Trace(std::string_view module, std::string_view message)
{
    Log(LogLevel::kTrace, module, message);
}

void Logger::Debug(std::string_view module, std::string_view message)
{
    Log(LogLevel::kDebug, module, message);
}

void Logger::Info(std::string_view module, std::string_view message)
{
    Log(LogLevel::kInfo, module, message);
}

void Logger::Warn(std::string_view module, std::string_view message)
{
    Log(LogLevel::kWarn, module, message);
}

void Logger::Error(std::string_view module, std::string_view message)
{
    Log(LogLevel::kError, module, message);
}

void Logger::Fatal(std::string_view module, std::string_view message)
{
    Log(LogLevel::kFatal, module, message);
}

std::string_view ToString(LogLevel level)
{
    switch (level) {
        case LogLevel::kTrace:
            return "TRACE";
        case LogLevel::kDebug:
            return "DEBUG";
        case LogLevel::kInfo:
            return "INFO";
        case LogLevel::kWarn:
            return "WARN";
        case LogLevel::kError:
            return "ERROR";
        case LogLevel::kFatal:
            return "FATAL";
    }
    return "UNKNOWN";
}

bool ParseLogLevel(std::string_view value, LogLevel& out_level)
{
    const std::string normalized = Lowercase(value);
    if (normalized == "trace") {
        out_level = LogLevel::kTrace;
        return true;
    }
    if (normalized == "debug") {
        out_level = LogLevel::kDebug;
        return true;
    }
    if (normalized == "info") {
        out_level = LogLevel::kInfo;
        return true;
    }
    if (normalized == "warn" || normalized == "warning") {
        out_level = LogLevel::kWarn;
        return true;
    }
    if (normalized == "error") {
        out_level = LogLevel::kError;
        return true;
    }
    if (normalized == "fatal") {
        out_level = LogLevel::kFatal;
        return true;
    }
    return false;
}

bool ShouldUseColor(bool forced_no_color)
{
    if (forced_no_color) {
        return false;
    }
    if (std::getenv("NO_COLOR") != nullptr) {
        return false;
    }
    return StdoutIsTty();
}

}  // namespace vox3d
