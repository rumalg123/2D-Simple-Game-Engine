#include "SceneSerializer.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
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

void writeBool(std::ostream& output, const char* name, bool value, bool trailingComma) {
    output << "        \"" << name << "\": " << (value ? "true" : "false");
    if (trailingComma) {
        output << ',';
    }
    output << '\n';
}

void writeFloat(std::ostream& output, const char* name, float value, bool trailingComma) {
    output << "        \"" << name << "\": " << value;
    if (trailingComma) {
        output << ',';
    }
    output << '\n';
}

void writeInt(std::ostream& output, const char* name, int value, bool trailingComma) {
    output << "        \"" << name << "\": " << value;
    if (trailingComma) {
        output << ',';
    }
    output << '\n';
}

void writeUnsigned(std::ostream& output, const char* name, unsigned int value, bool trailingComma) {
    output << "        \"" << name << "\": " << value;
    if (trailingComma) {
        output << ',';
    }
    output << '\n';
}

void writeSpriteFields(std::ostream& output, const SpriteComponent& sprite, const ResourceManager& resources) {
    const bool hasTextureReference = sprite.texture != InvalidTexture && sprite.texture < resources.textureCount();

    writeFloat(output, "halfWidth", sprite.halfWidth, true);
    writeFloat(output, "halfHeight", sprite.halfHeight, true);
    writeFloat(output, "red", sprite.red, true);
    writeFloat(output, "green", sprite.green, true);
    writeFloat(output, "blue", sprite.blue, true);
    writeFloat(output, "alpha", sprite.alpha, true);
    writeInt(output, "layer", sprite.layer, true);
    output << "        \"texture\": " << sprite.texture << ",\n";
    writeFloat(output, "uMin", sprite.uMin, true);
    writeFloat(output, "vMin", sprite.vMin, true);
    writeFloat(output, "uMax", sprite.uMax, true);
    writeFloat(output, "vMax", sprite.vMax, hasTextureReference);

    if (hasTextureReference) {
        const TextureResource& texture = resources.getTexture(sprite.texture);
        output << "        \"textureName\": \"" << escapeJson(texture.name) << "\",\n";
        output << "        \"texturePath\": \"" << escapeJson(texture.sourcePath) << "\"\n";
    }
}

void writeSpriteAnimationFields(std::ostream& output, const SpriteAnimationComponent& animation) {
    writeInt(output, "columns", animation.columns, true);
    writeInt(output, "rows", animation.rows, true);
    writeInt(output, "firstFrame", animation.firstFrame, true);
    writeInt(output, "frameCount", animation.frameCount, true);
    writeInt(output, "currentFrame", animation.currentFrame, true);
    writeFloat(output, "secondsPerFrame", animation.secondsPerFrame, true);
    writeFloat(output, "elapsedSeconds", animation.elapsedSeconds, true);
    writeBool(output, "playing", animation.playing, true);
    writeBool(output, "loop", animation.loop, false);
}

void writeTextFields(std::ostream& output, const TextComponent& text) {
    output << "        \"text\": \"" << escapeJson(text.text) << "\",\n";
    writeFloat(output, "characterWidth", text.characterWidth, true);
    writeFloat(output, "characterHeight", text.characterHeight, true);
    writeFloat(output, "characterSpacing", text.characterSpacing, true);
    writeFloat(output, "lineSpacing", text.lineSpacing, true);
    writeFloat(output, "red", text.red, true);
    writeFloat(output, "green", text.green, true);
    writeFloat(output, "blue", text.blue, true);
    writeFloat(output, "alpha", text.alpha, true);
    writeInt(output, "layer", text.layer, true);
    writeBool(output, "screenSpace", text.screenSpace, false);
}

