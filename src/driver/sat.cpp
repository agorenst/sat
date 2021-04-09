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

#include "cnf.h"
#include "preprocess.h"
#include "settings.h"
#include "solver.h"

// sat.cpp is the main driver module.
// It presents an executable interface to the solver library.
// This takes in input dictating how to interface with that library (e.g., what
// solver mode to use, and what algorithms to use there, what level of
// logging is desired, etc) and validates it, and assuming it works does what is
// asked.

// A lot of this, therefore is "just" parsing the command-line arguments, and
// using that to modify some of the starting conditions in the exeuction.

// Main entry point, this is a driver that allows for multiple modes.
int main(int argc, char* argv[]) {
  // Parse flags to set global state.
  int error_flag = settings::parse(argc, argv);
  if (error_flag) {
    printf("Error with flag #%d: \"%s\"\n", error_flag, argv[error_flag]);
    settings::print_help();
    return 1;
  }

  // Instantiate our CNF object
  std::string input_file;
  char c;
  while ((c = fgetc(stdin)) != EOF) {
    input_file.push_back(c);
  }
  cnf_t cnf;
  bool parse_successful =
      cnf::io::load_cnf(input_file.c_str(), input_file.size(), cnf);

  SAT_ASSERT(cnf.live_clause_count() > 0);  // make sure parsing worked.

  if (settings::print_parse) {
    std::cout << cnf << std::endl;
  }

  // Canonicalize it (remove redundancies, handle true triviliaty)
  cnf::transform::canon(cnf);
  if (settings::print_canon) {
    std::cout << cnf << std::endl;
  }

  // Preprocess, if requested
  if (settings::preprocess) {
    preprocess(cnf);
    if (settings::print_preprocess) {
      std::cout << cnf << std::endl;
    }
  }

  // TODO(aaron): fold this into a more general case, if possible.
  if (cnf::search::immediately_unsat(cnf)) {
    printf("UNSATISFIABLE\n");
    return 0;
  }
  if (cnf::search::immediately_sat(cnf)) {
    printf("SATISFIABLE\n");
    return 0;
  }

  // Do the actual solving
  solver_t solver(cnf);
  bool result = solver.solve();

  if (result) {
    if (settings::print_certificate) {
      solver.trail.print_certificate();
    }
    printf("SATISFIABLE\n");
  } else {
    printf("UNSATISFIABLE\n");
  }
  return 0;
}
