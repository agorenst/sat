#pragma once
#include "cnf.h"
#include "trail.h"

clause_t learn_clause(const cnf_t& cnf, const trail_t& actions);
