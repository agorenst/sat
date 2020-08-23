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

  cache_storage(const std::vector<T> &to_copy) {
    this->len = to_copy.size();
    if (this->len >= B) {
      cold = std::make_unique<T[]>(this->len - B);
    }
    for (size_t i = 0; i < to_copy.size(); i++) {
      (*this)[i] = to_copy[i];
    }
  }

  // TODO: erase this...
  cache_storage(const cache_storage &that) {
    this->len = that.size();
    if (this->len >= B) {
      cold = std::make_unique<T[]>(this->len - B);
    }
    for (size_t i = 0; i < that.size(); i++) {
      (*this)[i] = that[i];
    }
  }
  // cache_storage(cache_storage&& that): len(that.len) {
  // cold = std::move(that.cold);
  // hot = std::move(that.hot);
  //}
  cache_storage &operator=(const cache_storage &that) {
    this->len = that.size();
    if (this->len >= B) {
      cold = std::make_unique<T[]>(this->len - B);
    }
    for (size_t i = 0; i < that.size(); i++) {
      (*this)[i] = that[i];
    }
    return *this;
  }

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

#if 0
struct clause_t {
  cache_storage<literal_t, 16> mem;
  clause_t(std::vector<literal_t> m) : mem(m) {}

  // no copy constructor
  clause_t(const clause_t& that): mem(that.mem) {}
  clause_t(clause_t&& that): mem(std::move(that.mem)) {}
  clause_t& operator=(clause_t&& that) {
    mem = std::move(that.mem);
    return *this;
  }

  //clause_t() {}
  //void push_back(literal_t l) { mem.push_back(l); }


  //template<typename IT>
  //void erase(IT b, IT e) { mem.erase(b, e); }
  void clear() { mem.clear(); }
  auto begin() { return mem.begin(); }
  auto begin() const { return mem.begin(); }
  auto end() { return mem.end(); }
  auto end() const { return mem.end(); }
  auto size() const { return mem.size(); }
  auto empty() const { return mem.empty(); }
  auto& operator[](size_t i) { return mem[i]; }
  auto& operator[](size_t i) const { return mem[i]; }
  void pop_back() { mem.pop_back(); }
  bool operator==(const clause_t& that) const { return mem == that.mem; }
  bool operator!=(const clause_t& that) const { return mem != that.mem; }

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

  bool possibly_subsumes(const clause_t& that) const {
    return (this->signature() & that.signature()) == this->signature();
  }
};
#else
struct clause_t {
  std::unique_ptr<char[]> mem = nullptr;
  size_t *len = nullptr;
  literal_t *lits = nullptr;
  clause_t **left = nullptr;
  clause_t **right = nullptr;
  size_t *sig = nullptr;
  bool *sig_computed = nullptr;
  // literal_t* zero = nullptr;
  clause_t(std::vector<literal_t> m) {
    size_t lits_size = (m.size() + 1) * sizeof(literal_t);
    size_t s = lits_size;
    s += sizeof(*len);          // length
    s += sizeof(*left);         // doubly-linked list
    s += sizeof(*right);        // doubly-linked list
    s += sizeof(*sig);          // signature
    s += sizeof(*sig_computed); // length

    // Get the raw bytes
    mem = std::make_unique<char[]>(s);
    size_t offset = 0;

    len = (size_t *)&mem[offset];
    offset += sizeof(*len);

    lits = (literal_t *)&mem[offset];
    offset += lits_size;

    left = (clause_t **)&mem[offset];
    offset += sizeof(*left);

    right = (clause_t **)&mem[offset];
    offset += sizeof(*right);

    sig = (size_t *)&mem[offset];
    offset += sizeof(*sig);

    sig_computed = (bool *)&mem[offset];
    assert(offset + sizeof(*sig_computed) == s);

    int i = 0;
    for (literal_t l : m) {
      lits[i++] = l;
    }
    lits[i] = 0;
    *len = m.size();
    *sig = 0;
    *sig_computed = false;
    *left = nullptr;
    *right = nullptr;
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
  clause_t() {}
  clause_t(clause_t &&that)
      : mem(std::move(that.mem)), len(that.len), lits(that.lits),
        left(that.left), right(that.right), sig(that.sig),
        sig_computed(that.sig_computed) {}

  clause_t &operator=(clause_t &&that) {
    mem = std::move(that.mem);
    len = that.len;
    sig = that.sig;
    sig_computed = that.sig_computed;
    lits = that.lits;
    left = that.left;
    right = that.right;
    return *this;
  }

  // clause_t() {}
  // void push_back(literal_t l) { mem.push_back(l); }

  // template<typename IT>
  // void erase(IT b, IT e) { mem.erase(b, e); }
  // void clear() { mem.clear(); }
  auto begin() { return lits; }
  auto begin() const { return lits; }
  auto end() { return &lits[*len]; }
  auto end() const { return &lits[*len]; }
  auto size() const { return std::distance(begin(), end()); }
  auto empty() const { return *len == 0; }
  auto &operator[](size_t i) { return lits[i]; }
  auto &operator[](size_t i) const { return lits[i]; }
  void pop_back() {
    (*len)--;
    lits[*len] = 0; // update the end marker
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
    if (*sig_computed) {
      return *sig;
    }
    *sig_computed = true;
    auto h = [](literal_t l) { return std::hash<literal_t>{}(l); };
    auto addhash = [&h](literal_t a, literal_t b) { return h(a) | h(b); };
    *sig = std::accumulate(begin(), end(), 0, addhash);
    return *sig;
  }

  bool possibly_subsumes(const clause_t &that) const {
    return (this->signature() & that.signature()) == this->signature();
  }
};
#endif

typedef clause_t *clause_id;