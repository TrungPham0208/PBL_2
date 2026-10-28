#pragma once

#include <bits/stdc++.h>

namespace pbl {
namespace dsa {

using namespace std;


template <typename T>
class DynamicArray {
public:
    DynamicArray(int capacity = 0);
    DynamicArray(const DynamicArray& other);
    DynamicArray& operator=(const DynamicArray& other);
    ~DynamicArray();

    void push_back(T value);
    void pop_back();

    // DynamicArray<int> values;
    // values.at(0) = 10;
    T& at(int index);

    // Used when the array itself is read-only:
    // const DynamicArray<int> values;
    // values.at(0);        // allowed
    // values.at(0) = 10;   // blocked
    const T& at(int index) const;

    T& operator[](int index);
    const T& operator[](int index) const;

    int size();
    int size() const;
    int capacity();
    int capacity() const;
    bool empty();
    bool empty() const;

    void clear();

private:
    T* data_;
    int size_;
    int capacity_;

    void ensure_capacity(int required);
    void check_index(int index);
};

template <typename T>
DynamicArray<T>::DynamicArray(int capacity) {
    data_ = nullptr;
    size_ = 0;
    capacity_ = capacity;

    if (capacity_ > 0) {
        data_ = new T[capacity_]();
    }
}

template <typename T>
DynamicArray<T>::DynamicArray(const DynamicArray& other) {
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

template <typename T>
DynamicArray<T>& DynamicArray<T>::operator=(const DynamicArray& other) {
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

template <typename T>
DynamicArray<T>::~DynamicArray() {
    delete[] data_;
}

template <typename T>
void DynamicArray<T>::push_back(T value) {
    ensure_capacity(size_ + 1);
    data_[size_++] = value;
}

template <typename T>
void DynamicArray<T>::pop_back() {
    if (empty()) {
        throw out_of_range("cannot remove from an empty dynamic array");
    }
    --size_;
}

template <typename T>
T& DynamicArray<T>::at(int index) {
    check_index(index);
    return data_[index];
}

template <typename T>
const T& DynamicArray<T>::at(int index) const {
    check_index(index);
    return data_[index];
}

template <typename T>
T& DynamicArray<T>::operator[](int index) {
    return data_[index];
}

template <typename T>
const T& DynamicArray<T>::operator[](int index) const {
    return data_[index];
}

template <typename T>
int DynamicArray<T>::size() {
    return size_;
}

template <typename T>
int DynamicArray<T>::size() const {
    return size_;
}

template <typename T>
int DynamicArray<T>::capacity() {
    return capacity_;
}

template <typename T>
int DynamicArray<T>::capacity() const {
    return capacity_;
}

template <typename T>
bool DynamicArray<T>::empty() {
    return size_ == 0;
}

template <typename T>
bool DynamicArray<T>::empty() const {
    return size_ == 0;
}

template <typename T>
void DynamicArray<T>::clear() {
    size_ = 0;
}

template <typename T>
void DynamicArray<T>::ensure_capacity(int required) {
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

template <typename T>
void DynamicArray<T>::check_index(int index) {
    if (index >= size_) {
        throw out_of_range("dynamic array index out of range");
    }
}

}  // namespace dsa
}  // namespace pbl