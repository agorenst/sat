#include <cstdio>
#include <iostream>
#include <cstdint>
#include <vector>
#include <algorithm>
#include <cassert>

// TODO: Change clause_id to an iterator?

// The understanding of this project is that it will evolve
// frequently. We'll first start with the basic data structures,
// and then refine things as they progress.

// This is how we represent literals
typedef int32_t literal_t;
// Really, we can be clever and use unsigned, but come on.
typedef int32_t variable_t;



// Our initial data structures are really stupid.
typedef std::vector<literal_t> clause_t;
std::ostream& operator<<(std::ostream& o, const clause_t& c) {
  for (auto l : c) {
    o << l << " ";
  }
  return o;
}

variable_t max_variable = 0;

typedef std::vector<clause_t> cnf_t;
typedef size_t clause_id;

// This is our CNF object. It will contain a few helper methods.
/*
struct cnf_t {
  typedef std::vector<clause_t> clause_database_t;
  clause_database_t clause_database;
  typedef size_t clause_id;
};
*/

// The perspective I want to take is not one of deriving an assignment,
// but a trace exploring the recursive, DFS space of assignments.
// We don't forget anything -- I even want to record backtracking.
// So let's see what this looks like.
struct trace_t;
std::ostream& operator<<(std::ostream& o, const trace_t t);
struct trace_t {

  // This is the various kinds of actions we can record.
  struct action_t {

    // TODO: need to have an indicator of which state we are...
    enum class action_kind_t {
                        decision,
                        unit_prop,
                        backtrack,
                        halt_conflict,
                        halt_unsat,
                        halt_sat
    };
    action_kind_t action_kind;

    // Absent any unit clauses, we choose a literal.
    union {
      literal_t decision_literal;
      struct {
        literal_t propped_literal;
        clause_id reason; // maybe multiple reasons?
      } unit_prop;
    };
  };

  enum variable_state_t {
                       unassigned,
                       unassigned_to_true,
                       unassigned_to_false,
                       swap_to_true,
                       swap_to_false
  };


  const cnf_t& cnf;
  std::vector<action_t> actions;
  std::vector<variable_state_t> variable_state;

  trace_t(const cnf_t& cnf): cnf(cnf) {
    variable_t max_var = 0;
    for (auto& clause : cnf) {
      for (auto& literal : clause) {
        max_var = std::max(max_var, std::abs(literal));
      }
    }
    variable_state.resize(max_var+1);
    std::fill(std::begin(variable_state), std::end(variable_state), unassigned);
  }

  static bool halt_state(const action_t action) {
    return action.action_kind == trace_t::action_t::action_kind_t::halt_conflict ||
      action.action_kind == trace_t::action_t::action_kind_t::halt_unsat ||
      action.action_kind == trace_t::action_t::action_kind_t::halt_sat;
  }
  bool halted() const {
    std::ostream& operator<<(std::ostream& o, const trace_t t);
    return !actions.empty() && halt_state(*actions.rbegin());
  }

  bool literal_true(const literal_t l) const {
    variable_t v = l > 0 ? l : -l;
    if (l > 0) {
      if (variable_state[v] == unassigned_to_true ||
          variable_state[v] == swap_to_true) {
        return true;
      }
    } else {
      if (variable_state[v] == unassigned_to_false ||
          variable_state[v] == swap_to_false) {
        return true;
      }
    }
    return false;
  }
  bool literal_false(const literal_t l) const {
    variable_t v = l > 0 ? l : -l;
    if (l < 0) {
      if (variable_state[v] == unassigned_to_true ||
          variable_state[v] == swap_to_true) {
        return true;
      }
    } else {
      if (variable_state[v] == unassigned_to_false ||
          variable_state[v] == swap_to_false) {
        return true;
      }
    }
    return false;
  }

  bool literal_unassigned(const literal_t l) const {
    variable_t v = l > 0 ? l : -l;
    return variable_state[v] == unassigned;
  }

