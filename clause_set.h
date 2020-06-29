#pragma once
#include <vector>
typedef size_t clause_id;
// Really a "clause set", we don't promise anything about order.
// Using a vector is faster (?!) than just the array. Presumably I'm doing
// something silly.
struct clause_set_t {
  // clause_id* mem;
  // size_t c;
  // size_t s;
  std::vector<clause_id> mem;

  clause_set_t();
  clause_set_t(const clause_set_t& that);
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
  bool empty() const { return mem.empty(); }
  void pop_back() { mem.pop_back(); }
  clause_id back() const { return mem.back(); }
};
