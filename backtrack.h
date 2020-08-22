#pragma once
#include "cnf.h"
#include "trail.h"

action_t *backtrack(const clause_t &c, trail_t &actions);
