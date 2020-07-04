#include "watched_literals.h"
#include "measurements.h"

#include "trace.h"

std::ostream& operator<<(std::ostream& o, const watcher_t& w);

watched_literals_t::watched_literals_t(const cnf_t& cnf, trail_t& trail,
                                       unit_queue_t& units)
    : cnf(cnf),
      trail(trail),
      units(units),
      litstate(trail.litstate),
      literals_to_clause(cnf),
      clause_to_literals(cnf.live_clause_count()) {}

bool watch_contains(const watcher_t& w, literal_t l) {
  return w.l1 == l || w.l2 == l;
}

// Maybe we can give it the range, and get something useful there?
void watched_literals_t::construct(cnf_t& cnf) {
  for (clause_id cid : cnf) {
    watch_clause(cid);
  }
}

auto watched_literals_t::find_second_watcher(const clause_t& c, literal_t o) {
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
auto watched_literals_t::find_next_watcher(const clause_t& c, literal_t o) {
  //auto it = std::begin(c) + 2;
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

// Add in a new clause to be watched
void watched_literals_t::watch_clause(clause_id cid) {
  // SAT_ASSERT(!clause_watched(cid));
  const clause_t& c = cnf[cid];
  SAT_ASSERT(c.size() > 1);

  watcher_t w = {0, 0};

  // Find a non-false watcher; one should exist.
  literal_t l1 = find_first_watcher(c);
#ifdef SAT_DEBUG_MODE
  if (!l1) std::cerr << "Could not find watch in " << c << std::endl;
  SAT_ASSERT(l1);  // shouldn't be 0, at least
#endif
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
  literals_to_clause[w.l1].push_back({cid, w.l2});
  literals_to_clause[w.l2].push_back({cid, w.l1});

  #if 0
  // Keep the watch literals in the front of the clause.
  {
    auto it = std::find(std::begin(c), std::end(c), w.l1);
    std::iter_swap(std::begin(c), it);
    auto jt = std::find(std::begin(c), std::end(c), w.l2);
    std::iter_swap(std::next(std::begin(c)), jt);
  }
  #endif

  clause_to_literals[cid] = w;
}

void watched_literals_t::literal_falsed(literal_t l) {
  // timer t(timer::action::literal_falsed);
  // ul is the literal that is now unsat.
  literal_t ul = neg(l);
  SAT_ASSERT(
      trail.literal_false(ul));  // that's the whole point of this function
  // std::cerr << "literal_falsed: " << ul << std::endl;
  auto& watchers = literals_to_clause[ul];

  int s = watchers.size();
  for (int i = 0; i < s; i++) {
    clause_id cid = watchers[i].first;
    // other literal
    // std::cerr << "Reviewing clause: " << cnf[cid] << std::endl;
    literal_t ol = watchers[i].second;

    // Can we immediately conclude things?
    if (literal_true(ol)) continue;

    // This stores what literals we're actually using to watch
    // clause cid. Something that's out of date with the pair we
    // have
    watcher_t& w = clause_to_literals[cid];

    // Update things so the first literal is (hopefully) sat.
    if (w.l1 == ul) std::swap(w.l1, w.l2);

    // Update our information, keep it as live as possible.
    watchers[i].second = w.l1;
    ol = watchers[i].second;

    // Check the updated information
    if (literal_true(ol)) continue;

    SAT_ASSERT(w.l1 == ol);
    SAT_ASSERT(w.l2 == ul);

    SAT_ASSERT(contains(cnf[cid], ul));
    SAT_ASSERT(contains(cnf[cid], ol));

    const clause_t& c = cnf[cid];
    auto it = find_next_watcher(c, ol);

    if (it != std::end(c)) {
      literal_t n = *it;
      SAT_ASSERT(n != ul);
      auto& new_set = literals_to_clause[n];
      SAT_ASSERT(!contains(new_set, std::make_pair(cid, ol)));

      // Watch in the new clause
      new_set.push_back({cid, ol});

      // Remove from the old set
      std::swap(watchers[i], watchers[watchers.size() - 1]);
      watchers.pop_back();
      SAT_ASSERT(!contains(watchers, std::make_pair(cid, ol)));

      // Do the swap
      w.l2 = n;
      SAT_ASSERT(w.l1 == ol);
      SAT_ASSERT(w.l2 == n);

      i--;  // don't skip the thing we just swapped.
      s--;
    } else {
      if (literal_false(ol)) {
        SAT_ASSERT(trail.clause_unsat(cnf[cid]));
        trail.append(make_conflict(cid));
        break;
      } else {
        SAT_ASSERT(trail.literal_unassigned(ol));
        SAT_ASSERT(trail.count_unassigned_literals(c) == 1);
        SAT_ASSERT(trail.find_unassigned_literal(c) == ol);
        units.push(make_unit_prop(ol, cid));
      }
    }
  }

  // SAT_ASSERT(trace.halted() || validate_state());
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

__attribute__((noinline)) void watched_literals_t::remove_clause(
    clause_id cid) {
  if (!clause_watched(cid)) return;
  watcher_t& w = clause_to_literals[cid];

  {
    auto& l1 = literals_to_clause[w.l1];
    auto it = std::find_if(std::begin(l1), std::end(l1),
                           [cid](auto& p) { return p.first == cid; });
    SAT_ASSERT(it != std::end(l1));
    std::iter_swap(it, std::prev(std::end(l1)));
    l1.pop_back();
    SAT_ASSERT(std::find_if(std::begin(l1), std::end(l1), [cid](auto& p) {
                 return p.first == cid;
               }) == std::end(l1));
  }

  {
    auto& l2 = literals_to_clause[w.l2];
    auto it = std::find_if(std::begin(l2), std::end(l2),
                           [cid](auto& p) { return p.first == cid; });
    SAT_ASSERT(it != std::end(l2));
    std::iter_swap(it, std::prev(std::end(l2)));
    l2.pop_back();
    SAT_ASSERT(std::find_if(std::begin(l2), std::end(l2), [cid](auto& p) {
                 return p.first == cid;
               }) == std::end(l2));
  }

  w = {0, 0};
}

void watched_literals_t::reset() {
  literals_to_clause.clear();
}

#ifdef SAT_DEBUG_MODE
void watched_literals_t::print_watch_state() {
  for (auto cid : cnf) {
    if (!clause_watched(cid)) continue;
    const watcher_t& w = clause_to_literals[cid];
    std::cout << "#" << cid << ": " << cnf[cid] << " watched by " << w
              << std::endl;
  }

  auto lits = cnf.lit_range();
  for (literal_t l : lits) {
    std::cerr << "Literal " << l << " watching: ";
    for (const auto& wp : literals_to_clause[l]) {
      const clause_t& c = cnf[wp.first];
      std::cerr << "#" << wp.first << ": " << c << "; ";
    }
    std::cerr << std::endl;
  }
}
#endif

bool watched_literals_t::clause_watched(clause_id cid) {
  return cnf[cid].size() > 1;
  // return watched_literals[cid].l1 != 0;
}
bool watched_literals_t::validate_state() {
  for (clause_id cid : cnf) {
    const clause_t& c = cnf[cid];
    if (c.size() < 2) {
      SAT_ASSERT(!clause_watched(cid));
      continue;
    }
    SAT_ASSERT(clause_watched(cid));

    // TODO: Something to enforce about expected effect of backtracking
    // (the relative order of watched literals, etc. etc.)

    // I *think* that we have no other invariants, if the clause is sat.
    // I would think that the watched literals shouldn't be pointed to false,
    // but I'm not sure.

    const watcher_t& w = clause_to_literals[cid];
    // If both watches are false, the clause should be entirely unsat.
    if (trail.literal_false(w.l1) && trail.literal_false(w.l2)) {
      SAT_ASSERT(trail.clause_unsat(cnf[cid]));
    }

    // If exactly one watch is false, then the clause should be unit.
    else if (trail.clause_sat(c)) {
      //???
    } else if (trail.literal_false(w.l1) || trail.literal_false(w.l2)) {
      if (trail.count_unassigned_literals(cnf[cid]) != 1) {
        std::cout << "Failing with unit inconsistencies in: " << std::endl
                  << c << "; " << std::endl
                  << "With trail state: " << std::endl
                  << trail << std::endl;
      }
      SAT_ASSERT(trail.count_unassigned_literals(cnf[cid]) == 1);
    }

    contains(literals_to_clause[w.l1], std::make_pair(cid, c[1]));
    contains(literals_to_clause[w.l2], std::make_pair(cid, c[0]));
  }

  // The literals_to_clause maps shouldn't have extra edges
  // For every clause that a literal says it watches, that clause should say
  // it's watched by that literal.
#ifdef SAT_DEBUG_MODE
  std::map<clause_id, int> counter;
  auto lits = cnf.lit_range();
  for (literal_t l : lits) {
    const auto& cl = literals_to_clause[l];
    for (auto wp : cl) {
      counter[wp.first]++;
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
