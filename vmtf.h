
#include <list>
#include "cnf.h"
#include "trail.h"

struct vmtf_t {
  using queue_t = std::list<variable_t>;

  queue_t q;
  var_map_t<queue_t::iterator> pos;
  var_map_t<size_t> timestamp;
  queue_t::iterator next_to_choose;
  size_t conflict_index = 0;
  const cnf_t &cnf;
  const trail_t &trail;

  vmtf_t(const cnf_t &cnf, const trail_t &trail);
  void debug();

  void clause_learned(const clause_t &c);
  void bump(variable_t v);

  variable_t choose();
  void invariant();

  void unassign(variable_t v);
};
