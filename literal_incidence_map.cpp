#include "literal_incidence_map.h"

static size_t literal_to_index(literal_t l) {
  if (l < 0) {
    return std::abs(l) * 2 + 1;
  }
  else {
    return l * 2;
  }
}



template<typename T>
T& literal_map_t<T>::operator[](literal_t l) {
  size_t i = literal_to_index(l);
  return mem[i];
}

template<typename T>
const T& literal_map_t<T>::operator[](literal_t l) const {
  size_t i = literal_to_index(l);
  return mem[i];
}

template<typename T>
literal_map_t<T>::literal_map_t(const cnf_t& cnf) {

  max_var = max_variable(cnf);
  mem.resize(max_var*2+2);
}

template<typename T>
literal_t literal_map_t<T>::iter_to_literal(typename mem_t::const_iterator it) const {
  size_t index = std::distance(std::begin(mem), it);
  if (index % 2) {
    return -(index / 2);
  }
  return index / 2;
}

// We explicitly instantiate templates because we know how we're
// going to be using this type.
template struct literal_map_t<std::vector<clause_id>>;

// for VSIDS
template struct literal_map_t<float>;
