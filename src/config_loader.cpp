#include "config_loader.hpp"

#include <cctype>
#include <charconv>
#include <fstream>
#include <map>
#include <optional>
#include <sstream>
#include <string_view>
#include <utility>
#include <variant>

namespace vox3d {
namespace {

struct JsonValue {
    using Object = std::map<std::string, JsonValue>;

    std::variant<std::nullptr_t, bool, double, std::string, Object> value = nullptr;
};

class JsonParser {
public:
    explicit JsonParser(std::string_view input)
        : input_(input)
    {
    }

    [[nodiscard]] std::optional<JsonValue> Parse(std::string& error)
    {
        SkipWhitespace();
        auto value = ParseValue(error);
        if (!value.has_value()) {
            return std::nullopt;
        }
        SkipWhitespace();
        if (!IsEnd()) {
            error = "unexpected trailing characters";
            return std::nullopt;
        }
        return value;
    }

private:
    [[nodiscard]] bool IsEnd() const
    {
        return position_ >= input_.size();
    }

    [[nodiscard]] char Peek() const
    {
        return IsEnd() ? '\0' : input_[position_];
    }

    char Take()
    {
        return IsEnd() ? '\0' : input_[position_++];
    }

    void SkipWhitespace()
    {
        while (!IsEnd() && std::isspace(static_cast<unsigned char>(Peek())) != 0) {
            ++position_;
        }
    }

    [[nodiscard]] bool Consume(std::string_view expected)
    {
        if (input_.substr(position_, expected.size()) != expected) {
            return false;
        }
        position_ += expected.size();
        return true;
    }

    [[nodiscard]] std::optional<JsonValue> ParseValue(std::string& error)
    {
        SkipWhitespace();
        if (IsEnd()) {
            error = "unexpected end of file";
            return std::nullopt;
        }

        const char c = Peek();
        if (c == '{') {
            return ParseObject(error);
        }
        if (c == '"') {
            auto string_value = ParseString(error);
            if (!string_value.has_value()) {
                return std::nullopt;
            }
            return JsonValue{*string_value};
        }
        if (c == 't') {
            if (Consume("true")) {
                return JsonValue{true};
            }
        } else if (c == 'f') {
            if (Consume("false")) {
                return JsonValue{false};
            }
        } else if (c == 'n') {
            if (Consume("null")) {
                return JsonValue{nullptr};
            }
        } else if (c == '-' || std::isdigit(static_cast<unsigned char>(c)) != 0) {
            return ParseNumber(error);
        }

        error = "unexpected token";
        return std::nullopt;
    }

    [[nodiscard]] std::optional<JsonValue> ParseObject(std::string& error)
    {
        if (Take() != '{') {
            error = "expected object";
            return std::nullopt;
        }

        JsonValue::Object object;
        SkipWhitespace();
        if (Peek() == '}') {
            Take();
            return JsonValue{std::move(object)};
        }

        while (!IsEnd()) {
            SkipWhitespace();
            if (Peek() != '"') {
                error = "expected object key";
                return std::nullopt;
            }
            auto key = ParseString(error);
            if (!key.has_value()) {
                return std::nullopt;
            }

            SkipWhitespace();
            if (Take() != ':') {
                error = "expected ':' after object key";
                return std::nullopt;
            }

            auto value = ParseValue(error);
            if (!value.has_value()) {
                return std::nullopt;
            }
            object[*key] = std::move(*value);

            SkipWhitespace();
            const char next = Take();
            if (next == '}') {
                return JsonValue{std::move(object)};
            }
            if (next != ',') {
                error = "expected ',' or '}' in object";
                return std::nullopt;
            }
        }

        error = "unterminated object";
        return std::nullopt;
    }

