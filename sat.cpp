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

#if 0
// This captures the whole state of a solver instance.
struct solver_t {

  // These plugins are the sequence of things to do for a specific action.
  // The "default" action is to at least touch the fundamental data structures:
  // the CNF and the trail. Even that's manual, though.
  plugin<literal_t> apply_literal;
  plugin<clause_id> remove_clause;
  // This is the moment we enter the conflict.
  plugin<const cnf_t&, trail_t&> on_conflict;
  // this is before we commit the learned clause to the CNF.
  plugin<clause_t&, trail_t&> learned_clause;

  enum class state_t { quiescent, check_units, conflict, sat, unsat };
  solver_state_t s = state_t::quiescent;

  // Construct all the different things
  solver_t(cnf_t& cnf): cnf(cnf) {
  }

  // This is the "go" method.
  void solve() {
    for (;;) {
      switch (s) {
      case state_t::quiescent: {
        enter_quiescent(cnf);
        literal_t l = decide_literal();
        apply_literal(l);

        if (trace.final_state()) {
          s = state_t::sat;
        }
        else {
          s = state_t::check_units;
        }
      }
      }
    }
  }
}
#endif

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
  remove_clause.add_listener([&trace](clause_id cid) {
    if (trace.watch.clause_watched(cid)) {
      trace.watch.remove_clause(cid);
    }
  });

  // When a clause is added, if it's big enough to watch, we should.
  clause_added.add_listener([&trace](clause_id cid) {
    const clause_t& c = trace.cnf[cid];
    if (c.size() > 1) trace.watch.watch_clause(cid);
    SAT_ASSERT(trace.watch.validate_state());
  });
}

std::unique_ptr<lbm_t> lbm;
void install_lbm(trace_t& trace) {
  lbm = std::make_unique<lbm_t>(trace.cnf);

  // Every time before we make a new decision, LBM should
  // have a chance to go.
  before_decision.add_listener([&trace](cnf_t& cnf) {
    if (lbm->should_clean(cnf)) {
      auto cids_to_remove = lbm->clean(cnf);
      // std::cerr << "Cnf size = " <<  cnf.live_clause_count() << std::endl;
      // for (auto cid : cnf) { std::cout << cnf[cid] << " " <<
      // lbm.lbm[cid] << std::endl;
      //}
      // std::cout << "====================Clauses to remove: " <<
      // cids_to_remove.size() << std::endl;

      // Don't remove anything on the trail.
      auto et = std::end(cids_to_remove);
      for (const action_t& a : trace.actions) {
        if (a.has_clause()) {
          et = std::remove(std::begin(cids_to_remove), et, a.get_clause());
        }
      }
      cids_to_remove.erase(et, std::end(cids_to_remove));

      for (clause_id cid : cids_to_remove) {
        // THIS IS ANOTHER PLUGIN
        remove_clause(cid);
      }
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

    literal_incidence_map_t literal_to_clause(cnf);
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
      auto cids = literal_to_clause[-l];
      for (clause_id cid : cids) {
        auto d = cnf[cid];
        auto it = std::find(std::begin(d), std::end(d), -l);
        *it = -*it;
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

        *it = -*it;
        std::sort(std::begin(d), std::end(d));
      }
    }
    // if (orig_c > c.size())
    // std::cerr << "[SUBSUMPTION] Shrank learned clause from " << orig_c
    //<< " to " << c.size() << std::endl;
  });
}

