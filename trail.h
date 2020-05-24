#pragma once
#include "action.h"
#include <memory>

// This data structure captures the actual state our partial assignment.

struct trail_t {
  enum class v_state_t {
                        unassigned,
                        var_true,
                        var_false
  };

  std::unique_ptr<action_t[]> mem;
  std::unique_ptr<bool[]> varset;
  std::unique_ptr<size_t[]> varlevel;
  std::unique_ptr<v_state_t[]> varstate;
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
    return std::make_reverse_iterator(end());
  }
  auto rend() {
    return std::make_reverse_iterator(begin());
  }
  auto rbegin() const {
    return std::make_reverse_iterator(end());
  }
  auto rend() const {
    return std::make_reverse_iterator(begin());
  }
  auto crbegin() const { return std::make_reverse_iterator(cend()); }
  auto crend() const { return std::make_reverse_iterator(cbegin()); }

  void clear() {
    varstate = nullptr;
    next_index = 0;
  }
  bool empty() const { return next_index == 0; }

  void append(action_t a);
  void pop();

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
  bool literal_true(const literal_t l) const;
  bool literal_false(const literal_t l) const;
  bool literal_unassigned(const literal_t l) const;

  bool clause_unsat(const clause_t& c) const;

  size_t count_unassigned_literals(const clause_t& c) const;
};

std::ostream& operator<<(std::ostream& o, const trail_t::v_state_t s);
std::ostream& operator<<(std::ostream& o, const std::vector<trail_t::v_state_t>& s);
