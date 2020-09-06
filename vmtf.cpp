#include "vmtf.h"
void vmtf_t::invariant() {
  return;
  if (next_to_choose == std::begin(q)) return;
  if (!std::none_of(std::reverse_iterator(std::next(next_to_choose)),
                    std::rend(q), [&](variable_t v) {
                      return trail.literal_unassigned(lit(v)) &&
                             trail.level(lit(v));
                    })) {
    debug();
    assert(0);
  }
}

void vmtf_t::debug() {
  std::vector<int> spaces;
  for (variable_t v : q) {
    bool is_assigned = !trail.literal_unassigned(lit(v));
    bool is_true = false;
    if (is_assigned) {
      is_true = trail.literal_true(lit(v));
    }
    char c = is_assigned ? (is_true ? '1' : '0') : '_';
    int total = fprintf(stderr, "%d#%ld (%c) ", v, timestamp[v], c);
    spaces.push_back(total);
  }
  std::cerr << std::endl;
  int i = 0;
  for (auto it = std::begin(q); it != next_to_choose; it++) {
    int j = 0;
    while (j++ < spaces[i]) {
      std::cerr << " ";
    }
    i++;
  }
  std::cerr << *next_to_choose << std::endl;
  std::cerr << "CONFLICT INDEX: " << conflict_index << std::endl;
  std::cerr << trail << std::endl;
}

vmtf_t::vmtf_t(const cnf_t &cnf, const trail_t &trail)
    : next_to_choose(q.begin()), cnf(cnf), trail(trail) {
  variable_t max_var = max_variable(cnf);

  pos.construct(max_var);
  for (variable_t v : variable_range(max_var)) {
    q.push_front(v);
    pos[v] = q.begin();
  }
  next_to_choose = q.begin();

  timestamp.construct(max_var);
  std::fill(std::begin(timestamp), std::end(timestamp), 0);
}

void vmtf_t::clause_learned(const clause_t &c) {
  for (literal_t l : c) {
    bump(var(l));
  }
  conflict_index++;
}

void vmtf_t::bump(variable_t v) {
  auto it = pos[v];
  bool rep = next_to_choose == it;

  // TODO: splice?
  q.erase(it);  // this should not invalidate any other iterators.
  q.push_front(v);
  pos[v] = q.begin();

  if (rep) next_to_choose = q.begin();

  timestamp[v] = conflict_index;  // or is it +=?
  assert(*std::begin(q) == v);
  invariant();
}

variable_t vmtf_t::choose() {
  invariant();
  auto it = std::find_if(std::begin(q), std::end(q), [&](variable_t v) {
    return trail.literal_unassigned(lit(v));
  });
  if (it == std::end(q)) return 0;
  return trail.previously_assigned_literal(*it);
}

void vmtf_t::unassign(variable_t v) {
  auto s = timestamp[v];
  auto ns = timestamp[*next_to_choose];
  if (s > ns) {
    // this better actually be moving "right".
    assert(std::find(std::reverse_iterator(next_to_choose), std::rend(q), v) !=
           std::rend(q));
    next_to_choose = pos[v];
  }
  invariant();
}
