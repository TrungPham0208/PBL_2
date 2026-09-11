#pragma once

#include <bits/stdc++.h>

#include "pbl/dsa/DynamicArray.h"

namespace pbl {
namespace dsa {

using namespace std;
// Milestone 1: bounded FIFO storage for packets and future link queues.
template <typename T>
class Queue {
public:
                Queue(int capacity) {
                        slots_ = DynamicArray<T>(capacity);
                        capacity_ = capacity;
                        head_ = 0;
                        tail_ = 0;
                        size_ = 0;
        if (capacity == 0) {
                throw invalid_argument("queue capacity must be positive");
        }
    }

    void push(const T& value) {
        if (full()) {
                throw overflow_error("queue is full");
        }
        slots_[tail_] = value;
        tail_ = (tail_ + 1) % capacity_;
        ++size_;
    }

    T pop() {
        if (empty()) {
                throw out_of_range("cannot remove from an empty queue");
        }
        T value = slots_[head_];
        head_ = (head_ + 1) % capacity_;
        --size_;
        return value;
    }

    const T& front() const {
        if (empty()) {
                throw out_of_range("empty queue has no front");
        }
        return slots_[head_];
    }

        bool empty() const { return size_ == 0; }
        bool full() const { return size_ == capacity_; }
        int size() const { return size_; }
        int capacity() const { return capacity_; }

private:
    DynamicArray<T> slots_;
        int capacity_;
        int head_ = 0;
        int tail_ = 0;
        int size_ = 0;
};

}  // namespace dsa
}  // namespace pbl