    [[nodiscard]] std::optional<std::string> ParseString(std::string& error)
    {
        if (Take() != '"') {
            error = "expected string";
            return std::nullopt;
        }

        std::string result;
        while (!IsEnd()) {
            const char c = Take();
            if (c == '"') {
                return result;
            }
            if (c != '\\') {
                result.push_back(c);
                continue;
            }

            if (IsEnd()) {
                error = "unterminated escape sequence";
                return std::nullopt;
            }
            const char escaped = Take();
            switch (escaped) {
                case '"':
                case '\\':
                case '/':
                    result.push_back(escaped);
                    break;
                case 'b':
                    result.push_back('\b');
                    break;
                case 'f':
                    result.push_back('\f');
                    break;
                case 'n':
                    result.push_back('\n');
                    break;
                case 'r':
                    result.push_back('\r');
                    break;
                case 't':
                    result.push_back('\t');
                    break;
                default:
                    error = "unsupported escape sequence";
                    return std::nullopt;
            }
        }

        error = "unterminated string";
        return std::nullopt;
    }

    [[nodiscard]] std::optional<JsonValue> ParseNumber(std::string& error)
    {
        const std::size_t start = position_;
        if (Peek() == '-') {
            Take();
        }
        while (std::isdigit(static_cast<unsigned char>(Peek())) != 0) {
            Take();
        }
        if (Peek() == '.') {
            Take();
            while (std::isdigit(static_cast<unsigned char>(Peek())) != 0) {
                Take();
            }
        }
        if (Peek() == 'e' || Peek() == 'E') {
            Take();
            if (Peek() == '+' || Peek() == '-') {
                Take();
            }
            while (std::isdigit(static_cast<unsigned char>(Peek())) != 0) {
                Take();
            }
        }

        const std::string_view number_text = input_.substr(start, position_ - start);
        double number = 0.0;
        const auto* begin = number_text.data();
        const auto* end = number_text.data() + number_text.size();
        const auto [ptr, ec] = std::from_chars(begin, end, number);
        if (ec != std::errc{} || ptr != end) {
            error = "invalid number";
            return std::nullopt;
        }
        return JsonValue{number};
    }

