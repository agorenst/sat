#pragma once
#include <memory>
#include "action.h"
#include "literal_incidence_map.h"

// This data structure captures the actual state our partial assignment.

struct trail_t {
  enum class v_state_t { unassigned, var_true, var_false };
  literal_map_t<v_state_t> litstate;
  literal_map_t<v_state_t> oldlitstate;
  literal_map_t<size_t> lit_to_action;
  action_t* mem = nullptr;
  var_map_t<size_t> varlevel;
  var_bitset_t varset;

  size_t next_index;
  size_t size;
  size_t dlevel;
  variable_t max_var;

  trail_t(const trail_t& t) = delete;
  trail_t(): litstate(0), oldlitstate(0), lit_to_action(0) {}
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
  literal_t previously_assigned_literal(variable_t v) const;

  bool clause_unsat(const clause_t& c) const;
  bool clause_sat(const clause_t& c) const;

  size_t count_true_literals(const clause_t& c) const;
  size_t count_unassigned_literals(const clause_t& c) const;
  literal_t find_unassigned_literal(const clause_t& c) const;
  literal_t find_last_falsified(const clause_t& c) const;
  bool uses_clause(const clause_id cid) const;

  action_t cause(literal_t l) const;
  bool contains_clause(clause_id cid) const;
};

std::ostream& operator<<(std::ostream& o, const trail_t::v_state_t s);
std::ostream& operator<<(std::ostream& o,
                         const std::vector<trail_t::v_state_t>& s);
std::ostream& operator<<(std::ostream& o, const trail_t& t);
