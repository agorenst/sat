#include "cnf.h"

#include <algorithm>
#include <cassert>
#include <iostream>

#include "debug.h"

variable_t var(literal_t l) { return l >> 1; }
literal_t lit(variable_t l) {
  return l << 1;
} // given variable x, make it literal x
literal_t neg(literal_t l) { return l ^ 1; }
bool ispos(literal_t l) { return neg(l) & 1; }
literal_t dimacs_to_lit(int x) {
  bool is_neg = x < 0;
  literal_t l = std::abs(x);
  l <<= 1;
  if (is_neg)
    l++;
  assert(l > 1);
  return l;
}

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

#if 0
    std::vector<literal_t> lits;
    for (auto l : c) { lits.push_back(l); }

    size_t true_size = sizeof(clause_t) + sizeof(literal_t)*(lits.size()+1);
    clause_t* new_clause = (clause_t*) malloc(true_size);
    new_clause->zero_headers();
    new_clause->literals = (literal_t*) &new_clause[1];

    //clause_t* new_clause = (clause_t*) malloc(sizeof(clause_t));
    //new_clause->zero_headers();
    //new_clause->literals = (literal_t*) malloc(sizeof(literal_t)*(lits.size()+1));

    new_clause->len = lits.size();

    //fprintf(stderr, "True size is: %d of pointer %p\n", true_size, new_clause);

    //fprintf(stderr, "Setting literals first addr to %p\n", new_clause->literals);

    size_t i = 0;
    for (literal_t l : lits) {
      new_clause->literals[i++] = l;
    }
    new_clause->literals[i] = 0;


    //clause_t* new_clause = new (mem) clause_t(lits, (literal_t*) (mem+sizeof(c)));

    //auto new_clause = new clause_t(std::move(c));
    clause_id ret = new_clause;
    if (!head) {
      head = ret;
    } else {
      head->left = ret;
      ret->right = head;
      head = ret;
    }
#else
  auto ret = new clause_t(std::move(c));
#endif
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
  live_count--;
  // if (cid->literals) delete cid->literals;
  // delete cid;
  if (cid->is_alive) {
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

void cnf_t::clean_clauses() {
  for (auto cid : to_erase) {
    delete cid;
  }
  to_erase.clear();
}