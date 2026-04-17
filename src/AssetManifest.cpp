#include "AssetManifest.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
std::string escapeJson(const std::string& value) {
    std::ostringstream escaped;
    for (char character : value) {
        switch (character) {
            case '\\': escaped << "\\\\"; break;
            case '"': escaped << "\\\""; break;
            case '\n': escaped << "\\n"; break;
            case '\r': escaped << "\\r"; break;
            case '\t': escaped << "\\t"; break;
            default: escaped << character; break;
        }
    }
    return escaped.str();
}

std::string toLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

bool endsWith(const std::string& value, const std::string& suffix) {
    return value.size() >= suffix.size() &&
           value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::string normalizeStoredPath(const std::filesystem::path& path) {
    std::string normalized = path.lexically_normal().generic_string();
    while (normalized.rfind("./", 0) == 0) {
        normalized.erase(0, 2);
    }
    return normalized;
}

std::filesystem::path normalizedAbsolutePath(const std::filesystem::path& path) {
    std::error_code error;
    std::filesystem::path normalized = std::filesystem::weakly_canonical(path, error);
    if (!error) {
        return normalized;
    }

    normalized = std::filesystem::absolute(path, error);
    if (!error) {
        return normalized.lexically_normal();
    }

    return path.lexically_normal();
}

std::string relativeAssetPath(const std::filesystem::path& assetRoot, const std::filesystem::path& assetPath) {
    std::filesystem::path candidate = assetPath;
    if (!assetRoot.empty() && candidate.is_relative()) {
        candidate = assetRoot / candidate;
    }

    if (!assetRoot.empty()) {
        const std::filesystem::path absoluteRoot = normalizedAbsolutePath(assetRoot);
        const std::filesystem::path absolutePath = normalizedAbsolutePath(candidate);

        std::error_code error;
        const std::filesystem::path relativePath = std::filesystem::relative(absolutePath, absoluteRoot, error);
        if (!error && !relativePath.empty()) {
            const std::string text = relativePath.generic_string();
            if (text != "." && text != ".." && text.rfind("../", 0) != 0) {
                return normalizeStoredPath(relativePath);
            }
        }
    }

    return normalizeStoredPath(candidate);
}

std::string pathKey(const std::string& path) {
    return toLower(normalizeStoredPath(path));
}

std::string stripKnownAssetExtension(std::string value) {
    const std::string lower = toLower(value);
    if (endsWith(lower, ".prefab.json")) {
        value.erase(value.size() - 12);
    } else if (endsWith(lower, ".scene.json")) {
        value.erase(value.size() - 11);
    } else if (endsWith(lower, ".animation.json")) {
        value.erase(value.size() - 15);
    } else if (endsWith(lower, ".anim.json")) {
        value.erase(value.size() - 10);
    } else if (endsWith(lower, ".spritesheet.json")) {
        value.erase(value.size() - 17);
    } else {
        std::filesystem::path path = value;
        value = path.replace_extension().generic_string();
    }
    return value;
}

std::string displayNameFromPath(const std::string& path) {
    std::string name = std::filesystem::path(stripKnownAssetExtension(path)).filename().generic_string();
    return name.empty() ? path : name;
}

std::string sanitizeIdBody(std::string value) {
    value = toLower(value);
    for (char& character : value) {
        const unsigned char byte = static_cast<unsigned char>(character);
        if (std::isalnum(byte) || character == '/' || character == '_' || character == '-' || character == '.') {
            continue;
        }
        character = '_';
    }

    while (!value.empty() && value.front() == '/') {
        value.erase(value.begin());
    }
    while (!value.empty() && value.back() == '/') {
        value.pop_back();
    }

    return value.empty() ? "asset" : value;
}

std::string makeStableId(AssetType type, const std::string& path) {
    std::string prefix = AssetManifest::assetTypeToString(type);
    if (prefix == "unknown") {
        prefix = "asset";
    }

    return prefix + ":" + sanitizeIdBody(stripKnownAssetExtension(path));
}

struct JsonValue {
    enum class Type {
        Null,
        Bool,
        Number,
        String,
        Object,
        Array
    };

    Type type = Type::Null;
    bool boolean = false;
    double number = 0.0;
    std::string string;
    std::map<std::string, JsonValue> object;
    std::vector<JsonValue> array;

    const JsonValue* find(const std::string& key) const {
        const auto found = object.find(key);
        return found != object.end() ? &found->second : nullptr;
    }
};

class JsonParser {
public:
    explicit JsonParser(const std::string& input) : text(input) {}

    bool parse(JsonValue& value, std::string& parseError) {
        skipWhitespace();
        if (!parseValue(value)) {
            parseError = error.empty() ? "Invalid JSON." : error;
            return false;
        }

        skipWhitespace();
        if (position != text.size()) {
            parseError = "Unexpected trailing JSON content.";
            return false;
        }

        return true;
    }

private:
    const std::string& text;
    std::size_t position = 0;
    std::string error;

    bool parseValue(JsonValue& value) {
        skipWhitespace();
        if (position >= text.size()) {
            error = "Unexpected end of JSON.";
            return false;
        }

        const char character = text[position];
        if (character == '{') {
            return parseObject(value);
        }
        if (character == '[') {
            return parseArray(value);
        }
        if (character == '"') {
            value.type = JsonValue::Type::String;
            return parseString(value.string);
        }
        if (character == '-' || std::isdigit(static_cast<unsigned char>(character))) {
            value.type = JsonValue::Type::Number;
            return parseNumber(value.number);
        }
        if (matchLiteral("true")) {
            value.type = JsonValue::Type::Bool;
            value.boolean = true;
            return true;
        }
        if (matchLiteral("false")) {
            value.type = JsonValue::Type::Bool;
            value.boolean = false;
            return true;
        }
        if (matchLiteral("null")) {
            value.type = JsonValue::Type::Null;
            return true;
        }

        error = "Unexpected JSON value at offset " + std::to_string(position);
        return false;
    }

    bool parseObject(JsonValue& value) {
        value = {};
        value.type = JsonValue::Type::Object;
        ++position;
        skipWhitespace();

        if (consume('}')) {
            return true;
        }

        while (position < text.size()) {
            std::string key;
            if (!parseString(key)) {
                return false;
            }

            skipWhitespace();
            if (!consume(':')) {
                error = "Expected ':' after JSON object key.";
                return false;
            }

            JsonValue child;
            if (!parseValue(child)) {
                return false;
            }
            value.object[key] = std::move(child);

            skipWhitespace();
            if (consume('}')) {
                return true;
            }
            if (!consume(',')) {
                error = "Expected ',' or '}' in JSON object.";
                return false;
            }
            skipWhitespace();
        }

        error = "Unterminated JSON object.";
        return false;
    }

    bool parseArray(JsonValue& value) {
        value = {};
        value.type = JsonValue::Type::Array;
        ++position;
        skipWhitespace();

        if (consume(']')) {
            return true;
        }

        while (position < text.size()) {
            JsonValue child;
            if (!parseValue(child)) {
                return false;
            }
            value.array.push_back(std::move(child));

            skipWhitespace();
            if (consume(']')) {
                return true;
            }
            if (!consume(',')) {
                error = "Expected ',' or ']' in JSON array.";
                return false;
            }
            skipWhitespace();
        }

        error = "Unterminated JSON array.";
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
                error = "Unterminated JSON escape.";
                return false;
            }

            const char escaped = text[position++];
            switch (escaped) {
                case '"': value.push_back('"'); break;
                case '\\': value.push_back('\\'); break;
                case '/': value.push_back('/'); break;
                case 'b': value.push_back('\b'); break;
                case 'f': value.push_back('\f'); break;
                case 'n': value.push_back('\n'); break;
                case 'r': value.push_back('\r'); break;
                case 't': value.push_back('\t'); break;
                default:
                    error = "Unsupported JSON escape.";
                    return false;
            }
        }

        error = "Unterminated JSON string.";
        return false;
    }

    bool parseNumber(double& value) {
        const std::size_t begin = position;
        if (text[position] == '-') {
            ++position;
        }

        const std::size_t digitBegin = position;
        while (position < text.size() && std::isdigit(static_cast<unsigned char>(text[position]))) {
            ++position;
        }
        if (digitBegin == position) {
            error = "Expected JSON number.";
            return false;
        }

        if (position < text.size() && text[position] == '.') {
            ++position;
            while (position < text.size() && std::isdigit(static_cast<unsigned char>(text[position]))) {
                ++position;
            }
        }
        if (position < text.size() && (text[position] == 'e' || text[position] == 'E')) {
            ++position;
            if (position < text.size() && (text[position] == '+' || text[position] == '-')) {
                ++position;
            }
            while (position < text.size() && std::isdigit(static_cast<unsigned char>(text[position]))) {
                ++position;
            }
        }

        try {
            value = std::stod(text.substr(begin, position - begin));
        } catch (const std::exception&) {
            error = "Invalid JSON number.";
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

const JsonValue* objectField(const JsonValue& value, const char* name) {
    if (value.type != JsonValue::Type::Object) {
        return nullptr;
    }
    return value.find(name);
}

std::string readStringField(const JsonValue& value, const char* name, const std::string& fallback) {
    const JsonValue* field = objectField(value, name);
    return field && field->type == JsonValue::Type::String ? field->string : fallback;
}

int readIntField(const JsonValue& value, const char* name, int fallback) {
    const JsonValue* field = objectField(value, name);
    return field && field->type == JsonValue::Type::Number ? static_cast<int>(field->number) : fallback;
}

float readFloatField(const JsonValue& value, const char* name, float fallback) {
    const JsonValue* field = objectField(value, name);
    return field && field->type == JsonValue::Type::Number ? static_cast<float>(field->number) : fallback;
}

bool readBoolField(const JsonValue& value, const char* name, bool fallback) {
    const JsonValue* field = objectField(value, name);
    return field && field->type == JsonValue::Type::Bool ? field->boolean : fallback;
}

void readSpriteSheetMetadata(const JsonValue& value, SpriteSheetMetadata& metadata) {
    metadata.columns = std::max(1, readIntField(value, "columns", metadata.columns));
    metadata.rows = std::max(1, readIntField(value, "rows", metadata.rows));
    metadata.frameWidth = readFloatField(value, "frameWidth", metadata.frameWidth);
    metadata.frameHeight = readFloatField(value, "frameHeight", metadata.frameHeight);
}

void readAnimationClipMetadata(const JsonValue& value, AnimationClipMetadata& metadata) {
    metadata.spriteSheetId = readStringField(value, "spriteSheetId", metadata.spriteSheetId);
    metadata.firstFrame = std::max(0, readIntField(value, "firstFrame", metadata.firstFrame));
    metadata.frameCount = std::max(1, readIntField(value, "frameCount", metadata.frameCount));
    metadata.secondsPerFrame = readFloatField(value, "secondsPerFrame", metadata.secondsPerFrame);
    metadata.loop = readBoolField(value, "loop", metadata.loop);
}

void readFontMetadata(const JsonValue& value, FontMetadata& metadata) {
    metadata.glyphWidth = readFloatField(value, "glyphWidth", metadata.glyphWidth);
    metadata.glyphHeight = readFloatField(value, "glyphHeight", metadata.glyphHeight);
}

void readAudioMetadata(const JsonValue& value, AudioMetadata& metadata) {
    metadata.streaming = readBoolField(value, "streaming", metadata.streaming);
    metadata.volume = readFloatField(value, "volume", metadata.volume);
}

bool hasEntryWithId(const std::vector<AssetManifestEntry>& entries, const std::string& id) {
    return std::any_of(entries.begin(), entries.end(), [&id](const AssetManifestEntry& entry) {
        return entry.id == id;
    });
}

std::string uniqueIdForPath(
    const std::vector<AssetManifestEntry>& entries,
    AssetType type,
    const std::string& path) {
    const std::string baseId = makeStableId(type, path);
    std::string id = baseId;
    int suffix = 2;
    while (hasEntryWithId(entries, id)) {
        id = baseId + "-" + std::to_string(suffix++);
    }
    return id;
}

bool isManifestPath(const std::filesystem::path& assetRoot, const std::filesystem::path& path) {
    return normalizedAbsolutePath(path) == normalizedAbsolutePath(AssetManifest::defaultManifestPath(assetRoot));
}

void writeSpriteSheetMetadata(std::ostream& output, const SpriteSheetMetadata& metadata) {
    output << "      \"spriteSheet\": {\n";
    output << "        \"columns\": " << metadata.columns << ",\n";
    output << "        \"rows\": " << metadata.rows << ",\n";
    output << "        \"frameWidth\": " << metadata.frameWidth << ",\n";
    output << "        \"frameHeight\": " << metadata.frameHeight << "\n";
    output << "      }\n";
}

void writeAnimationClipMetadata(std::ostream& output, const AnimationClipMetadata& metadata) {
    output << "      \"animationClip\": {\n";
    output << "        \"spriteSheetId\": \"" << escapeJson(metadata.spriteSheetId) << "\",\n";
    output << "        \"firstFrame\": " << metadata.firstFrame << ",\n";
    output << "        \"frameCount\": " << metadata.frameCount << ",\n";
    output << "        \"secondsPerFrame\": " << metadata.secondsPerFrame << ",\n";
    output << "        \"loop\": " << (metadata.loop ? "true" : "false") << "\n";
    output << "      }\n";
}

void writeFontMetadata(std::ostream& output, const FontMetadata& metadata) {
    output << "      \"font\": {\n";
    output << "        \"glyphWidth\": " << metadata.glyphWidth << ",\n";
    output << "        \"glyphHeight\": " << metadata.glyphHeight << "\n";
    output << "      }\n";
}

void writeAudioMetadata(std::ostream& output, const AudioMetadata& metadata) {
    output << "      \"audio\": {\n";
    output << "        \"streaming\": " << (metadata.streaming ? "true" : "false") << ",\n";
    output << "        \"volume\": " << metadata.volume << "\n";
    output << "      }\n";
}
}

void AssetManifest::clear() {
    entries.clear();
}

bool AssetManifest::loadFromFile(const std::string& path, std::string& error) {
    std::ifstream input(path);
    if (!input) {
        error = "Failed to open asset manifest: " + path;
        return false;
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();

    const std::string contents = buffer.str();
    JsonValue root;
    JsonParser parser(contents);
    if (!parser.parse(root, error)) {
        error = path + ": " + error;
        return false;
    }

    if (root.type != JsonValue::Type::Object) {
        error = path + ": asset manifest root must be an object.";
        return false;
    }

    const JsonValue* assets = objectField(root, "assets");
    if (!assets || assets->type != JsonValue::Type::Array) {
        error = path + ": asset manifest must contain an assets array.";
        return false;
    }

    std::vector<AssetManifestEntry> parsedEntries;
    parsedEntries.reserve(assets->array.size());
    for (const JsonValue& asset : assets->array) {
        if (asset.type != JsonValue::Type::Object) {
            error = path + ": each asset manifest entry must be an object.";
            return false;
        }

        AssetManifestEntry entry;
        entry.id = readStringField(asset, "id", "");
        entry.type = assetTypeFromString(readStringField(asset, "type", "unknown"));
        entry.name = readStringField(asset, "name", "");
        entry.path = normalizeStoredPath(readStringField(asset, "path", ""));
        if (entry.id.empty() || entry.path.empty()) {
            error = path + ": asset manifest entries require non-empty id and path fields.";
            return false;
        }

        if (entry.name.empty()) {
            entry.name = displayNameFromPath(entry.path);
        }

        if (hasEntryWithId(parsedEntries, entry.id)) {
            error = path + ": duplicate asset id '" + entry.id + "'.";
            return false;
        }
        if (std::any_of(parsedEntries.begin(), parsedEntries.end(), [&entry](const AssetManifestEntry& existing) {
                return pathKey(existing.path) == pathKey(entry.path);
            })) {
            error = path + ": duplicate asset path '" + entry.path + "'.";
            return false;
        }

        if (const JsonValue* metadata = objectField(asset, "spriteSheet")) {
            if (metadata->type == JsonValue::Type::Object) {
                readSpriteSheetMetadata(*metadata, entry.spriteSheet);
            }
        }
        if (const JsonValue* metadata = objectField(asset, "animationClip")) {
            if (metadata->type == JsonValue::Type::Object) {
                readAnimationClipMetadata(*metadata, entry.animationClip);
            }
        }
        if (const JsonValue* metadata = objectField(asset, "font")) {
            if (metadata->type == JsonValue::Type::Object) {
                readFontMetadata(*metadata, entry.font);
            }
        }
        if (const JsonValue* metadata = objectField(asset, "audio")) {
            if (metadata->type == JsonValue::Type::Object) {
                readAudioMetadata(*metadata, entry.audio);
            }
        }

        parsedEntries.push_back(std::move(entry));
    }

    entries = std::move(parsedEntries);
    return true;
}

bool AssetManifest::saveToFile(const std::string& path, std::string& error) const {
    const std::filesystem::path manifestPath = path;
    if (manifestPath.has_parent_path()) {
        std::error_code createError;
        std::filesystem::create_directories(manifestPath.parent_path(), createError);
        if (createError) {
            error = "Failed to create asset manifest directory: " + manifestPath.parent_path().generic_string();
            return false;
        }
    }

    std::ofstream output(path);
    if (!output) {
        error = "Failed to write asset manifest: " + path;
        return false;
    }

    std::vector<AssetManifestEntry> sortedEntries = entries;
    std::sort(sortedEntries.begin(), sortedEntries.end(), [](const AssetManifestEntry& first, const AssetManifestEntry& second) {
        return first.id < second.id;
    });

    output << "{\n";
    output << "  \"version\": 1,\n";
    output << "  \"assets\": [\n";
    for (std::size_t index = 0; index < sortedEntries.size(); ++index) {
        const AssetManifestEntry& entry = sortedEntries[index];
        output << "    {\n";
        output << "      \"id\": \"" << escapeJson(entry.id) << "\",\n";
        output << "      \"type\": \"" << assetTypeToString(entry.type) << "\",\n";
        output << "      \"name\": \"" << escapeJson(entry.name) << "\",\n";
        output << "      \"path\": \"" << escapeJson(entry.path) << "\"";

        switch (entry.type) {
            case AssetType::SpriteSheet:
                output << ",\n";
                writeSpriteSheetMetadata(output, entry.spriteSheet);
                break;
            case AssetType::AnimationClip:
                output << ",\n";
                writeAnimationClipMetadata(output, entry.animationClip);
                break;
            case AssetType::Font:
                output << ",\n";
                writeFontMetadata(output, entry.font);
                break;
            case AssetType::Audio:
                output << ",\n";
                writeAudioMetadata(output, entry.audio);
                break;
            default:
                output << '\n';
                break;
        }

        output << "    }";
        if (index + 1 < sortedEntries.size()) {
            output << ',';
        }
        output << '\n';
    }
    output << "  ]\n";
    output << "}\n";

    if (!output) {
        error = "Failed to finish writing asset manifest: " + path;
        return false;
    }

    return true;
}

AssetManifestEntry& AssetManifest::importAsset(
    const std::filesystem::path& assetRoot,
    const std::filesystem::path& assetPath,
    AssetType type) {
    const std::string storedPath = relativeAssetPath(assetRoot, assetPath);
    if (AssetManifestEntry* existing = findByPath(storedPath)) {
        if (type != AssetType::Unknown) {
            existing->type = type;
        }
        return *existing;
    }

    const AssetType resolvedType = type == AssetType::Unknown ? inferAssetType(assetPath) : type;

    AssetManifestEntry entry;
    entry.type = resolvedType;
    entry.path = storedPath;
    entry.name = displayNameFromPath(storedPath);
    entry.id = uniqueIdForPath(entries, resolvedType, storedPath);

    entries.push_back(std::move(entry));
    return entries.back();
}

std::size_t AssetManifest::scanDirectory(const std::filesystem::path& assetRoot) {
    if (!std::filesystem::exists(assetRoot)) {
        return 0;
    }

    std::vector<std::filesystem::path> paths;
    for (const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator(assetRoot)) {
        if (!entry.is_regular_file() || isManifestPath(assetRoot, entry.path())) {
            continue;
        }

        if (inferAssetType(entry.path()) != AssetType::Unknown) {
            paths.push_back(entry.path());
        }
    }

    std::sort(paths.begin(), paths.end(), [](const std::filesystem::path& first, const std::filesystem::path& second) {
        return first.generic_string() < second.generic_string();
    });

    std::size_t added = 0;
    for (const std::filesystem::path& path : paths) {
        const std::size_t before = entries.size();
        importAsset(assetRoot, path);
        if (entries.size() > before) {
            ++added;
        }
    }

    return added;
}

const AssetManifestEntry* AssetManifest::findById(const std::string& id) const {
    const auto found = std::find_if(entries.begin(), entries.end(), [&id](const AssetManifestEntry& entry) {
        return entry.id == id;
    });
    return found != entries.end() ? &*found : nullptr;
}

AssetManifestEntry* AssetManifest::findById(const std::string& id) {
    const auto found = std::find_if(entries.begin(), entries.end(), [&id](AssetManifestEntry& entry) {
        return entry.id == id;
    });
    return found != entries.end() ? &*found : nullptr;
}

const AssetManifestEntry* AssetManifest::findByPath(const std::string& path) const {
    const std::string key = pathKey(path);
    const auto found = std::find_if(entries.begin(), entries.end(), [&key](const AssetManifestEntry& entry) {
        return pathKey(entry.path) == key;
    });
    return found != entries.end() ? &*found : nullptr;
}

AssetManifestEntry* AssetManifest::findByPath(const std::string& path) {
    const std::string key = pathKey(path);
    const auto found = std::find_if(entries.begin(), entries.end(), [&key](AssetManifestEntry& entry) {
        return pathKey(entry.path) == key;
    });
    return found != entries.end() ? &*found : nullptr;
}

const std::vector<AssetManifestEntry>& AssetManifest::getEntries() const {
    return entries;
}

std::size_t AssetManifest::entryCount() const {
    return entries.size();
}

AssetType AssetManifest::inferAssetType(const std::filesystem::path& path) {
    const std::string filename = toLower(path.filename().generic_string());
    const std::string extension = toLower(path.extension().generic_string());

    if (endsWith(filename, ".animation.json") || endsWith(filename, ".anim.json")) {
        return AssetType::AnimationClip;
    }
    if (endsWith(filename, ".spritesheet.json")) {
        return AssetType::SpriteSheet;
    }
    if (endsWith(filename, ".prefab.json")) {
        return AssetType::Prefab;
    }
    if (endsWith(filename, ".scene.json") || filename == "scene_saved.json") {
        return AssetType::Scene;
    }
    if (extension == ".png" || extension == ".jpg" || extension == ".jpeg" || extension == ".bmp" ||
        extension == ".tga" || extension == ".ppm") {
        return AssetType::Texture;
    }
    if (extension == ".wav") {
        return AssetType::Audio;
    }
    if (extension == ".ttf" || extension == ".otf" || extension == ".fnt") {
        return AssetType::Font;
    }

    return AssetType::Unknown;
}

const char* AssetManifest::assetTypeToString(AssetType type) {
    switch (type) {
        case AssetType::Texture: return "texture";
        case AssetType::SpriteSheet: return "spriteSheet";
        case AssetType::AnimationClip: return "animationClip";
        case AssetType::Audio: return "audio";
        case AssetType::Font: return "font";
        case AssetType::Prefab: return "prefab";
        case AssetType::Scene: return "scene";
        case AssetType::Unknown: return "unknown";
    }

    return "unknown";
}

AssetType AssetManifest::assetTypeFromString(const std::string& type) {
    const std::string normalized = toLower(type);
    if (normalized == "texture") {
        return AssetType::Texture;
    }
    if (normalized == "spritesheet" || normalized == "sprite_sheet") {
        return AssetType::SpriteSheet;
    }
    if (normalized == "animationclip" || normalized == "animation_clip" || normalized == "animation") {
        return AssetType::AnimationClip;
    }
    if (normalized == "audio") {
        return AssetType::Audio;
    }
    if (normalized == "font") {
        return AssetType::Font;
    }
    if (normalized == "prefab") {
        return AssetType::Prefab;
    }
    if (normalized == "scene") {
        return AssetType::Scene;
    }
    return AssetType::Unknown;
}

std::filesystem::path AssetManifest::defaultManifestPath(const std::filesystem::path& assetRoot) {
    return assetRoot / "asset_manifest.json";
}
