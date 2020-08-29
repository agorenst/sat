#include "cnf.h"
#include "trail.h"
struct acids_t {
  const cnf_t &cnf;
  const trail_t &trail;
  const variable_range vr;
  var_map_t<size_t> score;
  size_t conflict_index;

  acids_t(const cnf_t &cnf, const trail_t &trail);
  void clause_learned(const clause_t &c);
  literal_t choose();
};
