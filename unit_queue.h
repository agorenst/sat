#pragma once
#include "action.h"
// We use a real queue
struct unit_queue_t {
  std::vector<action_t> queue;
  size_t b = 0;
  size_t e = 0;
__attribute__((noinline))
  void push(action_t a);
  action_t pop();
  bool empty();
  void clear();

  auto begin() { return queue.begin() + b; }
  auto end() { return queue.begin() + e; }
};
