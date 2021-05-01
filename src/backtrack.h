#pragma once
#include "clause.h"
#include "trail.h"

action_t *nonchronological_backtrack(const clause_t &c, trail_t &actions);
