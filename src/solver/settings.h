namespace settings {
extern bool print_parse;
extern bool print_canon;
extern bool preprocess;

extern bool time_preprocess;

extern bool trace_applications;
extern bool trace_clause_learning;

int parse(int argc, char* argv[]);
void print_help();
}  // namespace settings