#pragma once
#include <forward_list>
#include <map>
#include <vector>

#include "cnf.h"
#include "debug.h"
#include "trail.h"
#include "unit_queue.h"

// This is ONLY for binary+ clauses, not unary or empty.
// The main entry points:
//  - watch_clause(cid): add a new clause to be watched
//  - literal_falsed(l): inform the machine that a new literal has been marked
//  false.
//                       This will add more things to the unit-prop queue, or
//                       report a conflict.

struct trace_t;
struct solver_t;

struct watched_literals_t {
  cnf_t &cnf;
  trail_t &trail;
  unit_queue_t &units;
  literal_map_t<std::vector<std::pair<clause_id, literal_t>>>
      literals_to_watcher;

  void install(solver_t &);
  watched_literals_t(cnf_t &cnf, trail_t &t, unit_queue_t &q);
  void watch_clause(clause_id cid);
  void literal_falsed(literal_t l);

  bool clause_watched(clause_id cid);

  void unwatch_clause(literal_t l, clause_id cid);
  void remove_clause(clause_id cid);

  literal_t find_first_watcher(const clause_t &c);
  auto find_watcher(clause_t &c, literal_t o = 0);
  void print_watch_state();
  bool validate_state(clause_id skip_id = nullptr);
};

std::ostream &operator<<(std::ostream &o, const watched_literals_t &w);
