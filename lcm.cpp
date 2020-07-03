#include <functional>
#include "action.h"
#include "cnf.h"
#include "debug.h"
#include "trail.h"

#include "literal_incidence_map.h"

// This approach, we "flood" the information of what depends
// on a decision forward.
void lcm(const cnf_t& cnf, clause_t& c, const trail_t& trail) {
  static lit_bitset_t decisions(cnf);
  decisions.clear();

  // populate the decisions set.
  for (action_t a : trail) {
    if (a.is_decision()) {
      decisions.set(a.get_literal());
    } else if (a.is_unit_prop()) {
      literal_t l = a.get_literal();
      const clause_t& r = cnf[a.get_clause()];
      for (auto m : r) {
        if (decisions.get(neg(m))) {
          decisions.set(l);
          break;
        }
      }
    }
  }

  auto et = std::remove_if(std::begin(c), std::end(c),
                           [&](literal_t l) { return !decisions.get(neg(l)); });
  c.erase(et, std::end(c));
  std::sort(std::begin(c), std::end(c));
}

void action_work_list_method(const cnf_t& cnf, clause_t& c,
                             const trail_t& actions, lit_bitset_t& seen) {
  for (size_t i = 0; i < c.size(); i++) {
    literal_t l = c[i];

    action_t a = actions.cause(neg(l));

    if (a.is_decision()) continue;

    SAT_ASSERT(a.is_unit_prop());

    static std::vector<action_t> work_list;
    work_list.clear();
    work_list.push_back(a);

    bool is_removable = true;
    while (!work_list.empty()) {
      action_t a = work_list.back();
      work_list.pop_back();
      SAT_ASSERT(a.is_unit_prop());
      const clause_t& d = cnf[a.get_clause()];
      SAT_ASSERT(contains(d, a.get_literal()));
      for (literal_t p : d) {
        if (a.get_literal() == p) continue;

        // Yay, at least some paths up are dominated by a marked literal.
        // At least *this* antecedent literal, p, is already in c. So if we
        // resolve c against r, p won't be added to c (it's already in c...)
        if (contains(c, p)) continue;

        // Our dominating set has a decision, we fail, quit.
        action_t a = actions.cause(neg(p));
        if (a.is_decision()) {
          is_removable = false;
          break;
        }
        SAT_ASSERT(a.get_literal() == neg(p));
        SAT_ASSERT(a.is_unit_prop());
        work_list.push_back(a);
      }
      if (!is_removable) break;
    }

    if (is_removable) {
      std::swap(c[i], c[c.size() - 1]);
      c.pop_back();
      i--;
    }
  }
  std::sort(std::begin(c), std::end(c));
}

void lcm_cache_dfs(const cnf_t& cnf, clause_t& c, const trail_t& actions) {
  static lit_bitset_t not_removable(cnf);
  static lit_bitset_t removable(cnf);
  not_removable.clear();
  removable.clear();

  // This is a recursive DFS, but we also cache results
  // along the way.
  std::function<bool(literal_t)> explore = [&](literal_t l) -> bool {
    // Have we already computed the answer?
    if (not_removable.get(l)) {
      return false;
    }
    if (removable.get(l)) {
      return true;
    }

    // If we're already in c, we're done.
    if (contains(c, l)) {
      removable.set(l);
      return true;
    }

    action_t a = actions.cause(neg(l));

    // If we're a decision, we're not removable.
    if (a.is_decision()) {
      not_removable.set(l);
      return false;
    }

    // If we have a reason, we're removable
    // only if everything else is.
    const clause_t& d = cnf[a.get_clause()];
    for (literal_t p : d) {
      if (p == neg(l)) continue;
      if (!explore(p)) {
        not_removable.set(l);
        return false;
      }
    }

    removable.set(l);
    return true;
  };

  for (size_t i = 0; i < c.size(); i++) {
    literal_t l = c[i];

    action_t a = actions.cause(neg(l));

    // We can't launch straight into the recursion yet
    // because we can't say l is removable just because
    // it's in c.

    if (a.is_decision()) {
      not_removable.set(neg(l));
      continue;
    }

    bool removable = true;

    const clause_t& d = cnf[a.get_clause()];
    for (literal_t p : d) {
      if (p == neg(l)) continue;
      if (!explore(p)) {
        removable = false;
        break;
      }
    }

    if (removable) {
      std::swap(c[i], c[c.size() - 1]);
      c.pop_back();
      i--;
    }
  }
  std::sort(std::begin(c), std::end(c));
}

void learned_clause_minimization(const cnf_t& cnf, clause_t& c,
                                 const trail_t& actions, lit_bitset_t& seen) {
  // Explicit search implementation
  // std::cout << "Minimizing " << c <<  " with trail " << std::endl << actions
  // << std::endl;

  // lcm(cnf, c, actions);
  // return;

  // auto ccopy = c;
  lcm_cache_dfs(cnf, c, actions);
  return;
  // auto answer1 = c;
  // c = ccopy;

  for (size_t i = 0; i < c.size(); i++) {
    literal_t l = c[i];
    // Find the action for this
    action_t a = actions.cause(neg(l));

    if (a.is_decision()) {
      continue;
    }

    SAT_ASSERT(a.is_unit_prop());

    // This is the reason that we have to include l in our learned clause.
    // If every other literal in r, however, is already in our clause, then we
    // don't need it. We implicitly resolve c against that reason to get a
    // subsuming resolvent (c, but without l). Moreover, for those r's not in
    // our clause, we can search backwards for /their/ implicants, to set up the
    // same, more advanced version of this.
    const clause_t& r = cnf[a.get_clause()];

    static std::vector<literal_t> work_list;
    work_list.clear();
    SAT_ASSERT(contains(r, neg(l)));
    for (literal_t p : r) {
      if (p == neg(l)) continue;  // this is the resolvent, so skip it.
      work_list.push_back(p);
    }

    seen.clear();

    bool is_removable = true;
    while (!work_list.empty()) {
      // std::cout << "Processing work-list: " << work_list << std::endl;

      literal_t p = work_list.back();
      // std::cout << "Considering candidate: " << p << std::endl;
      work_list.pop_back();

      // if (contains(seen, p)) {
      if (seen.get(p)) {
        continue;
      }
      seen.set(p);

      // Yay, at least some paths up are dominated by a marked literal.
      // At least *this* antecedent literal, p, is already in c. So if we
      // resolve c against r, p won't be added to c (it's already in c...)
      if (contains(c, p)) {
        continue;
      }

      // Otherwise, find its reason. Recall that p is falsified, so we're
      // really finding -p.
      action_t a = actions.cause(neg(p));

      // Our dominating set has a decision, we fail, quit.
      if (a.is_decision()) {
        is_removable = false;
        break;
      }

      // if it's not a decision, it's unit-prop. Add its units.
      // I don't think order matters for correctness.;
      SAT_ASSERT(a.is_unit_prop());
      const clause_t& pr = cnf[a.get_clause()];
      SAT_ASSERT(contains(pr, neg(p)));
      for (literal_t q : pr) {
        if (q == neg(p)) continue;
        work_list.push_back(q);
      }
    }

    if (is_removable) {
      std::swap(c[i], c[c.size() - 1]);
      c.pop_back();
      i--;
    }
  }
  std::sort(std::begin(c), std::end(c));
  // std::cerr << "problem with " << c << "; " << answer1 << std::endl;
  // SAT_ASSERT(c == answer1);
  // assert(c == answer1);
}
