#include "ipcxx/event_loop.h"

#include <mutex>

using namespace std::chrono_literals;

namespace ipcxx {
void EventLoop::start(const ExecMode &mode) {
  if (mStatus != Status::kStopped) {
    return;
  }

  mStatus = Status::kRunning;

  auto loop = [this] {
    while (mStatus != Status::kStopped) {
      std::unique_lock lock(mTriggeredEventsMutex);
      mTriggeredEventsCv.wait(lock, [this] { return !mTriggeredEvents.empty(); });

      if (mStatus != Status::kRunning) {
        continue;
      }

      // 快速交换队列，减少阻塞
      TriggeredEvents triggered_events;
      std::swap(mTriggeredEvents, triggered_events);
      lock.unlock();

      while (!triggered_events.empty()) {
        Event event = triggered_events.top();
        triggered_events.pop();
        event.trigger();
      }
    }
  };

  if (mode == ExecMode::kNewThread) {
    mThread = std::thread(loop);
  } else {
    loop();
  }
}

void EventLoop::stop() {
  if (mStatus == Status::kStopped) {
    return;
  }

  mStatus = Status::kStopped;

  // 唤醒循环
  {
    std::lock_guard lock(mTriggeredEventsMutex);
    mTriggeredEventsCv.notify_all();
  }

  if (mThread.joinable()) {
    mThread.join();
  }
}

void EventLoop::pause() {
  if (mStatus == Status::kRunning) {
    // TODO 注意两次原子操作之间的竞争
    mStatus = Status::kPaused;
  }
}

void EventLoop::resume() {
  if (mStatus == Status::kPaused) {
    mStatus = Status::kRunning;
  }
}

EventLoop::Status EventLoop::status() const {
  return mStatus;
}

void EventLoop::add(const std::string &event_name, const Event &event) {
  std::lock_guard lock(mEventsMutex);
  mEvents[event_name] = event;
}

std::optional<Event> EventLoop::remove(const std::string &event_name) {
  std::lock_guard lock(mEventsMutex);
  if (auto it = mEvents.find(event_name); it != mEvents.end()) {
    auto &[event_name, event_obj] = *it;
    return std::move(event_obj);
  }
  return {};
}

void EventLoop::trigger(const std::string &event_name) {
  std::shared_lock lock(mEventsMutex);
  auto it = mEvents.find(event_name);

  if (it == mEvents.end()) {
    return;
  }

  auto &[_, event_obj] = *it;

  std::lock_guard triggered_events_lock(mTriggeredEventsMutex);
  mTriggeredEvents.push(event_obj);
  mTriggeredEventsCv.notify_all();
}
}
