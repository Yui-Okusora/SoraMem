#include "ThreadPool.hpp"

ThreadPool::ThreadPool(size_t threadCount) : stop(false), availableThreads(0) {
    for (size_t i = 0; i < threadCount; ++i) {
        workers.emplace_back([this]() { workerLoop(); });
    }
}

ThreadPool::~ThreadPool() {
    stop = true;
    condition.notify_all();
    for (std::thread& worker : workers) {
        if (worker.joinable()) worker.join();
    }
}

void ThreadPool::workerLoop() {
    while (!stop) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(queueMutex);

            availableThreads.fetch_add(1, std::memory_order_relaxed);
            condition.wait(lock, [this]() { return stop || !tasks.empty(); });
            availableThreads.fetch_sub(1, std::memory_order_relaxed);

            if (stop && tasks.empty()) return;
            task = std::move(tasks.front());
            tasks.pop();
        }
        task();
    }
}

