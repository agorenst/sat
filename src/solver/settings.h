namespace settings {
extern bool print_parse;
extern bool print_canon;
extern bool preprocess;
extern bool print_preprocess;
extern bool output_dimacs;

extern bool time_preprocess;

extern bool trace_decisions;
extern bool trace_applications;

extern bool trace_clause_learning;
extern bool trace_hash_collisions;
extern bool trace_conflicts;
extern bool trace_cdcl;
extern bool trace_vivification;
const char* trace_cdcl_clause();

extern bool debug_max;

extern bool print_certificate;

extern bool naive_vsids;

extern bool only_preprocess;
extern bool no_viv_preprocess;

extern bool learned_clause_minimization;
extern bool lbd_cleaning;
extern bool ema_restart;
extern bool backtrack_subsumption;

extern bool only_positive_choices;

int parse(int argc, char* argv[]);
void print_help();
}  // namespace settings