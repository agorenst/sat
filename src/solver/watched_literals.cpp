#include "watched_literals.h"
#include "solver.h"
#include "subsumption.h"

#include <iterator>
#include "measurements.h"

void watched_literals_t::install(solver_t &s)
//: cnf(s.cnf),
// trail(s.trail),
// units(s.unit_queue),
// literals_to_watcher(max_variable(s.cnf))
{
  for (clause_id cid : s.cnf) {
    watch_clause(cid);
  }

  s.apply_decision_p.add_listener([&](literal_t l) {
    literal_falsed(l);
    MAX_ASSERT(trail.conflicted() || validate_state());
  });
  s.apply_unit_p.add_listener([&](literal_t l, clause_id cid) {
    literal_falsed(l);
    MAX_ASSERT(trail.conflicted() || validate_state());
  });
  s.remove_clause_p.add_listener([&](clause_id cid) { remove_clause(cid); });

  s.process_added_clause_p.add_listener([&](clause_id cid) {
    const clause_t &c = cnf[cid];
    if (c.size() > 1) watch_clause(cid);
    MAX_ASSERT(validate_state());
  });
  s.remove_literal_p.add_listener([&](clause_id cid, literal_t l) {
    remove_clause(cid);
    if (cnf[cid].size() > 1) {
      watch_clause(cid);
    }
  });

  MAX_ASSERT(validate_state());
}

watched_literals_t::watched_literals_t(cnf_t &cnf, trail_t &trail,
                                       unit_queue_t &units)
    : cnf(cnf),
      trail(trail),
      units(units),
      literals_to_watcher(max_variable(cnf)) {}

auto watched_literals_t::find_watcher(const clause_t &c, literal_t o /*= 0*/) {
  for (auto it = std::begin(c); it != std::end(c); it++) {
    literal_t l = *it;
    if (l == o) continue;
    if (trail.literal_true(l)) return it;
    if (!trail.literal_false(l)) return it;
  }
  return std::end(c);
}

// Add in a new clause to be watched
void watched_literals_t::watch_clause(clause_id cid) {
  // SAT_ASSERT(!clause_watched(cid));
  clause_t &c = cnf[cid];
  SAT_ASSERT(c.size() > 1);

  // Find a non-false watcher; one should exist.
  auto it = find_watcher(c, 0);
  literal_t l1 = *it;

  // Find the second watcher:
  auto it2 = find_watcher(c, l1);
  literal_t l2 = 0;

  // We may already be a unit. If so, to maintain backtracking
  // we want to have our other watcher be the last-falsified thing
  // if (it2 == std::end(c)) {
  if (it2 == std::end(c)) {
    literal_t l = trail.find_last_falsified(c);
    l2 = l;
  } else {
    l2 = *it2;
  }
  SAT_ASSERT(l2);
  SAT_ASSERT(l2 != l1);

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

// This is where the whole SAT solver spends most of its time.
// Lots of perf-specific choices.
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

    if (trail.literal_true(ol)) {
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
    if (trail.literal_true(ol)) {
      watchers[j++] = watchers[i++];
      continue;
    }

    SAT_ASSERT(c[0] == ol);
    SAT_ASSERT(c[1] == ul);

    SAT_ASSERT(contains(cnf[cid], ul));
    SAT_ASSERT(contains(cnf[cid], ol));

    auto it = std::begin(c) + 2;

    for (; it != std::end(c); it++) {
      if (!trail.literal_false(*it)) {
        break;
      }
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
      watchers[j++] = watchers[i++];
      if (trail.literal_false(ol)) {
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
      }
    }
  }
  watchers.resize(j);
  MAX_ASSERT(trail.conflicted() || validate_state());
}

void watched_literals_t::unwatch_clause(literal_t l, clause_id cid) {
  auto &lst = literals_to_watcher[l];
  auto it = std::find_if(std::begin(lst), std::end(lst),
                         [cid](auto &p) { return p.first == cid; });
  SAT_ASSERT(it != std::end(lst));
  std::iter_swap(it, std::prev(std::end(lst)));
  lst.pop_back();
  SAT_ASSERT(std::find_if(std::begin(lst), std::end(lst), [cid](auto &p) {
               return p.first == cid;
             }) == std::end(lst));
}

