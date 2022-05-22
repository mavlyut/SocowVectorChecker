#pragma once

#include <array>
#include <cstddef>

template <typename T, size_t SMALL_SIZE>
struct socow_vector {
    using iterator = T*;
    using const_iterator = T const*;

    // O(1) nothrow
    socow_vector(): is_small(true), size_(0) {}

    // O(SMALL_SIZE) basic | O(N) strong
    socow_vector(socow_vector const& other) {
        if (other.is_small) {
            data_copy(other.small_data.begin(), other.size_, small_data.begin());
        } else {
            new (&big_data) buffer_storage(other.big_data);
        }

        size_ = other.size_;
        is_small = other.is_small;
    }

    // O(SMALL_SIZE) basic | O(N) strong
    socow_vector& operator=(socow_vector const& other) {
        if (this != &other) {
            socow_vector(other).swap(*this);
        }
        return *this;
    }

    // O(N) nothrow
    ~socow_vector() {
        if (is_small) {
            clear();
        } else {
            big_data.clear(size_);
            big_data.~buffer_storage();
        }
    }

    // O(1) nothrow | O(N) strong
    T& operator[](size_t i) {
        if (is_small) {
            return small_data[i];
        } else {
            big_data.own(size_);
            return big_data.at(i);
        }
    }

    // O(1) nothrow
    T const& operator[](size_t i) const {
        return is_small ? small_data[i] : big_data.at(i);
    }

    // O(1) nothrow | O(N) strong
    T* data() {
        if (is_small) {
            return small_data.begin();
        } else {
            big_data.own(size_);
            return big_data.data();
        }
    }

    // O(1) nothrow
    T const* data() const {
        return is_small ? small_data.begin() : big_data.data();
    }

    // O(1) nothrow
    size_t size() const {
        return size_;
    }

    // O(1) nothrow | O(N) strong
    T& front() {
        if (is_small) {
            return small_data[0];
        } else {
            big_data.own(size_);
            return big_data.at(0);
        }
    }

    // O(1) nothrow
    T const& front() const {
        return is_small ? small_data[0] : big_data.at(0);
    }

    // O(1) nothrow | O(N) strong
    T& back() {
        if (is_small) {
            return small_data[size_ - 1];
        } else {
            big_data.own(size_);
            return big_data.at(size_ - 1);
        }
    }

    // O(1) nothrow
    T const& back() const {
        return is_small ? small_data[size_ - 1] : big_data.at(size_ - 1);
    }

    // O(1) strong
    void push_back(T const& t) {
        if (is_small && size_ < SMALL_SIZE) {
            new (&small_data[size_]) T(t);
        } else {
            T temp(t);
            if (is_small) {
                small_to_big(2 * SMALL_SIZE);
            } else {
                big_data.own(size_, (size_ == capacity() ? 2 : 1) * capacity());
            }
            new (&(big_data.data()[size_])) T(temp);
        }

        size_++;
    }

    // O(1) nothrow | O(N) strong
    void pop_back() {
        if (is_small) {
            small_data[--size_].~T();
        } else {
            big_data.own(size_);
            big_data.data()[--size_].~T();
        }
    }

    // O(1) nothrow
    bool empty() const {
        return size_ == 0;
    }

    // O(1) nothrow
    size_t capacity() const {
        return is_small ? SMALL_SIZE : big_data.capacity();
    }

    // O(N) strong
    void reserve(size_t new_capacity) {
        if (!is_small) {
            big_data.own(size_, std::max(new_capacity, capacity()));
        } else if (new_capacity > SMALL_SIZE) {
            small_to_big(new_capacity);
        }
    }

    // O(N) basic | string
    void shrink_to_fit() {
        if (!is_small) {
            if (size_ <= SMALL_SIZE) {
                big_to_small();
            } else {
                big_data.own(size_, size_);
            }
        }
    }

    // O(N) nothrow
    void clear() {
        erase(begin(), end());
    }

    // O(1) basic | strong
    void swap(socow_vector& other) {
        if (is_small && other.is_small) {
            swap_smalls(*this, other);
        } else if (!is_small && !other.is_small) {
            big_data.swap(other.big_data);
            std::swap(size_, other.size_);
            std::swap(is_small, other.is_small);
        } else if (is_small) {
            swap_small_big(*this, other);
        } else {
            swap_small_big(other, *this);
        }
    }

    // O(1) nothrow | O(N) strong
    iterator begin() {
        if (is_small) {
            return small_data.begin();
        } else {
            big_data.own(size_);
            return big_data.data();
        }
    }

    // O(1) nothrow | O(N) strong
    iterator end() {
        return begin() + size_;
    }

    // O(1) nothrow
    const_iterator begin() const {
        return is_small ? small_data.begin() : big_data.data();
    }

    // O(1) nothrow
    const_iterator end() const {
        return begin() + size_;
    }

    // O(N) strong
    iterator insert(const_iterator pos, T const& val) {
        ptrdiff_t ind = index(pos);
        push_back(val);

        for (size_t i = size_ - 1; i - ind > 0; i--) {
            swap(i, i - 1);
        }

        return begin() + ind;
    }

    // O(N) nothrow(swap) | strong
    iterator erase(const_iterator pos) {
        return erase(pos, pos + 1);
    }

    // O(N) nothrow(swap) | strong
    iterator erase(const_iterator first,
                   const_iterator last) {
        ptrdiff_t ind = index(first);

        if (first <= last) {
            ptrdiff_t swaps = last - first;

            if (!is_small) {
                big_data.own(size_);
            }

            for (size_t i = ind; i + swaps < size_; i++) {
                swap(i, i + swaps);
            }

            while (swaps-- > 0) {
                pop_back();
            }
        }

        return begin() + ind;
    }

private:
    struct data_block {
        size_t occurrences;
        size_t capacity;
        T data[];
    };

