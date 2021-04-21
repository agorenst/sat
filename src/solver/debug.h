#pragma once
#include "settings.h"

#include <cassert>

#define INLINESTATE
//#define INLINESTATE __attribute__((noinline))

#ifdef SAT_DEBUG_MODE
#define SAT_ASSERT(expr) assert(expr)
#else
#define SAT_ASSERT(expr)
#endif

#define COND_ASSERT(b, x) \
  {                       \
    if (b) {              \
      assert(x);          \
    }                     \
  }
#define MAX_ASSERT(x) COND_ASSERT(settings::debug_max, (x))