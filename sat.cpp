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
#include "preprocess.h"

#include "clause_learning.h"
#include "backtrack.h"
#include "bce.h"
#include "lcm.h"

#include "lbm.h"

// TODO: Change clause_id to an iterator?
// TODO: Exercise 257 to get shorter learned clauses

// The understanding of this project is that it will evolve
// frequently. We'll first start with the basic data structures,
// and then refine things as they progress.


namespace counters {
  size_t restarts = 0;
  size_t conflicts = 0;
  size_t decisions = 0;
  size_t propagations = 0;
  // learned clause size histogram
  std::map<int, int> lcsh;
  void print() {
    std::cout << "Restarts: " << restarts << std::endl;
    std::cout << "Conflicts: " << conflicts << std::endl;
    std::cout << "Decisions: " << decisions << std::endl;
    std::cout << "Propagations: " << propagations << std::endl;
    int max_index = 0;
    int max_value = 0;
    int total_count = 0;
    int total_length = 0;
    for (auto&& [l, c] : lcsh) {
      max_index = std::max(l, max_index);
      max_value = std::max(c, max_value);
      total_count += c;
      total_length += c*l;
    }
    for (int i = 0; i < 10; i++) {
      for (int j = 0; j < max_index; j++) {
        int count = lcsh[j];
        float normalized = float(100*count) / float(total_count);
        if (normalized >= 10-i) {
          std::cout << "#  ";
        }
        else {
          std::cout << "   ";
        }
      }
      std::cout << std::endl;
    }
    for (int j = 0; j < max_index; j++) {
      std::cout << j;
      if (j < 10) std::cout << " ";
      if (j < 100) std::cout << " ";
    }
    std::cout << std::endl;
    std::cout << "Average learned clause length: " << float(total_length) << " " <<  float(conflicts) << std::endl;
    std::cout << "Average learned clause length: " << float(total_length)/float(conflicts) << std::endl;
  }
}

// The perspective I want to take is not one of deriving an assignment,
// but a trace exploring the recursive, DFS space of assignments.
// We don't forget anything -- I even want to record backtracking.
// So let's see what this looks like.

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
  SAT_ASSERT(cnf.live_clause_count() > 0); // make sure parsing worked.

  preprocess(cnf);

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

  lbm_t lbm(cnf);

  for (;;) {
    //std::cout << "State: " << static_cast<int>(state) << std::endl;
    //std::cout << "Trace: " << trace << std::endl;
    switch (state) {
    case solver_state_t::quiescent: {

      if (false) {
        auto blocked_clauses = BCE(cnf);
        if (blocked_clauses.size() > 0) {
          std::cerr << "[BCE] Blocked clauses found after learning: " << blocked_clauses.size() << std::endl;
          for (clause_id cid : blocked_clauses) {
            trace.watch.remove_clause(cid);
          }
          cnf.remove_clauses(blocked_clauses);
        }
      }

      if (lbm.should_clean(cnf)) {
        auto cids_to_remove = lbm.clean(cnf);
        //std::cout << "Cnf size = " <<  cnf.live_clause_count() << std::endl;
        for (auto cid : cnf) {
          //std::cout << cnf[cid] << " " << lbm.lbm[cid] << std::endl;
        }
        //std::cout << "====================Clauses to remove: " << cids_to_remove.size() << std::endl;
        for (clause_id cid : cids_to_remove) {
          trace.watch.remove_clause(cid);
          //std::cout << cnf[cid] << " " << lbm.lbm[cid] << std::endl;
        }
        //std::cout << "====================" << std::endl;
        cnf.remove_clauses(cids_to_remove);
        //std::cout << "New size = " << cnf.live_clause_count() << std::endl;
      }

      counters::decisions++;
      if (trace.actions.level() == 0) {
        counters::restarts++;
      }

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
        counters::propagations++;
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

      counters::conflicts++;

      clause_t c = learn_clause(cnf, trace.actions);

      //auto orig_c = c.size();
      learned_clause_minimization(cnf, c, trace.actions);
      //if (orig_c > c.size()) std::cout << "[LCM] size from " << orig_c << " to " << c.size() << std::endl;

      counters::lcsh[c.size()]++;

      // early out in the unsat case.
      if (c.size() == 0) {
        state = solver_state_t::unsat;
        break;
      }


      size_t lbm_value = lbm.compute_value(c, trace.actions);
      backtrack(c, trace.actions);
      trace.clear_unit_queue(); // ???
      trace.vsids.clause_learned(c);

      cnf_t::clause_k key = trace.add_clause(c);
      //lbm.lbm[key] = lbm_value;
      if (unit_prop_mode == unit_prop_mode_t::watched) SAT_ASSERT(trace.watch.validate_state());

      if (unit_prop_mode == unit_prop_mode_t::queue ||
          unit_prop_mode == unit_prop_mode_t::watched) {
        if (backtrack_mode == backtrack_mode_t::nonchron) {
          assert(trace.count_unassigned_literals(c) == 1);
        }
        if (trace.count_unassigned_literals(c) == 1) {
          literal_t l = trace.find_unassigned_literal(c);
          trace.push_unit_queue(l, key);
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
      counters::print();
      std::cout << "SATISFIABLE" << std::endl;
      return 0; // quit entirely
    case solver_state_t::unsat:
      counters::print();
      std::cout << "UNSATISFIABLE" << std::endl;
      return 0; // quit entirely
    }
  }
}
