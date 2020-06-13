#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <list>
#include <map>
#include <queue>
#include <vector>

#include <cstring>

#include <getopt.h>

#include "cnf.h"

#include "debug.h"
#include "preprocess.h"
#include "trace.h"
#include "watched_literals.h"

#include "backtrack.h"
#include "bce.h"
#include "clause_learning.h"
#include "lcm.h"

#include "lbm.h"

#include "subsumption.h"

// Help with control flow and such...
#include "plugins.h"

// Debugging and experiments.
#include "visualizations.h"

// The understanding of this project is that it will evolve
// frequently. We'll first start with the basic data structures,
// and then refine things as they progress.

namespace counters {
size_t restarts = 0;
size_t conflicts = 0;
size_t decisions = 0;
size_t propagations = 0;
// learned clause size histogram
std::map<int, int> lcsh;
void print() {
  std::cout << "Restarts: " << restarts << std::endl;
  std::cout << "Conflicts: " << conflicts << std::endl;
  std::cout << "Decisions: " << decisions << std::endl;
  std::cout << "Propagations: " << propagations << std::endl;
  int max_index = 0;
  int max_value = 0;
  int total_count = 0;
  int total_length = 0;
  for (auto&& [l, c] : lcsh) {
    max_index = std::max(l, max_index);
    max_value = std::max(c, max_value);
    total_count += c;
    total_length += c * l;
  }
  for (int i = 0; i < 10; i++) {
    for (int j = 0; j < max_index; j++) {
      int count = lcsh[j];
      float normalized = float(100 * count) / float(total_count);
      if (normalized >= 10 - i) {
        std::cout << "#  ";
      } else {
        std::cout << "   ";
      }
    }
    std::cout << std::endl;
  }
  for (int j = 0; j < max_index; j++) {
    std::cout << j;
    if (j < 10) {
      std::cout << " ";
    }
    if (j < 100) {
      std::cout << " ";
    }
  }
  std::cout << std::endl;
  std::cout << "Average learned clause length: " << float(total_length) << " "
            << float(conflicts) << std::endl;
  std::cout << "Average learned clause length: "
            << float(total_length) / float(conflicts) << std::endl;
}
}  // namespace counters

// The perspective I want to take is not one of deriving an assignment,
// but a trace exploring the recursive, DFS space of assignments.
// We don't forget anything -- I even want to record backtracking.
// So let's see what this looks like.

// these are our global settings
bool optarg_match(const char* o, const char* s1, const char* s2) {
  return strcmp(o, s1) == 0 || strcmp(o, s2) == 0;
}
void process_flags(int argc, char* argv[]) {
  for (;;) {
    static struct option long_options[] = {
        {"backtracking", required_argument, nullptr, 'b'},
        {"learning", required_argument, nullptr, 'l'},
        {"unitprop", required_argument, nullptr, 'u'},
        {nullptr, 0, nullptr, 0}};
    int option_index = 0;
    int c = getopt_long(argc, argv, "b:l:u:", long_options, &option_index);

    if (c == -1) {
      break;
    }

    switch (c) {
      case 'b':
        if (optarg_match(optarg, "simplest", "s")) {
          backtrack_mode = backtrack_mode_t::simplest;
        } else if (optarg_match(optarg, "nonchron", "n")) {
          backtrack_mode = backtrack_mode_t::nonchron;
        }
        break;
      case 'l':
        if (optarg_match(optarg, "simplest", "s")) {
          learn_mode = learn_mode_t::simplest;
        } else if (optarg_match(optarg, "resolution", "r")) {
          learn_mode = learn_mode_t::explicit_resolution;
        }
        break;
      case 'u':
        if (optarg_match(optarg, "simplest", "s")) {
          unit_prop_mode = unit_prop_mode_t::simplest;
        } else if (optarg_match(optarg, "queue", "q")) {
          unit_prop_mode = unit_prop_mode_t::queue;
        } else if (optarg_match(optarg, "watched", "w")) {
          unit_prop_mode = unit_prop_mode_t::watched;
        }
        break;
    }
  }
  // std::cerr << "Settings are: " << static_cast<int>(learn_mode) << " " <<
  // static_cast<int>(backtrack_mode) << " " << static_cast<int>(unit_prop_mode)
  // << std::endl;
}

// This is the start of refactoring things to get a solver into its own object,
// to facilitate the (possibility) of having multiple solvers, etc. etc.

