#include <iterator>
#include <memory>
#include "cnf.h"
typedef int32_t literal_t;

template<typename T, size_t B>
struct cache_storage {

    std::array<T, B> hot;
    std::unique_ptr<T[]> cold;
    int len;


    struct iterator {
        typedef iterator self_type;
        typedef T value_type;
        typedef T& reference_type;
        typedef T* pointer;
        typedef int difference_type;
        typedef std::random_access_iterator_tag iterator_category;
        T* hot;
        T* cold;
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
        T& operator*() {
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
        const T& operator*() const {
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
        bool operator==(const iterator& that) const {
            return this->hot == that.hot && this->i == that.i;
        }
        bool operator!=(const iterator& that) const {
            return !(*this == that);
        }
        bool operator<(const iterator& that) const {
            return i < that.i;
        }
        difference_type operator-(const iterator& that) const {
            return i-that.i;
        }
        difference_type operator+(const iterator& that) const {
            return i+that.i;
        }
        iterator operator+(int d) const {
            return {hot, cold, i+d};
        }
        iterator operator-(int d) const {
            return {hot, cold, i-d};
        }
        iterator& operator=(const iterator& that) {
            hot = that.hot;
            cold = that.cold;
            i = that.i;
            return *this;
        }
        iterator operator+=(int d) {
            i+=d;
            return *this;
        }
    };
    struct const_iterator {
        typedef const_iterator self_type;
        typedef T value_type;
        typedef T& reference_type;
        typedef T* pointer;
        typedef int difference_type;
        typedef std::random_access_iterator_tag iterator_category;
        const T* hot;
        const T* cold;
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
        const T& operator*() const {
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
        bool operator==(const const_iterator& that) const {
            return this->hot == that.hot && this->i == that.i;
        }
        bool operator!=(const const_iterator& that) const {
            return !(*this == that);
        }
        bool operator<(const const_iterator& that) const {
            return i < that.i;
        }
        difference_type operator-(const const_iterator& that) const {
            return i-that.i;
        }
        const_iterator operator+(int d) const {
            return {hot, cold, i+d};
        }
        const_iterator operator-(int d) const {
            return {hot, cold, i-d};
        }
        const_iterator operator+=(int d) {
            i+=d;
            return *this;
        }
        const_iterator& operator=(const const_iterator& that) {
            hot = that.hot;
            cold = that.cold;
            i = that.i;
            return *this;
        }
    };

    auto begin() const {
        const_iterator it {hot.data(), cold.get(), 0};
        return it;
    }
    auto end() const {
        const_iterator it {hot.data(), cold.get(), len};
        return it;
    }
    auto begin() {
        iterator it {hot.data(), cold.get(), 0};
        return it;
    }
    auto end() {
        iterator it {hot.data(), cold.get(), len};
        return it;
    }


    void clear() {
        len = 0;
    }
    
    cache_storage(const std::vector<T>& to_copy) {
        this->len = to_copy.size();
        if (this->len >= B) {
            cold = std::make_unique<T[]>(this->len - B);
        }
        for (size_t i = 0; i < to_copy.size(); i++) {
            (*this)[i] = to_copy[i];
        }
    }

    // TODO: erase this...
cache_storage(const cache_storage& that) {
        this->len = that.size();
        if (this->len >= B) {
            cold = std::make_unique<T[]>(this->len - B);
        }
        for (size_t i = 0; i < that.size(); i++) {
            (*this)[i] = that[i];
        }
}
    //cache_storage(cache_storage&& that): len(that.len) {
        //cold = std::move(that.cold);
        //hot = std::move(that.hot);
    //}
cache_storage& operator=(const cache_storage& that) {
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
    auto& operator[](size_t i) {
        if (i < B) {
            return hot[i];
        }
        else {
            return cold[i-B];
        }
    }
    const auto& operator[](size_t i) const {
        if (i < B) {
            return hot[i];
        }
        else {
            return cold[i-B];
        }
    }
    void pop_back() {
        len--;
    }

    bool operator!=(const cache_storage<T,B>& that) const {
        return !std::equal(begin(), end(), that.begin(), that.end());
    }
    bool operator==(const cache_storage<T,B>& that) const {
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

template<>
struct std::iterator_traits<cache_storage<literal_t,16>::iterator> {
    typedef literal_t value_type;
    typedef literal_t& reference_type;
    typedef literal_t* pointer;
    typedef int difference_type;
    typedef std::random_access_iterator_tag iterator_category;
};

template<>
struct std::iterator_traits<cache_storage<literal_t,16>::const_iterator> {
    typedef literal_t value_type;
    typedef literal_t& reference_type;
    typedef literal_t* pointer;
    typedef int difference_type;
    typedef std::random_access_iterator_tag iterator_category;
};