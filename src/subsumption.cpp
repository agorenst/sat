#include <algorithm>
#include <iterator>
#include <memory>
#include <vector>

#include "cnf.h"

std::unique_ptr<literal_map_t<clause_set_t>> literal_to_clause;

bool subsumes_and_sort(const clause_t &c, const clause_t &d) {
  // Keep the memory pool as large as needed.
  static std::vector<literal_t> cbuff;
  static std::vector<literal_t> dbuff;

  cbuff.clear();
  dbuff.clear();

  std::copy(std::begin(c), std::end(c), std::back_inserter(cbuff));
  std::copy(std::begin(d), std::end(d), std::back_inserter(dbuff));

  // Basic early-out. Also needed for correctness (otherwise c always subsumes
  // c)
  SAT_ASSERT(d.size() > c.size());
  SAT_ASSERT(c.possibly_subsumes(d));

  std::sort(std::begin(cbuff), std::end(cbuff));
  std::sort(std::begin(dbuff), std::end(dbuff));

  // This says c is a subset of d.
  return std::includes(std::begin(dbuff), std::end(dbuff), std::begin(cbuff),
                       std::end(cbuff));
}

bool subsumes(const clause_t &c, const clause_t &d) {
  // Basic early-out. Also needed for correctness (otherwise c always subsumes
  // c)
  if (d.size() <= c.size()) return false;

  SAT_ASSERT(std::is_sorted(std::begin(c), std::end(c)));
  SAT_ASSERT(std::is_sorted(std::begin(d), std::end(d)));

  // This says c is a subset of d.
  return std::includes(std::begin(d), std::end(d), std::begin(c), std::end(c));
}

std::vector<clause_id> find_subsumed(cnf_t &cnf, const clause_t &c) {
  // Find the literal with the shortest occur list.
  auto compare_incidence_list = [&](literal_t a, literal_t b) {
    return (*literal_to_clause)[a].size() < (*literal_to_clause)[b].size();
  };
  literal_t l =
      *std::min_element(std::begin(c), std::end(c), compare_incidence_list);

  const auto &cids = (*literal_to_clause)[l];

  // Find everything in that occur list that we subsume.
  std::vector<clause_id> result;
  auto result_insert = std::back_inserter(result);
  std::copy_if(std::begin(cids), std::end(cids), result_insert,
               [&](clause_id cid) { return subsumes(c, cnf[cid]); });

  return result;
}

std::vector<clause_id> self_subsume(cnf_t &cnf, clause_id cid) {
  // deliberately *copying* here.
  std::vector<clause_id> strengthened;
  std::vector<literal_t> c;
  for (literal_t l : cnf[cid]) {
    c.push_back(l);
  }
  for (size_t i = 0; i < c.size(); i++) {
    c[i] = neg(c[i]);
    std::sort(std::begin(c), std::end(c));
    auto subsumes = find_subsumed(cnf, c);
    for (auto did : subsumes) {
      clause_t &d = cnf[did];
      // std::cerr << "[SUB] Shrinking " << d << " -> ";
      auto dit = std::remove(std::begin(d), std::end(d), c[i]);
      SAT_ASSERT(dit != std::end(d));
      auto to_erase = std::distance(dit, std::end(d));
      for (auto i = 0; i < to_erase; i++) {
        d.pop_back();
      }
      // d.erase(dit, std::end(d));
      // std::cerr << d << std::endl;
      strengthened.push_back(did);
    }
    c[i] = neg(c[i]);
    std::sort(std::begin(c), std::end(c));
  }
  return strengthened;
}

size_t naive_self_subsume(cnf_t &cnf) {
  std::for_each(std::begin(cnf), std::end(cnf), [&](clause_id cid) {
    std::sort(std::begin(cnf[cid]), std::end(cnf[cid]));
  });

  literal_to_clause =
      std::make_unique<literal_map_t<clause_set_t>>(max_variable(cnf));
  for (clause_id cid : cnf) {
    const clause_t &c = cnf[cid];
    for (literal_t l : c) {
      (*literal_to_clause)[l].push_back(cid);
    }
  }

  bool did_work = true;
  size_t total_strengthened = 0;
  while (did_work) {
    did_work = false;
    for (auto cid : cnf) {
      auto res = self_subsume(cnf, cid);
      if (res.size() > 0) {
        std::cerr << "tripped" << std::endl;
        did_work = true;
        total_strengthened += res.size();
      }
    }
  }

  literal_to_clause.release();

  return total_strengthened;
}