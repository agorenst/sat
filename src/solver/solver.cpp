#include <iostream>

#include "solver.h"

#include "backtrack.h"
#include "clause_learning.h"
#include "lcm.h"
#include "measurements.h"
#include "preprocess.h"
#include "subsumption.h"

// The implementation model our solver uses, for editing and modularity, is a
// "plugin" model. Each algorithm is maintained by an implementation object,
// and that object is "installed" by subscribing its methods to certain
// listeners (i.e., plugging in). In this way, we are able to place all the
// different steps that go in to, e.g., maintaining watched literals as they
// related to control flow. This also hopefully serves as guiderails for us to
// avoid unneeded interdependency between algorithms, and indeed we'll strive
// for complete independence from each.

// This is the main driver loop. It reads in some high-level state of the
// solver, and dispatches to the appropriate plugin.
std::string to_string(solver_t::state_t t);
bool solver_t::solve() {
  start_solve();

  for (;;) {
    switch (state) {
      case state_t::quiescent: {
        // This may be where we decide, e.g., to restart.
        before_decision(cnf);

        literal_t l;
        choose_literal(l);

        if (l == 0) {
          state = state_t::sat;
        } else {
          apply_decision(l);
          state = state_t::check_units;
        }
        break;
      }

      case state_t::check_units: {
        // State is either "quiescent", where
        // we ran out of units, or "conflict", we found... a conflict.
        state = drain_unit_queue();

        break;
      }

      case state_t::conflict: {
        // This is the complicated state!
        // This is where we want to spend a lot of time being very careful
        // and thinking a lot, because it is information we use to guide our
        // future search. That is why there are many sub-steps here.

        // Determine the clause we can learn from this trail.
        clause_t c = determine_conflict_clause();

        // Analyze, minimize, and otherwise work on the conflict clause
        process_learned_clause(c, trail);

        // early out in the unsat case.
        // We do this after processing, in case if minimization is what
        // was needed to handle this.
        if (c.empty()) {
          state = state_t::unsat;
          break;
        }

        // Determine the backtrack level we can go to.
        action_t *target = determine_backtrack_level(c);

        // Do the actual backtracking, including updating the unit queue and
        // such.
        process_backtrack_level(c, target);

        // High-level assert:
        // // the clause is unique
        // // the clause contains no literals that are level 0 (proven-fixed)
        // TODO: ADD ABOVE TO MAX DEBUGGIN

        // Actually add the learned clause, and (in many cases) that
        // induces a new unit.
        if (settings::trace_clause_learning) {
          std::cout << "learned: " << c << std::endl;
        }
        clause_id cid = cnf.add_clause(std::move(c));
        // Commit the clause, the LBM score of that clause, and so on.
        process_added_clause(cid);
        SAT_ASSERT(trail.count_unassigned_literals(cnf[cid]) == 1);
        literal_t u = trail.find_unassigned_literal(cnf[cid]);
        // SAT_ASSERT(trail.literal_unassigned(cnf[cid][0]));

        // Add it as a unit... this is dependent on certain backtracking, I
        // think.
        unit_queue.push({u, cid});

        state = state_t::check_units;
        break;
      }

      case state_t::sat:
        end_solve();
        return true;
      case state_t::unsat:
        end_solve();
        return false;
    }
  }
}

solver_t::state_t solver_t::drain_unit_queue() {
  while (!unit_queue.empty()) {
    auto e = unit_queue.pop();
    literal_t l = e.l;
    clause_id c = e.c;
    if (trail.literal_unassigned(l)) {
      apply_unit(l, c);
      cond_log(settings::trace_applications, solver_action::apply_unit, l,
               cnf[c]);
    } else {
      cond_log(settings::trace_applications, solver_action::skip_unit, l,
               cnf[c]);
    }
    if (halted()) {
      return state_t::conflict;
    }
  }
  return state_t::quiescent;
}

clause_t solver_t::determine_conflict_clause() {
  clause_t c = learn_clause(cnf, trail, stamped);
  cond_log(settings::trace_clause_learning,
           solver_action::determined_conflict_clause, c.size());
  return c;
}

action_t *solver_t::determine_backtrack_level(const clause_t &c) {
  action_t *target = nonchronological_backtrack(c, trail);
  return target;
}

void solver_t::process_backtrack_level(clause_t &c, action_t *target) {
  backtrack_subsumption(c, target, std::end(trail));
  trail.drop_from(target);
  unit_queue.clear();
}

