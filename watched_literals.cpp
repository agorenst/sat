#include "watched_literals.h"

#include "trace.h"

watched_literals_t::watched_literals_t(trace_t& t): cnf(t.cnf), trace(t) {}

bool active = true;

// Add in a new clause to be watched
void watched_literals_t::watch_clause(clause_id cid) {
  SAT_ASSERT(!clause_watched(cid));
  const clause_t& c = cnf[cid];
  SAT_ASSERT(c.size() > 1);

  watcher_t w = {0, 0};

  // Find a non-false watcher; one should exist.
  literal_t l1 = find_next_watcher(c, w);
  SAT_ASSERT(l1); // shouldn't be 0, at least
  w.l1 = l1;

  literal_t l2 = find_next_watcher(c, w);

  // We may already be a unit. If so, to maintain backtracking
  // we want to have our other watcher be the last-falsified thing 
  if (l2 == 0) {
    literal_t l = trace.find_last_falsified(cid);
    w.l2 = l;
  }
  else {
    w.l2 = l2;
  }
  SAT_ASSERT(w.l2);
  SAT_ASSERT(w.l2 != w.l1);

  //std::cout << "Watching #" << cid << " {" << c << "} with " << w << std::endl;
  literals_to_watcher[w.l1].push_back(cid);
  literals_to_watcher[w.l2].push_back(cid);
  watched_literals[cid] = w;
}

void watched_literals_t::literal_falsed(literal_t l) {
  ////std::cout << "About to falsify " << l << ", with state being: " << std::endl << *this << std::endl;
  // Make a copy because we're about to transform this list.
  clause_list_t clauses = literals_to_watcher[-l];
  for (clause_id cid : clauses) {
    literal_falsed(l, cid);
    if (trace.halted()) {
      break;
    }
  }
  //std::cout << "Just falsified" << l << ", with state being: " << std::endl << *this << std::endl;
  if (!trace.halted()) SAT_ASSERT(validate_state());
}

void watched_literals_t::literal_falsed(literal_t l, clause_id cid) {
  SAT_ASSERT(trace.actions.literal_true(l));
  const clause_t& c = cnf[cid];


  watcher_t& w = watched_literals[cid];
  watcher_t oldw = w;
  SAT_ASSERT(contains(c, -l));
  SAT_ASSERT(watch_contains(w, -l));

  // see if we can find a new watching value.
  literal_t n = find_next_watcher(c, w);

  // If we can't, we're either a unit or a conflict.
  if (n == 0) {
    auto s = trace.count_unassigned_literals(c);
    SAT_ASSERT(s < 2);
    if (!trace.clause_sat(c) && active) {
      if (s == 1) {
        literal_t u = trace.find_unassigned_literal(c);
        trace.push_unit_queue(u, cid);
        //std::cout << "Falsing " << l << " in clause #" << cid << " {" << c << "} caused unit, no change to watcher " << w << std::endl;
      }
      else {
        //std::cout << *this << std::endl;
        SAT_ASSERT(trace.clause_unsat(cid));
        trace.push_conflict(cid);
        //std::cout << "Falsing " << l << " in clause #" << cid << " {" << c << "} caused conflict, no change to watcher " << w << std::endl;
      }
    }
    else {
      //std::cout << "Falsing " << l << " in clause #" << cid << " {" << c << "} already sat, no change to watcher " << w << std::endl;
    }
  }
  // If we can, then we swap to that.
  else {
    if (trace.count_unassigned_literals(c) + trace.count_true_literals(c) <= 1) {
      std::cout << "Falsing " << l << " means {" << c << "} " << "watched by " << w << " has " << trace.count_unassigned_literals(c) << " free literals, found candidate n = " << n << std::endl;
      std::cout << trace << std::endl << cnf << std::endl;
    }
    SAT_ASSERT(trace.count_unassigned_literals(c) + trace.count_true_literals(c) > 1);
    watcher_swap(cid, w, -l, n);
    //std::cout << "Falsing " << l << " in clause #" << cid << " {" << c << "} has watcher go from: " << oldw << " to " << w << std::endl;
  }
}



