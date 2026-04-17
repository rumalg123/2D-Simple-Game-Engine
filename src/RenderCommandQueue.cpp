#include "RenderCommandQueue.h"

#include <functional>
#include <stdexcept>
#include <utility>

void RenderCommandQueue::setContext(OpenGLContext* nextContext) {
    context = nextContext;
}

void RenderCommandQueue::start() {
    if (!context) {
        throw std::runtime_error("RenderCommandQueue has no OpenGL context");
    }

    stop();

    {
        std::lock_guard<std::mutex> lock(mutex);
        stopping = false;
        frameRequested = false;
        frameExecuting = false;
        running = true;
    }

    renderThread = std::thread([this]() {
        renderThreadLoop();
    });
}

void RenderCommandQueue::stop() {
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (!running && !renderThread.joinable()) {
            return;
        }

        stopping = true;
        frameRequested = true;
    }

    frameRequestedCondition.notify_all();

    if (renderThread.joinable()) {
        renderThread.join();
    }

    std::lock_guard<std::mutex> lock(mutex);
    running = false;
    stopping = false;
    frameRequested = false;
    frameExecuting = false;
    renderThreadId = std::thread::id{};
}

void RenderCommandQueue::enqueue(std::string label, Command command) {
    if (!command) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex);
    commands.push_back({std::move(label), std::move(command)});
}

void RenderCommandQueue::execute() {
    if (!context) {
        throw std::runtime_error("RenderCommandQueue has no OpenGL context");
    }

    context->makeCurrent();
    context->assertCurrent("render command queue execution");
    executeQueuedCommands();
}

void RenderCommandQueue::submitFrame() {
    {
        std::unique_lock<std::mutex> lock(mutex);
        if (!running) {
            lock.unlock();
            execute();
            return;
        }

        frameRequested = true;
        ++submittedFrames;
    }

    frameRequestedCondition.notify_one();
}

void RenderCommandQueue::waitUntilIdle() {
    std::unique_lock<std::mutex> lock(mutex);
    frameCompletedCondition.wait(lock, [this]() {
        return !frameRequested && !frameExecuting;
    });

    if (lastException) {
        std::exception_ptr exception = lastException;
        lastException = nullptr;
        lock.unlock();
        std::rethrow_exception(exception);
    }
}

void RenderCommandQueue::submitFrameAndWait() {
    submitFrame();
    waitUntilIdle();
}

void RenderCommandQueue::clear() {
    std::lock_guard<std::mutex> lock(mutex);
    commands.clear();
    lastExecuted = 0;
}

bool RenderCommandQueue::isRunning() const {
    std::lock_guard<std::mutex> lock(mutex);
    return running;
}

std::size_t RenderCommandQueue::queuedCommandCount() const {
    std::lock_guard<std::mutex> lock(mutex);
    return commands.size();
}

std::size_t RenderCommandQueue::lastExecutedCommandCount() const {
    std::lock_guard<std::mutex> lock(mutex);
    return lastExecuted;
}

std::size_t RenderCommandQueue::totalExecutedCommandCount() const {
    std::lock_guard<std::mutex> lock(mutex);
    return totalExecuted;
}

std::size_t RenderCommandQueue::submittedFrameCount() const {
    std::lock_guard<std::mutex> lock(mutex);
    return submittedFrames;
}

std::size_t RenderCommandQueue::renderThreadHash() const {
    std::lock_guard<std::mutex> lock(mutex);
    return std::hash<std::thread::id>{}(renderThreadId);
}

void RenderCommandQueue::renderThreadLoop() {
    context->makeCurrent();

    {
        std::lock_guard<std::mutex> lock(mutex);
        renderThreadId = std::this_thread::get_id();
    }

    while (true) {
        {
            std::unique_lock<std::mutex> lock(mutex);
            frameRequestedCondition.wait(lock, [this]() {
                return stopping || frameRequested;
            });

            if (stopping) {
                break;
            }

            frameRequested = false;
            frameExecuting = true;
        }

        try {
            executeQueuedCommands();
        } catch (...) {
            std::lock_guard<std::mutex> lock(mutex);
            lastException = std::current_exception();
        }

        {
            std::lock_guard<std::mutex> lock(mutex);
            frameExecuting = false;
        }
        frameCompletedCondition.notify_all();
    }

    try {
        executeQueuedCommands();
    } catch (...) {
        std::lock_guard<std::mutex> lock(mutex);
        lastException = std::current_exception();
    }
    context->release();

    {
        std::lock_guard<std::mutex> lock(mutex);
        frameRequested = false;
        frameExecuting = false;
    }
    frameCompletedCondition.notify_all();
}

std::size_t RenderCommandQueue::executeQueuedCommands() {
    std::vector<RenderCommand> localCommands;
    {
        std::lock_guard<std::mutex> lock(mutex);
        localCommands.swap(commands);
    }

    for (RenderCommand& command : localCommands) {
        context->assertCurrent(command.label.c_str());
        command.command();
    }

    {
        std::lock_guard<std::mutex> lock(mutex);
        lastExecuted = localCommands.size();
        totalExecuted += localCommands.size();
    }

    return localCommands.size();
}
