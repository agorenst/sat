#include "action.h"
#include <iostream>

bool action_t::has_literal() const {
  return action_kind == action_kind_t::decision ||
         action_kind == action_kind_t::unit_prop;
}
literal_t action_t::get_literal() const {
  SAT_ASSERT(action_kind == action_kind_t::decision ||
             action_kind == action_kind_t::unit_prop);
  if (action_kind == action_kind_t::decision) {
    return decision_literal;
  } else if (action_kind == action_kind_t::unit_prop) {
    return unit_prop.propped_literal;
  }
  return 0;  // error!
}
bool action_t::is_decision() const {
  return action_kind == action_kind_t::decision;
}
bool action_t::is_unit_prop() const {
  return action_kind == action_kind_t::unit_prop;
}
bool action_t::is_conflict() const {
  return action_kind == action_kind_t::halt_conflict;
}
bool action_t::has_clause() const {
  return action_kind == action_kind_t::halt_conflict ||
         action_kind == action_kind_t::unit_prop;
}
clause_id action_t::get_clause() const {
  if (action_kind == action_kind_t::halt_conflict) {
    return conflict_clause_id;
  } else if (action_kind == action_kind_t::unit_prop) {
    return unit_prop.reason;
  }
  assert(0);
}

action_t make_decision(literal_t l) {
  action_t a;
  a.action_kind = action_t::action_kind_t::decision;
  a.decision_literal = l;
  return a;
}
action_t make_unit_prop(literal_t l, clause_id cid) {
  action_t a;
  a.action_kind = action_t::action_kind_t::unit_prop;
  a.unit_prop.propped_literal = l;
  a.unit_prop.reason = cid;
  return a;
}
action_t make_conflict(clause_id cid) {
  action_t a;
  a.action_kind = action_t::action_kind_t::halt_conflict;
  a.conflict_clause_id = cid;
  return a;
}

std::ostream& operator<<(std::ostream& o, const action_t::action_kind_t a) {
  switch (a) {
    case action_t::action_kind_t::decision:
      return o << "decision";
    case action_t::action_kind_t::unit_prop:
      return o << "unit_prop";
    case action_t::action_kind_t::backtrack:
      return o << "backtrack";
    case action_t::action_kind_t::halt_conflict:
      return o << "halt_conflict";
    case action_t::action_kind_t::halt_unsat:
      return o << "halt_unsat";
    case action_t::action_kind_t::halt_sat:
      return o << "halt_sat";
  }
  return o;
}
std::ostream& operator<<(std::ostream& o, const action_t a) {
  o << "{ " << a.action_kind;
  switch (a.action_kind) {
    case action_t::action_kind_t::decision:
      return o << ", " << a.decision_literal << " }";
    case action_t::action_kind_t::unit_prop:
      return o << ", " << a.unit_prop.propped_literal << ", "
               << a.unit_prop.reason << " }";
    case action_t::action_kind_t::halt_conflict:
      return o << ", " << a.conflict_clause_id << " }";
    default:
      return o << " }";
  }
}
