#pragma once

#include <cassert>

//#define INLINESTATE
#define INLINESTATE __attribute__((noinline))

#ifdef SAT_DEBUG_MODE
#define SAT_ASSERT(expr) assert(expr)
#else
#define SAT_ASSERT(expr)
#endif
