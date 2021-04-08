#include "settings.h"

#include <cstring>
#include <string>
#include <vector>
namespace settings {
bool print_parse = false;
bool print_canon = false;
bool preprocess = true;
bool print_preprocess = false;
bool output_dimacs = true;  // this should be an enum of "output format".

bool time_preprocess = false;

bool trace_applications = false;
bool trace_clause_learning = false;

bool print_certificate = false;

bool debug_max = false;  // this should ultimately be an integer or osmething.
struct flag_t {
  std::string name;
  std::string description;
  bool& value;
};
std::vector<flag_t> options{
    {"print-parse", "Output CNF just after parsing (mainly for debugging)",
     print_parse},
    {"print-canon", "Output CNF just after canon (mainly for debugging)",
     print_canon},
    {"print-preprocess",
     "Output CNF after preprocessing (mainly for debugging)", print_preprocess},
    {"preprocessor-", "Don't run the preprocessor", preprocess},
    {"time-preprocessor", "Log the timing of the preprocessor",
     time_preprocess},
    {"output-dimacs-", "Output literals in raw (rather than DIMACS) format",
     output_dimacs},
    {"trace-clause-learning", "Logging level of clause learning steps",
     trace_clause_learning},
    {"debug-max", "Run a lot more asserts", debug_max},
    {"print-certificate", "Output our solution (if sat) in a trivial format",
     print_certificate},
};
int parse(int argc, char* argv[]) {
  for (int i = 1; i < argc; i++) {
    const char* arg = argv[i];
    auto n = strlen(arg);
    if (n < 2) {
      return i;  // error
    }
    if (arg[0] != '-' || arg[1] != '-') {
      return i;
    }
    arg++;
    arg++;

    bool found = false;
    for (auto& p : options) {
      if (!strcmp(p.name.c_str(), arg)) {
        // This is a flag, flip it from default
        p.value = !p.value;
        found = true;
        break;
      }
    }
    if (!found) {
      return i;
    }
  }
  return 0;  // success
}
void print_help() { printf("Usage information TBD\n"); }
}  // namespace settings