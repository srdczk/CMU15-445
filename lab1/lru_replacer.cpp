/**
 * LRU implementation
 */
#include "buffer/lru_replacer.h"
#include "page/page.h"

namespace cmudb {

template <typename T> LRUReplacer<T>::LRUReplacer()
: head_(std::make_shared<Node>()), tail_(std::make_shared<Node>()) {
    head_->next = tail_;
    tail_->pre = head_;
}

template <typename T> LRUReplacer<T>::~LRUReplacer() {
    while (head_) {
        auto next = head_->next;
        head_->next = nullptr;
        head_ = next;
    }
    while (tail_) {
        auto pre = tail_->pre;
        tail_->pre = nullptr;
        tail_ = pre;
    }
}

/*
 * Insert value into LRU
 */
template <typename T> void LRUReplacer<T>::Insert(const T &value) {
    std::lock_guard<std::mutex> guard(latch_);
    if (map_.count(value)) {
        auto node = map_[value];
        auto pre = node->pre, next = node->next;
        pre->next = next;
        next->pre = pre;
        node->next = head_->next;
        node->next->pre = node;
        node->pre = head_;
        head_->next = node;
    } else {
        auto node = std::make_shared<Node>(value);
        map_[value] = node;
        node->next = head_->next;
        node->next->pre = node;
        head_->next = node;
        node->pre = head_;
    }
}
/*
 * return true. If LRU is empty, return false
 */
template <typename T> bool LRUReplacer<T>::Victim(T &value) {
    std::lock_guard<std::mutex> guard(latch_);
    if (map_.empty()) return false;
    auto node = tail_->pre;
    node->pre->next = tail_;
    tail_->pre = node->pre;
    map_.erase(node->val);
    node->pre = nullptr;
    node->next = nullptr;
    map_.erase(node->val);
    value = node->val;
    return true;
}

/*
 * Remove value from LRU. If removal is successful, return true, otherwise
 * return false
 */
template <typename T> bool LRUReplacer<T>::Erase(const T &value) {
    std::lock_guard<std::mutex> guard(latch_);
    if (!map_.count(value)) return false;
    auto node = map_[value];
    auto pre = node->pre, next = node->next;
    pre->next = next;
    next->pre = pre;
    map_.erase(value);
    node->next = nullptr;
    node->pre = nullptr;
    return true;
}

template <typename T> size_t LRUReplacer<T>::Size() { 
    std::lock_guard<std::mutex> guard(latch_);
    return map_.size(); 
}

template class LRUReplacer<Page *>;
// test only
template class LRUReplacer<int>;

} // namespace cmudb
