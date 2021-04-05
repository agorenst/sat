#include "unit_queue.h"

#include "debug.h"

unit_queue_t::iter unit_queue_t::begin() { return queue.begin() + b; }
unit_queue_t::iter unit_queue_t::end() { return queue.end() + e; }

void unit_queue_t::push(entry_t a) {
  queue.push_back(a);
  prev_pushed = a.l;
}
void unit_queue_t::erase_last() { queue.pop_back(); }

unit_queue_t::entry_t unit_queue_t::pop() { return queue[b++]; }
unit_queue_t::entry_t unit_queue_t::last() { return *(end() - 1); }

size_t unit_queue_t::size() { return queue.size() - b; }

bool unit_queue_t::empty() { return b == queue.size(); }

void unit_queue_t::clear() {
  queue.clear();
  b = 0;
  prev_pushed = 0;
}