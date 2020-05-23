#include "action.h"
#include <memory>

// This is some sequence of actions. For now we can think of it as a vector.
//typedef std::vector<action_t> trail_t;

struct trail_t {
  std::unique_ptr<action_t[]> mem;
  std::unique_ptr<bool[]> varset;
  std::unique_ptr<size_t[]> varlevel;
  size_t next_index;
  size_t size;
  size_t dlevel;

  void construct(size_t max_var);


  action_t* cbegin() const { return &(mem[0]); }
  action_t* cend() const { return &(mem[next_index]); }
  action_t* begin() { return &(mem[0]); }
  action_t* end() { return &(mem[next_index]); }
  action_t* begin() const { return &(mem[0]); }
  action_t* end() const { return &(mem[next_index]); }


  auto rbegin() {
    auto it = std::make_reverse_iterator(end());
    SAT_ASSERT(&*(it.base()-1) == &*it);
    return it;
  }
  auto rend() {
    return std::make_reverse_iterator(begin());
  }
  auto crbegin() const { return std::make_reverse_iterator(cend()); }
  auto crend() const { return std::make_reverse_iterator(cbegin()); }

  void clear() { next_index = 0; }
  bool empty() const { return next_index == 0; }

  void append(action_t a) {
    //std::cout << "next_index = " << next_index << ", size = " << size << std::endl;
    //std::cout << a << std::endl;
    if (a.is_decision()) {
      dlevel++;
    }
    if (a.has_literal()) {
      variable_t v = std::abs(a.get_literal());
      if (varset[v]) {
        // Don't add!
        return;
      }
      varset[v] = true;
      varlevel[v] = dlevel;
    }
  //if (next_index == size) {
      //std::cout << "[DBG][ERR] No room for " << a << " in trail" << std::endl;
      //for (const auto& a : *this) {
        //std::cout << a << std::endl;
        //}
      //}
    SAT_ASSERT(next_index < size);
    mem[next_index] = a;
    next_index++;
  }

  void pop() {
    SAT_ASSERT(next_index > 0);
    next_index--;
    action_t a = mem[next_index];
    if (a.has_literal()) {
      variable_t v = std::abs(a.get_literal());
      varset[v] = false;
    }
    if (a.is_decision()) {
      dlevel--;
    }
  }

  void drop_from(action_t* it) {
    //std::cout << it << " " << &(mem[next_index]) << std::endl;
    SAT_ASSERT(it <= &(mem[next_index]));
    while (end() > it) {
      pop();
    }
    //std::cout << next_index << " ";
    //next_index = std::distance(begin(), it);
    //std::cout << next_index << std::endl;
  }

  size_t level(literal_t l) const {
    return varlevel[std::abs(l)];
  }

  size_t level() const {
    return dlevel;
  }
};
