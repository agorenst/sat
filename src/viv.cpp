#include <chrono>
#include "measurements.h"
#include "settings.h"
#include "solver.h"

// Takes a CNF, and vivifies all the clauses

// Sort of like a solver, but not really.
// This supports inprocessing (you pass in the solver state), but I can't find a
// situation where it's worth it. Maybe with better filtering/heuristics.
struct vivifier_t {
  cnf_t &cnf;
  clause_id to_skip = nullptr;
  trail_t trail;
  const_watched_literals_t watch;
  unit_queue_t unit_queue;
  solver_t *solver = nullptr;

  plugin<literal_t> apply_decision;
  plugin<literal_t, clause_id> apply_unit;

  void install_core_plugins() {
    apply_decision.add_listener(
        [&](literal_t l) { trail.append(make_decision(l)); });
    apply_unit.add_listener([&](literal_t l, clause_id cid) {
      trail.append(make_unit_prop(l, cid));
    });
  }
  void install_watched_literals() {
    for (clause_id cid : cnf) {
      // TODO: we should be filtering out units aggressively?
      if (cnf[cid].size() > 1) watch.watch_clause(cid);
    }
    apply_decision.add_listener([&](literal_t l) {
      watch.literal_falsed(l);
      MAX_ASSERT(halted() || watch.validate_state(to_skip));
    });
    apply_unit.add_listener([&](literal_t l, clause_id cid) {
      watch.literal_falsed(l);
      MAX_ASSERT(halted() || watch.validate_state(to_skip));
    });
  }
  bool halt_state(const action_t action) const {
    return action.action_kind == action_t::action_kind_t::halt_conflict ||
           action.action_kind == action_t::action_kind_t::halt_unsat ||
           action.action_kind == action_t::action_kind_t::halt_sat;
  }
  bool halted() const {
    bool result = !trail.empty() && halt_state(*trail.crbegin());
    return result;
  }
  bool drain_units() {
    bool did_work = false;
    while (!unit_queue.empty()) {
      did_work = true;
      // action_t a = unit_queue.pop();
      // literal_t l = a.get_literal();
      // clause_id c = a.get_clause;
      auto e = unit_queue.pop();
      literal_t l = e.l;
      clause_id c = e.c;
      if (!trail.literal_unassigned(l)) continue;
      apply_unit(l, c);
      if (halted()) {
        break;
      }
    }
    return did_work;
  }

  bool vivify(clause_id cid) {
    if (!watch.clause_watched(cid)) return false;

    // Optimizations: what are things that are likely to not be vivified?
    cond_log(settings::trace_vivification, cnf[cid].size());

    clause_t &c = cnf[cid];
    // std::cerr << "[VIV][VERBOSE] Starting to viv " << c << std::endl;

    // Provisionally stop watching c.
    // This is simulating "cnf \ c".
    if (solver) {
      // if this is on the "real" trail, dont use it.
      if (solver->trail.uses_clause(cid)) return false;
      // solver->remove_clause(cid);
      solver->watch.remove_clause(cid);
    }
    watch.remove_clause(cid);
    to_skip = cid;

    bool did_work = false;
    auto it = std::begin(c);
    MAX_ASSERT(trail.next_index == 0);
    for (; it != std::end(c); it++) {
      MAX_ASSERT(watch.validate_state(to_skip));
      literal_t l = neg(*it);
      apply_decision(l);
      bool did_prop = drain_units();
      if (!did_prop) continue;

      // Case 1: there's a conflict:
      if (halted()) {
        MAX_ASSERT(trail.crbegin()->action_kind ==
                   action_t::action_kind_t::halt_conflict);

        if (std::next(it) != std::end(c)) {
          cond_log(settings::trace_vivification,
                   solver_action::vivification_case_1_pre, c);
          auto to_erase = std::distance(std::next(it), std::end(c));
          for (auto i = 0; i < to_erase; i++) {
            c.pop_back();
          }
          cond_log(settings::trace_vivification,
                   solver_action::vivification_case_1_post, c);
          did_work = true;
        }

        break;
      }

      // Case 2: we unit-propped something
      // that's in our clause:

      // else, see if we've assigned any future literal
      auto jt = std::find_if(std::next(it), std::end(c), [&](literal_t l) {
        return !trail.literal_unassigned(l);
      });

      if (jt == std::end(c)) continue;

      literal_t j = *jt;
      if (trail.literal_false(j)) {
        cond_log(settings::trace_vivification,
                 solver_action::vivification_case_2a_pre, c);
        std::iter_swap(jt, std::prev(std::end(c)));
        c.pop_back();
        cond_log(settings::trace_vivification,
                 solver_action::vivification_case_2a_post, c);
        did_work = true;
        break;
      } else {
        SAT_ASSERT(trail.literal_true(j));
        // case 2, we can just append j on and that's

        // Include j in our clause
        if (std::next(std::next(it)) != std::end(c)) {
          cond_log(settings::trace_vivification,
                   solver_action::vivification_case_2b_pre, c);
          std::iter_swap(jt, std::next(it));
          it++;

          // Erase everything after that
          // c.erase(std::next(it), std::end(c));
          auto to_erase = std::distance(std::next(it), std::end(c));
          for (auto i = 0; i < to_erase; i++) {
            c.pop_back();
          }
          cond_log(settings::trace_vivification,
                   solver_action::vivification_case_2b_post, c);
          did_work = true;
        }
        break;
      }
    }

    // reset completely.
    trail.drop_from(std::begin(trail));
    unit_queue.clear();

    if (c.size() > 1) watch.watch_clause(cid);
    if (solver) {
      // solver->cnf.restore_clause(cid);
      if (c.size() > 1) solver->watch.watch_clause(cid);
    }

    // cnf.restore_clause(cid);

    return did_work;
  }

  bool vivify() {
    // Loop until quiescent.
    // Report if there's any change.
    bool change = false;

    // The iterator we use here shouldn't be invalidated by vivify.
    bool continue_iterating = true;
    while (continue_iterating) {
      continue_iterating = false;
      std::vector<clause_id> worklist{std::begin(cnf), std::end(cnf)};
      for (auto cid : worklist) {
        if (vivify(cid)) {
          cond_log(settings::trace_vivification, cnf[cid]);
          change = true;
          continue_iterating = true;
        }
      }
    }
    return change;
  }

  vivifier_t(cnf_t &CNF, solver_t *solver = nullptr)
      : cnf(CNF), watch(cnf, trail, unit_queue), solver(solver) {
    variable_t max_var = max_variable(cnf);
    trail.construct(max_var);
    install_core_plugins();
    install_watched_literals();
    SAT_ASSERT(watch.validate_state());
  }
};

bool VIV(cnf_t &cnf) {
  vivifier_t viv(cnf);
  bool result = viv.vivify();
  return result;
}

bool VIV(cnf_t &cnf, solver_t *solver = nullptr) {
  vivifier_t viv(cnf, solver);
  bool result = viv.vivify();
  return result;
}
