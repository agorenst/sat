#include <cstdio>
#include <iostream>
#include <cstdint>
#include <vector>
#include <algorithm>
#include <cassert>
#include <map>
#include <queue>

#include "cnf.h"

// TODO: Change clause_id to an iterator?
// TODO: Exercise 257 to get shorter learned clauses

// The understanding of this project is that it will evolve
// frequently. We'll first start with the basic data structures,
// and then refine things as they progress.


variable_t max_variable = 0;

enum class backtrack_mode_t {
                             simplest,
                             trust_learning,
};
backtrack_mode_t backtrack_mode = backtrack_mode_t::trust_learning;
enum class learn_mode_t {
                         simplest,
                         explicit_resolution
};
learn_mode_t learn_mode = learn_mode_t::explicit_resolution;
enum class unit_prop_mode_t {
                             simplest,
                             queue
};
unit_prop_mode_t unit_prop_mode = unit_prop_mode_t::queue;
bool watched_literals_on = false; // this builds off the "queue" model.

// The perspective I want to take is not one of deriving an assignment,
// but a trace exploring the recursive, DFS space of assignments.
// We don't forget anything -- I even want to record backtracking.
// So let's see what this looks like.
struct trace_t;
std::ostream& operator<<(std::ostream& o, const trace_t t);
struct trace_t {

  enum variable_state_t {
                       unassigned,
                       unassigned_to_true,
                       unassigned_to_false
  };

  cnf_t& cnf;
  std::vector<action_t> actions;
  std::vector<variable_state_t> variable_state;

  // this is hugely expensive data structure. We'll see.
  std::map<literal_t, std::vector<clause_id>> literal_to_clause;

  // store the unit-props we're still getting through.
  std::queue<action_t> units;

  // the "vector" is really a map of clause_ids to their watchers.
  std::vector<watcher_t> watched_literals;
  std::map<literal_t, std::vector<clause_id>> literals_to_watcher;

  void reset() {
    actions.clear();
    variable_state.clear();
    literal_to_clause.clear();
    while (!units.empty()) { units.pop(); }
    watched_literals.clear();
    literals_to_watcher.clear();

    variable_t max_var = 0;
    for (size_t i = 0; i < cnf.size(); i++) {
      const auto& clause = cnf[i];
      assert(clause.size() > 1); // for watched literals, TODO, make this more robust.
      for (auto& literal : clause) {
        max_var = std::max(max_var, std::abs(literal));
        literal_to_clause[literal].push_back(i);
      }
    }

    variable_state.resize(max_var+1);
    std::fill(std::begin(variable_state), std::end(variable_state), unassigned);

    for (size_t i = 0; i < cnf.size(); i++) {
      new_watch(i);
    }

    check_watch_correctness();
  }

  trace_t(cnf_t& cnf): cnf(cnf) {
    reset();
  }

  // Make sure our two maps are mapping to each other correctly.
  // Not yet checked agains the consistency of the trail, something to add.
  void check_watch_correctness() {
    return;
    for (size_t cid = 0; cid < cnf.size(); cid++) {
      watcher_t w = watched_literals[cid];
      const clause_t& c = cnf[cid];
      assert(w.idx1 < c.size());
      assert(w.idx2 < c.size());
      assert(w.idx1 != w.idx2);

      assert(contains(literals_to_watcher[c[w.idx1]], cid));
      assert(contains(literals_to_watcher[c[w.idx2]], cid));
    }

    for (auto&& [literal, clause_list] : literals_to_watcher) {
      for (const auto& cid : clause_list) {
        const clause_t& c = cnf[cid];
        assert(contains(c, literal));

        watcher_t w = watched_literals[cid];
        assert(c[w.idx1] == literal || c[w.idx2] == literal);
      }
    }
  }

