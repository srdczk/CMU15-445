/**
 * index_iterator.h
 * For range scan of b+ tree
 */
#pragma once
#include "page/b_plus_tree_leaf_page.h"

namespace cmudb {

#define INDEXITERATOR_TYPE                                                     \
  IndexIterator<KeyType, ValueType, KeyComparator>

INDEX_TEMPLATE_ARGUMENTS
class IndexIterator {
public:
  // you may define your own constructor based on your member variables
  IndexIterator(B_PLUS_TREE_LEAF_PAGE_TYPE *leaf, int index, BufferPoolManager *bufferPoolManager);
  ~IndexIterator();

  bool isEnd() {
      return !leaf_ || (index_ >= leaf_->GetSize());
  }

  const MappingType &operator*() {
      return leaf_->GetItem(index_);
  }

  IndexIterator &operator++() {
      ++index_;
      if (index_ >= leaf_->GetSize()) {
          if (leaf_->GetNextPageId() == INVALID_PAGE_ID) leaf_ = nullptr;
          else {
              Page *page = bufferPoolManager_->FetchPage(leaf_->GetNextPageId());
              bufferPoolManager_->UnpinPage(leaf_->GetPageId(), false);
              leaf_ = reinterpret_cast<B_PLUS_TREE_LEAF_PAGE_TYPE *>(page->GetData());
              index_ = 0;
          }
      }
      return *this;
  }

private:
    B_PLUS_TREE_LEAF_PAGE_TYPE *leaf_;
    int index_;
    BufferPoolManager *bufferPoolManager_;
};

} // namespace cmudb