void watched_literals_t::remove_clause(clause_id cid) {
  if (!clause_watched(cid)) return;
  if (!cid->is_alive) return;
  const clause_t &c = cnf[cid];

  unwatch_clause(c[0], cid);
  unwatch_clause(c[1], cid);
}

void watched_literals_t::print_watch_state() {
  for (auto cid : cnf) {
    if (!clause_watched(cid)) continue;
    // const watcher_t& w = watched_literals[cid];
    // std::cout << "#" << cid << ": " << cnf[cid] << " watched by " << w
    //<< std::endl;
  }

  auto lits = cnf.lit_range();
  for (literal_t l : lits) {
    std::cerr << "Literal " << lit_to_dimacs(l) << " watching: ";
    for (const auto &wp : literals_to_watcher[l]) {
      const clause_t &c = cnf[wp.first];
      std::cerr << "{" << c << "}; ";
    }
    std::cerr << std::endl;
  }
}

bool watched_literals_t::clause_watched(clause_id cid) {
  return cnf[cid].size() > 1;
}
bool watched_literals_t::validate_state(clause_id skip_id) {
  for (clause_id cid : cnf) {
    if (cid == skip_id) continue;
    const clause_t &c = cnf[cid];
    if (c.size() < 2) {
      MAX_ASSERT(!clause_watched(cid));
      continue;
    }
    MAX_ASSERT(clause_watched(cid));

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
      MAX_ASSERT(trail.clause_unsat(c));
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
      MAX_ASSERT(trail.count_unassigned_literals(c) == 1);
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
      MAX_ASSERT(w != std::end(watchers));
      if (trail.literal_false(w->second)) {
        // this suggests that
        // I don't know about this! something to synthesize.
      } else {
        MAX_ASSERT(w->second != l2);
      }
    } else {
      MAX_ASSERT(!trail.clause_unsat(c));
    }

    // SAT_ASSERT(contains(literals_to_watcher[c[0]], std::make_pair(cid,
    // c[1]))); SAT_ASSERT(contains(literals_to_watcher[c[1]],
    // std::make_pair(cid, c[0])));
  }

  // The literals_to_watcher maps shouldn't have extra edges
  // For every clause that a literal says it watches, that clause should say
  // it's watched by that literal.
  std::map<clause_id, int> counter;
  auto lits = cnf.lit_range();
  for (literal_t l : lits) {
    const auto &cl = literals_to_watcher[l];
    for (auto wp : cl) {
      counter[wp.first]++;
    }
  }
  for (auto [cl, ct] : counter) {
    MAX_ASSERT(ct == 0 || ct == 2);
  }
  return true;
}

void const_watched_literals_t::install(solver_t &s)
//: cnf(s.cnf),
// trail(s.trail),
// units(s.unit_queue),
// literals_to_watcher(max_variable(s.cnf))
{
  for (clause_id cid : s.cnf) {
    watch_clause(cid);
  }

  s.apply_decision_p.add_listener([&](literal_t l) {
    literal_falsed(l);
    MAX_ASSERT(trail.conflicted() || validate_state());
  });
  s.apply_unit_p.add_listener([&](literal_t l, clause_id cid) {
    literal_falsed(l);
    MAX_ASSERT(trail.conflicted() || validate_state());
  });
  s.remove_clause_p.add_listener([&](clause_id cid) { remove_clause(cid); });

  s.process_added_clause_p.add_listener([&](clause_id cid) {
    const clause_t &c = cnf[cid];
    if (c.size() > 1) watch_clause(cid);
    SAT_ASSERT(validate_state());
  });
  s.remove_literal_p.add_listener([&](clause_id cid, literal_t l) {
    remove_clause(cid);
    if (cnf[cid].size() > 1) {
      watch_clause(cid);
    }
  });

  MAX_ASSERT(validate_state());
}

const_watched_literals_t::const_watched_literals_t(cnf_t &cnf, trail_t &trail,
                                                   unit_queue_t &units)
    : cnf(cnf),
      trail(trail),
      units(units),
      literals_to_watcher(max_variable(cnf)) {}

