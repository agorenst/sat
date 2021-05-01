#include <algorithm>
#include <iostream>
#include <sstream>

#include "solver.h"

#include "backtrack.h"
#include "clause_learning.h"
#include "lcm.h"
#include "measurements.h"
#include "preprocess.h"
#include "subsumption.h"
#include "viv.h"

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
        conflict_enter();
        // This is the complicated state!
        // This is where we want to spend a lot of time being very careful
        // and thinking a lot, because it is information we use to guide our
        // future search. That is why there are many sub-steps here.

        // Determine the clause we can learn from this trail.
        clause_id cid = determine_conflict_clause();

        // early out in the unsat case.
        // We do this after processing, in case if minimization is what
        // was needed to handle this.
        if (cid == nullptr) {
          state = state_t::unsat;
          break;
        }

        const clause_t &c = cnf[cid];

        // Determine the backtrack level we can go to.
        action_t *target = determine_backtrack_level(c);
        if (target == std::end(trail)) {
          state = state_t::unsat;
          break;
        }

        // Do the actual backtracking, including updating the unit queue and
        // such.
        unit_queue.clear();
        trail.drop_from(target);

        // without LCM, sometimes we can learn a unit clause that is still
        // contradicted on the trail!
        // CF: cat interesting_pm_graphs/trivalent_no_pm.txt | python
        // graph_enum.py --reduce-graph --dot | valgrind ./../build/sat
        // --only-positive-choices --trace-decisions --preprocessor-
        // --trace-clause-learning --backtrack-subsumption- --print-certificate
        // --trace-conflicts --trace-cdcl --learned-clause-minimization-
        // I think the punchline really is that if we backtrack to level 0,
        // that's also an UNSAT, but I want to be surer.

        // High-level assert:
        // // the clause is unique
        // // the clause contains no literals that are level 0 (proven-fixed)
        // TODO: ADD ABOVE TO MAX DEBUGGIN

#if 0
        MAX_ASSERT(std::is_sorted(std::begin(c), std::end(c)));
        for (const auto &d : clauses(cnf)) {
          std::vector<literal_t> D(std::begin(d), std::end(d));
          std::sort(std::begin(D), std::end(D));
          clause_t e(D);
          MAX_ASSERT(std::is_sorted(std::begin(e), std::end(e)));
          MAX_ASSERT(c != e);
        }
#endif

        MAX_ASSERT(trail.count_unassigned_literals(cnf[cid]) == 1);
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
      cond_log(settings::trace_applications, solver_action::apply_unit,
               lit_to_dimacs(l), cnf[c]);
    } else {
      cond_log(settings::trace_applications, solver_action::skip_unit,
               lit_to_dimacs(l), cnf[c]);
    }
    if (trail.conflicted()) {
      return state_t::conflict;
    }
  }
  return state_t::quiescent;
}

action_t *solver_t::determine_backtrack_level(const clause_t &c) {
  action_t *target = nonchronological_backtrack(c, trail);
  return target;
}

// We create a local copy of the CNF.
solver_t::solver_t(const cnf_t &CNF)
    : cnf(CNF),
      stamped(max_variable(cnf)),
      watch(cnf, trail, unit_queue),
      const_watch(cnf, trail, unit_queue),
      vsids(cnf, trail),
      vsids_heap(cnf, trail),
      polc(max_variable(cnf), trail),
      lbd(cnf) {
  variable_t max_var = max_variable(cnf);
  trail.construct(max_var);

  install_core_plugins();
  watch.install(*this);
  // const_watch.install(*this);

  if (settings::lbd_cleaning) {
    install_lbd();
  }
  if (settings::ema_restart) {
    install_restart();
  }
  install_literal_chooser();

  remove_clause.add_listener([&](clause_id cid) { cnf.remove_clause(cid); });
}

