#include <list>

#include "hash/extendible_hash.h"
#include "page/page.h"

namespace cmudb {

/*
 * constructor
 * array_size: fixed array size for each bucket
 */
template <typename K, typename V>
ExtendibleHash<K, V>::ExtendibleHash(size_t size)
: globalDepth_(0), maxSize_(size), buckets_({ std::make_shared<Bucket>(0) }) {}

template <typename K, typename V> 
ExtendibleHash<K, V>::ExtendibleHash(): ExtendibleHash(2) {}
/*
 * helper function to calculate the hashing address of input key
 */
template <typename K, typename V>
size_t ExtendibleHash<K, V>::HashKey(const K &key) { return std::hash<K>{}(key); }

/*
 * helper function to return global depth of hash table
 * NOTE: you must implement this function in order to pass test
 */
template <typename K, typename V>
int ExtendibleHash<K, V>::GetGlobalDepth() const { 
    std::lock_guard<std::mutex> guard(latch_);
    return globalDepth_; 
}

/*
 * helper function to return local depth of one specific bucket
 * NOTE: you must implement this function in order to pass test
 */
template <typename K, typename V>
int ExtendibleHash<K, V>::GetLocalDepth(int bucket_id) const { 
    std::lock_guard<std::mutex> guard(latch_);
    return buckets_[bucket_id]->depth; 
}

/*
 * helper function to return current number of bucket in hash table
 */
template <typename K, typename V>
int ExtendibleHash<K, V>::GetNumBuckets() const { 
    std::lock_guard<std::mutex> guard(latch_);
    return buckets_.size(); 
}

/*
 * lookup function to find value associate with input key
 */
template <typename K, typename V>
bool ExtendibleHash<K, V>::Find(const K &key, V &value) {
    std::lock_guard<std::mutex> guard(latch_);
    auto index = HashKey(key) & ((1 << globalDepth_) - 1);
    if (!buckets_[index]->map.count(key)) return false;
    value = buckets_[index]->map[key];
    return true;
}

/*
 * delete <key,value> entry in hash table
 * Shrink & Combination is not required for this project
 */
template <typename K, typename V>
bool ExtendibleHash<K, V>::Remove(const K &key) {
    std::lock_guard<std::mutex> guard(latch_);
    auto index = HashKey(key) & ((1 << globalDepth_) - 1);
    if (!buckets_[index]->map.count(key))  return false;
    buckets_[index]->map.erase(key);
    return true;
}

/*
 * insert <key,value> entry in hash table
 * Split & Redistribute bucket when there is overflow and if necessary increase
 * global depth
 */
template <typename K, typename V>
void ExtendibleHash<K, V>::Insert(const K &key, const V &value) {
    std::lock_guard<std::mutex> guard(latch_);
    auto index = HashKey(key) & ((1 << globalDepth_) - 1);
    auto bucket = buckets_[index];
    bucket->map[key] = value;
    while ((int)bucket->map.size() > maxSize_) {
        auto newBucket = std::make_shared<Bucket>(bucket->depth + 1);
        auto it = bucket->map.begin();
        while (it != bucket->map.end()) {
            if (HashKey(it->first) & (1 << bucket->depth)) {
                newBucket->map[it->first] = it->second;
                it = bucket->map.erase(it);
            } else ++it;
        }
        if (++bucket->depth > globalDepth_) {
            for (int i = 0; i < (1 << globalDepth_); ++i) {
                if (buckets_[i] == bucket) buckets_.push_back(newBucket);
                else buckets_.push_back(buckets_[i]);
            }
            ++globalDepth_;
        } else {
            for (int i = 0; i < (1 << globalDepth_); ++i) {
                if (buckets_[i] == bucket && (i & (1 << (bucket->depth - 1)))) buckets_[i] = newBucket;
            }
        }
        index = HashKey(key) & ((1 << globalDepth_) - 1);
        bucket = buckets_[index];
    }
}

template class ExtendibleHash<page_id_t, Page *>;
template class ExtendibleHash<Page *, std::list<Page *>::iterator>;
// test purpose
template class ExtendibleHash<int, std::string>;
template class ExtendibleHash<int, std::list<int>::iterator>;
template class ExtendibleHash<int, int>;
} // namespace cmudb
