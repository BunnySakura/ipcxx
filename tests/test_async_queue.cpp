#include "ipcxx/async_queue.h"

#include <functional>
#include <iostream>
#include <thread>

using namespace std;
using namespace ipcxx;
using namespace std::chrono_literals;
using TaskQ = AsyncQueue<int>;

void Producer(TaskQ &queue) {
  this_thread::sleep_for(3s);
  queue.push(-1);
  for (int i = 0; i < 10; i++) {
    queue.push(i);
    this_thread::sleep_for(1s);
  }
}

void Customer(TaskQ &queue) {
  int task;
  queue.pop(task);
  cout << "First task: " << task << endl;
  while (queue.pop(task, 3s)) {
    cout << "task: " << task << endl;
  }
  cout << "Finished" << endl;
}

int main() {
  TaskQ queue;
  thread producer(Producer, std::ref(queue));
  thread customer(Customer, std::ref(queue));
  producer.join();
  customer.join();
  return 0;
}
