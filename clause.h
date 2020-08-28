#pragma once
#include <algorithm>
#include <cassert>
#include <iterator>
#include <memory>
#include <numeric>
typedef int32_t literal_t;

template <typename T, size_t B> struct cache_storage {

  std::array<T, B> hot;
  std::unique_ptr<T[]> cold;
  int len;

  struct iterator {
    typedef iterator self_type;
    typedef T value_type;
    typedef T &reference_type;
    typedef T *pointer;
    typedef int difference_type;
    typedef std::random_access_iterator_tag iterator_category;
    T *hot;
    T *cold;
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
    T &operator*() {
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
    const T &operator*() const {
      if (i < B) {
        return hot[i];
      }
      return cold[i - B];
    }
    const pointer operator->() const {
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
  struct const_iterator {
    typedef const_iterator self_type;
    typedef T value_type;
    typedef T &reference_type;
    typedef T *pointer;
    typedef int difference_type;
    typedef std::random_access_iterator_tag iterator_category;
    const T *hot;
    const T *cold;
    int i;
    const_iterator operator++() {
      i++;
      return *this;
    }
    const_iterator operator++(int) {
      auto tmp = *this;
      i++;
      return tmp;
    }
    const_iterator operator--() {
      i--;
      return *this;
    }
    const_iterator operator--(int) {
      auto tmp = *this;
      i--;
      return tmp;
    }
    const T &operator*() const {
      if (i < B) {
        return hot[i];
      }
      return cold[i - B];
    }
    const pointer operator->() const {
      if (i < B) {
        return &hot[i];
      }
      return &cold[i - B];
    }
    bool operator==(const const_iterator &that) const {
      return this->hot == that.hot && this->i == that.i;
    }
    bool operator!=(const const_iterator &that) const {
      return !(*this == that);
    }
    bool operator<(const const_iterator &that) const { return i < that.i; }
    difference_type operator-(const const_iterator &that) const {
      return i - that.i;
    }
    const_iterator operator+(int d) const { return {hot, cold, i + d}; }
    const_iterator operator-(int d) const { return {hot, cold, i - d}; }
    const_iterator operator+=(int d) {
      i += d;
      return *this;
    }
    const_iterator &operator=(const const_iterator &that) {
      hot = that.hot;
      cold = that.cold;
      i = that.i;
      return *this;
    }
  };

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

  template <typename C> void construct(const C &container) {
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
#if 0
template<typename T, size_t B>
        bool operator==(const cache_storage<typename T,B>::iterator& a, const cache_storage<typename T,B>::iterator& b) {
            return a.hot == b.hot && a.i == b.i;
        }
        bool operator!=(const iterator& a, const iterator& b) {
            return !(a == b);
        }
#endif

template <>
struct std::iterator_traits<cache_storage<literal_t, 16>::iterator> {
  typedef literal_t value_type;
  typedef literal_t &reference_type;
  typedef literal_t *pointer;
  typedef int difference_type;
  typedef std::random_access_iterator_tag iterator_category;
};

template <>
struct std::iterator_traits<cache_storage<literal_t, 16>::const_iterator> {
  typedef literal_t value_type;
  typedef literal_t &reference_type;
  typedef literal_t *pointer;
  typedef int difference_type;
  typedef std::random_access_iterator_tag iterator_category;
};

#if 1
struct clause_t {
  cache_storage<literal_t, 16> mem;
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
  void pop_back() { mem.pop_back(); }
  bool operator==(const clause_t &that) const { return mem == that.mem; }
  bool operator!=(const clause_t &that) const { return mem != that.mem; }

  mutable size_t sig;
  mutable bool sig_computed = false;
  // For easier subsumption. This is its hash, really
  size_t signature() const {
    if (sig_computed) {
      return sig;
    }
    sig_computed = true;
    auto h = [](literal_t l) { return std::hash<literal_t>{}(l); };
    auto addhash = [&h](literal_t a, literal_t b) { return h(a) | h(b); };
    sig = std::accumulate(begin(), end(), 0, addhash);
    return sig;
  }

  bool possibly_subsumes(const clause_t &that) const {
    return (this->signature() & that.signature()) == this->signature();
  }
};
#else

struct clause_t {

  clause_t *left = nullptr;
  clause_t *right = nullptr;
  size_t len = 0;
  mutable size_t sig = 0;
  mutable bool sig_computed = false;
  bool is_alive = true;
  literal_t *literals;

  void zero_headers() {
    left = nullptr;
    right = nullptr;
    len = 0;
    sig = 0;
    sig_computed = false;
    is_alive = true;
  }

  clause_t(std::vector<literal_t> m) {
    zero_headers();

    literals = new literal_t[m.size() + 1];

    size_t i = 0;
    for (literal_t l : m) {
      literals[i++] = l;
    }
    literals[i] = 0;

    len = m.size();
  }

// no copy constructor
#if 0
  clause_t(const clause_t& that):
  len(that.len),
   mem(std::make_unique<literal_t[]>(that.len)) {
       for (int i = 0; i < len; i++) {
          mem[i] = that.mem[i];
      }
   }
#endif
  clause_t(clause_t &&that) = default;

  clause_t &operator=(clause_t &&that) = default;

  // clause_t() {}
  // void push_back(literal_t l) { mem.push_back(l); }

  // template<typename IT>
  // void erase(IT b, IT e) { mem.erase(b, e); }
  // void clear() { mem.clear(); }
  auto begin() { return literals; }
  auto begin() const { return literals; }
  auto end() { return &literals[len]; }
  auto end() const { return &literals[len]; }
  auto size() const { return std::distance(begin(), end()); }
  auto empty() const { return len == 0; }
  auto &operator[](size_t i) { return literals[i]; }
  auto &operator[](size_t i) const { return literals[i]; }
  void pop_back() {
    len--;
    literals[len] = 0; // update the end marker
  }
  bool operator==(const clause_t &that) const {
    return std::equal(begin(), end(), that.begin(), that.end());
  }
  bool operator!=(const clause_t &that) const {
    return !std::equal(begin(), end(), that.begin(), that.end());
  }

  // this should be embedded in the array as well...
  // For easier subsumption. This is its hash, really
  size_t signature() const {
    if (sig_computed) {
      return sig;
    }
    sig_computed = true;
    auto h = [](literal_t l) { return std::hash<literal_t>{}(l); };
    auto addhash = [&h](literal_t a, literal_t b) { return h(a) | h(b); };
    sig = std::accumulate(begin(), end(), 0, addhash);
    return sig;
  }

  bool possibly_subsumes(const clause_t &that) const {
    return (this->signature() & that.signature()) == this->signature();
  }
};
#endif

typedef clause_t *clause_id;