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
  clause_t* head = nullptr;
  size_t live_count = 0;

  struct clause_iterator {
    clause_t* curr;
    clause_id operator*() { return curr; }
    clause_id operator->() { return curr; }
    clause_iterator operator++() {
      curr = *(curr->right);
      return *this;
    }
    clause_iterator operator++(int) {
      auto tmp = *this;
      curr = *(curr->right);
      return tmp;
    }
    clause_iterator operator--(int) {
      auto tmp = *this;
      curr = *(curr->left);
      return tmp;
    }
    clause_iterator operator--() {
      curr = *(curr->left);
      return *this;
    }
    bool operator==(const clause_iterator& that) const {
      return curr == that.curr;
    }
    bool operator!=(const clause_iterator& that) const {
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

  clause_id add_clause(clause_t c) {
    live_count++;
    auto new_clause = new clause_t(std::move(c));
    clause_id ret = new_clause;
    if (!head) {
      head = ret;
    }
    else {
      *(head->left) = ret;
      *(ret->right) = head;
      head = ret;
    }
    return ret;
  }


  void remove_clause_set(const clause_set_t &cs);

  void remove_clause(clause_id cid) {
    live_count--;
    clause_t& c = *cid;
    if (*(c.left)) {
      clause_t& l = **(c.left);
      *(l.right) = *(c.right);
    }
    if (*(c.right)) {
      clause_t& r = **(c.right);
      *(r.left) = *(c.left);
    }
    if (cid == head) {
      head = *(c.right);
    }
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


template <>
struct std::iterator_traits<cnf_t::clause_iterator> {
  typedef clause_id value_type;
  typedef clause_id &reference_type;
  typedef clause_id *pointer;
  typedef int difference_type;
  typedef std::bidirectional_iterator_tag iterator_category;
};