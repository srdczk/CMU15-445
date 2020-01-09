/**
 * lru_replacer.h
 *
 * Functionality: The buffer pool manager must maintain a LRU list to collect
 * all the pages that are unpinned and ready to be swapped. The simplest way to
 * implement LRU is a FIFO queue, but remember to dequeue or enqueue pages when
 * a page changes from unpinned to pinned, or vice-versa.
 */

#pragma once

#include <unordered_map>
#include "buffer/replacer.h"
#include "hash/extendible_hash.h"

namespace cmudb {

    template <typename T> class LRUReplacer : public Replacer<T> {
    private:
        struct Node {
            T val;
            std::shared_ptr<Node> pre;
            std::shared_ptr<Node> next;
            Node(): pre(nullptr), next(nullptr) {}
            Node(T v): val(v), pre(nullptr), next(nullptr) {}
        };
    public:
        // do not change public interface
        LRUReplacer();

        ~LRUReplacer();

        void Insert(const T &value);

        bool Victim(T &value);

        bool Erase(const T &value);

        size_t Size();

    private:
        // add your member variables here
        // 链表加上首尾节点
        std::shared_ptr<Node> head;
        std::shared_ptr<Node> tail;
        std::unordered_map<T, std::shared_ptr<Node>> map;
    };

} // namespace cmudb