#include "variable.h"
#include <cmath>
#include <numeric>
#include "debug.h"

variable_t var(literal_t l) { return l >> 1; }
literal_t lit(variable_t l) { return l << 1; }
literal_t neg(literal_t l) { return l ^ 1; }
bool ispos(literal_t l) { return neg(l) & 1; }

literal_t dimacs_to_lit(int x) {
  bool is_neg = x < 0;
  literal_t l = std::abs(x);
  l <<= 1;
  if (is_neg) l++;
  MAX_ASSERT(l > 1);
  return l;
}
int lit_to_dimacs(literal_t l) {
  MAX_ASSERT(l != 0);
  bool is_neg = (l % 2) != 0;
  int x = l >> 1;
  if (is_neg) x = -x;
  return x;
}

variable_range::variable_range(const variable_t max_var) : max_var(max_var) {}

variable_range::iterator &variable_range::iterator::operator++() {
  v++;
  return *this;
}
variable_t variable_range::iterator::operator*() { return v; }
bool variable_range::iterator::operator==(
    const variable_range::iterator &that) const {
  return this->v == that.v;
}
bool variable_range::iterator::operator!=(
    const variable_range::iterator &that) const {
  return this->v != that.v;
}
variable_range::iterator variable_range::begin() const {
  return iterator{max_var + 1, 1};
}
variable_range::iterator variable_range::end() const {
  return iterator{max_var + 1, max_var + 1};
}