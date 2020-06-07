#include "clause_set.h"
// Really a "clause set", we don't promise anything about order.
clause_id* clause_list_t::begin() { return &mem[0]; }
clause_id* clause_list_t::end() { return begin()+mem.size(); }

const clause_id* clause_list_t::begin() const { return &mem[0]; }
const clause_id* clause_list_t::end() const { return begin()+mem.size(); }

void clause_list_t::push_back(clause_id cid) {
  mem.push_back(cid);
}
clause_id& clause_list_t::operator[](const size_t i) {
  return mem[i];
}
clause_id& clause_list_t::operator[](const int i) {
  return mem[i];
}
void clause_list_t::remove(clause_id cid) {
  SAT_ASSERT(contains(*this, cid));
  clause_id* e = end();
  clause_id* r = std::find(begin(), e, cid);
  SAT_ASSERT(r >= begin());
  SAT_ASSERT(r < e);
  std::swap(*(e-1), *r);
  mem.pop_back();
  SAT_ASSERT(!contains(*this, cid));
}
void clause_list_t::clear() {
  mem.clear();
}
size_t clause_list_t::size() const {
  return mem.size();
}
