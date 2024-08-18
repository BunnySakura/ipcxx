#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <vector>
#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <stdexcept>

class ThreadPool final {
  public:
    explicit ThreadPool(size_t threads = std::thread::hardware_concurrency());

    template<class F, class... Args>
    decltype(auto) enqueue(F &&f, Args &&... args);

    ~ThreadPool();

  private:
    // need to keep track of threads so we can join them
    std::vector<std::thread> workers;
    // the task queue
    std::queue<std::function<void()>> tasks;

    // synchronization
    std::mutex queue_mutex;
    std::condition_variable condition;
    bool stop;
};

// the constructor just launches some amount of workers
inline ThreadPool::ThreadPool(const size_t threads)
  : stop(false) {
  for (size_t i = 0; i < threads; ++i)
    workers.emplace_back(
      [this] {
        for (;;) {
          std::function<void()> task;

          do {
            std::unique_lock lock(this->queue_mutex);
            this->condition.wait(
              lock,
              [this] { return this->stop || !this->tasks.empty(); }
            );
            if (this->stop && this->tasks.empty()) return;
            task = std::move(this->tasks.front());
            this->tasks.pop();
          } while (false);

          task();
        }
      }
    );
}

// add new work item to the pool
template<class F, class... Args>
decltype(auto) ThreadPool::enqueue(F &&f, Args &&... args) {
  using return_type = decltype(f(args...));

  auto task = std::make_shared<std::packaged_task<return_type()>>(
    std::bind(std::forward<F>(f), std::forward<Args>(args)...)
  );

  std::future<return_type> res = task->get_future();

  do {
    std::unique_lock lock(queue_mutex);

    // don't allow enqueueing after stopping the pool
    if (stop)
      throw std::runtime_error("enqueue on stopped ThreadPool");

    tasks.emplace([task] { (*task)(); });
  } while (false);

  condition.notify_one();
  return res;
}

// the destructor joins all threads
inline ThreadPool::~ThreadPool() { {
    std::unique_lock lock(queue_mutex);
    stop = true;
  }
  condition.notify_all();
  for (std::thread &worker : workers)
    worker.join();
}

#endif
