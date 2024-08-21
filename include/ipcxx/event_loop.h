#ifndef EVENT_LOOP_H
#define EVENT_LOOP_H

#include "ipcxx/async_queue.h"

#include <atomic>
#include <functional>
#include <optional>
#include <string>
#include <thread>
#include <unordered_set>

namespace ipcxx {
class Event final {
  using EventHandler = std::function<void()>; // 事件处理函数

  public:
    /**
     * \brief 事件优先级
     */
    enum class Priority {
      kLow = 0,
      kNormal,
      kHigh,
    };

    Event() = default;

    ~Event() = default;

    Event(const Event &) = default;

    Event(Event &&) noexcept = default;

    Event &operator=(const Event &) = default;

    Event &operator=(Event &&) noexcept = default;

    bool operator<(const Event &other) const { return mPriority < other.mPriority; }

    bool operator>(const Event &other) const { return mPriority > other.mPriority; }

    bool operator==(const Event &other) const { return mPriority == other.mPriority; }

    /**
     * \brief 获取事件优先级
     * \return 事件优先级
     */
    [[nodiscard]] Priority priority() const { return mPriority; }

    /**
     * \brief 设置事件优先级
     * \param priority 事件优先级
     */
    void setPriority(const Priority &priority) { mPriority = priority; }

    /**
     * \brief 绑定事件处理函数
     * \tparam Func 回调函数类型模板
     * \tparam Args 回调变长参数类型模板
     * \param callable 回调函数
     * \param args 回调变长参数
     * \return 推导为 void
     */
    template<typename Func, typename... Args>
    auto bind(Func &&callable, Args &&... args) {
      mHandlers.push_back(std::bind(std::forward<Func>(callable), std::forward<Args>(args)...));
    }

    /**
     * \brief 触发事件
     * \note 事件优先级高的事件会优先响应
     */
    void trigger() {
      for (auto &handler : mHandlers) {
        handler();
      }
    }

  protected:
    Priority mPriority{Priority::kNormal}; // 事件优先级
    std::vector<EventHandler> mHandlers;   // 事件处理函数
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

    /**
     * \brief 事件循环状态
     */
    enum class Status {
      kRunning = 0,
      kPaused,
      kStopped,
    };

    /**
     * \brief 创建事件循环
     * \param mode 事件循环执行位置
     */
    explicit EventLoop(const ExecMode &mode = ExecMode::kNewThread)
      : mLoopStatus(Status::kRunning) {
      auto loop = [this] {
        while (mLoopStatus != Status::kStopped) {
          std::string event_name;
          std::vector<std::string> triggered_evt_names;
          std::priority_queue<Event> triggered_evts; // 优先队列，高优先级事件优先响应

          using namespace std::chrono_literals;
          do { // 快速取出触发的事件名称到局部变量，减少阻塞
            mTriggeredEvtNames.pop(event_name);
            triggered_evt_names.push_back(event_name);
          } while (!mTriggeredEvtNames.empty());

          if (mLoopStatus != Status::kRunning) { continue; } // 事件循环暂停，直接跳过

          // 若触发的事件名称已添加，快速取出对应事件对象到局部变量
          std::unique_lock lock(mEventsMutex);
          for (auto &name : triggered_evt_names) {
            if (auto it = mEvents.find(name); it != mEvents.end()) {
              auto &[_, event_obj] = *it;
              triggered_evts.push(event_obj);
            }
          }
          lock.unlock();

          while (!triggered_evts.empty()) {
            Event evt = triggered_evts.top();
            triggered_evts.pop();
            evt.trigger(); // 执行事件绑定的处理函数
          }
        }
      };

      if (mode == ExecMode::kThisThread) {
        loop();
      } else {
        mLoopThread = std::thread(loop);
      }
    }

    ~EventLoop() {
      mLoopStatus = Status::kStopped;
      mTriggeredEvtNames.push(std::string());

      if (mLoopThread.joinable()) {
        mLoopThread.join();
      }
    }

    EventLoop(const EventLoop &other) = delete;

    EventLoop(EventLoop &&other) = delete;

    EventLoop &operator=(const EventLoop &other) = delete;

    EventLoop &operator=(EventLoop &&other) = delete;

    /**
     * \brief 暂停事件循环
     */
    void pause() { mLoopStatus = Status::kPaused; }

    /**
     * \brief 恢复事件循环
     */
    void resume() { mLoopStatus = Status::kRunning; }

    /**
     * \brief 获取事件循环状态
     * \return 事件循环状态
     */
    [[nodiscard]] Status status() const { return mLoopStatus; }

    /**
     * \brief 注册事件
     * \param event_name 事件名称
     * \param event 事件对象
     */
    void add(const std::string &event_name, const Event &event) {
      std::lock_guard lock(mEventsMutex);
      mEvents[event_name] = event;
    }

    /**
     * \brief 移除事件
     * \param event_name 事件名称
     * \return 事件对象或 `std::nullopt`
     */
    std::optional<Event> remove(const std::string &event_name) {
      std::lock_guard lock(mEventsMutex);
      if (auto it = mEvents.find(event_name); it != mEvents.end()) {
        auto &[event_name, event_obj] = *it;
        return std::move(event_obj);
      }
      return std::nullopt;
    }