bool watched_literals_t::watch_contains(const watcher_t& w, literal_t l) {
  return w.l1 == l || w.l2 == l;
}
literal_t watched_literals_t::find_next_watcher(const clause_t& c, const watcher_t& w) {
  for (literal_t l : c) {
    if (watch_contains(w, l)) continue;
    if (trace.actions.literal_true(l)) return l;
  }
  for (literal_t l : c) {
    if (watch_contains(w, l)) continue;
    if (!trace.actions.literal_false(l)) return l;
  }
  return 0;
}


void watched_literals_t::watcher_literal_swap(watcher_t& w, literal_t o, literal_t n) {
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

void watched_literals_t::watcher_list_remove_clause(clause_list_t& clause_list, clause_id cid) {
  auto it = std::remove(std::begin(clause_list), std::end(clause_list), cid);
  SAT_ASSERT(it != std::end(clause_list));
  SAT_ASSERT(it == std::prev(std::end(clause_list)));
  clause_list.erase(it, std::end(clause_list));
}

void watched_literals_t::watcher_swap(clause_id cid, watcher_t& w, literal_t o, literal_t n) {
  clause_list_t& old_list = literals_to_watcher[o];
  clause_list_t& new_list = literals_to_watcher[n];
  SAT_ASSERT(contains(old_list, cid));
  SAT_ASSERT(!contains(new_list, cid));
  // Update our lists data:
  watcher_list_remove_clause(old_list, cid);
  new_list.push_back(cid);
  // Update our watcher data:
  watcher_literal_swap(w, o, n);
}

void watched_literals_t::print_watch_state() const {
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

bool watched_literals_t::clause_watched(clause_id cid) { return watched_literals.find(cid) != watched_literals.end(); }
bool watched_literals_t::validate_state() {
  for (clause_id cid = 0; cid < cnf.size(); cid++) {

    // We are, or aren't, watching this clause, as appropriate.
    const clause_t& c = cnf[cid];
    if (c.size() < 2) {
      SAT_ASSERT(!clause_watched(cid));
      continue;
    }
    if (!clause_watched(cid)) {
      std::cout << "CID not watched: " << cid << " {" << cnf[cid] << "}" << std::endl;
    }
    SAT_ASSERT(clause_watched(cid));

    // The two literals watching us are in the clause.
    const watcher_t w = watched_literals[cid];
    SAT_ASSERT(contains(c, w.l1));
    if (!contains(c, w.l2)) {
      std::cout << "Failing with " << c << " and w: " << w << ", specifically: " << w.l2 << std::endl;
    }
    SAT_ASSERT(contains(c, w.l2));

    // TODO: Something to enforce about expected effect of backtracking
    // (the relative order of watched literals, etc. etc.)

    // I *think* that we have no other invariants, if the clause is sat.
    // I would think that the watched literals shouldn't be pointed to false,
    // but I'm not sure.
    if (trace.clause_sat(cid)) {
      if(trace.actions.literal_false(w.l1) && trace.actions.literal_false(w.l2)) {
        std::cout << "Error, clause is sat, but both watched literals false: #" << cid << "{" << c << "} with watch: " << w << std::endl;
        std::cout << *this << std::endl;
        std::cout << trace << std::endl;
      }
      SAT_ASSERT(!trace.actions.literal_false(w.l1) || !trace.actions.literal_false(w.l2));
      continue;
    }

    // If both watches are false, the clause should be entirely unsat.
    if (trace.actions.literal_false(w.l1) && trace.actions.literal_false(w.l2)) {
      SAT_ASSERT(trace.clause_unsat(cid));
    }
    // If exactly one watch is false, then the clause should be unit.
    else if (trace.actions.literal_false(w.l1) || trace.actions.literal_false(w.l2)) {
      if (trace.count_unassigned_literals(cid) != 1) {
        std::cout << "Failing with unit inconsistencies in: " << std::endl << c << "; " << w << std::endl <<
          *this << std::endl << "With trace state: " << std::endl << trace << std::endl;
      }
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
  return true;
}

std::ostream& operator<<(std::ostream& o, const watcher_t& w) {
  return o << "[" << w.l1 << ", " << w.l2 << "]";
}
std::ostream& operator<<(std::ostream& o, const watched_literals_t& w) {
  w.print_watch_state();
  return o;
}
