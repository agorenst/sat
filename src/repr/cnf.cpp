// This is the package the provides
#include "cnf.h"

#include <algorithm>
#include <cassert>
#include <forward_list>
#include <iostream>
#include <sstream>
#include <string>

#include "debug.h"
#include "settings.h"

std::ostream &operator<<(std::ostream &o, const cnf_t &cnf) {
  std::for_each(std::begin(const_clauses(cnf)), std::end(const_clauses(cnf)),
                [&o](const clause_t &c) { o << c << "0\n"; });
  return o;
}

variable_t max_variable(const cnf_t &cnf) {
  literal_t m = 0;
  for (const clause_id cid : cnf) {
    for (const literal_t l : cnf[cid]) {
      m = std::max(var(l), m);
    }
  }
  return m;
}

clause_id cnf_t::add_clause(clause_t c) {
  live_count++;

  auto ret = new clause_t(std::move(c));

  if (!head) {
    head = ret;
  } else {
    head->left = ret;
    ret->right = head;
    head = ret;
  }
  return ret;
}

void cnf_t::remove_clause_set(const clause_set_t &cs) {
  for (auto cid : cs) {
    remove_clause(cid);
  }
}

void cnf_t::remove_clause(clause_id cid) {
  if (cid->is_alive) {
    live_count--;
    cid->is_alive = false;
    if (cid->left) {
      cid->left->right = cid->right;
    }
    if (cid->right) {
      cid->right->left = cid->left;
    }
    if (cid == head) {
      head = cid->right;
    }
    to_erase.push_back(cid);
  }
}

void cnf_t::restore_clause(clause_id cid) {
  SAT_ASSERT(!cid->is_alive);
  cid->is_alive = true;
  live_count++;

  if (!head) {
    head = cid;
  } else {
    head->left = cid;
    cid->right = head;
    head = cid;
  }
}

void cnf_t::clean_clauses() {
  for (auto cid : to_erase) {
    delete cid;
  }
  to_erase.clear();
}

literal_map_t<clause_set_t> build_incidence_map(const cnf_t &cnf) {
  literal_map_t<clause_set_t> literal_to_clause(max_variable(cnf));
  for (clause_id cid : cnf) {
    const clause_t &c = cnf[cid];
    for (literal_t l : c) {
      literal_to_clause[l].push_back(cid);
    }
  }
  return literal_to_clause;
}

