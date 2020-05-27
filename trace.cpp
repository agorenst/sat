#include "trace.h"
#include "debug.h"
#include <algorithm>

#include "clause_learning.h"
#include "backtrack.h"
#include "vsids.h"

// Defaults: fast and working...
backtrack_mode_t backtrack_mode = backtrack_mode_t::nonchron;
learn_mode_t learn_mode = learn_mode_t::explicit_resolution;
//unit_prop_mode_t unit_prop_mode = unit_prop_mode_t::queue;
unit_prop_mode_t unit_prop_mode = unit_prop_mode_t::watched;
variable_choice_mode_t variable_choice_mode = variable_choice_mode_t::nextclause;

void trace_t::reset() {
  actions.clear();
  literal_to_clause.clear();
  units.clear();

  variable_t max_var = 0;
  for (size_t i = 0; i < cnf.size(); i++) {
    const auto& clause = cnf[i];
    SAT_ASSERT(clause.size() > 1); // for watched literals, TODO, make this more robust.
    for (auto& literal : clause) {
      max_var = std::max(max_var, std::abs(literal));
      literal_to_clause[literal].push_back(i);
    }
  }

  actions.construct(max_var+1);

  for (size_t i = 0; i < cnf.size(); i++) {
    watch.watch_clause(i);
  }

  SAT_ASSERT(watch.validate_state());

}

trace_t::trace_t(cnf_t& cnf): cnf(cnf), watch(*this),
                              // Actions is invalid at this point, but we don't read it
                              // until after it's constructed. We're really just taking a pointer
                              // to it.
                              vsids(cnf, actions)
{
  reset();
}


bool trace_t::halt_state(const action_t action) {
  return action.action_kind == action_t::action_kind_t::halt_conflict ||
    action.action_kind == action_t::action_kind_t::halt_unsat ||
    action.action_kind == action_t::action_kind_t::halt_sat;
}

bool trace_t::halted() const {
  //std::cout << "Debug: current trace is: " << *this << std::endl;
  bool result = !actions.empty() && halt_state(*actions.crbegin());
  //std::cout << "[DBG](halted): " << result << " with " << *actions.crbegin() << std::endl;
  return result;
}
bool trace_t::final_state() {
  //std::cout << "Testing final state: " << *this << std::endl;
  if (actions.empty()) {
    return false;
  }
  action_t action = *actions.rbegin();

  return
    action.action_kind == action_t::action_kind_t::halt_unsat ||
    action.action_kind == action_t::action_kind_t::halt_sat;
}

bool trace_t::clause_sat(const clause_t& clause) const {
  return std::any_of(std::begin(clause), std::end(clause), [this](auto& c) {return this->actions.literal_true(c);});
}
bool trace_t::clause_sat(clause_id cid) const {
  return clause_sat(cnf[cid]);
}

// this means, stricly, that all literals are false (not just that none are true)
bool trace_t::clause_unsat(const clause_t& clause) const {
  return std::all_of(std::begin(clause), std::end(clause), [this](auto& l) { return this->actions.literal_false(l);});
}
bool trace_t::clause_unsat(clause_id cid) const {
  return clause_unsat(cnf[cid]);
}

auto trace_t::unsat_clause() const {
  return std::find_if(std::begin(cnf), std::end(cnf), [this](auto& c) {return this->clause_unsat(c);});
}

bool trace_t::cnf_unsat() const {
  return unsat_clause() != std::end(cnf);
}

size_t trace_t::count_true_literals(const clause_t& clause) const {
  return std::count_if(std::begin(clause), std::end(clause), [this](auto& c) {return this->actions.literal_true(c);});
}
size_t trace_t::count_false_literals(const clause_t& clause) const {
  return std::count_if(std::begin(clause), std::end(clause), [this](auto& c) {return this->actions.literal_false(c);});
}
size_t trace_t::count_unassigned_literals(const clause_t& clause) const {
  return std::count_if(std::begin(clause), std::end(clause), [this](auto& c) {return this->actions.literal_unassigned(c);});
}
size_t trace_t::count_unassigned_literals(clause_id cid) const {
  return count_unassigned_literals(cnf[cid]);
}

