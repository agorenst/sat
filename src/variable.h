#pragma once
// This is the core interface that provides everything we'd ever want to know
// about variables (and literals) on their own.

#include <cstdint>
#include <numeric>
#include <vector>
#include "debug.h"

struct action_t;

enum class v_state_t { unassigned, var_true, var_false };

typedef int32_t literal_t;
typedef int32_t variable_t;

variable_t var(literal_t l);
literal_t lit(variable_t l);
literal_t neg(literal_t l);
bool ispos(literal_t l);
literal_t dimacs_to_lit(int x);
int lit_to_dimacs(literal_t l);

struct variable_range {
  const variable_t max_var;
  variable_range(const variable_t max_var);
  struct iterator {
    const variable_t after_last_var;
    variable_t v;
    iterator &operator++();
    variable_t operator*();
    bool operator==(const iterator &that) const;
    bool operator!=(const iterator &that) const;
  };
  iterator begin() const;
  iterator end() const;
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

template <typename T>
struct literal_map_t {
  typedef std::vector<T> mem_t;
  mem_t mem;
  variable_t max_var;

  // Initialize the size based on the max var.
  void construct(variable_t m);
  literal_map_t(variable_t m);

  T &operator[](literal_t l);
  const T &operator[](literal_t l) const;

  auto begin() { return mem.begin(); }
  auto end() { return mem.end(); }
  auto begin() const { return mem.begin(); }
  auto end() const { return mem.end(); }

  void clear() { mem.clear(); }

  literal_t iter_to_literal(typename mem_t::const_iterator it) const;
  size_t literal_to_index(literal_t l) const;
};

template <typename T>
struct var_map_t {
  typedef std::vector<T> mem_t;
  mem_t mem;
  variable_t max_var;

  void construct(variable_t m) {
    max_var = m;
    mem.resize(max_var + 1);
  }
  size_t variable_to_index(variable_t v) const { return v; }
  size_t literal_to_index(literal_t l) const {
    return variable_to_index(var(l));
  }

  T &operator[](variable_t v) { return mem[variable_to_index(v)]; }
  const T &operator[](variable_t v) const { return mem[variable_to_index(v)]; }

  auto begin() { return mem.begin(); }
  auto end() { return mem.end(); }
  auto begin() const { return mem.begin(); }
  auto end() const { return mem.end(); }
};

struct var_bitset_t {
  typedef std::vector<char> mem_t;
  mem_t mem;
  variable_t max_var;
  size_t set_count = 0;

  void construct(variable_t m) {
    max_var = m;
    mem.resize(max_var);
  }
  size_t variable_to_index(variable_t v) const { return v - 1; }
  size_t literal_to_index(literal_t l) const {
    return variable_to_index(var(l));
  }

  void set(variable_t v) {
    mem[variable_to_index(v)] = true;
    set_count++;
  }
  void clear(variable_t v) {
    mem[variable_to_index(v)] = false;
    set_count--;
  }
  bool get(variable_t v) const { return mem[variable_to_index(v)]; }

 private:
  auto begin() { return mem.begin(); }
  auto end() { return mem.end(); }
  auto begin() const { return mem.begin(); }
  auto end() const { return mem.end(); }

 public:
  void clear() {
    std::fill(begin(), end(), 0);
    set_count = 0;
  }
};

template <typename T>
size_t literal_map_t<T>::literal_to_index(literal_t l) const {
  return l;
  // return l + max_var;
}

template <typename T>
T &literal_map_t<T>::operator[](literal_t l) {
  size_t i = literal_to_index(l);
  return mem[i];
}

template <typename T>
const T &literal_map_t<T>::operator[](literal_t l) const {
  size_t i = literal_to_index(l);
  return mem[i];
}

template <typename T>
void literal_map_t<T>::construct(variable_t m) {
  max_var = m;
  mem.resize(max_var * 2 + 2);
}

template <typename T>
literal_map_t<T>::literal_map_t(variable_t m) {
  construct(m);
}

template <typename T>
literal_t literal_map_t<T>::iter_to_literal(
    typename mem_t::const_iterator it) const {
  size_t index = std::distance(std::begin(mem), it);
  return index;
  // return index - max_var;
}

// Briggs and Torczon
struct sparse_integer_set {
  std::vector<size_t> sparse;
  std::vector<size_t> dense;
  size_t size = 0;
  sparse_integer_set(size_t s) : sparse(s), dense(s) {}
  bool set(size_t k) {
    auto j = sparse[k];
    if (j >= size || dense[j] != k) {
      sparse[k] = size;
      dense[size] = k;
      size++;
      return true;
    }
    return false;
  }
  bool clear(size_t k) {
    auto j = sparse[k];
    if (j < size && dense[j] == k) {
      size--;
      auto x = dense[size];
      dense[j] = x;
      sparse[x] = j;
      return true;
    }
    return false;
  }
  bool get(size_t k) const {
    auto j = sparse[k];
    return j < size && dense[j] == k;
  }

  void reset() { size = 0; }
  auto begin() { return dense.begin(); }
  auto end() { return dense.begin() + size; }
  auto begin() const { return dense.begin(); }
  auto end() const { return dense.begin() + size; }
};

struct lit_compactset_t {
  // typedef std::vector<char> mem_t;
  typedef sparse_integer_set mem_t;
  mem_t mem;

  // Initialize the size based on the max var.
  lit_compactset_t(variable_t m) : mem(m * 2 + 2) {}

  void set(literal_t v) { mem.set(v); }
  void clear(literal_t v) { mem.clear(v); }
  bool get(literal_t v) const { return mem.get(v); }

 public:
  auto begin() { return mem.begin(); }
  auto end() { return mem.end(); }
  auto begin() const { return mem.begin(); }
  auto end() const { return mem.end(); }

 public:
  void reset() { mem.reset(); }
  size_t count() const { return mem.size; }
};

struct lit_bitset_t {
  typedef std::vector<char> mem_t;
  mem_t mem;
  variable_t max_var;

  // Initialize the size based on the max var.
  void construct(variable_t m) {
    max_var = m;
    mem.resize(max_var * 2 + 2);
    reset();
  }
  lit_bitset_t(variable_t m) { construct(m); }

  void set(literal_t v) { mem[literal_to_index(v)] = 1; }
  void clear(literal_t v) { mem[literal_to_index(v)] = 0; }
  bool get(literal_t v) const { return mem[literal_to_index(v)]; }

 private:
  auto begin() { return mem.begin(); }
  auto end() { return mem.end(); }
  auto begin() const { return mem.begin(); }
  auto end() const { return mem.end(); }

 public:
  void reset() { std::fill(begin(), end(), 0); }
  size_t count() const { return std::accumulate(begin(), end(), 0); }

  size_t literal_to_index(literal_t l) const { return l; }
};

struct lit_bothset_t {
  lit_compactset_t c;
  lit_bitset_t b;
  // Initialize the size based on the max var.
  lit_bothset_t(variable_t m) : c(m), b(m) {}

  void set(literal_t v) {
    c.set(v);
    b.set(v);
  }
  void clear(literal_t v) {
    c.clear(v);
    b.clear(v);
  }
  bool get(literal_t v) const { return b.get(v); }

 public:
  auto begin() { return c.begin(); }
  auto end() { return c.end(); }
  auto begin() const { return c.begin(); }
  auto end() const { return c.end(); }

 public:
  void reset() {
    c.reset();
    b.reset();
  }
  size_t count() const { return c.count(); }
};