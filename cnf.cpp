#include <algorithm>
#include <iostream>

#include <cassert>

#include "cnf.h"
#include "debug.h"

variable_t var(literal_t l) { return l >> 1; }
literal_t lit(variable_t l) { return l << 1; } // given variable x, make it literal x
literal_t neg(literal_t l) { return l ^ 1; }
bool ispos(literal_t l) { return neg(l) & 1; }
literal_t dimacs_to_lit(int x) {
  bool is_neg = x < 0;
  literal_t l = std::abs(x);
  l <<= 1;
  if (is_neg) l++;
  assert(l > 1);
  return l;
}

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
  std::vector<clause_id> containing_clauses;
  std::copy_if(std::begin(cnf), std::end(cnf), std::back_inserter(containing_clauses),
               [&](const clause_id cid) { return contains(cnf[cid], l); });
  std::for_each(std::begin(containing_clauses), std::end(containing_clauses),
                [&](const clause_id cid) { cnf.remove_clause(cid); });
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
    next_clause.push_back(dimacs_to_lit(next_literal));
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

void cnf_t::remove_clause_set(const clause_set_t& cs) {
  SAT_ASSERT(std::is_sorted(std::begin(key_to_mem), std::end(key_to_mem)));
  SAT_ASSERT(std::is_sorted(std::begin(cs), std::end(cs)));
  SAT_ASSERT(key_to_mem.size() > cs.size());

  // We can do this truly in-place, but for now let's see what happens.
  //std::vector<size_t> key_to_mem_2;
  //std::set_difference(std::begin(key_to_mem), std::end(key_to_mem),
  //std::begin(cs), std::end(cs),
  //std::back_inserter(key_to_mem_2));
  //std::swap(key_to_mem, key_to_mem_2);
  //assert(key_to_mem.size() + cs.size() == key_to_mem_2.size());
  //return;

  if (cs.size() == 0) return;

  //std::cerr << "Removing ";
  //for (auto cid : cs) std::cerr << cid << " ";
  //std::cerr << std::endl;

  auto ct = std::begin(cs);
  auto it = std::lower_bound(std::begin(key_to_mem), std::end(key_to_mem), *ct);
  auto jt = it;
  while (jt != std::end(key_to_mem)) {
    // Found an item to skip
    while (ct != std::end(cs) && *jt == *ct) {
      //std::cerr << "Skipping " << *ct << std::endl;
      ct++; // consider the next item
      jt++;
    }
    if (jt == std::end(key_to_mem)) {
      //std::cerr << "Bailing " << std::endl;
      break;
    }
    //std::cerr << "Copying " << *jt << " into " << *it << std::endl;
    *it = *jt;
    it++;
    jt++;
  }
  //std::cerr <<std::distance(it, jt) << "; " << cs.size() << std::endl;
  //SAT_ASSERT(std::distance(it, jt) == cs.size());
  key_to_mem.erase(it, std::end(key_to_mem));
  SAT_ASSERT(std::is_sorted(std::begin(key_to_mem), std::end(key_to_mem)));
  //if (key_to_mem != key_to_mem_2) {
  //for (auto c : key_to_mem) std::cerr << c << " ";
  //std::cerr << std::endl;
  //for (auto c : key_to_mem_2) std::cerr << c << " ";
  //std::cerr << std::endl;
  //}
  SAT_ASSERT(key_to_mem == key_to_mem_2);
}
