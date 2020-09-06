#include <getopt.h>

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <list>
#include <map>
#include <queue>
#include <vector>

#include "backtrack.h"
#include "bce.h"
#include "clause_learning.h"
#include "cnf.h"
#include "debug.h"
#include "lbm.h"
#include "lcm.h"
#include "preprocess.h"
#include "subsumption.h"
#include "watched_literals.h"

// Help with control flow and such...
#include "plugins.h"
#include "solver.h"

// Debugging and experiments.
#include "measurements.h"
#include "visualizations.h"
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
  for (auto &&[l, c] : lcsh) {
    max_index = std::max(l, max_index);
    max_value = std::max(c, max_value);
    total_count += c;
    total_length += c * l;
  }
  for (int i = 0; i < 10; i++) {
    for (int j = 0; j < max_index; j++) {
      int count = lcsh[j];
      float normalized = float(100 * count) / float(total_count);
      if (normalized >= 10 - i) {
        std::cout << "#  ";
      } else {
        std::cout << "   ";
      }
    }
    std::cout << std::endl;
  }
  for (int j = 0; j < max_index; j++) {
    std::cout << j;
    if (j < 10) {
      std::cout << " ";
    }
    if (j < 100) {
      std::cout << " ";
    }
  }
  std::cout << std::endl;
  std::cout << "Average learned clause length: " << float(total_length) << " "
            << float(conflicts) << std::endl;
  std::cout << "Average learned clause length: "
            << float(total_length) / float(conflicts) << std::endl;
}
}  // namespace counters

// The perspective I want to take is not one of deriving an assignment,
// but a trace exploring the recursive, DFS space of assignments.
// We don't forget anything -- I even want to record backtracking.
// So let's see what this looks like.

// these are our global settings
bool optarg_match(const char *o, const char *s1, const char *s2) {
  return strcmp(o, s1) == 0 || strcmp(o, s2) == 0;
}
void process_flags(int argc, char *argv[]) {
  for (;;) {
    static struct option long_options[] = {
        {"backtracking", required_argument, nullptr, 'b'},
        {"learning", required_argument, nullptr, 'l'},
        {"unitprop", required_argument, nullptr, 'u'},
        {nullptr, 0, nullptr, 0}};
    int option_index = 0;
    int c = getopt_long(argc, argv, "b:l:u:", long_options, &option_index);

    if (c == -1) {
      break;
    }

    switch (c) {
      case 'b':
        if (optarg_match(optarg, "simplest", "s")) {
          backtrack_mode = backtrack_mode_t::simplest;
        } else if (optarg_match(optarg, "nonchron", "n")) {
          backtrack_mode = backtrack_mode_t::nonchron;
        }
        break;
      case 'l':
        if (optarg_match(optarg, "simplest", "s")) {
          learn_mode = learn_mode_t::simplest;
        } else if (optarg_match(optarg, "resolution", "r")) {
          learn_mode = learn_mode_t::explicit_resolution;
        }
        break;
      case 'u':
        if (optarg_match(optarg, "simplest", "s")) {
          unit_prop_mode = unit_prop_mode_t::simplest;
        } else if (optarg_match(optarg, "queue", "q")) {
          unit_prop_mode = unit_prop_mode_t::queue;
        } else if (optarg_match(optarg, "watched", "w")) {
          unit_prop_mode = unit_prop_mode_t::watched;
        }
        break;
    }
  }
  // std::cerr << "Settings are: " << static_cast<int>(learn_mode) << " " <<
  // static_cast<int>(backtrack_mode) << " " << static_cast<int>(unit_prop_mode)
  // << std::endl;
}

// The real goal here is to find conflicts as fast as possible.
int main(int argc, char *argv[]) {
  // Instantiate our CNF object
  cnf_t cnf = load_cnf(std::cin);
  SAT_ASSERT(cnf.live_clause_count() > 0);  // make sure parsing worked.

  auto start = std::chrono::steady_clock::now();
  preprocess(cnf);
  auto end = std::chrono::steady_clock::now();
  std::chrono::duration<double> elapsed_seconds = end - start;
  std::cout << "preprocessing elapsed time: " << elapsed_seconds.count()
            << "s\n";

  // TODO(aaron): fold this into a more general case, if possible.
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

  start = std::chrono::steady_clock::now();
  solver_t solver(cnf);

  bool result = solver.solve();
  solver.report_metrics();
  if (result) {
    std::cout << "SATISFIABLE" << std::endl;
  } else {
    std::cout << "UNSATISFIABLE" << std::endl;
  }
  return 0;
}
