#include <chrono>
#include <iostream>
#include <thread>

#include "ipcxx/event_loop.h"

#define EVT_NAME_HIGH_PRIORITY "HighPriorityEvent"  // 高优先级事件名
#define EVT_NAME_TEST "TestEvent"                   // 普通事件名
#define EVT_NAME_TIMER "TestTimerEvent"             // 定时器事件名
#define TIMER_NAME "TestTimer"                      // 定时器名

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
  event.setPriority(Event::Priority::kHigh); // 设为高优先级
  event.bind([] { std::cout << "High priority event triggered." << std::endl; });
  event.bind(
    [](const std::string &str) {
      std::cout << "High priority event say: Hi, " << str << std::endl;
    }, __func__
  );
  event_loop.addListener(EVT_NAME_HIGH_PRIORITY, event);
}

void TestEvent(EventLoop &event_loop) {
  // 添加事件到事件循环
  Event event;
  event.bind(
    [](const std::atomic_int &global_count) {
      static int internal_count = 0; // 回调执行一次，计数+1
      std::cout << "Event triggered, "
          << "global_count = " << global_count
          << ", internal_count = " << internal_count++
          << std::endl;
    },
    std::cref(global_count)
  );
  event_loop.addListener(EVT_NAME_TEST, event);
}

void TestTimerManager(EventLoop &event_loop, TimerManager &timer_manager) {
  // 在事件循环中定时触发事件
  Event event;
  event.bind(
    [](std::atomic_int &global_count) {
      std::cout << "Timer triggered, global_count = " << ++global_count << std::endl;
    }, std::ref(global_count)
  );
  event_loop.addListener(EVT_NAME_TIMER, event);

  // 添加定时器
  Event timer_event;
  timer_event.bind(
    [](EventLoop &event_loop) {
      event_loop.trigger(EVT_NAME_TIMER);
    }, std::ref(event_loop)
  );
  timer_manager.addTimer(TIMER_NAME, timer_event, 1s, TimerManager::TimerMode::kLoop);
}

int main() {
  // 在新线程启动事件循环
  EventLoop event_loop;
  event_loop.start(EventLoop::ExecMode::kNewThread);
  event_loop.start(EventLoop::ExecMode::kNewThread); // 测试重复启动

  TestEventPriority(event_loop); // 测试高优先级事件
  TestEvent(event_loop);         // 测试普通事件

  // 测试定时器
  TimerManager timer_manager;
  timer_manager.start();
  timer_manager.start(); // 测试重复启动
  TestTimerManager(event_loop, timer_manager);

  // 开启两个线程触发事件，验证事件循环满足响应优先级和线程安全
  auto trigger_thread = [](
    const std::chrono::milliseconds &sleep, const std::string &event_name, EventLoop &event_loop
  ) {
    for (int i = 0; i < 50; ++i) {
      ++global_count;
      event_loop.trigger(event_name);
      std::this_thread::sleep_for(sleep);
    }
  };
  std::thread thread1(trigger_thread, 100ms, EVT_NAME_HIGH_PRIORITY, std::ref(event_loop));
  std::thread thread2(trigger_thread, 20ms, EVT_NAME_TEST, std::ref(event_loop));

  std::this_thread::sleep_for(3s);
  event_loop.pause(); // 暂停事件循环
  std::cout << "Event loop paused." << std::endl;
  std::this_thread::sleep_for(3s);
  event_loop.resume(); // 恢复事件循环
  std::cout << "Event loop resumed." << std::endl;

  // 删除定时器
  std::this_thread::sleep_for(5s);
  timer_manager.removeTimer(TIMER_NAME);
  std::cout << "Timer removed." << std::endl;

  // 删除事件循环中的事件
  std::this_thread::sleep_for(1s);
  event_loop.removeListener(EVT_NAME_HIGH_PRIORITY);
  std::cout << "High priority event removed." << std::endl;
  std::this_thread::sleep_for(5s);
  event_loop.removeListener(EVT_NAME_TEST);
  std::cout << "Test event removed." << std::endl;

  // 等待触发事件的线程结束
  thread1.join();
  thread2.join();

  // 停止事件循环并清理绑定事件和定时器
  event_loop.stop();
  event_loop.clearListeners();
  timer_manager.clearTimers();
  std::cout << "Event loop stopped, clear timers and events." << std::endl;

  std::thread thread3(trigger_thread, 500ms, EVT_NAME_HIGH_PRIORITY, std::ref(event_loop));
  std::thread thread4(trigger_thread, 100ms, EVT_NAME_TEST, std::ref(event_loop));
  std::thread thread5(
    [&event_loop] {
      std::this_thread::sleep_for(10s);
      std::cout << "Event loop stopped in main thread." << std::endl;
      event_loop.stop();
    }
  );

  // 测试在当前线程执行事件循环
  std::cout << "Event loop started in main thread." << std::endl;
  TestEventPriority(event_loop); // 测试高优先级事件
  TestEvent(event_loop);         // 测试普通事件
  event_loop.start(EventLoop::ExecMode::kThisThread);

  // 等待触发事件的线程结束
  thread3.join();
  thread4.join();
  thread5.join();

  return 0;
}
