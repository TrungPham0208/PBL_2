#pragma once

#include <bits/stdc++.h>

#include "pbl/dsa/DynamicArray.h"

namespace pbl {
namespace dsa {

using namespace std;
template <typename T>
class PriorityQueue {
public:
    void push(const T& value, int priority) {
        entries_.push_back(Entry{value, priority, next_sequence_++});
        sift_up(entries_.size() - 1);
    }

    T pop() {
        if (empty()) {
            throw out_of_range("cannot remove from an empty priority queue");
        }
        T value = entries_[0].value;
        entries_[0] = entries_[entries_.size() - 1];
        entries_.pop_back();
        if (!empty()) {
            sift_down(0);
        }
        return value;
    }

    const T& top() const { // max
        if (empty()) {
            throw out_of_range("empty priority queue has no top");
        }
        return entries_[0].value;
    }

    bool empty() const { return entries_.empty(); }
    int size() const { return entries_.size(); }

private:
    struct Entry {
        T value;
        int priority;
        int sequence;
    };

    DynamicArray<Entry> entries_;
    int next_sequence_ = 0;

    bool higher(const Entry& left, const Entry& right) {
        if (left.priority != right.priority) {
            return left.priority > right.priority;
        }
        return left.sequence < right.sequence;
    }

    void sift_up(int index) {
        while (index > 0) {
            const int parent = (index - 1) / 2;
            if (!higher(entries_[index], entries_[parent])) {
                break;
            }
            swap(entries_[index], entries_[parent]);
            index = parent;
        }
    }

    void sift_down(int index) {
        while (true) {
            const int left = index * 2 + 1;
            const int right = left + 1;
            int best = index;
            if (left < entries_.size() && higher(entries_[left], entries_[best])) {
                best = left;
            }
            if (right < entries_.size() && higher(entries_[right], entries_[best])) {
                best = right;
            }
            if (best == index) {
                break;
            }
            swap(entries_[index], entries_[best]);
            index = best;
        }
    }
};

}  // namespace dsa
}  // namespace pbl