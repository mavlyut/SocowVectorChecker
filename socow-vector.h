#pragma once
#include <cstddef>

template <typename T, size_t SMALL_SIZE>
struct socow_vector {
    using iterator = T*;
    using const_iterator = T const*;

    socow_vector() : _size(0), is_small(true) {}

    socow_vector(socow_vector const& other) : _size(other._size), is_small(other.is_small) {
        if (other.is_small) {
            copy(const_cast<T*>(other.small_storage), small_storage, other._size);
        } else {
            new(&big_storage) storage(other.big_storage);
        }
    }

    socow_vector& operator=(socow_vector const& other) {
        if (&other != this) {
            socow_vector tmp(other);
            tmp.swap(*this);
        }
        return *this;
    }

    ~socow_vector() {
        if (is_small) {
            remove(my_begin(), my_end());
        } else {
            if (big_storage.is_unique()) {
                remove(my_begin(), my_end());
            }
            big_storage.~storage();
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
            T tmp = element;
            expand_storage(begin(), capacity() * 2);
            new(begin() + _size) T(tmp);
        } else {
            new(begin() + _size) T(element);
        }
        ++_size;
    }

    void pop_back() {
        (end() - 1)->~T();
        --_size;
    }

    bool empty() const {
        return _size == 0;
    }

    size_t capacity() const {
        return is_small ? SMALL_SIZE : big_storage.capacity();
    }

    void reserve(size_t new_capacity) {
        if ((is_small && new_capacity > SMALL_SIZE) || (!is_small && new_capacity > capacity())) {
            expand_storage(my_begin(), new_capacity);
        } else if (!is_small && !big_storage.is_unique()) {
            expand_storage(my_begin(), capacity());
        }
    }

    void shrink_to_fit() {
        if (!is_small) expand_storage(small_storage, _size);
    }

    void clear() {
        if (is_small || big_storage.is_unique()) {
            remove(my_begin(), my_end());
        } else {
            big_storage.~storage();
            new(&big_storage) storage(capacity());
        }
        _size = 0;
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
            for (size_t i = _size; i < other._size; ++i) {
                new(small_storage + i) T(other.small_storage[i]);
                other.small_storage[i].~T();
            }
        } else if (!is_small && !other.is_small) {
            std::swap(big_storage, other.big_storage);
        } else {
            storage tmp = other.big_storage;
            other.big_storage.~storage();
            try {
                copy(small_storage, other.small_storage, _size);
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
        return big_storage._data();
    }

    iterator end() {
        if (is_small) return small_storage + _size;
        make_copy();
        return big_storage._data() + _size;
    }

    const_iterator begin() const {
        return is_small ? small_storage : big_storage._data();
    }

    const_iterator end() const {
        return (is_small ? small_storage : big_storage._data()) + _size;
    }

    iterator insert(const_iterator pos, T const& t) {
        ptrdiff_t diff = pos - begin();
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
        ptrdiff_t start = first - my_begin();
        ptrdiff_t count = last - my_begin();
        make_copy();
        for (size_t i = start; i < _size - count; ++i) {
            std::swap(*(my_begin() + i), *(my_begin() + i + count));
        }
        for (size_t i = 0; i < count; ++i) {
            pop_back();
        }
        return my_begin() + start;
    }

private:
    iterator my_begin() {
        return is_small ? small_storage : big_storage._data();
    }

    iterator my_end() {
        return my_begin() + _size;
    }

    void make_copy() {
        if (!is_small && !big_storage.is_unique()) {
            expand_storage(big_storage._data(), capacity());
        }
    }

    // copy from `from` to `to` count elements from begin
    void copy(T const* from, T* to, size_t count) {
        size_t i = 0;
        try {
            while (i < count) {
                new(to + i) T(from[i]);
                ++i;
            }
        } catch (...) {
            remove(to, to + i);
            throw;
        }
    }

    void remove(T* start, T* end) {
        if (start != nullptr) {
            ptrdiff_t count = end - start;
            for (ptrdiff_t i = count - 1; i >= 0; --i) {
                (start + i)->~T();
            }
        }
    }

    // makes _size == new_capacity in `from`; after expand data is big
    void expand_storage(T const* from, size_t new_capacity) {
        if (!new_capacity) {
            _size = 0;
            return;
        }
        storage tmp(new_capacity);
        copy(from, tmp._data(), _size);
        if (is_small || big_storage.is_unique()) {
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
    };

    struct storage {

        storage() = default;

        explicit storage(size_t capacity) :
            ctrl(static_cast<control_block*>(operator new(sizeof(control_block) + sizeof(T) * capacity,
                 static_cast<std::align_val_t>(alignof(control_block))))) {
            new(&ctrl->_counter) size_t(1);
            new(&ctrl->_capacity) size_t(capacity);
        }

        storage(storage const& other) : ctrl(other.ctrl) {
            ctrl->_counter++;
        }

        storage& operator=(storage const& other) {
            if (&other != this) {
                storage tmp(other);
                std::swap(tmp.ctrl, this->ctrl);
            }
            return *this;
        }

        ~storage() {
            if (ctrl->_counter == 1) {
                operator delete(ctrl, static_cast<std::align_val_t>(alignof(ctrl)));
            } else {
                ctrl->_counter--;
            }
        }

        T& operator[](size_t i) {
            return ctrl->_data[i];
        }

        T& operator[](size_t i) const {
            return ctrl->_data[i];
        }

        bool is_unique() {
            return ctrl->_counter == 1;
        }

        T* _data() {
            return ctrl->_data;
        }

        T* _data() const {
            return ctrl->_data;
        }

        size_t capacity() const {
            return ctrl->_capacity;
        }

    private:
        control_block* ctrl;
    };

    size_t _size;
    bool is_small;
    union {
        T small_storage[SMALL_SIZE];
        storage big_storage;
    };
};

