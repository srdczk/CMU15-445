/**
 * lru_replacer.h
 *
 * Functionality: The buffer pool manager must maintain a LRU list to collect
 * all the pages that are unpinned and ready to be swapped. The simplest way to
 * implement LRU is a FIFO queue, but remember to dequeue or enqueue pages when
 * a page changes from unpinned to pinned, or vice-versa.
 */

#pragma once

#include "buffer/replacer.h"
#include "hash/extendible_hash.h"

namespace cmudb {

template <typename T> class LRUReplacer : public Replacer<T> {
private:
    struct Node {
        T val;
        std::shared_ptr<Node> pre;
        std::shared_ptr<Node> next;
        Node() {}
        Node(T v): val(v), pre(nullptr), next(nullptr) {}
    };
public:
  LRUReplacer();

  ~LRUReplacer();

  void Insert(const T &value);

  bool Victim(T &value);

  bool Erase(const T &value);

  size_t Size();

private:
    std::shared_ptr<Node> head_, tail_;
    std::unordered_map<T, std::shared_ptr<Node>> map_;
    std::mutex latch_;
};

} // namespace cmudb
