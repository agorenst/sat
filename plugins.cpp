#include "plugins.h"

static std::vector<remove_clause_listener> remove_clause_listeners;

void remove_clause(clause_id cid) {
  for (auto f : remove_clause_listeners) {
    f(cid);
  }
}
void remove_clause_listener_reg(remove_clause_listener r) {
  remove_clause_listeners.push_back(r);
}