// These are the utilities that maintain the underlying
// CNF and trail data structures.
void solver_t::install_core_plugins() {
  apply_decision.add_listener(
      [&](literal_t l) { trail.append(make_decision(l)); });
  apply_unit.add_listener([&](literal_t l, clause_id cid) {
    trail.append(make_unit_prop(l, cid));
  });

  restart.add_listener([&]() {
    while (trail.level()) trail.pop();
  });

  remove_literal.add([&](clause_id cid, literal_t l) {
    clause_t &c = cnf[cid];
    auto it = std::find(std::begin(c), std::end(c), l);
    MAX_ASSERT(it != std::end(c));
    *it = c[c.size() - 1];
    c.pop_back();
    MAX_ASSERT(std::find(std::begin(c), std::end(c), l) == std::end(c));
  });

  // Invariants!
  if (settings::debug_max) {
    before_decision.pre([&](const cnf_t &cnf) {
      trail_t::validate(cnf, trail);
      assert(!trail_t::is_conflicted(cnf, trail));
    });
    apply_unit.pre([&](literal_t l, clause_id cid) {
      assert(trail.count_unassigned_literals(cnf[cid]) == 1);
      assert(trail.find_unassigned_literal(cnf[cid]) == l);
      assert(!trail_t::is_conflicted(cnf, trail));
    });
    choose_literal.pre(
        [&](literal_t l) { assert(!trail_t::has_unit(cnf, trail)); });
    choose_literal.post(
        [&](literal_t l) { assert(trail.literal_unassigned(l)); });
  }

  // Not guarded under a flag, run only once per solve,
  // and I think it's worth it.
  start_solve.pre([&]() {
    assert(!trail_t::has_unit(cnf, trail));
    assert(!trail_t::is_satisfied(cnf, trail));
    assert(!trail_t::is_conflicted(cnf, trail));
  });

  if (settings::trace_decisions) {
    choose_literal.postcondition([&](literal_t l) {
      if (l) log_solver_action(solver_action::apply_decision, lit_to_dimacs(l));
    });
  }
  if (settings::trace_conflicts) {
    conflict_enter.add([&]() {
      const clause_t &c = cnf[trail.rbegin()->get_clause()];
      std::vector<literal_t> d(std::begin(c), std::end(c));
      std::sort(std::begin(d), std::end(d));
      log_solver_action(solver_action::conflict, d);
    });
  }
  if (settings::trace_cdcl) {
    cdcl_resolve.add([&](literal_t l, clause_id cid) {
      const auto &c = cnf[cid];
      std::vector<literal_t> tmp(c.begin(), c.end());
      std::sort(std::begin(tmp), std::end(tmp));
      log_solver_action('\t', lit_to_dimacs(l), tmp);
    });
  }
  if (settings::trace_clause_learning) {
    added_clause.add([&](const trail_t &t, clause_id cid) {
      std::cout << trail << std::endl << cnf[cid] << std::endl;
    });
  }
}

void solver_t::install_lbd() { lbd.install(*this); }

void solver_t::install_restart() {
  restart.add_listener([&]() { ema_restart.reset(); });
  before_decision.add_listener([&](const cnf_t &cnf) {
    if (ema_restart.should_restart()) {
      restart();
    }
  });
  added_clause.add_listener([&](const trail_t &trail, clause_id cid) {
    // this is frustrating: we read into the lbd cache, nothing to do with our
    // parameters.
    assert(lbd.value_cache);
    ema_restart.step(lbd.value_cache);
  });
}

void solver_t::install_literal_chooser() {
  if (settings::only_positive_choices) {
    choose_literal.add_listener([&](literal_t &d) { d = polc.choose(); });
  } else if (settings::naive_vsids) {
    added_clause.add([&](const trail_t &trail, clause_id cid) {
      vsids.clause_learned(cnf[cid]);
    });
    cdcl_resolve.add(
        [&](literal_t l, clause_id cid) { vsids.bump_variable(var(l)); });

    choose_literal.add_listener([&](literal_t &d) { d = vsids.choose(); });
    restart.add_listener([&]() { vsids.static_activity(); });
  } else {
    added_clause.add([&](const trail_t &trail, clause_id cid) {
      vsids.clause_learned(cnf[cid]);
    });

    choose_literal.add_listener([&](literal_t &d) { d = vsids_heap.choose(); });
    restart.add_listener([&]() { vsids_heap.static_activity(); });
  }
}

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
  return "";
}

