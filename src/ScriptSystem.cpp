#include "ScriptSystem.h"

#include <algorithm>
#include <utility>
#include <vector>

void ScriptSystem::clear() {
    scripts.clear();
}

void ScriptSystem::registerScript(
    const std::string& name,
    UpdateFunction update,
    std::vector<ScriptParameterDefinition> parameters) {
    scripts[name] = {update, std::move(parameters)};
}

bool ScriptSystem::hasScript(const std::string& name) const {
    return scripts.find(name) != scripts.end();
}

const std::vector<ScriptParameterDefinition>* ScriptSystem::getParameterDefinitions(const std::string& name) const {
    const auto script = scripts.find(name);
    return script != scripts.end() ? &script->second.parameters : nullptr;
}

void ScriptSystem::applyDefaultParameters(ScriptComponent& script) const {
    const std::vector<ScriptParameterDefinition>* definitions = getParameterDefinitions(script.scriptName);
    if (!definitions) {
        return;
    }

    for (const ScriptParameterDefinition& definition : *definitions) {
        if (script.parameters.find(definition.name) == script.parameters.end()) {
            script.parameters[definition.name] = definition.defaultValue;
        }
    }
}

void ScriptSystem::update(Scene& scene, float deltaTime, PrefabRegistry* prefabs, InputMap* inputMap) {
    ComponentPool<ScriptComponent>& scriptComponents = scene.getScriptPool();
    ScriptContext context{&scene, prefabs, inputMap};

    std::vector<Entity> entities;
    entities.reserve(scriptComponents.size());
    for (std::size_t index = 0; index < scriptComponents.size(); ++index) {
        entities.push_back(scriptComponents.entityAt(index));
    }

    for (Entity entity : entities) {
        if (!scene.isValidEntity(entity)) {
            continue;
        }

        ScriptComponent* script = scene.getScript(entity);
        if (!script) {
            continue;
        }

        if (!script->active) {
            continue;
        }

        applyDefaultParameters(*script);
        script->elapsedTime += deltaTime;

        const auto scriptUpdate = scripts.find(script->scriptName);
        if (scriptUpdate != scripts.end()) {
            scriptUpdate->second.update(context, entity, *script, deltaTime);
        }
    }
}

std::vector<std::string> ScriptSystem::getScriptNames() const {
    std::vector<std::string> names;
    names.reserve(scripts.size());
    for (const auto& entry : scripts) {
        names.push_back(entry.first);
    }

    std::sort(names.begin(), names.end());
    return names;
}

std::size_t ScriptSystem::scriptCount() const {
    return scripts.size();
}

float ScriptSystem::getFloatParameter(const ScriptComponent& script, const std::string& name, float fallback) {
    const auto parameter = script.parameters.find(name);
    return parameter != script.parameters.end() ? parameter->second : fallback;
}
