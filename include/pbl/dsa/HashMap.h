#pragma once

#include <bits/stdc++.h>

#include "pbl/dsa/DynamicArray.h"

namespace pbl {
namespace dsa {

using namespace std;
template <typename Key, typename Value>
class HashMap {
public:
    // Create a hash map and use 16 buckets when the caller gives no value.
    HashMap(int bucket_count = 16) {
        // Allocate one pointer slot for each requested bucket.
        buckets_ = DynamicArray<Entry*>(bucket_count);
        bucket_count_ = bucket_count;
        // A map with no buckets cannot store or find any key.
        if (bucket_count == 0) {
            throw invalid_argument("hash map requires at least one bucket");
        }
    }

    // Release all dynamically allocated entries when the map is destroyed.
    ~HashMap() { clear(); }

    void put(const Key& key, const Value& value) {
        Entry* entry = find_entry(key);
        if (entry != nullptr) {
            entry->value = value;
            return;
        }
        const int index = bucket_index(key);
        buckets_[index] = new Entry{key, value, buckets_[index]};
        ++size_;
    }

    Value& at(const Key& key) {
        Entry* entry = find_entry(key);
        if (entry == nullptr) {
                throw out_of_range("key is not present in hash map");
        }
        return entry->value;
    }

    const Value& at(const Key& key) const {
        const Entry* entry = find_entry(key);
        if (entry == nullptr) {
                throw out_of_range("key is not present in hash map");
        }
        return entry->value;
    }

    bool contains(const Key& key) const { return find_entry(key) != nullptr; }

    bool remove(const Key& key) {
        const int index = bucket_index(key);
        Entry* current = buckets_[index];
        Entry* previous = nullptr;
        while (current != nullptr) {
            if (current->key == key) {
                if (previous == nullptr) {
                    buckets_[index] = current->next;
                } else {
                    previous->next = current->next;
                }
                delete current;
                --size_;
                return true;
            }
            previous = current;
            current = current->next;
        }
        return false;
    }

    void clear() {
        for (int index = 0; index < bucket_count_; ++index) {
            Entry* current = buckets_[index];
            while (current != nullptr) {
                Entry* next = current->next;
                delete current;
                current = next;
            }
            buckets_[index] = nullptr;
        }
        size_ = 0;
    }

    int size() const { return size_; }
    bool empty() const { return size_ == 0; }

private:
    struct Entry {
        Key key;
        Value value;
        // This pointer links entries that share the same hash bucket.
        Entry* next;
    };

    // Entry pointers are used because this milestone visibly teaches chained hash buckets.
    DynamicArray<Entry*> buckets_;
    int bucket_count_;
    int size_ = 0;

    // The final const means this function promises not to change any objects here.
    int bucket_index(Key key) const {
        hash<Key> calculator;
        return calculator(key) % bucket_count_;
    }

    Entry* find_entry(const Key& key) {
        Entry* current = buckets_[bucket_index(key)];
        while (current != nullptr && !(current->key == key)) {
            current = current->next;
        }
        return current;
    }

    const Entry* find_entry(const Key& key) const {
        const Entry* current = buckets_[bucket_index(key)];
        while (current != nullptr && !(current->key == key)) {
            current = current->next;
        }
        return current;
    }
};

}  // namespace dsa
}  // namespace pbl