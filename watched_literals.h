#pragma once
#include <map>
#include <vector>

#include "debug.h"
#include "cnf.h"
#include "literal_incidence_map.h"
#include "clause_key_map.h"

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
struct watched_literals_t {
  cnf_t& cnf;
  trace_t& trace;
  //std::map<clause_id, watcher_t> watched_literals;
  literal_incidence_map_t literals_to_watcher;
  clause_map_t<watcher_t> watched_literals;
  typedef std::vector<clause_id> clause_list_t;
  //std::map<literal_t, clause_list_t> literals_to_watcher;

  std::vector<literal_t> units;


  watched_literals_t(trace_t& t);
  void watch_clause(clause_id cid);
  void literal_falsed(literal_t l);

  bool clause_watched(clause_id cid);

  void remove_clause(clause_id cid);

  literal_t find_next_watcher(const clause_t& c, const watcher_t& w);
  void literal_falsed(literal_t l, clause_id cid);
  void watcher_swap(clause_id cid, watcher_t& w, literal_t o, literal_t n);
  void print_watch_state() const;
  bool validate_state();
};

std::ostream& operator<<(std::ostream& o, const watched_literals_t& w);
