#include "cnf.h"
#include "watched_literals.h"
#include "unit_queue.h"
#include "vsids.h"
#include "literal_incidence_map.h"
#include "plugins.h"
#include "lbm.h"

struct solver_t {
  // The root data structure, the "true" CNF.
  // Currently we'll add learned clauses directly to this.
  // (Rather than having a paritition of "original" clauses
  // and "learned" clauses)
  cnf_t cnf;

  // This is a helper data structure that maintains a mapping of
  // literals to *all* clauses containing those literals. Much
  // more expensive than watched literals.
  literal_map_t<clause_set_t> literal_to_clauses_complete;

  // 
  lit_bitset_t stamped;

  // This is the history of committed actions we have.
  // This is the thing to ask if you need to know if a literal
  // is true, false, or unassigned (or its level, etc. etc.)
  trail_t trail;

  // This is the TWL scheme first implemented in chaff.
  watched_literals_t watch;


  // This is our queue of "pending" units that we need to prop.
  // We separate this rather than put things on the trail directly
  // originally to keep things simple. I wonder if we can adjust our
  // model so that this is unecessary.
  unit_queue_t unit_queue;

  // This VSIDS object is how we choose our literals.
  vsids_t vsids;

  // Our main clause-removal heuristic:
  lbm_t lbm;

  // We model our solver as a state machine.
  // These are the fundamental states it can be in:
  enum class state_t { quiescent, check_units, conflict, sat, unsat };
  state_t state = state_t::quiescent;

  // To keep things easy to experiment (and conceptually understand),
  // we have our core solver loop that, at certain major points, calls
  // into a sequence of handlers. These handlers are stored in these plugins.
  plugin<literal_t> apply_decision;
  plugin<literal_t,clause_id> apply_unit;
  plugin<clause_id> remove_clause;
  plugin<clause_id> clause_added;
  plugin<const cnf_t&, trail_t&> on_conflict;
  plugin<clause_t&, trail_t&> learned_clause;
  plugin<clause_id, literal_t> remove_literal;
  plugin<clause_id, literal_t> unit_detected;

  // Best opportunity for 
  plugin<cnf_t&> before_decision;

  // These install the fundamental actions.
  void install_core_plugins();

  void install_watched_literals();
  void install_lcm();
  void install_lbm();
  void install_restart();

  // These install various counters to track interesting things.
  void install_metrics_plugins();

  // We create a local copy of the CNF.
  solver_t(const cnf_t& cnf);
  // This is the core method:
  bool solve();

  void naive_cleaning();

  bool halt_state(const action_t action) const {
    return action.action_kind == action_t::action_kind_t::halt_conflict ||
      action.action_kind == action_t::action_kind_t::halt_unsat ||
      action.action_kind == action_t::action_kind_t::halt_sat;
  }

  bool halted() const {
    // std::cout << "Debug: current trace is: " << *this << std::endl;
    bool result = !trail.empty() && halt_state(*trail.crbegin());
    // std::cout << "[DBG](halted): " << result << " with " << *actions.crbegin()
    // << std::endl;
    return result;
}
bool final_state() {
  // std::cout << "Testing final state: " << *this << std::endl;
  if (trail.empty()) {
    return false;
  }
  action_t action = *trail.rbegin();

  return action.action_kind == action_t::action_kind_t::halt_unsat ||
         action.action_kind == action_t::action_kind_t::halt_sat;
}
};
