#pragma once

#include "cnf.h"
#include "debug.h"
#include <iostream>
#include <cstring> // memcmp

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
  bool is_conflict() const;
  bool has_clause() const;
  clause_id get_clause() const;

  bool operator==(const action_t& that) const {
    return memcmp(this, &that, sizeof(action_t)) == 0;
  }
};

std::ostream& operator<<(std::ostream& o, const action_t a);
