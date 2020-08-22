#include "trail.h"

#include <iostream>

void trail_t::construct(size_t m) {
  max_var = m;
  size = max_var + 1;
  next_index = 0;
  dlevel = 0;

  litstate.construct(m);
  oldlitstate.construct(m);
  varset.construct(m);
  varlevel.construct(m);
  lit_to_action.construct(m);

  mem = new action_t[size];

  varset.clear();
  std::fill(std::begin(litstate), std::end(litstate), v_state_t::unassigned);
  std::fill(std::begin(oldlitstate), std::end(oldlitstate),
            v_state_t::unassigned);
}

bool trail_t::literal_true(const literal_t l) const {
  return litstate[l] == v_state_t::var_true;
}
bool trail_t::literal_false(const literal_t l) const {
  return litstate[l] == v_state_t::var_false;
}

bool trail_t::literal_unassigned(const literal_t l) const {
  return litstate[l] == v_state_t::unassigned;
}

action_t trail_t::cause(literal_t l) const {
  SAT_ASSERT(literal_true(l));
  SAT_ASSERT(varset.get(var(l)));
  return mem[lit_to_action[l]];
}

void trail_t::append(action_t a) {
  // std::cout << "next_index = " << next_index << ", size = " << size <<
  // std::endl; std::cout << a << std::endl;
  if (a.is_decision()) {
    dlevel++;
  }
  if (a.has_literal()) {
    literal_t l = a.get_literal();
    variable_t v = var(l);
    if (varset.get(v)) {
      // Don't add!
      return;
    }
    varset.set(v);
    varlevel[v] = dlevel;
    litstate[l] = v_state_t::var_true;
    litstate[neg(l)] = v_state_t::var_false;
    oldlitstate[l] = v_state_t::var_true;
    oldlitstate[neg(l)] = v_state_t::var_false;

    // We never bother cleaning this. Mistake?
    lit_to_action[l] = next_index;
  }
  // if (next_index == size) {
  // std::cout << "[DBG][ERR] No room for " << a << " in trail" << std::endl;
  // for (const auto& a : *this) {
  // std::cout << a << std::endl;
  //}
  //}
  SAT_ASSERT(next_index < size);
  mem[next_index] = a;
  next_index++;
}

literal_t trail_t::previously_assigned_literal(variable_t v) const {
  literal_t l = lit(v);
  switch (oldlitstate[l]) {
  case v_state_t::var_false:
    return neg(l);
  default:
    return l;
  }
}

void trail_t::pop() {
  SAT_ASSERT(next_index > 0);
  next_index--;
  action_t a = mem[next_index];
  if (a.has_literal()) {
    literal_t l = a.get_literal();
    variable_t v = var(l);
    varset.clear(v);
    litstate[l] = v_state_t::unassigned;
    litstate[neg(l)] = v_state_t::unassigned;
    // we deliberately *don't* erase the oldlitstate.
  }
  if (a.is_decision()) {
    dlevel--;
  }
}

bool trail_t::clause_unsat(const clause_t &c) const {
  return std::all_of(std::begin(c), std::end(c),
                     [this](literal_t l) { return this->literal_false(l); });
}
bool trail_t::clause_sat(const clause_t &c) const {
  return std::any_of(std::begin(c), std::end(c),
                     [this](literal_t l) { return this->literal_true(l); });
}
size_t trail_t::count_true_literals(const clause_t &clause) const {
  return std::count_if(std::begin(clause), std::end(clause),
                       [this](auto &c) { return this->literal_true(c); });
}
size_t trail_t::count_unassigned_literals(const clause_t &c) const {
  return std::count_if(std::begin(c), std::end(c), [this](literal_t l) {
    return this->literal_unassigned(l);
  });
}
literal_t trail_t::find_unassigned_literal(const clause_t &c) const {
  return *std::find_if(std::begin(c), std::end(c), [this](literal_t l) {
    return this->literal_unassigned(l);
  });
}

std::ostream &operator<<(std::ostream &o, const trail_t::v_state_t s) {
  switch (s) {
  case trail_t::v_state_t::unassigned:
    return o << "unassigned";
  case trail_t::v_state_t::var_true:
    return o << "true";
  case trail_t::v_state_t::var_false:
    return o << "false";
  };
  return o;
}

std::ostream &operator<<(std::ostream &o,
                         const std::vector<trail_t::v_state_t> &s) {
  o << "{ ";
  for (size_t i = 0; i < s.size(); i++) {
    auto v = s[i];
    if (v != trail_t::v_state_t::unassigned) {
      o << i << "=" << v << " ";
    }
  }
  return o << "}";
}

std::ostream &operator<<(std::ostream &o, const trail_t &t) {
  for (auto a : t) {
    o << '\t' << a << std::endl;
  }
  return o;
}

literal_t trail_t::find_last_falsified(const clause_t &c) const {
  auto it = std::find_if(rbegin(), rend(), [&c](action_t a) {
    return a.has_literal() && contains(c, neg(a.get_literal()));
  });
  if (it != rend()) {
    return neg(it->get_literal());
  }
  return 0;
}

bool trail_t::uses_clause(const clause_id cid) const {
  for (action_t &a : *this) {
    if (a.has_clause() && a.get_clause() == cid) {
      return true;
    }
  }
  return false;
}

size_t trail_t::level(action_t a) const {
  size_t l = 0;
  for (action_t b : *this) {
    if (b.is_decision())
      l++;
    if (b == a) {
      return l;
    }
  }
  assert(0);
  return 0;
}
size_t trail_t::level(literal_t l) const { return varlevel[var(l)]; }

void trail_t::drop_from(action_t *it) {
  // std::cout << it << " " << &(mem[next_index]) << std::endl;
  SAT_ASSERT(it <= &(mem[next_index]));
  while (end() > it) {
    pop();
  }
  // std::cout << next_index << " ";
  // next_index = std::distance(begin(), it);
  // std::cout << next_index << std::endl;
}

bool trail_t::contains_clause(clause_id cid) const {
  return std::any_of(std::begin(*this), std::end(*this), [&](const action_t a) {
    return a.has_clause() && a.get_clause() == cid;
  });
}