// To help unit-prop, for now.
literal_t trace_t::find_unassigned_literal(const clause_t& clause) const {
  auto it = std::find_if(std::begin(clause), std::end(clause), [this](auto& c) {
                                                                 return this->actions.literal_unassigned(c);});
  SAT_ASSERT(it != std::end(clause));
  return *it;
}
literal_t trace_t::find_unassigned_literal(clause_id cid) const {
  return find_unassigned_literal(cnf[cid]);
}

// Purely for debugging.
bool trace_t::unit_clause_exists() const {
  auto tt = std::find_if(std::begin(cnf), std::end(cnf), [this](const clause_t& clause) {
                                                           return !clause_sat(clause) && this->count_unassigned_literals(clause) == 1;
                                                         });
  return tt != std::end(cnf);
}

// Really, "no conflict found". That's why we don't have the counterpart.
bool trace_t::cnf_sat() const {
  return std::all_of(std::begin(cnf), std::end(cnf), [this](auto& c){return this->clause_sat(c);});
}

// Note we *don't* push to the action queue, yet.
void trace_t::push_unit_queue(literal_t l, clause_id cid) {
  action_t a;
  a.action_kind = action_t::action_kind_t::unit_prop;
  a.unit_prop.propped_literal = l;
  a.unit_prop.reason = cid;
  units.push(a);
}
void trace_t::clear_unit_queue() {
  units.clear();
}
//void trace_t::clean_unit_queue() {
//  auto new_end = std::remove_if(std::begin(units), std::end(units), [this](const action_t& a) {
//                                                                      SAT_ASSERT(count_unassigned_literals(a.unit_prop.reason) > 0);
//                                                                      return count_unassigned_literals(a.unit_prop.reason) != 1;
//                                                                    });
//  units.erase(new_end, std::end(units));
//}

void trace_t::push_conflict(clause_id cid) {
  action_t action;
  action.action_kind = action_t::action_kind_t::halt_conflict;
  action.conflict_clause_id = cid;
  actions.append(action);
}

void trace_t::push_sat() {
  action_t action;
  action.action_kind = action_t::action_kind_t::halt_sat;
  actions.append(action);
}
void trace_t::push_unsat() {
  //std::cout << "Pushing unsat!" << std::endl;
  action_t action;
  action.action_kind = action_t::action_kind_t::halt_unsat;
  actions.append(action);
}

// We've just applied l, and now we do various machineries to queue up
// the newly-implied units.
// This may also change our state by finding a conflict
void trace_t::register_false_literal(literal_t l) {
  // We should never falsify a literal for which we have a unit clause asserting that it should be true.
  SAT_ASSERT(std::find_if(std::begin(cnf), std::end(cnf), [l](const clause_t& c) { return c.size() == 1 && contains(c, -l); }) == std::end(cnf));
  // Here we look for conflicts and new units!
  if (unit_prop_mode == unit_prop_mode_t::watched) {
    watch.literal_falsed(l);
    SAT_ASSERT(halted() || watch.validate_state());
  }
  else if (unit_prop_mode == unit_prop_mode_t::queue) {


    const auto& clause_ids = literal_to_clause[-l];
    for (auto clause_id : clause_ids) {
      const auto& c = cnf[clause_id];
      if (clause_sat(c)) {
        continue;
      }
      //all_sat = false;
      if (contains(c, -l)) {
        if (clause_unsat(c)) {
          push_conflict(clause_id);
          return;
        }
        if (count_unassigned_literals(c) == 1) {
          literal_t p = find_unassigned_literal(c);
          push_unit_queue(p, clause_id);
        }
      }
    }
  }
  // All we look for is conflicts, here.
  else if (unit_prop_mode == unit_prop_mode_t::simplest) {
    // we don't put in the queue, but we still look for conflicts.
    for (size_t cid = 0; cid < cnf.size(); cid++) {
      const clause_t& c = cnf[cid];
      if (contains(c, -l)) {
        if (clause_unsat(c)) {
          //std::cout << "Found conflict: " << c << std::endl;
          push_conflict(cid);
          return;
        }
      }
      else {
        SAT_ASSERT(!clause_unsat(c));
      }
    }
  }
}

