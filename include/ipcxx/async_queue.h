#ifndef ASYNC_QUEUE_H
#define ASYNC_QUEUE_H

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <queue>

namespace ipcxx {
template<class T, class Container = std::deque<T>>
class AsyncQueue final {
  using container_type = Container;
  using value_type = typename Container::value_type;
  using size_type = typename Container::size_type;
  using reference = typename Container::reference;
  using const_reference = typename Container::const_reference;

  public:
    AsyncQueue() = default;

    explicit AsyncQueue(const size_t max_size): mQueueMaxSize(max_size) {}

    /**
     * \brief 拷贝构造
     * \param other 其他同类对象
     */
    AsyncQueue(const AsyncQueue &other) {
      std::lock_guard lock(other.mQueueMutex);
      mQueue = other.mQueue;
      mQueueMaxSize = other.mQueueMaxSize;
    }

    /**
     * \brief 移动构造
     * \param other 其他同类对象
     */
    AsyncQueue(AsyncQueue &&other) noexcept {
      mQueue = std::move(other.mQueue);
      mQueueMaxSize = other.mQueueMaxSize;
    }

    ~AsyncQueue() = default;

    /**
     * \brief 拷贝赋值运算符重载
     * \param other 其他同类对象
     */
    AsyncQueue &operator=(const AsyncQueue &other) {
      std::scoped_lock lock(mQueueMutex, other.mQueueMutex); // 避免死锁
      if (this != &other) {
        std::queue<T, Container> temp{other.mQueue};
        mQueue.swap(temp); // CAS（复制然后交换）操作确保异常安全
        mQueueMaxSize = other.mQueueMaxSize;
      }
      if (!mQueue.empty()) mQueueCond.notify_all();
      return *this;
    }

    /**
     * \brief 移动赋值运算符重载
     * \param other 其他同类对象
     */
    AsyncQueue &operator=(AsyncQueue &&other) noexcept {
      std::lock_guard lock(mQueueMutex);
      if (this != &other) {
        mQueue = std::move(other.mQueue);
        mQueueMaxSize = other.mQueueMaxSize;
      }
      if (!mQueue.empty()) mQueueCond.notify_all();
      return *this;
    }

    /**
     * \brief 使用初始化列表构造
     * \param init_list 初始化列表
     */
    AsyncQueue(std::initializer_list<T> init_list) {
      std::lock_guard lock(mQueueMutex);
      mQueue = init_list;
    }

    /**
     * \brief 设置队列最大长度
     * \param max_size 队列最大长度
     */
    void setMaxSize(const size_t max_size) {
      mQueueMaxSize = max_size;
    }

    size_type size() const {
      std::lock_guard lock(mQueueMutex);
      return mQueue.size();
    }

    [[nodiscard]] bool empty() const {
      std::lock_guard lock(mQueueMutex);
      return mQueue.empty();
    }

    void swap(AsyncQueue &other) noexcept {
      if (this != &other) {
        std::scoped_lock lock(mQueueMutex, other.mQueueMutex); // 避免死锁
        mQueue.swap(other.mQueue);

        if (!mQueue.empty())
          mQueueCond.notify_all();

        if (!other.mQueue.empty())
          other.mQueueCond.notify_all();
      }
    }

    template<typename U>
    bool push(U &&element) {
      if (mQueueMaxSize > 0 && mQueue.size() >= mQueueMaxSize)
        return false;

      // 静态断言，确保U可以安全地转换为T
      static_assert(std::is_convertible_v<U, value_type>, "Element type must be convertible to value_type.");

      // 完美转发
      {
        std::lock_guard lock(mQueueMutex);
        mQueue.push(std::forward<U>(element));
      }
      mQueueCond.notify_one();
      return true;
    }

    template<typename... Args>
    bool emplace(Args &&... args) {
      if (mQueueMaxSize > 0 && mQueue.size() >= mQueueMaxSize)
        return false;

      // 完美转发
      {
        std::lock_guard lock(mQueueMutex);
        mQueue.emplace(std::forward<Args>(args)...);
      }
      mQueueCond.notify_one();
      return true;
    }

    bool pop(value_type &element, std::chrono::milliseconds timeout) {
      std::unique_lock lock(mQueueMutex);
      if (!mQueueCond.wait_for(lock, timeout, [this]() { return !mQueue.empty(); })) {
        return false;
      }
      element = std::move(mQueue.front());
      mQueue.pop();
      return true;
    }

    void pop(value_type &element) {
      std::unique_lock lock(mQueueMutex);
      mQueueCond.wait(lock, [this]() { return !mQueue.empty(); });
      element = std::move(mQueue.front());
      mQueue.pop();
    }

  private:
    std::queue<T, Container> mQueue;
    mutable std::mutex mQueueMutex;
    std::condition_variable mQueueCond;
    std::atomic_size_t mQueueMaxSize{0}; // 队列最大长度，超出则丢弃，0表示不限制
};

template<class T, class Container>
void swap(AsyncQueue<T, Container> &q1, AsyncQueue<T, Container> &q2) noexcept {
  q1.swap(q2);
}
}

#endif //ASYNC_QUEUE_H
