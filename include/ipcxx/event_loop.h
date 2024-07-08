#ifndef EVENT_LOOP_H
#define EVENT_LOOP_H

#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <optional>
#include <queue>
#include <shared_mutex>
#include <thread>

namespace ipcxx {
class Event final {
  using EventHandler = std::function<void()>;

  public:
    enum class Priority {
      kNormal = 0,
      kHigh,
    };

    bool operator<(const Event &other) const { return priority() < other.priority(); }

    bool operator>(const Event &other) const { return priority() > other.priority(); }

    bool operator==(const Event &other) const { return priority() == other.priority(); }

    [[nodiscard]] Priority priority() const { return mPriority; }

    void setPriority(const Priority &priority) { mPriority = priority; }

    template<typename Func, typename... Args>
    auto bind(Func &&callable, Args &&... args) {
      mHandlers.push_back(std::bind(std::forward<Func>(callable), std::forward<Args>(args)...));
    }

    void trigger() {
      for (auto &handler : mHandlers) {
        handler();
      }
    }

  protected:
    Priority mPriority{Priority::kNormal};
    std::vector<EventHandler> mHandlers; // 事件处理函数
};

class EventLoop {
  public:
    /**
     * \brief 事件循环执行位置
     */
    enum class ExecMode {
      kThisThread = 0, // 当前线程
      kNewThread,      // 新线程
    };

    enum class Status {
      kRunning = 0,
      kPaused,
      kStopped,
    };

    enum class TimerMode {
      kOnce = 0,
      kLoop
    };

    EventLoop();

    ~EventLoop();

    EventLoop(const EventLoop &other) = delete;

    EventLoop(EventLoop &&other) = delete;

    EventLoop &operator=(const EventLoop &other) = delete;

    EventLoop &operator=(EventLoop &&other) = delete;

    void start(const ExecMode &mode = ExecMode::kThisThread);

    void stop();

    void pause();

    void resume();

    [[nodiscard]] Status status() const;

    void add(const std::string &event_name, const Event &event);

    std::optional<Event> remove(const std::string &event_name);

    void trigger(const std::string &event_name);

  private:
    std::thread mThread;

    std::mutex mStatusMutex;
    std::atomic<Status> mStatus;

    std::shared_mutex mEventsMutex;
    std::unordered_map<std::string, Event> mEvents;

    using TriggeredEvents = std::priority_queue<Event>;
    std::mutex mTriggeredEventsMutex;
    std::condition_variable mTriggeredEventsCv;
    TriggeredEvents mTriggeredEvents;
};
}

#endif //EVENT_LOOP_H
