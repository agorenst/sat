#include "cnf.h"
#include "debug.h"
#include "action.h"
std::ostream& operator<<(std::ostream& o, const std::vector<action_t> v);
void learned_clause_minimization(const cnf_t& cnf, clause_t& c, const std::vector<action_t>& actions) {
  // Explicit search implementation
  //std::cout << "Minimizing " << c << " with trail " << std::endl << actions << std::endl;

  std::vector<literal_t> decisions;
  std::vector<literal_t> marked;
  std::vector<literal_t> candidates;
  std::vector<literal_t> to_remove;
  marked.reserve(c.size());
  candidates.reserve(c.size());
  to_remove.reserve(c.size());

  for (literal_t l : c) {
    // Find the action for this
    auto at = std::find_if(std::begin(actions), std::end(actions), [l](action_t a) {
                                      return a.has_literal() && a.get_literal() == -l;});
    assert(at != std::end(actions));
    action_t a = *at;

    if (a.is_decision()) {
      decisions.push_back(l);
      continue;
    }

    SAT_ASSERT(a.is_unit_prop());
    // The literals in our learned clause are exactly in our trail.
    marked.push_back(l);
    candidates.push_back(l);
  }

  while (!candidates.empty()) {
    literal_t l = candidates.back(); candidates.pop_back();

    // get the unit prop

    auto at = std::find_if(std::begin(actions), std::end(actions), [l](action_t a) {
                                                                     //std::cout << "Testing: " << a << std::endl;
                                      return a.has_literal() && a.get_literal() == -l;});
    assert(at != std::end(actions));
    action_t a = *at;

    //std::cout << "Found action for " << l << " it is: " << a << std::endl;
    std::vector<literal_t> work_list;
    std::vector<literal_t> seen;
    const clause_t& r = cnf[a.get_clause()];
    //std::cout << "Processing unit prop of " << l << " from reason: " << r << std::endl;
    SAT_ASSERT(contains(r, -l));
    for (literal_t p : r) {
      if (p == -l) continue;
      // note the negation here! This literal was falsified in our
      // clause, so the negation of it was truth-ified in our trail.
      work_list.push_back(-p);
    }

    bool is_removable = true;
    while (!work_list.empty()) {
      //std::cout << "Processing work-list: " << work_list << std::endl;

      literal_t p = work_list.back();
      work_list.pop_back();

      if (contains(seen, p)) {
        continue;
      }
      seen.push_back(p);

      // Yay, at least some paths up are dominated by a marked literal.
      if (contains(marked, p)) {
        continue;
      }
      // Our dominating set has a decision, we fail, quit.
      if (contains(decisions, p)) {
        is_removable = false;
        break;
      }

      auto at = std::find_if(std::begin(actions), std::end(actions), [p](action_t a) {
                                          return a.has_literal() && a.get_literal() == p;});
      assert(at != std::end(actions));
      action_t a = *at;
      // Our dominating set has a decision, we fail, quit.
      if (a.is_decision()) {
        decisions.push_back(p);
        is_removable = false;
        break;
      }

      // if it's not a decision, it's unit-prop. Add its units.
      // I don't think order matters for correctness.;
      assert(a.is_unit_prop());
      const clause_t& pr = cnf[a.get_clause()];
      SAT_ASSERT(contains(pr, p));
      for (literal_t q : pr) {
        if (q == p) continue;
        work_list.push_back(-q);
      }
    }

    if (is_removable) {
      to_remove.push_back(l);
    }
  }

  // Now we have all the things we want to remove.
  auto orig_c = c.size();
  auto et = std::remove_if(std::begin(c), std::end(c), [&to_remove](auto l) {return contains(to_remove, l);});
  c.erase(et, std::end(c));
  //std::cout << "[LCM] size from " << orig_c << " to " << c.size() << std::endl;
}
