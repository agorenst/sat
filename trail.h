#pragma once
#include <memory>
#include "action.h"

// This data structure captures the actual state our partial assignment.

struct trail_t {
  enum class v_state_t { unassigned, var_true, var_false };

  /*
  std::unique_ptr<action_t[]> mem;
  std::unique_ptr<bool[]> varset;
  std::unique_ptr<size_t[]> varlevel;
  std::unique_ptr<v_state_t[]> varstate;
  std::unique_ptr<v_state_t[]> litstate;
  */
  action_t* mem = nullptr;
  bool* varset = nullptr;
  size_t* varlevel = nullptr;
  v_state_t* varstate = nullptr;
  v_state_t* litstate = nullptr;

  size_t next_index;
  size_t size;
  size_t dlevel;
  variable_t max_var;

  trail_t(const trail_t& t) = delete;
  trail_t() {}
  void construct(size_t _max_var);

  action_t* cbegin() const { return &(mem[0]); }
  action_t* cend() const { return &(mem[next_index]); }
  action_t* begin() { return &(mem[0]); }
  action_t* end() { return &(mem[next_index]); }
  action_t* begin() const { return &(mem[0]); }
  action_t* end() const { return &(mem[next_index]); }

  auto rbegin() { return std::make_reverse_iterator(end()); }
  auto rend() { return std::make_reverse_iterator(begin()); }
  auto rbegin() const { return std::make_reverse_iterator(end()); }
  auto rend() const { return std::make_reverse_iterator(begin()); }
  auto crbegin() const { return std::make_reverse_iterator(cend()); }
  auto crend() const { return std::make_reverse_iterator(cbegin()); }

  void clear() {
    varstate = nullptr;
    next_index = 0;
  }
  bool empty() const { return next_index == 0; }

  void append(action_t a);
  void pop();

  void drop_from(action_t* it);

  size_t level(literal_t l) const;
  size_t level(action_t a) const;
  size_t level() const { return dlevel; }
  bool literal_true(const literal_t l) const;
  bool literal_false(const literal_t l) const;
  bool literal_unassigned(const literal_t l) const;

  bool clause_unsat(const clause_t& c) const;
  bool clause_sat(const clause_t& c) const;

  size_t count_true_literals(const clause_t& c) const;
  size_t count_unassigned_literals(const clause_t& c) const;
  literal_t find_unassigned_literal(const clause_t& c) const;
  literal_t find_last_falsified(const clause_t& c) const;
  bool uses_clause(const clause_id cid) const;
};

std::ostream& operator<<(std::ostream& o, const trail_t::v_state_t s);
std::ostream& operator<<(std::ostream& o,
                         const std::vector<trail_t::v_state_t>& s);
std::ostream& operator<<(std::ostream& o, const trail_t& t);
