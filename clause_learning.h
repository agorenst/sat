#pragma once
#include "cnf.h"
#include "trail.h"

enum class learn_mode_t {
                         simplest,
                         explicit_resolution
};
extern learn_mode_t learn_mode;
clause_t learn_clause(const cnf_t& cnf, const trail_t& actions);
