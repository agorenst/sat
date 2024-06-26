#include "settings.h"

#include <algorithm>
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
bool trace_hash_collisions = false;
bool trace_decisions = false;
bool trace_conflicts = false;
bool trace_cdcl = false;
bool trace_vivification = false;

bool only_preprocess = false;
bool no_viv_preprocess = false;

bool print_certificate = false;

bool naive_vsids = true;

bool learned_clause_minimization = true;
bool lbd_cleaning = true;
bool ema_restart = true;
bool backtrack_subsumption = true;
bool on_the_fly_subsumption = true;
bool trace_otf_subsumption = false;

bool only_positive_choices = false;
bool preprocess_bve = false;
bool preprocess_bce = false;

bool debug_max = false;  // this should ultimately be an integer or osmething.

struct flag_t {
  std::string name;
  std::string description;
  bool& value;
  std::string param = "";
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

    {"trace-hash-collisions", "Trace hash collisions", trace_hash_collisions},
    {"trace-decisions", "Trace decisions", trace_decisions},
    {"trace-applications", "Trace applications", trace_applications},
    {"trace-conflicts", "Trace conflicts", trace_conflicts},

    {"naive-vsids", "Use the original vsids algorithm", naive_vsids},
    {"trace-cdcl", "Emit the CDCL resolution for this clause", trace_cdcl},
    {"trace-vivification", "Trace what our viv algorithms do",
     trace_vivification},
    {"no-viv-preprocess", "Turn off vivification in the preprocessor",
     no_viv_preprocess},

    {"learned-clause-minimization-", "Turn off LCM",
     learned_clause_minimization},
    {"lbd-cleaning-", "Turn off LBD", lbd_cleaning},
    {"ema-restart-", "Turn off EMA restart strategy", ema_restart},
    {"only-positive-choices",
     "Use a naive literal-choosing strategy that can only return positive "
     "literals",
     only_positive_choices},
    {"backtrack-subsumption-", "Turn off backtrack subsumption",
     backtrack_subsumption},

    {"preprocess-bve-", "Turn off the BVE optimization in the preprocessor",
     preprocess_bve},
    {"preprocess-bce-", "Turn off the BCE optimization in the preprocessor",
     preprocess_bce},
    {"only-preprocess", "Quit after preprocessing", only_preprocess},
    {"on-the-fly-subsumption-", "Turn off the optimization",
     on_the_fly_subsumption},
    {"trace-otf-subsumption", "Trace on-the-fly subsumption",
     trace_otf_subsumption},
};
const char* trace_cdcl_clause() {
  auto flag =
      std::find_if(std::begin(options), std::end(options),
                   [&](const flag_t& o) { return o.name == "trace-cdcl"; });
  return flag->param.c_str();
}
bool parse_arg(const char* arg) {
#if 0
// don't handle this case yet (const char!)
  char* argend = &arg[strlen(arg)];
  auto param_ptr = std::find(arg, argend, '=');
  std::string param_value = "";
  if (param_ptr != argend) {
    *param_ptr = '\0';  // remove the '=';
    std::copy(param_ptr + 1, argend, std::back_inserter(param_value));
  }
#endif

  bool found = false;
  for (auto& p : options) {
    if (!strcmp(p.name.c_str(), arg)) {
      // This is a flag, flip it from default
      p.value = !p.value;
      found = true;
      break;
    }
  }
  return found;
}
int parse(int argc, char* argv[]) {
  for (int i = 1; i < argc; i++) {
    char* arg = argv[i];
    auto n = strlen(arg);
    if (n < 2) {
      return i;  // error
    }
    if (arg[0] != '-' || arg[1] != '-') {
      return i;
    }
    arg++;
    arg++;
    bool found = parse_arg(arg);
    if (!found) {
      return i;
    }
  }
  return 0;  // success
}
bool parse_strings(const std::vector<std::string>& args) {
  for (auto& arg_str : args) {
    const char* arg = arg_str.c_str();
    auto n = strlen(arg);
    if (n < 2) {
      return false;  // error
    }
    if (arg[0] != '-' || arg[1] != '-') {
      return false;
    }
    arg++;
    arg++;
    bool found = parse_arg(arg);
    if (!found) {
      return false;
    }
  }
  return true;  // success
}
void print_help() { printf("Usage information TBD\n"); }
}  // namespace settings