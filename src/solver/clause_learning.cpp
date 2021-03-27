#include "clause_learning.h"

#include "lcm.h"

// learn_mode_t learn_mode = learn_mode_t::explicit_resolution;
learn_mode_t learn_mode = learn_mode_t::explicit_resolution;

// Debugging purposes
bool verify_resolution_expected(const clause_t &c, const trail_t &actions) {
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
      if (contains(c, neg(it->get_literal()))) {
        counter++;
        new_implied = it->get_literal();
      }
    }
    if (it != std::rend(actions)) {
      SAT_ASSERT(it->action_kind == action_t::action_kind_t::decision);
      if (contains(c, neg(it->get_literal()))) {
        counter++;
        new_implied = neg(it->get_literal());
      }
    }

    // std::cout << "Learned clause " << c << std::endl;
    // std::cout << "Counter = " << counter << std::endl;
    SAT_ASSERT(counter == 1);
  }
  return true;
}

clause_t simplest_learning(const trail_t &actions) {
  assert(0);
  std::vector<literal_t> new_clause;
  for (action_t a : actions) {
    if (a.action_kind == action_t::action_kind_t::decision) {
      new_clause.push_back(neg(a.get_literal()));
    }
  }
  // std::cout << "Learned clause: " << new_clause << std::endl;
  return clause_t{new_clause};
}

#if 0
clause_t explicit_resolution(const cnf_t& cnf, const trail_t& actions) {
  assert(0);
  auto count_level_literals = [&actions](const clause_t& c) {
    return std::count_if(std::begin(c), std::end(c), [&actions](literal_t l) {
      return actions.level(l) == actions.level();
    });
  };
  auto it = actions.rbegin();
  SAT_ASSERT(it->action_kind == action_t::action_kind_t::halt_conflict);
  // We make an explicit copy of that conflict clause
  clause_t c = cnf[it->get_clause()];
  it++;

  // now go backwards until the decision, resolving things against it
  // std::cout << "STARTING " << c << std::endl;
  for (;
       // this is the IUP THING!
       count_level_literals(c) > 1; it++) {
    SAT_ASSERT(it->action_kind == action_t::action_kind_t::unit_prop);
    SAT_ASSERT(count_level_literals(c) >= 1);
    clause_t d = cnf[it->get_clause()];
    if (literal_t r = resolve_candidate(c, d, 0)) {
      // std::cout << "Resolving " << c << " against " << d << " over " << r;
      c = resolve(c, d, r);
      // std::cout << " to get " << c << std::endl;
    }
  }
  SAT_ASSERT(count_level_literals(c) >= 1);
  // std::cout << "DONE" << std::endl;

  // std::cout << "Counter: " << count_level_literals(C) << std::endl;
  // std::cout << "Learned: " << C << std::endl;
  // assert(counter == 1);
  std::sort(std::begin(c), std::end(c));
  return c;
}
#endif

clause_t stamp_resolution(const cnf_t &cnf, const trail_t &actions,
                          lit_bitset_t &stamped) {
#ifdef SAT_DEBUG_MODE
  auto count_level_literals = [&actions](const clause_t &c) {
    return std::count_if(std::begin(c), std::end(c), [&actions](literal_t l) {
      return actions.level(l) == actions.level();
    });
  };
#endif

  stamped.clear();

  std::vector<literal_t> C;
  const size_t D = actions.level();
  auto it = actions.rbegin();

  SAT_ASSERT(it->action_kind == action_t::action_kind_t::halt_conflict);
  const clause_t &c = cnf[it->get_clause()];
  it++;

  // This is the amount of things we know we'll be resolving against.
  size_t counter = 0;
  for (literal_t l : c) {
    if (actions.level(l) == actions.level()) counter++;
  }

  for (literal_t l : c) {
    stamped.set(neg(l));
    if (actions.level(neg(l)) < D) {
      C.push_back(l);
    }
  }

  for (; counter > 1; it++) {
    SAT_ASSERT(it->is_unit_prop());
    literal_t L = it->get_literal();

    // We don't expect to be able to resolve against this.
    if (!stamped.get(L)) {
      continue;
    }

    // L *is* stamped, so we *can* resolve against it!
    counter--;  // track the number of resolutions we're doing.

    const clause_t &d = cnf[it->get_clause()];

    for (literal_t a : d) {
      if (a == L) continue;
      // We care about future resolutions, so we negate a
      if (!stamped.get(neg(a))) {
        stamped.set(neg(a));
        if (actions.level(neg(a)) < D) {
          C.push_back(a);
        } else {
          counter++;
        }
      }
    }
  }

  while (!stamped.get(it->get_literal())) it++;

  SAT_ASSERT(it->has_literal());
  C.push_back(neg(it->get_literal()));

  SAT_ASSERT(count_level_literals(C) == 1);
  SAT_ASSERT(counter == 1);

  // std::cout << "Counter: " << counter << std::endl;
  // std::cout << "Learned: " << C << std::endl;
  std::sort(std::begin(C), std::end(C));

  return clause_t{C};
}

INLINESTATE clause_t learn_clause(const cnf_t &cnf, const trail_t &actions,
                                  lit_bitset_t &stamped) {
  SAT_ASSERT(actions.rbegin()->action_kind ==
             action_t::action_kind_t::halt_conflict);
  return stamp_resolution(cnf, actions, stamped);
  // std::cout << "About to learn clause from: " << *this << std::endl;
}
