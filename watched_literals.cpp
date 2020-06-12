#include "watched_literals.h"

#include "trace.h"

std::ostream& operator<<(std::ostream& o, const watcher_t& w);

watched_literals_t::watched_literals_t(trace_t& t)
    : cnf(t.cnf),
      trace(t),
      literals_to_watcher(t.cnf),
      watched_literals(cnf.live_clause_count()) {}

bool watch_contains(const watcher_t& w, literal_t l) {
  return w.l1 == l || w.l2 == l;
}
void watcher_literal_swap(watcher_t& w, literal_t o, literal_t n) {
  SAT_ASSERT(watch_contains(w, o));
  SAT_ASSERT(!watch_contains(w, n));
  if (w.l1 == o) {
    w.l1 = n;
  } else {
    w.l2 = n;
  }
  SAT_ASSERT(!watch_contains(w, o));
  SAT_ASSERT(watch_contains(w, n));
}

auto watched_literals_t::find_next_watcher(const clause_t& c, literal_t o) {
  auto it = std::begin(c);
  for (; it != std::end(c); it++) {
    literal_t l = *it;
    if (l == o) continue;
    if (!trace.actions.literal_false(l)) break;
  }
  for (auto jt = it; jt != std::end(c); jt++) {
    literal_t l = *jt;
    if (l == o) continue;
    if (trace.actions.literal_true(l)) return jt;
  }
  return it;
}

// Add in a new clause to be watched
void watched_literals_t::watch_clause(clause_id cid) {
  SAT_ASSERT(!clause_watched(cid));
  const clause_t& c = cnf[cid];
  SAT_ASSERT(c.size() > 1);

  watcher_t w = {0, 0};

  // Find a non-false watcher; one should exist.
  literal_t l1 = find_first_watcher(c);
  SAT_ASSERT(l1);  // shouldn't be 0, at least
  w.l1 = l1;

  auto it2 = find_next_watcher(c, l1);

  // We may already be a unit. If so, to maintain backtracking
  // we want to have our other watcher be the last-falsified thing
  if (it2 == std::end(c)) {
    literal_t l = trace.actions.find_last_falsified(cnf[cid]);
    w.l2 = l;
  } else {
    w.l2 = *it2;
  }
  SAT_ASSERT(w.l2);
  SAT_ASSERT(w.l2 != w.l1);

  // std::cout << "Watching #" << cid << " {" << c << "} with " << w <<
  // std::endl;
  literals_to_watcher[w.l1].push_back(cid);
  literals_to_watcher[w.l2].push_back(cid);
  watched_literals[cid] = w;
}

__attribute__((noinline))
void watched_literals_t::literal_falsed(literal_t l) {
  // ul is the literal that is now unsat.
  literal_t ul = neg(l);
  auto& clauses = literals_to_watcher[ul];

  auto s = clauses.size();
  for (int i = 0; i < s; i++) {
    clause_id cid = clauses[i];
    watcher_t& w = watched_literals[cid];

    SAT_ASSERT(contains(cnf[cid], ul));
    SAT_ASSERT(watch_contains(watched_literals[cid], ul));

    // If one of our clauses is SAT, we don't need to do
    // anything. We "cache" the SAT literal so we check
    // it first, if it comes up.
    if (trace.actions.literal_true(w.l1)) {
      continue;
    }
    if (trace.actions.literal_true(w.l2)) {
      std::swap(w.l1, w.l2);
      continue;
    }

    if (w.l2 == ul) {
      std::swap(w.l1, w.l2);
    }
    SAT_ASSERT(w.l2 != ul);
    SAT_ASSERT(w.l1 == ul);

    if (!trace.actions.literal_unassigned(w.l2)) {
      trace.push_conflict(cid);
      break;
    }

    const clause_t& c = cnf[cid];
    auto it = find_next_watcher(c, w.l2);

    if (it != std::end(c)) {
      s--;
      literal_t n = *it;
      SAT_ASSERT(n != ul);
      auto& new_set = literals_to_watcher[n];
      SAT_ASSERT(!contains(new_set, cid));
      new_set.push_back(cid);

      // Remove from the old set
      std::swap(clauses[i], clauses[clauses.size() - 1]);
      clauses.pop_back();

      w.l1 = n;

      i--;  // don't skip the thing we just swapped.
    } else {
      SAT_ASSERT(trace.actions.literal_unassigned(w.l2));
      SAT_ASSERT(trace.actions.count_unassigned_literals(c) == 1);
      SAT_ASSERT(trace.actions.find_unassigned_literal(c) == w.l2);
      trace.push_unit_queue(w.l2, cid);
    }
  }

  SAT_ASSERT(trace.halted() || validate_state());
}

