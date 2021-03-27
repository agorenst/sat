#include "cnf.h"
#include "lbm.h"
#include "plugins.h"
#include "unit_queue.h"
#include "vsids.h"
#include "watched_literals.h"

enum class backtrack_mode_t {
  simplest,
  nonchron,
};
enum class unit_prop_mode_t { simplest, queue, watched };
enum class variable_choice_mode_t { nextliteral, nextclause };

extern backtrack_mode_t backtrack_mode;
extern unit_prop_mode_t unit_prop_mode;
extern variable_choice_mode_t variable_choice_mode;

struct solver_t {
  // The root data structure, the "true" CNF.
  // Currently we'll add learned clauses directly to this.
  // (Rather than having a paritition of "original" clauses
  // and "learned" clauses)
  cnf_t cnf;

  // This is a helper data structure that maintains a mapping of
  // literals to *all* clauses containing those literals. Much
  // more expensive than watched literals.
  literal_map_t<clause_set_t> literal_to_clauses_complete;

  //
  lit_bitset_t stamped;

  // This is the history of committed actions we have.
  // This is the thing to ask if you need to know if a literal
  // is true, false, or unassigned (or its level, etc. etc.)
  trail_t trail;

  // This is the TWL scheme first implemented in chaff.
  watched_literals_t watch;

  // This is our queue of "pending" units that we need to prop.
  // We separate this rather than put things on the trail directly
  // originally to keep things simple. I wonder if we can adjust our
  // model so that this is unecessary.
  unit_queue_t unit_queue;

  // This VSIDS object is how we choose our literals.
  vsids_t vsids;

  // Our main clause-removal heuristic:
  lbm_t lbm;

  // We model our solver as a state machine.
  // These are the fundamental states it can be in:
  enum class state_t { quiescent, check_units, conflict, sat, unsat };
  state_t state = state_t::quiescent;

  // To keep things easy to experiment (and conceptually understand),
  // we have our core solver loop that, at certain major points, calls
  // into a sequence of handlers. These handlers are stored in these plugins.
  static const bool use_plugins = false;
  plugin<cnf_t &> before_decision_p;
  plugin<literal_t> apply_decision_p;
  plugin<literal_t, clause_id> apply_unit_p;
  plugin<clause_id> remove_clause_p;
  plugin<clause_id> clause_added_p;
  plugin<clause_t &, trail_t &> learned_clause_p;
  plugin<clause_id, literal_t> remove_literal_p;
  plugin<> restart_p;
  plugin<literal_t &> choose_literal_p;
  plugin<> start_solve_p;
  plugin<> end_solve_p;

  plugin<> print_metrics_plugins_p;

  // And instead, let's try doing function stuff.
  void before_decision(cnf_t &);
  void apply_unit(literal_t, clause_id);
  void apply_decision(literal_t);
  void remove_clause(clause_id);
  void clause_added(clause_id);
  void learned_clause(clause_t &, trail_t &);
  void remove_literal(clause_id, literal_t);
  void restart();
  void choose_literal(literal_t &);
  void start_solve();
  void end_solve();

  void backtrack_subsumption(clause_t &c, action_t *a, action_t *e);

  // These install the fundamental actions.
  void install_core_plugins();

  void install_complete_tracker();

  void install_watched_literals();
  void install_lcm();
  void install_lbm();
  void install_restart();
  void install_literal_chooser();

  // These install various counters to track interesting things.
  void install_metrics_plugins();

  // We create a local copy of the CNF.
  solver_t(const cnf_t &cnf);
  // This is the core method:
  bool solve();

  void report_metrics();

  void naive_cleaning();

  bool halt_state(const action_t action) const {
    return action.action_kind == action_t::action_kind_t::halt_conflict ||
           action.action_kind == action_t::action_kind_t::halt_unsat ||
           action.action_kind == action_t::action_kind_t::halt_sat;
  }

  bool halted() const {
    // std::cout << "Debug: current trace is: " << *this << std::endl;
    bool result = !trail.empty() && halt_state(*trail.crbegin());
    // std::cout << "[DBG](halted): " << result << " with " <<
    // *actions.crbegin()
    // << std::endl;
    return result;
  }
  bool final_state() {
    // std::cout << "Testing final state: " << *this << std::endl;
    if (trail.empty()) {
      return false;
    }
    action_t action = *trail.rbegin();

    return action.action_kind == action_t::action_kind_t::halt_unsat ||
           action.action_kind == action_t::action_kind_t::halt_sat;
  }

  struct ema_restart_t {
    // For restarts...
    const float alpha_fast = 1.0 / 32.0;
    const float alpha_slow = 1.0 / 4096.0;
    const float c = 1.25;

    float alpha_incremental = 1;
    int counter = 0;
    float ema_fast = 0;
    float ema_slow = 0;

    void reset() {
      alpha_incremental = 1;
      counter = 0;
      ema_fast = 0;
      ema_slow = 0;
    }
    bool should_restart() const {
      if (counter < 50) return false;
      if (ema_fast > c * ema_slow) return true;
      return false;
    }
    void step(const size_t lbd) {
      // Update the emas

      if (alpha_incremental > alpha_fast) {
        ema_fast =
            alpha_incremental * lbd + (1.0 - alpha_incremental) * ema_fast;
      } else {
        ema_fast = alpha_fast * lbd + (1.0 - alpha_fast) * ema_fast;
      }

      if (alpha_incremental > alpha_slow) {
        ema_slow =
            alpha_incremental * lbd + (1.0 - alpha_incremental) * ema_slow;
      } else {
        ema_slow = alpha_slow * lbd + (1.0 - alpha_slow) * ema_slow;
      }
      alpha_incremental *= 0.5;

      // std::cerr << lbd << "; " << ema_fast << "; " << ema_slow <<
      // std::endl;
      counter++;
    }
  };
  ema_restart_t ema_restart;
};
