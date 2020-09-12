#include "watched_literals.h"
#include "subsumption.h"

#include "measurements.h"

watched_literals_t::watched_literals_t(cnf_t &cnf, trail_t &trail,
                                       unit_queue_t &units)
    : cnf(cnf),
      trail(trail),
      units(units),
      litstate(trail.litstate),
      literals_to_watcher(max_variable(cnf)) {}

auto watched_literals_t::find_second_watcher(clause_t &c, literal_t o) {
  auto it = std::begin(c);
  // for (; it != std::end(c); it++) {
  for (; it != std::end(c); it++) {
    literal_t l = *it;
    if (l == o) continue;
    if (!literal_false(l)) return it;
  }
  return it;
}

// Add in a new clause to be watched
void watched_literals_t::watch_clause(clause_id cid) {
  // SAT_ASSERT(!clause_watched(cid));
  clause_t &c = cnf[cid];
  SAT_ASSERT(c.size() > 1);

  // Find a non-false watcher; one should exist.
  literal_t l1 = find_first_watcher(c);
#ifdef SAT_DEBUG_MODE
  if (!l1) std::cerr << "Could not find watch in " << c << std::endl;
  SAT_ASSERT(l1);  // shouldn't be 0, at least
#endif

  auto it2 = find_second_watcher(c, l1);
  literal_t l2 = 0;

  // We may already be a unit. If so, to maintain backtracking
  // we want to have our other watcher be the last-falsified thing
  // if (it2 == std::end(c)) {
  if (it2 == std::end(c)) {
    literal_t l = trail.find_last_falsified(cnf[cid]);
    l2 = l;
  } else {
    l2 = *it2;
  }
  SAT_ASSERT(l2);
  SAT_ASSERT(l2 != l1);

  // std::cout << "Watching #" << cid << " {" << c << "} with " << w <<
  // std::endl;
  literals_to_watcher[l1].push_back({cid, l2});
  literals_to_watcher[l2].push_back({cid, l1});

  // Keep the watch literals in the front of the clause.
  {
    auto it = std::find(std::begin(c), std::end(c), l1);
    std::iter_swap(std::begin(c), it);
    auto jt = std::find(std::begin(c), std::end(c), l2);
    std::iter_swap(std::next(std::begin(c)), jt);
  }
}

void watched_literals_t::literal_falsed(literal_t l) {
  literal_t ul = neg(l);
  SAT_ASSERT(trail.literal_false(ul));

  auto &watchers = literals_to_watcher[ul];

  int s = watchers.size();
  int i = 0;
  int j = 0;
  while (i < s) {
    clause_id cid = watchers[i].first;
    // other literal
    literal_t ol = watchers[i].second;

    if (literal_true(ol)) {
      watchers[j++] = watchers[i++];
      continue;
    }

    // The first two literals in the clause are always the "live" watcher.
    // What if we're not live?
    clause_t &c = cnf[cid];
    if (c[0] == ul) std::swap(c[0], c[1]);

    // Update our information
    ol = c[0];
    watchers[i].second = ol;

    // Check the updated information
    if (literal_true(ol)) {
      watchers[j++] = watchers[i++];
      continue;
    }

    SAT_ASSERT(c[0] == ol);
    SAT_ASSERT(c[1] == ul);

    SAT_ASSERT(contains(cnf[cid], ul));
    SAT_ASSERT(contains(cnf[cid], ol));

    auto it = std::begin(c) + 2;
    // metrics.rewatch_count++;
    for (; it != std::end(c); it++) {
      if (!literal_false(*it)) {
        break;
      }
      // metrics.rewatch_iterations++;
    }

    if (it != std::end(c)) {
      // we do have a next watcher, "n", at location "it".
      literal_t n = *it;
      SAT_ASSERT(n != ul);
      auto &new_set = literals_to_watcher[n];
      SAT_ASSERT(!contains(new_set, std::make_pair(cid, ol)));

      // Watch in the new clause
      new_set.emplace_back(std::make_pair(cid, ol));

      // Do the swap
      c[1] = n;
      *it = ul;

      SAT_ASSERT(c[0] == ol);
      SAT_ASSERT(c[1] == n);

      // Explicitly don't copy this thing
      i++;
    } else {
      if (literal_false(ol)) {
        SAT_ASSERT(trail.clause_unsat(cnf[cid]));
        trail.append(make_conflict(cid));
        while (i < s) {
          watchers[j++] = watchers[i++];
        }
      } else {
        SAT_ASSERT(trail.literal_unassigned(ol));
        SAT_ASSERT(trail.count_unassigned_literals(c) == 1);
        SAT_ASSERT(trail.find_unassigned_literal(c) == ol);
        units.push({ol, cid});
        watchers[j++] = watchers[i++];
      }
    }
  }
  watchers.resize(j);
}

literal_t watched_literals_t::find_first_watcher(const clause_t &c) {
  for (literal_t l : c) {
    if (trail.literal_true(l)) return l;
  }
  for (literal_t l : c) {
    if (!trail.literal_false(l)) return l;
  }
  return 0;
}

INLINESTATE void watched_literals_t::remove_clause(clause_id cid) {
  if (!clause_watched(cid)) return;
  if (!cid->is_alive) return;
  const clause_t &c = cnf[cid];

  {
    auto &l1 = literals_to_watcher[c[0]];
    auto it = std::find_if(std::begin(l1), std::end(l1),
                           [cid](auto &p) { return p.first == cid; });
    SAT_ASSERT(it != std::end(l1));
    std::iter_swap(it, std::prev(std::end(l1)));
    l1.pop_back();
    SAT_ASSERT(std::find_if(std::begin(l1), std::end(l1), [cid](auto &p) {
                 return p.first == cid;
               }) == std::end(l1));
  }

  {
    auto &l2 = literals_to_watcher[c[1]];
    auto it = std::find_if(std::begin(l2), std::end(l2),
                           [cid](auto &p) { return p.first == cid; });
    SAT_ASSERT(it != std::end(l2));
    std::iter_swap(it, std::prev(std::end(l2)));
    l2.pop_back();
    SAT_ASSERT(std::find_if(std::begin(l2), std::end(l2), [cid](auto &p) {
                 return p.first == cid;
               }) == std::end(l2));
  }
}

