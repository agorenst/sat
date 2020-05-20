#define DEBUG_MODE
#ifdef DEBUG_MODE
#define SAT_ASSERT(expr) assert(expr)
#else
#define SAT_ASSERT(expr)
#endif
