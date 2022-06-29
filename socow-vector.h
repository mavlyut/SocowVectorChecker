#pragma once
#include <cstddef>
#include <algorithm>

template <typename T, size_t SMALL_SIZE>
struct socow_vector {
  using iterator = T*;
  using const_iterator = T const*;

  socow_vector() : size_(0), is_small(true) {}

  socow_vector(socow_vector const& other) : size_(other.size_), is_small(other.is_small) {
    if (other.is_small) {
      copy_from_begin(other.small_storage, small_storage, other.size_);
    } else {
      big_storage = other.big_storage;
      big_storage->counter_++;
    }
  }

  socow_vector& operator=(socow_vector const& other) {
    if (&other != this) {
      socow_vector(other).swap(*this);
    }
    return *this;
  }

  ~socow_vector() {
    if (is_small) {
      remove(my_begin(), my_end());
      return;
    }
    if (big_storage->dec()) {
      remove(my_begin(), my_end());
      operator delete(big_storage);
    }
  }

  T& operator[](size_t i) {
    return *(begin() + i);
  }

  T const& operator[](size_t i) const {
    return *(begin() + i);
  }

  T* data() {
    return begin();
  }

  T const* data() const {
    return begin();
  }

  size_t size() const {
    return size_;
  }

  T& front() {
    return *begin();
  }

  T const& front() const {
    return *begin();
  }

  T& back() {
    return *(end() - 1);
  }

  T const& back() const {
    return *(end() - 1);
  }

  void push_back(T const& element) {
    if (size_ == capacity()) {
      storage* tmp = copy_storage_with_fixed_capacity(capacity() * 2);
      try {
        new(tmp->data_ + size_) T(element);
      } catch (...) {
        remove(tmp->data_, tmp->data_ + size_);
        operator delete(tmp);
        throw;
      }
      this->~socow_vector();
      big_storage = tmp;
      is_small = false;
    } else {
      new(begin() + size_) T(element);
    }
    size_++;
  }

  void pop_back() {
    (end() - 1)->~T();
    size_--;
  }

  bool empty() const {
    return size_ == 0;
  }

  size_t capacity() const {
    return is_small ? SMALL_SIZE : big_storage->capacity_;
  }

  void reserve(size_t new_capacity) {
    if ((!is_small && big_storage->is_not_unique()) || new_capacity > capacity()) {
      expand_storage(std::max<size_t>(new_capacity, capacity()));
    }
  }

  void shrink_to_fit() {
    if (is_small) return;
    if (size_ <= SMALL_SIZE) {
      storage* tmp = big_storage;
      big_storage = nullptr;
      try {
        copy_from_begin(tmp->data_, small_storage, size_);
      } catch (...) {
        big_storage = tmp;
        throw;
      }
      if (tmp->dec()) {
        remove(tmp->data_, tmp->data_ + size_);
        operator delete(tmp);
      }
      is_small = true;
    } else if (size_ != capacity()) {
      expand_storage(size_);
    }
  }

  void clear() {
    erase(begin(), end());
  }

  void swap(socow_vector& other) {
    if (size_ > other.size_ || (!is_small && other.is_small)) {
      other.swap(*this);
      return;
    }
    if (is_small && other.is_small) {
      for (size_t i = 0; i < size_; ++i) {
        std::swap(small_storage[i], other.small_storage[i]);
      }
      copy_in_range(other.small_storage, small_storage, size_, other.size_);
      remove(other.my_begin() + size_, other.my_end());
    } else if (!is_small && !other.is_small) {
      std::swap(big_storage, other.big_storage);
    } else {
      storage* tmp = other.big_storage;
      other.big_storage = nullptr;
      try {
        copy_from_begin(small_storage, other.small_storage, size_);
      } catch (...) {
        other.big_storage = tmp;
        throw;
      }
      remove(my_begin(), my_end());
      big_storage = tmp;
    }
    std::swap(size_, other.size_);
    std::swap(is_small, other.is_small);
  }

  iterator begin() {
    if (is_small) return small_storage;
    if (!is_small && big_storage->is_not_unique()) {
      expand_storage(capacity());
    }
    return big_storage->data_;
  }

  iterator end() {
    return begin() + size_;
  }

  const_iterator begin() const {
    return is_small ? small_storage : big_storage->data_;
  }

  const_iterator end() const {
    return begin() + size_;
  }

  iterator insert(const_iterator pos, T const& t) {
    ptrdiff_t diff = pos - my_begin();
    push_back(t);
    for (size_t i = size_ - 1; i > diff; --i) {
      std::swap(*(my_begin() + i), *(my_begin() + i - 1));
    }
    return my_begin() + diff;
  }

  iterator erase(const_iterator pos) {
    return erase(pos, pos + 1);
  }

  iterator erase(const_iterator first, const_iterator last) {
    ptrdiff_t count = last - first;
    ptrdiff_t start = first - my_begin();
    for (size_t i = start; i < size_ - count; i++) {
      std::swap(operator[](i), operator[](i + count));
    }
    for (size_t i = 0; i < count; ++i) {
      pop_back();
    }
    return begin() + start;
  }

private:
  iterator my_begin() {
    return is_small ? small_storage : big_storage->data_;
  }

  iterator my_end() {
    return my_begin() + size_;
  }

  void copy_in_range(T const* from, T* to, size_t start, size_t end) {
    size_t i = start;
    try {
      while (i < end) {
        new(to + i) T(from[i]);
        ++i;
      }
    } catch (...) {
      remove(to, to + i);
      throw;
    }
  }

  void copy_from_begin(T const* from, T* to, size_t count) {
    copy_in_range(from, to, 0, count);
  }

  void remove(T* start, T* end) {
    if (start != nullptr) {
      ptrdiff_t count = end - start;
      for (ptrdiff_t i = count - 1; i >= 0; --i) {
        (start + i)->~T();
      }
    }
  }

  void expand_storage(size_t new_capacity) {
    storage* tmp = copy_storage_with_fixed_capacity(new_capacity);
    this->~socow_vector();
    big_storage = tmp;
    is_small = false;
  }

  struct storage {
    size_t counter_;
    size_t capacity_;
    T data_[0];
    
    explicit storage(size_t n) : counter_(1), capacity_(n) {}

    bool dec() {
      counter_--;
      return counter_ == 0;
    }

    bool is_not_unique() const {
      return counter_ > 1;
    }
  };

  static storage* make_new_storage_with_fixed_capacity(size_t new_capacity) {
    storage* ans =
      new (static_cast<storage*>(operator new(sizeof(storage) + new_capacity * sizeof(T))))
      storage(new_capacity);
    return ans;
  }

  storage* copy_storage_with_fixed_capacity(size_t new_capacity) {
    storage* ans = make_new_storage_with_fixed_capacity(new_capacity);
    try {
      copy_from_begin(my_begin(), ans->data_, size_);
    } catch (...) {
      operator delete(ans);
      throw;
    }
    return ans;
  }

  bool is_small;
  size_t size_;
  union {
    T small_storage[SMALL_SIZE];
    storage* big_storage;
  };
};

