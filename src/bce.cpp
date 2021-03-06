#include <algorithm>
#include <vector>

#include "cnf.h"
#include "debug.h"
// Blocked clause elimination

bool resolve_taut(const clause_t &c, const clause_t &d, literal_t l) {
  SAT_ASSERT(contains(c, l));
  SAT_ASSERT(contains(d, neg(l)));
  // we trust that c, d are sorted by the literal absolute values.
  size_t i = 0;
  size_t j = 0;
  while (i < c.size() && j < d.size()) {
    // std::cout << (c[i])  << " " <<  (d[j]) << std::endl;

    // If the literals are the ones we're resolving against, skip.
    if (c[i] == l) {
      i++;
      continue;
    }
    if (d[j] == neg(l)) {
      j++;
      continue;
    }

    // We can resolve!
    if (c[i] == neg(d[j])) {
      return true;
    }

    // Identical -- nice, but proceed.
    if (c[i] == d[j]) {
      i++;
      j++;
    }
    // Do the merge-sort-esque increments.
    else if (var(c[i]) < var(d[j])) {
      i++;
    } else {
      j++;
    }
  }
  return false;
}

std::vector<clause_id> BCE(cnf_t &cnf) {
  // Prepare the CNF itself by sorting the contents of its clauses.
  // This does not invalidate our indexing, though it is risky...
  for (clause_id cid : cnf) {
    clause_t &c = cnf[cid];
    std::sort(std::begin(c), std::end(c),
              [](literal_t l1, literal_t l2) { return var(l1) < var(l2); });
  }

#if 0
  std::cout << cnf << std::endl;
#endif
  literal_map_t<clause_set_t> literal_to_clauses(max_variable(cnf));
  for (clause_id cid : cnf) {
    const clause_t &c = cnf[cid];
    for (literal_t l : c) {
      literal_to_clauses[l].push_back(cid);
    }
  }

  auto lits = cnf.lit_range();

  // Create our initial worklist:
  std::vector<literal_t> worklist;
  std::for_each(std::begin(lits), std::end(lits),
                [&worklist](literal_t l) { worklist.push_back(l); });
  std::sort(std::begin(worklist), std::end(worklist),
            [&literal_to_clauses](literal_t l1, literal_t l2) {
              return literal_to_clauses[l1].size() >
                     literal_to_clauses[l2].size();
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
    literal_t l = worklist.back();
    worklist.pop_back();

    auto &CL = literal_to_clauses[l];
    const auto &DL = literal_to_clauses[neg(l)];
    const auto orig_size = result.size();

    for (clause_id cid : CL) {
      // If we've already eliminated this, skip.
      if (contains(result, cid)) continue;

      bool is_blocked = std::all_of(
          std::begin(DL), std::end(DL), [l, cid, &cnf](clause_id did) {
            return resolve_taut(cnf[cid], cnf[did], l);
          });

      if (!is_blocked) continue;

      result.push_back(cid);

      for (literal_t m : cnf[cid]) {
        if (!contains(worklist, neg(m))) {
          worklist.push_back(neg(m));
        }
      }
    }

    // To facilitate the loop, we remove the clauses
    // from our own data structure:
    auto orig_end = result.begin() + orig_size;
    std::for_each(orig_end, std::end(result), [&](clause_id cid) {
      for (literal_t l : cnf[cid]) {
        auto &cl = literal_to_clauses[l];
        cl.remove(cid);
        // auto dit = std::remove(std::begin(cl), std::end(cl), cid);
        // cl.erase(dit, std::end(cl));
      }
    });
  }
  // if (result.size()) std::cerr << "[BCE] Total clauses removed: " <<
  // result.size() << std::endl; if (result.size()) std::cerr << result.size()
  // << std::endl;

  return result;
}
