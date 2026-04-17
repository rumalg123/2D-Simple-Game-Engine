#include "ScriptSystem.h"

#include <algorithm>
#include <utility>
#include <vector>

namespace {
std::vector<Entity> collectScriptEntities(Scene& scene) {
    ComponentPool<ScriptComponent>& scriptComponents = scene.getScriptPool();
    std::vector<Entity> entities;
    entities.reserve(scriptComponents.size());
    for (std::size_t index = 0; index < scriptComponents.size(); ++index) {
        entities.push_back(scriptComponents.entityAt(index));
    }
    return entities;
}
}

void ScriptSystem::clear() {
    scripts.clear();
    instances.clear();
}

void ScriptSystem::clearInstances() {
    instances.clear();
}

void ScriptSystem::registerScript(
    const std::string& name,
    UpdateFunction update,
    std::vector<ScriptParameterDefinition> parameters) {
    ScriptLifecycle lifecycle;
    lifecycle.onFixedUpdate = std::move(update);
    registerScript(name, std::move(lifecycle), std::move(parameters));
}

void ScriptSystem::registerScript(
    const std::string& name,
    ScriptLifecycle lifecycle,
    std::vector<ScriptParameterDefinition> parameters) {
    scripts[name] = {std::move(lifecycle), std::move(parameters)};
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
    ScriptContext context{&scene, prefabs, inputMap};
    syncScriptInstances(scene, context);

    const std::vector<Entity> entities = collectScriptEntities(scene);

    for (Entity entity : entities) {
        if (!scene.isValidEntity(entity)) {
            continue;
        }

        ScriptComponent* script = scene.getScript(entity);
        if (!script || !script->active) {
            continue;
        }

        applyDefaultParameters(*script);
        const std::string scriptName = script->scriptName;
        if (!ensureScriptStarted(scene, context, entity, scriptName)) {
            continue;
        }

        script = scene.getScript(entity);
        if (!script || !script->active || script->scriptName != scriptName) {
            continue;
        }

        const auto definition = scripts.find(scriptName);
        if (definition != scripts.end() && definition->second.lifecycle.onUpdate) {
            definition->second.lifecycle.onUpdate(context, entity, *script, deltaTime);
        }

        rememberLatestScript(scene, entity, scriptName);
    }
}

void ScriptSystem::fixedUpdate(Scene& scene, float deltaTime, PrefabRegistry* prefabs, InputMap* inputMap) {
    ScriptContext context{&scene, prefabs, inputMap};
    syncScriptInstances(scene, context);

    const std::vector<Entity> entities = collectScriptEntities(scene);

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
        const std::string scriptName = script->scriptName;
        if (!ensureScriptStarted(scene, context, entity, scriptName)) {
            continue;
        }

        script = scene.getScript(entity);
        if (!script || !script->active || script->scriptName != scriptName) {
            continue;
        }

        script->elapsedTime += deltaTime;

        const auto definition = scripts.find(scriptName);
        if (definition != scripts.end() && definition->second.lifecycle.onFixedUpdate) {
            definition->second.lifecycle.onFixedUpdate(context, entity, *script, deltaTime);
        }

        rememberLatestScript(scene, entity, scriptName);
    }
}

void ScriptSystem::handleEvent(Scene& scene, const Event& event, PrefabRegistry* prefabs, InputMap* inputMap) {
    ScriptContext context{&scene, prefabs, inputMap};
    syncScriptInstances(scene, context);

    const std::vector<Entity> entities = collectScriptEntities(scene);

    for (Entity entity : entities) {
        if (!scene.isValidEntity(entity)) {
            continue;
        }

        ScriptComponent* script = scene.getScript(entity);
        if (!script || !script->active) {
            continue;
        }

        applyDefaultParameters(*script);
        const std::string scriptName = script->scriptName;
        if (!ensureScriptStarted(scene, context, entity, scriptName)) {
            continue;
        }

        script = scene.getScript(entity);
        if (!script || !script->active || script->scriptName != scriptName) {
            continue;
        }

        const auto definition = scripts.find(scriptName);
        if (definition != scripts.end() && definition->second.lifecycle.onEvent) {
            definition->second.lifecycle.onEvent(context, entity, *script, event);
        }

        rememberLatestScript(scene, entity, scriptName);
    }
}

void ScriptSystem::destroyEntity(Scene& scene, Entity entity, PrefabRegistry* prefabs, InputMap* inputMap) {
    ScriptContext context{&scene, prefabs, inputMap};
    syncScriptInstances(scene, context);

    const auto state = instances.find(entity);
    if (state == instances.end()) {
        return;
    }

    invokeDestroy(context, entity, state->second);
    instances.erase(state);
}

