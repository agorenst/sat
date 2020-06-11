#pragma once
// This isn't "cnf" so much as it is all our types
#include <algorithm>
#include <iostream>
#include <vector>
#include <queue>

#include "debug.h"

typedef int32_t literal_t;
// Really, we can be clever and use unsigned, but come on.
typedef int32_t variable_t;

struct clause_t {
  literal_t* raw;
  size_t len;
  std::vector<literal_t> mem;
  clause_t() {}
  clause_t(std::vector<literal_t> m): mem(m) {}
  template<typename IT>
  void erase(IT it, IT et) {
    mem.erase(it, et);
  }
  void clear() { mem.clear(); }
  void push_back(literal_t l) { mem.push_back(l); }
  auto begin() { return mem.begin(); }
  auto begin() const { return mem.begin(); }
  auto end() { return mem.end(); }
  auto end() const { return mem.end(); }
  auto size() const { return mem.size(); }
  auto empty() const { return mem.empty(); }
  auto& operator[](size_t i) { return mem[i]; }
  auto& operator[](size_t i) const { return mem[i]; }
  void pop_back() { mem.pop_back(); }
};

typedef size_t clause_id;

template <typename C, typename V>
bool contains(const C& c, const V& v) {
  return std::find(std::begin(c), std::end(c), v) != std::end(c);
}

std::ostream& operator<<(std::ostream& o, const clause_t& c);
struct cnf_t {
  // The raw memory containing the actual clauses
  // We ONLY append to this.
  typedef size_t clause_k;
  std::vector<clause_t> mem;

  // keep track of the old key vacancies.
  std::priority_queue<clause_id, std::vector<clause_id>, std::greater<clause_id>> free_keys;

  // This extra layer of indirection supports
  // clause removal in an efficient way (hopefully).
  std::vector<size_t> key_to_mem;

  size_t live_clause_count() const { return key_to_mem.size(); }
  clause_t& operator[](size_t i) { return mem[i]; }
  const clause_t& operator[](size_t i) const { return mem[i]; }

  clause_k add_clause(const clause_t& c) {
    if (!free_keys.empty()) {
      clause_id key = free_keys.top();
      free_keys.pop();
      mem[key] = c;
      //std::cerr << "Re-using key: " << key << std::endl;
      // maintain sorted order:
      auto kt = std::lower_bound(std::begin(key_to_mem), std::end(key_to_mem), key);
      // kt is the first element >= key.
      // We want to put key right before that:
      key_to_mem.insert(kt, key);
      SAT_ASSERT(std::is_sorted(std::begin(key_to_mem), std::end(key_to_mem)));
      return key;
    } else {
      clause_id key = mem.size();
      mem.push_back(c);
      key_to_mem.push_back(key);
      SAT_ASSERT(std::is_sorted(std::begin(key_to_mem), std::end(key_to_mem)));
      return key;
    }
  }

  auto begin() const { return key_to_mem.begin(); }
  auto end() const { return key_to_mem.end(); }
  auto begin() { return key_to_mem.begin(); }
  auto end() { return key_to_mem.end(); }
  template <typename IT>
  void erase(IT a, IT e) {
    key_to_mem.erase(a, e);
  }

  void remove_clause(clause_id cid) {
    // this is likely pretty expensive because we keep things in sorted order.
    // Is that best?
    free_keys.push(cid);
    SAT_ASSERT(std::count(std::begin(key_to_mem), std::end(key_to_mem), cid) == 1);
    SAT_ASSERT(std::is_sorted(std::begin(key_to_mem), std::end(key_to_mem)));
#ifdef SAT_DEBUG_MODE
    //auto et =
      //std::remove(std::begin(key_to_mem), std::end(key_to_mem), cid);
    //SAT_ASSERT(et == std::prev(std::end(key_to_mem)));
    //key_to_mem.pop_back();
#endif
    auto it = std::lower_bound(std::begin(key_to_mem), std::end(key_to_mem), cid);
    key_to_mem.erase(it);
    SAT_ASSERT(std::count(std::begin(key_to_mem), std::end(key_to_mem), cid) == 0);
    SAT_ASSERT(std::is_sorted(std::begin(key_to_mem), std::end(key_to_mem)));
  }

  void restore_clause(clause_id cid) {
    SAT_ASSERT(!contains(key_to_mem, cid));
    SAT_ASSERT(cid < mem.size());
    key_to_mem.push_back(cid);
    std::sort(std::begin(key_to_mem), std::end(key_to_mem));
  }
};

// These are some helper functions for clauses that
// don't need the state implicit in a trail:
clause_t resolve(clause_t c1, clause_t c2, literal_t l);

literal_t resolve_candidate(clause_t c1, clause_t c2, literal_t after);

// this is for units, unconditionally simplifying the CNF.
void commit_literal(cnf_t& cnf, literal_t l);
literal_t find_unit(const cnf_t& cnf);
bool immediately_unsat(const cnf_t& cnf);
bool immediately_sat(const cnf_t& cnf);

std::ostream& operator<<(std::ostream& o, const cnf_t& cnf);
void print_cnf(const cnf_t& cnf);

cnf_t load_cnf(std::istream& in);

variable_t max_variable(const cnf_t& cnf);
