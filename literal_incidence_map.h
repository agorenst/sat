#pragma once
#include "cnf.h"

#include "clause_set.h"

template <typename T>
struct literal_map_t {
  typedef std::vector<T> mem_t;
  mem_t mem;
  variable_t max_var;

  // Initialize the size based on the max var.
  literal_map_t(const cnf_t& cnf);

  T& operator[](literal_t l);
  const T& operator[](literal_t l) const;

  auto begin() { return mem.begin(); }
  auto end() { return mem.end(); }
  auto begin() const { return mem.begin(); }
  auto end() const { return mem.end(); }

  literal_t iter_to_literal(typename mem_t::const_iterator it) const;
  size_t literal_to_index(literal_t l) const;
};

literal_map_t<clause_set_t> build_incidence_map(const cnf_t& cnf);
