#include "unit_queue.h"

#include "debug.h"

auto unit_queue_t::begin() { return queue.begin() + b; }
auto unit_queue_t::end() { return queue.end() + e; }

void unit_queue_t::push(entry_t a) { queue.push_back(a); }

unit_queue_t::entry_t unit_queue_t::pop() { return queue[b++]; }

size_t unit_queue_t::size() { return queue.size() - b; }

bool unit_queue_t::empty() { return b == queue.size(); }

void unit_queue_t::clear() {
  queue.clear();
  b = 0;
}