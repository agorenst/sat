#include "action.h"

// This is some sequence of actions. For now we can think of it as a vector.
//typedef std::vector<action_t> trail_t;

struct trail_t {
  std::vector<action_t> values;

  auto rbegin() const { return std::rbegin(values); }
  auto rend() const { return std::rend(values); }

  auto rbegin() { return std::rbegin(values); }
  auto rend() { return std::rend(values); }

  auto begin() const { return std::begin(values); }
  auto end() const { return std::end(values); }

  auto begin() { return std::begin(values); }
  auto end() { return std::end(values); }

  void clear() { values.clear(); }
  bool empty() const { return values.empty(); }

  void append(action_t a) {
    values.push_back(a);
  }

  void pop() {
    values.pop_back();
  }

  void drop_from(std::vector<action_t>::iterator it) {
    values.erase(it, std::end(values));
  }
};
