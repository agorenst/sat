#pragma once

#include <cassert>

#ifdef SAT_DEBUG_MODE
#define SAT_ASSERT(expr) assert(expr)
#else
#define SAT_ASSERT(expr)
#endif
