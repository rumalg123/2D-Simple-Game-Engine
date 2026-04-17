#include "OpenGLContext.h"

#include <functional>
#include <stdexcept>
#include <string>

void OpenGLContext::attach(GLFWwindow* nextWindow) {
    std::lock_guard<std::mutex> lock(mutex);
    window = nextWindow;
    ownerThread = std::thread::id{};
    current = false;
}

void OpenGLContext::makeCurrent() {
    GLFWwindow* attachedWindow = nullptr;
    {
        std::lock_guard<std::mutex> lock(mutex);
        attachedWindow = window;
    }

    if (!attachedWindow) {
        throw std::runtime_error("No GLFW window attached to OpenGLContext");
    }

    {
        std::lock_guard<std::mutex> lock(mutex);
        if (glfwGetCurrentContext() == attachedWindow && ownerThread == std::this_thread::get_id()) {
            current = true;
            return;
        }
    }

    glfwMakeContextCurrent(attachedWindow);

    {
        std::lock_guard<std::mutex> lock(mutex);
        ownerThread = std::this_thread::get_id();
        current = true;
    }
}

void OpenGLContext::release() {
    GLFWwindow* attachedWindow = nullptr;
    {
        std::lock_guard<std::mutex> lock(mutex);
        attachedWindow = window;
    }

    if (!attachedWindow) {
        return;
    }

    if (glfwGetCurrentContext() == attachedWindow) {
        glfwMakeContextCurrent(nullptr);
    }

    std::lock_guard<std::mutex> lock(mutex);
    ownerThread = std::thread::id{};
    current = false;
}

void OpenGLContext::assertCurrent(const char* operation) const {
    std::lock_guard<std::mutex> lock(mutex);
    if (!window || glfwGetCurrentContext() != window || ownerThread != std::this_thread::get_id()) {
        throw std::runtime_error(std::string("OpenGL context is not owned by this thread during ") + operation);
    }
}

bool OpenGLContext::isAttached() const {
    std::lock_guard<std::mutex> lock(mutex);
    return window != nullptr;
}

bool OpenGLContext::isCurrentThreadOwner() const {
    std::lock_guard<std::mutex> lock(mutex);
    return window && current && glfwGetCurrentContext() == window && ownerThread == std::this_thread::get_id();
}

std::size_t OpenGLContext::ownerThreadHash() const {
    std::lock_guard<std::mutex> lock(mutex);
    return std::hash<std::thread::id>{}(ownerThread);
}

GLFWwindow* OpenGLContext::getWindow() const {
    std::lock_guard<std::mutex> lock(mutex);
    return window;
}
