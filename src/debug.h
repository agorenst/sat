#pragma once
#include "settings.h"

#include <cassert>

#define INLINESTATE
//#define INLINESTATE __attribute__((noinline))

#define COND_ASSERT(b, x) \
  {                       \
    if (b) {              \
      assert(x);          \
    }                     \
  }
#ifdef REL
#define MAX_ASSERT(x)
#else
#define MAX_ASSERT(x) COND_ASSERT(settings::debug_max, (x))
#endif
#define SAT_ASSERT(expr) MAX_ASSERT(expr)
#define SEARCH_ASSERT(expr) MAX_ASSERT(expr)