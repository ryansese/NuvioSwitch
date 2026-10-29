#pragma once

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace nuvio::core::async
{

// A fixed set of threads running jobs from one FIFO queue.
// borealis's own brls::async has a single thread, too few for parallel requests.
class WorkerPool
{
  public:
    explicit WorkerPool(size_t threads);
    ~WorkerPool(); // shutdown()

    WorkerPool(const WorkerPool&)            = delete;
    WorkerPool& operator=(const WorkerPool&) = delete;

    // Queues a job. Jobs posted after shutdown() are dropped.
    void post(std::function<void()> job);

    // Blocks until the queue is empty and no job is running.
    void waitIdle();

    // Runs the jobs already queued, then joins the threads.
    void shutdown();

    size_t size() const { return threads.size(); }

  private:
    void run();

    std::vector<std::thread> threads;
    std::deque<std::function<void()>> queue;
    std::mutex mutex;
    std::condition_variable wake;
    std::condition_variable idle;
    size_t running = 0;
    bool stopping  = false;
};

} // namespace nuvio::core::async
