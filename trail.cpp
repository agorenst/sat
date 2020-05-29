#include "trail.h"
#include <iostream>

trail_t::v_state_t satisfy_literal(literal_t l) {
  if (l > 0) {
    return trail_t::v_state_t::var_true;
  }
  return trail_t::v_state_t::var_false;
}

void trail_t::construct(size_t max_var) {
  size = max_var+1;
  next_index = 0;
  dlevel = 0;
  mem = std::make_unique<action_t[]>(size);
  varset = std::make_unique<bool[]>(size);
  varlevel = std::make_unique<size_t[]>(size);
  varstate = std::make_unique<v_state_t[]>(size);

  std::fill(&varstate[0], &varstate[size-1], v_state_t::unassigned);
}

bool trail_t::literal_true(const literal_t l) const {
  variable_t v = l > 0 ? l : -l;
  v_state_t is_true = l > 0 ? v_state_t::var_true : v_state_t::var_false;
  return varstate[v] == is_true;
}
bool trail_t::literal_false(const literal_t l) const {
  variable_t v = l > 0 ? l : -l;
  v_state_t is_false = l < 0 ? v_state_t::var_true : v_state_t::var_false;
  return varstate[v] == is_false;
}

bool trail_t::literal_unassigned(const literal_t l) const {
  variable_t v = l > 0 ? l : -l;
  return varstate[v] == v_state_t::unassigned;
}

void trail_t::append(action_t a) {
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
    varstate[v] = satisfy_literal(a.get_literal());
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

void trail_t::pop() {
  SAT_ASSERT(next_index > 0);
  next_index--;
  action_t a = mem[next_index];
  if (a.has_literal()) {
    variable_t v = std::abs(a.get_literal());
    varset[v] = false;
    varstate[v] = v_state_t::unassigned;
  }
  if (a.is_decision()) {
    dlevel--;
  }
}

bool trail_t::clause_unsat(const clause_t& c) const {
  return std::all_of(std::begin(c), std::end(c), [this](literal_t l) { return this->literal_false(l); });
}
size_t trail_t::count_unassigned_literals(const clause_t& c) const {
  return std::count_if(std::begin(c), std::end(c), [this](literal_t l) {return this->literal_unassigned(l);});
}

std::ostream& operator<<(std::ostream& o, const trail_t::v_state_t s) {
  switch(s) {
    case trail_t::v_state_t::unassigned: return o << "unassigned";
    case trail_t::v_state_t::var_true: return o << "true";
    case trail_t::v_state_t::var_false: return o << "false";
  };
  return o;
}

std::ostream& operator<<(std::ostream& o, const std::vector<trail_t::v_state_t>& s) {
  o << "{ ";
  for (size_t i = 0; i < s.size(); i++) {
    auto v = s[i];
    if (v != trail_t::v_state_t::unassigned) {
      o << i << "=" << v << " ";
    }
  }
  return o << "}";
}

std::ostream& operator<<(std::ostream& o, const trail_t& t) {
  for (auto a : t) {
    o << '\t' <<  a << std::endl;
  }
  return o;
}
