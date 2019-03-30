#pragma once
#include <vector>
#include <mutex>
#include <thread>
#include "crt-expr.hpp"




//=============================================================================
namespace crt {
    class worker_pool;
}




//=============================================================================
class crt::worker_pool
{
public:


    using product_t = crt::expression;
    using run_t = std::function<product_t(const std::atomic<bool>* status)>;


    class listener_t
    {
    public:
        virtual void task_starting(int worker, std::string name) = 0;
        virtual void task_canceled(int worker, std::string name) = 0;
        virtual void task_finished(int worker, std::string name, product_t result) = 0;
    };


    struct task_t
    {
        task_t(std::string name, run_t run) : name(name), run(run)
        {
            canceled = std::make_shared<std::atomic<bool>>(false);
        }

        operator bool() const { return run != nullptr; }
        std::string name;
        std::shared_ptr<std::atomic<bool>> canceled;
        run_t run = nullptr;
    };


    worker_pool(int num_workers=4, listener_t* listener=nullptr) : listener(listener)
    {
        for (int n = 0; n < num_workers; ++n)
        {
            threads.push_back(make_worker(n));
        }
    }


    ~worker_pool()
    {
        stop_all();
    }


    void stop_all()
    {
        if (! stop)
        {
            stop = true;
            condition.notify_all();

            for (auto& thread : threads)
            {
                thread.join();
            }
        }
    }


    void enqueue(std::string name, run_t task)
    {
        cancel(name);

        std::lock_guard<std::mutex> lock(mutex);
        pending_tasks.push_back({name, task});
        condition.notify_one();
    }


    bool is_running(std::string name)
    {
        std::lock_guard<std::mutex> lock(mutex);
        return named(name, running_tasks) != running_tasks.end();
    }


    bool is_pending(std::string name)
    {
        std::lock_guard<std::mutex> lock(mutex);
        return named(name, pending_tasks) != pending_tasks.end();
    }


    bool is_submitted(std::string name)
    {
        return is_running(name) || is_pending(name);
    }


    void cancel(std::string name)
    {
        std::lock_guard<std::mutex> lock(mutex);
        auto running = named(name, running_tasks);
        auto pending = named(name, pending_tasks);

        if (running != running_tasks.end())
        {
            *running->canceled = true;
        }
        else if (pending != pending_tasks.end())
        {
            pending_tasks.erase(pending);
        }
    }


private:


    /**
     * Convenience method to find a task with the given name.
     */
    std::vector<task_t>::iterator named(std::string name, std::vector<task_t>& v)
    {
        return std::find_if(v.begin(), v.end(), [name] (auto& t) { return t.name == name; });
    }


    /**
     * Called by other threads to await the next available task. Pops that
     * task from the queue and returns it. If there was no task, then nullptr
     * is returned. That should only be the case when the pool is shutting
     * down.
     */
    task_t next(int id)
    {
        std::unique_lock<std::mutex> lock(mutex);
        condition.wait(lock, [this] { return stop || ! pending_tasks.empty(); });

        if (pending_tasks.empty())
        {
            return {std::string(), nullptr};
        }

        auto task = pending_tasks.front();
        pending_tasks.erase(pending_tasks.begin());
        running_tasks.push_back(task);

        if (listener)
        {
            listener->task_starting(id, task.name);
        }
        return task;
    }


    /**
     * Called by other threads to indicate they have finished a task.
     */
    void complete(std::string name, int id, product_t result)
    {
        std::lock_guard<std::mutex> lock(mutex);

        auto task = named(name, running_tasks);

        if (listener)
        {
            if (*task->canceled)
            {
                listener->task_canceled(id, name);
            }
            else
            {
                listener->task_finished(id, name, result);
            }
        }
        running_tasks.erase(task);
    }


    /**
     * Called by the constructor to create the workers.
     */
    std::thread make_worker(int id)
    {
        return std::thread([this, id] ()
        {
            while (auto task = next(id))
            {
                complete(task.name, id, task.run(task.canceled.get()));
            }
        });
    }


    std::vector<std::thread> threads;
    std::vector<task_t> pending_tasks;
    std::vector<task_t> running_tasks;
    std::condition_variable condition;
    std::atomic<bool> stop = {false};
    std::mutex mutex;
    listener_t* listener = nullptr;
};
