#include <algorithm>
#include <iostream>

#include <cassert>

#include "cnf.h"


std::ostream& operator<<(std::ostream& o, const clause_t& c) {
  for (auto l : c) {
    o << l << " ";
  }
  return o;
}

void print_cnf(const cnf_t& cnf) {
  for (auto&& c : cnf) {
    for (auto&& l : c) {
      std::cout << l << " ";
    }
    std::cout << "0" << std::endl;
  }
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

bool action_t::has_literal() const {
  return action_kind == action_kind_t::decision ||
    action_kind == action_kind_t::unit_prop;
}
literal_t action_t::get_literal() const {
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
bool action_t::is_decision() const {
  return action_kind == action_kind_t::decision;
}
bool action_t::has_clause() const {
  return action_kind == action_kind_t::halt_conflict || action_kind == action_kind_t::unit_prop;
}
clause_id action_t::get_clause() const {
  if (action_kind == action_kind_t::halt_conflict) {
    return conflict_clause_id;
  }
  else if (action_kind == action_kind_t::unit_prop) {
    return unit_prop.reason;
  }
  assert(0);
}

std::ostream& operator<<(std::ostream& o, const action_t::action_kind_t a) {
  switch(a) {
  case action_t::action_kind_t::decision: return o << "decision";
  case action_t::action_kind_t::unit_prop: return o << "unit_prop";
  case action_t::action_kind_t::backtrack: return o << "backtrack";
  case action_t::action_kind_t::halt_conflict: return o << "halt_conflict";
  case action_t::action_kind_t::halt_unsat: return o << "halt_unsat";
  case action_t::action_kind_t::halt_sat: return o << "halt_sat";
  }
  return o;
}
std::ostream& operator<<(std::ostream& o, const action_t a) {
  o << "{ " << a.action_kind;
  switch(a.action_kind) {
  case action_t::action_kind_t::decision: return o << ", " << a.decision_literal << " }";
  case action_t::action_kind_t::unit_prop: return o << ", " << a.unit_prop.propped_literal << ", " << a.unit_prop.reason << " }";
  case action_t::action_kind_t::halt_conflict: return o << ", " << a.conflict_clause_id << " }";
  default: return o << " }";
  }
}

/*
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
 */
