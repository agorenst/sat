#pragma once
#include "action.h"
// We use a real queue
struct unit_queue_t {
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
  //using entry_t = size_entry_t;

  std::vector<entry_t> queue;
  size_t b = 0;
  size_t e = 0;
  void push(entry_t a);
  entry_t pop();
  bool empty();
  void clear();

  size_t size();

  using iterator = std::vector<entry_t>::iterator;

  iterator begin();
  iterator end();
};
