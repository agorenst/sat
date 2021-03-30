#ifndef _MSC_VER
#include <getopt.h>
#endif

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

// The perspective I want to take is not one of deriving an assignment,
// but a trace exploring the recursive, DFS space of assignments.
// We don't forget anything -- I even want to record backtracking.
// So let's see what this looks like.

// What is the mode the driver is in?
// For now just "solver", but could be, e.g., "verifier"
enum class sat_mode_t { solver, verifier };

sat_mode_t mode = sat_mode_t::solver;
// These may evolved into more levels.
bool verbose = false;
bool timing = false;
bool solution = false;
bool emit_help = false;  // should this be a mode? Whatever.
bool debug = false;

#ifndef _MSC_VER

const static struct option long_options[] = {
    // Usage
    {"help", no_argument, nullptr, 'h'},

    // Debugging etc.
    {"verbose", no_argument, nullptr, 'v'},
    {"timing", no_argument, nullptr, 't'},
    {"solution", no_argument, nullptr, 's'},
    {"debug", no_argument, nullptr, 'd'},

    // Modes
    {"mode", required_argument, nullptr, 'm'},

    // Options (depend on the mode!)
    {"backtracking", required_argument, nullptr, 'b'},
    {"learning", required_argument, nullptr, 'l'},
    {"unitprop", required_argument, nullptr, 'u'},
    {"preprocessor", required_argument, nullptr, 'p'},

    {nullptr, 0, nullptr, 0}};

// Helper to match option arguments against known strings.
bool optarg_match(const char *o, const char *s1, const char *s2) {
  return strcmp(o, s1) == 0 || strcmp(o, s2) == 0;
}
void process_flags(int argc, char *argv[]) {
  for (;;) {
    int option_index = 0;
    int c =
        getopt_long(argc, argv, "b:l:u:p:m:vth", long_options, &option_index);

    if (c == -1) {
      break;
    }

    switch (c) {
      case 'h':
        emit_help = true;
        break;
      case 'm':
        if (optarg_match(optarg, "solver", "s")) {
          mode = sat_mode_t::solver;
        }
        break;
      case 'd':
        debug = true;
        break;
      case 'v':
        verbose = true;
        break;
      case 't':
        timing = true;
        break;
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
}

void pretty_print_option(const option *o) {
  printf("\t--%s, -%c\n", o->name, o->val);
}
void pretty_print_options() {
  int last_option_index = (sizeof(long_options) / sizeof(*long_options)) - 1;
  for (int i = 0; i < last_option_index; i++) {
    pretty_print_option(&(long_options[i]));
  }
}
#endif

// Main entry point, this is a driver that allows for multiple modes.
int main(int argc, char *argv[]) {
  // Parse flags to set global state.
#ifndef _MSC_VER
  process_flags(argc, argv);

  // Help case
  if (emit_help) {
    pretty_print_options();
    return 0;
  }
#endif

  // Instantiate our CNF object
  cnf_t cnf = load_cnf(std::cin);
  SAT_ASSERT(cnf.live_clause_count() > 0);  // make sure parsing worked.

  if (debug) {
    std::cout << cnf << std::endl;
  }

  canon_cnf(cnf);
  if (debug) {
    std::cout << cnf << std::endl;
  }

  // Preprocess
  auto start = std::chrono::steady_clock::now();
  preprocess(cnf);
  auto end = std::chrono::steady_clock::now();
  std::chrono::duration<double> elapsed_seconds = end - start;
  if (timing) {
    std::cout << "preprocessing elapsed time: " << elapsed_seconds.count()
              << "s\n";
  }

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

  start = std::chrono::steady_clock::now();
  solver_t solver(cnf);

  // Solve!
  bool result = solver.solve();
  solver.report_metrics();
  if (result) {
    std::cout << "SATISFIABLE" << std::endl;
  } else {
    std::cout << "UNSATISFIABLE" << std::endl;
  }
  return 0;
}
