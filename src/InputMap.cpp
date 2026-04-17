#include "InputMap.h"

#include <algorithm>
#include <cmath>

void InputMap::clear() {
    actions.clear();
    keyStates.clear();
    gamepadButtonStates.clear();
    gamepadAxisValues.clear();
}

void InputMap::bindAction(const std::string& actionName, int key) {
    bindKey(actionName, key);
}

void InputMap::bindKey(const std::string& actionName, int key) {
    InputAction& action = getOrCreateAction(actionName);
    if (std::find(action.keys.begin(), action.keys.end(), key) == action.keys.end()) {
        action.keys.push_back(key);
    }
    action.down = evaluateActionDown(action);
}

void InputMap::bindGamepadButton(const std::string& actionName, int button) {
    InputAction& action = getOrCreateAction(actionName);
    if (std::find(action.gamepadButtons.begin(), action.gamepadButtons.end(), button) == action.gamepadButtons.end()) {
        action.gamepadButtons.push_back(button);
    }
    action.down = evaluateActionDown(action);
}

void InputMap::bindGamepadAxis(
    const std::string& actionName,
    int axis,
    GamepadAxisDirection direction,
    float threshold) {
    InputAction& action = getOrCreateAction(actionName);
    const auto existing = std::find_if(
        action.gamepadAxes.begin(),
        action.gamepadAxes.end(),
        [axis, direction](const GamepadAxisBinding& binding) {
            return binding.axis == axis && binding.direction == direction;
        });

    if (existing == action.gamepadAxes.end()) {
        action.gamepadAxes.push_back({axis, direction, threshold});
    } else {
        existing->threshold = threshold;
    }
    action.down = evaluateActionDown(action);
}

void InputMap::handleKeyChanged(int key, bool pressed) {
    keyStates[key] = pressed;
    refreshActionStates();
}

void InputMap::clearGamepadState() {
    gamepadButtonStates.clear();
    gamepadAxisValues.clear();
    refreshActionStates();
}

void InputMap::setGamepadButtonState(int button, bool pressed) {
    gamepadButtonStates[button] = pressed;
    refreshActionStates();
}

void InputMap::setGamepadAxisValue(int axis, float value) {
    gamepadAxisValues[axis] = value;
    refreshActionStates();
}

bool InputMap::isDown(const std::string& actionName) const {
    const auto action = actions.find(actionName);
    return action != actions.end() && evaluateActionDown(action->second);
}

bool InputMap::isBoundToKey(const std::string& actionName, int key) const {
    const auto action = actions.find(actionName);
    return action != actions.end() &&
           std::find(action->second.keys.begin(), action->second.keys.end(), key) != action->second.keys.end();
}

float InputMap::getAxis(const std::string& negativeAction, const std::string& positiveAction) const {
    float value = 0.0f;
    if (isDown(positiveAction)) {
        value += 1.0f;
    }
    if (isDown(negativeAction)) {
        value -= 1.0f;
    }

    return value;
}

const std::unordered_map<std::string, InputAction>& InputMap::getActions() const {
    return actions;
}

InputAction& InputMap::getOrCreateAction(const std::string& actionName) {
    InputAction& action = actions[actionName];
    if (action.name.empty()) {
        action.name = actionName;
    }
    return action;
}

bool InputMap::evaluateActionDown(const InputAction& action) const {
    for (int key : action.keys) {
        const auto state = keyStates.find(key);
        if (state != keyStates.end() && state->second) {
            return true;
        }
    }

    for (int button : action.gamepadButtons) {
        const auto state = gamepadButtonStates.find(button);
        if (state != gamepadButtonStates.end() && state->second) {
            return true;
        }
    }

    for (const GamepadAxisBinding& binding : action.gamepadAxes) {
        const auto state = gamepadAxisValues.find(binding.axis);
        if (state == gamepadAxisValues.end()) {
            continue;
        }

        const float threshold = std::max(0.0f, std::fabs(binding.threshold));
        if (binding.direction == GamepadAxisDirection::Positive && state->second >= threshold) {
            return true;
        }
        if (binding.direction == GamepadAxisDirection::Negative && state->second <= -threshold) {
            return true;
        }
    }

    return false;
}

void InputMap::refreshActionStates() {
    for (auto& action : actions) {
        action.second.down = evaluateActionDown(action.second);
    }
}
