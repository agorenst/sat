#pragma once

#include <cassert>

//#define SAT_DEBUG_MODE
#ifdef SAT_DEBUG_MODE
#define SAT_ASSERT(expr) assert(expr)
#else
#define SAT_ASSERT(expr)
#endif
