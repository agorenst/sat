#include "solver.h"
#include "backtrack.h"
#include "clause_learning.h"
#include "lcm.h"

// We create a local copy of the CNF.
solver_t::solver_t(const cnf_t& CNF)
    : cnf(CNF),
      literal_to_clauses_complete(cnf),
      stamped(cnf),
      watch(cnf, trail, unit_queue),
      vsids(cnf, trail),
      lbm(cnf) {
  variable_t max_var = max_variable(cnf);
  trail.construct(max_var);
  for (clause_id cid : cnf) {
    watch.watch_clause(cid);
    for (literal_t l : cnf[cid]) {
      literal_to_clauses_complete[l].push_back(cid);
    }
  }
  SAT_ASSERT(watch.validate_state());
  install_core_plugins();
  install_watched_literals();
  install_lcm();
  install_lbm();
  //install_restart();
}
auto find_unit_clause(const cnf_t& cnf, const trail_t& trail) {
  return std::find_if(std::begin(cnf), std::end(cnf), [&](clause_id cid) {
    return !trail.clause_sat(cnf[cid]) &&
           trail.count_unassigned_literals(cnf[cid]) == 1;
  });
}

bool has_unit_clause(const cnf_t& cnf, const trail_t& trail) {
  auto it = find_unit_clause(cnf, trail);
  if (it != std::end(cnf)) {
    std::cerr << "Found unit clause: " << cnf[*it] << std::endl;
  }
  return it != std::end(cnf);
}

void solver_t::install_core_plugins() {
  apply_decision.add_listener(
      [&](literal_t l) { trail.append(make_decision(l)); });
  apply_unit.add_listener([&](literal_t l, clause_id cid) {
    trail.append(make_unit_prop(l, cid));
  });
  remove_clause.add_listener([&](clause_id cid) { cnf.remove_clause(cid); });

  // maintain the literal_to_clauses_complete map:
  /*
  remove_clause.add_listener([&](clause_id cid) {
                               for (literal_t l : cnf[cid]) {
                                   literal_to_clauses_complete[l].remove(cid);
                               }
                             });
  remove_literal.add_listener([&](clause_id cid, literal_t l) {
                                literal_to_clauses_complete[l].remove(cid);
  });
  clause_added.add_listener([&](clause_id cid) {
                              const clause_t& c = cnf[cid];
                              for (literal_t l : c) {
                                literal_to_clauses_complete[l].push_back(cid);
                              }
                            });
  */

  // Invariatns:
  #ifdef SAT_DEBUG_MODE
  before_decision.precondition(
      [&](const cnf_t& avoid) { SAT_ASSERT(!has_unit_clause(cnf, trail)); });
  /*
  before_decision.precondition(
    [&](const cnf_t& cnf) {
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
  */
  #endif
}

// These install various counters to track interesting things.
void solver_t::install_metrics_plugins() {}

void solver_t::install_watched_literals() {
  apply_decision.add_listener([&](literal_t l) {
    watch.literal_falsed(l);
    SAT_ASSERT(halted() || watch.validate_state());
  });
  apply_unit.add_listener([&](literal_t l, clause_id cid) {
    watch.literal_falsed(l);
    SAT_ASSERT(halted() || watch.validate_state());
  });
  remove_clause.add_listener([&](clause_id cid) { watch.remove_clause(cid); });

  clause_added.add_listener([&](clause_id cid) {
    const clause_t& c = cnf[cid];
    if (c.size() > 1) watch.watch_clause(cid);
    SAT_ASSERT(watch.validate_state());
  });
  remove_literal.add_listener([&](clause_id cid, literal_t l) {
    watch.remove_clause(cid);
    if (cnf[cid].size() > 1) {
      watch.watch_clause(cid);
    }
  });
}

void solver_t::install_lcm() {
  // NEEDED TO HELP OUR LOOP. THAT'S BAD.
  learned_clause.add_listener([&](clause_t& c, trail_t& trail) {
                                learned_clause_minimization(cnf, c, trail, stamped);
  });
}

/*
void solver_t::install_fake_on_the_fly_subsumption() {
  learned_clause.add_listener([&](clause_t& c, const trail_t& trail) {
                                for (action_t a : trail) {
                                  if (!a.has_clause()) {
                                    continue;
                                  }
                                  clause_t& c = cnf[a.get_clause()];
                                }
                              }
}
    */