literal_t watched_literals_t::find_first_watcher(const clause_t& c) {
  for (literal_t l : c) {
    if (trace.actions.literal_true(l)) return l;
  }
  for (literal_t l : c) {
    if (!trace.actions.literal_false(l)) return l;
  }
  return 0;
}

void watched_literals_t::remove_clause(clause_id id) {
  if (!clause_watched(id)) return;
  watcher_t& w = watched_literals[id];
  // std::cerr << "w to remove: " << w << std::endl;
  unsorted_remove(literals_to_watcher[w.l1], id);
  unsorted_remove(literals_to_watcher[w.l2], id);
  w.l1 = 0;
  w.l2 = 0;
  SAT_ASSERT(!clause_watched(id));
}

#ifdef SAT_DEBUG_MODE
void watched_literals_t::print_watch_state() const {
  // for (auto&& [cid, w] : watched_literals) {
  // std::cout << cnf[cid] << " watched by " << w << std::endl;
  //}
  // for (auto&& [literal, clause_list] : literals_to_watcher) {

  auto lits = cnf.lit_range();
  for (literal_t l : lits) {
    std::cout << "Literal " << l << " watching: ";
    for (const auto& cid : literals_to_watcher[l]) {
      const clause_t& c = cnf[cid];
      std::cout << c << "; ";
    }
    std::cout << std::endl;
  }
}
#endif

bool watched_literals_t::clause_watched(clause_id cid) {
  return watched_literals[cid].l1 != 0;
}
bool watched_literals_t::validate_state() {
  for (clause_id cid : cnf) {
    // We are, or aren't, watching this clause, as appropriate.
    const clause_t& c = cnf[cid];
    if (c.size() < 2) {
      SAT_ASSERT(!clause_watched(cid));
      continue;
    }
    if (!clause_watched(cid)) {
      std::cout << "CID not watched: " << cid << " {" << cnf[cid] << "}"
                << std::endl;
    }
    SAT_ASSERT(clause_watched(cid));

    // The two literals watching us are in the clause.
    const watcher_t w = watched_literals[cid];
    if (!contains(c, w.l1)) {
      std::cout << "Failing with #" << cid << "= {" << c << "} and w: " << w
                << ", specifically: " << w.l1 << std::endl;
    }
    SAT_ASSERT(contains(c, w.l1));
    if (!contains(c, w.l2)) {
      std::cout << "Failing with " << c << " and w: " << w
                << ", specifically: " << w.l2 << std::endl;
    }
    SAT_ASSERT(contains(c, w.l2));

    // TODO: Something to enforce about expected effect of backtracking
    // (the relative order of watched literals, etc. etc.)

    // I *think* that we have no other invariants, if the clause is sat.
    // I would think that the watched literals shouldn't be pointed to false,
    // but I'm not sure.
    if (trace.clause_sat(cid)) {
      if (trace.actions.literal_false(w.l1) &&
          trace.actions.literal_false(w.l2)) {
        std::cout << "Error, clause is sat, but both watched literals false: #"
                  << cid << "{" << c << "} with watch: " << w << std::endl;
        std::cout << *this << std::endl;
        std::cout << trace << std::endl;
      }
      SAT_ASSERT(!trace.actions.literal_false(w.l1) ||
                 !trace.actions.literal_false(w.l2));
      continue;
    }

    // If both watches are false, the clause should be entirely unsat.
    if (trace.actions.literal_false(w.l1) &&
        trace.actions.literal_false(w.l2)) {
      SAT_ASSERT(trace.clause_unsat(cid));
    }
    // If exactly one watch is false, then the clause should be unit.
    else if (trace.actions.literal_false(w.l1) ||
             trace.actions.literal_false(w.l2)) {
      if (trace.count_unassigned_literals(cid) != 1) {
        std::cout << "Failing with unit inconsistencies in: " << std::endl
                  << c << "; " << w << std::endl
                  << *this << std::endl
                  << "With trace state: " << std::endl
                  << trace << std::endl;
      }
      SAT_ASSERT(trace.count_unassigned_literals(cid) == 1);
    }

    contains(literals_to_watcher[w.l1], cid);
    contains(literals_to_watcher[w.l2], cid);
  }

  // The literals_to_watcher maps shouldn't have extra edges
  // For every clause that a literal says it watches, that clause should say
  // it's watched by that literal.
#ifdef SAT_DEBUG_MODE
  auto lits = cnf.lit_range();
  for (literal_t l : lits) {
    const auto& cl = literals_to_watcher[l];
    for (auto cid : cl) {
      auto w = watched_literals[cid];
      SAT_ASSERT(watch_contains(w, l));
    }
  }
#endif
  return true;
}

std::ostream& operator<<(std::ostream& o, const watcher_t& w) {
  return o << "[" << w.l1 << ", " << w.l2 << "]";
}
std::ostream& operator<<(std::ostream& o, const watched_literals_t& w) {
  assert(0);
  // w.print_watch_state();
  return o;
}
