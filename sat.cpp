#include <cstdio>
#include <iostream>
#include <cstdint>
#include <vector>
#include <algorithm>
#include <cassert>
#include <map>

// TODO: Do I really need to track variable assignments?
// TODO: Change clause_id to an iterator?
// TODO: Exercise 257 to get shorter learned clauses

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
std::ostream& operator<<(std::ostream& o,  clause_t& c) {
  for (auto l : c) {
    o << l << " ";
  }
  return o;
}

variable_t max_variable = 0;

typedef std::vector<clause_t> cnf_t;
typedef size_t clause_id;

template<typename C, typename V>
bool contains(const C& c, const V& v) {
  return std::find(std::begin(c), std::end(c), v) != std::end(c);
}

clause_t resolve(clause_t c1, clause_t c2, literal_t l) {
  assert(contains(c1, l));
  assert(contains(c2, -l));
  clause_t c3;
  for (literal_t x : c1) {
    if (std::abs(x) != std::abs(l)) {
      c3.push_back(x);
    }
  }
  for (literal_t x : c2) {
    if (std::abs(x) != std::abs(l)) {
      c3.push_back(x);
    }
  }
  std::sort(std::begin(c3), std::end(c3));
  auto new_end = std::unique(std::begin(c3), std::end(c3));
  c3.erase(new_end, std::end(c3));
  return c3;
}

literal_t resolve_candidate(clause_t c1, clause_t c2) {
  for (literal_t l : c1) {
    for (literal_t m : c2) {
      if (l == -m) return l;
    }
  }
  return 0;
}

