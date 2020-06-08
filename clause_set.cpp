#include "clause_set.h"
// Really a "clause set", we don't promise anything about order.

clause_list_t::clause_list_t() {
  //c = 16;
  //s = 0;
  //mem = (clause_id*) malloc(c * sizeof(clause_id));
}
clause_list_t::clause_list_t(const clause_list_t& that) {
  //c = 16;
  //s = 0;
  //mem = (clause_id*) malloc(c * sizeof(clause_id));
  for (clause_id cid : that) {
    push_back(cid);
  }
}

clause_id* clause_list_t::begin() { return &mem[0]; }
clause_id* clause_list_t::end() { return begin()+size(); }

const clause_id* clause_list_t::begin() const { return &mem[0]; }
const clause_id* clause_list_t::end() const { return begin()+size(); }

void clause_list_t::push_back(clause_id cid) {
  //if (s >= c) {
  //c *= 1.4;
  //clause_id* old_mem = mem;
  //mem = (clause_id*) realloc(mem, c*sizeof(clause_id));
  //}
  //mem[s++] = cid;
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
  //s--;

  SAT_ASSERT(!contains(*this, cid));
}
void clause_list_t::clear() {
  mem.clear();
  //s = 0;
}
size_t clause_list_t::size() const {
  //return s;
  return mem.size();
}