    /**
     * \brief 清空所有事件
     */
    void clear() {
      std::lock_guard lock(mEventsMutex);
      mEvents.clear();
    }

    /**
     * \brief 触发事件
     * \param event_name 事件名称
     */
    void trigger(const std::string &event_name) {
      mTriggeredEvtNames.push(event_name);
    }

  private:
    std::thread mLoopThread;         // 在新线程中执行循环
    std::atomic<Status> mLoopStatus; // 事件循环状态

    std::unordered_map<std::string, Event> mEvents; // 事件名称 - 事件对象
    std::mutex mEventsMutex;

    AsyncQueue<std::string> mTriggeredEvtNames; // 触发的事件名称
};

class TimerManager {
  using milliseconds = std::chrono::milliseconds;
  using time_point = std::chrono::time_point<std::chrono::steady_clock>;

  public:
    /**
     * \brief 定时器模式
     */
    enum class TimerMode {
      kOnce = 0,
      kLoop
    };

    /**
     * \brief 启动定时器管理线程
     * \note 需要在线程启动后再触发定时器，否则会丢失此次触发
     */
    TimerManager()
      : mThreadExit(false),
        mManagerThread(&TimerManager::timerManagerThread, this) {}

    ~TimerManager() {
      mThreadExit = true;
      mThreadCv.notify_all();
      mManagerThread.join();
    }

    TimerManager(const TimerManager &) = delete;

    TimerManager &operator=(const TimerManager &) = delete;

    TimerManager(TimerManager &&other) = delete;

    TimerManager &operator=(TimerManager &&other) = delete;

    /**
     * \brief 添加定时器
     * \param name 定时器名称
     * \param event 定时器事件
     * \param timeout 超时时间
     * \param mode 定时器模式，一次或循环
     */
    void addTimer(
      const std::string &name,
      const Event &event,
      const milliseconds &timeout,
      TimerMode mode = TimerMode::kOnce
    ) {
      std::lock_guard lock(mThreadMutex);
      mTimers.push({name, event, mode, std::chrono::steady_clock::now(), timeout, timeout});
      mThreadCv.notify_one();
    }

    /**
     * \brief 移除定时器
     * \param name 定时器名称
     * \note 定时器将被标记为删除，在下一轮中会被清理，避免遍历耗时
     */
    void removeTimer(const std::string &name) {
      std::lock_guard lock(mDeletedTimersMutex);
      mDeletedTimers.insert(name); // 标记为删除，在下一轮中被清理
    }

    /**
     * \brief 清除所有定时器
     */
    void clearTimers() {
      std::lock_guard lock(mThreadMutex);
      std::priority_queue<Timer> timers;
      mTimers.swap(timers);
    }

  private:
    std::atomic_bool mThreadExit;
    std::thread mManagerThread;
    std::mutex mThreadMutex;
    std::condition_variable mThreadCv;

    struct Timer {
      std::string name;
      Event event;
      TimerMode mode;

      time_point last_tick;  // 最后一次触发时间
      milliseconds timeout;  // 超时时间
      milliseconds timeleft; // 剩余时间

      bool operator<(const Timer &other) const {
        return timeleft > other.timeleft; // 剩余时间小的优先响应
      }
    };

    std::priority_queue<Timer> mTimers;
    std::unordered_set<std::string> mDeletedTimers;
    std::mutex mDeletedTimersMutex;

    /**
     * \brief 定时器管理线程
     */
    void timerManagerThread() {
      using namespace std::chrono_literals;
      milliseconds next_timeout = 365 * 24h;
      while (!mThreadExit) {
        std::unique_lock lock(mThreadMutex);
        mThreadCv.wait_for(lock, next_timeout);
        if (mTimers.empty()) { continue; }

        // 遍历定时器，如果已删除则跳过，否则更新剩余时间和最后一次滴答计数时间
        // 如果剩余时间小于等于 0 则触发事件
        // 更新下一次休眠时间
        std::unique_lock deleted_lock(mDeletedTimersMutex);
        auto deleted_timers = mDeletedTimers;
        deleted_lock.unlock();

        for (int i = 0; i < mTimers.size(); ++i) {
          auto timer = mTimers.top();
          mTimers.pop();
          if (deleted_timers.find(timer.name) != deleted_timers.end()) { continue; }

          auto now = std::chrono::steady_clock::now();
          timer.timeleft = timer.timeleft - std::chrono::duration_cast<milliseconds>(now - timer.last_tick);
          timer.last_tick = now;

          if (timer.timeleft <= 0ms) {
            timer.timeleft = timer.timeout;
            timer.event.trigger();
          }

          if (timer.mode == TimerMode::kLoop) {
            mTimers.push(timer);
          }
        }

        if (mTimers.empty()) {
          next_timeout = 24h;
        } else {
          next_timeout = mTimers.top().timeleft;
        }
      }
    }
};
}

#endif //EVENT_LOOP_H
