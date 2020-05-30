#include <vector>

#include "cnf.h"
std::vector<clause_id> find_subsumed(cnf_t& cnf, const clause_t& c);
bool subsumes(const clause_t& c, const clause_t& d);
