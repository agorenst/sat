#pragma once
#include <functional>
#include "cnf.h"

template<typename... Args>
struct plugin {
  typedef std::function<void(Args... args)> listener;
  std::vector<listener> preconditions;
  std::vector<listener> listeners;
  std::vector<listener> postconditions;
  void add_listener(listener r) {
    listeners.push_back(r);
  }
  void precondition(listener r) {
    preconditions.push_back(r);
  }
  void postcondition(listener r) {
    postconditions.push_back(r);
  }

  // Generally the parameter pack will be primitive data,
  // literals or references (which are just pointers).
  // Passing things with nontrivial constructors/destructors
  // here is probably a bad idea.
  void operator()(Args... args) {
    for (auto f : preconditions) {
      f(args...);
    }
    for (auto f : listeners) {
      f(args...);
    }
    for (auto f : postconditions) {
      f(args...);
    }
  }
};

static plugin<literal_t> apply_literal;
static plugin<clause_id> remove_clause;


// This is the moment we enter the conflict.
static plugin<const cnf_t&, trail_t&> on_conflict;
// this is before we commit the learned clause to the CNF.
static plugin<clause_t&, trail_t&> learned_clause;
