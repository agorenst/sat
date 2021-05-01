#include "trail.h"
#include "variable.h"
struct solver_t;
struct positive_only_literal_chooser_t {
  std::vector<variable_t> vars;
  const trail_t& trail;
  positive_only_literal_chooser_t(variable_t max_var, const trail_t& trail);
  literal_t choose() const;
};