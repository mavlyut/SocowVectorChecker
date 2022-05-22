#pragma once
#include <cstddef>

template <typename T, size_t SMALL_SIZE>
struct socow_vector {
    using iterator = T*;
    using const_iterator = T const*;

    socow_vector() : size_(0), small(true)
    {}

    socow_vector(socow_vector const& other) :
          size_(other.size_), small(other.small) {
        if (other.small) {
            copy_range(const_cast<T*>(other.static_storage), static_storage, other.size_);
        } else {
            new(&dynamic_storage) dynamic_buffer(other.dynamic_storage);
        }
    }

    socow_vector& operator=(socow_vector const& other) {
        if (this != &other) {
            socow_vector t(other);
            t.swap(*this);
        }
        return *this;
    }

    ~socow_vector() {
        if (small) {
            destruct_range(current_begin(), current_end());
        } else {
            if (dynamic_storage.unique()) {
                destruct_range(current_begin(), current_end());
            }
            dynamic_storage.~dynamic_buffer();
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

    void push_back(T const& t) {
        if (capacity() == size_) {
            T t_tmp = t;
            extend_buffer(capacity() * 2, begin(), size_);
            new (begin() + size_) T(t_tmp);
        } else {
            new (begin() + size_) T(t);
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
        if (small) {
            return SMALL_SIZE;
        }
        return dynamic_storage.m_data->capacity_;
    }
    void reserve(size_t new_cap) {
        if ((small && new_cap > SMALL_SIZE) || (!small && new_cap > capacity())) {
            extend_buffer(new_cap, current_begin(), size_);
        } else if (!small && !dynamic_storage.unique()) {
            extend_buffer(capacity(), current_begin(), size_);
        }
    }
    void shrink_to_fit() {
        if (!small) {
            if (size_ <= SMALL_SIZE) {
                dynamic_buffer tmp = dynamic_storage;
                dynamic_storage.~dynamic_buffer();
                try {
                    copy_range(tmp.get(), static_storage, size_);
                } catch (...) {
                    new(&dynamic_storage) dynamic_buffer(tmp);
                    throw;
                }
                destruct_range(tmp.get(), tmp.get() + size_);
                small = true;
            } else if (size_ != capacity()) {
                extend_buffer(size_, dynamic_storage.get(), size_);
            }
        }
    }

    void clear() {
        if (small || dynamic_storage.unique()) {
            destruct_range(current_begin(), current_end());
        } else {
            dynamic_storage.~dynamic_buffer();
            new(&dynamic_storage) dynamic_buffer(capacity());
        }
        size_ = 0;
    }

    void swap(socow_vector& other) {
        if (!small && !other.small) {
            std::swap(dynamic_storage, other.dynamic_storage);
        } else if (small  && other.small) {
            this->swap_small(other);
        } else if (small  && !other.small) {
            this->swap_to_big(other);
        } else {
            other.swap_to_big(*this);
        }
        std::swap(other.size_, size_);
        std::swap(other.small, small);
    }

    iterator begin() {
        if (small) {
            return static_storage;
        }
        create_new_copy();
        return dynamic_storage.get();
    }
    iterator end() {
        if (small) {
            return static_storage + size_;
        }
        create_new_copy();
        return dynamic_storage.get() + size_;
    }

    const_iterator begin() const {
        if (small) {
            return static_storage;
        }
        return dynamic_storage.get();
    }
    const_iterator end() const {
        if (small) {
            return static_storage + size_;
        }
        return dynamic_storage.get() + size_;
    }

    iterator insert(const_iterator pos, T const& t) {
        size_t p = pos - current_begin();
        push_back(t);
        for (size_t i = size_ - 1; i > p; i--) {
            std::swap(*(current_begin() + i), *(current_begin() + i - 1));
        }
        return current_begin() + p;
    }

    iterator erase(const_iterator pos) {
        return erase(pos, pos + 1);
    }

    iterator erase(const_iterator first, const_iterator last) {
        size_t pos = last - current_begin();
        size_t res = first - current_begin();
        create_new_copy();
        for (auto it = current_begin() + pos; it < current_end(); it++) {
            std::swap(*it, *(it - (last - first)));
        }
        auto old_size = size_;
        size_ -= last - first;
        destruct_range(current_begin() + size_, current_begin() + old_size);
        return current_begin() + res;
    }

private:
    void create_new_copy() {
        if (!small && !dynamic_storage.unique()) {
            extend_buffer(capacity(), dynamic_storage.get(), size_);
        }
    }

    void extend_buffer(size_t new_capacity, T const* source, size_t count) {
        if (new_capacity == 0) {
            size_ = 0;
            return;
        }
        dynamic_buffer tmp(new_capacity);
        copy_range(source, tmp.get(), count);
        if (small || dynamic_storage.unique()) {
            destruct_range(current_begin(), current_end());
        }
        if (!small) {
            dynamic_storage.~dynamic_buffer();
        }
        new (&dynamic_storage) dynamic_buffer(tmp);
        size_ = count;
        small = false;
    }

    void swap_to_big(socow_vector& big) {
        dynamic_buffer tmp = big.dynamic_storage;
        big.dynamic_storage.~dynamic_buffer();
        try {
            copy_range(static_storage, big.static_storage, size_);
        } catch (...) {
            new(&big.dynamic_storage) dynamic_buffer(tmp);
            throw;
        }
        destruct_range(current_begin(), current_end());
        new(&dynamic_storage) dynamic_buffer(tmp);
    }

    void swap_small(socow_vector& b) {
        size_t i = 0;
        while (i < size_ && i < b.size_) {
            std::swap(static_storage[i], b.static_storage[i]);
            i++;
        }
        while (i < size_) {
            new (b.static_storage + i) T(static_storage[i]);
            static_storage[i].~T();
            i++;
        }
        while (i < b.size_) {
            new (static_storage + i) T(b.static_storage[i]);
            b.static_storage[i].~T();
            i++;
        }
    }

    iterator current_begin() {
        if (small) {
            return static_storage;
        }
        return dynamic_storage.get();
    }
    iterator current_end() {
        if (small) {
            return static_storage + size_;
        }
        return dynamic_storage.get() + size_;
    }

    void copy_range(T const* source, T* receiver, size_t count) {
        size_t i = 0;
        try {
            while (i != count) {
                new (receiver + i) T(source[i]);
                i++;
            }
        } catch (...) {
            destruct_range(receiver, receiver + i);
            throw;
        }
    }

    static void destruct_range(T* start, T* end) {
        if (start != nullptr) {
            for (auto it = --end; it >= start; it--) {
                it->~T();
            }
        }
    }

    struct metadata {
        size_t capacity_;
        size_t ref_counter;
        T data_[];
    };

    struct dynamic_buffer {
        metadata* m_data;

        explicit dynamic_buffer(size_t cap) : m_data(static_cast<metadata*>(operator new(sizeof(metadata) + sizeof(T) * cap,
                                            static_cast<std::align_val_t>(alignof(metadata))))) {
            new(&m_data->ref_counter) size_t(1);
            new(&m_data->capacity_) size_t(cap);
        }

        dynamic_buffer(dynamic_buffer const& other) : m_data(other.m_data) {
            m_data->ref_counter++;
        }

        ~dynamic_buffer() {
            if (m_data->ref_counter == 1) {
                operator delete(m_data, static_cast<std::align_val_t>(alignof(metadata)));
            } else {
                m_data->ref_counter--;
            }
        }

        dynamic_buffer& operator=(dynamic_buffer const& other) {
            if (&other != this) {
                dynamic_buffer tmp(other);
                std::swap(tmp.m_data, this->m_data);
            }
            return *this;
        }

        T& operator[](size_t index) {
            return m_data->data_[index];
        }

        T& operator[](size_t index) const {
            return m_data->data_[index];
        }

        size_t unique() {
            return m_data->ref_counter == 1;
        }

        T* get() {
            return m_data->data_;
        }

        T* get() const {
            return m_data->data_;
        }
    };

    size_t size_;
    bool small;
    union {
        dynamic_buffer dynamic_storage;
        T static_storage[SMALL_SIZE];
    };
};