  bool clause_sat(const clause_t& clause) const {
    return std::any_of(std::begin(clause), std::end(clause), [this](auto& c) {return this->literal_true(c);});
  }
  bool clause_sat(clause_id cid) const {
    return clause_sat(cnf[cid]);
  }

  // this means, stricly, that all literals are false (not just that none are true)
  bool clause_unsat(const clause_t& clause) const {
    return std::all_of(std::begin(clause), std::end(clause), [this](auto& l) { return this->literal_false(l);});
  }
  bool clause_unsat(clause_id cid) const {
    return clause_unsat(cnf[cid]);
  }

  bool cnf_unsat() const {
    return std::any_of(std::begin(cnf), std::end(cnf), [this](auto& c) {return this->clause_unsat(c);});
  }

  size_t count_unassigned_literals(const clause_t& clause) const {
    return std::count_if(std::begin(clause), std::end(clause), [this](auto& c) {return this->literal_unassigned(c);});
  }
  size_t count_unassigned_literals(clause_id cid) const {
    return count_unassigned_literals(cnf[cid]);
  }

  // To help unit-prop, for now.
  literal_t find_unassigned_literal(const clause_t& clause) const {
    auto it = std::find_if(std::begin(clause), std::end(clause), [this](auto& c) {
                                                                return this->literal_unassigned(c);});
    assert(it != std::end(clause));
    return *it;
  }
  literal_t find_unassigned_literal(clause_id cid) const {
    return find_unassigned_literal(cnf[cid]);
  }

  // Really, "no conflict found". That's why we don't have the counterpart.
  bool cnf_sat() const {
    return std::all_of(std::begin(cnf), std::end(cnf), [this](auto& c){return this->clause_sat(c);});
  }

  variable_state_t satisfy_literal(literal_t l) const {
    if (l > 0) {
      return unassigned_to_true;
    }
    return unassigned_to_false;
  }

  // Find a new, unassigned literal, and assign it.
  void decide_literal() {
    assert(!units_to_prop());
    auto it = std::find_if(std::begin(cnf), std::end(cnf), [this](const clause_t& clause) {
                                                             return this->count_unassigned_literals(clause) > 0;
                                                           });
    assert(it != std::end(cnf));
    literal_t l = find_unassigned_literal(*it);
    variable_t v = l > 0 ? l : -l;
    variable_state[v] = satisfy_literal(l);
    action_t action;
    action.action_kind = trace_t::action_t::action_kind_t::decision;
    action.decision_literal = l;
    actions.push_back(action);
  }

  bool units_to_prop() const {
    for (auto& c : cnf) {
      assert(!clause_unsat(c));
      if (clause_sat(c)) {
        continue;
      }
      if (1 == count_unassigned_literals(c)) {
        return true;
      }
    }
    return false;
  }

  bool prop_unit() {
    for (auto& c : cnf) {
      assert(!clause_unsat(c));
      if (clause_sat(c)) {
        continue;
      }
      if (1 == count_unassigned_literals(c)) {
        literal_t l = find_unassigned_literal(c);
        variable_state[std::abs(l)] = satisfy_literal(l);
        action_t action;
        action.action_kind = trace_t::action_t::action_kind_t::unit_prop;
        action.unit_prop.propped_literal = l;
        action.unit_prop.reason = 0; // TODO: do this right
        actions.push_back(action);

        // Now, have we introduced a conflict?
        auto it = std::find_if(std::begin(cnf), std::end(cnf), [this](const clause_t& c) { return clause_unsat(c); });
        if (it != std::end(cnf)) {
          action_t action;
          action.action_kind = trace_t::action_t::action_kind_t::halt_conflict;
          actions.push_back(action);
          return false;
        }

        // Have we solved everything?
        if (cnf_sat()) {
          action_t action;
          action.action_kind = trace_t::action_t::action_kind_t::halt_sat;
          actions.push_back(action);
          return false;
        }


        return true;
      }
    }
    assert(0);
  }

  // We have a special initial trace that looks for degenerate
  // We normally maintain certain invariants about the CNF (such as
  // we never decide a variable so long as there are unit clauses to prop),
  // but those invariants can be violated by the initial CNF itself.

