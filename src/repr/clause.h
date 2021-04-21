#pragma once
#include <algorithm>
#include <array>
#include <cassert>
#include <iterator>
#include <map>
#include <memory>
#include <numeric>

#include "variable.h"

template <typename C, typename V>
bool contains(const C &c, const V &v) {
  return std::find(std::begin(c), std::end(c), v) != std::end(c);
}

template <typename T, size_t B>
struct cache_storage {
  std::array<T, B> hot;
  std::unique_ptr<T[]> cold;
  int len;

  template <typename U>
  struct iterator_actual {
    typedef iterator_actual<U> iterator;
    typedef U value_type;
    typedef U &reference_type;
    typedef U &reference;
    typedef U *pointer;
    typedef int difference_type;
    typedef std::random_access_iterator_tag iterator_category;
    U *hot;
    U *cold;
    int i;
    iterator operator++() {
      i++;
      return *this;
    }
    iterator operator++(int) {
      auto tmp = *this;
      i++;
      return tmp;
    }
    iterator operator--() {
      i--;
      return *this;
    }
    iterator operator--(int) {
      auto tmp = *this;
      i--;
      return tmp;
    }
    U &operator*() {
      if (i < B) {
        return hot[i];
      }
      return cold[i - B];
    }
    pointer operator->() {
      if (i < B) {
        return &hot[i];
      }
      return &cold[i - B];
    }
    U &operator*() const {
      if (i < B) {
        return hot[i];
      }
      return cold[i - B];
    }
    pointer operator->() const {
      if (i < B) {
        return &hot[i];
      }
      return &cold[i - B];
    }
    bool operator==(const iterator &that) const {
      return this->hot == that.hot && this->i == that.i;
    }
    bool operator!=(const iterator &that) const { return !(*this == that); }
    bool operator<(const iterator &that) const { return i < that.i; }
    difference_type operator-(const iterator &that) const { return i - that.i; }
    difference_type operator+(const iterator &that) const { return i + that.i; }
    iterator operator+(int d) const { return {hot, cold, i + d}; }
    iterator operator-(int d) const { return {hot, cold, i - d}; }
    iterator &operator=(const iterator &that) {
      hot = that.hot;
      cold = that.cold;
      i = that.i;
      return *this;
    }
    iterator operator+=(int d) {
      i += d;
      return *this;
    }
  };

  using iterator = iterator_actual<T>;
  using const_iterator = iterator_actual<const T>;

  auto begin() const {
    const_iterator it{hot.data(), cold.get(), 0};
    return it;
  }
  auto end() const {
    const_iterator it{hot.data(), cold.get(), len};
    return it;
  }
  auto begin() {
    iterator it{hot.data(), cold.get(), 0};
    return it;
  }
  auto end() {
    iterator it{hot.data(), cold.get(), len};
    return it;
  }

  void clear() { len = 0; }

  template <typename C>
  void construct(const C &container) {
    this->len = container.size();
    size_t with_padding = this->len + 1;
    if (with_padding >= B) {
      cold = std::make_unique<T[]>((with_padding - B));
    }
    size_t i = 0;
    for (; i < container.size(); i++) {
      (*this)[i] = container[i];
    }
    (*this)[i] = 0;
  }

  cache_storage(const std::vector<T> &to_copy) { construct(to_copy); }

  // TODO: erase this...
  cache_storage(const cache_storage &that) { construct(that); }
  // cache_storage(cache_storage&& that): len(that.len) {
  // cold = std::move(that.cold);
  // hot = std::move(that.hot);
  //}
  cache_storage &operator=(const cache_storage &that) { construct(that); }

  auto size() const { return len; }
  auto empty() const { return len == 0; }
  auto &operator[](size_t i) {
    if (i < B) {
      return hot[i];
    } else {
      return cold[i - B];
    }
  }
  const auto &operator[](size_t i) const {
    if (i < B) {
      return hot[i];
    } else {
      return cold[i - B];
    }
  }
  void pop_back() { len--; }

  bool operator!=(const cache_storage<T, B> &that) const {
    return !std::equal(begin(), end(), that.begin(), that.end());
  }
  bool operator==(const cache_storage<T, B> &that) const {
    return std::equal(begin(), end(), that.begin(), that.end());
  }
};

