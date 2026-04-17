#pragma once

#include "Scene.h"
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

class PrefabRegistry;
class InputMap;

struct ScriptParameterDefinition {
    std::string name;
    float defaultValue;
    float minValue;
    float maxValue;
};

struct ScriptContext {
    Scene* scene = nullptr;
    PrefabRegistry* prefabs = nullptr;
    InputMap* inputMap = nullptr;
};

class ScriptSystem {
public:
    using UpdateFunction = std::function<void(ScriptContext&, Entity, ScriptComponent&, float)>;

    void clear();
    void registerScript(
        const std::string& name,
        UpdateFunction update,
        std::vector<ScriptParameterDefinition> parameters = {});
    bool hasScript(const std::string& name) const;
    const std::vector<ScriptParameterDefinition>* getParameterDefinitions(const std::string& name) const;
    void applyDefaultParameters(ScriptComponent& script) const;
    void update(Scene& scene, float deltaTime, PrefabRegistry* prefabs = nullptr, InputMap* inputMap = nullptr);
    std::vector<std::string> getScriptNames() const;
    std::size_t scriptCount() const;
    static float getFloatParameter(const ScriptComponent& script, const std::string& name, float fallback);

private:
    struct ScriptDefinition {
        UpdateFunction update;
        std::vector<ScriptParameterDefinition> parameters;
    };

    std::unordered_map<std::string, ScriptDefinition> scripts;
};
