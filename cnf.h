#pragma once
// This isn't "cnf" so much as it is all our types
#include <iostream>
#include <vector>
#include <algorithm>

#include "debug.h"

typedef int32_t literal_t;
// Really, we can be clever and use unsigned, but come on.
typedef int32_t variable_t;


typedef std::vector<literal_t> clause_t;
typedef size_t clause_id;

template<typename C, typename V>
bool contains(const C& c, const V& v) {
  return std::find(std::begin(c), std::end(c), v) != std::end(c);
}


struct cnf_t {
  // The raw memory containing the actual clauses
  // We ONLY append to this.
  typedef size_t clause_k;
  std::vector<clause_t> mem;

  // This extra layer of indirection supports
  // clause removal in an efficient way (hopefully).
  std::vector<size_t> key_to_mem;

  size_t live_clause_count() const {
    return key_to_mem.size();
  }
  clause_t& operator[](size_t i) {
    return mem[i];
  }
  const clause_t& operator[](size_t i) const {
    return mem[i];
  }
  clause_k push_back(const clause_t& c) {
    clause_id key = mem.size();
    mem.push_back(c);
    key_to_mem.push_back(key);
    return key;
  }

  auto begin() const { return key_to_mem.begin(); }
  auto end() const { return key_to_mem.end(); }
  auto begin() { return key_to_mem.begin(); }
  auto end() { return key_to_mem.end(); }
  template<typename IT>
  void erase(IT a, IT e) { key_to_mem.erase(a, e); }

  template<typename C>
  void remove_clauses(const C& cids_to_remove) {

    SAT_ASSERT(std::all_of(std::begin(cids_to_remove), std::end(cids_to_remove),
                           [&](clause_id cid) { return contains(*this, cid); }));

    auto et = std::remove_if(std::begin(key_to_mem), std::end(key_to_mem),
                [&](clause_id cid) { return contains(cids_to_remove, cid); });

    key_to_mem.erase(et, std::end(key_to_mem));
  }

  void remove_clause(clause_id cid) {
    auto et = std::remove(std::begin(key_to_mem), std::end(key_to_mem), cid);
    key_to_mem.erase(et, std::end(key_to_mem));
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

literal_t resolve_candidate(clause_t c1, clause_t c2);

// this is for units, unconditionally simplifying the CNF.
void commit_literal(cnf_t& cnf, literal_t l);
literal_t find_unit(const cnf_t& cnf);
bool immediately_unsat(const cnf_t& cnf);
bool immediately_sat(const cnf_t& cnf);



std::ostream& operator<<(std::ostream& o, const clause_t& c);
std::ostream& operator<<(std::ostream& o, const cnf_t& c);
void print_cnf(const cnf_t& cnf);

cnf_t load_cnf(std::istream& in);

variable_t max_variable(const cnf_t& cnf);

