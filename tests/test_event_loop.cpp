#include <chrono>
#include <iostream>
#include <thread>

#include "ipcxx/event_loop.h"

using namespace ipcxx;
using namespace std::chrono_literals;

std::atomic_int global_count = 0; // 测试携带参数的回调

/**
 * \brief 测试高优先级事件
 * \param event_loop 事件循环
 */
void TestEventPriority(EventLoop &event_loop) {
  // 添加事件到事件循环
  Event event;
  event.bind([] { std::cout << "=== Event triggered. ===" << std::endl; });
  event.setPriority(Event::Priority::kHigh); // 设置高优先级
  event_loop.add("HighPriorityEvent", event);
}

void TestEvent(EventLoop &event_loop) {
  // 添加事件到事件循环
  Event event;
  event.bind(
    [](const std::atomic_int &global_count) {
      static int internal_count = 0; // 回调执行一次，计数+1
      std::cout << "Event triggered. "
          << "global_count = " << global_count << ", internal_count = " << internal_count++
          << std::endl;
    },
    std::cref(global_count)
  );
  event_loop.add("TestEvent", event);
}

void TestTimerManager(EventLoop &event_loop, TimerManager &timer_manager) {
  // 在事件循环中定时触发事件
  Event event;
  event.bind(
    [](std::atomic_int &global_count) {
      std::cout << "Timer triggered. global_count = " << ++global_count << std::endl;
    }, std::ref(global_count)
  );
  event_loop.add("TestTimerEvent", event);

  // 添加定时器
  Event timer_event;
  timer_event.bind(
    [](EventLoop &event_loop) {
      std::cout << "=== Timer triggered. ===" << std::endl;
      event_loop.trigger("TestTimerEvent");
    }, std::ref(event_loop)
  );
  timer_manager.addTimer("TestTimer", timer_event, 1s, TimerManager::TimerMode::kLoop);
}

int main() {
  // 在新线程启动事件循环
  EventLoop event_loop;
  event_loop.start(EventLoop::ExecMode::kNewThread);
  event_loop.start(EventLoop::ExecMode::kNewThread); // 测试重复启动

  // 测试高优先级事件
  TestEventPriority(event_loop);
  // 测试普通事件
  TestEvent(event_loop);
  // 测试定时器
  TimerManager timer_manager;
  timer_manager.start();
  timer_manager.start(); // 测试重复启动
  // std::this_thread::sleep_for(1s);
  TestTimerManager(event_loop, timer_manager);

  // 在两个新线程中触发事件
  auto trigger_event = [](const std::string &event_name, EventLoop &event_loop) {
    for (int i = 0; i < 20; ++i) {
      ++global_count;
      event_loop.trigger(event_name);
      std::this_thread::sleep_for(1s);
    }
  };
  std::thread thread1(trigger_event, "HighPriorityEvent", std::ref(event_loop));
  std::thread thread2(trigger_event, "TestEvent", std::ref(event_loop));

  std::this_thread::sleep_for(3s);
  event_loop.pause();
  std::this_thread::sleep_for(3s);
  event_loop.resume();

  // 删除定时器
  // timer_manager.clearTimers();
  std::this_thread::sleep_for(5s);
  timer_manager.removeTimer("TestTimer");

  // 删除事件循环中的事件
  // event_loop.clear();
  std::this_thread::sleep_for(1s);
  event_loop.remove("HighPriorityEvent");
  std::this_thread::sleep_for(5s);
  event_loop.remove("TestEvent");

  // 等待触发事件的线程结束
  thread1.join();
  thread2.join();

  // 停止事件循环
  std::cout << "Event loop finished processing events." << std::endl;

  return 0;
}
