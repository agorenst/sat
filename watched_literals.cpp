#include "watched_literals.h"

#include "trace.h"

std::ostream& operator<<(std::ostream& o, const watcher_t& w);

watched_literals_t::watched_literals_t(trace_t& t)
    : cnf(t.cnf),
      trace(t),
      trail(t.actions),
      litstate(t.actions.litstate),
      literals_to_watcher(t.cnf),
      watched_literals(cnf.live_clause_count()) {}

bool watch_contains(const watcher_t& w, literal_t l) {
  return w.l1 == l || w.l2 == l;
}
auto watched_literals_t::find_second_watcher(clause_t& c,
                                           literal_t o) {
  auto it = std::begin(c);
  for (; it != std::end(c); it++) {
    literal_t l = *it;
    if (l == o) continue;
    if (!literal_false(l)) return it;
  }
  /*
  for (auto jt = it; jt != std::end(c); jt++) {
    literal_t l = *jt;
    if (l == o) continue;
    if (literal_true(l)) return jt;
  }
  */
  return it;
}
auto watched_literals_t::find_next_watcher(clause_t& c,
                                           literal_t o) {
  auto it = std::begin(c) + 2;
  for (; it != std::end(c); it++) {
    literal_t l = *it;
    if (l == o) continue;
    if (!literal_false(l)) return it;
  }
  /*
  for (auto jt = it; jt != std::end(c); jt++) {
    literal_t l = *jt;
    if (l == o) continue;
    if (literal_true(l)) return jt;
  }
  */
  return it;
}


// Add in a new clause to be watched
void watched_literals_t::watch_clause(clause_id cid) {
  //SAT_ASSERT(!clause_watched(cid));
  clause_t& c = cnf[cid];
  SAT_ASSERT(c.size() > 1);

  watcher_t w = {0, 0};

  // Find a non-false watcher; one should exist.
  literal_t l1 = find_first_watcher(c);
  SAT_ASSERT(l1);  // shouldn't be 0, at least
  w.l1 = l1;

  auto it2 = find_second_watcher(c, l1);

  // We may already be a unit. If so, to maintain backtracking
  // we want to have our other watcher be the last-falsified thing
  if (it2 == std::end(c)) {
    literal_t l = trail.find_last_falsified(cnf[cid]);
    w.l2 = l;
  } else {
    w.l2 = *it2;
  }
  SAT_ASSERT(w.l2);
  SAT_ASSERT(w.l2 != w.l1);

  // std::cout << "Watching #" << cid << " {" << c << "} with " << w <<
  // std::endl;
  literals_to_watcher[w.l1].push_back({cid, w.l2});
  literals_to_watcher[w.l2].push_back({cid, w.l1});

  // Keep the watch literlas in the front of the clause.
  {
    auto it = std::find(std::begin(c), std::end(c), w.l1);
    std::iter_swap(std::begin(c), it);
    auto jt = std::find(std::begin(c), std::end(c), w.l2);
    std::iter_swap(std::next(std::begin(c)), jt);
  }
  //watched_literals[cid] = w;
}

void watched_literals_t::literal_falsed(literal_t l) {
  // ul is the literal that is now unsat.
  literal_t ul = neg(l);
  SAT_ASSERT(trail.literal_false(ul)); // that's the whole point of this function
  //std::cerr << "literal_falsed: " << ul << std::endl;
  auto& watchers = literals_to_watcher[ul];

  int s = watchers.size();
  for (int i = 0; i < s; i++) {
    clause_id cid = watchers[i].first;
    // other literal
    literal_t ol = watchers[i].second;

    // Can we immediately conclude things?
    if (literal_true(ol)) continue;

    // The first two literals in the clause are always the "live" watcher.
    // What if we're not live?
    clause_t& c = cnf[cid];
    if (c[0] == ul) std::swap(c[0], c[1]);

    // Update our information
    watchers[i].second = c[0];
    ol = watchers[i].second;

    // Check the updated information
    if (literal_true(ol)) continue;

    SAT_ASSERT(c[0] == ol);
    SAT_ASSERT(c[1] == ul);


    SAT_ASSERT(contains(cnf[cid], ul));
    SAT_ASSERT(contains(cnf[cid], ol));
    
    auto it = find_next_watcher(c, ol);

    if (it != std::end(c)) {
      literal_t n = *it;
      SAT_ASSERT(n != ul);
      auto& new_set = literals_to_watcher[n];
      SAT_ASSERT(!contains(new_set, std::make_pair(cid, ol)));

      // Watch in the new clause
      new_set.push_back({cid, ol});

      // Remove from the old set
      std::swap(watchers[i], watchers[watchers.size() - 1]);
      watchers.pop_back();
      SAT_ASSERT(!contains(watchers, std::make_pair(cid, ol)));

      // Do the swap
      c[1] = n;
      *it = ul;
      SAT_ASSERT(c[0] == ol);
      SAT_ASSERT(c[1] == n);

      i--;  // don't skip the thing we just swapped.
      s--;
    } else {
      if (literal_false(ol)) {
        SAT_ASSERT(trail.clause_unsat(cnf[cid]));
        trace.push_conflict(cid);
        break;
      }
      else {
        SAT_ASSERT(trail.literal_unassigned(ol));
        SAT_ASSERT(trail.count_unassigned_literals(c) == 1);
        SAT_ASSERT(trail.find_unassigned_literal(c) == ol);
        trace.push_unit_queue(ol, cid);
      }
    }
  }

  SAT_ASSERT(trace.halted() || validate_state());
}