plugin<literal_t> apply_literal;
plugin<clause_id> remove_clause;
plugin<clause_id> clause_added;
// This is the moment we enter the conflict.
plugin<const cnf_t&, trail_t&> on_conflict;
plugin<cnf_t&> before_decision;
// this is before we commit the learned clause to the CNF.
plugin<clause_t&, trail_t&> learned_clause;
plugin<clause_id, literal_t> remove_literal;
plugin<clause_id, literal_t> unit_detected;

// Plug in all the listeners for watched literals:
void install_watched_literals(trace_t& trace) {
  // When we apply a literal, we need to tell the watched literals so it
  // can propogate the effects of -l being unsat.
  apply_literal.add_listener([&trace](literal_t l) {
    trace.watch.literal_falsed(l);
    SAT_ASSERT(trace.halted() || trace.watch.validate_state());
  });

  // When a clause is removed, we want to (if our TWL is watching it at all)
  // remove it!
  remove_clause.add_listener(
      [&trace](clause_id cid) { trace.watch.remove_clause(cid); });

  // When a clause is added, if it's big enough to watch, we should.
  clause_added.add_listener([&trace](clause_id cid) {
    const clause_t& c = trace.cnf[cid];
    if (c.size() > 1) trace.watch.watch_clause(cid);
    SAT_ASSERT(trace.watch.validate_state());
  });

  remove_literal.add_listener([&trace](clause_id cid, literal_t l) {
    trace.watch.remove_clause(cid);
    if (trace.cnf[cid].size() > 1) {
      trace.watch.watch_clause(cid);
    }
  });

  clause_added.postcondition([&trace](clause_id cid) {
                               SAT_ASSERT(trace.watch.validate_state());

                               // only in nonchron case...
                               SAT_ASSERT(trace.count_unassigned_literals(trace.cnf[cid]) == 1);
                               SAT_ASSERT(trace.actions.literal_unassigned(trace.cnf[cid][0]));
                             });
}

std::unique_ptr<lbm_t> lbm;
void install_lbm(trace_t& trace) {
  lbm = std::make_unique<lbm_t>(trace.cnf);

  // Every time before we make a new decision, LBM should
  // have a chance to go.
  auto enact_lbm_cleaning = [&trace](cnf_t& cnf) {
    if (lbm->should_clean(cnf)) {
      auto to_remove = lbm->clean(remove_clause);
      auto et = std::remove_if(
          std::begin(to_remove), std::end(to_remove),
          [&](clause_id cid) { return trace.actions.uses_clause(cid); });
      to_remove.erase(et, std::end(to_remove));
      for (clause_id cid : to_remove) {
        remove_clause(cid);
      }
    }
  };

  before_decision.add_listener(enact_lbm_cleaning);

  // This is a pretty roundabout way...
  // Is needed to keep LBM consistent with other clause-removal strategies
  // (Or we can have remove_clause be more robust to repeated elements.)
  remove_clause.add_listener([](clause_id cid) {
    auto it = std::find_if(std::begin(lbm->worklist), std::end(lbm->worklist),
                           [cid](const lbm_entry& e) { return e.id == cid; });
    if (it != std::end(lbm->worklist)) {
      std::iter_swap(it, std::prev(std::end(lbm->worklist)));
      lbm->worklist.pop_back();
    }
  });

  // Actually caching the LBM value is a bit janky: we key things in by
  // their clause-id, but that's only computed *after* we backtrack.
  // So what we do is right after having learned the conflict, we save
  // an (anonymous) LBM value.
  //
  // Then, we listen in to the next clause to be added to the CNF. When
  // *that* happens, we get the key we want to associate things with.

  // be careful to do this only after learned-clause minimization
  // TODO(aaron): on uuf250-01.cnf, removing *any* sequence of clauses
  // seems to be enough, the LBM score is there and is used, but doesn't
  // actually improve the runtime.
  learned_clause.add_listener(
      [](clause_t& c, trail_t& trail) { lbm->push_value(c, trail); });

  clause_added.add_listener([](clause_id cid) { lbm->flush_value(cid); });
}