enum class backtrack_mode_t {
                             simplest,
                             trust_learning, // does the learning phase do the backtracking for us?
};
backtrack_mode_t backtrack_mode = backtrack_mode_t::trust_learning;

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
      clause_id conflict_clause_id;
    };

    bool has_literal() const {
      return action_kind == action_kind_t::decision ||
        action_kind == action_kind_t::unit_prop;
    }
    literal_t get_literal() const {
      assert(action_kind == action_kind_t::decision ||
             action_kind == action_kind_t::unit_prop);
      if (action_kind == action_kind_t::decision) {
        return decision_literal;
      }
      else if (action_kind == action_kind_t::unit_prop) {
        return unit_prop.propped_literal;
      }
      return 0; // error!
    }
    bool is_decision() const {
      return action_kind == action_kind_t::decision;
    }
  };

  enum variable_state_t {
                       unassigned,
                       unassigned_to_true,
                       unassigned_to_false,
                       swap_to_true,
                       swap_to_false
  };



  cnf_t& cnf;
  std::vector<action_t> actions;
  std::vector<variable_state_t> variable_state;

  trace_t(cnf_t& cnf): cnf(cnf) {
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
    //std::out << "Debug: current trace is: " << *this << std::endl;
    return !actions.empty() && halt_state(*actions.rbegin());
  }
  bool final_state() {
    if (actions.empty()) {
      return false;
    }
    action_t action = *actions.rbegin();

    // TODO: there is a better place for this check/computation, I'm sure.
    if (action.action_kind == trace_t::action_t::action_kind_t::halt_conflict) {
      bool has_decision = false;
      for (auto& a : actions) {
        if (a.action_kind == trace_t::action_t::action_kind_t::decision) {
          has_decision = true;
          break;
        }
      }
      //auto it = std::find(std::begin(actions), std::end(actions), [](action_t a) { return a.action_kind == trace_t::action_t::action_kind_t::decision; });
      //if (it == std::end(actions)) {
      if (!has_decision) {
        action_t a;
        a.action_kind = trace_t::action_t::action_kind_t::halt_unsat;
        actions.push_back(a);
        action = a;
      }
    }
    return
      action.action_kind == trace_t::action_t::action_kind_t::halt_unsat ||
      action.action_kind == trace_t::action_t::action_kind_t::halt_sat;
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
  void unassign_literal(const literal_t l)  {
    variable_t v = l > 0 ? l : -l;
    variable_state[v] = unassigned;
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

    // Have we solved the equation?
    if (cnf_sat()) {
      action_t action;
      action.action_kind = trace_t::action_t::action_kind_t::halt_sat;
      actions.push_back(action);
    }
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
    for (clause_id i = 0; i < cnf.size(); i++) {
      auto& c = cnf[i];
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
        action.unit_prop.reason = i; // TODO: do this right
        actions.push_back(action);

        // Now, have we introduced a conflict?
        auto it = std::find_if(std::begin(cnf), std::end(cnf), [this](const clause_t& c) { return clause_unsat(c); });
        if (it != std::end(cnf)) {
          action_t action;
          action.action_kind = trace_t::action_t::action_kind_t::halt_conflict;
          action.conflict_clause_id = std::distance(std::begin(cnf), it); // get the id of the conflict clause!
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
    return false;
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

  void backtrack() {
    //backtrack_mode m = backtrack_mode::trust_learning;
    if (backtrack_mode == backtrack_mode_t::simplest) {
      std::fill(std::begin(variable_state), std::end(variable_state), variable_state_t::unassigned);
      actions.clear();
    }
    else if (backtrack_mode == backtrack_mode_t::trust_learning) {
      // do nothing!
    }
  }

  void learn_clause();

};
std::ostream& operator<<(std::ostream& o, const trace_t::action_t a);
std::ostream& operator<<(std::ostream& o, const std::vector<trace_t::action_t> v) {
  for (auto& a : v) {
    o << a << std::endl;
  }
  return o;
}


void trace_t::learn_clause() {
  enum class learn_mode {
                         simplest,
                         explicit_resolution,
                         knuth64,
  };
  //learn_mode mode = learn_mode::simplest;
  learn_mode mode = learn_mode::explicit_resolution;
  assert(actions.rbegin()->action_kind == trace_t::action_t::action_kind_t::halt_conflict);
  //std::cout << "About to learn clause from: " << *this << std::endl;
  if (mode == learn_mode::simplest) {
    clause_t new_clause;
    for (action_t a : actions) {
      if (a.action_kind == action_t::action_kind_t::decision) {
        new_clause.push_back(-a.decision_literal);
      }
    }
    //std::cout << "Learned clause: " << new_clause << std::endl;
    cnf.push_back(new_clause);
  }
  else if (mode == learn_mode::explicit_resolution) {
    // Get the initial conflict clause
    auto it = actions.rbegin();
    assert(it->action_kind == trace_t::action_t::action_kind_t::halt_conflict);
    // We make an explicit copy of that conflict clause
    clause_t c = cnf[it->conflict_clause_id];

    it++;

    // now go backwards until the decision, resolving things against it
    auto restart_it = it;
    for (; it->action_kind != trace_t::action_t::action_kind_t::decision; it++) {
      assert(it->action_kind == trace_t::action_t::action_kind_t::unit_prop);
      clause_t d = cnf[it->unit_prop.reason];
      if (literal_t r = resolve_candidate(c, d)) {
        c = resolve(c, d, r);
      }
    }

    // reset our iterator
    int counter = 0;
    literal_t new_implied = 0;
    it = std::next(actions.rbegin());

    for (; it->action_kind != trace_t::action_t::action_kind_t::decision; it++) {
      assert(it->action_kind == trace_t::action_t::action_kind_t::unit_prop);
      if (contains(c, -it->unit_prop.propped_literal)) {
        counter++;
        new_implied = it->get_literal();
      }
    }
    assert(it->action_kind == trace_t::action_t::action_kind_t::decision);
    if (contains(c, -it->decision_literal)) {
      counter++;
      new_implied = -it->get_literal();
    }

    //std::cout << "Learned clause " << c << std::endl;
    //std::cout << "Counter = " << counter << std::endl;
    assert(counter == 1);
    cnf.push_back(c);

    if (backtrack_mode == backtrack_mode_t::trust_learning) {
      // create the action that this new clause is unit_propped by the trail
      action_t a;
      a.action_kind = action_t::action_kind_t::unit_prop;
      a.unit_prop.reason = cnf.size() - 1;
      a.unit_prop.propped_literal = new_implied;

      // TODO: Show that this is nonlinear! That this backtracking is really worth-it.

      // we also do the backtracking here:
      it++; // start the next level of our decision trail
      // Continue going down our level
      for (; it != actions.rend(); it++) {
        literal_t l = it->get_literal();
        if (contains(c, -l)) {
          break;
        }
      }
      auto fwd_it = it.base();
      assert(fwd_it != actions.end());
      // now go back to the start of the "next" decision:
      while (!fwd_it->is_decision()) {
        fwd_it++;
        assert(fwd_it != actions.end());
      }
      //std::cout << "fwd_it : " << *fwd_it << std::endl;
      //std::cout << "it: " << *it << std::endl;
      //std::cout << "actions: " << actions << std::endl;
      assert(fwd_it->is_decision());

      // Backtrack!
      for (auto reset_it = fwd_it; reset_it != actions.end(); reset_it++) {
        if (reset_it->has_literal()) {
          literal_t l = reset_it->get_literal();
          unassign_literal(l);
        }
      }
      actions.erase(fwd_it, std::end(actions));

      // correctness check: let's make sure we really do induce
      // the new unit_prop:
      if (count_unassigned_literals(a.unit_prop.reason) != 1) {
        for (auto u : cnf[a.unit_prop.reason]) {
          if (literal_unassigned(u)) {
            //std::cout << "Unassigned: " << u << std::endl;
          }
        }
        //std::cout << "Count is: " << count_unassigned_literals(a.unit_prop.reason) << std::endl;
        //std::cout << "actions: " << actions << std::endl;
      }
      assert(count_unassigned_literals(a.unit_prop.reason) == 1);
      //std::cout << find_unassigned_literal(a.unit_prop.reason) << std::endl;
      //std::cout << a.unit_prop.propped_literal << std::endl;
      assert(find_unassigned_literal(a.unit_prop.reason) == a.unit_prop.propped_literal);
      // TODO: do we need to continue unit-prop here?
      // Eep, for now, I'll just rely on the next call to unit_prop?
    }
  }
  else {
    // This is really expensive: compute a mapping of literals to their levels.
    std::map<literal_t, int> literal_levels;
    int level = 0;
    {
      for (action_t a : actions) {
        if (a.action_kind == trace_t::action_t::action_kind_t::decision) {
          level++;
        }

        literal_t l = 0;
        if (a.action_kind == trace_t::action_t::action_kind_t::decision) {
          l = a.decision_literal;
        }
        else if (a.action_kind == trace_t::action_t::action_kind_t::unit_prop) {
          l = a.unit_prop.propped_literal;
        }
        else continue;

        assert(literal_levels.find(l) == literal_levels.end());
        literal_levels[l] = level;
      }
    }


    // Get the initial conflict clause
    auto it = actions.rbegin();
    assert(it->action_kind == trace_t::action_t::action_kind_t::halt_conflict);
    // We make an explicit copy of that conflict clause
    clause_t c = cnf[it->conflict_clause_id];


    // Get the initial literal that was propped into that conflict.
    // Given our current structure, it should be the immediately-preceeding unit prop.
    // TODO: This is not always true (mathematically, the algo can change!), make this more robust
    it++;
    assert(it->action_kind == trace_t::action_t::action_kind_t::unit_prop);
    literal_t l = it->unit_prop.propped_literal;
    assert(std::find(std::begin(c), std::end(c), -l) != std::end(c));
    assert(literal_levels.find(l) != literal_levels.end());

    std::vector<literal_t> resolve_possibilities;
    std::vector<literal_t> learned;
    for (literal_t L : c) {
      std::cout << "Considering literal in conflict clause: " << L << std::endl;
      resolve_possibilities.push_back(-L);
      if (-L == l) {
        continue;
      }

      // Everything else was set in our trail.
      assert(literal_levels.find(-L) != literal_levels.end());

      if (literal_levels[-L] < literal_levels[l]) {
        //std::cout << "Learning: " << L << " because its level is " << literal_levels[L] << " while our main l (" << l << ") is level " << literal_levels[l] << std::endl;
        learned.push_back(L);
      }
    }

    //std::cout << "Resolve possibilities: " << resolve_possibilities << std::endl;


    // we must have two conflicting unit props, at least, so counter must start out as greater-than-one.
    int counter = 0;
    for (auto jt = std::rbegin(actions); jt->action_kind != trace_t::action_t::action_kind_t::decision; jt++) {
      std::ostream& operator<<(std::ostream& o, const trace_t::action_t a);
      //std::cout << "Considering action: " << *jt;
      if (jt->action_kind == trace_t::action_t::action_kind_t::unit_prop) {
        const clause_t r = cnf[jt->unit_prop.reason];
        for (literal_t x : r) {
          //std::cout << x << " ";
        }
      }
      if (jt->action_kind == trace_t::action_t::action_kind_t::halt_conflict) {
        const clause_t r = cnf[jt->conflict_clause_id];
        for (literal_t x : r) {
          //std::cout << x << " ";
        }
      }
      //std::cout << std::endl;
      if (jt->action_kind == trace_t::action_t::action_kind_t::unit_prop) {
        // we consider the /negation/ of this propped literal.
        literal_t to_check = jt->unit_prop.propped_literal;
        if (std::find(std::begin(resolve_possibilities), std::end(resolve_possibilities), to_check) != std::end(resolve_possibilities)) {
          counter++;
        }
      }
      if (std::next(jt)->action_kind == trace_t::action_t::action_kind_t::decision) {
        counter++;
        resolve_possibilities.push_back(std::next(jt)->decision_literal);
      }
    }

    //std::cout << "counter: " << counter << std::endl;
    assert(counter > 1);
    it++;

    // Remember it is now the first thing /after/ our initial conflict-clause, so it's pointing to the preceeding unit-prop.
    for (; it != actions.rend(); it++) {
      //std::cout << "learned: " << learned << std::endl;
      //std::cout << "resolve_possibilities: " << resolve_possibilities << std::endl;
      //std::cout << "counter: " << counter << std::endl;
      //std::cout << "it: " << static_cast<int>(it->action_kind) << std::endl;
      if (it->action_kind == trace_t::action_t::action_kind_t::unit_prop) {
        literal_t L = it->unit_prop.propped_literal;
        // this is a unit prop we could resolve against!
        // We /don't/ negate it here, because that's the idea of resolution.
        //std::cout << "Considering L: " << L << std::endl;
        if (std::find(std::begin(resolve_possibilities), std::end(resolve_possibilities), L) != std::end(resolve_possibilities)) {
          if (counter == 1) {
            learned.push_back(-L);
            break;
          }
          clause_t d = cnf[it->unit_prop.reason];
          assert(std::find(std::begin(d), std::end(d), L) != std::end(d));
          std::cout << "Considering reason clause: " << d << std::endl;
          for (literal_t m : d) {
            std::cout << "Considering reason literal: " << m << std::endl;
            assert(literal_levels.find(-m) != literal_levels.end());
            assert(literal_levels.find(l) != literal_levels.end());
            if (literal_levels[-m] < literal_levels[l]) {
              learned.push_back(m);
            }
            if (m == L) { continue; }

            // note that this is now something we can resolve against too.
            if (std::find(std::begin(resolve_possibilities), std::end(resolve_possibilities), -m) == std::end(resolve_possibilities)) {
              resolve_possibilities.push_back(-m);
            }
          }
          counter--;
        }
      }
      if (it->action_kind == trace_t::action_t::action_kind_t::decision) {
        assert(counter == 1); // ?
        learned.push_back(-it->decision_literal);
      }
    }

    //std::cout << "Learned clause: " << learned << std::endl;
    std::sort(std::begin(learned), std::end(learned));
    auto new_end = std::unique(std::begin(learned), std::end(learned));
    learned.erase(new_end, std::end(learned));
    std::cout << "learned: " << learned << std::endl;
    cnf.push_back(learned);
  }
}

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
    if (next_literal == 0) {
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
  return o;
}
std::ostream& operator<<(std::ostream& o, const trace_t::variable_state_t s) {
  switch(s) {
    case trace_t::variable_state_t::unassigned: return o << "unassigned";
    case trace_t::variable_state_t::unassigned_to_true: return o << "true";
    case trace_t::variable_state_t::unassigned_to_false: return o << "false";
    case trace_t::variable_state_t::swap_to_true: return o << "true";
    case trace_t::variable_state_t::swap_to_false: return o << "false";
  };
  return o;
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
  case trace_t::action_t::action_kind_t::halt_conflict: return o << ", " << a.conflict_clause_id << " }";
  default: return o << " }";
  }
}
std::ostream& operator<<(std::ostream& o, const trace_t t) {
  return o << t.variable_state << std::endl << t.actions;
}

std::string report_conclusion(trace_t::action_t::action_kind_t conclusion) {
  switch(conclusion) {
  case trace_t::action_t::action_kind_t::decision: return "ERROR";
  case trace_t::action_t::action_kind_t::unit_prop: return "ERROR";
  case trace_t::action_t::action_kind_t::backtrack: return "ERROR";
  case trace_t::action_t::action_kind_t::halt_conflict: return "ERROR";
  case trace_t::action_t::action_kind_t::halt_unsat: return "UNSATISFIABLE";
  case trace_t::action_t::action_kind_t::halt_sat: return "SATISFIABLE";
  }
  return "ERROR";
}
// The real goal here is to find conflicts as fast as possible.
int main(int argc, char* argv[]) {

  // Instantiate our CNF object
  cnf_t cnf = load_cnf();
  //print_cnf(cnf);
  //std::cout << "CNF DONE" << std::endl;
  assert(cnf.size() > 0); // make sure parsing worked.

  // TODO: fold this into a more general case, if possible.
  if (std::find(std::begin(cnf), std::end(cnf), clause_t()) != std::end(cnf)) {
    std::cout << "UNSATISIFIABLE" << std::endl;
  }

  // Preprocess trace. This is to clean out "degenerate" aspects,
  // like units and trivial CNF cases.
  trace_t trace(cnf);
  trace.process();
  //std::cout << trace;

  while (!trace.final_state()) {
    assert(trace.actions.rbegin()->action_kind == trace_t::action_t::action_kind_t::halt_conflict);
    trace.learn_clause();
    trace.backtrack();
    trace.process();
  }
  std::cout << report_conclusion(trace.actions.rbegin()->action_kind) << std::endl;


  // do the main loop
}
