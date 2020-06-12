#include <forward_list>
#include "literal_incidence_map.h"

template<typename T>
size_t literal_map_t<T>::literal_to_index(literal_t l) const {
  return l + max_var;
}

template <typename T>
T& literal_map_t<T>::operator[](literal_t l) {
  size_t i = literal_to_index(l);
  return mem[i];
}

template <typename T>
const T& literal_map_t<T>::operator[](literal_t l) const {
  size_t i = literal_to_index(l);
  return mem[i];
}

template <typename T>
literal_map_t<T>::literal_map_t(const cnf_t& cnf) {
  max_var = max_variable(cnf);
  mem.resize(max_var * 2 + 2);
}

template <typename T>
literal_t literal_map_t<T>::iter_to_literal(
    typename mem_t::const_iterator it) const {
  size_t index = std::distance(std::begin(mem), it);
  return index - max_var;
}


literal_map_t<clause_set_t> build_incidence_map(const cnf_t& cnf) {
  literal_map_t<clause_set_t> literal_to_clause(cnf);
  for (clause_id cid : cnf) {
    const clause_t& c = cnf[cid];
    for (literal_t l : c) {
      literal_to_clause[l].push_back(cid);
    }
  }
  return literal_to_clause;
}

bool check_incidence_map(const literal_map_t<clause_set_t>& m, const cnf_t& cnf) {
  for (clause_id cid : cnf) {
    const clause_t& c = cnf[cid];
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
  return true;
}

// We explicitly instantiate templates because we know how we're
// going to be using this type.
template struct literal_map_t<clause_set_t>;

// for VSIDS
template struct literal_map_t<float>;

// For lcm helper?
template struct literal_map_t<int>;

// For TWl
template struct literal_map_t<std::forward_list<clause_id>>;
template struct literal_map_t<std::vector<std::pair<clause_id, literal_t>>>;
