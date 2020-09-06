#pragma once

#include <cstring>  // memcmp
#include <iostream>
#include "cnf.h"
#include "debug.h"

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

  literal_t l;
  clause_id c;

  bool has_literal() const;
  literal_t get_literal() const;
  bool is_decision() const;
  bool is_unit_prop() const;
  bool is_conflict() const;
  bool has_clause() const;
  clause_id get_clause() const;

  bool operator==(const action_t &that) const {
    return memcmp(this, &that, sizeof(action_t)) == 0;
  }
};

action_t make_decision(literal_t l);
action_t make_unit_prop(literal_t l, clause_id cid);
action_t make_conflict(clause_id cid);
std::ostream &operator<<(std::ostream &o, const action_t a);
