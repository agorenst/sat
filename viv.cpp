#include "solver.h"

// Takes a CNF, and vivifies all the clauses

// Sort of like a solver, but not really.
struct vivifier_t {

  cnf_t& cnf;
  trail_t trail;
  watched_literals_t watch;
  unit_queue_t unit_queue;

  plugin<literal_t> apply_decision;
  plugin<literal_t,clause_id> apply_unit;

  void install_core_plugins() {
    apply_decision.add_listener(
                                [&](literal_t l) { trail.append(make_decision(l)); });
    apply_unit.add_listener([&](literal_t l, clause_id cid) {
                              trail.append(make_unit_prop(l, cid));
                            });
  }
  void install_watched_literals() {
    apply_decision.add_listener([&](literal_t l) {
                                  watch.literal_falsed(l);
                                  SAT_ASSERT(halted() || watch.validate_state());
                                });
    apply_unit.add_listener([&](literal_t l, clause_id cid) {
                              watch.literal_falsed(l);
                              SAT_ASSERT(halted() || watch.validate_state());
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
      action_t a = unit_queue.pop();
      apply_unit(a.get_literal(), a.get_clause());
      if (halted()) {
        break;
      }
    }
    return did_work;
  }

  bool vivify(clause_id cid) {
    if (!watch.clause_watched(cid)) return false;

    clause_t& c = cnf[cid];
    //std::cerr << "[VIV][VERBOSE] Starting to viv " << c << std::endl;

    // Provisionally stop watching c.
    // This is simulating "cnf \ c".
    watch.remove_clause(cid);

    bool did_work = false;
    auto it = std::begin(c);
    for (; it != std::end(c); it++) {
      literal_t l = neg(*it);
      apply_decision(l);
      bool did_prop = drain_units();
      if (!did_prop) continue;

      // Case 1: there's a conflict:
      // if there's a conflict?
      if (halted()) {
        // assert it's a conflict
        assert(trail.crbegin()->action_kind == action_t::action_kind_t::halt_conflict);
        if (std::next(it) != std::end(c)) {
          std::cerr << "Case 1: " << c << " into ";
          c.erase(std::next(it), std::end(c));
          std::cerr << c << std::endl;
          did_work = true;
        }
        break;
      }

      // Case 2: we unit-propped something
      // that's in our clause:

      // else, see if we've assigned any future literal
      auto jt = std::find_if(std::next(it), std::end(c),
                             [&](literal_t l) {
                               return !trail.literal_unassigned(l);
                             });

      if (jt == std::end(c)) continue;

      literal_t j = *jt;
      if (trail.literal_false(j)) {
        // case 1, we can exactly remove j.
        std::cerr << "Case 2a: " << c << " into ";
        std::iter_swap(jt, std::prev(std::end(c)));
        c.pop_back();
        std::cerr << c << std::endl;
        did_work = true;
        break;
      }
      else {
        assert(trail.literal_true(j));
        // case 2, we can just append j on and that's

        // Include j in our clause
        if (std::next(std::next(it)) != std::end(c)) {
          std::cerr << "Case 2b: " << c << " into ";
          std::iter_swap(jt, std::next(it));
          it++;

          // Erase everything after that
          c.erase(std::next(it), std::end(c));
          std::cerr << c << std::endl;
          did_work = true;
        }
        break;
      }
    }

    // reset completely.
    while (trail.level()) {
      trail.pop();
    }
    unit_queue.clear();

    if (c.size() > 1) watch.watch_clause(cid);

    return did_work;
  }

  bool vivify() {
    // Loop until quiescent.
    // Report if there's any change.
    bool change = false;
    
    // The iterator we use here shouldn't
    while (std::any_of(std::begin(cnf),
                       std::end(cnf),
                       [&](clause_id cid) { return vivify(cid); })) {
      change = true;
    }
    return change;
  }

  vivifier_t(cnf_t& CNF)
    : cnf(CNF),
      watch(cnf, trail, unit_queue) {
    variable_t max_var = max_variable(cnf);
    trail.construct(max_var);
    // TODO: This should be unnecessary?
    for (clause_id cid : cnf) {
      if (cnf[cid].size() > 1) watch.watch_clause(cid);
    }
    SAT_ASSERT(watch.validate_state());
    install_core_plugins();
    install_watched_literals();
  }

};

bool VIV(cnf_t& cnf) {
  vivifier_t viv(cnf);
  return viv.vivify();
}