auto const_watched_literals_t::find_watcher(const clause_t &c,
                                            literal_t o /*= 0*/) {
  for (auto it = std::begin(c); it != std::end(c); it++) {
    literal_t l = *it;
    if (l == o) continue;
    if (trail.literal_true(l)) return it;
    if (!trail.literal_false(l)) return it;
  }
  return std::end(c);
}

// Add in a new clause to be watched
void const_watched_literals_t::watch_clause(clause_id cid) {
  // SAT_ASSERT(!clause_watched(cid));
  const clause_t &c = cnf[cid];
  SAT_ASSERT(c.size() > 1);

  // Find a non-false watcher; one should exist.
  auto it = find_watcher(c, 0);
  literal_t l1 = *it;

  // Find the second watcher:
  auto it2 = find_watcher(c, l1);
  literal_t l2 = 0;

  // We may already be a unit. If so, to maintain backtracking
  // we want to have our other watcher be the last-falsified thing
  // if (it2 == std::end(c)) {
  if (it2 == std::end(c)) {
    literal_t l = trail.find_last_falsified(c);
    l2 = l;
  } else {
    l2 = *it2;
  }
  MAX_ASSERT(l2);
  MAX_ASSERT(l2 != l1);

  literals_to_watcher[l1].push_back({cid, l2});
  literals_to_watcher[l2].push_back({cid, l1});

  watched_literals[cid] = {l1, l2};
}

// This is where the whole SAT solver spends most of its time.
// Lots of perf-specific choices.
void const_watched_literals_t::literal_falsed(literal_t l) {
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

    if (trail.literal_true(ol)) {
      watchers[j++] = watchers[i++];
      continue;
    }

    // What are the actual literals watching us?
    watcher_t &w = watched_literals[cid];
    if (w.l1 == ul) std::swap(w.l1, w.l2);

    // Update our information
    ol = w.l1;
    watchers[i].second = ol;

    // Check the updated information
    if (trail.literal_true(ol)) {
      watchers[j++] = watchers[i++];
      continue;
    }

    MAX_ASSERT(w.l1 == ol);
    MAX_ASSERT(w.l2 == ul);

    MAX_ASSERT(contains(cnf[cid], ul));
    MAX_ASSERT(contains(cnf[cid], ol));

    const clause_t &c = cnf[cid];

    auto it = std::find_if(std::begin(c), std::end(c), [&](literal_t l) {
      return l != w.l1 && l != w.l2 && !trail.literal_false(l);
    });

    if (it != std::end(c)) {
      // we do have a next watcher, "n", at location "it".

      literal_t n = *it;
      MAX_ASSERT(n != ul);
      auto &new_set = literals_to_watcher[n];
      MAX_ASSERT(!contains(new_set, std::make_pair(cid, ol)));

      // Watch in the new clause
      new_set.emplace_back(std::make_pair(cid, ol));

      // Do the swap
      w.l2 = n;

      MAX_ASSERT(w.l1 == ol);
      MAX_ASSERT(w.l2 == n);

      // Explicitly don't copy this thing
      i++;
    } else {
      watchers[j++] = watchers[i++];
      if (trail.literal_false(ol)) {
        MAX_ASSERT(trail.clause_unsat(cnf[cid]));
        trail.append(make_conflict(cid));
        while (i < s) {
          watchers[j++] = watchers[i++];
        }
      } else {
        MAX_ASSERT(trail.literal_unassigned(ol));
        MAX_ASSERT(trail.count_unassigned_literals(c) == 1);
        MAX_ASSERT(trail.find_unassigned_literal(c) == ol);
        units.push({ol, cid});
      }
    }
  }
  watchers.resize(j);
  // For the vivification case, we need to somehow pass the "except-this-one"
  // CID, so for now we comment this.
  // MAX_ASSERT(trail.conflicted() || validate_state());
}

void const_watched_literals_t::unwatch_clause(literal_t l, clause_id cid) {
  auto &lst = literals_to_watcher[l];
  auto it = std::find_if(std::begin(lst), std::end(lst),
                         [cid](auto &p) { return p.first == cid; });
  MAX_ASSERT(it != std::end(lst));
  std::iter_swap(it, std::prev(std::end(lst)));
  lst.pop_back();
  MAX_ASSERT(std::find_if(std::begin(lst), std::end(lst), [cid](auto &p) {
               return p.first == cid;
             }) == std::end(lst));
}

