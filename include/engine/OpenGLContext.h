#pragma once

#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>

#include <cstddef>
#include <mutex>
#include <thread>

class OpenGLContext {
public:
    void attach(GLFWwindow* nextWindow);
    void makeCurrent();
    void release();
    void assertCurrent(const char* operation) const;

    bool isAttached() const;
    bool isCurrentThreadOwner() const;
    std::size_t ownerThreadHash() const;
    GLFWwindow* getWindow() const;

private:
    mutable std::mutex mutex;
    GLFWwindow* window = nullptr;
    std::thread::id ownerThread;
    bool current = false;
};
