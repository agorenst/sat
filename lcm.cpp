#include "cnf.h"
#include "debug.h"
#include "action.h"
#include "trail.h"

#include "literal_incidence_map.h"

void lcm(const cnf_t& cnf, clause_t& c, const trail_t& trail) {

  bool didWork = true;
  while (didWork) {
    didWork = false;
    literal_map_t<int> depends_on_decision(cnf);
    std::fill(std::begin(depends_on_decision), std::end(depends_on_decision), 0);

    for (action_t a : trail) {
      if (a.is_decision()) {
        depends_on_decision[a.get_literal()] = 1;
      }
      else if (a.is_unit_prop()) {
        literal_t l = a.get_literal();
        const clause_t& r = cnf[a.get_clause()];
        for (auto m : r) {
          if (depends_on_decision[-m]) {
            depends_on_decision[l] = 1;
          }
        }
      }
    }

    auto is_decision = [&](literal_t l) {
                         auto at = std::find_if(std::begin(trail), std::end(trail),
                                                [l](action_t a) { return a.has_literal() && a.get_literal() == l; });
                         assert(at != std::end(trail));
                         return at->is_decision();
                       };

    auto et = std::remove_if(std::begin(c), std::end(c),
                             [&](literal_t l) {
                               return !is_decision(-l) && !depends_on_decision[-l];
                             });
    if (et != std::end(c)) {
      didWork = true;
    }
    c.erase(et, std::end(c));
  }
}

void learned_clause_minimization(const cnf_t& cnf, clause_t& c, const trail_t& actions) {
  // Explicit search implementation
  //std::cout << "Minimizing " << c <<  " with trail " << std::endl << actions << std::endl;

  //lcm(cnf, c, actions);
  //return;

  std::vector<literal_t> decisions;
  std::vector<literal_t> marked;
  std::vector<literal_t> candidates;
  std::vector<literal_t> to_remove;
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
    candidates.push_back(l);
  }

  while (!candidates.empty()) {
    literal_t l = candidates.back(); candidates.pop_back();

    // Get the unit prop. This will be the thing that demands -l be satisfied,
    // hence falsifying l.
    auto at = std::find_if(std::begin(actions), std::end(actions), [l](action_t a) {
                                      return a.has_literal() && a.get_literal() == -l;});
    assert(at != std::end(actions));
    action_t a = *at;
    assert(a.is_unit_prop());

    // This is the reason that we have to include l in our learned clause.
    // If every other literal in r, however, is already in our clause, then we don't need
    // it. We implicitly resolve c against that reason to get a subsuming resolvent (c, but without l).
    // Moreover, for those r's not in our clause, we can search backwards for /their/ implicants,
    // to set up the same, more advanced version of this.
    const clause_t& r = cnf[a.get_clause()];

    std::vector<literal_t> work_list;
    assert(contains(r, -l));
    for (literal_t p : r) {
      if (p == -l) continue; // this is the resolvent, so skip it.
      work_list.push_back(p);
    }

    std::vector<literal_t> seen;

    bool is_removable = true;
    while (!work_list.empty()) {
      //std::cout << "Processing work-list: " << work_list << std::endl;

      literal_t p = work_list.back();
      //std::cout << "Considering candidate: " << p << std::endl;
      work_list.pop_back();

      if (contains(seen, p)) {
        continue;
      }
      seen.push_back(p);

      // Yay, at least some paths up are dominated by a marked literal.
      // At least *this* antecedent literal, p, is already in c. So if we
      // resolve c against r, p won't be added to c (it's already in c...)
      if (contains(c, p)) {
        continue;
      }

      // Otherwise, find its reason. Recall that p is falsified, so we're
      // really finding -p.
      auto at = std::find_if(std::begin(actions), std::end(actions), [p](action_t a) {
                                          return a.has_literal() && a.get_literal() == -p;});
      assert(at != std::end(actions));
      action_t a = *at;

      // Our dominating set has a decision, we fail, quit.
      if (a.is_decision()) {
        is_removable = false;
        break;
      }

      // if it's not a decision, it's unit-prop. Add its units.
      // I don't think order matters for correctness.;
      assert(a.is_unit_prop());
      const clause_t& pr = cnf[a.get_clause()];
      SAT_ASSERT(contains(pr, p));
      for (literal_t q : pr) {
        if (q == -p) continue;
        work_list.push_back(q);
      }
    }

    if (is_removable) {
      to_remove.push_back(l);
    }
  }

  // Now we have all the things we want to remove.
  auto et = std::remove_if(std::begin(c), std::end(c), [&to_remove](auto l) {return contains(to_remove, l);});
  c.erase(et, std::end(c));
}
