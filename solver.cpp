#include "solver.h"

#include "backtrack.h"
#include "clause_learning.h"
#include "lcm.h"
#include "measurements.h"
#include "subsumption.h"

// Defaults: fast and working...
backtrack_mode_t backtrack_mode = backtrack_mode_t::nonchron;
// unit_prop_mode_t unit_prop_mode = unit_prop_mode_t::queue;
unit_prop_mode_t unit_prop_mode = unit_prop_mode_t::watched;
variable_choice_mode_t variable_choice_mode =
    variable_choice_mode_t::nextclause;

// We create a local copy of the CNF.
solver_t::solver_t(const cnf_t &CNF)
    : cnf(CNF), literal_to_clauses_complete(max_variable(cnf)),
      stamped(max_variable(cnf)), watch(cnf, trail, unit_queue),
      vsids(cnf, trail), vmtf(cnf, trail), acids(cnf, trail), lbm(cnf) {
  variable_t max_var = max_variable(cnf);
  trail.construct(max_var);

  install_core_plugins();
  install_watched_literals();
  install_lcm(); // we want LCM early on, to improve, e.g., LBD values.
  install_lbm();
  install_restart();
  install_literal_chooser();

  remove_clause_p.add_listener([&](clause_id cid) { cnf.remove_clause(cid); });
  // Optional.
  // install_metrics_plugins();
}
auto find_unit_clause(const cnf_t &cnf, const trail_t &trail) {
  return std::find_if(std::begin(cnf), std::end(cnf), [&](clause_id cid) {
    return !trail.clause_sat(cnf[cid]) &&
           trail.count_unassigned_literals(cnf[cid]) == 1;
  });
}

bool has_unit_clause(const cnf_t &cnf, const trail_t &trail) {
  auto it = find_unit_clause(cnf, trail);
  if (it != std::end(cnf)) {
    std::cerr << "Found unit clause: " << cnf[*it] << std::endl;
  }
  return it != std::end(cnf);
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
  clause_added_p.add_listener([&](clause_id cid) {
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
#if 0
                         if (trail.level() > 200) {
                           // The key insight is that any assignment that is made before xnext after
                           // a restart must also have been assigned before the restart.
                           literal_t n = vsids.choose();
                           float nscore = vsids.score(n);
                           size_t level = 0;
                           for (auto it = trail.begin(); it != trail.end(); it++) {
                             if (!it->is_decision()) {
                               continue;
                             }
                             level++;
                             literal_t l = it->get_literal();
                             if (vsids.score(l) < nscore) break; // this is the first decision we'd change.
                           }
                           while (trail.level() >= level) trail.pop();
                         } else {
#endif
    while (trail.level())
      trail.pop();
#if 0
                         }
#endif
  });

  // Invariants:
#ifdef SAT_DEBUG_MODE
  // before_decision.precondition(
  //[&](const cnf_t &avoid) { SAT_ASSERT(!has_unit_clause(cnf, trail)); });
#endif
}

// These install various counters to track interesting things.
void solver_t::install_metrics_plugins() {
  // Restart counter:
  static size_t restart_counter = 0;
  restart_p.add_listener([&]() { restart_counter++; });
  print_metrics_plugins_p.add_listener([&]() {
    std::cout << "Total restarts:\t\t\t" << restart_counter << std::endl;
  });

  static std::chrono::time_point<std::chrono::steady_clock> start_solve_time;
  static std::chrono::time_point<std::chrono::steady_clock> end_solve_time;
  start_solve_p.add_listener(
      [&]() { start_solve_time = std::chrono::steady_clock::now(); });
  end_solve_p.add_listener(
      [&]() { end_solve_time = std::chrono::steady_clock::now(); });
  print_metrics_plugins_p.add_listener([&]() {
    std::chrono::duration<double> elapsed = end_solve_time - start_solve_time;
    std::cout << "Total solve time:\t\t" << elapsed.count() << "s" << std::endl;
  });

  static size_t total_decisions = 0;
  choose_literal_p.add_listener([&](const literal_t &l) { total_decisions++; });
  print_metrics_plugins_p.add_listener([&]() {
    std::cout << "Total decisions:\t\t" << total_decisions << std::endl;
  });

  static size_t clause_learned_size = 0;
  static size_t clause_learned_count = 0;
  clause_added_p.add_listener([&](const clause_id cid) {
    clause_learned_count++;
    clause_learned_size += cnf[cid].size();
  });
  print_metrics_plugins_p.add_listener([&]() {
    std::cout << "Average clause learned size:\t"
              << (double(clause_learned_size) / double(clause_learned_count))
              << std::endl;
  });
}

