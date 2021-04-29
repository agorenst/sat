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
#define MAX_ASSERT(x) COND_ASSERT(settings::debug_max, (x))
#define SAT_ASSERT(expr) MAX_ASSERT(expr)