// We create a local copy of the CNF.
solver_t::solver_t(const cnf_t &CNF)
    : cnf(CNF),
      literal_to_clauses_complete(max_variable(cnf)),
      stamped(max_variable(cnf)),
      watch(cnf, trail, unit_queue),
      vsids(cnf, trail),
      lbm(cnf) {
  variable_t max_var = max_variable(cnf);
  trail.construct(max_var);

  install_core_plugins();
  install_watched_literals();
  install_lcm();  // we want LCM early on, to improve, e.g., LBD values.
  install_lbm();
  install_restart();
  install_literal_chooser();

  remove_clause_p.add_listener([&](clause_id cid) { cnf.remove_clause(cid); });
}

// This installs a machine to keep track of the "complete"
// map of literals-to-clauses.
void solver_t::install_complete_tracker() {
  // This builds the initial state.
  for (clause_id cid : cnf) {
    for (literal_t l : cnf[cid]) {
      literal_to_clauses_complete[l].push_back(cid);
    }
  }

  remove_clause_p.add_listener([&](clause_id cid) {
    for (literal_t l : cnf[cid]) {
      literal_to_clauses_complete[l].remove(cid);
    }
  });
  remove_literal_p.add_listener([&](clause_id cid, literal_t l) {
    literal_to_clauses_complete[l].remove(cid);
  });
  process_added_clause_p.add_listener([&](clause_id cid) {
    const clause_t &c = cnf[cid];
    for (literal_t l : c) {
      literal_to_clauses_complete[l].push_back(cid);
    }
  });
  // Invariants:
#ifdef SAT_DEBUG_MODE
  before_decision_p.precondition([&](const cnf_t &cnf) {
    for (clause_id cid : cnf) {
      for (literal_t l : cnf[cid]) {
        SAT_ASSERT(contains(literal_to_clauses_complete[l], cid));
      }
    }
    for (literal_t l : cnf.lit_range()) {
      for (clause_id cid : literal_to_clauses_complete[l]) {
        SAT_ASSERT(contains(cnf, cid));
      }
    }
  });
#endif
}

// These are the utilities that maintain the underlying
// CNF and trail data structures.
void solver_t::install_core_plugins() {
  apply_decision_p.add_listener(
      [&](literal_t l) { trail.append(make_decision(l)); });
  apply_unit_p.add_listener([&](literal_t l, clause_id cid) {
    trail.append(make_unit_prop(l, cid));
  });

  restart_p.add_listener([&]() {
    while (trail.level()) trail.pop();
  });

  // Invariants!
  if (settings::debug_max) {
    before_decision_p.precondition([&](const cnf_t &cnf) {
      trail_t::validate(cnf, trail);
      assert(!trail_t::is_conflicted(cnf, trail));
    });
    apply_unit_p.precondition([&](literal_t l, clause_id cid) {
      assert(trail.count_unassigned_literals(cnf[cid]) == 1);
      assert(trail.find_unassigned_literal(cnf[cid]) == l);
      assert(!trail_t::is_conflicted(cnf, trail));
    });
    choose_literal_p.postcondition([&](literal_t l) {
      assert(trail.literal_unassigned(l));
      assert(!trail_t::has_unit(cnf, trail));
    });
  }

  if (settings::trace_decisions) {
    choose_literal_p.postcondition([&](literal_t l) {
      log_solver_action(solver_action::apply_decision, l);
    });
  }
}

void solver_t::install_watched_literals() {
  // Do the initial thing.
  for (clause_id cid : cnf) {
    watch.watch_clause(cid);
  }

  apply_decision_p.add_listener([&](literal_t l) {
    watch.literal_falsed(l);
    SAT_ASSERT(halted() || watch.validate_state());
  });
  apply_unit_p.add_listener([&](literal_t l, clause_id cid) {
    watch.literal_falsed(l);
    SAT_ASSERT(halted() || watch.validate_state());
  });
  remove_clause_p.add_listener(
      [&](clause_id cid) { watch.remove_clause(cid); });

  process_added_clause_p.add_listener([&](clause_id cid) {
    const clause_t &c = cnf[cid];
    if (c.size() > 1) watch.watch_clause(cid);
    SAT_ASSERT(watch.validate_state());
  });
  remove_literal_p.add_listener([&](clause_id cid, literal_t l) {
    watch.remove_clause(cid);
    if (cnf[cid].size() > 1) {
      watch.watch_clause(cid);
    }
  });

  SAT_ASSERT(watch.validate_state());
}