void ScriptSystem::destroyAll(Scene& scene, PrefabRegistry* prefabs, InputMap* inputMap) {
    ScriptContext context{&scene, prefabs, inputMap};
    syncScriptInstances(scene, context);

    std::vector<Entity> entities;
    entities.reserve(instances.size());
    for (const auto& entry : instances) {
        entities.push_back(entry.first);
    }

    for (Entity entity : entities) {
        const auto state = instances.find(entity);
        if (state == instances.end()) {
            continue;
        }

        invokeDestroy(context, entity, state->second);
        instances.erase(state);
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

void ScriptSystem::syncScriptInstances(Scene& scene, ScriptContext& context) {
    const std::vector<Entity> entities = collectScriptEntities(scene);

    for (Entity entity : entities) {
        if (!scene.isValidEntity(entity)) {
            continue;
        }

        ScriptComponent* script = scene.getScript(entity);
        if (!script) {
            continue;
        }

        std::string scriptName = script->scriptName;
        const auto existing = instances.find(entity);
        if (existing != instances.end() && existing->second.scriptName == scriptName) {
            existing->second.lastScript = *script;
            continue;
        }

        if (existing != instances.end()) {
            invokeDestroy(context, entity, existing->second);
            instances.erase(existing);

            if (!scene.isValidEntity(entity)) {
                continue;
            }

            script = scene.getScript(entity);
            if (!script) {
                continue;
            }
            scriptName = script->scriptName;
        }

        applyDefaultParameters(*script);
        ScriptInstanceState state{scriptName, *script, false};
        auto [stateEntry, inserted] = instances.emplace(entity, std::move(state));
        if (!inserted) {
            stateEntry->second = {scriptName, *script, false};
        }

        const auto definition = scripts.find(scriptName);
        if (definition != scripts.end() && definition->second.lifecycle.onCreate) {
            definition->second.lifecycle.onCreate(context, entity, *script);
        }

        ScriptComponent* latestScript = scene.isValidEntity(entity) ? scene.getScript(entity) : nullptr;
        const auto latestState = instances.find(entity);
        if (!latestScript || latestScript->scriptName != scriptName) {
            if (latestState != instances.end() && latestState->second.scriptName == scriptName) {
                invokeDestroy(context, entity, latestState->second);
                instances.erase(latestState);
            }
            continue;
        }

        if (latestState != instances.end() && latestState->second.scriptName == scriptName) {
            latestState->second.lastScript = *latestScript;
        }
    }

    std::vector<Entity> removedEntities;
    for (const auto& entry : instances) {
        const ScriptComponent* script = scene.isValidEntity(entry.first) ? scene.getScript(entry.first) : nullptr;
        if (!script || script->scriptName != entry.second.scriptName) {
            removedEntities.push_back(entry.first);
        }
    }

    for (Entity entity : removedEntities) {
        const auto state = instances.find(entity);
        if (state == instances.end()) {
            continue;
        }

        invokeDestroy(context, entity, state->second);
        instances.erase(state);
    }
}

bool ScriptSystem::ensureScriptStarted(
    Scene& scene,
    ScriptContext& context,
    Entity entity,
    const std::string& scriptName) {
    const auto state = instances.find(entity);
    if (state == instances.end() || state->second.scriptName != scriptName) {
        return false;
    }

    if (state->second.started) {
        return true;
    }

    ScriptComponent* script = scene.getScript(entity);
    if (!script || !script->active || script->scriptName != scriptName) {
        return false;
    }

    const auto definition = scripts.find(scriptName);
    if (definition != scripts.end() && definition->second.lifecycle.onStart) {
        definition->second.lifecycle.onStart(context, entity, *script);
    }

    const auto latestState = instances.find(entity);
    ScriptComponent* latestScript = scene.isValidEntity(entity) ? scene.getScript(entity) : nullptr;
    if (latestState == instances.end() || latestState->second.scriptName != scriptName ||
        !latestScript || !latestScript->active || latestScript->scriptName != scriptName) {
        return false;
    }

    latestState->second.started = true;
    latestState->second.lastScript = *latestScript;
    return true;
}

void ScriptSystem::rememberLatestScript(Scene& scene, Entity entity, const std::string& scriptName) {
    const auto state = instances.find(entity);
    if (state == instances.end() || state->second.scriptName != scriptName || !scene.isValidEntity(entity)) {
        return;
    }

    const ScriptComponent* script = scene.getScript(entity);
    if (!script || script->scriptName != scriptName) {
        return;
    }

    state->second.lastScript = *script;
}

void ScriptSystem::invokeDestroy(ScriptContext& context, Entity entity, ScriptInstanceState& state) {
    const auto definition = scripts.find(state.scriptName);
    if (definition != scripts.end() && definition->second.lifecycle.onDestroy) {
        definition->second.lifecycle.onDestroy(context, entity, state.lastScript);
    }
}
