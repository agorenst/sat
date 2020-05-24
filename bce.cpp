#include <algorithm>
#include <vector>

#include "cnf.h"
#include "debug.h"
// Blocked clause elimination


size_t literal_to_index(literal_t l) {
  if (l < 0) {
    return std::abs(l)*2+1;
  }
  else {
    return l * 2;
  }
}

variable_t max_variable(const cnf_t& cnf) {
  literal_t m = 0;
  for (const clause_t& c : cnf) {
    for (const literal_t l : c) {
      m = std::max(std::abs(l), m);
    }
  }
  return m;
}

bool resolve_taut(const clause_t& c, const clause_t& d, literal_t l) {
  SAT_ASSERT(contains(c, l));
  SAT_ASSERT(contains(d, -l));
  // we trust that c, d are sorted by the literal absolute values.
  size_t i = 0;
  size_t j = 0;
  while (i < c.size() && j < d.size()) {
    if (c[i] == l) { i++; continue; }
    if (d[j] == -l) { j++; continue; }
    if (c[i] == -d[j]) { return true; }
    if (std::abs(c[i]) < std::abs(c[j])) { i++; }
    else {
      SAT_ASSERT(std::abs(c[i]) > std::abs(c[j]));
      j++;
    }
  }
  return false;
}

std::vector<clause_id> BCE(cnf_t& cnf) {

  // Prepare the CNF itself by sorting the contents of its clauses.
  // This does not invalidate our indexing, though it is risky...
  for (clause_t& c : cnf) {
    std::sort(std::begin(c), std::end(c),
              [](literal_t l1, literal_t l2) {
                return std::abs(l1) < std::abs(l2);
              });
  }

#if 0
  std::cout << cnf << std::endl;
#endif

  // Build the incidence maps:
  std::vector<std::vector<clause_id>> literal_to_clauses;
  variable_t max_var = max_variable(cnf);
  literal_to_clauses.resize(max_var*2+2);

  for (clause_id cid = 0; cid < cnf.size(); cid++) {
    const clause_t& c = cnf[cid];
    for (literal_t l : c) {
      size_t i = literal_to_index(l);
      literal_to_clauses[i].push_back(cid);
    }
  }

  // Create our initial worklist:
  std::vector<literal_t> worklist;
  for (literal_t l = -max_var; l <= max_var; l++) {
    if (!l) continue;
    worklist.push_back(l);
  }
  std::sort(std::begin(worklist), std::end(worklist),
            [&literal_to_clauses](literal_t l1, literal_t l2) {
              size_t i = literal_to_index(l1);
              size_t j = literal_to_index(l2);
              return literal_to_clauses[i].size() > literal_to_clauses[j].size();
            });

  // Debugging: make sure I'm sorting in the right order:
  // Because we pop from the back, we want the last one (the first one we'd
  // pop) have the shortest incidence list.
#if 0
  for (literal_t l : worklist) {
    size_t i = literal_to_index(l);
    std::cout << l << ": " << literal_to_clauses[i].size() << std::endl;
  }
#endif

  std::vector<clause_id> result;

  // Now we work through our worklist!
  while (!worklist.empty()) {
    literal_t l = worklist.back(); worklist.pop_back();
    size_t I = literal_to_index(l);
    size_t J = literal_to_index(-l);
    const auto& CL = literal_to_clauses[I];
    const auto& DL = literal_to_clauses[J];
    for (clause_id cid : CL) {

      // If we've already eliminated this, skip.
      if (contains(result, cid)) continue;

      bool is_blocked = true;
      for (clause_id did : DL) {
        if (!resolve_taut(cnf[cid], cnf[did], l)) {
          is_blocked = false;
          break;
        }
      }

      if (is_blocked) {
        result.push_back(cid);
        for (literal_t m : cnf[cid]) {
          if (!contains(worklist, -m)) {
            worklist.push_back(-m);
          }
        }
      }
    }
  }

  return result;
}
