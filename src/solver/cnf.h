#pragma once
// This isn't "cnf" so much as it is all our types
#include <algorithm>
#include <forward_list>
#include <functional>
#include <iostream>
#include <numeric>
#include <queue>
#include <unordered_map>
#include <vector>

#include "debug.h"

#include "clause.h"
#include "variable.h"

template <typename C, typename V>
void unsorted_remove(C &c, const V &v) {
  auto it = std::find(std::begin(c), std::end(c), v);
  // it exists
  SAT_ASSERT(it != std::end(c));
  // it's unique
  SAT_ASSERT(std::find(std::next(it), std::end(c), v) == std::end(c));
  std::iter_swap(std::prev(std::end(c)), it);
  c.pop_back();  // remove.
  SAT_ASSERT(std::find(std::begin(c), std::end(c), v) == std::end(c));
}

std::ostream &operator<<(std::ostream &o, const clause_t &c);
struct cnf_t {
  // clause_t* is punned to clause_id
  clause_t *head = nullptr;
  size_t live_count = 0;
  std::vector<clause_id> to_erase;

  std::unordered_map<size_t, std::vector<clause_t *>> sig_to_clause;

  struct clause_iterator {
    clause_t *curr;
    clause_id operator*() { return curr; }
    clause_id operator->() { return curr; }
    clause_iterator operator++() {
      curr = curr->right;
      while (curr && !curr->is_alive) curr = curr->right;
      return *this;
    }
    clause_iterator operator++(int) {
      auto tmp = *this;
      curr = curr->right;
      while (curr && !curr->is_alive) curr = curr->right;
      return tmp;
    }
    clause_iterator operator--(int) {
      auto tmp = *this;
      curr = curr->left;
      while (curr && !curr->is_alive) curr = curr->left;
      return tmp;
    }
    clause_iterator operator--() {
      curr = curr->left;
      while (curr && !curr->is_alive) curr = curr->left;
      return *this;
    }
    bool operator==(const clause_iterator &that) const {
      return curr == that.curr;
    }
    bool operator!=(const clause_iterator &that) const {
      return curr != that.curr;
    }
  };

  auto begin() const {
    clause_iterator ci;
    ci.curr = head;
    return ci;
  }
  auto end() const {
    clause_iterator ci;
    ci.curr = nullptr;
    return ci;
  }
  auto begin() {
    clause_iterator ci;
    ci.curr = head;
    return ci;
  }
  auto end() {
    clause_iterator ci;
    ci.curr = nullptr;
    return ci;
  }

  cnf_t() = default;
  cnf_t(const cnf_t &cnf) {
    std::vector<literal_t> tmp;
    for (clause_id cid : cnf) {
      for (literal_t l : cnf[cid]) {
        tmp.push_back(l);
      }
      add_clause({tmp});
      tmp.clear();
    }
  }

  size_t live_clause_count() const { return live_count; }
  clause_t &operator[](clause_id cid) { return *cid; }
  const clause_t &operator[](clause_id cid) const { return *cid; }

  clause_id add_clause(clause_t c);
  void remove_clause_set(const clause_set_t &cs);
  void remove_clause(clause_id cid);
  void restore_clause(clause_id cid);
  void clean_clauses();

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

template <>
struct std::iterator_traits<cnf_t::clause_iterator> {
  typedef clause_id value_type;
  typedef clause_id &reference_type;
  typedef clause_id *pointer;
  typedef int difference_type;
  typedef std::bidirectional_iterator_tag iterator_category;
};

template <>
struct std::iterator_traits<variable_range::iterator> {
  typedef variable_t value_type;
  typedef variable_t &reference_type;
  typedef variable_t *pointer;
  typedef int difference_type;
  typedef std::random_access_iterator_tag iterator_category;
};

literal_map_t<clause_set_t> build_incidence_map(const cnf_t &cnf);
bool check_incidence_map(const literal_map_t<clause_set_t> &m,
                         const cnf_t &cnf);