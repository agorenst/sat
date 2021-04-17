#pragma once
#include "trail.h"

struct vsids_t {
  vsids_t(const cnf_t &cnf, const trail_t &trail);
  literal_t choose() const;
  void clause_learned(const clause_t &c);
  void clear_activity();
  void static_activity();
  void bump_variable(variable_t v);

  const float alpha = 0.95f;
  const float bump = 1.0f;

  const float beta = 1.2;

  const cnf_t &cnf;
  // This is the activity. One day we'll have a better heap.
  // (Right now we're obliged to do a lot of linear scans, not great.)
  var_map_t<float> activity;
  // literal_map_t<bool> polarity;
  std::vector<variable_t> heap;
  variable_range vars;
  const trail_t &trail;
};

struct vsids_heap_t {
  struct heap_node {
    variable_t var;
    float activity;
  };
  vsids_heap_t(const cnf_t &cnf, const trail_t &trail);
  static bool heap_cmp(heap_node *a, heap_node *b);
  void static_activity();
  void clear_activity();
  void clause_learned(const clause_t &c);
  literal_t choose();
  const float alpha = 0.95f;
  const float bump = 1.0f;
  const trail_t &trail;
  const cnf_t &cnf;
  std::unique_ptr<heap_node[]> mem;
  size_t n = 0;
  std::vector<heap_node *> heap;
  std::vector<heap_node *>::iterator heap_end;
  var_map_t<heap_node *> activity;
  variable_range vars;
};