  void new_watch(clause_id cid) {
    assert(watched_literals.size() == cid);
    const clause_t& c = cnf[cid];
    assert(c.size() > 1);

    size_t i1 = c.size();
    size_t i2 = c.size();
    // try to assign the 2 watched literals.
    for (size_t i = 0; i < c.size(); i++) {
      // we can do with unassigned or with true.
      if (!literal_false(c[i])) {
        literals_to_watcher[c[i]].push_back(cid);
        i1 = i;
        break;
      }
    }
    for (size_t i = i1+1; i < c.size(); i++) {
      if (!literal_false(c[i])) {
        i2 = i;
        literals_to_watcher[c[i]].push_back(cid);
        break;
      }
    }

    // If we couldn't find anything at all, we already have a conflict.
    if (i1 == c.size()) {
      if (watched_literals_on) {
        action_t a;
        a.action_kind = action_t::action_kind_t::halt_conflict;
        actions.push_back(a);
        //std::cout << "Added conflict from initial watch" << std::endl;
      }
      // still want to keep the watchers and everything in a consistent state.
      i1 = 0;
      literals_to_watcher[c[i1]].push_back(cid);
    }
    if (i2 == c.size()) {
      if (watched_literals_on) {
        action_t a;
        a.action_kind = action_t::action_kind_t::unit_prop;
        a.unit_prop.propped_literal = c[i1];
        a.unit_prop.reason = cid;
        units.push(a); // units, we haven't processed this yet.
        //std::cout << "Added unit from initial watch" << std::endl;
      }

      // still want to keep the watchers and everything in a consistent state.
      i2 = i1 == 0 ? 1 : 0;
      literals_to_watcher[c[i2]].push_back(cid);
    }

    watched_literals.push_back({i1, i2});
  }

  // This is the real magic!
  void re_watch(clause_id cid, literal_t l) {
    assert(watched_literals.size() > cid);
    assert(literal_false(l));
    watcher_t w = watched_literals[cid];
    const clause_t& c = cnf[cid];
    assert(c[w.idx1] == l || c[w.idx2] == l);

    // We want to find something new to watch cid, not l.
    // So we find the thing we expect to remove.
    auto& literal_watch_list = literals_to_watcher[l];
    auto to_remove = std::find(std::begin(literal_watch_list), std::end(literal_watch_list), cid);
    assert(to_remove != std::end(literal_watch_list));

    // First, see if we found a true literal to move to. That's "best" (intuitively, at least...)
    size_t i = 0;
    bool found = false;
    for (; i < c.size(); i++) {
      if (i == w.idx1) continue;
      if (i == w.idx2) continue;
      literal_t l = c[i];
      if (literal_true(l)) { found = true; break; }
    }

    if (!found) {
      i = 0;
      for (; i < c.size(); i++) {
        if (i == w.idx1) continue;
        if (i == w.idx2) continue;
        literal_t l = c[i];
        if (!literal_false(l)) { found = true; break; }
      }
    }

    // great, we found a new literal for our
    // replacement literal.
    if (i < c.size()) {
      assert(found);
      // swap out the correct index.
      if (c[w.idx1] == l) {
        w.idx1 = i;
      }
      else {
        assert(c[w.idx2] == l);
        w.idx2 = i;
      }
      // update the watch list of that new index
      literals_to_watcher[i].push_back(cid);

      // remove the old index.
      std::swap(*to_remove, *(std::end(literal_watch_list)-1));
      literal_watch_list.pop_back();
    }
    // only take action if we've turned on this feature.
    else if (watched_literals_on) {
      // if our counterpart is unassigned, we're a unit
      // if our counterpart is true, we're done (check this earlier?)
      // if our counterpart is false, we're a conflict
      literal_t other_literal = l == c[w.idx1] ? w.idx2 : w.idx1; 
      assert(other_literal != l);

      if (literal_true(other_literal)) {
        return;
      }
      if (literal_unassigned(other_literal)) {
        action_t a;
        a.action_kind = action_t::action_kind_t::unit_prop;
        a.unit_prop.reason = cid;
        a.unit_prop.propped_literal = other_literal;
        units.push(a); // UNITS, not our action: we haven't processed this yet.
      }
      if (literal_false(other_literal)) {
        action_t a;
        a.action_kind = action_t::action_kind_t::halt_conflict;
        a.conflict_clause_id = cid;
        actions.push_back(a); // we push this onto action, to record this conflict.
      }
    }

    check_watch_correctness();
  }