void solver_t::naive_cleaning() {
  static lit_bitset_t seen(cnf);
  for (action_t a : trail) {
    if (trail.level(a)) break;
    if (!a.is_unit_prop()) continue;
    clause_id cid = a.get_clause();
    {
      const clause_t& c = cnf[cid];
      if (c.size() != 1) continue;
    }

    literal_t l = a.get_literal();
    if (seen.get(l)) continue;
    seen.set(l);

    // Collect all clauses with that literal.
    //std::cerr << "Cleaning literal " << l << std::endl;
    std::vector<clause_id> to_remove;
    std::copy_if(std::begin(cnf), std::end(cnf), std::back_inserter(to_remove),
                 [&](clause_id cid) { return contains(cnf[cid], l); });
    // Don't remove those on the trail
    auto to_save_it = std::remove_if(std::begin(to_remove), std::end(to_remove),
                                     [&](clause_id cid) { return trail.contains_clause(cid); });
    std::for_each(std::begin(to_remove), to_save_it, [&](const clause_id cid) { remove_clause(cid); });
    //std::cerr << "Removed " << to_remove.size() << " clauses" << std::endl;

    // Remove literals
    to_remove.clear();
    std::copy_if(std::begin(cnf), std::end(cnf), std::back_inserter(to_remove),
                 [&](clause_id cid) { return contains(cnf[cid], neg(l)); });
    auto to_not_clean_it = std::remove_if(std::begin(to_remove), std::end(to_remove),
                                     [&](clause_id cid) { return trail.contains_clause(cid); });
    std::for_each(std::begin(to_remove), to_not_clean_it, [&](const clause_id cid) { remove_literal(cid, neg(l)); });
    //std::cerr << "Cleaned " << to_remove.size() << " clauses" << std::endl;

  }
}

void solver_t::install_lbm() {
  before_decision.add_listener([&](cnf_t& cnf) {
    if (lbm.should_clean(cnf)) {
      auto to_remove = lbm.clean();
      auto et =
          std::remove_if(std::begin(to_remove), std::end(to_remove),
                         [&](clause_id cid) { return trail.uses_clause(cid); });
      to_remove.erase(et, std::end(to_remove));
      for (clause_id cid : to_remove) {
        remove_clause(cid);
      }
    }
  });
  remove_clause.add_listener([&](clause_id cid) {
    auto it = std::find_if(std::begin(lbm.worklist), std::end(lbm.worklist),
                           [cid](const lbm_entry& e) { return e.id == cid; });
    if (it != std::end(lbm.worklist)) {
      std::iter_swap(it, std::prev(std::end(lbm.worklist)));
      lbm.worklist.pop_back();
    }
  });
  learned_clause.add_listener(
      [&](clause_t& c, trail_t& trail) { lbm.push_value(c, trail); });

  clause_added.add_listener([&](clause_id cid) { lbm.flush_value(cid); });
}


void solver_t::install_restart() {
  static int counter = 0;
  before_decision.add_listener([&](const cnf_t& cnf) {
                                 counter++;
                                 if (counter > 10000) {
                                   counter = 0;
                                   while (trail.level()) trail.pop();
                                 }
                               });
}

// This is the core method:
bool solver_t::solve() {
  SAT_ASSERT(state == state_t::quiescent);
  for (;;) {
    // std::cerr << "State: " << static_cast<int>(state) << std::endl;
    // std::cerr << "Trail: " << trail << std::endl;
    switch (state) {
      case state_t::quiescent: {
        before_decision(cnf);

        literal_t l = vsids.choose();
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
          action_t a = unit_queue.pop();
          apply_unit(a.get_literal(), a.get_clause());
          //if (trail.level() == 0) {
            //std::cerr << "Cleaning" << std::endl;
            //naive_cleaning();
            //}
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
        // This is a core action where a plugin doesn't seem to make sense.
        // SAT_ASSERT(std::any_of(std::begin(trail), std::end(trail),
        // [](action_t a){ return a.is_decision(); })); if
        // (!std::any_of(std::begin(trail), std::end(trail), [](action_t a){
        // return a.is_decision(); })) { std::cerr << trail << std::endl;
        //}
        clause_t c = learn_clause(cnf, trail, stamped);

        // Minimize, cache lbm score, etc.
        // Order matters (we want to minimize before LBM'ing)
        learned_clause(c, trail);
        vsids.clause_learned(c);

        // early out in the unsat case.
        if (c.empty()) {
          state = state_t::unsat;
          break;
        }

        backtrack(c, trail);

        unit_queue.clear();  // ???

        // Core action -- we decide to commit the clause.
        cnf_t::clause_k cid = cnf.add_clause(c);

        // Commit the clause, the LBM score of that clause, and so on.
        clause_added(cid);
        SAT_ASSERT(trail.count_unassigned_literals(cnf[cid]) == 1);
        SAT_ASSERT(trail.literal_unassigned(cnf[cid][0]));
        unit_queue.push(make_unit_prop(cnf[cid][0], cid));

        if (final_state()) {
          state = state_t::unsat;
        } else {
          state = state_t::check_units;
        }
        break;
      }

      case state_t::sat:
        return true;
      case state_t::unsat:
        return false;
    }
  }
}