literal_t watched_literals_t::find_first_watcher(const clause_t& c) {
  for (literal_t l : c) {
    if (trail.literal_true(l)) return l;
  }
  for (literal_t l : c) {
    if (!trail.literal_false(l)) return l;
  }
  return 0;
}

void watched_literals_t::remove_clause(clause_id cid) {
  if (!clause_watched(cid)) return;
  const clause_t& c = cnf[cid];

  {
    auto& l1 = literals_to_watcher[c[0]];
    auto it = std::find_if(std::begin(l1), std::end(l1), [cid](auto& p) { return p.first == cid; });
    SAT_ASSERT(it != std::end(l1));
    std::iter_swap(it, std::prev(std::end(l1)));
    l1.pop_back();
    SAT_ASSERT(std::find_if(std::begin(l1), std::end(l1), [cid](auto& p) { return p.first == cid; }) == std::end(l1));
  }

  {
    auto& l2 = literals_to_watcher[c[1]];
    auto it = std::find_if(std::begin(l2), std::end(l2), [cid](auto& p) { return p.first == cid; });
    SAT_ASSERT(it != std::end(l2));
    std::iter_swap(it, std::prev(std::end(l2)));
    l2.pop_back();
    SAT_ASSERT(std::find_if(std::begin(l2), std::end(l2), [cid](auto& p) { return p.first == cid; }) == std::end(l2));
  }

}

#ifdef SAT_DEBUG_MODE
void watched_literals_t::print_watch_state() {
  for (auto cid : cnf) {
    if (!clause_watched(cid)) continue;
    const watcher_t& w = watched_literals[cid];
    std::cout << "#" << cid << ": " << cnf[cid] << " watched by " << w << std::endl;
  }

  auto lits = cnf.lit_range();
  for (literal_t l : lits) {
    std::cerr << "Literal " << l << " watching: ";
    for (const auto& wp : literals_to_watcher[l]) {
      const clause_t& c = cnf[wp.first];
      std::cerr << "#" << wp.first << ": " << c << "; ";
    }
    std::cerr << std::endl;
  }
}
#endif

bool watched_literals_t::clause_watched(clause_id cid) {
  return cnf[cid].size() > 1;
  //return watched_literals[cid].l1 != 0;
}
bool watched_literals_t::validate_state() {
  for (clause_id cid : cnf) {
    // We are, or aren't, watching this clause, as appropriate.
    const clause_t& c = cnf[cid];
    if (c.size() < 2) {
      SAT_ASSERT(!clause_watched(cid));
      continue;
    }
    //if (!clause_watched(cid)) {
    //std::cout << "CID not watched: " << cid << " {" << cnf[cid] << "}"
    //<< std::endl;
    //}
    SAT_ASSERT(clause_watched(cid));

    // The two literals watching us are in the clause.
    //const watcher_t w = watched_literals[cid];
    //if (!contains(c, w.l1)) {
    //std::cout << "Failing with #" << cid << "= {" << c << "} and w: " << w
    //<< ", specifically: " << w.l1 << std::endl;
    //}
    //SAT_ASSERT(contains(c, w.l1));
    //if (!contains(c, w.l2)) {
    //std::cout << "Failing with " << c << " and w: " << w
    //<< ", specifically: " << w.l2 << std::endl;
    //}
    //SAT_ASSERT(contains(c, w.l2));

    // TODO: Something to enforce about expected effect of backtracking
    // (the relative order of watched literals, etc. etc.)

    // I *think* that we have no other invariants, if the clause is sat.
    // I would think that the watched literals shouldn't be pointed to false,
    // but I'm not sure.
    #if 0
    if (trace.clause_sat(cid)) {
      if (trail.literal_false(w.l1) &&
          trail.literal_false(w.l2)) {
        std::cout << "Error, clause is sat, but both watched literals false: #"
                  << cid << "{" << c << "} with watch: " << w << std::endl;
        std::cout << *this << std::endl;
        std::cout << trace << std::endl;
      }
      SAT_ASSERT(!trail.literal_false(w.l1) ||
                 !trail.literal_false(w.l2));
      continue;
    }

    // If both watches are false, the clause should be entirely unsat.
    if (trail.literal_false(w.l1) &&
        trail.literal_false(w.l2)) {
      SAT_ASSERT(trace.clause_unsat(cid));
    }
    // If exactly one watch is false, then the clause should be unit.
    else if (trail.literal_false(w.l1) ||
             trail.literal_false(w.l2)) {
      if (trace.count_unassigned_literals(cid) != 1) {
        std::cout << "Failing with unit inconsistencies in: " << std::endl
                  << c << "; " << w << std::endl
                  << *this << std::endl
                  << "With trace state: " << std::endl
                  << trace << std::endl;
      }
      SAT_ASSERT(trace.count_unassigned_literals(cid) == 1);
    }

    contains(literals_to_watcher[w.l1], std::make_pair(cid, w.l2));
    contains(literals_to_watcher[w.l2], std::make_pair(cid, w.l1));
    #endif
  }

  // The literals_to_watcher maps shouldn't have extra edges
  // For every clause that a literal says it watches, that clause should say
  // it's watched by that literal.
#ifdef SAT_DEBUG_MODE
  std::map<clause_id, int> counter;
  auto lits = cnf.lit_range();
  for (literal_t l : lits) {
    const auto& cl = literals_to_watcher[l];
    for (auto wp : cl) {
      //auto w = watched_literals[wp.first];
      //SAT_ASSERT(watch_contains(w, l));
      //counter[wp.first]++;
    }
  }
  for (auto [cl, ct] : counter) {
    SAT_ASSERT(ct == 0 || ct == 2);
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