  static bool halt_state(const action_t action) {
    return action.action_kind == action_t::action_kind_t::halt_conflict ||
      action.action_kind == action_t::action_kind_t::halt_unsat ||
      action.action_kind == action_t::action_kind_t::halt_sat;
  }
  bool halted() const {
    //std::cout << "Debug: current trace is: " << *this << std::endl;
    return !actions.empty() && halt_state(*actions.rbegin());
  }
  bool final_state() {
    if (actions.empty()) {
      return false;
    }
    action_t action = *actions.rbegin();

    // TODO: there is a better place for this check/computation, I'm sure.
    if (action.action_kind == action_t::action_kind_t::halt_conflict) {
      bool has_decision = false;
      for (auto& a : actions) {
        if (a.action_kind == action_t::action_kind_t::decision) {
          has_decision = true;
          break;
        }
      }
      //auto it = std::find(std::begin(actions), std::end(actions), [](action_t a) { return a.action_kind == action_t::action_kind_t::decision; });
      //if (it == std::end(actions)) {
      if (!has_decision) {
        action_t a;
        a.action_kind = action_t::action_kind_t::halt_unsat;
        actions.push_back(a);
        action = a;
      }
    }
    return
      action.action_kind == action_t::action_kind_t::halt_unsat ||
      action.action_kind == action_t::action_kind_t::halt_sat;
  }

  bool literal_true(const literal_t l) const {
    variable_t v = l > 0 ? l : -l;
    variable_state_t is_true = l > 0 ? unassigned_to_true : unassigned_to_false;
    return variable_state[v] == is_true;
  }
  bool literal_false(const literal_t l) const {
    variable_t v = l > 0 ? l : -l;
    variable_state_t is_false = l < 0 ? unassigned_to_true : unassigned_to_false;
    return variable_state[v] == is_false;
  }

  bool literal_unassigned(const literal_t l) const {
    variable_t v = l > 0 ? l : -l;
    return variable_state[v] == unassigned;
  }
  void unassign_literal(const literal_t l)  {
    variable_t v = l > 0 ? l : -l;
    variable_state[v] = unassigned;
  }

  bool clause_sat(const clause_t& clause) const {
    return std::any_of(std::begin(clause), std::end(clause), [this](auto& c) {return this->literal_true(c);});
  }
  bool clause_sat(clause_id cid) const {
    return clause_sat(cnf[cid]);
  }

  // this means, stricly, that all literals are false (not just that none are true)
  bool clause_unsat(const clause_t& clause) const {
    return std::all_of(std::begin(clause), std::end(clause), [this](auto& l) { return this->literal_false(l);});
  }
  bool clause_unsat(clause_id cid) const {
    return clause_unsat(cnf[cid]);
  }

  auto unsat_clause() const {
    return std::find_if(std::begin(cnf), std::end(cnf), [this](auto& c) {return this->clause_unsat(c);});
  }

  bool cnf_unsat() const {
    return unsat_clause() != std::end(cnf);
  }

  size_t count_unassigned_literals(const clause_t& clause) const {
    return std::count_if(std::begin(clause), std::end(clause), [this](auto& c) {return this->literal_unassigned(c);});
  }
  size_t count_unassigned_literals(clause_id cid) const {
    return count_unassigned_literals(cnf[cid]);
  }

  // To help unit-prop, for now.
  literal_t find_unassigned_literal(const clause_t& clause) const {
    auto it = std::find_if(std::begin(clause), std::end(clause), [this](auto& c) {
                                                                return this->literal_unassigned(c);});
    assert(it != std::end(clause));
    return *it;
  }
  literal_t find_unassigned_literal(clause_id cid) const {
    return find_unassigned_literal(cnf[cid]);
  }

  // Really, "no conflict found". That's why we don't have the counterpart.
  bool cnf_sat() const {
    return std::all_of(std::begin(cnf), std::end(cnf), [this](auto& c){return this->clause_sat(c);});
  }

