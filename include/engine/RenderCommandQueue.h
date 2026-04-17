#pragma once

#include "OpenGLContext.h"

#include <cstddef>
#include <condition_variable>
#include <exception>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class RenderCommandQueue {
public:
    using Command = std::function<void()>;

    void setContext(OpenGLContext* nextContext);
    void start();
    void stop();
    void enqueue(std::string label, Command command);
    void execute();
    void submitFrame();
    void waitUntilIdle();
    void submitFrameAndWait();
    void clear();

    bool isRunning() const;
    std::size_t queuedCommandCount() const;
    std::size_t lastExecutedCommandCount() const;
    std::size_t totalExecutedCommandCount() const;
    std::size_t submittedFrameCount() const;
    std::size_t renderThreadHash() const;

private:
    struct RenderCommand {
        std::string label;
        Command command;
    };

    void renderThreadLoop();
    std::size_t executeQueuedCommands();

    OpenGLContext* context = nullptr;
    mutable std::mutex mutex;
    std::condition_variable frameRequestedCondition;
    std::condition_variable frameCompletedCondition;
    std::vector<RenderCommand> commands;
    std::thread renderThread;
    std::thread::id renderThreadId;
    bool running = false;
    bool stopping = false;
    bool frameRequested = false;
    bool frameExecuting = false;
    std::exception_ptr lastException;
    std::size_t submittedFrames = 0;
    std::size_t lastExecuted = 0;
    std::size_t totalExecuted = 0;
};
