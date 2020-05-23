#include <cstdio>
#include <iostream>
#include <cstdint>
#include <vector>
#include <algorithm>
#include <cassert>
#include <map>
#include <queue>
#include <list>
#include <chrono>

#include <cstring>

#include <getopt.h>

#include "cnf.h"

#include "debug.h"
#include "watched_literals.h"
#include "trace.h"

// TODO: Change clause_id to an iterator?
// TODO: Exercise 257 to get shorter learned clauses

// The understanding of this project is that it will evolve
// frequently. We'll first start with the basic data structures,
// and then refine things as they progress.


variable_t max_variable = 0;

bool watched_literals_on = false; // this builds off the "queue" model.

// The perspective I want to take is not one of deriving an assignment,
// but a trace exploring the recursive, DFS space of assignments.
// We don't forget anything -- I even want to record backtracking.
// So let's see what this looks like.
struct trace_t;
std::ostream& operator<<(std::ostream& o, const trace_t t);

clause_t trace_t::learn_clause() {
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
    auto it = actions.rbegin();
    SAT_ASSERT(it->action_kind == action_t::action_kind_t::halt_conflict);
    // We make an explicit copy of that conflict clause
    clause_t c = cnf[it->conflict_clause_id];

    it++;

    // now go backwards until the decision, resolving things against it
    auto restart_it = it;
    for (; it != std::rend(actions) && it->action_kind != action_t::action_kind_t::decision; it++) {
      SAT_ASSERT(it->action_kind == action_t::action_kind_t::unit_prop);
      clause_t d = cnf[it->unit_prop.reason];
      if (literal_t r = resolve_candidate(c, d)) {
        c = resolve(c, d, r);
      }
    }

    SAT_ASSERT(verify_resolution_expected(c));

    return c;
  }
  assert(0);
  return {};
}




// these are our global settings
void process_flags(int argc, char* argv[]) {
  for (;;) {
    static struct option long_options[] = {
                                           {"backtracking", required_argument, 0, 'b'},
                                           {"learning", required_argument, 0, 'l'},
                                           {"unitprop", required_argument, 0, 'u'},
                                           {0,0,0,0}
    };
    int option_index = 0;
    int c = getopt_long(argc, argv, "b:l:u:", long_options, &option_index);

    if (c == -1) {
      break;
    }

    switch (c) {
    case 'b':
      if (!strcmp(optarg, "simplest") || !strcmp(optarg, "s")) {
        backtrack_mode = backtrack_mode_t::simplest;
      } else if (!strcmp(optarg, "nonchron") || !strcmp(optarg, "n")) {
        backtrack_mode = backtrack_mode_t::nonchron;
      }
      break;
    case 'l':
      if (!strcmp(optarg, "simplest") || !strcmp(optarg, "s")) {
        learn_mode = learn_mode_t::simplest;
      } else if (!strcmp(optarg, "resolution") || !strcmp(optarg, "r")) {
        learn_mode = learn_mode_t::explicit_resolution;
      }
      break;
    case 'u':
      if (!strcmp(optarg, "simplest") || !strcmp(optarg, "s")) {
        unit_prop_mode = unit_prop_mode_t::simplest;
      } else if (!strcmp(optarg, "queue") || !strcmp(optarg, "q")) {
        unit_prop_mode = unit_prop_mode_t::queue;
      } else if (!strcmp(optarg, "watched") || !strcmp(optarg, "w")) {
        unit_prop_mode = unit_prop_mode_t::watched;
      }
      break;
    }
  }
  //std::cerr << "Settings are: " << static_cast<int>(learn_mode) << " " << static_cast<int>(backtrack_mode) << " " << static_cast<int>(unit_prop_mode) << std::endl;
}
// The real goal here is to find conflicts as fast as possible.
int main(int argc, char* argv[]) {

  // Instantiate our CNF object
  cnf_t cnf = load_cnf(std::cin);
  SAT_ASSERT(cnf.size() > 0); // make sure parsing worked.

  // Do the naive unit_prop
  while (literal_t u = find_unit(cnf)) {
    commit_literal(cnf, u);
  }

  // TODO: fold this into a more general case, if possible.
  if (immediately_unsat(cnf)) {
    std::cout << "UNSATISFIABLE" << std::endl;
    return 0;
  }
  if (immediately_sat(cnf)) {
    std::cout << "SATISFIABLE" << std::endl;
    return 0;
  }

  SAT_ASSERT(!find_unit(cnf));

  process_flags(argc, argv);

  trace_t trace(cnf);

  enum class solver_state_t {
                             quiescent,
                             check_units,
                             conflict,
                             sat,
                             unsat
  };
  solver_state_t state = solver_state_t::quiescent;

  for (;;) {
    //std::cout << "State: " << static_cast<int>(state) << std::endl;
    //std::cout << "Trace: " << trace << std::endl;
    switch (state) {
    case solver_state_t::quiescent: {

      literal_t l = trace.decide_literal();
      if (l != 0) {
        trace.apply_decision(l);
        trace.register_false_literal(l);
      }

      if (trace.final_state()) {
        state = solver_state_t::sat;
      } else {
        state = solver_state_t::check_units;
      }
      break;
    }

    case solver_state_t::check_units:

      for (;;) {
        auto [l, cid] = trace.prop_unit();
        //std::cout << "Found unit prop: " << l << " " << cid << std::endl;
        if (l == 0) break;
        trace.apply_unit(l, cid);
        trace.register_false_literal(l);
        if (trace.halted()) break;
      }

      if (trace.halted()) {
        state = solver_state_t::conflict;
      }
      else {
        state = solver_state_t::quiescent;
      }

      break;

    case solver_state_t::conflict: {

      const clause_t c = trace.learn_clause();

      // early out in the unsat case.
      if (c.size() == 0) {
        state = solver_state_t::unsat;
        break;
      }

      //std::cout << "Learned clause: " << c << std::endl;
      trace.backtrack(c);
      trace.add_clause(c);
      if (unit_prop_mode == unit_prop_mode_t::watched) SAT_ASSERT(trace.watch.validate_state());

      if (unit_prop_mode == unit_prop_mode_t::queue ||
          unit_prop_mode == unit_prop_mode_t::watched) {
        if (backtrack_mode == backtrack_mode_t::nonchron) {
          assert(trace.count_unassigned_literals(c) == 1);
        }
        if (trace.count_unassigned_literals(c) == 1) {
          literal_t l = trace.find_unassigned_literal(c);
          trace.push_unit_queue(l, cnf.size()-1);
        }
      }

      if (trace.final_state()) {
        state = solver_state_t::unsat;
      } else {
        state = solver_state_t::check_units;
      }

      break;
    }

    case solver_state_t::sat:
      std::cout << "SATISFIABLE" << std::endl;
      return 0; // quit entirely
    case solver_state_t::unsat:
      std::cout << "UNSATISFIABLE" << std::endl;
      return 0; // quit entirely
    }
  }
}
