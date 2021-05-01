#pragma once
#include "cnf.h"
#include "ema.h"
#include "lbd.h"
#include "plugins.h"
#include "positive_only_literal_chooser.h"
#include "unit_queue.h"
#include "vsids.h"
#include "watched_literals.h"

struct solver_t {
  // The root data structure, the "true" CNF.
  // Currently we'll add learned clauses directly to this.
  // (Rather than having a paritition of "original" clauses
  // and "learned" clauses)
  cnf_t cnf;

  lit_bitset_t stamped;

  // This is the history of committed actions we have.
  // This is the thing to ask if you need to know if a literal
  // is true, false, or unassigned (or its level, etc. etc.)
  trail_t trail;

  // This is the TWL scheme first implemented in chaff.
  watched_literals_t watch;
  const_watched_literals_t const_watch;

  // This is our queue of "pending" units that we need to prop.
  // We separate this rather than put things on the trail directly
  // originally to keep things simple. I wonder if we can adjust our
  // model so that this is unecessary.
  unit_queue_t unit_queue;

  // This VSIDS object is how we choose our literals.
  vsids_t vsids;
  vsids_heap_t vsids_heap;
  positive_only_literal_chooser_t polc;

  // Our main clause-removal heuristic:
  lbd_t lbd;

  // We model our solver as a state machine.
  // These are the fundamental states it can be in:
  enum class state_t { quiescent, check_units, conflict, sat, unsat };
  state_t state = state_t::quiescent;

  // These are the various listeners for
  literal_t decision_literal;
  plugin<cnf_t &> before_decision_p;
  plugin<literal_t &> choose_literal_p;
  plugin<literal_t> apply_decision_p;

  literal_t unit_literal;
  clause_id unit_reason;
  // plugin<> unit_pop;
  plugin<literal_t, clause_id> apply_unit_p;

  // clause_t conflict_clause;
  plugin<> conflict_enter;
  plugin<> conflict_clause_process;

  action_t *backtrack_level;
  plugin<> backtrack_level_process;

  plugin<const trail_t &, clause_id> added_clause;

  plugin<clause_id> process_added_clause_p;
  plugin<clause_t &, trail_t &> learned_clause_p;
  plugin<clause_id, literal_t> remove_literal_p;
  plugin<> restart_p;
  plugin<> start_solve_p;
  plugin<> end_solve_p;

  plugin<literal_t, clause_id> cdcl_resolve;

  // This are listeners for actions that occur at
  // various independent points in evolving the CNF
  plugin<clause_id> remove_clause_p;
  // plugin<> clause_add;

  plugin<> print_metrics_plugins_p;

  // And instead, let's try doing function stuff.
  void before_decision(cnf_t &);
  void apply_unit(literal_t, clause_id);
  void apply_decision(literal_t);
  void remove_clause(clause_id);
  void process_learned_clause(clause_t &, trail_t &);
  void process_added_clause(clause_id);
  void remove_literal(clause_id, literal_t);
  void restart();
  void choose_literal(literal_t &);
  void start_solve();
  void end_solve();

  void backtrack_subsumption(clause_t &c, action_t *a, action_t *e);

  // These install the fundamental actions.
  void install_core_plugins();

  void install_complete_tracker();

  void install_lcm();
  void install_lbd();
  void install_restart();
  void install_literal_chooser();

  // These install various counters to track interesting things.
  void install_metrics_plugins();

  // We create a local copy of the CNF.
  solver_t(const cnf_t &cnf);
  // This is the core method:
  bool solve();

  state_t drain_unit_queue();
  clause_id determine_conflict_clause();
  action_t *determine_backtrack_level(const clause_t &c);
  void process_backtrack_level(const clause_t &c, action_t *target);
  state_t commit_learned_clause(clause_t &&c);
  void report_metrics();

  bool halt_state(const action_t action) const {
    return action.action_kind == action_t::action_kind_t::halt_conflict ||
           action.action_kind == action_t::action_kind_t::halt_unsat ||
           action.action_kind == action_t::action_kind_t::halt_sat;
  }

  bool halted() const {
    // std::cout << "Debug: current trace is: " << *this << std::endl;
    bool result = !trail.empty() && halt_state(*trail.crbegin());
    // std::cout << "[DBG](halted): " << result << " with " <<
    // *actions.crbegin()
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

  ema_restart_t ema_restart;
};
