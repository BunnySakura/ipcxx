#include <chrono>
#include <iostream>
#include <string>
#include <thread>

#include "ipcxx/event_loop.h"

namespace ipcxx {
// 示例事件处理函数
void exampleEventHandler() {
  std::cout << "Event handler is processing the event." << std::endl;
}

// 触发事件的线程函数
void triggerEvent(const std::string &event_name, EventLoop &eventLoop) {
  // 直接触发事件
  eventLoop.trigger(event_name);
}
} // namespace ipcxx

int main() {
  using namespace ipcxx;
  EventLoop eventLoop;

  // 创建事件并设置处理程序
  Event event;
  event.bind(exampleEventHandler);
  event.setPriority(Event::Priority::kHigh); // 设置高优先级

  // 添加事件到事件循环
  eventLoop.add("TestEvent", event);

  // 启动事件循环在新线程
  std::thread eventLoopThread(
    [&eventLoop] {
      eventLoop.start(EventLoop::ExecMode::kThisThread);
    }
  );

  // 等待片刻以确保事件循环开始运行
  std::this_thread::sleep_for(std::chrono::seconds(1));

  // 在两个新线程中触发事件
  std::thread triggerThread1(triggerEvent, "TestEvent", std::ref(eventLoop));
  std::thread triggerThread2(triggerEvent, "TestEvent", std::ref(eventLoop));

  // 等待触发事件的线程结束
  triggerThread1.join();
  triggerThread2.join();

  // 停止事件循环
  eventLoop.stop();

  // 等待事件循环线程结束
  if (eventLoopThread.joinable()) {
    eventLoopThread.join();
  }

  std::cout << "Event loop finished processing events." << std::endl;

  return 0;
}
