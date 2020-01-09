#include <list>

#include "hash/extendible_hash.h"
#include "page/page.h"

namespace cmudb {

    template <typename K, typename V>
    ExtendibleHash<K, V>::ExtendibleHash(): ExtendibleHash(2) {}
    /*
     * constructor
     * array_size: fixed array size for each bucket
     */
    template <typename K, typename V>
    ExtendibleHash<K, V>::ExtendibleHash(size_t size)
            : globalDepth(0), maxSize(static_cast<int>(size)), buckets({std::make_shared<Bucket>(0)}) {}
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
      std::lock_guard<std::mutex> guard(latch);
      return globalDepth;
    }

    /*
     * helper function to return local depth of one specific bucket
     * NOTE: you must implement this function in order to pass test
     */
    template <typename K, typename V>
    int ExtendibleHash<K, V>::GetLocalDepth(int bucket_id) const {
      std::lock_guard<std::mutex> guard(latch);
      return buckets[bucket_id]->depth;
    }

    /*
     * helper function to return current number of bucket in hash table
     */
    template <typename K, typename V>
    int ExtendibleHash<K, V>::GetNumBuckets() const {
      std::lock_guard<std::mutex> guard(latch);
      return static_cast<int>(buckets.size());
    }

    /*
     * lookup function to find value associate with input key
     */
    template <typename K, typename V>
    bool ExtendibleHash<K, V>::Find(const K &key, V &value) {
      std::lock_guard<std::mutex> guard(latch);
      size_t index = HashKey(key) & ((1 << globalDepth) - 1);
      // index <--> 去 筒 中 找
      auto bucket = buckets[index];
      if (bucket->map.count(key)) {
        value = bucket->map[key];
        return true;
      }
      return false;
    }

    /*
     * delete <key,value> entry in hash table
     * Shrink & Combination is not required for this project
     */
    template <typename K, typename V>
    bool ExtendibleHash<K, V>::Remove(const K &key) {
      std::lock_guard<std::mutex> guard(latch);
      size_t index = HashKey(key) & ((1 << globalDepth) - 1);
      auto bucket = buckets[index];
      if (bucket->map.count(key)) {
        bucket->map.erase(key);
        return true;
      }
      return false;
    }

    /*
     * insert <key,value> entry in hash table
     * Split & Redistribute bucket when there is overflow and if necessary increase
     * global depth
     */
    template <typename K, typename V>
    void ExtendibleHash<K, V>::Insert(const K &key, const V &value) {
      std::lock_guard<std::mutex> guard(latch);
      size_t index = HashKey(key) & ((1 << globalDepth) - 1);
      std::shared_ptr<Bucket> bucket = buckets[index];
      if (bucket->map.count(key)) {
        bucket->map[key] = value;
        return;
      }
      while (true) {
        if (bucket->map.size() < (size_t)maxSize) break;
        if (++bucket->depth > globalDepth) {
          for (int i = 0; i < (1 << globalDepth); ++i) buckets.push_back(buckets[i]);
          ++globalDepth;
        }
        std::shared_ptr<Bucket> newBucket = std::make_shared<Bucket>(bucket->depth);
        auto it = bucket->map.begin();
        while (it != bucket->map.end()) {
          if (HashKey(it->first) & (1 << (bucket->depth - 1))) {
            newBucket->map[it->first] = it->second;
            it = bucket->map.erase(it);
          } else ++it;
        }
        for (size_t i = 0; i < buckets.size(); ++i) {
          if (buckets[i] == bucket && (i & (1 << ((bucket->depth) - 1)))) buckets[i] = newBucket;
        }
        index = HashKey(key) & ((1 << globalDepth) - 1);
        bucket = buckets[index];
      }
      buckets[index]->map[key] = value;
    }

    template class ExtendibleHash<page_id_t, Page *>;
    template class ExtendibleHash<Page *, std::list<Page *>::iterator>;
    // test purpose
    template class ExtendibleHash<int, std::string>;
    template class ExtendibleHash<int, std::list<int>::iterator>;
    template class ExtendibleHash<int, int>;
} // namespace cmudb