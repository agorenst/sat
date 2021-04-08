#include "clause.h"
#include <iterator>
#include "settings.h"

// Really a "clause set", we don't promise anything about order.

clause_set_t::clause_set_t() {
  // c = 16;
  // s = 0;
  // mem = (clause_id*) malloc(c * sizeof(clause_id));
}
clause_set_t::clause_set_t(const clause_set_t &that) {
  // c = 16;
  // s = 0;
  // mem = (clause_id*) malloc(c * sizeof(clause_id));
  for (clause_id cid : that) {
    push_back(cid);
  }
}

clause_set_t::iter_t clause_set_t::begin() { return mem.begin(); }
clause_set_t::iter_t clause_set_t::end() { return mem.end(); }

clause_set_t::const_iter_t clause_set_t::begin() const { return mem.begin(); }
clause_set_t::const_iter_t clause_set_t::end() const { return mem.end(); }

void clause_set_t::push_back(clause_id cid) {
  // if (s >= c) {
  // c *= 1.4;
  // clause_id* old_mem = mem;
  // mem = (clause_id*) realloc(mem, c*sizeof(clause_id));
  //}
  // mem[s++] = cid;
  mem.push_back(cid);
}
clause_id &clause_set_t::operator[](const size_t i) { return mem[i]; }
clause_id &clause_set_t::operator[](const int i) { return mem[i]; }
void clause_set_t::remove(clause_id cid) {
  SAT_ASSERT(contains(*this, cid));
  auto e = end();

  auto r = std::find(begin(), e, cid);
  SAT_ASSERT(r >= begin());
  SAT_ASSERT(r < e);
  std::iter_swap((e - 1), r);

  mem.pop_back();
  // s--;

  SAT_ASSERT(!contains(*this, cid));
}
void clause_set_t::clear() {
  mem.clear();
  // s = 0;
}

size_t clause_set_t::size() const {
  // return s;
  return mem.size();
}

int64_t clause_t::signature() const {
  if (sig_computed) {
    return sig;
  }
  sig_computed = true;
  auto h = [](literal_t l) { return std::hash<literal_t>{}(l); };
  auto addhash = [&h](literal_t a, literal_t b) { return h(a) | h(b); };
  sig = std::accumulate(begin(), end(), 0, addhash);
  return sig;
}

bool clause_t::possibly_subsumes(const clause_t &that) const {
  return (this->signature() & that.signature()) == this->signature();
}

bool clause_taut(const clause_t &c) {
  for (auto l : c) {
    for (auto r : c) {
      if (l == neg(r)) {
        return true;
      }
    }
  }
  return false;
}

void canon_clause(clause_t &c) {
  std::sort(std::begin(c), std::end(c));

  auto new_end = std::unique(std::begin(c), std::end(c));
  // Very primitive form of "erase"
  auto new_len = std::distance(std::begin(c), new_end);
  while (std::distance(std::begin(c), std::end(c)) != new_len) {
    c.pop_back();
  }
}

bool clauses_equal(const clause_t &a, const clause_t &b) {
  if (a.size() != b.size()) return false;
  std::vector<literal_t> al;
  for (auto l : a) {
    al.push_back(l);
  }
  std::vector<literal_t> bl;
  for (auto l : b) {
    bl.push_back(l);
  }
  std::sort(std::begin(al), std::end(al));
  std::sort(std::begin(bl), std::end(bl));
  return al == bl;
}

clause_t resolve_ref(const clause_t &c1, const clause_t &c2, literal_t l) {
  SAT_ASSERT(contains(c1, l));
  SAT_ASSERT(contains(c2, neg(l)));
  std::vector<literal_t> c3tmp;
  // clause_t c3tmp;
  for (literal_t x : c1) {
    if (var(x) != var(l)) {
      c3tmp.push_back(x);
    }
  }
  for (literal_t x : c2) {
    if (var(x) != var(l)) {
      c3tmp.push_back(x);
    }
  }
  std::sort(std::begin(c3tmp), std::end(c3tmp));
  auto new_end = std::unique(std::begin(c3tmp), std::end(c3tmp));
  c3tmp.erase(new_end, std::end(c3tmp));
  return c3tmp;
}

std::ostream &operator<<(std::ostream &o, const clause_t &c) {
  for (auto l : c) {
    if (settings::output_dimacs)
      o << lit_to_dimacs(l) << " ";
    else
      o << l << " ";
  }
  return o;
}