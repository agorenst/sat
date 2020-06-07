#pragma once
#include <functional>
#include "cnf.h"
#include "trail.h"

template <typename... Args>
struct plugin {
  typedef std::function<void(Args... args)> listener;
  std::vector<listener> preconditions;
  std::vector<listener> listeners;
  std::vector<listener> postconditions;
  void add_listener(listener r) { listeners.push_back(r); }
  void precondition(listener r) { preconditions.push_back(r); }
  void postcondition(listener r) { postconditions.push_back(r); }

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
