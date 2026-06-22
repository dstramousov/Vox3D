#pragma once

#include <mutex>
#include <ostream>
#include <string>
#include <string_view>

namespace vox3d {

/**
 * @brief Severity level used by the application logger.
 */
enum class LogLevel {
    kTrace,
    kDebug,
    kInfo,
    kWarn,
    kError,
    kFatal,
};

/**
 * @brief Runtime logger configuration.
 */
struct LoggerConfig {
    LogLevel minimum_level = LogLevel::kInfo;
    bool color_enabled = true;
    std::string thread_name = "main";
};

/**
 * @brief Thread-safe terminal logger with human-readable messages.
 */
class Logger {
public:
    /**
     * @brief Creates a logger that writes to the provided output stream.
     *
     * @param config Logger runtime configuration.
     * @param output Destination stream. The stream must outlive the logger.
     */
    explicit Logger(LoggerConfig config, std::ostream& output);

    /**
     * @brief Writes a log message if the level is enabled.
     *
     * @param level Message severity.
     * @param module Logical module or subsystem name.
     * @param message Human-readable message.
     */
    void Log(LogLevel level, std::string_view module, std::string_view message);

    /**
     * @brief Returns true when messages with the given level are currently enabled.
     *
     * @param level Message severity to test.
     * @return True if the message should be written.
     */
    [[nodiscard]] bool IsEnabled(LogLevel level) const;

    /**
     * @brief Updates the current thread role used in subsequent log messages.
     *
     * @param thread_name Human-readable thread role.
     */
    void SetThreadName(std::string thread_name);

    /**
     * @brief Logs a TRACE message.
     */
    void Trace(std::string_view module, std::string_view message);

    /**
     * @brief Logs a DEBUG message.
     */
    void Debug(std::string_view module, std::string_view message);

    /**
     * @brief Logs an INFO message.
     */
    void Info(std::string_view module, std::string_view message);

    /**
     * @brief Logs a WARN message.
     */
    void Warn(std::string_view module, std::string_view message);

    /**
     * @brief Logs an ERROR message.
     */
    void Error(std::string_view module, std::string_view message);

    /**
     * @brief Logs a FATAL message.
     */
    void Fatal(std::string_view module, std::string_view message);

private:
    LoggerConfig config_;
    std::ostream& output_;
    mutable std::mutex mutex_;
};

/**
 * @brief Converts a log level to a stable uppercase name.
 *
 * @param level Log level value.
 * @return String representation of the level.
 */
[[nodiscard]] std::string_view ToString(LogLevel level);

/**
 * @brief Parses a command-line log level value.
 *
 * @param value User-provided level name.
 * @param out_level Parsed log level on success.
 * @return True if parsing succeeded.
 */
[[nodiscard]] bool ParseLogLevel(std::string_view value, LogLevel& out_level);

/**
 * @brief Returns whether color output should be enabled for stdout.
 *
 * @param forced_no_color True when the user explicitly disabled colors.
 * @return True when ANSI colors should be emitted.
 */
[[nodiscard]] bool ShouldUseColor(bool forced_no_color);

}  // namespace vox3d
