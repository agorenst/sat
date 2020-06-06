#include <iostream>
#include <sstream>

#include "cnf.h"
#include "trail.h"
#include "clause_learning.h"
#include "lcm.h"

variable_t max_var = 0;

clause_t get_clause(std::istream& iss) {
  literal_t l;
  clause_t c;
  while (iss >> l) {
    max_var = std::max(max_var, std::abs(l));
    assert(l);
    c.push_back(l);
  }
  return c;
}

// Do a single iteration of the test.
bool test_learning(std::istream& in) {
  cnf_t cnf;

  std::vector<action_t> dynamic_trail;

  clause_t goal;

  // Read in the log file:
  std::string line;
  while (std::getline(in, line)) {
    std::istringstream iss(line);
    std::string word;
    iss >> word;
    if (word == "decision:") {
      literal_t l;
      iss >> l;
      max_var = std::max(max_var, std::abs(l));
      action_t a = make_decision(l);
      dynamic_trail.push_back(a);
    }
    else if (word == "unitprop:") {
      literal_t l;
      iss >> l;
      std::string semicolon;
      iss >> semicolon;
      assert(semicolon == ";");
      clause_t c = get_clause(iss);
      auto key = cnf.push_back(c);
      action_t a = make_unit_prop(l, key);
      dynamic_trail.push_back(a);

      assert(contains(c, l));
    }
    else if (word == "conflict:") {
      clause_t c = get_clause(iss);
      auto key = cnf.push_back(c);
      action_t a = make_conflict(key);
      dynamic_trail.push_back(a);
    }
    else if (word == "learned:") {
      goal = get_clause(iss);
      break;
    }
    else {
      assert(0);
    }
  }

  trail_t trail;
  trail.construct(max_var);
  std::cout << "Done constructing, max_var = " << max_var << std::endl;

  // TODO: Confirm that each unit-prop really does only contain literals
  // from earlier in the trail. Standard correctness checks.

  for (action_t a : dynamic_trail) {
    // Correctness:
    if (a.is_unit_prop()) {
      clause_id cid = a.get_clause();
      const clause_t& c = cnf[cid];
      assert(trail.count_unassigned_literals(c) == 1);
      assert(trail.find_unassigned_literal(c) == a.get_literal());
    }
    else if (a.is_decision()) {
      literal_t l = a.get_literal();
      assert(trail.literal_unassigned(l));
    }
    else if (a.is_conflict()) {
      clause_id cid = a.get_clause();
      const clause_t& c = cnf[cid];
      assert(trail.clause_unsat(c));
    }
    trail.append(a);
  }

  // Now we have our real trail, and our real cnf.

  clause_t learned = learn_clause(cnf, trail);
  learned_clause_minimization(cnf, learned, trail);
  std::sort(std::begin(learned), std::end(learned));
  std::sort(std::begin(goal), std::end(goal));
  std::cout << "Learned: " << learned << std::endl;
  std::cout << "Goal:    " << goal << std::endl;
  if (learned != goal) {
    std::cout << "DIFFERENT" << std::endl;
    return false;
  }
  return true;
}

int main() {
  // First, read in the log:
  std::istream& in = std::cin;

  std::string line;
  while (std::getline(in, line)) {
    if (line == "===============================") {
      if (!test_learning(in)) {
        return 1;
      }
    }
  }
  return 0;
}
