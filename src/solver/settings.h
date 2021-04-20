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
const char* trace_cdcl_clause();

extern bool debug_max;

extern bool print_certificate;

extern bool naive_vsids;

int parse(int argc, char* argv[]);
void print_help();
}  // namespace settings