#pragma once

#include <string>
#include <unordered_map>
#include <vector>

enum class GamepadAxisDirection {
    Negative,
    Positive
};

struct GamepadAxisBinding {
    int axis = 0;
    GamepadAxisDirection direction = GamepadAxisDirection::Positive;
    float threshold = 0.5f;
};

struct InputAction {
    std::string name;
    std::vector<int> keys;
    std::vector<int> gamepadButtons;
    std::vector<GamepadAxisBinding> gamepadAxes;
    bool down = false;
};

class InputMap {
public:
    void clear();
    void bindAction(const std::string& actionName, int key);
    void bindKey(const std::string& actionName, int key);
    void bindGamepadButton(const std::string& actionName, int button);
    void bindGamepadAxis(
        const std::string& actionName,
        int axis,
        GamepadAxisDirection direction,
        float threshold = 0.5f);
    void handleKeyChanged(int key, bool pressed);
    void clearGamepadState();
    void setGamepadButtonState(int button, bool pressed);
    void setGamepadAxisValue(int axis, float value);
    bool isDown(const std::string& actionName) const;
    bool isBoundToKey(const std::string& actionName, int key) const;
    float getAxis(const std::string& negativeAction, const std::string& positiveAction) const;
    const std::unordered_map<std::string, InputAction>& getActions() const;

private:
    InputAction& getOrCreateAction(const std::string& actionName);
    bool evaluateActionDown(const InputAction& action) const;
    void refreshActionStates();

    std::unordered_map<std::string, InputAction> actions;
    std::unordered_map<int, bool> keyStates;
    std::unordered_map<int, bool> gamepadButtonStates;
    std::unordered_map<int, float> gamepadAxisValues;
};
