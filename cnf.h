#pragma once
// This isn't "cnf" so much as it is all our types
#include <iostream>
#include <vector>
#include <algorithm>

typedef int32_t literal_t;
// Really, we can be clever and use unsigned, but come on.
typedef int32_t variable_t;


typedef std::vector<literal_t> clause_t;
typedef size_t clause_id;

struct cnf_t {
  // The raw memory containing the actual clauses
  typedef size_t clause_k;
  std::vector<clause_t> mem;

  // This extra layer of indirection supports
  // clause removal in an efficient way (hopefully).
  std::vector<size_t> key_to_mem;

  size_t size() const { return mem.size(); }
  size_t clause_count() const { return mem.size(); }
  clause_t& operator[](size_t i) { return mem[i]; }
  const clause_t& operator[](size_t i) const { return mem[i]; }
  clause_k push_back(const clause_t& c) { mem.push_back(c); return mem.size()-1;}

  auto begin() const { return mem.begin(); }
  auto end() const { return mem.end(); }
  auto begin() { return mem.begin(); }
  auto end() { return mem.end(); }
  template<typename IT>
  void erase(IT a, IT e) { mem.erase(a, e); }

  // We have an iterator that needs to be smart, to skip over
  // the now-removed memory locations.
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


template<typename C, typename V>
bool contains(const C& c, const V& v) {
  return std::find(std::begin(c), std::end(c), v) != std::end(c);
}


std::ostream& operator<<(std::ostream& o, const clause_t& c);
std::ostream& operator<<(std::ostream& o, const cnf_t& c);
void print_cnf(const cnf_t& cnf);

cnf_t load_cnf(std::istream& in);

variable_t max_variable(const cnf_t& cnf);