void watched_literals_t::remove_literal(clause_id cid, literal_t l) {
  // TODO: This has never been used. See how remove_literal simply removes and
  // re-adds the clause. That seems to be good enough, for our purposes.
  assert(0);
  if (!clause_watched(cid)) return;
  if (!cid->is_alive) return;
  const clause_t &c = cnf[cid];

  // We won't be watching this, anyways.
  if (c.size() == 2) {
    return;
  }

  auto it = std::find(std::begin(c), std::end(c), l);

  // The literal we're removing isn't watching this clause -- great.
  SAT_ASSERT(it != std::end(c));
  if (std::next(std::begin(c)) < it) {
    return;
  }

  // Otherwise, we need to find a jt to swap with that it.
  auto jt = std::begin(c) + 2;

  // let's try to find a non-falsed literal:
  for (; *jt; jt++) {
    if (!literal_false(*jt)) {
      break;
    }
  }

  // we failed to find a non-falsed literal,
  // find the falsed literal with the highest level.
  if (jt == std::end(c)) {
    jt = std::max_element(std::begin(c) + 2, std::end(c),
                          [&](literal_t a, literal_t b) {
                            SAT_ASSERT(a != l && b != l);
                            return this->trail.level(a) < this->trail.level(b);
                          });
  }

  SAT_ASSERT(jt != std::end(c));

  // No matter what, now, we have jt which is not watching
  // this clause, and it that is. Swap them.
}

#ifdef SAT_DEBUG_MODE
void watched_literals_t::print_watch_state() {
  for (auto cid : cnf) {
    if (!clause_watched(cid)) continue;
    // const watcher_t& w = watched_literals[cid];
    // std::cout << "#" << cid << ": " << cnf[cid] << " watched by " << w
    //<< std::endl;
  }

  auto lits = cnf.lit_range();
  for (literal_t l : lits) {
    std::cerr << "Literal " << l << " watching: ";
    for (const auto &wp : literals_to_watcher[l]) {
      const clause_t &c = cnf[wp.first];
      std::cerr << "#" << wp.first << ": " << c << "; ";
    }
    std::cerr << std::endl;
  }
}
#endif

bool watched_literals_t::clause_watched(clause_id cid) {
  return cnf[cid].size() > 1;
}
bool watched_literals_t::validate_state(clause_id skip_id) {
  for (clause_id cid : cnf) {
    if (cid == skip_id) continue;
    const clause_t &c = cnf[cid];
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

    literal_t l1 = c[0];
    literal_t l2 = c[1];

    // If both watches are false, the clause should be entirely unsat.
    if (trail.literal_false(l1) && trail.literal_false(l2)) {
      if (!trail.clause_unsat(c)) {
        std::cerr << "Failing with unit inconsistencies in: " << std::endl
                  << c << "; " << std::endl
                  << "With trail state: " << std::endl
                  << trail << std::endl;
      }
      SAT_ASSERT(trail.clause_unsat(c));
    } else if (trail.clause_sat(c)) {
      // ???
    }
    // we have this condition (c[1], instead of c[0] || c[1] or something)
    // mainly out of experimental results. Hrrrrrm.
    else if (trail.literal_false(l2)) {
      if (trail.count_unassigned_literals(c) != 1) {
        std::cerr << "Failing with unit inconsistencies in: " << std::endl
                  << c << "; " << std::endl
                  << "With trail state: " << std::endl
                  << trail << std::endl;
      }
      SAT_ASSERT(trail.count_unassigned_literals(c) == 1);
    } else if (trail.literal_false(l1)) {
      // this is something subtle, because we do have a "lazy" update
      // strategy -- see what happens in literal_falsed, for instance.
      auto &watchers = literals_to_watcher[l1];
      // find our watcher
      auto w = std::find_if(std::begin(watchers), std::end(watchers),
                            [cid](const std::pair<clause_id, literal_t> &o) {
                              return o.first == cid;
                            });
      if (w == std::end(watchers)) {
        std::cerr << "Only single watcher for clause: " << c << std::endl;
      }
      SAT_ASSERT(w != std::end(watchers));
      if (trail.literal_false(w->second)) {
        // this suggests that
        // I don't know about this! something to synthesize.
      } else {
        SAT_ASSERT(w->second != l2);
      }
    } else {
      SAT_ASSERT(!trail.clause_unsat(c));
    }

    // SAT_ASSERT(contains(literals_to_watcher[c[0]], std::make_pair(cid,
    // c[1]))); SAT_ASSERT(contains(literals_to_watcher[c[1]],
    // std::make_pair(cid, c[0])));
  }

  // The literals_to_watcher maps shouldn't have extra edges
  // For every clause that a literal says it watches, that clause should say
  // it's watched by that literal.
#ifdef SAT_DEBUG_MODE
  std::map<clause_id, int> counter;
  auto lits = cnf.lit_range();
  for (literal_t l : lits) {
    const auto &cl = literals_to_watcher[l];
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

std::ostream &operator<<(std::ostream &o, const watched_literals_t &w) {
  assert(0);
  // w.print_watch_state();
  return o;
}
