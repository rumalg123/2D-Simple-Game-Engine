#include "GameConfig.h"

#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace {
enum class ConfigValueType {
    Bool,
    Number,
    String
};

struct ConfigValue {
    ConfigValueType type = ConfigValueType::String;
    bool boolean = false;
    int number = 0;
    std::string string;
};

class FlatJsonConfigParser {
public:
    explicit FlatJsonConfigParser(const std::string& input) : text(input) {}

    bool parse(std::unordered_map<std::string, ConfigValue>& values, std::string& parseError) {
        skipWhitespace();
        if (!consume('{')) {
            parseError = "Config must start with a JSON object.";
            return false;
        }

        skipWhitespace();
        if (consume('}')) {
            return true;
        }

        while (position < text.size()) {
            std::string key;
            if (!parseString(key)) {
                parseError = error;
                return false;
            }

            skipWhitespace();
            if (!consume(':')) {
                parseError = "Expected ':' after config key.";
                return false;
            }

            ConfigValue value;
            if (!parseValue(value)) {
                parseError = error;
                return false;
            }
            values[key] = value;

            skipWhitespace();
            if (consume('}')) {
                skipWhitespace();
                if (position != text.size()) {
                    parseError = "Unexpected content after config object.";
                    return false;
                }
                return true;
            }

            if (!consume(',')) {
                parseError = "Expected ',' or '}' in config object.";
                return false;
            }
            skipWhitespace();
        }

        parseError = "Unterminated config object.";
        return false;
    }

private:
    const std::string& text;
    std::size_t position = 0;
    std::string error;

    bool parseValue(ConfigValue& value) {
        skipWhitespace();
        if (position >= text.size()) {
            error = "Unexpected end of config value.";
            return false;
        }

        if (text[position] == '"') {
            value.type = ConfigValueType::String;
            return parseString(value.string);
        }

        if (matchLiteral("true")) {
            value.type = ConfigValueType::Bool;
            value.boolean = true;
            return true;
        }

        if (matchLiteral("false")) {
            value.type = ConfigValueType::Bool;
            value.boolean = false;
            return true;
        }

        if (text[position] == '-' || std::isdigit(static_cast<unsigned char>(text[position]))) {
            value.type = ConfigValueType::Number;
            return parseInteger(value.number);
        }

        error = "Unsupported config value. Use strings, integers, or booleans.";
        return false;
    }

    bool parseString(std::string& value) {
        if (!consume('"')) {
            error = "Expected JSON string.";
            return false;
        }

        value.clear();
        while (position < text.size()) {
            const char character = text[position++];
            if (character == '"') {
                return true;
            }

            if (character != '\\') {
                value.push_back(character);
                continue;
            }

            if (position >= text.size()) {
                error = "Unterminated string escape.";
                return false;
            }

            const char escaped = text[position++];
            switch (escaped) {
                case '"': value.push_back('"'); break;
                case '\\': value.push_back('\\'); break;
                case '/': value.push_back('/'); break;
                case 'n': value.push_back('\n'); break;
                case 'r': value.push_back('\r'); break;
                case 't': value.push_back('\t'); break;
                default:
                    error = "Unsupported string escape.";
                    return false;
            }
        }

        error = "Unterminated string.";
        return false;
    }

    bool parseInteger(int& value) {
        const std::size_t begin = position;
        if (text[position] == '-') {
            ++position;
        }

        const std::size_t digitBegin = position;
        while (position < text.size() && std::isdigit(static_cast<unsigned char>(text[position]))) {
            ++position;
        }

        if (digitBegin == position) {
            error = "Expected integer value.";
            return false;
        }

        try {
            value = std::stoi(text.substr(begin, position - begin));
        } catch (const std::exception&) {
            error = "Invalid integer value.";
            return false;
        }

        return true;
    }

    void skipWhitespace() {
        while (position < text.size() && std::isspace(static_cast<unsigned char>(text[position]))) {
            ++position;
        }
    }

    bool consume(char expected) {
        if (position < text.size() && text[position] == expected) {
            ++position;
            return true;
        }
        return false;
    }

    bool matchLiteral(const char* literal) {
        const std::size_t begin = position;
        for (std::size_t index = 0; literal[index] != '\0'; ++index) {
            if (begin + index >= text.size() || text[begin + index] != literal[index]) {
                return false;
            }
        }

        position += std::char_traits<char>::length(literal);
        return true;
    }
};

bool readString(
    const std::unordered_map<std::string, ConfigValue>& values,
    const char* key,
    std::string& target,
    std::string& error) {
    const auto found = values.find(key);
    if (found == values.end()) {
        return true;
    }

    if (found->second.type != ConfigValueType::String) {
        error = std::string("Config key '") + key + "' must be a string.";
        return false;
    }

    target = found->second.string;
    return true;
}

bool readInt(
    const std::unordered_map<std::string, ConfigValue>& values,
    const char* key,
    int& target,
    std::string& error) {
    const auto found = values.find(key);
    if (found == values.end()) {
        return true;
    }

    if (found->second.type != ConfigValueType::Number) {
        error = std::string("Config key '") + key + "' must be an integer.";
        return false;
    }

    target = found->second.number;
    return true;
}

bool readBool(
    const std::unordered_map<std::string, ConfigValue>& values,
    const char* key,
    bool& target,
    std::string& error) {
    const auto found = values.find(key);
    if (found == values.end()) {
        return true;
    }

    if (found->second.type != ConfigValueType::Bool) {
        error = std::string("Config key '") + key + "' must be a boolean.";
        return false;
    }

    target = found->second.boolean;
    return true;
}
}

bool loadGameConfigFromFile(const std::string& path, GameConfig& config, std::string& error) {
    std::ifstream input(path);
    if (!input) {
        error = "Failed to open game config: " + path;
        return false;
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();

    std::unordered_map<std::string, ConfigValue> values;
    FlatJsonConfigParser parser(buffer.str());
    if (!parser.parse(values, error)) {
        error = path + ": " + error;
        return false;
    }

    GameConfig parsed = config;
    if (!readString(values, "windowTitle", parsed.windowTitle, error) ||
        !readInt(values, "windowWidth", parsed.windowWidth, error) ||
        !readInt(values, "windowHeight", parsed.windowHeight, error) ||
        !readBool(values, "editorEnabled", parsed.editorEnabled, error) ||
        !readBool(values, "startPlaying", parsed.startPlaying, error) ||
        !readBool(values, "hotReload", parsed.hotReload, error) ||
        !readBool(values, "vsync", parsed.vsync, error) ||
        !readString(values, "assetRoot", parsed.assetRoot, error) ||
        !readString(values, "prefabDirectory", parsed.prefabDirectory, error) ||
        !readString(values, "editorLayoutPath", parsed.editorLayoutPath, error)) {
        error = path + ": " + error;
        return false;
    }

    if (parsed.windowWidth <= 0 || parsed.windowHeight <= 0) {
        error = path + ": windowWidth and windowHeight must be greater than zero.";
        return false;
    }

    if (parsed.windowTitle.empty()) {
        error = path + ": windowTitle must not be empty.";
        return false;
    }

    if (parsed.assetRoot.empty()) {
        error = path + ": assetRoot must not be empty.";
        return false;
    }

    if (parsed.prefabDirectory.empty()) {
        error = path + ": prefabDirectory must not be empty.";
        return false;
    }

    if (parsed.editorEnabled && parsed.editorLayoutPath.empty()) {
        error = path + ": editorLayoutPath must not be empty when editorEnabled is true.";
        return false;
    }

    config = parsed;
    return true;
}

