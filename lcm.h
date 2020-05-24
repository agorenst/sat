#include "cnf.h"
#include "trail.h"

void learned_clause_minimization(const cnf_t& cnf, clause_t& c, const trail_t& actions);