bool check_incidence_map(const literal_map_t<clause_set_t> &m,
                         const cnf_t &cnf) {
#ifdef SAT_DEBUG_MODE
  for (clause_id cid : cnf) {
    const clause_t &c = cnf[cid];
    for (literal_t l : c) {
      SAT_ASSERT(contains(m[l], cid));
    }
  }
  for (literal_t l : cnf.lit_range()) {
    const auto cl = m[l];
    for (auto cid : cl) {
      SAT_ASSERT(contains(cnf[cid], l));
    }
    for (auto it = std::begin(cl); it != std::end(cl); it++) {
      for (auto jt = std::next(it); jt != std::end(cl); jt++) {
        SAT_ASSERT(*it != *jt);
      }
    }
  }
#endif
  return true;
}
namespace cnf {
namespace transform {

void commit_literal(cnf_t &cnf, literal_t l) {
  std::vector<clause_id> containing_clauses;
  std::copy_if(std::begin(cnf), std::end(cnf),
               std::back_inserter(containing_clauses),
               [&](const clause_id cid) { return contains(cnf[cid], l); });
  std::for_each(std::begin(containing_clauses), std::end(containing_clauses),
                [&](const clause_id cid) { cnf.remove_clause(cid); });
  for (clause_t &c : clauses(cnf)) {
    auto new_end = std::remove(std::begin(c), std::end(c), neg(l));
    // c.erase(new_end, std::end(c));
    auto to_erase = std::distance(new_end, std::end(c));
    for (auto i = 0; i < to_erase; i++) {
      c.pop_back();
    }
  }
}
void canon(cnf_t &cnf) {
  // Remove all tautologies
  std::vector<clause_id> to_remove;
  std::copy_if(std::begin(cnf), std::end(cnf), std::back_inserter(to_remove),
               [&cnf](clause_id cid) { return clause_taut(cnf[cid]); });
  for (auto cid : to_remove) {
    cnf.remove_clause(cid);
  }
  cnf.clean_clauses();
  to_remove.clear();

  // in-place canon---basically, remove redundant literals.
  for (auto cid : cnf) {
    canon_clause(cnf[cid]);
  }

  // We remove redundant clauses (clauses that represent the same set of
  // literals) through "hash them, and if any end up in the same bucket, really
  // see if they're equal" technique.
  std::unordered_map<int64_t, std::vector<clause_id>> m;
  for (auto cid : cnf) {
    int64_t key = cnf[cid].signature();
    m[key].push_back(cid);
  }

  for (auto &[k, cs] : m) {
    if (cs.size() < 2) {
      continue;
    }
    std::for_each(std::begin(cs), std::end(cs), [&](auto c) {
      std::sort(std::begin(cnf[c]), std::end(cnf[c]));
    });
    std::sort(std::begin(cs), std::end(cs),
              [&](auto a, auto b) { return cnf[a] < cnf[b]; });

    // This is basically std::unique, but we have to call to_remove.push_back
    // (std::unique can trash the values "in the suffix" it leaves.)
    auto input = std::begin(cs);
    auto output = std::begin(cs);
    while (input != std::end(cs)) {
      // if input and output have the same value, but are not the same iterator,
      // "skip" that input.
      if (cnf[*input] == cnf[*output] && input != output) {
        to_remove.push_back(*input);
        input++;
        continue;
      } else {
        *output++ = *input++;
      }
    }
  }

  for (auto cid : to_remove) {
    cnf.remove_clause(cid);
  }
  cnf.clean_clauses();
}
int apply_trivial_units(cnf_t &cnf) {
  int hitcount = 0;
  while (literal_t u = cnf::search::find_unit(cnf)) {
    commit_literal(cnf, u);
    hitcount++;
  }
  return hitcount;
}
}  // namespace transform

namespace search {
literal_t find_unit(const cnf_t &cnf) {
  auto it =
      std::find_if(std::begin(cnf), std::end(cnf),
                   [&](const clause_id cid) { return cnf[cid].size() == 1; });
  if (it != std::end(cnf)) {
    return cnf[*it][0];
  }
  return 0;
}
bool immediately_unsat(const cnf_t &cnf) {
  for (clause_id cid : cnf) {
    if (cnf[cid].empty()) {
      return true;
    }
  }
  return false;
}
bool immediately_sat(const cnf_t &cnf) { return cnf.live_clause_count() == 0; }
}  // namespace search

namespace io {
bool load_cnf(const char *buffer, size_t s, cnf_t &cnf) {
  std::vector<literal_t> next_clause_tmp;

  std::string cppbuffer{buffer, s};
  std::istringstream ss(cppbuffer);
  std::string line;

  while (std::getline(ss, line)) {
    std::istringstream ws(line);
    std::vector<std::string> words;
    std::copy(std::istream_iterator<std::string>(ws),
              std::istream_iterator<std::string>(), std::back_inserter(words));

    if (words.size() == 0) continue;
    if (words[0] == "c") continue;
    if (words[0] == "p") continue;

    for (const std::string &w : words) {
      size_t chars_read = 0;
      literal_t l = 0;
      try {
        l = std::stol(w, &chars_read);
      } catch (std::invalid_argument) {
        return false;
      } catch (std::out_of_range) {
        return false;
      }

      // Parse error: didn't get an int we expected.
      if (chars_read != w.size()) {
        return false;
      }

      if (l == 0) {
        std::sort(std::begin(next_clause_tmp), std::end(next_clause_tmp));
        cnf.add_clause(next_clause_tmp);
        next_clause_tmp.clear();
      } else {
        next_clause_tmp.push_back(dimacs_to_lit(l));
      }
    }
  }
  if (next_clause_tmp.size() > 0) {
    return false;
  }
  return true;
}
}  // namespace io
}  // namespace cnf

// REVIEW: put these in a namespace, too?
bool operator==(const const_clauses::iterator &a,
                const const_clauses::iterator &b) {
  return a.it == b.it;
}
bool operator!=(const const_clauses::iterator &a,
                const const_clauses::iterator &b) {
  return a.it != b.it;
}
bool operator==(const clauses::iterator &a, const clauses::iterator &b) {
  return a.it == b.it;
}
bool operator!=(const clauses::iterator &a, const clauses::iterator &b) {
  return a.it != b.it;
}