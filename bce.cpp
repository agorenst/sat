#include <algorithm>
#include <vector>

#include "cnf.h"
#include "debug.h"
#include "literal_incidence_map.h"
// Blocked clause elimination

bool resolve_taut(const clause_t& c, const clause_t& d, literal_t l) {
  SAT_ASSERT(contains(c, l));
  SAT_ASSERT(contains(d, -l));
  // we trust that c, d are sorted by the literal absolute values.
  size_t i = 0;
  size_t j = 0;
  while (i < c.size() && j < d.size()) {
    //std::cout << (c[i])  << " " <<  (d[j]) << std::endl;
    if (c[i] == l) { i++; continue; }
    if (d[j] == -l) { j++; continue; }
    if (c[i] == -d[j]) { return true; }
    if (c[i] == d[j]) { i++; j++; continue; }
    if (std::abs(c[i]) < std::abs(d[j])) { i++; continue; }
    SAT_ASSERT(std::abs(c[i]) > std::abs(d[j]));
    j++;
  }
  return false;
}

std::vector<clause_id> BCE(cnf_t& cnf) {

  // Prepare the CNF itself by sorting the contents of its clauses.
  // This does not invalidate our indexing, though it is risky...
  for (clause_id cid : cnf) {
    clause_t& c = cnf[cid];
    std::sort(std::begin(c), std::end(c),
              [](literal_t l1, literal_t l2) {
                return std::abs(l1) < std::abs(l2);
              });
  }

#if 0
  std::cout << cnf << std::endl;
#endif


  literal_incidence_map_t literal_to_clauses(cnf);
  for (clause_id cid : cnf) {
    const clause_t& c = cnf[cid];
    for (literal_t l : c) {
      literal_to_clauses[l].push_back(cid);
    }
  }

  variable_t max_var = max_variable(cnf);

  // Create our initial worklist:
  std::vector<literal_t> worklist;
  for (literal_t l = -max_var; l <= max_var; l++) {
    if (!l) continue;
    worklist.push_back(l);
  }
  std::sort(std::begin(worklist), std::end(worklist),
            [&literal_to_clauses](literal_t l1, literal_t l2) {
              return literal_to_clauses[l1].size() > literal_to_clauses[l2].size();
            });

  // Debugging: make sure I'm sorting in the right order:
  // Because we pop from the back, we want the last one (the first one we'd
  // pop) have the shortest incidence list.
#if 0
  for (literal_t l : worklist) {
    std::cout << l << ": " << literal_to_clauses[l].size() << std::endl;
  }
#endif

  std::vector<clause_id> result;

  // Now we work through our worklist!
  while (!worklist.empty()) {
    literal_t l = worklist.back(); worklist.pop_back();
    auto& CL = literal_to_clauses[l];
    const auto& DL = literal_to_clauses[-l];
    const auto orig_size = result.size();
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
    // To facilitate the loop, we "remove" the clauses
    // from our own data structure:
    std::for_each(std::begin(result)+orig_size, std::end(result),
                  [&CL](clause_id cid) {
                    auto to_del = std::remove(std::begin(CL), std::end(CL), cid);
                    CL.erase(to_del, std::end(CL));
                  });
  }
  //std::cerr << "[BCE] Total clauses removed: " << result.size();

  return result;
}
