#include "preprocess.h"
#include <iostream>
#include <map>
#include <cassert>

// PRE = preprocess
// NUP = Niave unit prop
// PLE = Pure literal elimination

literal_t find_pure_literal(const cnf_t& cnf) {
  std::vector<literal_t> positives;
  std::vector<literal_t> negatives;
  for (const clause_t& c : cnf) {
    for (const literal_t l : c) {
      if (l < 0) {
        if (!contains(negatives, -l)) {
          negatives.push_back(-l);
        }
      }
      if (l > 0) {
        if (!contains(positives, l)) {
          positives.push_back(l);
        }
      }
    }
  }
  std::sort(std::begin(positives), std::end(positives));
  std::sort(std::begin(negatives), std::end(negatives));
  // Do a "merge":
  auto pt = std::begin(positives);
  auto nt = std::begin(negatives);
  while (pt != std::end(positives) &&
         nt != std::end(negatives)) {
    if (*pt < *nt) {
      return *pt;
    }
    if (*nt < *pt) {
      return -*nt;
    }
    pt++;
    nt++;
  }
  return 0;
}

bool clauses_self_subsume(variable_t v, const clause_t& pclause, const clause_t& nclause) {
  auto pit = std::begin(pclause);
  auto nit = std::begin(nclause);
  while (pit != std::end(pclause) &&
         nit != std::end(nclause)) {
    if (*pit == v) {
      pit++;
      continue;
    }
    if (*nit == -v) {
      nit++;
      continue;
    }
    if (*pit != *nit) {
      return false;
    }
    pit++;
    nit++;
  }
  return pit == std::end(pclause) && nit == std::end(nclause);
}
void naive_self_subsumption(cnf_t& cnf) {
  std::map<literal_t, std::vector<clause_id>> literal_to_clauses;
  variable_t max_var = 0;
  for (clause_id i = 0; i < cnf.size(); i++) {
    for (literal_t l : cnf[i]) {
      literal_to_clauses[l].push_back(i);
      max_var = std::max(std::abs(l), max_var);
    }
  }
  for (variable_t v = 1; v < max_var+1; v++) {
    const auto& pos_clause_list = literal_to_clauses[v];
    const auto& neg_clause_list = literal_to_clauses[-v];
    for (auto pcid : pos_clause_list) {
      for (auto ncid : neg_clause_list) {
        const auto& pclause = cnf[pcid];
        const auto& nclause = cnf[ncid];
        if (clauses_self_subsume(v, pclause, nclause)) {
        }
      }
    }
  }
}

std::vector<clause_id> BCE(cnf_t& cnf);
void preprocess(cnf_t& cnf) {
  // Niavely unit-prop
  bool did_work = true;
  // this will be useful for self-subsumption.
  std::for_each(std::begin(cnf), std::end(cnf), [](auto& c) { std::sort(std::begin(c), std::end(c)); });
  while (did_work) {
    did_work = false;
    while (literal_t u = find_unit(cnf)) {
      //std::cout << "[PRE][NUP] " << u << std::endl;
      commit_literal(cnf, u);
      did_work = true;
    }
    //while (literal_t p = find_pure_literal(cnf)) {
    //  std::cout << "[PRE][PLE] " << p << std::endl;
    //  commit_literal(cnf, p);
    //  did_work = true;
    //}

    //naive_self_subsumption(cnf);
    if (true) {
      std::vector<clause_id> bc = BCE(cnf);
      std::sort(std::begin(bc), std::end(bc), [](clause_id c1, clause_id c2) { return c2 < c1; });
#if 0
      std::cout << "To remove clause_ids: (should be from highest index to lowest: " << std::endl;
      for (clause_id cid : bc) {
        std::cout << " " << cid;
      }
      std::cout << std::endl;
      std::cout << "Removing: " << bc.size() << " from " << cnf.size() << std::endl;
      std::cout << cnf << std::endl;
#endif
      auto end_iterator = std::prev(cnf.end());
      for (clause_id i : bc) {
        //std::cout << "  Removing clause " << cnf[i] << std::endl;
        std::swap(cnf[i], *end_iterator);
        end_iterator--;
        did_work = true;
      }
      cnf.erase(std::next(end_iterator), std::end(cnf));
      //std::cout << "Removed: " << cnf.size() << std::endl;
      //std::cout << cnf << std::endl;
    }
  }
}
