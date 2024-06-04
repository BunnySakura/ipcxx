#include "ipcxx/async_queue.h"

#include <functional>
#include <iostream>
#include <thread>

using namespace std;
using namespace ipcxx;
using TaskQ = AsyncQueue<int>;

void Producer(TaskQ &queue) {
  this_thread::sleep_for(chrono::seconds(3));
  queue.push(-1);
  for (int i = 0; i < 10; i++) {
    queue.push(i);
    this_thread::sleep_for(chrono::seconds(1));
  }
}

void Customer(TaskQ &queue) {
  int task;
  queue.pop(task);
  cout << "First task: " << task << endl;
  while (queue.pop(task, chrono::seconds(3))) {
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
