#pragma once
#include <utility>
#include <vector>
#include <map>
#include <list>

#include "cnf.h"
#include "watched_literals.h"
#include "action.h"
#include "trail.h"

enum class backtrack_mode_t {
                             simplest,
                             nonchron,
};
enum class learn_mode_t {
                         simplest,
                         explicit_resolution
};
enum class unit_prop_mode_t {
                             simplest,
                               queue,
                               watched
};
enum class variable_choice_mode_t {
  nextliteral,
  nextclause
    };

extern backtrack_mode_t backtrack_mode;
extern learn_mode_t learn_mode;
extern unit_prop_mode_t unit_prop_mode;
extern variable_choice_mode_t variable_choice_mode;

struct trace_t {
private:
public:
  trail_t actions;
  void print_actions(std::ostream&) const;

  cnf_t& cnf;

  // this is hugely expensive data structure. We'll see.
  std::map<literal_t, std::vector<clause_id>> literal_to_clause;

  // store the unit-props we're still getting through.
  std::list<action_t> units;

  watched_literals_t watch;


  void reset();

  trace_t(cnf_t& cnf);

  static bool halt_state(const action_t action);
  bool halted() const;
  bool final_state();
  bool clause_sat(const clause_t& clause) const;
  bool clause_sat(clause_id cid) const;
  bool clause_unsat(const clause_t& clause) const;
  bool clause_unsat(clause_id cid) const;
  auto unsat_clause() const;
  bool cnf_unsat() const;
  size_t count_unassigned_literals(const clause_t& clause) const;
  size_t count_unassigned_literals(clause_id cid) const;
  literal_t find_unassigned_literal(const clause_t& clause) const;
  literal_t find_unassigned_literal(clause_id cid) const;
  bool unit_clause_exists() const;
  bool cnf_sat() const;
  void push_unit_queue(literal_t l, clause_id cid);
  void clear_unit_queue();
  void clean_unit_queue();
  void push_conflict(clause_id cid);
  bool has_conflict() const;
  void push_sat();
  void push_unsat();
  void register_false_literal(literal_t l);
  void apply_literal(literal_t l);
  void apply_decision(literal_t l);
  void apply_unit(literal_t l, clause_id cid);
  literal_t decide_literal();
  std::pair<literal_t, clause_id> prop_unit();
  void backtrack(const clause_t& c);
  void add_clause(const clause_t& c);
  bool verify_resolution_expected(const clause_t& c);
  size_t count_true_literals(const clause_t& clause) const;
  size_t count_false_literals(const clause_t& clause) const;
  literal_t find_last_falsified(clause_id cid);
};


std::ostream& operator<<(std::ostream& o, const trace_t& t);
