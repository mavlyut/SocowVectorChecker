#pragma once
#include <cstddef>
#include <algorithm>

template <typename T, size_t SMALL_SIZE>
struct socow_vector {
  using iterator = T*;
  using const_iterator = T const*;
  static const size_t size_of_T = sizeof(T);

  socow_vector() : _size(0), is_small(true) {}

  socow_vector(socow_vector const& other) : _size(other._size), is_small(other.is_small) {
    if (other.is_small) {
      copy_from_begin(other.small_storage, small_storage, other._size);
    } else {
      new(&big_storage) storage(other.big_storage);
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
    if (big_storage.ctrl->is_unique()) {
      remove(my_begin(), my_end());
    }
    big_storage.~storage();
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
    return _size;
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
    if (_size == capacity()) {
      storage tmp(capacity() * 2);
      copy_from_begin(begin(), tmp.ctrl->_data, _size);
      new(tmp.ctrl->_data + _size) T(element);
      if (is_small || big_storage.ctrl->is_unique()) remove(my_begin(), my_end());
      if (!is_small) big_storage.~storage();
      new(&big_storage) storage(tmp);
      is_small = false;
    } else {
      new(begin() + _size) T(element);
    }
    _size++;
  }

  void pop_back() {
    (end() - 1)->~T();
    _size--;
  }

  bool empty() const {
    return _size == 0;
  }

  size_t capacity() const {
    return is_small ? SMALL_SIZE : big_storage.ctrl->_capacity;
  }

  void reserve(size_t new_capacity) {
    if ((is_small && new_capacity > SMALL_SIZE) || !is_small) {
      expand_storage(my_begin(), std::max<size_t>(new_capacity, capacity()));
    }
  }

  void shrink_to_fit() {
    if (is_small) return;
    if (_size <= SMALL_SIZE) {
      storage tmp = big_storage;
      big_storage.~storage();
      try {
        copy_from_begin(tmp.ctrl->_data, small_storage, _size);
      } catch (...) {
        new(&big_storage) storage(tmp);
        throw;
      }
      remove(tmp.ctrl->_data, tmp.ctrl->_data + _size);
      is_small = true;
    } else if (_size != capacity()) {
      expand_storage(big_storage.ctrl->_data, _size);
    }
  }

  void clear() {
    erase(begin(), end());
  }

  void swap(socow_vector& other) {
    if (_size > other._size || (!is_small && other.is_small)) {
      other.swap(*this);
      return;
    }
    if (is_small && other.is_small) {
      for (size_t i = 0; i < _size; ++i) {
        std::swap(small_storage[i], other.small_storage[i]);
      }
      copy_in_range(other.small_storage, small_storage, _size, other._size);
      remove(other.my_begin() + _size, other.my_end());
    } else if (!is_small && !other.is_small) {
      std::swap(big_storage, other.big_storage);
    } else {
      storage tmp = other.big_storage;
      other.big_storage.~storage();
      try {
        copy_from_begin(small_storage, other.small_storage, _size);
      } catch (...) {
        new(&other.big_storage) storage(tmp);
        throw;
      }
      remove(my_begin(), my_end());
      new(&big_storage) storage(tmp);
    }
    std::swap(_size, other._size);
    std::swap(is_small, other.is_small);
  }

  iterator begin() {
    if (is_small) return small_storage;
    make_copy();
    return big_storage.ctrl->_data;
  }

  iterator end() {
    return begin() + _size;
  }

  const_iterator begin() const {
    return is_small ? small_storage : big_storage.ctrl->_data;
  }

  const_iterator end() const {
    return begin() + _size;
  }

  iterator insert(const_iterator pos, T const& t) {
    ptrdiff_t diff = pos - my_begin();
    push_back(t);
    for (size_t i = _size - 1; i > diff; --i) {
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
    for (size_t i = start; i < _size - count; i++) {
      std::swap(operator[](i), operator[](i + count));
    }
    for (size_t i = 0; i < count; ++i) {
      pop_back();
    }
    return begin() + start;
  }

private:
  iterator my_begin() {
    return is_small ? small_storage : big_storage.ctrl->_data;
  }

  iterator my_end() {
    return my_begin() + _size;
  }

  void make_copy() {
    if (!is_small && !big_storage.ctrl->is_unique()) {
      expand_storage(big_storage.ctrl->_data, capacity());
    }
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

  void expand_storage(T const* from, size_t new_capacity) {
    if (new_capacity == 0) {
      _size = 0;
      return;
    }
    storage tmp(new_capacity);
    copy_from_begin(from, tmp.ctrl->_data, _size);
    if (is_small || big_storage.ctrl->is_unique()) {
      remove(my_begin(), my_end());
    }
    if (!is_small) {
      big_storage.~storage();
    }
    new(&big_storage) storage(tmp);
    is_small = false;
  }

  struct control_block {
    size_t _counter;
    size_t _capacity;
    T _data[0];

    bool is_unique() {
      return _counter == 1;
    }
  };

  static control_block* get_size(size_t capacity) {
    return static_cast<control_block*>(operator new(size_of_ctrl_block + size_of_T * capacity, _align));
  }

  struct storage {
    storage() = default;

    explicit storage(size_t capacity) : ctrl(get_size(capacity)) {
      ctrl->_counter = 1;
      ctrl->_capacity = capacity;
    }

    storage(storage const& other) : ctrl(other.ctrl) {
      ctrl->_counter++;
    }

    ~storage() {
      if (ctrl->_counter == 1) {
        operator delete(ctrl, static_cast<std::align_val_t>(alignof(ctrl)));
      } else {
        ctrl->_counter--;
      }
    }

    control_block* ctrl;
  };

  bool is_small;
  size_t _size;
  static const std::align_val_t _align = static_cast<std::align_val_t>(alignof(control_block));
  static const size_t size_of_ctrl_block = sizeof(control_block);
  union {
    T small_storage[SMALL_SIZE];
    storage big_storage;
  };
};