// This is a core subsidiary function. It is not a plugin. It calls plugins.
clause_id solver_t::determine_conflict_clause() {
  stamped.clear();  // This is, in effect, the current resolution clause
  std::vector<literal_t> C;        // the clause to learn
  const size_t D = trail.level();  // the conflict level

  // The end of our trail contains the conflicting clause and literal:
  auto it = trail.rbegin();
  MAX_ASSERT(it->is_conflict());

  // Get the conflict clause
  const clause_t &conflict_clause = cnf[it->get_clause()];
  // clause_t resolvent = conflict_clause.clone();
  it++;

  size_t counter =
      std::count_if(std::begin(conflict_clause), std::end(conflict_clause),
                    [&](literal_t l) { return trail.level(l) == D; });
  MAX_ASSERT(counter > 1);

  size_t resolvent_size = 0;
  auto last_subsumed = std::rend(trail);
  for (literal_t l : conflict_clause) {
    stamped.set(neg(l));
    resolvent_size++;
    if (trail.level(neg(l)) < D) {
      C.push_back(l);
    }
  }

  for (; counter > 1; it++) {
    MAX_ASSERT(it->is_unit_prop());
    literal_t L = it->get_literal();

    // We don't expect to be able to resolve against this.
    // i.e., this literal (and its reason) don't play a role
    // in resolution to derive a new clause.
    if (!stamped.get(L)) {
      continue;
    }

    // L *is* stamped, so we *can* resolve against it!

    // Report that we're about to resolve against this.
    cdcl_resolve(L, it->get_clause());

    counter--;  // track the number of resolutions we're doing.
    stamped.clear(L);
    MAX_ASSERT(resolvent_size > 0);
    resolvent_size--;

    const clause_t &d = cnf[it->get_clause()];
    // resolvent = resolve_ref(resolvent, d, neg(L));

    for (literal_t a : d) {
      if (a == L) continue;
      // We care about future resolutions, so we negate "a"
      if (!stamped.get(neg(a))) {
        resolvent_size++;
        stamped.set(neg(a));
        if (trail.level(neg(a)) < D) {
          C.push_back(a);
        } else {
          // this is another thing to resolve against.
          counter++;
        }
      }
    }

    // for (literal_t l : cnf.lit_range()) {
    // assert(contains(resolvent, l) == stamped.get(neg(l)));
    //}

    if (settings::on_the_fly_subsumption) {
      if (d.size() - 1 == resolvent_size) {
        last_subsumed = it;
        remove_literal(it->get_clause(), it->get_literal());
        MAX_ASSERT(std::find(std::begin(d), std::end(d), L) == std::end(d));
        cond_log(settings::trace_otf_subsumption, cnf[it->get_clause()],
                 lit_to_dimacs(it->get_literal()));

        // We are only good at detecting unit clauses if they're the thing we
        // learned. If on-the-fly-subsumption is able to get an intermediate
        // resolvand into unit form, we would miss that, and some core
        // assumptions of our solver would be violated. So let's look for that.
        // Experimentally things look good, but I am suspicious.
        assert(resolvent_size != 1 || counter == 1);
      }
    }
  }

  clause_id to_return = nullptr;
  bool to_lbd = true;
  if (last_subsumed == std::prev(it)) {
    to_return = last_subsumed->get_clause();
    // hack: will be re-added with "added clause"
    watch.remove_clause(to_return);
    to_lbd = lbd.remove(to_return);
    // QUESTION: how to "LCM" this? LCM with "remove_literal"?
  } else {
    while (!stamped.get(it->get_literal())) it++;

    MAX_ASSERT(it->has_literal());
    C.push_back(neg(it->get_literal()));

    MAX_ASSERT(counter == 1);
    MAX_ASSERT(C.size() == stamped.count());

    // TODO: is there a "nice" sort we can do here that could be helpful? VSIDS,
    // for instance?
    std::sort(std::begin(C), std::end(C));

    clause_t learned_clause{C};

    to_return = cnf.add_clause(std::move(learned_clause));
  }
  // Having learned the clause, now minimize it.
  // Fun fact: there are some rare cases where we're able to minimize the
  // already-existing clause, presumably thanks to our ever-smarter trail. This
  // doesn't really give us a perf benefit in my motivating benchmarks, though.
  if (settings::learned_clause_minimization) {
    learned_clause_minimization(cnf, cnf[to_return], trail, stamped);
  }

  // Special, but crucial, case.
  if (cnf[to_return].size() == 0) {
    return nullptr;
  }

  added_clause(trail, to_return);

  if (!to_lbd) {
    // this was an original clause, make it unremovable.
    // This is a *really* painful hack; an artifact of how we want to
    // remove literals in LCM without the pain of triggering constant updates
    // in, e.g., watched literals or the like.
    lbd.remove(to_return);
  }

  return to_return;
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