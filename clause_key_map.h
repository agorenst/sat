#pragma once

#include <vector>

template <typename T> struct clause_map_t {
  typedef size_t key_t;
  std::vector<T> mem;
  clause_map_t(size_t s) : mem(s) {}
  T &operator[](key_t k) {
    if (k == mem.size()) {
      mem.push_back(T{});
    } else if (k > mem.size()) {
      mem.resize(k + 1);
    }
    return mem[k];
  }
  const T &operator[](key_t k) const {
    if (k == mem.size()) {
      mem.push_back(T{});
    } else if (k > mem.size()) {
      mem.resize(k + 1);
    }
    return mem[k];
  }
  size_t size() const { return mem.size(); }
};
