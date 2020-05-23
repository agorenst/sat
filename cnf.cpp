#include <algorithm>
#include <iostream>

#include <cassert>

#include "cnf.h"


std::ostream& operator<<(std::ostream& o, const clause_t& c) {
  for (auto l : c) {
    o << l << " ";
  }
  return o;
}


clause_t resolve(clause_t c1, clause_t c2, literal_t l) {
  assert(contains(c1, l));
  assert(contains(c2, -l));
  clause_t c3;
  for (literal_t x : c1) {
    if (std::abs(x) != std::abs(l)) {
      c3.push_back(x);
    }
  }
  for (literal_t x : c2) {
    if (std::abs(x) != std::abs(l)) {
      c3.push_back(x);
    }
  }
  std::sort(std::begin(c3), std::end(c3));
  auto new_end = std::unique(std::begin(c3), std::end(c3));
  c3.erase(new_end, std::end(c3));
  return c3;
}

literal_t resolve_candidate(clause_t c1, clause_t c2) {
  for (literal_t l : c1) {
    for (literal_t m : c2) {
      if (l == -m) return l;
    }
  }
  return 0;
}

void commit_literal(cnf_t& cnf, literal_t l) {
  auto new_end = 
    std::remove_if(std::begin(cnf), std::end(cnf), [l](const clause_t& c) { return contains(c, l); });
  cnf.erase(new_end, std::end(cnf));
  for (auto& c : cnf) {
    auto new_end = std::remove(std::begin(c), std::end(c), -l);
    c.erase(new_end, std::end(c));
  }
}

literal_t find_unit(const cnf_t& cnf) {
  auto it = std::find_if(std::begin(cnf), std::end(cnf), [](const clause_t& c) { return c.size() == 1; });
  if (it != std::end(cnf)) {
    return (*it)[0];
  }
  return 0;
}

bool immediately_unsat(const cnf_t& cnf) {
  return contains(cnf, clause_t());
}
bool immediately_sat(const cnf_t& cnf) {
  return cnf.size() == 0;
}

bool action_t::has_literal() const {
  return action_kind == action_kind_t::decision ||
    action_kind == action_kind_t::unit_prop;
}
literal_t action_t::get_literal() const {
  assert(action_kind == action_kind_t::decision ||
         action_kind == action_kind_t::unit_prop);
  if (action_kind == action_kind_t::decision) {
    return decision_literal;
  }
  else if (action_kind == action_kind_t::unit_prop) {
    return unit_prop.propped_literal;
  }
  return 0; // error!
}
bool action_t::is_decision() const {
  return action_kind == action_kind_t::decision;
}
bool action_t::has_clause() const {
  return action_kind == action_kind_t::halt_conflict || action_kind == action_kind_t::unit_prop;
}
clause_id action_t::get_clause() const {
  if (action_kind == action_kind_t::halt_conflict) {
    return conflict_clause_id;
  }
  else if (action_kind == action_kind_t::unit_prop) {
    return unit_prop.reason;
  }
  assert(0);
}

std::ostream& operator<<(std::ostream& o, const action_t::action_kind_t a) {
  switch(a) {
  case action_t::action_kind_t::decision: return o << "decision";
  case action_t::action_kind_t::unit_prop: return o << "unit_prop";
  case action_t::action_kind_t::backtrack: return o << "backtrack";
  case action_t::action_kind_t::halt_conflict: return o << "halt_conflict";
  case action_t::action_kind_t::halt_unsat: return o << "halt_unsat";
  case action_t::action_kind_t::halt_sat: return o << "halt_sat";
  }
  return o;
}
std::ostream& operator<<(std::ostream& o, const action_t a) {
  o << "{ " << a.action_kind;
  switch(a.action_kind) {
  case action_t::action_kind_t::decision: return o << ", " << a.decision_literal << " }";
  case action_t::action_kind_t::unit_prop: return o << ", " << a.unit_prop.propped_literal << ", " << a.unit_prop.reason << " }";
  case action_t::action_kind_t::halt_conflict: return o << ", " << a.conflict_clause_id << " }";
  default: return o << " }";
  }
}

std::ostream& operator<<(std::ostream& o, const cnf_t& cnf) {
  for (auto&& c : cnf) {
    for (auto&& l : c) {
      o << l << " ";
    }
    o << "0" << std::endl;
  }
  return o;
}

cnf_t load_cnf(std::istream& in) {
  cnf_t cnf;
  // Load in cnf from stdin.
  literal_t next_literal;
  clause_t next_clause;

  std::string line;
  while (std::getline(in, line)) {
    if (line.size() == 0) { continue; }
    if (line[0] == 'c') { continue; }
    if (line[0] == 'p') {
      // TODO: do some error-checking;
      break;
    }
  }

  while (in >> next_literal) {
    if (next_literal == 0) {
      cnf.push_back(next_clause);
      next_clause.clear();
      continue;
    }
    next_clause.push_back(next_literal);
  }
  return cnf;
}
