#pragma once
#include <vector>
#include "cnf.h"
// Really a "clause set", we don't promise anything about order.
struct clause_list_t {
  std::vector<clause_id> mem;
  clause_id* begin();
  clause_id* end();

  const clause_id* begin() const;
  const clause_id* end() const;

  void push_back(clause_id cid);
  clause_id& operator[](const size_t i);
  clause_id& operator[](const int i);
  void remove(clause_id cid);
  void clear();
  size_t size() const;
};
