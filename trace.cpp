#include "trace.h"
#include "debug.h"
#include <algorithm>

backtrack_mode_t backtrack_mode = backtrack_mode_t::nonchron;
learn_mode_t learn_mode = learn_mode_t::explicit_resolution;
unit_prop_mode_t unit_prop_mode = unit_prop_mode_t::queue;

void trace_t::reset() {
  actions.clear();
  variable_state.clear();
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

  variable_state.resize(max_var+1);
  std::fill(std::begin(variable_state), std::end(variable_state), unassigned);

}

trace_t::trace_t(cnf_t& cnf): cnf(cnf) {
  reset();
}


bool trace_t::halt_state(const action_t action) {
  return action.action_kind == action_t::action_kind_t::halt_conflict ||
    action.action_kind == action_t::action_kind_t::halt_unsat ||
    action.action_kind == action_t::action_kind_t::halt_sat;
}

bool trace_t::halted() const {
  //std::cout << "Debug: current trace is: " << *this << std::endl;
  return !actions.empty() && halt_state(*actions.rbegin());
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

bool trace_t::literal_true(const literal_t l) const {
  variable_t v = l > 0 ? l : -l;
  variable_state_t is_true = l > 0 ? unassigned_to_true : unassigned_to_false;
  return variable_state[v] == is_true;
}
bool trace_t::literal_false(const literal_t l) const {
  variable_t v = l > 0 ? l : -l;
  variable_state_t is_false = l < 0 ? unassigned_to_true : unassigned_to_false;
  return variable_state[v] == is_false;
}

bool trace_t::literal_unassigned(const literal_t l) const {
  variable_t v = l > 0 ? l : -l;
  return variable_state[v] == unassigned;
}
void trace_t::unassign_literal(const literal_t l)  {
  variable_t v = l > 0 ? l : -l;
  variable_state[v] = unassigned;
}

bool trace_t::clause_sat(const clause_t& clause) const {
  return std::any_of(std::begin(clause), std::end(clause), [this](auto& c) {return this->literal_true(c);});
}
bool trace_t::clause_sat(clause_id cid) const {
  return clause_sat(cnf[cid]);
}

// this means, stricly, that all literals are false (not just that none are true)
bool trace_t::clause_unsat(const clause_t& clause) const {
  return std::all_of(std::begin(clause), std::end(clause), [this](auto& l) { return this->literal_false(l);});
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

size_t trace_t::count_unassigned_literals(const clause_t& clause) const {
  return std::count_if(std::begin(clause), std::end(clause), [this](auto& c) {return this->literal_unassigned(c);});
}
size_t trace_t::count_unassigned_literals(clause_id cid) const {
  return count_unassigned_literals(cnf[cid]);
}

// To help unit-prop, for now.
literal_t trace_t::find_unassigned_literal(const clause_t& clause) const {
  auto it = std::find_if(std::begin(clause), std::end(clause), [this](auto& c) {
                                                                 return this->literal_unassigned(c);});
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

trace_t::variable_state_t trace_t::satisfy_literal(literal_t l) const {
  if (l > 0) {
    return unassigned_to_true;
  }
  return unassigned_to_false;
}


// Note we *don't* push to the action queue, yet.
void trace_t::push_unit_queue(literal_t l, clause_id cid) {
  action_t a;
  a.action_kind = action_t::action_kind_t::unit_prop;
  a.unit_prop.propped_literal = l;
  a.unit_prop.reason = cid;
  units.push_back(a);
}
void trace_t::clear_unit_queue() {
  units.clear();
}
void trace_t::clean_unit_queue() {
  auto new_end = std::remove_if(std::begin(units), std::end(units), [this](const action_t& a) {
                                                                      SAT_ASSERT(count_unassigned_literals(a.unit_prop.reason) > 0);
                                                                      return count_unassigned_literals(a.unit_prop.reason) != 1;
                                                                    });
  units.erase(new_end, std::end(units));
}

void trace_t::push_conflict(clause_id cid) {
  action_t action;
  action.action_kind = action_t::action_kind_t::halt_conflict;
  action.conflict_clause_id = cid;
  actions.push_back(action);
}

void trace_t::push_sat() {
  action_t action;
  action.action_kind = action_t::action_kind_t::halt_sat;
  actions.push_back(action);
}
void trace_t::push_unsat() {
  //std::cout << "Pushing unsat!" << std::endl;
  action_t action;
  action.action_kind = action_t::action_kind_t::halt_unsat;
  actions.push_back(action);
}

// We've just applied l, and now we do various machineries to queue up
// the newly-implied units.
// This may also change our state by finding a conflict
void trace_t::register_false_literal(literal_t l) {
  // We should never falsify a literal for which we have a unit clause asserting that it should be true.
  SAT_ASSERT(std::find_if(std::begin(cnf), std::end(cnf), [l](const clause_t& c) { return c.size() == 1 && contains(c, -l); }) == std::end(cnf));
  // Here we look for conflicts and new units!
  if (unit_prop_mode == unit_prop_mode_t::queue) {
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

// This is the one entry point where we actually change a literal's value
// (presumably having just added an appropriate action to the trail). So
// we look at unit-prop modes here.
void trace_t::apply_literal(literal_t l) {
  variable_t v = l > 0 ? l : -l;
  variable_state[v] = satisfy_literal(l);
}

// This applies the action to make l true in our trail.
void trace_t::apply_decision(literal_t l) {
  //std::cout << "Deciding: " << l << std::endl;
  action_t action;
  action.action_kind = action_t::action_kind_t::decision;
  action.decision_literal = l;
  actions.push_back(action);

  apply_literal(l);
}

// This applies the action to unit prop l in our trail.
void trace_t::apply_unit(literal_t l, clause_id cid) {
  action_t action;
  action.action_kind = action_t::action_kind_t::unit_prop;
  action.unit_prop.propped_literal = l;
  action.unit_prop.reason = cid;
  actions.push_back(action);
  //std::cout << "Unit-propping : " << action << std::endl;

  apply_literal(l);
}


// Find a new, unassigned literal, and assign it.
literal_t trace_t::decide_literal() {
  SAT_ASSERT(!cnf_unsat());
  if (unit_clause_exists()) {
    std::cout << "Failing with trace: " << std::endl << *this << std::endl;
    print_cnf(cnf);
  }
  SAT_ASSERT(!unit_clause_exists());

  auto it = std::find_if(std::begin(cnf), std::end(cnf), [this](const clause_t& clause) {
                                                           return !clause_sat(clause) && this->count_unassigned_literals(clause) > 0;
                                                         });
  //SAT_ASSERT(it != std::end(cnf));
  if (it == std::end(cnf)) {
    SAT_ASSERT(cnf_sat());
    push_sat();
    return 0;
  }
  literal_t l = find_unassigned_literal(*it);
  return l;
}

// Get an implication that comes from our trail.
std::pair<literal_t, clause_id> trace_t::prop_unit() {
  // Are we in a mode where we keep a queue of units?
  if (unit_prop_mode == unit_prop_mode_t::queue) {
    if (units.empty()) {
      return std::make_pair(0,0);
    }

    // Transfer the action from the queue into our trail.
    action_t a = units.front(); units.pop_front();
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

void trace_t::backtrack(const clause_t& c) {
  if (c.size() == 0) { push_unsat(); return; }

  if (backtrack_mode == backtrack_mode_t::simplest) {
    SAT_ASSERT(std::prev(actions.end())->action_kind == action_t::action_kind_t::halt_conflict);
    actions.pop_back();
    // it can't be completely dumb: we have to leave the prefix of our automatic unit-props

    auto to_erase = std::find_if(std::begin(actions), std::end(actions), [](action_t& a) { return a.is_decision(); });
    std::for_each(to_erase, std::end(actions), [this](action_t& a) { unassign_literal(a.get_literal()); });
    actions.erase(to_erase, std::end(actions));
    clear_unit_queue();
  }
  else if (backtrack_mode == backtrack_mode_t::nonchron) {
    SAT_ASSERT(std::prev(actions.end())->action_kind == action_t::action_kind_t::halt_conflict);
    actions.pop_back();
    SAT_ASSERT(clause_unsat(c));

    // Find the most recent decision. If we pop "just" this, we'll have naive backtracking (that somehow still doesn't
    // work, in that there are unsat clauses in areas where there shouldn't be).

    // Find the latest decision, this naively makes us an implication
    auto bit = std::find_if(std::rbegin(actions), std::rend(actions), [](const action_t& a) { return a.is_decision(); });
    // Look backwards for the next trail entry that negates something in c. /That's/ the actual thing we can't pop.
    auto needed_for_implication = std::find_if(bit+1, std::rend(actions), [&](const action_t& a) { return contains(c, -a.get_literal()); });
    // Look forwards again, starting at the trail entry after that necessary one.
    auto del_it = needed_for_implication.base()-1;
    SAT_ASSERT(&(*del_it) == &(*needed_for_implication));
    // We look for the first decision we find. We basically scanned backwards, finding the first thing we /couldn't/ pop without
    // compromising the unit-ness of our new clause c. Then we scan forward -- the next decision we find is the "chronologically
    // earliest" one we can pop. So we do that.
    auto to_erase = std::find_if(del_it+1, std::end(actions), [](const action_t& a) { return a.is_decision(); });
    SAT_ASSERT(to_erase != std::end(actions));

    // diagnostistic: how many levels can be pop back?
    //int popcount = std::count_if(to_erase, std::end(actions), [](const action_t& a) { return a.is_decision(); });
    //std::cout << "Popcount: " << popcount << std::endl;

    // Actually do the erasure:
    std::for_each(to_erase, std::end(actions), [this](action_t& a) { unassign_literal(a.get_literal()); });
    actions.erase(to_erase, std::end(actions));

    SAT_ASSERT(count_unassigned_literals(c) == 1);
    literal_t l = find_unassigned_literal(c);
    //SAT_ASSERT(l == new_implied); // derived from when we learned the clause, this passed all our tests.
    //SAT_ASSERT(cnf[cnf.size()-1] == c);

    // Reset whatever could be units (TODO: only reset what we know we've invalidated?)
    clean_unit_queue();
    //std::cout << "Pushing " << l << " " << c << std::endl;
    //push_unit_queue(l, cnf.size()-1);
    //std::cout << "Trail is now: " << *this << std::endl;
  }
}

void trace_t::add_clause(const clause_t& c) {
  size_t id = cnf.size();
  cnf.push_back(c);

  for (literal_t l : c) {
    literal_to_clause[l].push_back(id);
  }
}

// Debugging purposes
bool trace_t::verify_resolution_expected(const clause_t& c) {
  if (!c.empty()) {
    // correctness checks:
    // reset our iterator
    int counter = 0;
    literal_t new_implied = 0;
    auto it = std::next(actions.rbegin());

    for (; it->action_kind != action_t::action_kind_t::decision; it++) {
      SAT_ASSERT(it->action_kind == action_t::action_kind_t::unit_prop);
      if (contains(c, -it->unit_prop.propped_literal)) {
        counter++;
        new_implied = it->get_literal();
      }
    }
    SAT_ASSERT(it->action_kind == action_t::action_kind_t::decision);
    if (contains(c, -it->decision_literal)) {
      counter++;
      new_implied = -it->get_literal();
    }

    //std::cout << "Learned clause " << c << std::endl;
    //std::cout << "Counter = " << counter << std::endl;
    SAT_ASSERT(counter == 1);
  }
  return true;
}


std::ostream& operator<<(std::ostream& o, const trace_t::variable_state_t s) {
  switch(s) {
    case trace_t::variable_state_t::unassigned: return o << "unassigned";
    case trace_t::variable_state_t::unassigned_to_true: return o << "true";
    case trace_t::variable_state_t::unassigned_to_false: return o << "false";
  };
  return o;
}
std::ostream& operator<<(std::ostream& o, const std::vector<trace_t::variable_state_t>& s) {
  o << "{ ";
  for (size_t i = 0; i < s.size(); i++) {
    auto v = s[i];
    if (v != trace_t::variable_state_t::unassigned) {
      o << i << "=" << v << " ";
    }
  }
  return o << "}";
}


std::ostream& operator<<(std::ostream& o, const std::vector<action_t> v) {
  for (auto& a : v) {
    o << a << std::endl;
  }
  return o;
}



std::ostream& operator<<(std::ostream& o, const trace_t t) {
  return o << t.variable_state << std::endl << t.actions;
}
