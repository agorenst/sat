#include "cnf.h"

#include <algorithm>
#include <cassert>
#include <forward_list>
#include <iostream>
#include <string>

#include "debug.h"

std::ostream &operator<<(std::ostream &o, const clause_t &c) {
  for (auto l : c) {
    o << l << " ";
  }
  return o;
}

clause_t resolve(clause_t c1, clause_t c2, literal_t l) {
  assert(0);
  SAT_ASSERT(contains(c1, l));
  SAT_ASSERT(contains(c2, neg(l)));
  std::vector<literal_t> c3tmp;
  // clause_t c3tmp;
  for (literal_t x : c1) {
    if (var(x) != var(l)) {
      c3tmp.push_back(x);
    }
  }
  for (literal_t x : c2) {
    if (var(x) != var(l)) {
      c3tmp.push_back(x);
    }
  }
  std::sort(std::begin(c3tmp), std::end(c3tmp));
  auto new_end = std::unique(std::begin(c3tmp), std::end(c3tmp));
  // c3tmp.erase(new_end, std::end(c3tmp));
  for (int i = std::distance(new_end, std::end(c3tmp)); i >= 0; i--) {
    c3tmp.pop_back();
  }
  return c3tmp;
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

void commit_literal(cnf_t &cnf, literal_t l) {
  std::vector<clause_id> containing_clauses;
  std::copy_if(std::begin(cnf), std::end(cnf),
               std::back_inserter(containing_clauses),
               [&](const clause_id cid) { return contains(cnf[cid], l); });
  std::for_each(std::begin(containing_clauses), std::end(containing_clauses),
                [&](const clause_id cid) { cnf.remove_clause(cid); });
  for (auto cid : cnf) {
    clause_t &c = cnf[cid];
    auto new_end = std::remove(std::begin(c), std::end(c), neg(l));
    // c.erase(new_end, std::end(c));
    auto to_erase = std::distance(new_end, std::end(c));
    for (auto i = 0; i < to_erase; i++) {
      c.pop_back();
    }
  }
}

literal_t find_unit(const cnf_t &cnf) {
  auto it =
      std::find_if(std::begin(cnf), std::end(cnf),
                   [&](const clause_id cid) { return cnf[cid].size() == 1; });
  if (it != std::end(cnf)) {
    return cnf[*it][0];
  }
  return 0;
}

bool immediately_unsat(const cnf_t &cnf) {
  for (clause_id cid : cnf) {
    if (cnf[cid].empty()) {
      return true;
    }
  }
  return false;
}
bool immediately_sat(const cnf_t &cnf) { return cnf.live_clause_count() == 0; }

std::ostream &operator<<(std::ostream &o, const cnf_t &cnf) {
  for (auto &&cid : cnf) {
    for (auto &&l : cnf[cid]) {
      o << l << " ";
    }
    o << "0" << std::endl;
  }
  return o;
}

cnf_t load_cnf(std::istream &in) {
  cnf_t cnf;
  // Load in cnf from stdin.
  literal_t next_literal;
  std::vector<literal_t> next_clause_tmp;

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
      std::sort(std::begin(next_clause_tmp), std::end(next_clause_tmp));
      cnf.add_clause(next_clause_tmp);
      next_clause_tmp.clear();
      continue;
    }
    next_clause_tmp.push_back(dimacs_to_lit(next_literal));
  }
  return cnf;
}

void canon_cnf(cnf_t &cnf) {
  std::vector<clause_id> to_remove;
  for (auto cid : cnf) {
    if (clause_taut(cnf[cid])) {
      to_remove.push_back(cid);
    }
  }
  for (auto cid : to_remove) {
    cnf.remove_clause(cid);
  }
  cnf.clean_clauses();
  to_remove.clear();

  // in-place canon---basically, remove redundant literals.
  for (auto cid : cnf) {
    canon_clause(cnf[cid]);
  }

  // Finally, remove redundant clauses
  for (auto cit = std::begin(cnf); cit != std::end(cnf); cit++) {
    for (auto dit = std::next(cit); dit != std::end(cnf); dit++) {
      if (clauses_equal(cnf[*cit], cnf[*dit])) {
        to_remove.push_back(*cit);
      }
    }
  }
  for (auto cid : to_remove) {
    cnf.remove_clause(cid);
  }
  cnf.clean_clauses();
}

variable_t max_variable(const cnf_t &cnf) {
  literal_t m = 0;
  for (const clause_id cid : cnf) {
    for (const literal_t l : cnf[cid]) {
      m = std::max(var(l), m);
    }
  }
  return m;
}

clause_id cnf_t::add_clause(clause_t c) {
  live_count++;

  auto ret = new clause_t(std::move(c));

  if (!head) {
    head = ret;
  } else {
    head->left = ret;
    ret->right = head;
    head = ret;
  }
  return ret;
}

void cnf_t::remove_clause_set(const clause_set_t &cs) {
  for (auto cid : cs) {
    remove_clause(cid);
  }
}

void cnf_t::remove_clause(clause_id cid) {
  if (cid->is_alive) {
    live_count--;
    cid->is_alive = false;
    if (cid->left) {
      cid->left->right = cid->right;
    }
    if (cid->right) {
      cid->right->left = cid->left;
    }
    if (cid == head) {
      head = cid->right;
    }
    to_erase.push_back(cid);
  }
}

void cnf_t::restore_clause(clause_id cid) {
  SAT_ASSERT(!cid->is_alive);
  cid->is_alive = true;
  live_count++;

  if (!head) {
    head = cid;
  } else {
    head->left = cid;
    cid->right = head;
    head = cid;
  }
}

void cnf_t::clean_clauses() {
  for (auto cid : to_erase) {
    delete cid;
  }
  to_erase.clear();
}

literal_map_t<clause_set_t> build_incidence_map(const cnf_t &cnf) {
  literal_map_t<clause_set_t> literal_to_clause(max_variable(cnf));
  for (clause_id cid : cnf) {
    const clause_t &c = cnf[cid];
    for (literal_t l : c) {
      literal_to_clause[l].push_back(cid);
    }
  }
  return literal_to_clause;
}

bool check_incidence_map(const literal_map_t<clause_set_t> &m,
                         const cnf_t &cnf) {
#ifdef SAT_DEBUG_MODE
  for (clause_id cid : cnf) {
    const clause_t &c = cnf[cid];
    for (literal_t l : c) {
      SAT_ASSERT(contains(m[l], cid));
    }
  }
  for (literal_t l : cnf.lit_range()) {
    const auto cl = m[l];
    for (auto cid : cl) {
      SAT_ASSERT(contains(cnf[cid], l));
    }
    for (auto it = std::begin(cl); it != std::end(cl); it++) {
      for (auto jt = std::next(it); jt != std::end(cl); jt++) {
        SAT_ASSERT(*it != *jt);
      }
    }
  }
#endif
  return true;
}