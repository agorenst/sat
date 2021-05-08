#include "vsids.h"

#include <cassert>

vsids_t::vsids_t(const cnf_t &cnf, const trail_t &trail)
    : cnf(cnf), vars(cnf.var_range()), trail(trail) {
  activity.construct(max_variable(cnf));

  static_activity();
}

void vsids_t::static_activity() {
  clear_activity();
  for (auto cid : cnf) {
    for (auto l : cnf[cid]) {
      activity[var(l)] += bump;
    }
  }
}

void vsids_t::clear_activity() {
  std::fill(std::begin(activity), std::end(activity), 0.0f);
}

void vsids_t::bump_variable(variable_t v) { activity[v] += bump; }

void vsids_t::clause_learned(const clause_t &c) {
  for (literal_t l : c) {
    bump_variable(var(l));
  }
  std::for_each(std::begin(activity), std::end(activity),
                [this](float &s) { s *= alpha; });
}

literal_t vsids_t::choose() const {
  variable_t c = 0;
  float a = -1;
  for (variable_t v : vars) {
    if (trail.varset.get(v)) continue;
    if (activity[v] >= a) {
      a = activity[v];
      c = v;
    }
  }
  if (!c) return 0;
  return trail.previously_assigned_literal(c);
}
float vsids_t::score(literal_t l) const { return activity[var(l)]; }

vsids_heap_t::vsids_heap_t(const cnf_t &cnf, const trail_t &trail)
    : trail(trail), cnf(cnf), vars(cnf.var_range()) {
  mem = std::make_unique<heap_node[]>(vars.max_var);
  activity.construct(max_variable(cnf));
  for (variable_t v : vars) {
    activity[v] = &mem[v - 1];
    activity[v]->var = v;
    heap.push_back(&mem[v - 1]);
    n++;
  }
  heap_end = std::end(heap);
  static_activity();
}

bool vsids_heap_t::heap_cmp(heap_node *a, heap_node *b) {
  return a->activity < b->activity;
}

void vsids_heap_t::static_activity() {
  clear_activity();
  for (auto cid : cnf) {
    for (auto l : cnf[cid]) {
      activity[var(l)]->activity += bump;
    }
  }
  heap_end = std::end(heap);
  std::make_heap(std::begin(heap), heap_end, heap_cmp);
}

void vsids_heap_t::clear_activity() {
  std::for_each(mem.get(), mem.get() + n, [](heap_node &m) { m.activity = 0; });
}

void vsids_heap_t::clause_learned(const clause_t &c) {
  for (literal_t l : c) {
    activity[var(l)]->activity += bump;
  }

  // TODO: is there a way to only call this sometimes?
  // We can also limit the range to the "lowest" we've incremented, but finding
  // that is tricky.
  heap_end = std::end(heap);
  if (std::is_heap(std::begin(heap), heap_end, heap_cmp)) {
    // printf("skipping\n");
  } else {
    std::make_heap(std::begin(heap), heap_end, heap_cmp);
  }

  // don't ref the whole structure into the lambda... hopefully
  // helps with alias analysis (in compiler optimizations).
  float a = alpha;
  std::for_each(mem.get(), mem.get() + n,
                [a](heap_node &m) { m.activity *= a; });
}
literal_t vsids_heap_t::choose() {
  // assert(std::is_heap(std::begin(heap), heap_end, heap_cmp));
  while (heap_end != std::begin(heap) && trail.varset.get(heap[0]->var)) {
    std::pop_heap(std::begin(heap), heap_end, heap_cmp);
    heap_end--;
  }
  if (heap_end == std::begin(heap)) {
    return 0;
  }
  // assert(std::all_of(heap_end, std::end(heap),
  //[&](heap_node *n) { return trail.varset.get(n->var); }));
  variable_t c = heap[0]->var;
  std::pop_heap(std::begin(heap), heap_end, heap_cmp);
  heap_end--;
  return trail.previously_assigned_literal(c);
}