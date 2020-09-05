#include "unit_queue.h"

#include "debug.h"
#define OLD_UNITS

  unit_queue_t::iterator unit_queue_t::begin() {
    return queue.begin() + b;
    }
  unit_queue_t::iterator unit_queue_t::end() {
    return queue.end() + e;
    }

void unit_queue_t::push(entry_t a) {
  //auto it = std::find_if(begin(), end(), [a](entry_t b) { return b.l == a.l; });

  // already in the queue
  //if (it == end()) {
    queue.push_back(a);
    //return;
  //}

  //if (it->s > a.s) {
    // This seems to induce a perf *loss*. Not sure why, yet.
    //*it = a;
    //it->c = a.c;
    //it->s = a.s;
  //}
}

unit_queue_t::entry_t unit_queue_t::pop() {
  entry_t a = queue[b++];
  return a;
}

size_t unit_queue_t::size() {
  return queue.size() - b;
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
