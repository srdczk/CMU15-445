/*
 * extendible_hash.h : implementation of in-memory hash table using extendible
 * hashing
 *
 * Functionality: The buffer pool manager must maintain a page table to be able
 * to quickly map a PageId to its corresponding memory location; or alternately
 * report that the PageId does not match any currently-buffered page.
 */

#pragma once

#include <cstdlib>
#include <vector>
#include <string>
#include <memory>
#include <unordered_map>
#include <mutex>

#include "hash/hash_table.h"

using namespace std;

namespace cmudb {

    template <typename K, typename V>
    class ExtendibleHash : public HashTable<K, V> {
    private:
        struct Bucket {
            int depth;
            std::unordered_map<K, V> map;
            Bucket(int d): depth(d) {}
        };
    public:

        // constructor
        ExtendibleHash(size_t size);
        ExtendibleHash();
        // helper function to generate hash addressing
        size_t HashKey(const K &key);
        // helper function to get global & local depth
        int GetGlobalDepth() const;
        int GetLocalDepth(int bucket_id) const;
        int GetNumBuckets() const;
        // lookup and modifier
        bool Find(const K &key, V &value) override;
        bool Remove(const K &key) override;
        void Insert(const K &key, const V &value) override;
    private:
        int globalDepth;
        int maxSize;
        std::vector<std::shared_ptr<Bucket>> buckets;
        // 上锁
        mutable std::mutex latch;
    };
} // namespace cmudb