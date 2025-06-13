#pragma once
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <atomic>

class ThreadPool {
public:
    explicit ThreadPool(size_t threadCount);
    ~ThreadPool();

    // Submit a task, returning a future
    template<typename F, typename... Args>
    auto submit(F&& f, Args&&... args)
        -> std::future<std::invoke_result_t<F, Args...>>;
    // Trailing return type deduction;

    int getAvailableThreads() const {
        return availableThreads.load(std::memory_order_relaxed);
    }

private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;

    std::mutex queueMutex;
    std::condition_variable condition;
    std::atomic<bool> stop;
    std::atomic<int> availableThreads;

    void workerLoop();
};

// template definition to prevent linking error

template<typename F, typename... Args>
auto ThreadPool::submit(F&& f, Args&&... args)
-> std::future<std::invoke_result_t<F, Args...>> {

    using return_type = std::invoke_result_t<F, Args...>;

    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );

    std::future<return_type> res = task->get_future();
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        if (stop) throw std::runtime_error("ThreadPool is stopped");
        tasks.emplace([task]() { (*task)(); });
    }
    condition.notify_one();
    return res;
}