void install_lcm(const cnf_t& cnf) {
  learned_clause.add_listener([&cnf](clause_t& c, trail_t& trail) {
    learned_clause_minimization(cnf, c, trail);
  });
}
void install_lcm_subsumption(cnf_t& cnf) {
  learned_clause.add_listener([&cnf](clause_t& c, trail_t& trail) {
    // MINIMIZE BY SUBSUMPTION
    std::for_each(std::begin(cnf), std::end(cnf), [&cnf](clause_id cid) {
      std::sort(std::begin(cnf[cid]), std::end(cnf[cid]));
    });

    literal_map_t<clause_set_t> literal_to_clause(cnf);
    for (clause_id cid : cnf) {
      const clause_t& c = cnf[cid];
      for (literal_t l : c) {
        literal_to_clause[l].push_back(cid);
      }
    }
    // for everything in our new clause:
    size_t orig_c = c.size();
    for (int i = 0; i < c.size(); i++) {
      literal_t l = c[i];
      // all possible resolvents
      auto cids = literal_to_clause[neg(l)];
      for (clause_id cid : cids) {
        auto d = cnf[cid];
        auto it = std::find(std::begin(d), std::end(d), neg(l));
        *it = neg(*it);
        std::sort(std::begin(d), std::end(d));
        // this means we can resolve the original d against c,
        // and we'd end up with a clause just like c, but missing
        // l.
        if (subsumes(d, c)) {
          auto et = std::remove(std::begin(c), std::end(c), l);
          assert(std::distance(et, std::end(c)) == 1);
          c.erase(et, std::end(c));
          i--;  // go back one.
        }

        *it = neg(*it);
        std::sort(std::begin(d), std::end(d));
      }
    }
    // if (orig_c > c.size())
    // std::cerr << "[SUBSUMPTION] Shrank learned clause from " << orig_c
    //<< " to " << c.size() << std::endl;
  });
}

struct naive_cleaner {
  const cnf_t& cnf;
  const trail_t& trail;
  std::vector<literal_t> seen;
  literal_map_t<clause_set_t> m;
  naive_cleaner(const trace_t& trace)
      : cnf(trace.cnf), trail(trace.actions), m(build_incidence_map(cnf)) {}
  bool clean(literal_t l) {
    if (contains(seen, l)) return false;
    seen.push_back(l);
    auto cl = m[l];
    auto et = std::remove_if(std::begin(cl), std::end(cl), [&](clause_id cid) {
      return std::any_of(std::begin(trail), std::end(trail), [&](action_t a) {
        return a.has_clause() && a.get_clause() == cid;
      });
    });
    std::for_each(std::begin(cl), et, remove_clause);
    const auto dl = m[neg(l)];
    std::for_each(std::begin(dl), std::end(dl),
                  [&](clause_id cid) { remove_literal(cid, neg(l)); });
    return true;
  }
  void clause_added_listener(clause_id cid) {
    const clause_t& c = cnf[cid];
    for (literal_t l : c) {
      m[l].push_back(cid);
    }
  }
  void clause_removed_listener(clause_id cid) {
    const clause_t& c = cnf[cid];
    for (literal_t l : c) {
      m[l].remove(cid);
    }
  }
  void remove_literal_listener(clause_id cid, literal_t l) { m[l].remove(cid); }
};

void install_naive_cleaning(trace_t& trace, naive_cleaner& c) {
  before_decision.add_listener([&trace, &c](cnf_t& cnf) {
    literal_t cand = 0;
    for (action_t a : trace.actions) {
      if (a.is_unit_prop()) {
        literal_t l = a.get_literal();
        clause_id cid = a.get_clause();
        if (cnf[cid].size() == 1) {
          if (c.clean(l)) {
            break;
          }
        }
      }
    }
  });
  before_decision.precondition(
      [&c](const cnf_t& cnf) { SAT_ASSERT(check_incidence_map(c.m, cnf)); });
  before_decision.postcondition(
      [&c](const cnf_t& cnf) { SAT_ASSERT(check_incidence_map(c.m, cnf)); });
  clause_added.add_listener(
      [&c](clause_id cid) { c.clause_added_listener(cid); });
  remove_clause.add_listener(
      [&c](clause_id cid) { c.clause_removed_listener(cid); });
  remove_literal.add_listener(
      [&c](clause_id cid, literal_t l) { c.remove_literal_listener(cid, l); });
}


