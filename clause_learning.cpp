#include "clause_learning.h"
#include "trace.h"
#include "lcm.h"

// Debugging purposes
bool verify_resolution_expected(const clause_t& c, const trail_t& actions) {
  if (!c.empty()) {
    // correctness checks:
    // reset our iterator
    int counter = 0;
    literal_t new_implied = 0;
    auto it = std::next(actions.rbegin());

    for (; it->action_kind != action_t::action_kind_t::decision; it++) {
      SAT_ASSERT(it->action_kind == action_t::action_kind_t::unit_prop);
      if (contains(c, -it->unit_prop.propped_literal)) {
        counter++;
        new_implied = it->get_literal();
      }
    }
    SAT_ASSERT(it->action_kind == action_t::action_kind_t::decision);
    if (contains(c, -it->decision_literal)) {
      counter++;
      new_implied = -it->get_literal();
    }

    //std::cout << "Learned clause " << c << std::endl;
    //std::cout << "Counter = " << counter << std::endl;
    SAT_ASSERT(counter == 1);
  }
  return true;
}



clause_t learn_clause(const cnf_t& cnf, const trail_t& actions) {
  SAT_ASSERT(actions.rbegin()->action_kind == action_t::action_kind_t::halt_conflict);
  //std::cout << "About to learn clause from: " << *this << std::endl;

  if (learn_mode == learn_mode_t::simplest) {
    clause_t new_clause;
    for (action_t a : actions) {
      if (a.action_kind == action_t::action_kind_t::decision) {
        new_clause.push_back(-a.decision_literal);
      }
    }
    //std::cout << "Learned clause: " << new_clause << std::endl;
    return new_clause;
  }

  else if (learn_mode == learn_mode_t::explicit_resolution) {
    /*
      auto it = actions.rbegin();
      SAT_ASSERT(it->action_kind == action_t::action_kind_t::halt_conflict);
      // We make an explicit copy of that conflict clause
      clause_t c = cnf[it->conflict_clause_id];

      it++;

      // now go backwards until the decision, resolving things against it
      for (; it != std::rend(actions) && it->action_kind != action_t::action_kind_t::decision; it++) {
      SAT_ASSERT(it->action_kind == action_t::action_kind_t::unit_prop);
      clause_t d = cnf[it->unit_prop.reason];
      if (literal_t r = resolve_candidate(c, d)) {
      c = resolve(c, d, r);
      }
      }
    */

    // now try the stamping method
    std::vector<literal_t> C;
    {
      const size_t D = actions.level();
      auto it = actions.rbegin();


      SAT_ASSERT(it->action_kind == action_t::action_kind_t::halt_conflict);
      const clause_t& c = cnf[it->conflict_clause_id];
      it++;

      // Everything in our conflict clause is false, and
      // we want to take that as starting points for resolution.
      int counter = 0;
      std::vector<literal_t> stamped;
      for (literal_t l : c)  {
        stamped.push_back(-l);
        if (actions.level(-l) < D) {
          C.push_back(l);
        }
        else {
          counter++;
        }
      }

      //for (; it != std::rend(actions) && it->is_unit_prop(); it++) {
      for (; it != std::rend(actions) && it->is_unit_prop(); it++) {
        literal_t L = it->get_literal();
        // L is *not* negated, so this is a resolution candidate with our
        // current conflict clause!
        if (contains(stamped, L)) {
          const clause_t& d = cnf[it->unit_prop.reason];
          for (literal_t a : d) {
            if (a == L) continue;
            // We care about future resolutions, so we negate a
            if (!contains(stamped, -a)) {
              stamped.push_back(-a);
              if (actions.level(-a) < D) {
                C.push_back(a);
              }
            }
          }
        }
      }
      if (it != std::rend(actions) && it->is_decision() && contains(stamped, it->get_literal())) {
        C.push_back(-it->get_literal());
      }

      //std::cout << "Counter: " << counter << std::endl;
      //assert(counter == 1);
      std::sort(std::begin(C), std::end(C));

      // Now we have a lot of things stamped. Everything that's not
      // in our current decision level compromises the actual learned clause
    }
    //std::cout << "C: " << C << "; c: " << c << std::endl;
    //assert(C == c);



    SAT_ASSERT(verify_resolution_expected(C, actions));

    learned_clause_minimization(cnf, C, actions);
    return C;
  }
  assert(0);
  return {};
}