// This applies the action to make l true in our trail.
void trace_t::apply_decision(literal_t l) {
  //std::cout << "Deciding: " << l << std::endl;
  action_t action;
  action.action_kind = action_t::action_kind_t::decision;
  action.decision_literal = l;
  actions.append(action);
}

// This applies the action to unit prop l in our trail.
void trace_t::apply_unit(literal_t l, clause_id cid) {
  action_t action;
  action.action_kind = action_t::action_kind_t::unit_prop;
  action.unit_prop.propped_literal = l;
  action.unit_prop.reason = cid;
  actions.append(action);
}


// Find a new, unassigned literal, and assign it.
literal_t trace_t::decide_literal() {
#ifdef SAT_DEBUG_MODE
  SAT_ASSERT(!cnf_unsat());
  if (unit_clause_exists()) {
    std::cout << "Failing with trace: " << std::endl << *this << std::endl << cnf << std::endl;
  }
  SAT_ASSERT(!unit_clause_exists());
#endif

  literal_t l = 0;
  if (variable_choice_mode == variable_choice_mode_t::nextliteral) {
    assert(0);
  }
  else if (false) {
    auto it = std::find_if(std::begin(cnf), std::end(cnf), [this](const clause_t& clause) {
                                                             return !clause_sat(clause) && this->count_unassigned_literals(clause) > 0;
                                                           });
    if (it != std::end(cnf)) {
      l = find_unassigned_literal(*it);
    }
  } else {
    l = vsids.choose();
  }
  //SAT_ASSERT(it != std::end(cnf));
  if (l == 0) {
    SAT_ASSERT(cnf_sat());
    push_sat();
  }
  return l;
}

// Get an implication that comes from our trail.
std::pair<literal_t, clause_id> trace_t::prop_unit() {
  // Are we in a mode where we keep a queue of units?
  if (unit_prop_mode == unit_prop_mode_t::queue ||
      unit_prop_mode == unit_prop_mode_t::watched) {
    if (units.empty()) {
      return std::make_pair(0,0);
    }

    // Transfer the action from the queue into our trail.
    action_t a = units.pop();
    SAT_ASSERT(a.action_kind == action_t::action_kind_t::unit_prop);
    return std::make_pair(a.get_literal(), a.get_clause());
  }
  // Otherwise, just look manually for a unit
  else if (unit_prop_mode == unit_prop_mode_t::simplest) {
    for (clause_id i = 0; i < cnf.size(); i++) {
      auto& c = cnf[i];
      SAT_ASSERT(!clause_unsat(c));
      if (clause_sat(c)) {
        continue;
      }
      if (1 == count_unassigned_literals(c)) {
        literal_t l = find_unassigned_literal(c);
        return std::make_pair(l, i);
      }
    }
    return std::make_pair(0,0);
  }
  return std::make_pair(0,0);
}

cnf_t::clause_k trace_t::add_clause(const clause_t& c) {
  size_t id = cnf.size();
  auto ret_val = cnf.push_back(c);

  for (literal_t l : c) {
    literal_to_clause[l].push_back(id);
  }
  if (c.size() > 1) watch.watch_clause(id);
  
  if (unit_prop_mode == unit_prop_mode_t::watched) SAT_ASSERT(watch.validate_state());
  return ret_val;
}


void trace_t::print_actions(std::ostream& o) const {
  for (const auto& a : actions) {
    o << a << std::endl;
  }
}


std::ostream& operator<<(std::ostream& o, const trace_t& t) {
  t.print_actions(o);
  return o;
}

literal_t trace_t::find_last_falsified(clause_id cid) {
  const clause_t& c = cnf[cid];
  for (auto it = std::rbegin(actions); it != std::rend(actions); it++) {
    if (it->has_literal()) {
      literal_t l = it->get_literal();
      if (contains(c, -l)) {
        return -l;
      }
    }
  }
  return 0;
}
