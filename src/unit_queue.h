#pragma once
#include <vector>
#include "clause.h"
#include "variable.h"
// We use a real queue
struct unit_queue_t {
  literal_t prev_pushed;
  struct size_entry_t {
    literal_t l;
    clause_id c;
    int s;
  };
  struct simple_entry_t {
    literal_t l;
    clause_id c;
  };

  using entry_t = simple_entry_t;
  // using entry_t = size_entry_t;

  std::vector<entry_t> queue;
  using iter = std::vector<entry_t>::const_iterator;
  size_t b = 0;
  size_t e = 0;
  void push(entry_t a);
  entry_t pop();
  bool empty();
  void clear();

  size_t size();

  // using iterator = std::vector<entry_t>::iterator;

  iter begin();
  iter end();
  entry_t last();
  void erase_last();
};
