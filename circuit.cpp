#include "cnf.h"
#include "literal_incidence_map.h"
// Experiments finding a circuit.

// Given a CNF, find all candidates that can lead to a binary clause
std::vector<clause_t> binary_saturation(const cnf_t& cnf) {
  std::vector<clause_t> res;
  auto m = build_incidence_map(cnf);
  for (literal_t l = m.first_index(); l < 0; ++l) {
    if (!l) continue;
    const auto& cl = m[l];
    const auto& dl = m[-l];
    for (auto cid : cl) {
      for (auto did : dl) {
        const clause_t& c = cnf[cid];
        const clause_t& d = cnf[did];
        clause_t e = resolve(c, d, l);
        if (e.size() == 2) {
          std::sort(std::begin(e), std::end(e));
          res.push_back(e);
          //std::cerr << "Found binary over " << l << ": " << e << " from " << c << " and " << d << std::endl;
        }
      }
    }
  }
  return res;
}
