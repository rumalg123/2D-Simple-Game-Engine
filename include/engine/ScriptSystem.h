#pragma once

#include "Event.h"
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
    using LifecycleFunction = std::function<void(ScriptContext&, Entity, ScriptComponent&)>;
    using EventFunction = std::function<void(ScriptContext&, Entity, ScriptComponent&, const Event&)>;

    struct ScriptLifecycle {
        LifecycleFunction onCreate;
        LifecycleFunction onStart;
        UpdateFunction onUpdate;
        UpdateFunction onFixedUpdate;
        LifecycleFunction onDestroy;
        EventFunction onEvent;
    };

    void clear();
    void clearInstances();
    void registerScript(
        const std::string& name,
        UpdateFunction update,
        std::vector<ScriptParameterDefinition> parameters = {});
    void registerScript(
        const std::string& name,
        ScriptLifecycle lifecycle,
        std::vector<ScriptParameterDefinition> parameters = {});
    bool hasScript(const std::string& name) const;
    const std::vector<ScriptParameterDefinition>* getParameterDefinitions(const std::string& name) const;
    void applyDefaultParameters(ScriptComponent& script) const;
    void update(Scene& scene, float deltaTime, PrefabRegistry* prefabs = nullptr, InputMap* inputMap = nullptr);
    void fixedUpdate(Scene& scene, float deltaTime, PrefabRegistry* prefabs = nullptr, InputMap* inputMap = nullptr);
    void handleEvent(Scene& scene, const Event& event, PrefabRegistry* prefabs = nullptr, InputMap* inputMap = nullptr);
    void destroyEntity(Scene& scene, Entity entity, PrefabRegistry* prefabs = nullptr, InputMap* inputMap = nullptr);
    void destroyAll(Scene& scene, PrefabRegistry* prefabs = nullptr, InputMap* inputMap = nullptr);
    std::vector<std::string> getScriptNames() const;
    std::size_t scriptCount() const;
    static float getFloatParameter(const ScriptComponent& script, const std::string& name, float fallback);

private:
    struct ScriptDefinition {
        ScriptLifecycle lifecycle;
        std::vector<ScriptParameterDefinition> parameters;
    };

    struct ScriptInstanceState {
        std::string scriptName;
        ScriptComponent lastScript;
        bool started = false;
    };

    void syncScriptInstances(Scene& scene, ScriptContext& context);
    bool ensureScriptStarted(Scene& scene, ScriptContext& context, Entity entity, const std::string& scriptName);
    void rememberLatestScript(Scene& scene, Entity entity, const std::string& scriptName);
    void invokeDestroy(ScriptContext& context, Entity entity, ScriptInstanceState& state);

    std::unordered_map<std::string, ScriptDefinition> scripts;
    std::unordered_map<Entity, ScriptInstanceState> instances;
};
