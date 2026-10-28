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
    HashMap(int bucket_count = 16);

    // Release all dynamically allocated entries when the map is destroyed.
    ~HashMap();

    void put(const Key& key, const Value& value);
    Value& at(const Key& key);
    const Value& at(const Key& key) const;
    bool contains(const Key& key) const;
    bool remove(const Key& key);
    void clear();
    int size() const;
    bool empty() const;

private:
    struct Entry {
        Key key;
        Value value;
        Entry* next;
    };

    DynamicArray<Entry*> buckets_;
    int bucket_count_;
    int size_ = 0;

    int bucket_index(Key key) const;
    Entry* find_entry(const Key& key);
    const Entry* find_entry(const Key& key) const;
};

template <typename Key, typename Value>
HashMap<Key, Value>::HashMap(int bucket_count) {
    // Allocate one pointer slot for each requested bucket.
    buckets_ = DynamicArray<Entry*>(bucket_count);
    bucket_count_ = bucket_count;
    // A map with no buckets cannot store or find any key.
    if (bucket_count == 0) {
        throw invalid_argument("hash map requires at least one bucket");
    }
}

template <typename Key, typename Value>
HashMap<Key, Value>::~HashMap() {
    clear();
}

template <typename Key, typename Value>
void HashMap<Key, Value>::put(const Key& key, const Value& value) {
    Entry* entry = find_entry(key);
    if (entry != nullptr) {
        entry->value = value;
        return;
    }
    const int index = bucket_index(key);
    buckets_[index] = new Entry{key, value, buckets_[index]};
    ++size_;
}

template <typename Key, typename Value>
Value& HashMap<Key, Value>::at(const Key& key) {
    Entry* entry = find_entry(key);
    if (entry == nullptr) {
        throw out_of_range("key is not present in hash map");
    }
    return entry->value;
}

template <typename Key, typename Value>
const Value& HashMap<Key, Value>::at(const Key& key) const {
    const Entry* entry = find_entry(key);
    if (entry == nullptr) {
        throw out_of_range("key is not present in hash map");
    }
    return entry->value;
}

template <typename Key, typename Value>
bool HashMap<Key, Value>::contains(const Key& key) const {
    return find_entry(key) != nullptr;
}

template <typename Key, typename Value>
bool HashMap<Key, Value>::remove(const Key& key) {
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

template <typename Key, typename Value>
void HashMap<Key, Value>::clear() {
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

template <typename Key, typename Value>
int HashMap<Key, Value>::size() const {
    return size_;
}

template <typename Key, typename Value>
bool HashMap<Key, Value>::empty() const {
    return size_ == 0;
}

// The final const means this function promises not to change any objects here.
template <typename Key, typename Value>
int HashMap<Key, Value>::bucket_index(Key key) const {
    hash<Key> calculator;
    return calculator(key) % bucket_count_;
}

template <typename Key, typename Value>
typename HashMap<Key, Value>::Entry* HashMap<Key, Value>::find_entry(const Key& key) {
    Entry* current = buckets_[bucket_index(key)];
    while (current != nullptr && !(current->key == key)) {
        current = current->next;
    }
    return current;
}

template <typename Key, typename Value>
const typename HashMap<Key, Value>::Entry* HashMap<Key, Value>::find_entry(const Key& key) const {
    const Entry* current = buckets_[bucket_index(key)];
    while (current != nullptr && !(current->key == key)) {
        current = current->next;
    }
    return current;
}

}  // namespace dsa
}  // namespace pbl