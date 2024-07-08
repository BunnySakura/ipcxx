#include "ipcxx/event_loop.h"

#include <mutex>

using namespace std::chrono_literals;

namespace ipcxx {
EventLoop::EventLoop() : mStatus(Status::kStopped) {}

EventLoop::~EventLoop() {
  stop();
}

void EventLoop::start(const ExecMode &mode) {
  if (mStatus != Status::kStopped) {
    return;
  }

  std::unique_lock status_lock(mStatusMutex);
  mStatus = Status::kRunning;

  auto loop = [this] {
    while (mStatus != Status::kStopped) {
      std::unique_lock triggered_events_lock(mTriggeredEventsMutex);
      mTriggeredEventsCv.wait(triggered_events_lock, [this] { return !mTriggeredEvents.empty(); });

      if (mStatus != Status::kRunning) {
        continue;
      }

      // 快速交换队列，减少阻塞
      TriggeredEvents triggered_events;
      std::swap(mTriggeredEvents, triggered_events);
      triggered_events_lock.unlock();

      while (!triggered_events.empty()) {
        Event event = triggered_events.top();
        event.trigger();
        triggered_events.pop();
      }
    }
  };

  if (mode == ExecMode::kNewThread) {
    mThread = std::thread(loop);
  } else {
    status_lock.unlock();
    loop();
  }
}

void EventLoop::stop() {
  if (mStatus == Status::kStopped) {
    return;
  }

  std::lock(mStatusMutex, mTriggeredEventsMutex);

  mStatus = Status::kStopped;
  mEvents.clear();
  mTriggeredEventsCv.notify_all(); // 唤醒循环

  if (mThread.joinable()) {
    mThread.join();
  }
}

void EventLoop::pause() {
  if (mStatus == Status::kRunning) {
    std::lock_guard status_lock(mStatusMutex);
    mStatus = Status::kPaused;
  }
}

void EventLoop::resume() {
  if (mStatus == Status::kPaused) {
    std::lock_guard status_lock(mStatusMutex);
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

  if (auto it = mEvents.find(event_name); it != mEvents.end()) {
    auto &[_, event_obj] = *it;
    std::lock_guard triggered_events_lock(mTriggeredEventsMutex);
    mTriggeredEvents.push(event_obj);
  }

  mTriggeredEventsCv.notify_all();
}
}