  variable_state_t satisfy_literal(literal_t l) const {
    if (l > 0) {
      return unassigned_to_true;
    }
    return unassigned_to_false;
  }

  // Find a new, unassigned literal, and assign it.
  void decide_literal() {
    //if (unit_prop_mode == unit_prop_mode_t::queue) {
    //  seed_units_queue(); // this shouldn't add anything.
    //}
    assert(!units_to_prop());
    auto it = std::find_if(std::begin(cnf), std::end(cnf), [this](const clause_t& clause) {
                                                             return this->count_unassigned_literals(clause) > 0;
                                                           });
    //assert(it != std::end(cnf));
    if (it == std::end(cnf)) {
      assert(cnf_sat());
      action_t action;
      action.action_kind = action_t::action_kind_t::halt_sat;
      actions.push_back(action);
      return;
    }
    literal_t l = find_unassigned_literal(*it);
    variable_t v = l > 0 ? l : -l;
    variable_state[v] = satisfy_literal(l);
    action_t action;
    action.action_kind = action_t::action_kind_t::decision;
    action.decision_literal = l;
    actions.push_back(action);

    // Have we solved the equation?
    if (cnf_sat()) {
      action_t action;
      action.action_kind = action_t::action_kind_t::halt_sat;
      actions.push_back(action);
    }
    /* This shouldn't be necessary because we should find units.
    else if (auto it = unsat_clause(); it != std::end(cnf)) {
      action_t action;
      action.action_kind = action_t::action_kind_t::halt_conflict;
      action.conflict_clause_id = std::distance(std::begin(cnf), it);
      actions.push_back(action);
    }
    */
    else {
      if (unit_prop_mode == unit_prop_mode_t::queue) {
        // this is the "real" call, not just for debugging.
        seed_units_queue();
      }
    }
  }

  bool units_to_prop() const {
    if (unit_prop_mode == unit_prop_mode_t::queue) {
      return !units.empty();
    }
    else {
      for (auto& c : cnf) {
        assert(!clause_unsat(c));
        if (clause_sat(c)) {
          continue;
        }
        if (1 == count_unassigned_literals(c)) {
          return true;
        }
      }
      return false;
    }
  }

  void seed_units_queue() {
    if (unit_prop_mode != unit_prop_mode_t::queue) {
      return;
    }

    for (size_t i = 0; i < cnf.size(); i++) {
      const auto& c = cnf[i];
    //for (auto it = std::begin(cnf); it != std::end(cnf); it++) {
      //const auto& c = *it;
      //if (clause_unsat(c)) {
      //std::cout << "This clause: " << c << " is unsat in " << *this << std::endl;
      //}
      assert(!clause_unsat(c));
      if (clause_sat(c)) {
        continue;
      }
      if (1 == count_unassigned_literals(c)) {
        action_t a;
        a.action_kind = action_t::action_kind_t::unit_prop;
        a.unit_prop.propped_literal = find_unassigned_literal(c);
        a.unit_prop.reason = i; // std::distance(std::begin(cnf), it);
        //assert(cnf[a.unit_prop.reason] == *it);
        units.push(a);
      }
    }
  }

