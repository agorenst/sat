#pragma once
// This isn't "cnf" so much as it is all our types
#include <iostream>
#include <vector>
#include <algorithm>

typedef int32_t literal_t;
// Really, we can be clever and use unsigned, but come on.
typedef int32_t variable_t;


typedef std::vector<literal_t> clause_t;
typedef std::vector<clause_t> cnf_t;
typedef size_t clause_id;


// These are some helper functions for clauses that
// don't need the state implicit in a trail:
clause_t resolve(clause_t c1, clause_t c2, literal_t l);

literal_t resolve_candidate(clause_t c1, clause_t c2);

// this is for units, unconditionally simplifying the CNF.
void commit_literal(cnf_t& cnf, literal_t l);
literal_t find_unit(const cnf_t& cnf);
bool immediately_unsat(const cnf_t& cnf);
bool immediately_sat(const cnf_t& cnf);

// This is the various kinds of actions we can record.
struct action_t {

  // TODO: need to have an indicator of which state we are...
  enum class action_kind_t {
                            decision,
                            unit_prop,
                            backtrack,
                            halt_conflict,
                            halt_unsat,
                            halt_sat
  };
  action_kind_t action_kind;

  // Absent any unit clauses, we choose a literal.
  union {
    literal_t decision_literal;
    struct {
      literal_t propped_literal;
      clause_id reason; // maybe multiple reasons?
    } unit_prop;
    clause_id conflict_clause_id;
  };

  bool has_literal() const;
  literal_t get_literal() const;
  bool is_decision() const;
  bool is_unit_prop() const;
  bool has_clause() const;
  clause_id get_clause() const;
};

template<typename C, typename V>
bool contains(const C& c, const V& v) {
  return std::find(std::begin(c), std::end(c), v) != std::end(c);
}


std::ostream& operator<<(std::ostream& o, const clause_t& c);
std::ostream& operator<<(std::ostream& o, const cnf_t& c);
void print_cnf(const cnf_t& cnf);
std::ostream& operator<<(std::ostream& o, const action_t a);

cnf_t load_cnf(std::istream& in);
