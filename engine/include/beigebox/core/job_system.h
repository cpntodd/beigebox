// engine/include/beigebox/core/job_system.h
// ─────────────────────────────────────────────────────────────
// Simple thread-pool job system for multi-core parallelism.
//
// Design:
//   - N worker threads (default: std::thread::hardware_concurrency - 1,
//     leaving one core for the main/render thread)
//   - Mutex + condition_variable for task dispatch (simple, not
//     lock-free — fine for 30Hz game loop with batch dispatch)
//   - main thread dispatches N jobs → workers execute → main waits
//   - Single-threaded fallback if threads unavailable (potato PC)
//
// Target baseline: x86-64, C++17 std::thread, no external deps.
// ─────────────────────────────────────────────────────────────
#pragma once

#include <functional>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <queue>

namespace beigebox {

class JobSystem
{
public:
    using Job = std::function<void()>;

    JobSystem();
    ~JobSystem();

    // Non-copyable (owns threads)
    JobSystem(const JobSystem&) = delete;
    JobSystem& operator=(const JobSystem&) = delete;

    // ── Configuration ────────────────────────────────────────
    // Start worker threads. Call once at engine init.
    // threadCount: 0 = auto (hardware_concurrency - 1)
    void Init(int threadCount = 0);

    // Stop all workers. Call at engine shutdown.
    void Shutdown();

    // Number of active worker threads.
    int WorkerCount() const { return static_cast<int>(workers_.size()); }

    // ── Job Dispatch ─────────────────────────────────────────
    // Push a job onto the queue. Non-blocking.
    void Enqueue(Job job);

    // Push N jobs and wait for all to complete.
    // Used for parallel system execution: split work into chunks,
    // dispatch each chunk, wait for completion.
    void RunParallel(const std::vector<Job>& jobs);

    // Wait for all currently queued jobs to complete.
    void WaitAll();

    // True if using single-threaded fallback.
    bool IsSingleThreaded() const { return workers_.empty(); }

private:
    void WorkerLoop();

    std::vector<std::thread> workers_;
    std::queue<Job>          queue_;
    std::mutex               mutex_;
    std::condition_variable  cv_;
    std::condition_variable  doneCv_;
    std::atomic<bool>        running_{false};
    std::atomic<int>         activeJobs_{0};
};

} // namespace beigebox