  bool prop_unit() {
    if (unit_prop_mode == unit_prop_mode_t::queue) {
      if (units.empty()) {
        return false;
      }
      action_t a = units.front(); units.pop();
      assert(a.action_kind == action_t::action_kind_t::unit_prop);
      literal_t l = a.get_literal();
      variable_state[std::abs(l)] = satisfy_literal(l);
      actions.push_back(a);

      // Only look at clauses we possibly changed.
      bool all_sat = true;
      const auto& clause_ids = literal_to_clause[-l];
      for (auto clause_id : clause_ids) {
      //for (auto it = std::begin(cnf); it != std::end(cnf); it++) {
        //for (size_t i = 0; i < cnf.size(); i++) {
        //const auto& c = cnf[i];
        //const auto& c = *it;
        const auto& c = cnf[clause_id];
        if (clause_sat(c)) {
          continue;
        }
        //all_sat = false;
        if (contains(c, -l)) {
          if (clause_unsat(c)) {
            action_t action;
            action.action_kind = action_t::action_kind_t::halt_conflict;
            //action.conflict_clause_id = i; //std::distance(std::begin(cnf), it); // get the id of the conflict clause!
            //action.conflict_clause_id = std::distance(std::begin(cnf), it); // get the id of the conflict clause!
            action.conflict_clause_id = clause_id;
            //assert(cnf[action.conflict_clause_id] == *it);
            actions.push_back(action);
            return false;
          }
          if (count_unassigned_literals(c) == 1) {
            literal_t p = find_unassigned_literal(c);
            action_t imp;
            imp.action_kind = action_t::action_kind_t::unit_prop;
            imp.unit_prop.propped_literal = p;
            //imp.unit_prop.reason = i; //std::distance(std::begin(cnf), it);
            //imp.unit_prop.reason = std::distance(std::begin(cnf), it);
            imp.unit_prop.reason = clause_id;
            //assert(cnf[imp.unit_prop.reason] == *it);
            units.push(imp);
          }
        }
      }
      /*
      if (all_sat) {
        action_t action;
        action.action_kind = action_t::action_kind_t::halt_sat;
        actions.push_back(action);
        return false;
      }
      */

      // we'll just assume that we propped *something*. We'll figure it out if the queue is really empty
      // on the next iteration.
      return true;
    }
    else if (unit_prop_mode == unit_prop_mode_t::simplest) {
      for (clause_id i = 0; i < cnf.size(); i++) {
        auto& c = cnf[i];
        assert(!clause_unsat(c));
        if (clause_sat(c)) {
          continue;
        }
        if (1 == count_unassigned_literals(c)) {
          literal_t l = find_unassigned_literal(c);
          variable_state[std::abs(l)] = satisfy_literal(l);
          action_t action;
          action.action_kind = action_t::action_kind_t::unit_prop;
          action.unit_prop.propped_literal = l;
          action.unit_prop.reason = i; // TODO: do this right
          actions.push_back(action);

          // Now, have we introduced a conflict?
          auto it = std::find_if(std::begin(cnf), std::end(cnf), [this](const clause_t& c) { return clause_unsat(c); });
          if (it != std::end(cnf)) {
            action_t action;
            action.action_kind = action_t::action_kind_t::halt_conflict;
            action.conflict_clause_id = std::distance(std::begin(cnf), it); // get the id of the conflict clause!
            actions.push_back(action);
            return false;
          }

          // Have we solved everything?
          if (cnf_sat()) {
            action_t action;
            action.action_kind = action_t::action_kind_t::halt_sat;
            actions.push_back(action);
            return false;
          }


          return true;
        }
      }
      return false;
    }
    return false;
  }

  // We have a special initial trace that looks for degenerate
  // We normally maintain certain invariants about the CNF (such as
  // we never decide a variable so long as there are unit clauses to prop),
  // but those invariants can be violated by the initial CNF itself.

  // This is the main entry point of the trace.
  // Basically, continue adding actions until it's halted.
  void process() {
    while (!halted()) {
      while (units_to_prop()) {
        // This does a lot of work.
        // It can find a conflict.
        if (!prop_unit()) {
          break;
        }
      }
      if (halted()) {
        return;
      }
      // This can see if we've found a totally-sat assignment.
      decide_literal();
    }
  }

  void backtrack() {
    //backtrack_mode m = backtrack_mode::trust_learning;
    if (backtrack_mode == backtrack_mode_t::simplest) {
      std::fill(std::begin(variable_state), std::end(variable_state), variable_state_t::unassigned);
      actions.clear();
      
      // We want to start with a clean slate here.
      while (!units.empty()) {
        units.pop();
      }
    }
    else if (backtrack_mode == backtrack_mode_t::trust_learning) {
      // do nothing!
    }

  }