    std::string_view input_;
    std::size_t position_ = 0;
};

[[nodiscard]] const JsonValue* FindMember(const JsonValue& root, std::initializer_list<std::string_view> path)
{
    const JsonValue* current = &root;
    for (const std::string_view segment : path) {
        const auto* object = std::get_if<JsonValue::Object>(&current->value);
        if (object == nullptr) {
            return nullptr;
        }
        const auto found = object->find(std::string(segment));
        if (found == object->end()) {
            return nullptr;
        }
        current = &found->second;
    }
    return current;
}

[[nodiscard]] std::optional<std::string> ReadString(const JsonValue& root, std::initializer_list<std::string_view> path)
{
    const JsonValue* value = FindMember(root, path);
    if (value == nullptr) {
        return std::nullopt;
    }
    if (const auto* string_value = std::get_if<std::string>(&value->value)) {
        return *string_value;
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<double> ReadNumber(const JsonValue& root, std::initializer_list<std::string_view> path)
{
    const JsonValue* value = FindMember(root, path);
    if (value == nullptr) {
        return std::nullopt;
    }
    if (const auto* number_value = std::get_if<double>(&value->value)) {
        return *number_value;
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<bool> ReadBool(const JsonValue& root, std::initializer_list<std::string_view> path)
{
    const JsonValue* value = FindMember(root, path);
    if (value == nullptr) {
        return std::nullopt;
    }
    if (const auto* bool_value = std::get_if<bool>(&value->value)) {
        return *bool_value;
    }
    return std::nullopt;
}

void AssignString(
    const JsonValue& root,
    std::initializer_list<std::string_view> path,
    std::string& target,
    std::string_view name,
    std::vector<std::string>& diagnostics)
{
    const JsonValue* value = FindMember(root, path);
    if (value == nullptr) {
        return;
    }
    if (const auto* string_value = std::get_if<std::string>(&value->value)) {
        target = *string_value;
        return;
    }
    diagnostics.push_back("config: ignored non-string field " + std::string(name));
}

void AssignPath(
    const JsonValue& root,
    std::initializer_list<std::string_view> path,
    std::filesystem::path& target,
    std::string_view name,
    std::vector<std::string>& diagnostics)
{
    const JsonValue* value = FindMember(root, path);
    if (value == nullptr) {
        return;
    }
    if (const auto* string_value = std::get_if<std::string>(&value->value)) {
        target = *string_value;
        return;
    }
    diagnostics.push_back("config: ignored non-string field " + std::string(name));
}

void AssignPositiveInt(
    const JsonValue& root,
    std::initializer_list<std::string_view> path,
    int& target,
    std::string_view name,
    std::vector<std::string>& diagnostics)
{
    const auto value = ReadNumber(root, path);
    if (!value.has_value()) {
        if (FindMember(root, path) != nullptr) {
            diagnostics.push_back("config: ignored non-number field " + std::string(name));
        }
        return;
    }
    if (*value <= 0.0) {
        diagnostics.push_back("config: ignored non-positive field " + std::string(name));
        return;
    }
    target = static_cast<int>(*value);
}

void AssignPositiveFloat(
    const JsonValue& root,
    std::initializer_list<std::string_view> path,
    float& target,
    std::string_view name,
    std::vector<std::string>& diagnostics)
{
    const auto value = ReadNumber(root, path);
    if (!value.has_value()) {
        if (FindMember(root, path) != nullptr) {
            diagnostics.push_back("config: ignored non-number field " + std::string(name));
        }
        return;
    }
    if (*value <= 0.0) {
        diagnostics.push_back("config: ignored non-positive field " + std::string(name));
        return;
    }
    target = static_cast<float>(*value);
}

void AssignBool(
    const JsonValue& root,
    std::initializer_list<std::string_view> path,
    bool& target,
    std::string_view name,
    std::vector<std::string>& diagnostics)
{
    const auto value = ReadBool(root, path);
    if (!value.has_value()) {
        if (FindMember(root, path) != nullptr) {
            diagnostics.push_back("config: ignored non-bool field " + std::string(name));
        }
        return;
    }
    target = *value;
}

void AssignLogLevel(
    const JsonValue& root,
    std::initializer_list<std::string_view> path,
    LogLevel& target,
    std::string_view name,
    std::vector<std::string>& diagnostics)
{
    const auto value = ReadString(root, path);
    if (!value.has_value()) {
        if (FindMember(root, path) != nullptr) {
            diagnostics.push_back("config: ignored non-string field " + std::string(name));
        }
        return;
    }

    LogLevel parsed = target;
    if (!ParseLogLevel(*value, parsed)) {
        diagnostics.push_back("config: ignored invalid log level field " + std::string(name) + " value=" + *value);
        return;
    }
    target = parsed;
}

void NormalizeConfig(AppConfig& config, std::vector<std::string>& diagnostics)
{
    if (config.ui_scale_min > config.ui_scale_max) {
        diagnostics.push_back("config: swapped ui.scale_min and ui.scale_max because min was greater than max");
        std::swap(config.ui_scale_min, config.ui_scale_max);
    }
    if (config.max_monitor_fraction > 1.0F) {
        diagnostics.push_back("config: clamped window.max_monitor_fraction to 1.0");
        config.max_monitor_fraction = 1.0F;
    }
    if (config.max_monitor_fraction < 0.10F) {
        diagnostics.push_back("config: clamped window.max_monitor_fraction to 0.10");
        config.max_monitor_fraction = 0.10F;
    }
    if (config.ui_font_scale < 0.50F) {
        diagnostics.push_back("config: clamped ui.font_scale to 0.50");
        config.ui_font_scale = 0.50F;
    }
    if (config.ui_font_scale > 2.00F) {
        diagnostics.push_back("config: clamped ui.font_scale to 2.00");
        config.ui_font_scale = 2.00F;
    }
}

}  // namespace

bool LoadAppConfigFromFile(
    const std::filesystem::path& config_path,
    AppConfig& config,
    std::vector<std::string>& diagnostics)
{
    config.config_path = config_path;

    std::ifstream file(config_path);
    if (!file) {
        diagnostics.push_back("config: file not found path=\"" + config_path.string() + "\", using built-in defaults");
        return false;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();

    std::string parse_error;
    const std::string json_text = buffer.str();
    JsonParser parser(json_text);
    std::optional<JsonValue> root = parser.Parse(parse_error);
    if (!root.has_value()) {
        diagnostics.push_back("config: parse failed path=\"" + config_path.string() + "\" reason=\"" + parse_error + "\"");
        return false;
    }

    AssignString(*root, {"app", "name"}, config.app_name, "app.name", diagnostics);
    AssignString(*root, {"language", "current"}, config.language, "language.current", diagnostics);
    AssignPath(*root, {"language", "directory"}, config.language_dir, "language.directory", diagnostics);
    AssignPath(*root, {"map", "path"}, config.map_package_path, "map.path", diagnostics);
    AssignPath(*root, {"map", "package_path"}, config.map_package_path, "map.package_path", diagnostics);

    AssignPositiveInt(*root, {"window", "base_width"}, config.base_width, "window.base_width", diagnostics);
    AssignPositiveInt(*root, {"window", "base_height"}, config.base_height, "window.base_height", diagnostics);
    AssignPositiveInt(*root, {"window", "preferred_width"}, config.base_width, "window.preferred_width", diagnostics);
    AssignPositiveInt(*root, {"window", "preferred_height"}, config.base_height, "window.preferred_height", diagnostics);
    AssignPositiveInt(*root, {"window", "fallback_width"}, config.fallback_width, "window.fallback_width", diagnostics);
    AssignPositiveInt(*root, {"window", "fallback_height"}, config.fallback_height, "window.fallback_height", diagnostics);
    AssignPositiveFloat(*root, {"window", "max_monitor_fraction"}, config.max_monitor_fraction, "window.max_monitor_fraction", diagnostics);
    AssignBool(*root, {"window", "resizable"}, config.window_resizable, "window.resizable", diagnostics);
    AssignBool(*root, {"window", "vsync"}, config.window_vsync, "window.vsync", diagnostics);
    AssignPositiveInt(*root, {"window", "target_fps"}, config.target_fps, "window.target_fps", diagnostics);

    AssignPositiveFloat(*root, {"ui", "scale_min"}, config.ui_scale_min, "ui.scale_min", diagnostics);
    AssignPositiveFloat(*root, {"ui", "scale_max"}, config.ui_scale_max, "ui.scale_max", diagnostics);
    AssignPositiveFloat(*root, {"ui", "font_scale"}, config.ui_font_scale, "ui.font_scale", diagnostics);
    AssignPath(*root, {"ui", "title_font_path"}, config.ui_title_font_path, "ui.title_font_path", diagnostics);
    AssignPath(*root, {"ui", "text_font_path"}, config.ui_text_font_path, "ui.text_font_path", diagnostics);

    AssignBool(*root, {"debug", "ui"}, config.debug_ui, "debug.ui", diagnostics);
    AssignBool(*root, {"log", "color"}, config.log_color, "log.color", diagnostics);
    AssignLogLevel(*root, {"log", "level"}, config.log_level, "log.level", diagnostics);
    AssignString(*root, {"log", "raylib_level"}, config.raylib_log_level, "log.raylib_level", diagnostics);
    AssignString(*root, {"raylib_log_level"}, config.raylib_log_level, "raylib_log_level", diagnostics);

    NormalizeConfig(config, diagnostics);
    return true;
}

}  // namespace vox3d
