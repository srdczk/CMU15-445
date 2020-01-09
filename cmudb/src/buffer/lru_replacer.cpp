/**
 * LRU implementation
 */
#include "buffer/lru_replacer.h"
#include "page/page.h"

namespace cmudb {

    template <typename T> LRUReplacer<T>::LRUReplacer()
            : head(std::make_shared<Node>()), tail(std::make_shared<Node>()){
      head->next = tail;
      tail->pre = head;
    }

    template <typename T> LRUReplacer<T>::~LRUReplacer() {
      // 智能指针 引用计数问题 <--> 将前后 指针全部设为 null,
      while (head) {
        auto next = head->next;
        head->next = nullptr;
        head = next;
      }
      while (tail) {
        auto pre = tail->pre;
        tail->pre = nullptr;
        tail = pre;
      }
    }

/*
 * Insert value into LRU
 */
    template <typename T> void LRUReplacer<T>::Insert(const T &value) {
      if (map.count(value)) {
        auto node = map[value], pre = node->pre, next = node->next;
        pre->next = next;
        next->pre = pre;
        node->next = head->next;
        node->next->pre = node;
        node->pre = head;
        head->next = node;
      } else {
        auto node = std::make_shared<Node>(value);
        map[value] = node;
        node->next = head->next;
        node->next->pre = node;
        node->pre = head;
        head->next = node;
      }
    }

/* If LRU is non-empty, pop the head member from LRU to argument "value", and
 * return true. If LRU is empty, return false
 */
    template <typename T> bool LRUReplacer<T>::Victim(T &value) {
      if (map.empty()) return false;
      auto node = tail->pre;
      value = node->val;
      map.erase(node->val);
      node->pre->next = tail;
      tail->pre = node->pre;
      node->pre = nullptr;
      node->next = nullptr;
      return true;
    }

/*
 * Remove value from LRU. If removal is successful, return true, otherwise
 * return false
 */
    template <typename T> bool LRUReplacer<T>::Erase(const T &value) {
      if (!map.count(value)) return false;
      auto node = map[value];
      map.erase(value);
      auto pre = node->pre, next = node->next;
      pre->next = next;
      next->pre = pre;
      node->pre = nullptr;
      node->next = nullptr;
      return true;
    }

    template <typename T> size_t LRUReplacer<T>::Size() { return map.size(); }

    template class LRUReplacer<Page *>;
// test only
    template class LRUReplacer<int>;

} // namespace cmudb