  bool add_clause(const clause_t& c) {

    //special case if we learned a unit or similar:
    if (c.size() == 1) {
      //std::cout << "COMITTING" << std::endl;
      commit_literal(cnf, c[0]);
      // sometimes this can happen!
      while (literal_t u = find_unit(cnf)) {
        commit_literal(cnf, u);
      }
      if (immediately_unsat(cnf)) {
        action_t a;
        a.action_kind = action_t::action_kind_t::halt_unsat;
        actions.push_back(a);
      }
      else {
        reset();
      }
      return false;
    }
    else if (c.size() == 0) {
      //std::cout << "RESETTING" << std::endl;
      reset(); // we're about to fail...?
      return false;
    }
    else {
      size_t id = cnf.size();
      cnf.push_back(c);

      for (literal_t l : c) {
        literal_to_clause[l].push_back(id);
      }
      // this isn't necessarily right.
      new_watch(id);
      check_watch_correctness();
    }
    return true;
  }

  void learn_clause();

};
std::ostream& operator<<(std::ostream& o, const std::vector<action_t> v) {
  for (auto& a : v) {
    o << a << std::endl;
  }
  return o;
}


void trace_t::learn_clause() {
  assert(actions.rbegin()->action_kind == action_t::action_kind_t::halt_conflict);
  //std::cout << "About to learn clause from: " << *this << std::endl;
  if (learn_mode == learn_mode_t::simplest) {
    clause_t new_clause;
    for (action_t a : actions) {
      if (a.action_kind == action_t::action_kind_t::decision) {
        new_clause.push_back(-a.decision_literal);
      }
    }
    //std::cout << "Learned clause: " << new_clause << std::endl;
    add_clause(new_clause);
  }
  else if (learn_mode == learn_mode_t::explicit_resolution) {
    // Get the initial conflict clause
    auto it = actions.rbegin();
    assert(it->action_kind == action_t::action_kind_t::halt_conflict);
    // We make an explicit copy of that conflict clause
    clause_t c = cnf[it->conflict_clause_id];

    it++;

    // now go backwards until the decision, resolving things against it
    auto restart_it = it;
    for (; it->action_kind != action_t::action_kind_t::decision; it++) {
      assert(it->action_kind == action_t::action_kind_t::unit_prop);
      clause_t d = cnf[it->unit_prop.reason];
      if (literal_t r = resolve_candidate(c, d)) {
        c = resolve(c, d, r);
      }
    }

    // reset our iterator
    int counter = 0;
    literal_t new_implied = 0;
    it = std::next(actions.rbegin());

    for (; it->action_kind != action_t::action_kind_t::decision; it++) {
      assert(it->action_kind == action_t::action_kind_t::unit_prop);
      if (contains(c, -it->unit_prop.propped_literal)) {
        counter++;
        new_implied = it->get_literal();
      }
    }
    assert(it->action_kind == action_t::action_kind_t::decision);
    if (contains(c, -it->decision_literal)) {
      counter++;
      new_implied = -it->get_literal();
    }

    //std::cout << "Learned clause " << c << std::endl;
    //std::cout << "Counter = " << counter << std::endl;
    assert(counter == 1);

    // didn't just add a clause, but learned a unit:
    if (!add_clause(c)) {
      return;
    }

    if (backtrack_mode == backtrack_mode_t::trust_learning) {
      // create the action that this new clause is unit_propped by the trail
      action_t a;
      a.action_kind = action_t::action_kind_t::unit_prop;
      a.unit_prop.reason = cnf.size() - 1;
      a.unit_prop.propped_literal = new_implied;

      // TODO: Show that this is nonlinear! That this backtracking is really worth-it.

      // we also do the backtracking here:
      it++; // start the next level of our decision trail
      // Continue going down our level
      for (; it != actions.rend(); it++) {
        literal_t l = it->get_literal();
        if (contains(c, -l)) {
          break;
        }
      }
      auto fwd_it = it.base();
      assert(fwd_it != actions.end());
      // now go back to the start of the "next" decision:
      while (!fwd_it->is_decision()) {
        fwd_it++;
        assert(fwd_it != actions.end());
      }
      //std::cout << "fwd_it : " << *fwd_it << std::endl;
      //std::cout << "it: " << *it << std::endl;
      //std::cout << "actions: " << actions << std::endl;
      assert(fwd_it->is_decision());

      // Backtrack!
      for (auto reset_it = fwd_it; reset_it != actions.end(); reset_it++) {
        if (reset_it->has_literal()) {
          literal_t l = reset_it->get_literal();
          unassign_literal(l);
        }
      }
      actions.erase(fwd_it, std::end(actions));

      // correctness check: let's make sure we really do induce
      // the new unit_prop:
      if (count_unassigned_literals(a.unit_prop.reason) != 1) {
        for (auto u : cnf[a.unit_prop.reason]) {
          if (literal_unassigned(u)) {
            //std::cout << "Unassigned: " << u << std::endl;
          }
        }
        //std::cout << "Count is: " << count_unassigned_literals(a.unit_prop.reason) << std::endl;
        //std::cout << "actions: " << actions << std::endl;
      }
      assert(count_unassigned_literals(a.unit_prop.reason) == 1);
      //std::cout << find_unassigned_literal(a.unit_prop.reason) << std::endl;
      //std::cout << a.unit_prop.propped_literal << std::endl;
      assert(find_unassigned_literal(a.unit_prop.reason) == a.unit_prop.propped_literal);
      // TODO: do we need to continue unit-prop here?
      // Eep, for now, I'll just rely on the next call to unit_prop?
      if (unit_prop_mode == unit_prop_mode_t::queue) {
        // We want to start with a clean slate here.
        while (!units.empty()) {
          units.pop();
        }
        units.push(a);
      }
    }
  }
}


