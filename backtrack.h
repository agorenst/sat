#pragma once
#include "cnf.h"
#include "trail.h"

void backtrack(const clause_t& c, trail_t& actions);