void install_naive_cleaning(trace_t& trace) {
  before_decision.add_listener([&trace](cnf_t cnf) {
    if (trace.actions.level() == 0) {
      counters::restarts++;

      std::vector<clause_id> satisfied;
      for (action_t a : trace.actions) {
        if (a.has_literal()) {
          literal_t l = a.get_literal();
          for (clause_id cid : trace.literal_to_clause[l]) {
            satisfied.push_back(cid);
          }
        }
      }
      std::sort(std::begin(satisfied), std::end(satisfied));
      auto it = std::unique(std::begin(satisfied), std::end(satisfied));
      satisfied.erase(it, std::end(satisfied));

      // Don't remove anything on the trail.
      auto et = std::end(satisfied);
      for (const action_t& a : trace.actions) {
        if (a.has_clause()) {
          et = std::remove(std::begin(satisfied), et, a.get_clause());
        }
      }
      satisfied.erase(et, std::end(satisfied));

      for (clause_id cid : satisfied) {
        remove_clause(cid);
      }
    }
  });
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

  enum class solver_state_t { quiescent, check_units, conflict, sat, unsat };
  solver_state_t state = solver_state_t::quiescent;

  if (unit_prop_mode == unit_prop_mode_t::watched) {
    install_watched_literals(trace);
  }

  // Deliberate bug: we will compute the LBM score before LCM,
  // so the score will presumably be much worse.
  install_lcm(cnf);
  install_lbm(trace);

  // install_naive_cleaning(trace);

  // Adding some invariants for correctness.
  apply_literal.precondition([&trace](literal_t l) {
    SAT_ASSERT(std::find_if(std::begin(trace.cnf), std::end(trace.cnf),
                            [&](const clause_id cid) {
                              const clause_t& c = trace.cnf[cid];
                              return c.size() == 1 && contains(c, -l);
                            }) == std::end(trace.cnf));
  });

  if (unit_prop_mode == unit_prop_mode_t::queue) {
    apply_literal.add_listener([&trace](literal_t l) {
      const auto& clause_ids = trace.literal_to_clause[-l];
      for (auto clause_id : clause_ids) {
        const auto& c = trace.cnf[clause_id];
        if (trace.clause_sat(c)) {
          continue;
        }
        // all_sat = false;
        if (contains(c, -l)) {
          if (trace.clause_unsat(c)) {
            trace.push_conflict(clause_id);
            return;
          }
          if (trace.count_unassigned_literals(c) == 1) {
            literal_t p = trace.find_unassigned_literal(c);
            trace.push_unit_queue(p, clause_id);
          }
        }
      }
    });
  } else if (unit_prop_mode == unit_prop_mode_t::simplest) {
    apply_literal.add_listener([&trace](literal_t l) {
      for (clause_id cid : trace.cnf) {
        const clause_t& c = trace.cnf[cid];
        if (contains(c, -l)) {
          if (trace.clause_unsat(c)) {
            // std::cout << "Found conflict: " << c << std::endl;
            trace.push_conflict(cid);
            return;
          }
        } else {
          SAT_ASSERT(!trace.clause_unsat(c));
        }
      }
    });
  }

  remove_clause.add_listener([&cnf](clause_id cid) { cnf.remove_clause(cid); });

  remove_clause.add_listener([&trace](clause_id cid) {
    const clause_t& c = trace.cnf[cid];
    for (literal_t l : c) {
      auto& incidence_list = trace.literal_to_clause[l];
      SAT_ASSERT(contains(incidence_list, cid));
      auto et = std::remove(std::begin(incidence_list),
                            std::end(incidence_list), cid);
      incidence_list.erase(et, std::end(incidence_list));
    }
  });

  int counter = 0;

  for (;;) {
    // std::cout << "State: " << static_cast<int>(state) << std::endl;
    // std::cout << "Trace: " << trace << std::endl;
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

        before_decision(cnf);

        /* RESTART, DON"T.
    counter++;
    if (counter > 1000) {
      while (trace.actions.level()) {
        trace.actions.pop();
      }
      counter = 0;
      //size_t total_strengthened = naive_self_subsume(cnf);
      //if (total_strengthened) std::cerr << "[NSS] " << total_strengthened
    << std::endl;
    }
    */

        //  #if 0
        //  std::for_each(std::begin(cnf), std::end(cnf), [&cnf](clause_id cid)
        //  {
        //                                                  std::sort(std::begin(cnf[cid]),
        //                                                  std::end(cnf[cid]));
        //                                                  });
        //  for (auto cid : cnf) {
        //    clause_t& c = cnf[cid];
        //    for (size_t i = 0; i < c.size(); i++) {
        //      c[i] = -c[i];
        //      std::sort(std::begin(c), std::end(c));
        //      auto subsumes = find_subsumed(cnf, c);
        //      for (auto did: subsumes) {
        //        clause_t d = cnf[did];
        //        std::cerr << "Strengthening " << d << " into ";
        //        assert(contains(d, c[i]));
        //        auto dt = std::remove(std::begin(d), std::end(d), c[i]);
        //        d.erase(dt, std::end(d));
        //        std::cerr << d << " thanks to " << c << "(with the " << i <<
        //        "th element negated)" << std::endl;
        //      }
        //      c[i] = -c[i];
        //      std::sort(std::begin(c), std::end(c));
        //    }
        //  }
        //  #endif
        //}
        counters::decisions++;

        literal_t l = trace.decide_literal();
        if (l != 0) {
          trace.apply_decision(l);
          apply_literal(l);
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
        if (unit_prop_mode == unit_prop_mode_t::watched)
          SAT_ASSERT(trace.watch.validate_state());

        if (unit_prop_mode == unit_prop_mode_t::queue ||
            unit_prop_mode == unit_prop_mode_t::watched) {
          if (backtrack_mode == backtrack_mode_t::nonchron) {
            assert(trace.count_unassigned_literals(c) == 1);
          }
          if (trace.count_unassigned_literals(c) == 1) {
            literal_t l = trace.find_unassigned_literal(c);
            trace.push_unit_queue(l, key);
          }
        }

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
