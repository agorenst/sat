#pragma once
#include <cassert>
#include <chrono>
#include <cstdio>
#include "clause.h"
#include "cnf.h"
#include "settings.h"

struct timer {
  enum class action {
    literal_falsed,
    last_action,
  };
  static const constexpr int to_int(action a) { return static_cast<size_t>(a); }
  static std::chrono::duration<double> cumulative_time[];
  static size_t counter[];
  static void initialize();
  action a;
  std::chrono::time_point<std::chrono::steady_clock> start;
  timer(action a) : a(a), start(std::chrono::steady_clock::now()) {}
  ~timer();
};

enum class solver_action {
  preprocessor_start,
  preprocessor_end,
  apply_unit,
  skip_unit,
  apply_decision,
  restart,
  determined_conflict_clause,  // 6
  learned_clause,              // 7
  hash_false_positive,
  conflict,
  cdcl_interior_resolution,

  vivification_case_1_pre,
  vivification_case_1_post,
  vivification_case_2a_pre,
  vivification_case_2a_post,
  vivification_case_2b_pre,
  vivification_case_2b_post,
};

template <typename T>
inline void log_action_element(const T& t) {
  assert(0);
}
template <>
inline void log_action_element(
    const std::chrono::time_point<std::chrono::steady_clock>& c) {
  printf("%ldms ", std::chrono::duration_cast<std::chrono::milliseconds>(
                       c.time_since_epoch())
                       .count());
}
template <>
inline void log_action_element(const solver_action& a) {
  printf("%d ", static_cast<int>(a));
}
template <>
inline void log_action_element(const literal_t& l) {
  printf("%d ", l);
}
template <>
inline void log_action_element(const bool& b) {
  printf("%s ", b ? "true" : "false");
}
template <>
inline void log_action_element(const size_t& l) {
  printf("%zu ", l);
}
template <>
inline void log_action_element(const clause_id& cid) {
  printf("%p ", cid);
}
template <>
inline void log_action_element(const char& c) {
  printf("%c ", c);
}
template <>
inline void log_action_element(const clause_t& c) {
  printf("{ ");
  for (auto l : c) {
    printf("%d ", lit_to_dimacs(l));
  }
  printf("}");
}
template <>
inline void log_action_element(const std::vector<literal_t>& c) {
  printf("{ ");
  for (auto l : c) {
    printf("%d ", lit_to_dimacs(l));
  }
  printf("}");
}

inline void log_solver_action(void) { printf("\n"); }

template <typename T, typename... Rest>
inline void log_solver_action(const T& a, const Rest&... rest) {
  log_action_element(a);
  log_solver_action(rest...);
}
template <typename... Rest>
inline void cond_log(const bool f, const Rest&... rest) {
  if (f) {
    log_solver_action(rest...);
  }
}
