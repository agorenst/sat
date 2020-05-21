#include "trace.h"
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
std::ostream& operator<<(std::ostream& o, const watcher_t& w) {
  return o << "[" << w.l1 << ", " << w.l2 << "]";
}
struct watched_literals_t {
  std::map<clause_id, watcher_t> watched_literals;
  typedef std::vector<clause_id> clause_list_t;
  std::map<literal_t, clause_list_t> literals_to_watcher;

  std::vector<literal_t> units;

  cnf_t& cnf;
  trace_t& trace;

  watched_literals_t(trace_t& t): trace(t), cnf(t.cnf) {}

  // Add in a new clause to be watched
  void watch_clause(clause_id cid) {
    assert(!clause_watched(cid));
    const clause_t& c = cnf[cid];
    assert(c.size() > 1);

    watcher_t w = {0, 0};

    // Find a non-false watcher; one should exist.
    literal_t l1 = find_next_watcher(c, w);
    assert(l1); // shouldn't be 0, at least
    w.l1 = l1;

    literal_t l2 = find_next_watcher(c, w);

    // We may already be a unit. If so, just get our other watcher to be *something*.
    if (l2 == 0) {
      if (c[0] == w.l1) { w.l2 = c[1]; }
      else { w.l2 = c[0]; }
    }
    else {
      w.l2 = l2;
    }

    literals_to_watcher[w.l1].push_back(cid);
    literals_to_watcher[w.l2].push_back(cid);
  }
  void literal_falsed(literal_t l) {
    // Make a copy because we're about to transform this list.
    clause_list_t clauses = literals_to_watcher[-l];
    for (clause_id cid : clauses) {
      literal_falsed(l, cid);
      if (trace.halted()) {
        break;
      }
    }
  }


  bool clause_watched(clause_id cid) { return watched_literals.find(cid) != watched_literals.end(); }

  bool watch_contains(const watcher_t& w, literal_t l) {
    return w.l1 == l || w.l2 == l;
  }
  literal_t find_next_watcher(const clause_t& c, const watcher_t& w) {
    for (literal_t l : c) {
      if (watch_contains(w, l)) continue;
      if (!trace.literal_false(l)) return l;
    }
    return 0;
  }

  void literal_falsed(literal_t l, clause_id cid) {
    const clause_t& c = cnf[cid];
    watcher_t& w = watched_literals[cid];
    assert(contains(c, -l));
    assert(watch_contains(w, -l));
    literal_t n = find_next_watcher(c, w);
    if (n == 0) {
      auto s = trace.count_unassigned_literals(c);
      assert(s < 2);
      if (s == 1) {
        literal_t u = trace.find_unassigned_literal(c);
        trace.push_unit_queue(u, cid);
      }
      else {
        trace.push_conflict(cid);
      }
    }
    else {
      watcher_swap(cid, w, -l, n);
    }
  }

  void watcher_literal_swap(watcher_t& w, literal_t o, literal_t n) {
    assert(watch_contains(w, o));
    assert(!watch_contains(w, n));
    if (w.l1 == o) {
      w.l1 = n;
    } else {
      w.l2 = n;
    }
    assert(!watch_contains(w, o));
    assert(watch_contains(w, n));
  }

  void watcher_list_remove_clause(clause_list_t& clause_list, clause_id cid) {
    auto it = std::remove(std::begin(clause_list), std::end(clause_list), cid);
    assert(it != std::end(clause_list));
    assert(it == std::prev(std::end(clause_list)));
    clause_list.erase(it, std::end(clause_list));
  }

  void watcher_swap(clause_id cid, watcher_t& w, literal_t o, literal_t n) {
    clause_list_t& old_list = literals_to_watcher[o];
    clause_list_t& new_list = literals_to_watcher[n];
    assert(contains(old_list, cid));
    assert(!contains(new_list, cid));
    // Update our lists data:
    watcher_list_remove_clause(old_list, cid);
    new_list.push_back(cid);
    // Update our watcher data:
    watcher_literal_swap(w, o, n);
  }

  void print_watch_state() {
    for (auto&& [cid, w] : watched_literals) {
      std::cout << cnf[cid] << " watched by " << w << std::endl;
    }
    for (auto&& [literal, clause_list] : literals_to_watcher) {
      std::cout << "Literal " << literal << " watching: ";
      for (const auto& cid : clause_list) {
        const clause_t& c = cnf[cid];
        std::cout << c << "; ";
      }
      std::cout << std::endl;
    }
  }

  void validate_state() {
    for (clause_id cid = 0; cid < cnf.size(); cid++) {

      // We are, or aren't, watching this clause, as appropriate.
      const clause_t& c = cnf[cid];
      if (c.size() < 2) {
        SAT_ASSERT(!clause_watched(cid));
        continue;
      }
      SAT_ASSERT(clause_watched(cid));

      // The two literals watching us are in the clause.
      const watcher_t w = watched_literals[cid];
      SAT_ASSERT(contains(c, w.l1));
      SAT_ASSERT(contains(c, w.l2));

      // TODO: Something to enforce about expected effect of backtracking
      // (the relative order of watched literals, etc. etc.)

      // I *think* that we have no other invariants, if the clause is sat.
      // I would think that the watched literals shouldn't be pointed to false,
      // but I'm not sure.
      if (trace.clause_sat(cid)) {
        SAT_ASSERT(!trace.literal_false(w.l1) || !trace.literal_false(w.l2));
        continue;
      }

      // If both watches are false, the clause should be entirely unsat.
      if (trace.literal_false(w.l1) && trace.literal_false(w.l2)) {
        SAT_ASSERT(trace.clause_unsat(cid));
      }
      // If exactly one watch is false, then the clause should be unit.
      else if (trace.literal_false(w.l1) || trace.literal_false(w.l2)) {
        SAT_ASSERT(trace.count_unassigned_literals(cid) == 1);
      }

      contains(literals_to_watcher[w.l1], cid);
      contains(literals_to_watcher[w.l2], cid);
    }

    // The literals_to_watcher maps shouldn't have extra edges
    // For every clause that a literal says it watches, that clause should say it's
    // watched by that literal.
    for (auto&& [l, cl] : literals_to_watcher) {
      for (auto cid : cl) {
        auto w = watched_literals[cid];
        SAT_ASSERT(watch_contains(w, l));
      }
    }

  }

};
