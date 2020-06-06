#include <iostream>
#include <sstream>

#include "cnf.h"
#include "trail.h"
#include "clause_learning.h"
#include "lcm.h"
#include "visualizations.h"

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

struct test_instance {
  trail_t trail;
  cnf_t cnf;
  clause_t goal;
  clause_t learned;
  bool test() {
    learned = learn_clause(cnf, trail);
    learned_clause_minimization(cnf, learned, trail);
    std::sort(std::begin(learned), std::end(learned));
    std::sort(std::begin(goal), std::end(goal));
    if (learned != goal) {
      return false;
    }
    return true;
  }
};

// Do a single iteration of the test.
test_instance test_learning(std::istream& in) {
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

  test_instance T{std::move(trail), cnf, goal};
  max_var = 0;
  return T;
}

void pretty_print_trail(const cnf_t& cnf, const trail_t& t) {
  for (action_t a : t) {
    if (a.is_decision()) {
      std::cout << "decision: " << a.get_literal() << std::endl;
    }
    else if (a.is_unit_prop()) {
      std::cout << "unitprop: " << a.get_literal() << " ; ";
      for (auto l : cnf[a.get_clause()]) {
        std::cout << l << " ";
      }
      std::cout << std::endl;
    }
    else if (a.is_conflict()) {
      std::cout << "conflict: ";
      for (auto l : cnf[a.get_clause()]) {
        std::cout << l << " ";
      }
      std::cout << std::endl;
    }
    else {
      assert(0);
    }
  }
}

int main() {
  // First, read in the log:
  std::istream& in = std::cin;

  std::string line;
  std::vector<test_instance> differences;
  while (std::getline(in, line)) {
    if (line.size() > 0 && line[0] == '#') continue; // comment!
    if (line == "===============================") {
      test_instance T = test_learning(in);
      if (!T.test()) {
        differences.emplace_back(std::move(T));
      }
    }
  }
  if (differences.empty()) {
    std::cout << "No differences found!" << std::endl;
    return 0;
  }
  auto smallest_test = std::min_element(std::begin(differences),
                                        std::end(differences),
                                        [](const test_instance& t1,
                                           const test_instance& t2) {
                                          //return t1.trail.level() < t2.trail.level();
                                          return std::distance(std::begin(t1.trail), std::end(t1.trail)) <
                                            std::distance(std::begin(t2.trail), std::end(t2.trail));
                                        });
  pretty_print_trail(smallest_test->cnf, smallest_test->trail);
  std::cout << "goal: " << smallest_test->goal << std::endl;
  std::cout << "learned: " << smallest_test->learned << std::endl;
  print_conflict_graph(smallest_test->cnf, smallest_test->trail);
  return 0;
}
