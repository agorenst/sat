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

  // This is used in the learn-clause routine.
  // lit_compactset_t stamped;
  lit_compactset_t compact_stamped;

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

  // Our main restart heuristic
  ema_restart_t ema_restart;

  // We model our solver as a state machine.
  // These are the fundamental states it can be in:
  enum class state_t { quiescent, check_units, conflict, sat, unsat };
  state_t state = state_t::quiescent;

  // These are the various listeners for core actions. They are a bit
  // in consistent development. They enable run-time plugins of tracing and
  // correctness checks (though such things also exist outside the plugins, for
  // now), as well as optional optimizations. Part of this exercise is to try to
  // tease out (or see why we can't tease out) distinct optimizations that have
  // been developed over the years. How much of a SAT solver is a collection of
  // "different" optimizations, and how much of it is one big soup?
  plugin<cnf_t &> before_decision;
  plugin<literal_t &> choose_literal;
  plugin<literal_t> apply_decision;
  plugin<literal_t, clause_id> apply_unit;
  plugin<> conflict_enter;
  plugin<const trail_t &, clause_id> added_clause;
  plugin<clause_id, literal_t> remove_literal;
  plugin<> restart;
  plugin<> start_solve;
  plugin<> end_solve;
  plugin<literal_t, clause_id> cdcl_resolve;
  plugin<clause_id> remove_clause;

  // These install the fundamental actions.
  void install_core_plugins();
  void install_lcm();
  void install_lbd();
  void install_restart();
  void install_literal_chooser();

  // We create a local copy of the CNF.
  solver_t(const cnf_t &cnf);
  // These are the core methods. Solve is the only real entry point,
  // the others are key helper methods.
  bool solve();
  state_t drain_unit_queue();
  clause_id determine_conflict_clause();
  action_t *determine_backtrack_level(const clause_t &c);
};
