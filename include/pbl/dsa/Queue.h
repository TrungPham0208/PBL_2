#pragma once

#include <bits/stdc++.h>

#include "pbl/dsa/DynamicArray.h"

using namespace std;

namespace pbl {
namespace dsa {


template <typename T>
class Queue {
    private:
    DynamicArray<T> slots_;
    int capacity_;
    int head_;
    int tail_;
    int size_;
public:
    Queue(int capacity = 1);

    void push(const T& value);
    T pop();
    const T& front() const;
    bool empty() const;
    bool full() const;
    int size() const;
    int capacity() const;

};

template <typename T>
Queue<T>::Queue(int capacity) {
    slots_ = DynamicArray<T>(capacity);
    capacity_ = capacity;
    head_ = 0;
    tail_ = 0;
    size_ = 0;
    if (capacity == 0) {
        throw invalid_argument("queue capacity must be positive");
    }
}

// % capacity needed because, suppose:
// Now pop one value:
// There is now one free slot at index 0. The next push must reuse index 0:
template <typename T>
void Queue<T>::push(const T& value) {
    if (full()) {
        throw overflow_error("queue is full");
    }
    slots_[tail_] = value;
    tail_ = (tail_ + 1) % capacity_;
    ++size_;
}

template <typename T>
T Queue<T>::pop() {
    if (empty()) {
        throw out_of_range("cannot remove from an empty queue");
    }
    T value = slots_[head_];
    head_ = (head_ + 1) % capacity_;
    --size_;
    return value;
}

template <typename T>
const T& Queue<T>::front() const {
    if (empty()) {
        throw out_of_range("empty queue has no front");
    }
    return slots_[head_];
}

template <typename T>
bool Queue<T>::empty() const {
    return size_ == 0;
}

template <typename T>
bool Queue<T>::full() const {
    return size_ == capacity_;
}

template <typename T>
int Queue<T>::size() const {
    return size_;
}

template <typename T>
int Queue<T>::capacity() const {
    return capacity_;
}

}  // namespace dsa
}  // namespace pbl
