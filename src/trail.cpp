#include "trail.h"

#include <iostream>

struct new_trail_t {
  size_t level;
  size_t size;
  std::vector<action_t> mem;
  std::vector<var_data_t> data;
  void construct(size_t max_var);
};

void new_trail_t::construct(size_t max_var) {
  level = 0;
  size = max_var + 1;

  mem.resize(size);
  data.resize(size);
}

void trail_t::construct(size_t max_var) {
  size = max_var + 1;
  next_index = 0;
  dlevel = 0;

  litstate.construct(max_var);
  oldlitstate.construct(max_var);
  varset.construct(max_var);
  varlevel.construct(max_var);
  lit_to_action.construct(max_var);

  mem.resize(size);

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
  return *lit_to_action[l];
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
    lit_to_action[l] = &mem[next_index];
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

action_t trail_t::pop() {
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
  return a;
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
size_t trail_t::count_false_literals(const clause_t &clause) const {
  return std::count_if(std::begin(clause), std::end(clause),
                       [this](auto &c) { return this->literal_false(c); });
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
  for (const action_t &a : *this) {
    if (a.has_clause() && a.get_clause() == cid) {
      return true;
    }
  }
  return false;
}

size_t trail_t::level(action_t a) const {
  size_t l = 0;
  for (action_t b : *this) {
    if (b.is_decision()) l++;
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

// For error-checking. This is by-design incredibly simple.
struct correctness_trail {
  const cnf_t &cnf;
  std::vector<literal_t> assigned;
  correctness_trail(const cnf_t &cnf) : cnf(cnf) {}
  bool literal_true(literal_t l) const {
    return std::find(std::begin(assigned), std::end(assigned), l) !=
           std::end(assigned);
  }
  bool literal_false(literal_t l) const {
    return std::find(std::begin(assigned), std::end(assigned), neg(l)) !=
           std::end(assigned);
  }
  bool literal_assigned(literal_t l) const {
    return literal_true(l) || literal_false(l);
  }

  int count_unassigned_literals(const clause_t &c) const {
    return std::count_if(std::begin(c), std::end(c),
                         [&](literal_t l) { return !literal_assigned(l); });
  }
  literal_t first_unassigned_literal(const clause_t &c) const {
    auto it = std::find_if(std::begin(c), std::end(c),
                           [&](literal_t l) { return !literal_assigned(l); });
    return *it;
  }
  bool is_sat_clause(const clause_t &c) const {
    return std::any_of(std::begin(c), std::end(c),
                       [&](literal_t l) { return literal_true(l); });
  }
  bool is_conflict_clause(const clause_t &c) const {
    return std::all_of(std::begin(c), std::end(c),
                       [&](literal_t l) { return literal_false(l); });
  }
  bool cnf_is_sat() const {
    return std::all_of(std::begin(cnf), std::end(cnf),
                       [&](clause_id cid) { return is_sat_clause(cnf[cid]); });
  }
  bool cnf_is_conflict() const {
    return std::any_of(std::begin(cnf), std::end(cnf), [&](clause_id cid) {
      return is_conflict_clause(cnf[cid]);
    });
  }
  bool cnf_is_indeterminate() const {
    return !cnf_is_sat() && !cnf_is_conflict();
  }
  literal_t is_unit(const clause_t &c) {
    if (!is_sat_clause(c) && (count_unassigned_literals(c) == 1)) {
      return first_unassigned_literal(c);
    }
    return 0;
  }

  void validate_no_units() {
    for (auto cid : cnf) {
      assert(!is_unit(cnf[cid]));
    }
  }
  void push_decision(literal_t l) {
    assert(!literal_assigned(l));
    assigned.push_back(l);
  }
  void push_unit(literal_t l, const clause_t &c) {
    assert(is_unit(c) == l);
    assigned.push_back(l);
  }
  void validate_trail(const trail_t &t) {
    for (const action_t &a : t) {
      if (a.is_decision()) {
        assert(!cnf_is_conflict());
        // it can be SAT, so we can't say "indeterminate"
        validate_no_units();
        push_decision(a.get_literal());
      } else if (a.is_unit_prop()) {
        assert(cnf_is_indeterminate());
        push_unit(a.get_literal(), cnf[a.get_clause()]);
      } else if (a.is_conflict()) {
        assert(is_conflict_clause(cnf[a.get_clause()]));
      }
    }
  }
};

// Use only "primitive" CNF operations, make sure the trail is valid.
void trail_t::validate(const cnf_t &cnf, const trail_t &trail) {
  correctness_trail T(cnf);
  T.validate_trail(trail);
}

void trail_t::print_certificate() const {
  for (auto &a : *this) {
    printf("%d\n", lit_to_dimacs(a.get_literal()));
  }
}

bool trail_t::has_unit(const cnf_t &cnf, const trail_t &trail) {
  for (auto cid : cnf) {
    if (trail.clause_sat(cnf[cid])) continue;
    // not sure what I want in this case.
    if (trail.clause_unsat(cnf[cid])) continue;
    size_t c = trail.count_unassigned_literals(cnf[cid]);
    assert(c);
    if (c == 1) return true;
  }
  return false;
}
clause_id trail_t::get_unit_clause(const cnf_t &cnf, const trail_t &trail) {
  for (auto cid : cnf) {
    if (trail.clause_sat(cnf[cid])) continue;
    // not sure what I want in this case.
    if (trail.clause_unsat(cnf[cid])) continue;
    size_t c = trail.count_unassigned_literals(cnf[cid]);
    assert(c);
    if (c == 1) return cid;
  }
  return nullptr;
}
bool trail_t::is_satisfied(const cnf_t &cnf, const trail_t &trail) {
  return std::all_of(std::begin(const_clauses(cnf)),
                     std::end(const_clauses(cnf)),
                     [&](const clause_t &c) { return trail.clause_sat(c); });
}
bool trail_t::is_conflicted(const cnf_t &cnf, const trail_t &trail) {
  for (auto cid : cnf) {
    if (trail.clause_unsat(cnf[cid])) return true;
  }
  return false;
}
bool trail_t::is_indeterminate(const cnf_t &cnf, const trail_t &trail) {
  size_t indeterminated_clause_count = 0;
  for (auto cid : cnf) {
    if (trail.clause_unsat(cnf[cid])) return false;
    if (!trail.clause_sat(cnf[cid])) indeterminated_clause_count++;
  }
  return indeterminated_clause_count > 0;
}
literal_t trail_t::is_asserting(const trail_t &trail, const clause_t &c) {
  literal_t r = 0;
  for (literal_t l : c) {
    if (trail.literal_unassigned(l)) {
      if (r) return 0;
      r = l;
    }
    if (trail.literal_true(l)) {
      return 0;
    }
  }
  return r;
}