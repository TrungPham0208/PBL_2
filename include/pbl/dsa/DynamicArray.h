#pragma once

#include <bits/stdc++.h>

namespace pbl {
namespace dsa {

using namespace std;

// Milestone 1: owned contiguous storage used by the reusable structures.
template <typename T>
class DynamicArray {
public:
    DynamicArray() {
        data_ = nullptr;
        size_ = 0;
        capacity_ = 0;
    }

    DynamicArray(int capacity) {
        data_ = nullptr;
        size_ = 0;
        capacity_ = capacity;
        if (capacity_ > 0) {
            data_ = new T[capacity_]();
        }
    }

    DynamicArray(const DynamicArray& other) {
        data_ = nullptr;
        size_ = other.size_;
        capacity_ = other.capacity_;
        if (capacity_ > 0) {
            data_ = new T[capacity_]();
            for (int index = 0; index < size_; ++index) {
                data_[index] = other.data_[index];
            }
        }
    }

    DynamicArray& operator=(const DynamicArray& other) {
        if (this != &other) {
            delete[] data_;
            data_ = nullptr;
            size_ = other.size_;
            capacity_ = other.capacity_;
            if (capacity_ > 0) {
                data_ = new T[capacity_]();
                for (int index = 0; index < size_; ++index) {
                    data_[index] = other.data_[index];
                }
            }
        }
        return *this;
    }

    ~DynamicArray() { delete[] data_; }

    void push_back(T value) {
        ensure_capacity(size_ + 1);
        data_[size_++] = value;
    }

    void pop_back() {
        if (empty()) {
            throw out_of_range("cannot remove from an empty dynamic array");
        }
        --size_;
    }

    T& at(int index) {
        check_index(index);
        return data_[index];
    }

    const T& at(int index) const {
        check_index(index);
        return data_[index];
    }

    T& operator[](int index) { return data_[index]; }
    const T& operator[](int index) const { return data_[index]; }

    int size() { return size_; }
    int size() const { return size_; }
    int capacity() { return capacity_; }
    int capacity() const { return capacity_; }
    bool empty() { return size_ == 0; }
    bool empty() const { return size_ == 0; }

    void clear() { size_ = 0; }

private:
    T* data_;
    int size_;
    int capacity_;

    void ensure_capacity(int required) {
        if (required <= capacity_) {
            return;
        }
        const int new_capacity = capacity_ == 0 ? 4 : capacity_ * 2;
        T* new_data = new T[new_capacity]();
        for (int index = 0; index < size_; ++index) {
            new_data[index] = data_[index];
        }
        delete[] data_;
        data_ = new_data;
        capacity_ = new_capacity;
    }

    void check_index(int index) {
        if (index >= size_) {
            throw out_of_range("dynamic array index out of range");
        }
    }

};

}  // namespace dsa
}  // namespace pbl