  // This is the main entry point of the trace.
  // Basically, continue adding actions until it's halted.
  void process() {
    while (!halted()) {
      while (units_to_prop()) {
        // This does a lot of work.
        // It can find a conflict.
        if (!prop_unit()) {
          break;
        }
      }
      if (halted()) {
        return;
      }
      // This can see if we've found a totally-sat assignment.
      decide_literal();
    }
  }
};

void print_cnf(const cnf_t& cnf) {
  for (auto&& c : cnf) {
    for (auto&& l : c) {
      std::cout << l << " ";
    }
    std::cout << "0" << std::endl;
  }
}

cnf_t load_cnf() {
  cnf_t cnf;
  // Load in cnf from stdin.
  literal_t next_literal;
  clause_t next_clause;

  std::string line;
  while (std::getline(std::cin, line)) {
    if (line.size() == 0) { continue; }
    if (line[0] == 'c') { continue; }
    if (line[0] == 'p') {
      // TODO: do some error-checking;
      break;
    }
  }

  while (std::cin >> next_literal) {
    if (!next_literal) {
      cnf.push_back(next_clause);
      next_clause.clear();
      continue;
    }
    next_clause.push_back(next_literal);
  }
  return cnf;
}


std::ostream& operator<<(std::ostream& o, const trace_t::action_t::action_kind_t a) {
  switch(a) {
  case trace_t::action_t::action_kind_t::decision: return o << "decision";
  case trace_t::action_t::action_kind_t::unit_prop: return o << "unit_prop";
  case trace_t::action_t::action_kind_t::backtrack: return o << "backtrack";
  case trace_t::action_t::action_kind_t::halt_conflict: return o << "halt_conflict";
  case trace_t::action_t::action_kind_t::halt_unsat: return o << "halt_unsat";
  case trace_t::action_t::action_kind_t::halt_sat: return o << "halt_sat";
  }
}
std::ostream& operator<<(std::ostream& o, const trace_t::variable_state_t s) {
  switch(s) {
    case trace_t::variable_state_t::unassigned: return o << "unassigned";
    case trace_t::variable_state_t::unassigned_to_true: return o << "true";
    case trace_t::variable_state_t::unassigned_to_false: return o << "false";
    case trace_t::variable_state_t::swap_to_true: return o << "true";
    case trace_t::variable_state_t::swap_to_false: return o << "false";
  };
}
std::ostream& operator<<(std::ostream& o, const std::vector<trace_t::variable_state_t>& s) {
  o << "{ ";
  for (size_t i = 0; i < s.size(); i++) {
    auto v = s[i];
    if (v != trace_t::variable_state_t::unassigned) {
      o << i << "=" << v << " ";
    }
  }
  return o << "}";
}
std::ostream& operator<<(std::ostream& o, const trace_t::action_t a) {
  o << "{ " << a.action_kind;
  switch(a.action_kind) {
  case trace_t::action_t::action_kind_t::decision: return o << ", " << a.decision_literal << " }";
  case trace_t::action_t::action_kind_t::unit_prop: return o << ", " << a.unit_prop.propped_literal << ", " << a.unit_prop.reason << " }";
  default: return o << " }";
  }
}
std::ostream& operator<<(std::ostream& o, const std::vector<trace_t::action_t> v) {
  for (auto& a : v) {
    o << a << std::endl;
  }
  return o;
}
std::ostream& operator<<(std::ostream& o, const trace_t t) {
  return o << t.variable_state << std::endl << t.actions;
}

// The real goal here is to find conflicts as fast as possible.
int main(int argc, char* argv[]) {

  // Instantiate our CNF object
  cnf_t cnf = load_cnf();
  print_cnf(cnf);
  std::cout << "CNF DONE" << std::endl;

  // Preprocess trace.
  trace_t trace(cnf);
  trace.process();
  std::cout << trace;

  // construct our partial assignment object

  // do the main loop
}