cnf_t load_cnf() {
  cnf_t cnf;
  // Load in cnf from stdin.
  literal_t next_literal;
  clause_t next_clause;

  std::string line;
  while (std::getline(std::cin, line)) {
    if (line.size() == 0) { continue; }
    if (line[0] == 'c') { continue; }
    if (line[0] == 'p') {
      // TODO: do some error-checking;
      break;
    }
  }

  while (std::cin >> next_literal) {
    if (next_literal == 0) {
      cnf.push_back(next_clause);
      next_clause.clear();
      continue;
    }
    next_clause.push_back(next_literal);
  }
  return cnf;
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
std::ostream& operator<<(std::ostream& o, const trace_t t) {
  return o << t.variable_state << std::endl << t.actions;
}

std::string report_conclusion(action_t::action_kind_t conclusion) {
  switch(conclusion) {
  case action_t::action_kind_t::decision: return "ERROR";
  case action_t::action_kind_t::unit_prop: return "ERROR";
  case action_t::action_kind_t::backtrack: return "ERROR";
  case action_t::action_kind_t::halt_conflict: return "ERROR";
  case action_t::action_kind_t::halt_unsat: return "UNSATISFIABLE";
  case action_t::action_kind_t::halt_sat: return "SATISFIABLE";
  }
  return "ERROR";
}
// The real goal here is to find conflicts as fast as possible.
int main(int argc, char* argv[]) {

  // Instantiate our CNF object
  cnf_t cnf = load_cnf();
  //print_cnf(cnf);
  //std::cout << "CNF DONE" << std::endl;
  assert(cnf.size() > 0); // make sure parsing worked.

  // Do the naive unit_prop
  while (literal_t u = find_unit(cnf)) {
    commit_literal(cnf, u);
  }

  // TODO: fold this into a more general case, if possible.
  if (immediately_unsat(cnf)) {
    std::cout << "UNSATISFIABLE" << std::endl;
    return 0;
  }
  if (immediately_sat(cnf)) {
    std::cout << "SATISFIABLE" << std::endl;
    return 0;
  }

  assert(!find_unit(cnf));

  // Preprocess trace. This is to clean out "degenerate" aspects,
  // like units and trivial CNF cases.
  trace_t trace(cnf);
  // trace.seed_units_queue(); // outmoded, we fold away those entirely.
  trace.process();
  //std::cout << trace;

  while (!trace.final_state()) {
    assert(trace.actions.rbegin()->action_kind == action_t::action_kind_t::halt_conflict);
    trace.learn_clause();
    trace.backtrack();
    trace.process();
  }
  std::cout << report_conclusion(trace.actions.rbegin()->action_kind) << std::endl;


  // do the main loop
}