void solver_t::install_lcm() {
  // NEEDED TO HELP OUR LOOP. THAT'S BAD.
  // As in, if we don't install this, we don't terminate? Strange.
  learned_clause_p.add_listener([&](clause_t &c, trail_t &trail) {
    learned_clause_minimization(cnf, c, trail, stamped);
  });
}

void solver_t::install_lbm() {
  before_decision_p.add_listener([&](cnf_t &cnf) {
    if (lbm.should_clean(cnf)) {
      auto to_remove = lbm.clean();
      // don't remove things on the trail
      auto et =
          std::remove_if(std::begin(to_remove), std::end(to_remove),
                         [&](clause_id cid) { return trail.uses_clause(cid); });

      // erase those
      while (std::end(to_remove) != et) to_remove.pop_back();

      for (auto cid : to_remove) remove_clause(cid);

      cnf.clean_clauses();
    }
  });

  learned_clause_p.add_listener(
      [&](clause_t &c, trail_t &trail) { lbm.push_value(c, trail); });

  process_added_clause_p.add_listener(
      [&](clause_id cid) { lbm.flush_value(cid); });
}

void solver_t::install_restart() {
  restart_p.add_listener([&]() { ema_restart.reset(); });
  before_decision_p.add_listener([&](const cnf_t &cnf) {
    if (ema_restart.should_restart()) {
      restart();
    }
  });
  learned_clause_p.add_listener([&](const clause_t &c, const trail_t &trail) {
    ema_restart.step(lbm.value_cache);
  });
}

void solver_t::install_literal_chooser() {
  // For now, we'll just install vsids.
  learned_clause_p.add_listener(
      [&](const clause_t &c, const trail_t &t) { vsids.clause_learned(c); });

  choose_literal_p.add_listener([&](literal_t &d) { d = vsids.choose(); });
  restart_p.add_listener([&]() { vsids.static_activity(); });
}

// This is my poor-man's attempt at on-the-fly subsumption... to  be honest
void solver_t::backtrack_subsumption(clause_t &c, action_t *a, action_t *e) {
  // TODO: mesh this with on-the-fly subsumption?
  // size_t counter = 0;
  for (; a != e; a++) {
    if (a->has_clause()) {
      const clause_t &d = cnf[a->get_clause()];
      if (c.size() >= d.size()) continue;
      if (!c.possibly_subsumes(d)) continue;
      if (subsumes_and_sort(c, d)) {
        remove_clause(a->get_clause());
      } else {
        cond_log(settings::trace_hash_collisions,
                 solver_action::hash_false_positive);
      }
    }
  }
  // if (counter) std::cerr << counter << std::endl;
}

// We may want to "inline" the different algorithms at some point; did that
// once, probably not again.
void solver_t::before_decision(cnf_t &cnf) { before_decision_p(cnf); }
void solver_t::apply_unit(literal_t l, clause_id cid) { apply_unit_p(l, cid); }
void solver_t::apply_decision(literal_t l) { apply_decision_p(l); }
void solver_t::remove_clause(clause_id cid) { remove_clause_p(cid); }
void solver_t::process_learned_clause(clause_t &c, trail_t &trail) {
  learned_clause_p(c, trail);
}
void solver_t::process_added_clause(clause_id cid) {
  process_added_clause_p(cid);
}
void solver_t::remove_literal(clause_id cid, literal_t l) {
  remove_literal_p(cid, l);
}
void solver_t::restart() { restart_p(); }
void solver_t::choose_literal(literal_t &l) { choose_literal_p(l); }
void solver_t::start_solve() { start_solve_p(); }
void solver_t::end_solve() { end_solve_p(); }

std::string to_string(solver_t::state_t t) {
  switch (t) {
    case solver_t::state_t::check_units:
      return "check_units";
    case solver_t::state_t::conflict:
      return "conflict";
    case solver_t::state_t::quiescent:
      return "quiescent";
    case solver_t::state_t::sat:
      return "sat";
    case solver_t::state_t::unsat:
      return "unsat";
  }
}

// LibFuzzer support:
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
  cnf_t cnf;
  bool parse_successful = cnf::io::load_cnf((char *)Data, Size, cnf);
  if (!parse_successful) {
    // We trim ourselves to just valid CNFs
    return 0;
  }
  // Just exercise the parser.
  return 0;

  cnf::transform::canon(cnf);
  preprocess(cnf);
  if (cnf::search::immediately_unsat(cnf)) {
    return 0;
  }
  if (cnf::search::immediately_sat(cnf)) {
    return 0;
  }
  solver_t solver(cnf);
  solver.solve();
  return 0;  // Non-zero return values are reserved for future use.
}