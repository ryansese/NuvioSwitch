#include "core/async/worker_pool.hpp"

#include <exception>

#include "core/log.hpp"

namespace nuvio::core::async
{

WorkerPool::WorkerPool(size_t count)
{
    if (count == 0)
        count = 1;
    threads.reserve(count);
    for (size_t i = 0; i < count; i++)
        threads.emplace_back([this] { run(); });
}

WorkerPool::~WorkerPool()
{
    shutdown();
}

void WorkerPool::post(std::function<void()> job)
{
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (stopping)
            return;
        queue.push_back(std::move(job));
    }
    wake.notify_one();
}

void WorkerPool::waitIdle()
{
    std::unique_lock<std::mutex> lock(mutex);
    idle.wait(lock, [this] { return queue.empty() && running == 0; });
}

void WorkerPool::shutdown()
{
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (stopping)
            return;
        stopping = true;
    }
    wake.notify_all();
    for (auto& thread : threads)
        if (thread.joinable())
            thread.join();
}

void WorkerPool::run()
{
    while (true)
    {
        std::function<void()> job;
        {
            std::unique_lock<std::mutex> lock(mutex);
            wake.wait(lock, [this] { return stopping || !queue.empty(); });
            if (queue.empty())
                return; // stopping, and the queue is drained
            job = std::move(queue.front());
            queue.pop_front();
            running++;
        }

        try
        {
            job();
        }
        catch (const std::exception& e)
        {
            log::error(std::string("WorkerPool: job threw: ") + e.what());
        }

        {
            std::lock_guard<std::mutex> lock(mutex);
            running--;
        }
        idle.notify_all();
    }
}

} // namespace nuvio::core::async
