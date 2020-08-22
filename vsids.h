#pragma once
#include "literal_incidence_map.h"
#include "trail.h"

struct vsids_t {
  vsids_t(const cnf_t &cnf, const trail_t &trail);
  literal_t choose() const;
  void clause_learned(const clause_t &c);
  float score(literal_t l) const;

  // const float alpha = 0.95;
  const float alpha = 0.95;
  const float bump = 1.0;

  // This is the activity. One day we'll have a better heap.
  // (Right now we're obliged to do a lot of linear scans, not great.)
  var_map_t<float> activity;
  // literal_map_t<bool> polarity;
  variable_range vars;
  const trail_t &trail;
};