    struct buffer_storage {
        // O(1) nothrow
        buffer_storage() = default;

        // O(1) nothrow
        buffer_storage(buffer_storage const& other): block(other.block) {
            block->occurrences++;
        }

        // O(1) nothrow
        explicit buffer_storage(data_block* block): block(block) {
            block->occurrences++;
        }

        // O(N) strong
        buffer_storage(T const* data, size_t length, size_t capacity) {
            if (length <= capacity) {
                block = static_cast<data_block*>(
                    operator new(sizeof(data_block) + capacity * sizeof(T),
                                 static_cast<std::align_val_t>(alignof(data_block))));

                try {
                    data_copy(data, length, block->data);
                } catch (...) {
                    operator delete(block, static_cast<std::align_val_t>(alignof(data_block)));
                    throw;
                }

                block->capacity = capacity;
                block->occurrences = 1;
            } else {
                throw std::runtime_error("capacity < length in buffer_storage constructor");
            }
        }

        // O(1) nothrow
        void clear(size_t len) {
            if (block->occurrences == 1) {
                destruct_data(block->data, len);
                operator delete(block, static_cast<std::align_val_t>(alignof(data_block)));
            } else {
                block->occurrences--;
            }
        }

        // O(1) nothrow
        T& at(size_t index) const {
            return block->data[index];
        }

        // O(1) nothrow
        T* data() const {
            return block->data;
        }

        // O(1) nothrow
        size_t capacity() const {
            return block->capacity;
        }

        // O(N) strong
        void own(size_t len) {
            own(len, block->capacity);
        }

        // O(N) strong
        void own(size_t len, size_t new_capacity) {
            if (len <= new_capacity) {
                if (block->occurrences > 1 || block->capacity != new_capacity) {
                    buffer_storage temp(data(), len, new_capacity);
                    temp.swap(*this);
                    temp.clear(len);
                }
            }
        }

        void swap(buffer_storage &other) {
            std::swap(block, other.block);
        }

    private:
        data_block *block;
    };

    ptrdiff_t index(const_iterator pos) const {
        return pos - (is_small ? small_data.begin() : big_data.data());
    }

    // O(1) nothrow
    void swap(size_t i, size_t j) {
        if (is_small) {
            std::swap(small_data[i], small_data[j]);
        } else {
            std::swap(big_data.at(i), big_data.at(j));
        }
    }

    // O(N) nothrow
    static void destruct_data(T *data, size_t len) {
        destruct_data(data, 0, len);
    }

    // O(N) nothrow
    static void destruct_data(T *data, size_t pos, size_t len) {
        while (len > 0) {
            data[--len + pos].~T();
        }
    }

    // O(from_size) strong
    static void data_copy(T const *data_from, const size_t from_size, T *data_to) {
        size_t count = 0;
        try {
            for (; count < from_size; count++) {
                new (&data_to[count]) T(data_from[count]);
            }
        } catch (...) {
            destruct_data(data_to, count);
            throw;
        }
    }

    // O(SMALL_SIZE) basic
    void swap_smalls(socow_vector &a, socow_vector &b) {
        size_t minimum = std::min(a.size_, b.size_);
        for (size_t i = 0; i < minimum; i++) {
            std::swap(a.small_data[i], b.small_data[i]);
        }

        if (a.size_ > b.size_) {
            swap_smalls_tail(a, b);
        } else {
            swap_smalls_tail(b, a);
        }

        std::swap(a.size_, b.size_);
    }

    void swap_smalls_tail(socow_vector &bigger, socow_vector & smaller) {
        size_t count = smaller.size_;
        try {
            for (; count < bigger.size_; count++) {
                new (&smaller.small_data[count]) T(bigger.small_data[count]);
                bigger.small_data[count].~T();
            }
        } catch (...) {
            destruct_data(bigger.small_data.begin(), count, bigger.size_ - count + 1);
            bigger.size_ = smaller.size_;
            smaller.size_ = count;
            throw;
        }
    }

    // O(N) strong
    void swap_small_big(socow_vector &small, socow_vector &big) {
        buffer_storage temp(big.big_data);
        big.big_data.clear(big.size_);

        try {
            data_copy(small.data(), small.size_, big.small_data.begin());
        } catch (...) {
            new (&big.big_data) buffer_storage(temp);
            temp.clear(big.size_);
            throw;
        }

        destruct_data(small.data(), small.size());
        new (&small.big_data) buffer_storage(temp);

        temp.clear(big.size_);
        std::swap(small.size_, big.size_);
        std::swap(small.is_small, big.is_small);
    }

    // O(N) strong
    void small_to_big(const size_t new_capacity) {
        buffer_storage temp(small_data.begin(), size_, new_capacity);

        destruct_data(small_data.begin(), size_);
        new (&big_data) buffer_storage(temp);

        is_small = false;

        temp.clear(size_);
    }

    // O(N) strong
    void big_to_small() {
        buffer_storage temp(big_data);

        big_data.clear(size_);
        try {
            data_copy(temp.data(), size_, small_data.begin());
        } catch (...) {
            new (&big_data) buffer_storage(temp);
            temp.clear(size_);
            throw;
        }

        is_small = true;

        temp.clear(size_);
    }

    size_t size_;
    bool is_small;
    union {
        std::array<T, SMALL_SIZE> small_data;
        buffer_storage big_data;
    };
};
