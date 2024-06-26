#include "preprocess.h"

#include <cassert>
#include <iostream>
#include <map>

#include "bce.h"
#include "subsumption.h"

#include "measurements.h"
#include "settings.h"

literal_t find_pure_literal(const cnf_t &cnf) {
  std::vector<literal_t> positives;
  std::vector<literal_t> negatives;
  for (const clause_id cid : cnf) {
    const clause_t &c = cnf[cid];
    for (const literal_t l : c) {
      if (l < 0) {
        if (!contains(negatives, neg(l))) {
          negatives.push_back(neg(l));
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
  while (pt != std::end(positives) && nt != std::end(negatives)) {
    if (*pt < *nt) {
      return *pt;
    }
    if (*nt < *pt) {
      return neg(*nt);
    }
    pt++;
    nt++;
  }
  return 0;
}

bool clauses_self_subsume(variable_t v, const clause_t &pclause,
                          const clause_t &nclause) {
  auto pit = std::begin(pclause);
  auto nit = std::begin(nclause);
  while (pit != std::end(pclause) && nit != std::end(nclause)) {
    if (*pit == v) {
      pit++;
      continue;
    }
    if (*nit == neg(v)) {
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
void naive_self_subsumption(cnf_t &cnf) {
  std::map<literal_t, std::vector<clause_id>> literal_to_clauses;
  for (clause_id i : cnf) {
    for (literal_t l : cnf[i]) {
      literal_to_clauses[l].push_back(i);
    }
  }

  for (variable_t v : cnf.var_range()) {
    const auto &pos_clause_list = literal_to_clauses[lit(v)];
    const auto &neg_clause_list = literal_to_clauses[neg(v)];
    for (auto pcid : pos_clause_list) {
      for (auto ncid : neg_clause_list) {
        const auto &pclause = cnf[pcid];
        const auto &nclause = cnf[ncid];
        if (clauses_self_subsume(v, pclause, nclause)) {
          std::cerr << "trip" << std::endl;
        }
      }
    }
  }
}

void preprocess(cnf_t &cnf) {
  cond_log(settings::time_preprocess, solver_action::preprocessor_start,
           std::chrono::steady_clock::now());
  // Niavely unit-prop
  bool did_work = true;
  // this will be useful for self-subsumption.
  std::for_each(std::begin(cnf), std::end(cnf), [&](clause_id cid) {
    clause_t &c = cnf[cid];
    std::sort(std::begin(c), std::end(c));
  });

  while (did_work) {
    did_work = false;
    did_work |= cnf::transform::apply_trivial_units(cnf) > 0;

    std::vector<clause_id> bc = BCE(cnf);
    std::sort(std::begin(bc), std::end(bc),
              [](clause_id c1, clause_id c2) { return c2 < c1; });
#if 0
    std::cout << "To remove clause_ids: (should be from highest index to lowest: " << std::endl;
    for (clause_id cid : bc) {
      std::cout << " " << cid;
    }
    std::cout << std::endl;
    std::cout << "Removing: " << bc.size() << " from " << cnf.size() << std::endl;
    std::cout << cnf << std::endl;
#endif
    std::for_each(std::begin(bc), std::end(bc),
                  [&](const clause_id cid) { cnf.remove_clause(cid); });

    did_work |= cnf::transform::apply_trivial_units(cnf) > 0;

    bool BVE(cnf_t & cnf);
    if (settings::preprocess_bve && BVE(cnf)) {
      // std::cerr << "BVE" << std::endl;
      did_work = true;
    }

    did_work |= cnf::transform::apply_trivial_units(cnf) > 0;

    bool VIV(cnf_t & cnf);
    if (!settings::no_viv_preprocess && VIV(cnf)) {
      // this has correctness issues?
      did_work = true;
    }

    did_work |= cnf::transform::apply_trivial_units(cnf) > 0;

    /*
        std::for_each(std::begin(cnf), std::end(cnf), [&cnf](clause_id cid) {
          std::sort(std::begin(cnf[cid]), std::end(cnf[cid]));
        });

        for (auto cid : cnf) {
            const auto& c = cnf[cid];
          for (auto did : cnf) {
            const auto& d = cnf[did];
            if (c.possibly_subsumes(d) &&
                subsumes(c, d)) {
                  std::cerr << "SUBSUMPTION: " << c << "; " << d << std::endl;
            }
          }
        }

        naive_self_subsumption(cnf);
        */

    /*
      // THIS HAS A CORRECTNESS BUG???
    auto v = binary_saturation(cnf);
    for (auto e : v) {
      if (std::all_of(std::begin(cnf), std::end(cnf),
                       [&](clause_id cid) {
                         return cnf[cid] != e;
                       })) {
        cnf.push_back(e);
        did_work = true;
      }
    }
    */
  }
  if (settings::time_preprocess) {
    cond_log(settings::time_preprocess, solver_action::preprocessor_end,
             std::chrono::steady_clock::now());
  }
  // std::cerr << "Done preprocessing" << std::endl;
}

// This should be entirely subsumed by BCE
