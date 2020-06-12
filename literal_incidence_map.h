#pragma once
#include "cnf.h"

#include "clause_set.h"

template <typename T>
struct literal_map_t {
  typedef std::vector<T> mem_t;
  mem_t mem;
  variable_t max_var;

  // Initialize the size based on the max var.
  void construct(variable_t m);
  literal_map_t(variable_t m);
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

bool check_incidence_map(const literal_map_t<clause_set_t>& m, const cnf_t& cnf);


template <typename T>
struct var_map_t {
  typedef std::vector<T> mem_t;
  mem_t mem;
  variable_t max_var;

  void construct(variable_t m) {
    max_var = m;
    mem.resize(max_var);
  }
  size_t variable_to_index(variable_t v) const { return v - 1; }
  size_t literal_to_index(literal_t l) const { return variable_to_index(var(l)); }

  T& operator[](variable_t v) {
    return mem[variable_to_index(v)];
  }
  const T& operator[](variable_t v) const {
    return mem[variable_to_index(v)];
  }

  auto begin() { return mem.begin(); }
  auto end() { return mem.end(); }
  auto begin() const { return mem.begin(); }
  auto end() const { return mem.end(); }
};

struct var_bitset_t {
  typedef std::vector<bool> mem_t;
  mem_t mem;
  variable_t max_var;

  void construct(variable_t m) {
    max_var = m;
    mem.resize(max_var);
  }
  size_t variable_to_index(variable_t v) const { return v - 1; }
  size_t literal_to_index(literal_t l) const { return variable_to_index(var(l)); }

  void set(variable_t v) {
    mem[variable_to_index(v)] = true;
  }
  void clear(variable_t v) {
    mem[variable_to_index(v)] = false;
  }
  bool get(variable_t v) {
    return mem[variable_to_index(v)];
  }

  auto begin() { return mem.begin(); }
  auto end() { return mem.end(); }
  auto begin() const { return mem.begin(); }
  auto end() const { return mem.end(); }
};
