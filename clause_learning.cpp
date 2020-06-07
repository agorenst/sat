#include "clause_learning.h"
#include "lcm.h"
#include "trace.h"

learn_mode_t learn_mode = learn_mode_t::explicit_resolution;

// Debugging purposes
bool verify_resolution_expected(const clause_t& c, const trail_t& actions) {
  if (!c.empty()) {
    // correctness checks:
    // reset our iterator
    int counter = 0;
    literal_t new_implied = 0;
    auto it = std::next(actions.rbegin());

    for (; it != std::rend(actions) &&
           it->action_kind != action_t::action_kind_t::decision;
         it++) {
      if (it->action_kind != action_t::action_kind_t::unit_prop) {
        std::cerr << actions << std::endl;
        std::cerr << *it << std::endl;
      }
      SAT_ASSERT(it->action_kind == action_t::action_kind_t::unit_prop);
      if (contains(c, -it->unit_prop.propped_literal)) {
        counter++;
        new_implied = it->get_literal();
      }
    }
    if (it != std::rend(actions)) {
      SAT_ASSERT(it->action_kind == action_t::action_kind_t::decision);
      if (contains(c, -it->decision_literal)) {
        counter++;
        new_implied = -it->get_literal();
      }
    }

    // std::cout << "Learned clause " << c << std::endl;
    // std::cout << "Counter = " << counter << std::endl;
    SAT_ASSERT(counter == 1);
  }
  return true;
}

clause_t learn_clause(const cnf_t& cnf, const trail_t& actions) {
  SAT_ASSERT(actions.rbegin()->action_kind ==
             action_t::action_kind_t::halt_conflict);
  // std::cout << "About to learn clause from: " << *this << std::endl;

  // Helper -- maybe make as a trail_t method?
  auto count_level_literals = [&actions](const clause_t& c) {
    return std::count_if(std::begin(c), std::end(c), [&actions](literal_t l) {
      return actions.level(l) == actions.level();
    });
  };

  if (learn_mode == learn_mode_t::simplest) {
    assert(0);
    clause_t new_clause;
    for (action_t a : actions) {
      if (a.action_kind == action_t::action_kind_t::decision) {
        new_clause.push_back(-a.decision_literal);
      }
    }
    // std::cout << "Learned clause: " << new_clause << std::endl;
    return new_clause;
  }

  else if (learn_mode == learn_mode_t::explicit_resolution) {
    std::vector<literal_t> C;
#if 0
    auto it = actions.rbegin();
    SAT_ASSERT(it->action_kind == action_t::action_kind_t::halt_conflict);
    // We make an explicit copy of that conflict clause
    clause_t c = cnf[it->conflict_clause_id];
    it++;

    // now go backwards until the decision, resolving things against it
    //std::cout << "STARTING " << c << std::endl;
    for (;
         // this is the IUP THING!
         count_level_literals(c) > 1;
         it++) {
      SAT_ASSERT(it->action_kind == action_t::action_kind_t::unit_prop);
      SAT_ASSERT(count_level_literals(c) >= 1);
      clause_t d = cnf[it->unit_prop.reason];
      if (literal_t r = resolve_candidate(c, d,0)) {
        //std::cout << "Resolving " << c << " against " << d << " over " << r;
        c = resolve(c, d, r);
        //std::cout << " to get " << c << std::endl;
      }
    }
    SAT_ASSERT(count_level_literals(c) >= 1);
    //std::cout << "DONE" << std::endl;
    C = c;

    //std::cout << "Counter: " << count_level_literals(C) << std::endl;
    //std::cout << "Learned: " << C << std::endl;
      //assert(counter == 1);
      std::sort(std::begin(C), std::end(C));
#else
    // now try the stamping method
    {
      const size_t D = actions.level();
      auto it = actions.rbegin();

      SAT_ASSERT(it->action_kind == action_t::action_kind_t::halt_conflict);
      const clause_t& c = cnf[it->conflict_clause_id];
      it++;

      // This is the amount of things we know we'll be resolving against.
      int counter = count_level_literals(c);

      std::vector<literal_t> stamped;
      for (literal_t l : c) {
        stamped.push_back(-l);
        if (actions.level(-l) < D) {
          C.push_back(l);
        }
      }

      // for (; it != std::rend(actions) && it->is_unit_prop(); it++) {
      // for (; it != std::rend(actions) && it->is_unit_prop(); it++) {
      for (; counter > 1; it++) {
        assert(it->is_unit_prop());
        literal_t L = it->get_literal();

        // We don't expect to be able to resolve against this.
        if (!contains(stamped, L)) {
          continue;
        }

        // L *is* stamped, so we *can* resolve against it!
        counter--;  // track the number of resolutions we're doing.

        const clause_t& d = cnf[it->unit_prop.reason];
        for (literal_t a : d) {
          if (a == L) continue;
          // We care about future resolutions, so we negate a
          if (!contains(stamped, -a)) {
            stamped.push_back(-a);
            if (actions.level(-a) < D) {
              C.push_back(a);
            } else {
              counter++;
            }
          }
        }
      }
      while (!contains(stamped, it->get_literal())) it++;
      assert(it->has_literal());
      C.push_back(-it->get_literal());

      assert(count_level_literals(C) == 1);
      assert(counter == 1);

      // std::cout << "Counter: " << counter << std::endl;
      // std::cout << "Learned: " << C << std::endl;
      std::sort(std::begin(C), std::end(C));

      // Now we have a lot of things stamped. Everything that's not
      // in our current decision level compromises the actual learned clause
    }
// std::cout << "C: " << C << "; c: " << c << std::endl;
// assert(C == c);
#endif

    SAT_ASSERT(verify_resolution_expected(C, actions));
    return C;
  }
  assert(0);
  return {};
}
