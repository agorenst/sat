#pragma once
#include <map>
#include <vector>

#include "debug.h"
#include "cnf.h"

// This is ONLY for binary+ clauses, not unary or empty.
// The main entry points:
//  - watch_clause(cid): add a new clause to be watched
//  - literal_falsed(l): inform the machine that a new literal has been marked false.
//                       This will add more things to the unit-prop queue, or report a conflict.
struct trace_t;

struct watcher_t {
  literal_t l1 = 0;
  literal_t l2 = 0;
};
std::ostream& operator<<(std::ostream& o, const watcher_t& w);
struct watched_literals_t {
  std::map<clause_id, watcher_t> watched_literals;
  typedef std::vector<clause_id> clause_list_t;
  std::map<literal_t, clause_list_t> literals_to_watcher;

  std::vector<literal_t> units;

  cnf_t& cnf;
  trace_t& trace;

  watched_literals_t(trace_t& t);
  void watch_clause(clause_id cid);
  void literal_falsed(literal_t l);

  bool clause_watched(clause_id cid);

  bool watch_contains(const watcher_t& w, literal_t l);
  literal_t find_next_watcher(const clause_t& c, const watcher_t& w);
  void literal_falsed(literal_t l, clause_id cid);
  void watcher_literal_swap(watcher_t& w, literal_t o, literal_t n);
  void watcher_list_remove_clause(clause_list_t& clause_list, clause_id cid);
  void watcher_swap(clause_id cid, watcher_t& w, literal_t o, literal_t n);
  void print_watch_state() const;
  bool validate_state();
};

std::ostream& operator<<(std::ostream& o, const watched_literals_t& w);
