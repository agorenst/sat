#include "cnf.h"

struct literal_incidence_map_t {
  // The backing memory
  std::vector<std::vector<clause_id>> mem;
  variable_t max_var = 0;

  // This does the index mapping for you!
  std::vector<clause_id>& operator[](literal_t l);
  const std::vector<clause_id>& operator[](literal_t l) const;
  // Create the mapping.
  literal_incidence_map_t(const cnf_t& cnf);
  void populate(const cnf_t& cnf);

  auto begin() { return mem.begin(); }
  auto end() { return mem.end(); }
  auto begin() const { return mem.begin(); }
  auto end() const { return mem.end(); }

  literal_t first_index() const { return -max_var; }
  literal_t end_index() const { return max_var+1; }

};
