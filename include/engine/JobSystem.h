#pragma once

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <type_traits>
#include <vector>

class JobSystem {
public:
    JobSystem() = default;
    explicit JobSystem(std::size_t requestedWorkers);
    ~JobSystem();

    JobSystem(const JobSystem&) = delete;
    JobSystem& operator=(const JobSystem&) = delete;

    void start(std::size_t requestedWorkers = 0);
    void stop();

    template <typename Function>
    auto submit(Function&& function) -> std::future<std::invoke_result_t<std::decay_t<Function>&>> {
        using Job = std::decay_t<Function>;
        using ReturnType = std::invoke_result_t<Job&>;

        auto task = std::make_shared<std::packaged_task<ReturnType()>>(std::forward<Function>(function));
        std::future<ReturnType> future = task->get_future();
        bool runImmediately = false;

        {
            std::lock_guard<std::mutex> lock(queueMutex);
            if (stopping || workers.empty()) {
                runImmediately = true;
            } else {
                jobs.emplace([task]() {
                    (*task)();
                });
            }
        }

        if (runImmediately) {
            (*task)();
        } else {
            queueCondition.notify_one();
        }

        return future;
    }

    void parallelFor(
        std::size_t itemCount,
        std::size_t chunkSize,
        const std::function<void(std::size_t begin, std::size_t end)>& worker);

    bool isRunning() const;
    std::size_t workerCount() const;

private:
    void workerLoop();

    mutable std::mutex queueMutex;
    std::condition_variable queueCondition;
    std::queue<std::function<void()>> jobs;
    std::vector<std::thread> workers;
    bool stopping = false;
};