void solver_t::report_metrics() { print_metrics_plugins_p(); }

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

  clause_added_p.add_listener([&](clause_id cid) {
    const clause_t &c = cnf[cid];
    if (c.size() > 1)
      watch.watch_clause(cid);
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

void solver_t::naive_cleaning() {
  static lit_bitset_t seen(max_variable(cnf));
  for (action_t a : trail) {
    if (trail.level(a))
      break;
    if (!a.is_unit_prop())
      continue;
    clause_id cid = a.get_clause();
    {
      const clause_t &c = cnf[cid];
      if (c.size() != 1)
        continue;
    }

    literal_t l = a.get_literal();
    if (seen.get(l))
      continue;
    seen.set(l);

    // Collect all clauses with that literal.
    // std::cerr << "Cleaning literal " << l << std::endl;
    std::vector<clause_id> to_remove;
    std::copy_if(std::begin(cnf), std::end(cnf), std::back_inserter(to_remove),
                 [&](clause_id cid) { return contains(cnf[cid], l); });
    // Don't remove those on the trail
    auto to_save_it = std::remove_if(
        std::begin(to_remove), std::end(to_remove),
        [&](clause_id cid) { return trail.contains_clause(cid); });
    std::for_each(std::begin(to_remove), to_save_it,
                  [&](const clause_id cid) { remove_clause(cid); });
    // std::cerr << "Removed " << to_remove.size() << " clauses" << std::endl;

    // Remove literals
    to_remove.clear();
    std::copy_if(std::begin(cnf), std::end(cnf), std::back_inserter(to_remove),
                 [&](clause_id cid) { return contains(cnf[cid], neg(l)); });
    auto to_not_clean_it = std::remove_if(
        std::begin(to_remove), std::end(to_remove),
        [&](clause_id cid) { return trail.contains_clause(cid); });
    std::for_each(std::begin(to_remove), to_not_clean_it,
                  [&](const clause_id cid) { remove_literal(cid, neg(l)); });
    // std::cerr << "Cleaned " << to_remove.size() << " clauses" << std::endl;
  }
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
      while (std::end(to_remove) != et)
        to_remove.pop_back();

      for (auto cid : to_remove)
        remove_clause(cid);

      cnf.clean_clauses();
    }
  });

  learned_clause_p.add_listener(
      [&](clause_t &c, trail_t &trail) { lbm.push_value(c, trail); });

  clause_added_p.add_listener([&](clause_id cid) { lbm.flush_value(cid); });
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
}

// This is my poor-man's attempt at on-the-fly subsumption... to  be honest
INLINESTATE void solver_t::backtrack_subsumption(clause_t &c, action_t *a,
                                                 action_t *e) {
  // TODO: mesh this with on-the-fly subsumption?
  // size_t counter = 0;
  for (; a != e; a++) {
    if (a->has_clause()) {
      const clause_t &d = cnf[a->get_clause()];
      if (c.size() >= d.size())
        continue;
      if (!c.possibly_subsumes(d))
        continue;
      // if (subsumes(c, d)) {
      if (subsumes_and_sort(c, d)) {
        // counter++;
        // std::cerr << "removing clause! " << cnf[a->get_clause()] << "
        // with " << c << std::endl;
        remove_clause(a->get_clause());
      }
    }
  }
  // if (counter) std::cerr << counter << std::endl;
}

// This is the core method:

// These are the "function" versions of the plugins. Trying to measure how much
// overhead there is.
void solver_t::before_decision(cnf_t &cnf) {
  if (use_plugins) {
    before_decision_p(cnf);
    return;
  }

  // Do LBM cleaning
  if (lbm.should_clean(cnf)) {
    auto to_remove = lbm.clean();
    // don't remove things on the trail
    auto et =
        std::remove_if(std::begin(to_remove), std::end(to_remove),
                       [&](clause_id cid) { return trail.uses_clause(cid); });
    while (std::end(to_remove) != et) {
      to_remove.pop_back();
    }

    for (auto cid : to_remove) {
      SAT_ASSERT(cnf[cid].size() > 2);
      remove_clause(cid);
    }

    cnf.clean_clauses();

  }


  // Restart policy
  if (ema_restart.should_restart()) {
    restart(); // this is calling the solver's "restart" plugin!
  }
}
INLINESTATE void solver_t::apply_unit(literal_t l, clause_id cid) {
  if (use_plugins) {
    apply_unit_p(l, cid);
    return;
  }
  trail.append(make_unit_prop(l, cid));
  watch.literal_falsed(l);
}
INLINESTATE void solver_t::apply_decision(literal_t l) {
  if (use_plugins) {
    apply_decision_p(l);
    return;
  }
  trail.append(make_decision(l));
  watch.literal_falsed(l);
}
INLINESTATE void solver_t::remove_clause(clause_id cid) {
  if (use_plugins) {
    remove_clause_p(cid);
    return;
  }
  watch.remove_clause(cid);
  cnf.remove_clause(cid);
}
INLINESTATE void solver_t::clause_added(clause_id cid) {
  if (use_plugins) {
    clause_added_p(cid);
    return;
  }
  const clause_t &c = cnf[cid];

  if (c.size() > 1)
    watch.watch_clause(cid);
  SAT_ASSERT(watch.validate_state());

  lbm.flush_value(cid);
}
INLINESTATE void solver_t::learned_clause(clause_t &c, trail_t &trail) {
  if (use_plugins) {
    learned_clause_p(c, trail);
    return;
  }
  learned_clause_minimization(cnf, c, trail, stamped);

  lbm.push_value(c, trail);

  // Update the emas

  ema_restart.step(lbm.value_cache);

  vsids.clause_learned(c);
}
INLINESTATE void solver_t::remove_literal(clause_id cid, literal_t l) {
  if (use_plugins) {
    remove_literal_p(cid, l);
    return;
  }
  watch.remove_clause(cid);
  if (cnf[cid].size() > 1) {
    watch.watch_clause(cid);
  }
}
INLINESTATE void solver_t::restart() {
  if (use_plugins) {
    restart_p();
    return;
  }
  while (trail.level())
    trail.pop();

  vsids.static_activity();

  ema_restart.reset();
}
INLINESTATE void solver_t::choose_literal(literal_t &l) {
  if (use_plugins) {
    choose_literal_p(l);
    return;
  }
  l = vsids.choose();
}
INLINESTATE void solver_t::start_solve() {
  if (use_plugins) {
    start_solve_p();
    return;
  }
}
INLINESTATE void solver_t::end_solve() {
  if (use_plugins) {
    end_solve_p();
    return;
  }
#if 0
  std::cerr << watch.metrics.rewatch_count << std::endl;
  std::cerr << watch.metrics.rewatch_iterations << std::endl;
  std::cerr << ((double)watch.metrics.rewatch_iterations)/((double)(watch.metrics.rewatch_count)) << std::endl;
#endif
}

bool solver_t::solve() {
  SAT_ASSERT(state == state_t::quiescent);

  start_solve();

  timer::initialize();

  // for (auto cid : cnf) { assert(std::is_sorted(std::begin(cnf[cid]),
  // std::end(cnf[cid]))); }

  for (;;) {
     //std::cerr << "State: " << static_cast<int>(state) << std::endl;
     //std::cerr << "Trail: " << trail << std::endl;
    switch (state) {
    case state_t::quiescent: {
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

    case state_t::check_units:

      while (!unit_queue.empty()) {

        auto e = unit_queue.pop();
        literal_t l = e.l;
        clause_id c = e.c;
        if (trail.literal_unassigned(l)) {
          apply_unit(l, c);
        }

        if (halted()) {
          break;
        }
      }

      if (halted()) {
        state = state_t::conflict;
      } else {
        state = state_t::quiescent;
      }

      break;

    case state_t::conflict: {
      // TODO: how handle on-the-fly subsumption?
      clause_t c = learn_clause(cnf, trail, stamped);

      // Minimize, cache lbm score, etc.
      // Order matters (we want to minimize before LBM'ing)
      learned_clause(c, trail);

      // early out in the unsat case.
      if (c.empty()) {
        state = state_t::unsat;
        break;
      }

      action_t *target = backtrack(c, trail);

      backtrack_subsumption(c, target, std::end(trail));
      trail.drop_from(target);

      // Experimentally validated that there are no "leftover" units
      // we're missing out on.
      unit_queue.clear();

      // High-level assert:
      // // the clause is unique
      // // the clause contains no literals that are level 0 (proven-fixed)

#if 1
      if (c.size() == 1) {
        auto l = c[0];

#if 0
        // weird heurstic: what about just what we're currently watching?
        // Doesn't seem to hurt or help.
        for (auto&& [did, ol] : watch.literals_to_watcher[l]) {
          remove_clause(did);
        }
        for (auto&& [did, ol] : watch.literals_to_watcher[neg(l)]) {
          remove_literal(did, neg(l));
        }
#else
        auto n = neg(l);
        for (auto did : cnf) {
          if (trail.contains_clause(did))
            continue;

          const clause_t& d = cnf[did];
          if (contains(d, n)) {
            remove_literal(did, n);
            // todo: what if this induces a unit?
            //if (cnf[did].size() == 1) {
              //std::cerr << "KNOCK-ON-UNIT" << std::endl;
            //}
          }
          if (contains(d, l)) {
            remove_clause(did);
          }
        }
#endif
      }
#endif

      // Core action -- we decide to commit the clause.
      clause_id cid = cnf.add_clause(std::move(c));

      // Commit the clause, the LBM score of that clause, and so on.
      clause_added(cid);


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