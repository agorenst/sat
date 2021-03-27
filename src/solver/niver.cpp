#include <map>
#include "cnf.h"
class NIVER {
  struct metrics_t {
    size_t tauts_found = 0;
    size_t resolvents_tried = 0;
  };

  metrics_t metrics;
  cnf_t &cnf;
  size_t cnf_size = 0;

  literal_map_t<clause_set_t> literals_to_clauses;
  literal_map_t<size_t> occurrence_count;

  const float growth_factor = 1.0;

  mutable clause_set_t cp, cn;
  mutable std::vector<clause_t> r;

  struct entry_t {
    variable_t v;
    size_t count;
    bool operator<(const entry_t &e) const { return count < e.count; }
  };

  bool is_taut(const clause_t &c) const {
    for (auto it = std::begin(c); it != std::end(c); it++) {
      for (auto jt = std::next(it); jt != std::end(c); jt++) {
        if (*it == neg(*jt)) {
          return true;
        }
      }
    }
    return false;
  }

  void remove_clause(clause_id cid) {
    for (literal_t l : cnf[cid]) {
      literals_to_clauses[l].remove(cid);
    }
    cnf.remove_clause(cid);
  }
  void add_clause(clause_t &&c) {
    auto cid = cnf.add_clause(std::move(c));
    for (literal_t l : cnf[cid]) {
      literals_to_clauses[l].push_back(cid);
    }
  }

 public:
  std::vector<entry_t> worklist;
  NIVER(cnf_t &cnf, float growth_factor)
      : cnf(cnf),
        literals_to_clauses(max_variable(cnf)),
        occurrence_count(max_variable(cnf)),
        growth_factor(growth_factor) {
    // build a mapping of literals to clauses.

    for (clause_id cid : cnf) {
      SAT_ASSERT(!is_taut(cnf[cid]));
      const clause_t &c = cnf[cid];

      cnf_size += c.size();

      for (literal_t l : c) {
        occurrence_count[var(l)]++;
        SAT_ASSERT(!contains(literals_to_clauses[l], cid));
        literals_to_clauses[l].push_back(cid);
      }
    }

    for (literal_t l : cnf.lit_range()) {
      auto &cl = literals_to_clauses[l];
      std::sort(std::begin(cl), std::end(cl));
      SAT_ASSERT(std::unique(std::begin(cl), std::end(cl)) == std::end(cl));
    }

    for (variable_t v : cnf.var_range()) {
      worklist.push_back({v, occurrence_count[v]});
    }
    std::sort(std::begin(worklist), std::end(worklist));
  }

  bool report_possibility(variable_t v) const {
    literal_t p = lit(v);
    literal_t n = neg(p);
    // These collections will transform when we remove clauses,
    // so make a copy
    const auto &CP = literals_to_clauses[p];
    const auto &CN = literals_to_clauses[n];

    size_t csize = 0;
    for (auto pid : CP) {
      csize += cnf[pid].size();
    }
    for (auto nid : CN) {
      csize += cnf[nid].size();
    }

    // compute resolvents.
    size_t rsize = 0;
    r.clear();
    for (auto pid : CP) {
      for (auto nid : CN) {
        clause_t c = resolve_ref(cnf[pid], cnf[nid], p);
        // metrics.resolvents_tried++;
        if (is_taut(c)) {
          // metrics.tauts_found++;
          continue;
        }
        rsize += c.size();
        r.push_back(std::move(c));
      }
    }

    if (!rsize) return false;

    if ((growth_factor * csize) < rsize) {
      return false;
    }

    // export our results as needed
    cp = CP;
    cn = CN;

    return true;
  }

  bool any_possibility() const {
    for (variable_t v : cnf.var_range()) {
      if (report_possibility(v)) {
        return true;
      }
    }
    return false;
  }

  bool try_resolve(variable_t v) {
    // this method also initializes all the metadata
    // we are counting on to actually resolve this
    if (!report_possibility(v)) {
      return false;
    }

    for (auto pid : cp) {
      remove_clause(pid);
    }
    for (auto nid : cn) {
      remove_clause(nid);
    }
    for (auto &&c : r) {
      add_clause(std::move(c));
    }
    cnf.clean_clauses();
    return true;
  }
};

bool BVE(cnf_t &cnf) {
  NIVER niv(cnf, 1.3);
  bool continue_work = true;
  bool did_work = false;
  while (continue_work) {
    continue_work = false;
    // for (variable_t v : cnf.var_range()) {
    for (auto [v, c] : niv.worklist) {
      if (niv.try_resolve(v)) {
        // std::cerr << "DID WORK " << v << std::endl;
        continue_work = true;
        did_work = true;
      }
    }
  }
  return did_work;
}

bool can_BVE(cnf_t &cnf) {
  NIVER niv(cnf, 1.0);
  return niv.any_possibility();
}