// The real goal here is to find conflicts as fast as possible.
int main(int argc, char* argv[]) {
  // Instantiate our CNF object
  cnf_t cnf = load_cnf(std::cin);
  SAT_ASSERT(cnf.live_clause_count() > 0);  // make sure parsing worked.

  preprocess(cnf);

  // TODO(aaron): fold this into a more general case, if possible.
  if (immediately_unsat(cnf)) {
    std::cout << "UNSATISFIABLE" << std::endl;
    return 0;
  }
  if (immediately_sat(cnf)) {
    std::cout << "SATISFIABLE" << std::endl;
    return 0;
  }

  SAT_ASSERT(!find_unit(cnf));

  process_flags(argc, argv);

  trace_t trace(cnf);

  // This is the actual literal-removal, needs to happen first.
  remove_literal.add_listener([&trace](clause_id cid, literal_t l) {
    clause_t& c = trace.cnf[cid];
    auto et = std::remove(std::begin(c), std::end(c), l);
    c.erase(et, std::end(c));

    // And do the unit prop.
    if (c.size() == 1) {
      trace.push_unit_queue(c[0], cid);
    }
  });

  enum class solver_state_t { quiescent, check_units, conflict, sat, unsat };
  solver_state_t state = solver_state_t::quiescent;

  if (unit_prop_mode == unit_prop_mode_t::watched) {
    install_watched_literals(trace);
  }

  install_lcm(cnf);
  install_lbm(trace);

  // We will see if we have new units, and if so,
  // apply them to clean up the CNF
  //naive_cleaner naive_clean(trace);
  //install_naive_cleaning(trace, naive_clean);

  // Adding some invariants for correctness.
  apply_literal.precondition([&trace](literal_t l) {
    SAT_ASSERT(std::find_if(std::begin(trace.cnf), std::end(trace.cnf),
                            [&](const clause_id cid) {
                              const clause_t& c = trace.cnf[cid];
                              return c.size() == 1 && contains(c, neg(l));
                            }) == std::end(trace.cnf));
  });

  remove_clause.precondition([&trace](clause_id cid) {
    for (action_t a : trace.actions) {
      if (a.has_clause()) {
        SAT_ASSERT(a.get_clause() != cid);
      }
    }
  });
  remove_clause.add_listener([&cnf](clause_id cid) { cnf.remove_clause(cid); });

  int counter = 0;

  clause_added.postcondition([&cnf](clause_id cid) {
    // if (cnf[cid].size() == 2) {
    // std::cerr << "SIZE 2!" << std::endl;
    //}
  });

  for (;;) {
    // std::cerr << "State: " << static_cast<int>(state) << std::endl;
    // std::cerr << "Trace: " << trace << std::endl;
    // trace.watch.print_watch_state();
    switch (state) {
      case solver_state_t::quiescent: {
        // this is way too slow
        if (false) {
          auto blocked_clauses = BCE(cnf);
          if (!blocked_clauses.empty()) {
            auto et = std::end(blocked_clauses);
            for (const action_t& a : trace.actions) {
              if (a.has_clause()) {
                et = std::remove(std::begin(blocked_clauses), et,
                                 a.get_clause());
              }
            }
            blocked_clauses.erase(et, std::end(blocked_clauses));

            std::cerr << "[BCE] Blocked clauses found after learning: "
                      << blocked_clauses.size() << std::endl;
            for (auto cid : blocked_clauses) {
              remove_clause(cid);
            }
          }
        }

        // Transform the state before making a decision
        before_decision(cnf);

        counters::decisions++;

        // naive constant folding may add units.
        if (trace.units.empty()) {
          literal_t l = trace.decide_literal();
          if (l != 0) {
            trace.apply_decision(l);
            apply_literal(l);
          }
        }

        if (trace.final_state()) {
          state = solver_state_t::sat;
        } else {
          state = solver_state_t::check_units;
        }
        break;
      }

      case solver_state_t::check_units:

        for (;;) {
          auto [l, cid] = trace.prop_unit();
          counters::propagations++;
          // std::cout << "Found unit prop: " << l << " " << cid << std::endl;
          if (l == 0) {
            break;
          }
          trace.apply_unit(l, cid);
          apply_literal(l);
          if (trace.halted()) {
            break;
          }
        }

        if (trace.halted()) {
          state = solver_state_t::conflict;
        } else {
          state = solver_state_t::quiescent;
        }

        break;

      case solver_state_t::conflict: {
        counters::conflicts++;

        // print_conflict_graph(cnf, trace.actions);
        clause_t c = learn_clause(cnf, trace.actions);

        // Minimize, cache lbm score, etc.
        learned_clause(c, trace.actions);

        counters::lcsh[c.size()]++;

        // early out in the unsat case.
        if (c.empty()) {
          state = solver_state_t::unsat;
          break;
        }

        backtrack(c, trace.actions);

        trace.clear_unit_queue();  // ???
        trace.vsids.clause_learned(c);

        cnf_t::clause_k key = trace.add_clause(c);

        // Commit the clause, the LBM score of that clause, and so on.
        clause_added(key);
        trace.push_unit_queue(cnf[key][0], key);

        if (trace.final_state()) {
          state = solver_state_t::unsat;
        } else {
          state = solver_state_t::check_units;
        }

        break;
      }

      case solver_state_t::sat:
        counters::print();
        std::cout << "SATISFIABLE" << std::endl;
        return 0;  // quit entirely
      case solver_state_t::unsat:
        counters::print();
        std::cout << "UNSATISFIABLE" << std::endl;
        return 0;  // quit entirely
    }
  }
}
