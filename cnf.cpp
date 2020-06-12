#include <algorithm>
#include <iostream>

#include <cassert>

#include "cnf.h"
#include "debug.h"

variable_t var(literal_t l) { return std::abs(l); }
literal_t neg(literal_t l) { return -l; }
bool ispos(literal_t l) { return l > 0; }

std::ostream& operator<<(std::ostream& o, const clause_t& c) {
  for (auto l : c) {
    o << l << " ";
  }
  return o;
}

clause_t resolve(clause_t c1, clause_t c2, literal_t l) {
  SAT_ASSERT(contains(c1, l));
  SAT_ASSERT(contains(c2, neg(l)));
  clause_t c3;
  for (literal_t x : c1) {
    if (var(x) != var(l)) {
      c3.push_back(x);
    }
  }
  for (literal_t x : c2) {
    if (var(x) != var(l)) {
      c3.push_back(x);
    }
  }
  std::sort(std::begin(c3), std::end(c3));
  auto new_end = std::unique(std::begin(c3), std::end(c3));
  c3.erase(new_end, std::end(c3));
  return c3;
}

literal_t resolve_candidate(clause_t c1, clause_t c2, literal_t after = 0) {
  bool seen = (after == 0);

  for (literal_t l : c1) {
    // Skip everything until after "after"
    if (l == after) {
      seen = true;
      continue;
    }
    if (!seen) {
      continue;
    }

    for (literal_t m : c2) {
      if (l == neg(m)) {
        return l;
      }
    }
  }
  return 0;
}

void commit_literal(cnf_t& cnf, literal_t l) {
  auto new_end = std::remove_if(
      std::begin(cnf), std::end(cnf),
      [&](const clause_id cid) { return contains(cnf[cid], l); });
  cnf.erase(new_end, std::end(cnf));
  for (auto cid : cnf) {
    clause_t& c = cnf[cid];
    auto new_end = std::remove(std::begin(c), std::end(c), neg(l));
    c.erase(new_end, std::end(c));
  }
}

literal_t find_unit(const cnf_t& cnf) {
  auto it =
      std::find_if(std::begin(cnf), std::end(cnf),
                   [&](const clause_id cid) { return cnf[cid].size() == 1; });
  if (it != std::end(cnf)) {
    return cnf[*it][0];
  }
  return 0;
}

bool immediately_unsat(const cnf_t& cnf) {
  for (clause_id cid : cnf) {
    if (cnf[cid].empty()) {
      return true;
    }
  }
  return false;
}
bool immediately_sat(const cnf_t& cnf) { return cnf.live_clause_count() == 0; }

std::ostream& operator<<(std::ostream& o, const cnf_t& cnf) {
  for (auto&& cid : cnf) {
    for (auto&& l : cnf[cid]) {
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
    if (line.empty()) {
      continue;
    }
    if (line[0] == 'c') {
      continue;
    }
    if (line[0] == 'p') {
      // TODO(aaron): do some error-checking;
      break;
    }
  }

  while (in >> next_literal) {
    if (next_literal == 0) {
      cnf.add_clause(next_clause);
      next_clause.clear();
      continue;
    }
    next_clause.push_back(next_literal);
  }
  return cnf;
}

variable_t max_variable(const cnf_t& cnf) {
  literal_t m = 0;
  for (const clause_id cid : cnf) {
    for (const literal_t l : cnf[cid]) {
      m = std::max(var(l), m);
    }
  }
  return m;
}