void writeTilemapFields(std::ostream& output, const TilemapComponent& tilemap, const ResourceManager& resources) {
    const bool hasTextureReference = tilemap.texture != InvalidTexture && tilemap.texture < resources.textureCount();
    const int columns = std::max(1, tilemap.columns);

    writeInt(output, "columns", tilemap.columns, true);
    writeInt(output, "rows", tilemap.rows, true);
    writeFloat(output, "tileWidth", tilemap.tileWidth, true);
    writeFloat(output, "tileHeight", tilemap.tileHeight, true);
    writeInt(output, "atlasColumns", tilemap.atlasColumns, true);
    writeInt(output, "atlasRows", tilemap.atlasRows, true);
    writeInt(output, "layer", tilemap.layer, true);
    output << "        \"texture\": " << tilemap.texture << ",\n";
    writeFloat(output, "red", tilemap.red, true);
    writeFloat(output, "green", tilemap.green, true);
    writeFloat(output, "blue", tilemap.blue, true);
    writeFloat(output, "alpha", tilemap.alpha, true);

    if (hasTextureReference) {
        const TextureResource& texture = resources.getTexture(tilemap.texture);
        output << "        \"textureName\": \"" << escapeJson(texture.name) << "\",\n";
        output << "        \"texturePath\": \"" << escapeJson(texture.sourcePath) << "\",\n";
    }

    writeBool(output, "collisionEnabled", tilemap.collisionEnabled, true);
    writeBool(output, "collisionSolid", tilemap.collisionSolid, true);
    writeBool(output, "collisionTrigger", tilemap.collisionTrigger, true);
    writeUnsigned(output, "collisionLayer", tilemap.collisionLayer, true);
    writeUnsigned(output, "collisionMask", tilemap.collisionMask, true);

    output << "        \"tiles\": [";
    for (std::size_t index = 0; index < tilemap.tiles.size(); ++index) {
        if (index % static_cast<std::size_t>(columns) == 0) {
            output << "\n          ";
        } else {
            output << ' ';
        }
        output << tilemap.tiles[index];
        if (index + 1 < tilemap.tiles.size()) {
            output << ',';
        }
    }
    if (!tilemap.tiles.empty()) {
        output << '\n' << "        ";
    }
    output << "]\n";
}

