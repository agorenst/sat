#include "literal_incidence_map.h"

size_t literal_to_index(literal_t l) {
  if (l < 0) {
    return std::abs(l) * 2 + 1;
  }
  else {
    return l * 2;
  }
}



std::vector<clause_id>& literal_incidence_map_t::operator[](literal_t l) {
  size_t i = literal_to_index(l);
  return mem[i];
}

const std::vector<clause_id>& literal_incidence_map_t::operator[](literal_t l) const {
  size_t i = literal_to_index(l);
  return mem[i];
}


// Create the mapping.
void literal_incidence_map_t::populate(const cnf_t& cnf) {
  for (clause_id cid = 0; cid < cnf.size(); cid++) {
    const clause_t& c = cnf[cid];
    for (literal_t l : c) {
      (*this)[l].push_back(cid);
    }
  }
}
literal_incidence_map_t::literal_incidence_map_t(const cnf_t& cnf) {

  max_var = max_variable(cnf);
  mem.resize(max_var*2+2);
}
