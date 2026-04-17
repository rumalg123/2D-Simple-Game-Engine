#include "JobSystem.h"

#include <algorithm>

JobSystem::JobSystem(std::size_t requestedWorkers) {
    start(requestedWorkers);
}

JobSystem::~JobSystem() {
    stop();
}

void JobSystem::start(std::size_t requestedWorkers) {
    stop();

    if (requestedWorkers == 0) {
        requestedWorkers = std::thread::hardware_concurrency();
    }
    if (requestedWorkers == 0) {
        requestedWorkers = 2;
    }

    {
        std::lock_guard<std::mutex> lock(queueMutex);
        stopping = false;
    }

    workers.reserve(requestedWorkers);
    for (std::size_t index = 0; index < requestedWorkers; ++index) {
        workers.emplace_back([this]() {
            workerLoop();
        });
    }
}

void JobSystem::stop() {
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        stopping = true;
    }

    queueCondition.notify_all();

    for (std::thread& worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }

    workers.clear();
    while (!jobs.empty()) {
        jobs.pop();
    }
}

void JobSystem::parallelFor(
    std::size_t itemCount,
    std::size_t chunkSize,
    const std::function<void(std::size_t begin, std::size_t end)>& worker) {
    if (itemCount == 0) {
        return;
    }

    chunkSize = std::max<std::size_t>(1, chunkSize);
    if (!isRunning() || itemCount <= chunkSize) {
        worker(0, itemCount);
        return;
    }

    std::vector<std::future<void>> futures;
    futures.reserve((itemCount + chunkSize - 1) / chunkSize);

    for (std::size_t begin = 0; begin < itemCount; begin += chunkSize) {
        const std::size_t end = std::min(itemCount, begin + chunkSize);
        futures.push_back(submit([begin, end, &worker]() {
            worker(begin, end);
        }));
    }

    for (std::future<void>& future : futures) {
        future.get();
    }
}

bool JobSystem::isRunning() const {
    std::lock_guard<std::mutex> lock(queueMutex);
    return !stopping && !workers.empty();
}

std::size_t JobSystem::workerCount() const {
    std::lock_guard<std::mutex> lock(queueMutex);
    return workers.size();
}

void JobSystem::workerLoop() {
    while (true) {
        std::function<void()> job;

        {
            std::unique_lock<std::mutex> lock(queueMutex);
            queueCondition.wait(lock, [this]() {
                return stopping || !jobs.empty();
            });

            if (stopping && jobs.empty()) {
                return;
            }

            job = std::move(jobs.front());
            jobs.pop();
        }

        job();
    }
}