void writeScriptParameters(std::ostream& output, const ScriptComponent& script) {
    std::vector<std::string> names;
    names.reserve(script.parameters.size());
    for (const auto& parameter : script.parameters) {
        names.push_back(parameter.first);
    }
    std::sort(names.begin(), names.end());

    output << "        \"parameters\": {\n";
    for (std::size_t index = 0; index < names.size(); ++index) {
        const std::string& name = names[index];
        output << "          \"" << escapeJson(name) << "\": " << script.parameters.at(name);
        if (index + 1 < names.size()) {
            output << ',';
        }
        output << '\n';
    }
    output << "        }\n";
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
        if (position < text.size()) {
            error += " near '";
            error += text[position];
            error += "'";
        }
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
        while (position < text.size() && std::isdigit(static_cast<unsigned char>(text[position]))) {
            ++position;
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

bool parseJsonFile(const std::string& path, JsonValue& root, std::string& error) {
    std::ifstream input(path);
    if (!input) {
        error = "Failed to open JSON file: " + path;
        return false;
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    const std::string contents = buffer.str();
    JsonParser parser(contents);
    if (!parser.parse(root, error)) {
        error = path + ": " + error;
        return false;
    }

    if (root.type != JsonValue::Type::Object) {
        error = path + ": root JSON value must be an object.";
        return false;
    }

    return true;
}

const JsonValue* objectField(const JsonValue& value, const char* name) {
    if (value.type != JsonValue::Type::Object) {
        return nullptr;
    }

    return value.find(name);
}

float readFloatField(const JsonValue& value, const char* name, float fallback) {
    const JsonValue* field = objectField(value, name);
    return field && field->type == JsonValue::Type::Number ? static_cast<float>(field->number) : fallback;
}

int readIntField(const JsonValue& value, const char* name, int fallback) {
    const JsonValue* field = objectField(value, name);
    return field && field->type == JsonValue::Type::Number ? static_cast<int>(field->number) : fallback;
}

unsigned int readUnsignedField(const JsonValue& value, const char* name, unsigned int fallback) {
    const JsonValue* field = objectField(value, name);
    if (!field || field->type != JsonValue::Type::Number || field->number < 0.0) {
        return fallback;
    }
    return static_cast<unsigned int>(field->number);
}

Entity readEntityField(const JsonValue& value, const char* name, Entity fallback) {
    const JsonValue* field = objectField(value, name);
    return field && field->type == JsonValue::Type::Number ? static_cast<Entity>(field->number) : fallback;
}

bool readBoolField(const JsonValue& value, const char* name, bool fallback) {
    const JsonValue* field = objectField(value, name);
    return field && field->type == JsonValue::Type::Bool ? field->boolean : fallback;
}

std::string readStringField(const JsonValue& value, const char* name, const std::string& fallback) {
    const JsonValue* field = objectField(value, name);
    return field && field->type == JsonValue::Type::String ? field->string : fallback;
}

std::vector<int> readIntArrayField(const JsonValue& value, const char* name) {
    std::vector<int> result;
    const JsonValue* field = objectField(value, name);
    if (!field || field->type != JsonValue::Type::Array) {
        return result;
    }

    result.reserve(field->array.size());
    for (const JsonValue& item : field->array) {
        if (item.type == JsonValue::Type::Number) {
            result.push_back(static_cast<int>(item.number));
        }
    }
    return result;
}

void readScriptParameters(const JsonValue& scriptObject, ScriptComponent& script) {
    const JsonValue* parameters = objectField(scriptObject, "parameters");
    if (!parameters || parameters->type != JsonValue::Type::Object) {
        return;
    }

    for (const auto& parameter : parameters->object) {
        if (parameter.second.type == JsonValue::Type::Number) {
            script.parameters[parameter.first] = static_cast<float>(parameter.second.number);
        }
    }
}

TextureHandle resolveTextureReference(ResourceManager& resources, const JsonValue& spriteObject) {
    const std::string textureName = readStringField(spriteObject, "textureName", "");
    const std::string texturePath = readStringField(spriteObject, "texturePath", "");

    if (!texturePath.empty() && std::filesystem::exists(texturePath)) {
        try {
            return resources.loadTextureFromFile(textureName.empty() ? texturePath : textureName, texturePath);
        } catch (const std::exception&) {
        }
    }

    if (!textureName.empty()) {
        const TextureHandle existing = resources.getTextureHandle(textureName);
        if (existing != InvalidTexture) {
            return existing;
        }
    }

    const TextureHandle numericHandle = readEntityField(spriteObject, "texture", InvalidTexture);
    if (numericHandle != InvalidTexture && numericHandle < resources.textureCount()) {
        return numericHandle;
    }

    return resources.createSolidColorTexture("editor_default_white", 255, 255, 255, 255);
}

SpriteComponent readSpriteComponent(const JsonValue& sprite, ResourceManager& resources) {
    return SpriteComponent{
        readFloatField(sprite, "halfWidth", 0.10f),
        readFloatField(sprite, "halfHeight", 0.10f),
        readFloatField(sprite, "red", 1.0f),
        readFloatField(sprite, "green", 1.0f),
        readFloatField(sprite, "blue", 1.0f),
        readFloatField(sprite, "alpha", 1.0f),
        readIntField(sprite, "layer", 10),
        resolveTextureReference(resources, sprite),
        readFloatField(sprite, "uMin", 0.0f),
        readFloatField(sprite, "vMin", 0.0f),
        readFloatField(sprite, "uMax", 1.0f),
        readFloatField(sprite, "vMax", 1.0f)
    };
}

SpriteAnimationComponent readSpriteAnimationComponent(const JsonValue& animation) {
    return SpriteAnimationComponent{
        readIntField(animation, "columns", 1),
        readIntField(animation, "rows", 1),
        readIntField(animation, "firstFrame", 0),
        readIntField(animation, "frameCount", 1),
        readIntField(animation, "currentFrame", 0),
        readFloatField(animation, "secondsPerFrame", 0.1f),
        readFloatField(animation, "elapsedSeconds", 0.0f),
        readBoolField(animation, "playing", true),
        readBoolField(animation, "loop", true)
    };
}

TextComponent readTextComponent(const JsonValue& text) {
    return TextComponent{
        readStringField(text, "text", "TEXT"),
        readFloatField(text, "characterWidth", 0.08f),
        readFloatField(text, "characterHeight", 0.12f),
        readFloatField(text, "characterSpacing", 0.02f),
        readFloatField(text, "lineSpacing", 0.04f),
        readFloatField(text, "red", 1.0f),
        readFloatField(text, "green", 1.0f),
        readFloatField(text, "blue", 1.0f),
        readFloatField(text, "alpha", 1.0f),
        readIntField(text, "layer", 100),
        readBoolField(text, "screenSpace", true)
    };
}

TilemapComponent readTilemapComponent(const JsonValue& tilemapObject, ResourceManager& resources) {
    TilemapComponent tilemap{
        readIntField(tilemapObject, "columns", 1),
        readIntField(tilemapObject, "rows", 1),
        readFloatField(tilemapObject, "tileWidth", 0.16f),
        readFloatField(tilemapObject, "tileHeight", 0.16f),
        readIntField(tilemapObject, "atlasColumns", 1),
        readIntField(tilemapObject, "atlasRows", 1),
        readIntField(tilemapObject, "layer", -100),
        resolveTextureReference(resources, tilemapObject),
        readFloatField(tilemapObject, "red", 1.0f),
        readFloatField(tilemapObject, "green", 1.0f),
        readFloatField(tilemapObject, "blue", 1.0f),
        readFloatField(tilemapObject, "alpha", 1.0f),
        readIntArrayField(tilemapObject, "tiles")
    };

    tilemap.columns = std::max(1, tilemap.columns);
    tilemap.rows = std::max(1, tilemap.rows);
    tilemap.tileWidth = std::max(0.001f, tilemap.tileWidth);
    tilemap.tileHeight = std::max(0.001f, tilemap.tileHeight);
    tilemap.atlasColumns = std::max(1, tilemap.atlasColumns);
    tilemap.atlasRows = std::max(1, tilemap.atlasRows);
    tilemap.collisionEnabled = readBoolField(tilemapObject, "collisionEnabled", tilemap.collisionEnabled);
    tilemap.collisionSolid = readBoolField(tilemapObject, "collisionSolid", tilemap.collisionSolid);
    tilemap.collisionTrigger = readBoolField(tilemapObject, "collisionTrigger", tilemap.collisionTrigger);
    tilemap.collisionLayer = readUnsignedField(tilemapObject, "collisionLayer", tilemap.collisionLayer);
    tilemap.collisionMask = readUnsignedField(tilemapObject, "collisionMask", tilemap.collisionMask);

    const std::size_t tileCount = static_cast<std::size_t>(tilemap.columns) * static_cast<std::size_t>(tilemap.rows);
    tilemap.tiles.resize(tileCount, 0);
    return tilemap;
}

Prefab readPrefabFromComponents(
    const JsonValue& root,
    const JsonValue& components,
    ResourceManager& resources,
    const std::string& fallbackName) {
    Prefab prefab;
    prefab.name = readStringField(root, "name", fallbackName);

    if (const JsonValue* name = objectField(components, "name")) {
        prefab.name = readStringField(*name, "name", prefab.name);
    }
    if (const JsonValue* tag = objectField(components, "tag")) {
        prefab.tag = TagComponent{readStringField(*tag, "tag", "Untagged")};
    }
    if (const JsonValue* transform = objectField(components, "transform")) {
        prefab.transform = TransformComponent{
            readFloatField(*transform, "x", 0.0f),
            readFloatField(*transform, "y", 0.0f)
        };
    }
    if (const JsonValue* sprite = objectField(components, "sprite")) {
        prefab.sprite = readSpriteComponent(*sprite, resources);
    }
    if (const JsonValue* animation = objectField(components, "spriteAnimation")) {
        prefab.spriteAnimation = readSpriteAnimationComponent(*animation);
    }
    if (const JsonValue* text = objectField(components, "text")) {
        prefab.text = readTextComponent(*text);
    }
    if (const JsonValue* tilemap = objectField(components, "tilemap")) {
        prefab.tilemap = readTilemapComponent(*tilemap, resources);
    }
    if (const JsonValue* physics = objectField(components, "physics")) {
        prefab.physics = PhysicsComponent{
            readFloatField(*physics, "velocityX", 0.0f),
            readFloatField(*physics, "velocityY", 0.0f),
            readFloatField(*physics, "forceX", 0.0f),
            readFloatField(*physics, "forceY", 0.0f),
            readFloatField(*physics, "inverseMass", 1.0f),
            readFloatField(*physics, "drag", 0.0f),
            readFloatField(*physics, "restitution", 0.0f),
            readBoolField(*physics, "isStatic", false),
            readBoolField(*physics, "usesWorldBounds", false),
            readBoolField(*physics, "bounceAtBounds", false)
        };
    }
    if (const JsonValue* collider = objectField(components, "collider")) {
        prefab.collider = ColliderComponent{
            readFloatField(*collider, "halfWidth", 0.10f),
            readFloatField(*collider, "halfHeight", 0.10f),
            readBoolField(*collider, "solid", true),
            readBoolField(*collider, "trigger", false),
            readUnsignedField(*collider, "layer", ColliderLayerDefault),
            readUnsignedField(*collider, "mask", ColliderMaskAll)
        };
    }
    if (const JsonValue* bounds = objectField(components, "bounds")) {
        prefab.bounds = BoundsComponent{
            readFloatField(*bounds, "minX", -1.0f),
            readFloatField(*bounds, "maxX", 1.0f),
            readFloatField(*bounds, "minY", -1.0f),
            readFloatField(*bounds, "maxY", 1.0f)
        };
    }
    if (const JsonValue* input = objectField(components, "input")) {
        prefab.input = InputComponent{
            readBoolField(*input, "enabled", true),
            readStringField(*input, "moveLeftAction", "MoveLeft"),
            readStringField(*input, "moveRightAction", "MoveRight"),
            readStringField(*input, "moveUpAction", "MoveUp"),
            readStringField(*input, "moveDownAction", "MoveDown"),
            readFloatField(*input, "moveForce", 6.0f)
        };
    }
    if (const JsonValue* script = objectField(components, "script")) {
        ScriptComponent scriptComponent{
            readStringField(*script, "scriptName", ""),
            readBoolField(*script, "active", true),
            readFloatField(*script, "elapsedTime", 0.0f)
        };
        readScriptParameters(*script, scriptComponent);
        prefab.script = std::move(scriptComponent);
    }
    if (const JsonValue* lifetime = objectField(components, "lifetime")) {
        prefab.lifetime = LifetimeComponent{
            readFloatField(*lifetime, "remainingSeconds", 5.0f)
        };
    }

    return prefab;
}

void applyEntityComponents(Scene& scene, ResourceManager& resources, Entity entity, const JsonValue& components) {
    if (const JsonValue* name = objectField(components, "name")) {
        scene.setName(entity, {readStringField(*name, "name", "Entity " + std::to_string(entity))});
    }
    if (const JsonValue* tag = objectField(components, "tag")) {
        scene.setTag(entity, {readStringField(*tag, "tag", "Untagged")});
    }
    if (const JsonValue* transform = objectField(components, "transform")) {
        scene.setTransform(entity, {
            readFloatField(*transform, "x", 0.0f),
            readFloatField(*transform, "y", 0.0f)
        });
    }
    if (const JsonValue* sprite = objectField(components, "sprite")) {
        scene.setSprite(entity, readSpriteComponent(*sprite, resources));
    }
    if (const JsonValue* animation = objectField(components, "spriteAnimation")) {
        scene.setSpriteAnimation(entity, readSpriteAnimationComponent(*animation));
    }
    if (const JsonValue* text = objectField(components, "text")) {
        scene.setText(entity, readTextComponent(*text));
    }
    if (const JsonValue* tilemap = objectField(components, "tilemap")) {
        scene.setTilemap(entity, readTilemapComponent(*tilemap, resources));
    }
    if (const JsonValue* physics = objectField(components, "physics")) {
        scene.setPhysics(entity, {
            readFloatField(*physics, "velocityX", 0.0f),
            readFloatField(*physics, "velocityY", 0.0f),
            readFloatField(*physics, "forceX", 0.0f),
            readFloatField(*physics, "forceY", 0.0f),
            readFloatField(*physics, "inverseMass", 1.0f),
            readFloatField(*physics, "drag", 0.0f),
            readFloatField(*physics, "restitution", 0.0f),
            readBoolField(*physics, "isStatic", false),
            readBoolField(*physics, "usesWorldBounds", false),
            readBoolField(*physics, "bounceAtBounds", false)
        });
    }
    if (const JsonValue* collider = objectField(components, "collider")) {
        scene.setCollider(entity, {
            readFloatField(*collider, "halfWidth", 0.10f),
            readFloatField(*collider, "halfHeight", 0.10f),
            readBoolField(*collider, "solid", true),
            readBoolField(*collider, "trigger", false),
            readUnsignedField(*collider, "layer", ColliderLayerDefault),
            readUnsignedField(*collider, "mask", ColliderMaskAll)
        });
    }
    if (const JsonValue* bounds = objectField(components, "bounds")) {
        scene.setBounds(entity, {
            readFloatField(*bounds, "minX", -1.0f),
            readFloatField(*bounds, "maxX", 1.0f),
            readFloatField(*bounds, "minY", -1.0f),
            readFloatField(*bounds, "maxY", 1.0f)
        });
    }
    if (const JsonValue* input = objectField(components, "input")) {
        scene.setInput(entity, {
            readBoolField(*input, "enabled", true),
            readStringField(*input, "moveLeftAction", "MoveLeft"),
            readStringField(*input, "moveRightAction", "MoveRight"),
            readStringField(*input, "moveUpAction", "MoveUp"),
            readStringField(*input, "moveDownAction", "MoveDown"),
            readFloatField(*input, "moveForce", 6.0f)
        });
    }
    if (const JsonValue* script = objectField(components, "script")) {
        ScriptComponent scriptComponent{
            readStringField(*script, "scriptName", ""),
            readBoolField(*script, "active", true),
            readFloatField(*script, "elapsedTime", 0.0f)
        };
        readScriptParameters(*script, scriptComponent);
        scene.setScript(entity, std::move(scriptComponent));
    }
    if (const JsonValue* lifetime = objectField(components, "lifetime")) {
        scene.setLifetime(entity, {
            readFloatField(*lifetime, "remainingSeconds", 5.0f)
        });
    }
}

std::string prefabNameFromPath(const std::string& path) {
    return std::filesystem::path(path).stem().string();
}
}

bool saveSceneToJson(const Scene& scene, const ResourceManager& resources, const std::string& path, std::string& error) {
    std::ofstream output(path);
    if (!output) {
        error = "Failed to open scene file for writing: " + path;
        return false;
    }

    const Camera& camera = scene.getCamera();

    output << std::fixed << std::setprecision(4);
    output << "{\n";
    output << "  \"camera\": {\n";
    output << "    \"x\": " << camera.x << ",\n";
    output << "    \"y\": " << camera.y << ",\n";
    output << "    \"halfWidth\": " << camera.halfWidth << ",\n";
    output << "    \"halfHeight\": " << camera.halfHeight << ",\n";
    output << "    \"target\": " << camera.target << "\n";
    output << "  },\n";
    output << "  \"playerEntity\": " << scene.getPlayerEntity() << ",\n";
    output << "  \"entities\": [\n";

    bool wroteEntity = false;
    for (Entity entity = 0; entity < scene.entityCount(); ++entity) {
        if (!scene.isValidEntity(entity)) {
            continue;
        }

        const NameComponent* name = scene.getName(entity);
        const TagComponent* tag = scene.getTag(entity);
        const TransformComponent* transform = scene.getTransform(entity);
        const PhysicsComponent* physics = scene.getPhysics(entity);
        const SpriteComponent* sprite = scene.getSprite(entity);
        const SpriteAnimationComponent* spriteAnimation = scene.getSpriteAnimation(entity);
        const TextComponent* text = scene.getText(entity);
        const TilemapComponent* tilemap = scene.getTilemap(entity);
        const ColliderComponent* collider = scene.getCollider(entity);
        const BoundsComponent* entityBounds = scene.getBounds(entity);
        const InputComponent* input = scene.getInput(entity);
        const ScriptComponent* script = scene.getScript(entity);
        const LifetimeComponent* lifetime = scene.getLifetime(entity);

        if (wroteEntity) {
            output << ",\n";
        }
        wroteEntity = true;
        output << "    {\n";
        output << "      \"id\": " << entity;

        if (name) {
            output << ",\n      \"name\": {\n";
            output << "        \"name\": \"" << escapeJson(name->name) << "\"\n";
            output << "      }";
        }

        if (tag) {
            output << ",\n      \"tag\": {\n";
            output << "        \"tag\": \"" << escapeJson(tag->tag) << "\"\n";
            output << "      }";
        }

        if (transform) {
            output << ",\n      \"transform\": {\n";
            writeFloat(output, "x", transform->x, true);
            writeFloat(output, "y", transform->y, false);
            output << "      }";
        }

        if (sprite) {
            output << ",\n      \"sprite\": {\n";
            writeSpriteFields(output, *sprite, resources);
            output << "      }";
        }

        if (spriteAnimation) {
            output << ",\n      \"spriteAnimation\": {\n";
            writeSpriteAnimationFields(output, *spriteAnimation);
            output << "      }";
        }

        if (text) {
            output << ",\n      \"text\": {\n";
            writeTextFields(output, *text);
            output << "      }";
        }

        if (tilemap) {
            output << ",\n      \"tilemap\": {\n";
            writeTilemapFields(output, *tilemap, resources);
            output << "      }";
        }

        if (physics) {
            output << ",\n      \"physics\": {\n";
            writeFloat(output, "velocityX", physics->velocityX, true);
            writeFloat(output, "velocityY", physics->velocityY, true);
            writeFloat(output, "forceX", physics->forceX, true);
            writeFloat(output, "forceY", physics->forceY, true);
            writeFloat(output, "inverseMass", physics->inverseMass, true);
            writeFloat(output, "drag", physics->drag, true);
            writeFloat(output, "restitution", physics->restitution, true);
            writeBool(output, "isStatic", physics->isStatic, true);
            writeBool(output, "usesWorldBounds", physics->usesWorldBounds, true);
            writeBool(output, "bounceAtBounds", physics->bounceAtBounds, false);
            output << "      }";
        }

        if (collider) {
            output << ",\n      \"collider\": {\n";
            writeFloat(output, "halfWidth", collider->halfWidth, true);
            writeFloat(output, "halfHeight", collider->halfHeight, true);
            writeBool(output, "solid", collider->solid, true);
            writeBool(output, "trigger", collider->trigger, true);
            writeUnsigned(output, "layer", collider->layer, true);
            writeUnsigned(output, "mask", collider->mask, false);
            output << "      }";
        }

        if (entityBounds) {
            output << ",\n      \"bounds\": {\n";
            writeFloat(output, "minX", entityBounds->minX, true);
            writeFloat(output, "maxX", entityBounds->maxX, true);
            writeFloat(output, "minY", entityBounds->minY, true);
            writeFloat(output, "maxY", entityBounds->maxY, false);
            output << "      }";
        }

        if (input) {
            output << ",\n      \"input\": {\n";
            writeBool(output, "enabled", input->enabled, true);
            output << "        \"moveLeftAction\": \"" << escapeJson(input->moveLeftAction) << "\",\n";
            output << "        \"moveRightAction\": \"" << escapeJson(input->moveRightAction) << "\",\n";
            output << "        \"moveUpAction\": \"" << escapeJson(input->moveUpAction) << "\",\n";
            output << "        \"moveDownAction\": \"" << escapeJson(input->moveDownAction) << "\",\n";
            writeFloat(output, "moveForce", input->moveForce, false);
            output << "      }";
        }

        if (script) {
            output << ",\n      \"script\": {\n";
            output << "        \"scriptName\": \"" << escapeJson(script->scriptName) << "\",\n";
            writeBool(output, "active", script->active, true);
            writeFloat(output, "elapsedTime", script->elapsedTime, !script->parameters.empty());
            if (!script->parameters.empty()) {
                writeScriptParameters(output, *script);
            }
            output << "      }";
        }

        if (lifetime) {
            output << ",\n      \"lifetime\": {\n";
            writeFloat(output, "remainingSeconds", lifetime->remainingSeconds, false);
            output << "      }";
        }

        output << "\n    }";
    }

    if (wroteEntity) {
        output << '\n';
    }
    output << "  ]\n";
    output << "}\n";
    return true;
}

bool saveEntityPrefabToJson(
    const Scene& scene,
    const ResourceManager& resources,
    Entity entity,
    const std::string& path,
    std::string& error) {
    if (!scene.isValidEntity(entity)) {
        error = "Cannot save prefab from an invalid entity.";
        return false;
    }

    std::ofstream output(path);
    if (!output) {
        error = "Failed to open prefab file for writing: " + path;
        return false;
    }

    std::string prefabName = "Entity " + std::to_string(entity);
    if (const NameComponent* name = scene.getName(entity)) {
        prefabName = name->name;
    }

    const NameComponent* name = scene.getName(entity);
    const TagComponent* tag = scene.getTag(entity);
    const TransformComponent* transform = scene.getTransform(entity);
    const PhysicsComponent* physics = scene.getPhysics(entity);
    const SpriteComponent* sprite = scene.getSprite(entity);
    const SpriteAnimationComponent* spriteAnimation = scene.getSpriteAnimation(entity);
    const TextComponent* text = scene.getText(entity);
    const TilemapComponent* tilemap = scene.getTilemap(entity);
    const ColliderComponent* collider = scene.getCollider(entity);
    const BoundsComponent* entityBounds = scene.getBounds(entity);
    const InputComponent* input = scene.getInput(entity);
    const ScriptComponent* script = scene.getScript(entity);
    const LifetimeComponent* lifetime = scene.getLifetime(entity);

    output << std::fixed << std::setprecision(4);
    output << "{\n";
    output << "  \"name\": \"" << escapeJson(prefabName) << "\",\n";
    output << "  \"sourceEntity\": " << entity << ",\n";
    output << "  \"components\": {\n";

    bool wroteComponent = false;
    auto beginComponent = [&output, &wroteComponent](const char* componentName) {
        if (wroteComponent) {
            output << ",\n";
        }
        wroteComponent = true;
        output << "    \"" << componentName << "\": {\n";
    };

    if (name) {
        beginComponent("name");
        output << "        \"name\": \"" << escapeJson(name->name) << "\"\n";
        output << "    }";
    }

    if (tag) {
        beginComponent("tag");
        output << "        \"tag\": \"" << escapeJson(tag->tag) << "\"\n";
        output << "    }";
    }

    if (transform) {
        beginComponent("transform");
        writeFloat(output, "x", transform->x, true);
        writeFloat(output, "y", transform->y, false);
        output << "    }";
    }

    if (sprite) {
        beginComponent("sprite");
        writeSpriteFields(output, *sprite, resources);
        output << "    }";
    }

    if (spriteAnimation) {
        beginComponent("spriteAnimation");
        writeSpriteAnimationFields(output, *spriteAnimation);
        output << "    }";
    }

    if (text) {
        beginComponent("text");
        writeTextFields(output, *text);
        output << "    }";
    }

    if (tilemap) {
        beginComponent("tilemap");
        writeTilemapFields(output, *tilemap, resources);
        output << "    }";
    }

    if (physics) {
        beginComponent("physics");
        writeFloat(output, "velocityX", physics->velocityX, true);
        writeFloat(output, "velocityY", physics->velocityY, true);
        writeFloat(output, "forceX", physics->forceX, true);
        writeFloat(output, "forceY", physics->forceY, true);
        writeFloat(output, "inverseMass", physics->inverseMass, true);
        writeFloat(output, "drag", physics->drag, true);
        writeFloat(output, "restitution", physics->restitution, true);
        writeBool(output, "isStatic", physics->isStatic, true);
        writeBool(output, "usesWorldBounds", physics->usesWorldBounds, true);
        writeBool(output, "bounceAtBounds", physics->bounceAtBounds, false);
        output << "    }";
    }

    if (collider) {
        beginComponent("collider");
        writeFloat(output, "halfWidth", collider->halfWidth, true);
        writeFloat(output, "halfHeight", collider->halfHeight, true);
        writeBool(output, "solid", collider->solid, true);
        writeBool(output, "trigger", collider->trigger, true);
        writeUnsigned(output, "layer", collider->layer, true);
        writeUnsigned(output, "mask", collider->mask, false);
        output << "    }";
    }

    if (entityBounds) {
        beginComponent("bounds");
        writeFloat(output, "minX", entityBounds->minX, true);
        writeFloat(output, "maxX", entityBounds->maxX, true);
        writeFloat(output, "minY", entityBounds->minY, true);
        writeFloat(output, "maxY", entityBounds->maxY, false);
        output << "    }";
    }

    if (input) {
        beginComponent("input");
        writeBool(output, "enabled", input->enabled, true);
        output << "        \"moveLeftAction\": \"" << escapeJson(input->moveLeftAction) << "\",\n";
        output << "        \"moveRightAction\": \"" << escapeJson(input->moveRightAction) << "\",\n";
        output << "        \"moveUpAction\": \"" << escapeJson(input->moveUpAction) << "\",\n";
        output << "        \"moveDownAction\": \"" << escapeJson(input->moveDownAction) << "\",\n";
        writeFloat(output, "moveForce", input->moveForce, false);
        output << "    }";
    }

    if (script) {
        beginComponent("script");
        output << "        \"scriptName\": \"" << escapeJson(script->scriptName) << "\",\n";
        writeBool(output, "active", script->active, true);
        writeFloat(output, "elapsedTime", script->elapsedTime, !script->parameters.empty());
        if (!script->parameters.empty()) {
            writeScriptParameters(output, *script);
        }
        output << "    }";
    }

    if (lifetime) {
        beginComponent("lifetime");
        writeFloat(output, "remainingSeconds", lifetime->remainingSeconds, false);
        output << "    }";
    }

    if (wroteComponent) {
        output << '\n';
    }
    output << "  }\n";
    output << "}\n";
    return true;
}

bool loadSceneFromJson(Scene& scene, ResourceManager& resources, const std::string& path, std::string& error) {
    JsonValue root;
    if (!parseJsonFile(path, root, error)) {
        return false;
    }

    const JsonValue* entities = objectField(root, "entities");
    if (!entities || entities->type != JsonValue::Type::Array) {
        error = path + ": scene is missing an entities array.";
        return false;
    }

    scene.clear();

    std::unordered_map<Entity, Entity> entityMap;
    for (const JsonValue& entityObject : entities->array) {
        if (entityObject.type != JsonValue::Type::Object) {
            continue;
        }

        const Entity oldEntity = readEntityField(entityObject, "id", entityMap.size());
        const Entity newEntity = scene.createEntity();
        entityMap[oldEntity] = newEntity;
        applyEntityComponents(scene, resources, newEntity, entityObject);
    }

    Camera camera{0.0f, 0.0f, 1.0f, 1.0f, InvalidEntity};
    if (const JsonValue* cameraObject = objectField(root, "camera")) {
        camera.x = readFloatField(*cameraObject, "x", camera.x);
        camera.y = readFloatField(*cameraObject, "y", camera.y);
        camera.halfWidth = readFloatField(*cameraObject, "halfWidth", camera.halfWidth);
        camera.halfHeight = readFloatField(*cameraObject, "halfHeight", camera.halfHeight);

        const Entity oldTarget = readEntityField(*cameraObject, "target", InvalidEntity);
        const auto mappedTarget = entityMap.find(oldTarget);
        camera.target = mappedTarget != entityMap.end() ? mappedTarget->second : InvalidEntity;
    }
    scene.setCamera(camera);

    const Entity oldPlayer = readEntityField(root, "playerEntity", InvalidEntity);
    const auto mappedPlayer = entityMap.find(oldPlayer);
    scene.setPlayerEntity(mappedPlayer != entityMap.end() ? mappedPlayer->second : InvalidEntity);

    return true;
}

bool loadPrefabFromJson(
    PrefabRegistry& prefabs,
    ResourceManager& resources,
    const std::string& path,
    std::string& error,
    std::string* loadedPrefabName) {
    JsonValue root;
    if (!parseJsonFile(path, root, error)) {
        return false;
    }

    const JsonValue* components = objectField(root, "components");
    if (!components || components->type != JsonValue::Type::Object) {
        error = path + ": prefab is missing a components object.";
        return false;
    }

    Prefab prefab = readPrefabFromComponents(root, *components, resources, prefabNameFromPath(path));
    if (prefab.name.empty()) {
        prefab.name = prefabNameFromPath(path);
    }

    if (loadedPrefabName) {
        *loadedPrefabName = prefab.name;
    }
    prefabs.registerPrefab(std::move(prefab));
    return true;
}

bool loadPrefabDirectoryFromJson(
    PrefabRegistry& prefabs,
    ResourceManager& resources,
    const std::string& directory,
    std::size_t& loadedCount,
    std::string& error) {
    loadedCount = 0;
    const std::filesystem::path root = directory;
    if (!std::filesystem::exists(root)) {
        return true;
    }

    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(root)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".json") {
            continue;
        }

        const std::string path = entry.path().generic_string();
        if (path.size() < std::string(".prefab.json").size() ||
            path.substr(path.size() - std::string(".prefab.json").size()) != ".prefab.json") {
            continue;
        }

        if (!loadPrefabFromJson(prefabs, resources, path, error)) {
            return false;
        }
        ++loadedCount;
    }

    return true;
}
