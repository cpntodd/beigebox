// engine/src/core/job_system.cpp
// ─────────────────────────────────────────────────────────────
// Job system implementation.
// ─────────────────────────────────────────────────────────────

#include "beigebox/core/job_system.h"
#include <SDL2/SDL.h>
#include <algorithm>

namespace beigebox {

JobSystem::JobSystem()
{
}

JobSystem::~JobSystem()
{
    Shutdown();
}

void JobSystem::Init(int threadCount)
{
    if (threadCount <= 0)
    {
        int hw = static_cast<int>(std::thread::hardware_concurrency());
        threadCount = std::max(1, hw - 1); // leave 1 core for main/render
    }

    running_ = true;
    activeJobs_ = 0;

    for (int i = 0; i < threadCount; ++i)
    {
        workers_.emplace_back(&JobSystem::WorkerLoop, this);
    }

    SDL_Log("JobSystem: %d worker threads started (hw concurrency: %d)",
        threadCount, static_cast<int>(std::thread::hardware_concurrency()));
}

void JobSystem::Shutdown()
{
    if (!running_) return;

    running_ = false;
    cv_.notify_all();

    for (auto& t : workers_)
    {
        if (t.joinable()) t.join();
    }
    workers_.clear();

    SDL_Log("JobSystem: shut down");
}

void JobSystem::Enqueue(Job job)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(std::move(job));
        activeJobs_++;
    }
    cv_.notify_one();
}

void JobSystem::RunParallel(const std::vector<Job>& jobs)
{
    if (jobs.empty()) return;

    if (workers_.empty())
    {
        // Single-threaded fallback
        for (auto& job : jobs) job();
        return;
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& job : jobs)
        {
            queue_.push(job);
            activeJobs_++;
        }
    }
    cv_.notify_all();
    WaitAll();
}

void JobSystem::WaitAll()
{
    if (workers_.empty()) return;

    std::unique_lock<std::mutex> lock(mutex_);
    doneCv_.wait(lock, [this]() { return activeJobs_ == 0; });
}

void JobSystem::WorkerLoop()
{
    while (running_)
    {
        Job job;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this]() {
                return !queue_.empty() || !running_;
            });

            if (!running_ && queue_.empty()) return;

            job = std::move(queue_.front());
            queue_.pop();
        }

        // Execute the job
        job();

        // Signal completion
        {
            std::lock_guard<std::mutex> lock(mutex_);
            activeJobs_--;
        }
        doneCv_.notify_one();
    }
}

} // namespace beigebox