struct clause_t {
  // cache_storage<literal_t, 16> mem;
  std::vector<literal_t> mem;
  clause_t(std::vector<literal_t> m) : mem(m) {}

  // no copy constructor
  clause_t(clause_t &&that) = default;
  clause_t &operator=(clause_t &&that) = default;

  clause_t *left = nullptr;
  clause_t *right = nullptr;
  bool is_alive = true;

  // clause_t() {}
  // void push_back(literal_t l) { mem.push_back(l); }

  // template<typename IT>
  // void erase(IT b, IT e) { mem.erase(b, e); }
  void clear() { mem.clear(); }
  auto begin() { return mem.begin(); }
  auto begin() const { return mem.begin(); }
  auto end() { return mem.end(); }
  auto end() const { return mem.end(); }
  auto size() const { return mem.size(); }
  auto empty() const { return mem.empty(); }
  auto &operator[](size_t i) { return mem[i]; }
  auto &operator[](size_t i) const { return mem[i]; }
  void pop_back() {
    mem.pop_back();
    // mem[mem.len] = 0;
  }
  bool operator==(const clause_t &that) const { return mem == that.mem; }
  bool operator!=(const clause_t &that) const { return mem != that.mem; }

  mutable int64_t sig = 0;
  mutable bool sig_computed = false;
  // For easier subsumption. This is its hash, really
  int64_t signature() const;
  bool possibly_subsumes(const clause_t &that) const;
};

inline bool operator<(const clause_t &a, const clause_t &b) {
  auto [e1, e2] =
      std::mismatch(std::begin(a), std::end(a), std::begin(b), std::end(b));
  return e1 == std::end(a) && e2 != std::end(b);
}

typedef clause_t *clause_id;

template <>
struct std::iterator_traits<
    cache_storage<literal_t, 16>::iterator_actual<literal_t>> {
  typedef literal_t value_type;
  typedef literal_t &reference_type;
  typedef literal_t &reference;
  typedef literal_t *pointer;
  typedef int difference_type;
  typedef std::random_access_iterator_tag iterator_category;
};

template <>
struct std::iterator_traits<
    cache_storage<literal_t, 16>::iterator_actual<const literal_t>> {
  typedef literal_t value_type;
  typedef const literal_t &reference_type;
  typedef const literal_t &reference;
  typedef const literal_t *pointer;
  typedef int difference_type;
  typedef std::random_access_iterator_tag iterator_category;
};

template <typename T>
using clause_map_t = std::map<clause_id, T>;
// template <typename T>
// struct clause_map_t {
//  typedef size_t key_t;
//  std::vector<T> mem;
//  clause_map_t(size_t s) : mem(s) {}
//  T &operator[](key_t k) {
//    if (k == mem.size()) {
//      mem.push_back(T{});
//    } else if (k > mem.size()) {
//      mem.resize(k + 1);
//    }
//    return mem[k];
//  }
//  const T &operator[](key_t k) const {
//    if (k == mem.size()) {
//      mem.push_back(T{});
//    } else if (k > mem.size()) {
//      mem.resize(k + 1);
//    }
//    return mem[k];
//  }
//  size_t size() const { return mem.size(); }
//};

// Really a "clause set", we don't promise anything about order.
// Using a vector is faster (?!) than just the array. Presumably I'm doing
// something silly.
struct clause_set_t {
  // clause_id* mem;
  // size_t c;
  // size_t s;
  std::vector<clause_id> mem;
  using iter_t = std::vector<clause_id>::iterator;
  using const_iter_t = std::vector<clause_id>::const_iterator;

  clause_set_t();
  clause_set_t(const clause_set_t &that);
  iter_t begin();
  iter_t end();

  const_iter_t begin() const;
  const_iter_t end() const;

  void push_back(clause_id cid);
  clause_id &operator[](const size_t i);
  clause_id &operator[](const int i);
  void remove(clause_id cid);
  void clear();
  size_t size() const;
  bool empty() const { return mem.empty(); }
  void pop_back() { mem.pop_back(); }
  clause_id back() const { return mem.back(); }
};

bool clauses_equal(const clause_t &a, const clause_t &b);
bool clause_taut(const clause_t &c);
void canon_clause(clause_t &c);
clause_t resolve_ref(const clause_t &c1, const clause_t &c2, literal_t l);
std::ostream &operator<<(std::ostream &o, const clause_t &c);