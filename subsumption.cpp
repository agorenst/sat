#include <vector>
#include <algorithm>
#include <iterator>

#include "trace.h"
#include "cnf.h"

bool subsumes(const clause_t& c, const clause_t& d) {

  // Basic early-out. Also needed for correctness (otherwise c always subsumes c)
  if (d.size() <= c.size()) return false;

  assert(std::is_sorted(std::begin(c), std::end(c)));
  assert(std::is_sorted(std::begin(d), std::end(d)));

  // This says c is a subset of d.
  return std::includes(std::begin(d), std::end(d),
                       std::begin(c), std::end(c));
}

std::vector<clause_id> find_subsumed(cnf_t& cnf, const clause_t& c) {

  literal_incidence_map_t literal_to_clause(cnf);
  for (clause_id cid : cnf) {
    const clause_t& c = cnf[cid];
    for (literal_t l : c) {
      literal_to_clause[l].push_back(cid);
    }
  }

  // Find the literal with the shortest occur list.
  auto compare_incidence_list = [&](literal_t a, literal_t b) {
                                  return literal_to_clause[a].size() < literal_to_clause[b].size();
                                };
  literal_t l = *std::min_element(std::begin(c), std::end(c), compare_incidence_list);

  const auto& cids = literal_to_clause[l];

  // Find everything in that occur list that we subsume.
  std::vector<clause_id> result;
  auto result_insert = std::back_inserter(result);
  std::copy_if(std::begin(cids), std::end(cids), result_insert,
               [&](clause_id cid) {
                 return subsumes(c, cnf[cid]);
               });

  return result;
}
