#include "unit_queue.h"

#include "debug.h"
#define OLD_UNITS

void unit_queue_t::push(action_t a) {
  SAT_ASSERT(std::find_if(begin(), end(),
                          [a](action_t b) { return b.l == a.l; }) == end());
#ifdef OLD_UNITS
  queue.push_back(a);
#else
  if (e == queue.size()) {
    queue.push_back(a);
    e++;
  } else {
    SAT_ASSERT(queue.size() > e);
    queue[e] = a;
    e++;
  }
#endif
}

action_t unit_queue_t::pop() {
#ifdef OLD_UNITS
  action_t a = queue[b++];
  // action_t a = queue.back();
  // queue.pop_back();
  return a;
#else
  SAT_ASSERT(b < e);
  action_t a = queue[b];
  b++;
  return a;
#endif
}

bool unit_queue_t::empty() {
#ifdef OLD_UNITS
  return b == queue.size();
#else
  if (b == e) {
    clear();
    return true;
  }
  return false;
#endif
}

void unit_queue_t::clear() {
#ifdef OLD_UNITS
  b = 0;
  queue.clear();
#else
  b = 0;
  e = 0;
#endif
}
