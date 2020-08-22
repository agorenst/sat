#pragma once
// This isn't "cnf" so much as it is all our types
#include <algorithm>
#include <functional>
#include <iostream>
#include <numeric>
#include <queue>
#include <vector>

#include "clause_set.h"
#include "debug.h"

#include "clause.h"

typedef int32_t literal_t;
// Really, we can be clever and use unsigned, but come on.
typedef int32_t variable_t;

variable_t var(literal_t l);
literal_t lit(variable_t l);
literal_t neg(literal_t l);
bool ispos(literal_t l);
literal_t dimacs_to_lit(int x);

struct variable_range {
  const variable_t max_var;
  variable_range(const variable_t max_var) : max_var(max_var) {}
  struct iterator {
    const variable_t after_last_var;
    variable_t v;

    iterator &operator++() {
      v++;
      return *this;
    }

    literal_t operator*() { return v; }

    bool operator==(const iterator &that) const { return this->v == that.v; }
    bool operator!=(const iterator &that) const { return this->v != that.v; }
  };
  iterator begin() const { return iterator{max_var + 1, 1}; }
  iterator end() const { return iterator{max_var + 1, max_var + 1}; }
};
struct literal_range {
  const variable_t max_var;
  literal_range(const variable_t max_var) : max_var(max_var) {}
  struct iterator {
    const literal_t after_last_literal;
    literal_t l;

    iterator &operator++() {
      l++;
      SAT_ASSERT(l >= 2);
      return *this;
    }

    literal_t operator*() { return l; }

    bool operator==(const iterator &that) const { return this->l == that.l; }
    bool operator!=(const iterator &that) const { return this->l != that.l; }
  };
  iterator begin() const {
    // return iterator{max_var, -max_var};
    return iterator{(2 * max_var) + 2, 2};
  }
  iterator end() const {
    return iterator{(2 * max_var) + 2, (2 * max_var) + 2};
  }
};

template <typename C, typename V> bool contains(const C &c, const V &v) {
  return std::find(std::begin(c), std::end(c), v) != std::end(c);
}

template <typename C, typename V> void unsorted_remove(C &c, const V &v) {
  auto it = std::find(std::begin(c), std::end(c), v);
  // it exists
  SAT_ASSERT(it != std::end(c));
  // it's unique
  SAT_ASSERT(std::find(std::next(it), std::end(c), v) == std::end(c));
  std::iter_swap(std::prev(std::end(c)), it);
  c.pop_back(); // remove.
  SAT_ASSERT(std::find(std::begin(c), std::end(c), v) == std::end(c));
}

std::ostream &operator<<(std::ostream &o, const clause_t &c);
struct cnf_t {
  // clause_t* is punned to clause_id
  std::vector<clause_t*> mem;

  cnf_t() = default;
  cnf_t(const cnf_t &cnf) {
    std::vector<literal_t> tmp;
    for (const auto c : cnf.mem) {
      for (auto l : *c) {
        tmp.push_back(l);
      }
      add_clause({tmp});
      tmp.clear();
    }
  }
  size_t live_clause_count() const { return mem.size(); }
  clause_t &operator[](clause_id cid) { return *cid; }
  const clause_t &operator[](clause_id cid) const { return *cid; }
  clause_t &operator[](size_t i) { return *mem[i]; }
  const clause_t &operator[](size_t i) const { return *mem[i]; }

  clause_id add_clause(clause_t c) {
    auto new_clause = new clause_t(std::move(c));
    clause_id ret = new_clause;
    mem.emplace_back(new_clause);
    return ret;
  }

  auto begin() const { return mem.begin(); }
  auto end() const { return mem.end(); }
  auto begin() { return mem.begin(); }
  auto end() { return mem.end(); }

  void remove_clause_set(const clause_set_t &cs);

  void remove_clause(clause_id cid) {
    // this is likely pretty expensive because we keep things in sorted order.
    // Is that best?
    //std::sort(std::begin(mem), std::end(mem));
    SAT_ASSERT(std::count(std::begin(mem), std::end(mem), cid) ==
               1);
    SAT_ASSERT(std::is_sorted(std::begin(mem), std::end(mem)));
#ifdef SAT_DEBUG_MODE
    // auto et =
    // std::remove(std::begin(mem), std::end(mem), cid);
    // SAT_ASSERT(et == std::prev(std::end(mem)));
    // mem.pop_back();
#endif
    //auto it =
        //std::lower_bound(std::begin(mem), std::end(mem), cid);
        auto it = std::find(std::begin(mem), std::end(mem), cid);
    mem.erase(it);
    SAT_ASSERT(std::count(std::begin(mem), std::end(mem), cid) ==
               0);
    SAT_ASSERT(std::is_sorted(std::begin(mem), std::end(mem)));
    delete cid;
  }

  literal_range lit_range() const {
    variable_t max_variable(const cnf_t &cnf);
    variable_t max_var = max_variable(*this);
    literal_range lits(max_var);
    return lits;
  }
  variable_range var_range() const {
    variable_t max_variable(const cnf_t &cnf);
    variable_t max_var = max_variable(*this);
    variable_range vars(max_var);
    return vars;
  }
};

// These are some helper functions for clauses that
// don't need the state implicit in a trail:
clause_t resolve(clause_t c1, clause_t c2, literal_t l);

literal_t resolve_candidate(clause_t c1, clause_t c2, literal_t after);

// this is for units, unconditionally simplifying the CNF.
void commit_literal(cnf_t &cnf, literal_t l);
literal_t find_unit(const cnf_t &cnf);
bool immediately_unsat(const cnf_t &cnf);
bool immediately_sat(const cnf_t &cnf);

std::ostream &operator<<(std::ostream &o, const cnf_t &cnf);
void print_cnf(const cnf_t &cnf);

cnf_t load_cnf(std::istream &in);

variable_t max_variable(const cnf_t &cnf);

size_t signature(const clause_t &c);