void const_watched_literals_t::remove_clause(clause_id cid) {
  if (!clause_watched(cid)) return;
  if (!cid->is_alive) return;
  watcher_t w = watched_literals[cid];

  unwatch_clause(w.l1, cid);
  unwatch_clause(w.l2, cid);
  watched_literals.erase(cid);
}

void const_watched_literals_t::print_watch_state() {
  for (auto cid : cnf) {
    if (!clause_watched(cid)) continue;
    // const watcher_t& w = watched_literals[cid];
    // std::cout << "#" << cid << ": " << cnf[cid] << " watched by " << w
    //<< std::endl;
  }

  auto lits = cnf.lit_range();
  for (literal_t l : lits) {
    std::cerr << "Literal " << lit_to_dimacs(l) << " watching: ";
    for (const auto &wp : literals_to_watcher[l]) {
      const clause_t &c = cnf[wp.first];
      std::cerr << "{" << c << "}; ";
    }
    std::cerr << std::endl;
  }
}

bool const_watched_literals_t::clause_watched(clause_id cid) {
  return watched_literals.find(cid) != watched_literals.end();
}
bool const_watched_literals_t::validate_state(clause_id skip_id) {
  // Our two maps are consistent:
  auto watch_list_contains = [](const watch_list_t &wl, clause_id cid) {
    // deliberately doing count-if to also enforce that we expect no repeats.
    return std::count_if(std::begin(wl), std::end(wl),
                         [&](auto wp) { return wp.first == cid; }) == 1;
  };
  for (auto cid : cnf) {
    if (!clause_watched(cid)) continue;
    if (cid == skip_id) continue;
    const watcher_t &w = watched_literals[cid];
    const auto &wl1 = literals_to_watcher[w.l1];
    const auto &wl2 = literals_to_watcher[w.l2];
    assert(watch_list_contains(wl1, cid));
    assert(watch_list_contains(wl2, cid));
  }

  for (clause_id cid : cnf) {
    if (cid == skip_id) continue;
    const clause_t &c = cnf[cid];
    if (c.size() < 2) {
      MAX_ASSERT(!clause_watched(cid));
      continue;
    }
    MAX_ASSERT(clause_watched(cid));

    // TODO: Something to enforce about expected effect of backtracking
    // (the relative order of watched literals, etc. etc.)

    // I *think* that we have no other invariants, if the clause is sat.
    // I would think that the watched literals shouldn't be pointed to false,
    // but I'm not sure.

    auto [l1, l2] = watched_literals[cid];

    // If both watches are false, the clause should be entirely unsat.
    if (trail.literal_false(l1) && trail.literal_false(l2)) {
      if (!trail.clause_unsat(c)) {
        std::cerr << "Failing with unit inconsistencies in: " << std::endl
                  << c << "; " << std::endl
                  << "With trail state: " << std::endl
                  << trail << std::endl;
      }
      MAX_ASSERT(trail.clause_unsat(c));
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
      MAX_ASSERT(trail.count_unassigned_literals(c) == 1);
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
      MAX_ASSERT(w != std::end(watchers));
      if (trail.literal_false(w->second)) {
        // this suggests that
        // I don't know about this! something to synthesize.
      } else {
        MAX_ASSERT(w->second != l2);
      }
    } else {
      MAX_ASSERT(!trail.clause_unsat(c));
    }

    // SAT_ASSERT(contains(literals_to_watcher[c[0]], std::make_pair(cid,
    // c[1]))); SAT_ASSERT(contains(literals_to_watcher[c[1]],
    // std::make_pair(cid, c[0])));
  }

  // The literals_to_watcher maps shouldn't have extra edges
  // For every clause that a literal says it watches, that clause should say
  // it's watched by that literal.
  std::map<clause_id, int> counter;
  auto lits = cnf.lit_range();
  for (literal_t l : lits) {
    const auto &cl = literals_to_watcher[l];
    for (auto wp : cl) {
      counter[wp.first]++;
    }
  }
  for (auto [cl, ct] : counter) {
    MAX_ASSERT(ct == 0 || ct == 2);
    (void)cl;
  }
  return true;
}