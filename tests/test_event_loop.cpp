#include <chrono>
#include <iostream>
#include <thread>

#include "ipcxx/event_loop.h"

using namespace ipcxx;
using namespace std::chrono_literals;

int main() {
  // 在新线程启动事件循环
  EventLoop event_loop(EventLoop::ExecMode::kNewThread);

  // 创建事件对象并绑定处理函数
  std::atomic_int global_count = 0; // 测试携带参数的回调
  Event event;
  event.bind([] { std::cout << "=== Event triggered. ===" << std::endl; });
  event.bind(
    [](const std::atomic_int &global_count) {
      static int internal_count = 0; // 回调执行一次，计数+1
      std::cout << "==> " << global_count << ", " << internal_count++ << std::endl;
    },
    std::cref(global_count)
  );
  event.setPriority(Event::Priority::kHigh); // 设置高优先级

  // 添加事件到事件循环
  std::string event_name = "TestEvent";
  event_loop.add(event_name, event);

  // 在两个新线程中触发事件
  auto trigger_event = [&global_count](const std::string &event_name, EventLoop &event_loop) {
    for (int i = 0; i < 100; ++i) {
      global_count = i;
      event_loop.trigger(event_name);
      std::this_thread::sleep_for(100ms);
    }
  };
  std::thread thread1(trigger_event, event_name, std::ref(event_loop));
  std::thread thread2(trigger_event, event_name, std::ref(event_loop));

  std::this_thread::sleep_for(1s);
  event_loop.pause();
  std::this_thread::sleep_for(1s);
  event_loop.resume();

  // 添加定时器
  Event timer_event;
  timer_event.bind([] { std::cout << "=== Timer triggered. ===" << std::endl; });
  event_name = "TestTimer";
  TimerManager timer_manager;
  std::this_thread::sleep_for(1s);
  timer_manager.addTimer(event_name, timer_event, 1s, TimerManager::TimerMode::kLoop);
  std::this_thread::sleep_for(5s);
  timer_manager.removeTimer(event_name);
  timer_manager.clearTimers();

  // 等待触发事件的线程结束
  thread1.join();
  thread2.join();

  // 停止事件循环
  std::cout << "Event loop finished processing events." << std::endl;

  return 0;
}
