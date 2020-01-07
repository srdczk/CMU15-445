/**
 * b_plus_tree.cpp
 */
#include <iostream>
#include <string>

#include "common/exception.h"
#include "common/logger.h"
#include "common/rid.h"
#include "index/b_plus_tree.h"
#include "page/header_page.h"

namespace cmudb {

INDEX_TEMPLATE_ARGUMENTS
BPLUSTREE_TYPE::BPlusTree(const std::string &name,
                                BufferPoolManager *buffer_pool_manager,
                                const KeyComparator &comparator,
                                page_id_t root_page_id)
    : index_name_(name), root_page_id_(root_page_id),
      buffer_pool_manager_(buffer_pool_manager), comparator_(comparator) {}

/*
 * Helper function to decide whether current b+tree is empty
 */
INDEX_TEMPLATE_ARGUMENTS
bool BPLUSTREE_TYPE::IsEmpty() const { return root_page_id_ == INVALID_PAGE_ID; }
/*****************************************************************************
 * SEARCH
 *****************************************************************************/
/*
 * Return the only value that associated with input key
 * This method is used for point query
 * @return : true means key exists
 */
INDEX_TEMPLATE_ARGUMENTS
bool BPLUSTREE_TYPE::GetValue(const KeyType &key,
                              std::vector<ValueType> &result,
                              Transaction *transaction) {
    if (IsEmpty()) return false;
  // 所有 Fetch 的 地方, 都必须 Unpin
  B_PLUS_TREE_LEAF_PAGE_TYPE *leaf = FindLeafPage(key);
  ValueType val;
  if (!leaf->Lookup(key, val, comparator_)) {
    buffer_pool_manager_->UnpinPage(leaf->GetPageId(), false);
    return false;
  } else {
    result.push_back(val);
    buffer_pool_manager_->UnpinPage(leaf->GetPageId(), false);
    return true;
  }
}

/*****************************************************************************
 * INSERTION
 *****************************************************************************/
/*
 * Insert constant key & value pair into b+ tree
 * if current tree is empty, start new tree, update root page id and insert
 * entry, otherwise insert into leaf page.
 * @return: since we only support unique key, if user try to insert duplicate
 * keys return false, otherwise return true.
 */
INDEX_TEMPLATE_ARGUMENTS
bool BPLUSTREE_TYPE::Insert(const KeyType &key, const ValueType &value,
                            Transaction *transaction) {
  if (IsEmpty()) {
    StartNewTree(key, value);
    return true;
  }
  return InsertIntoLeaf(key, value, transaction);
}
/*
 * Insert constant key & value pair into an empty tree
 * User needs to first ask for new page from buffer pool manager(NOTICE: throw
 * an "out of memory" exception if returned value is nullptr), then update b+
 * tree's root page id and insert entry directly into leaf page.
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::StartNewTree(const KeyType &key, const ValueType &value) {
  // 开始 一个 新的树
  page_id_t new_page_id;
  Page *page = buffer_pool_manager_->NewPage(new_page_id);
  if (!page) throw "out of memory";
  // 一开始 是 leafPage ;
  B_PLUS_TREE_LEAF_PAGE_TYPE *root = reinterpret_cast<B_PLUS_TREE_LEAF_PAGE_TYPE *>(page->GetData());
  // update b+ root page id, insert ->
  root->Init(new_page_id, INVALID_PAGE_ID);
//  std::cout << "MAXSIZE:" << root->GetMaxSize() << "\n";
  root_page_id_ = new_page_id;
  UpdateRootPageId(true);
  root->Insert(key, value, comparator_);
//  std::cout << root->GetSize() << "\n";
  // 页面已经被 修改, 释放 并且重写到磁盘上
  buffer_pool_manager_->UnpinPage(new_page_id, true);
}

/*
 * Insert constant key & value pair into leaf page
 * User needs to first find the right leaf page as insertion target, then look
 * through leaf page to see whether insert key exist or not. If exist, return
 * immdiately, otherwise insert entry. Remember to deal with split if necessary.
 * @return: since we only support unique key, if user try to insert duplicate
 * keys return false, otherwise return true.
 */
INDEX_TEMPLATE_ARGUMENTS
bool BPLUSTREE_TYPE::InsertIntoLeaf(const KeyType &key, const ValueType &value,
                                    Transaction *transaction) {
  B_PLUS_TREE_LEAF_PAGE_TYPE *leaf = FindLeafPage(key);
  ValueType val;
  if (leaf->Lookup(key, val, comparator_)) {
    buffer_pool_manager_->UnpinPage(leaf->GetPageId(), false);
    return false;
  }
//  std::cout << leaf->GetMaxSize() << " leafSize" << leaf->GetSize() << "\n";
  leaf->Insert(key, value, comparator_);
  if (leaf->GetSize() == leaf->GetMaxSize()) {
    // 进行分裂, 分裂之后, 重写 <-->
    // 分裂的时候, 重新写到磁盘上;
    B_PLUS_TREE_LEAF_PAGE_TYPE *newNode = Split(leaf);
    // 向 父节点 插入, key
    // newNode 还未进行 更新
    InsertIntoParent(leaf, newNode->KeyAt(0), newNode);
  }
  // 这边 修改,并且返回磁盘了
  buffer_pool_manager_->UnpinPage(leaf->GetPageId(), true);
  return true;
}

/*
 * Split input page and return newly created page.
 * Using template N to represent either internal page or leaf page.
 * User needs to first ask for new page from buffer pool manager(NOTICE: throw
 * an "out of memory" exception if returned value is nullptr), then move half
 * of key & value pairs from input page to newly created page
 */
INDEX_TEMPLATE_ARGUMENTS
template <typename N> N *BPLUSTREE_TYPE::Split(N *node) {
  page_id_t newPageId;
  Page *page = buffer_pool_manager_->NewPage(newPageId);
  N *newPage = reinterpret_cast<N *>(page->GetData());
  newPage->Init(newPageId, node->GetParentPageId());
  node->MoveHalfTo(newPage, buffer_pool_manager_);
  return newPage;
}

/*
 * Insert key & value pair into internal page after split
 * @param   old_node      input page from split() method
 * @param   key
 * @param   new_node      returned page from split() method
 * User needs to first find the parent page of old_node, parent node must be
 * adjusted to take info of new_node into account. Remember to deal with split
 * recursively if necessary.
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::InsertIntoParent(BPlusTreePage *old_node,
                                      const KeyType &key,
                                      BPlusTreePage *new_node,
                                      Transaction *transaction) {
  if (old_node->IsRootPage()) {
    Page *newRoot = buffer_pool_manager_->NewPage(root_page_id_);
    assert(newRoot != nullptr);
    B_PLUS_TREE_INTERNAL_PAGE *root = reinterpret_cast<B_PLUS_TREE_INTERNAL_PAGE *>(newRoot->GetData());
    root->Init(root_page_id_, INVALID_PAGE_ID);
    root->PopulateNewRoot(old_node->GetPageId(), key, new_node->GetPageId());
    old_node->SetParentPageId(root_page_id_);
    new_node->SetParentPageId(root_page_id_);
    UpdateRootPageId();
    buffer_pool_manager_->UnpinPage(old_node->GetPageId(), true);
    buffer_pool_manager_->UnpinPage(newRoot->GetPageId(), true);
    buffer_pool_manager_->UnpinPage(new_node->GetPageId(), true);
    return;
  }
  // 否则 直接插入
  page_id_t parentId = old_node->GetParentPageId();
//  std::cout << "NIMASILE2\n";
  Page *page = buffer_pool_manager_->FetchPage(parentId);
  // 为什么 变成 叶子节点了
  assert(parentId < 4);
  // ?? 为什么 会变成 叶子 节点
  assert(page != nullptr);
  BPlusTreePage *parent = reinterpret_cast<BPlusTreePage *>(page->GetData());
  assert(!parent->IsLeafPage());
  new_node->SetParentPageId(parentId);
  // 修改 pP
  buffer_pool_manager_->UnpinPage(new_node->GetPageId(), true);
  B_PLUS_TREE_INTERNAL_PAGE *pPage = static_cast<B_PLUS_TREE_INTERNAL_PAGE *>(parent);
  pPage->InsertNodeAfter(old_node->GetPageId(), key, new_node->GetPageId());
  // 假如 刚好 == MaxSize 的 时候 <>
  if (parent->GetSize() == parent->GetMaxSize()) {
    B_PLUS_TREE_INTERNAL_PAGE *newNode = Split(pPage);
    // 把 最后 一个 key 插入 parent
    // parent 指针, 表示 <parent>
    InsertIntoParent(pPage, pPage->KeyAt(parent->GetSize() - 1), newNode);
  }
  buffer_pool_manager_->UnpinPage(parentId, true);
}

/*****************************************************************************
 * REMOVE
 *****************************************************************************/
/*
 * Delete key & value pair associated with input key
 * If current tree is empty, return immdiately.
 * If not, User needs to first find the right leaf page as deletion target, then
 * delete entry from leaf page. Remember to deal with redistribute or merge if
 * necessary.
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::Remove(const KeyType &key, Transaction *transaction) {}

/*
 * User needs to first find the sibling of input page. If sibling's size + input
 * page's size > page's max size, then redistribute. Otherwise, merge.
 * Using template N to represent either internal page or leaf page.
 * @return: true means target leaf page should be deleted, false means no
 * deletion happens
 */
INDEX_TEMPLATE_ARGUMENTS
template <typename N>
bool BPLUSTREE_TYPE::CoalesceOrRedistribute(N *node, Transaction *transaction) {
  return false;
}

/*
 * Move all the key & value pairs from one page to its sibling page, and notify
 * buffer pool manager to delete this page. Parent page must be adjusted to
 * take info of deletion into account. Remember to deal with coalesce or
 * redistribute recursively if necessary.
 * Using template N to represent either internal page or leaf page.
 * @param   neighbor_node      sibling page of input "node"
 * @param   node               input from method coalesceOrRedistribute()
 * @param   parent             parent page of input "node"
 * @return  true means parent node should be deleted, false means no deletion
 * happend
 */
INDEX_TEMPLATE_ARGUMENTS
template <typename N>
bool BPLUSTREE_TYPE::Coalesce(
    N *&neighbor_node, N *&node,
    BPlusTreeInternalPage<KeyType, page_id_t, KeyComparator> *&parent,
    int index, Transaction *transaction) {
  return false;
}

/*
 * Redistribute key & value pairs from one page to its sibling page. If index ==
 * 0, move sibling page's first key & value pair into end of input "node",
 * otherwise move sibling page's last key & value pair into head of input
 * "node".
 * Using template N to represent either internal page or leaf page.
 * @param   neighbor_node      sibling page of input "node"
 * @param   node               input from method coalesceOrRedistribute()
 */
INDEX_TEMPLATE_ARGUMENTS
template <typename N>
void BPLUSTREE_TYPE::Redistribute(N *neighbor_node, N *node, int index) {}
/*
 * Update root page if necessary
 * NOTE: size of root page can be less than min size and this method is only
 * called within coalesceOrRedistribute() method
 * case 1: when you delete the last element in root page, but root page still
 * has one last child
 * case 2: when you delete the last element in whole b+ tree
 * @return : true means root page should be deleted, false means no deletion
 * happend
 */
INDEX_TEMPLATE_ARGUMENTS
bool BPLUSTREE_TYPE::AdjustRoot(BPlusTreePage *old_root_node) {
  return false;
}

/*****************************************************************************
 * INDEX ITERATOR
 *****************************************************************************/
/*
 * Input parameter is void, find the leaftmost leaf page first, then construct
 * index iterator
 * @return : index iterator
 */
INDEX_TEMPLATE_ARGUMENTS
INDEXITERATOR_TYPE BPLUSTREE_TYPE::Begin() {
  KeyType k;
  B_PLUS_TREE_LEAF_PAGE_TYPE *leaf = FindLeafPage(k, true);
  return INDEXITERATOR_TYPE(0, leaf, buffer_pool_manager_);
}

/*
 * Input parameter is low key, find the leaf page that contains the input key
 * first, then construct index iterator
 * @return : index iterator
 */
INDEX_TEMPLATE_ARGUMENTS
INDEXITERATOR_TYPE BPLUSTREE_TYPE::Begin(const KeyType &key) {
  B_PLUS_TREE_LEAF_PAGE_TYPE *leaf = FindLeafPage(key);
  if (!leaf) return INDEXITERATOR_TYPE(0, leaf, buffer_pool_manager_);
  int idx = leaf->KeyIndex(key, comparator_);
  return INDEXITERATOR_TYPE(idx, leaf, buffer_pool_manager_);
}

/*****************************************************************************
 * UTILITIES AND DEBUG
 *****************************************************************************/
/*
 * Find leaf page containing particular key, if leftMost flag == true, find
 * the left most leaf page
 */
INDEX_TEMPLATE_ARGUMENTS
B_PLUS_TREE_LEAF_PAGE_TYPE *BPLUSTREE_TYPE::FindLeafPage(const KeyType &key,
                                                         bool leftMost) {
  if (IsEmpty()) return nullptr;
  Page *page = buffer_pool_manager_->FetchPage(root_page_id_);
  BPlusTreePage *bPage = reinterpret_cast<BPlusTreePage *>(page->GetData());
//  std::cout << "ROOT:" << root_page_id_ << " Is:" << bPage->IsLeafPage() << "\n";
  while (!bPage->IsLeafPage()) {
    page_id_t id;
    B_PLUS_TREE_INTERNAL_PAGE *bp = static_cast<B_PLUS_TREE_INTERNAL_PAGE *>(bPage);
    if (leftMost) id = bp->ValueAt(0);
    else id = bp->Lookup(key, comparator_);
    page = buffer_pool_manager_->FetchPage(id);
    buffer_pool_manager_->UnpinPage(bPage->GetPageId(), false);
    bPage = reinterpret_cast<BPlusTreePage *>(page->GetData());
  }
  return static_cast<B_PLUS_TREE_LEAF_PAGE_TYPE *>(bPage);
}

/*
 * Update/Insert root page id in header page(where page_id = 0, header_page is
 * defined under include/page/header_page.h)
 * Call this method everytime root page id is changed.
 * @parameter: insert_record      defualt value is false. When set to true,
 * insert a record <index_name, root_page_id> into header page instead of
 * updating it.
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::UpdateRootPageId(int insert_record) {
  HeaderPage *header_page = static_cast<HeaderPage *>(
      buffer_pool_manager_->FetchPage(HEADER_PAGE_ID));
  if (insert_record)
    // create a new record<index_name + root_page_id> in header_page
    header_page->InsertRecord(index_name_, root_page_id_);
  else
    // update root_page_id in header_page
    header_page->UpdateRecord(index_name_, root_page_id_);
  buffer_pool_manager_->UnpinPage(HEADER_PAGE_ID, true);
}

/*
 * This method is used for debug only
 * print out whole b+tree sturcture, rank by rank
 */
INDEX_TEMPLATE_ARGUMENTS
std::string BPLUSTREE_TYPE::ToString(bool verbose) {
    if (!IsEmpty()) {
        Page *page = buffer_pool_manager_->FetchPage(root_page_id_);
        B_PLUS_TREE_INTERNAL_PAGE *root = reinterpret_cast<B_PLUS_TREE_INTERNAL_PAGE *>(page->GetData());
        std::cout << root->ToString(verbose) << "\n";
        for (int i = 0; i < root->GetSize(); ++i) {
            Page *child = buffer_pool_manager_->FetchPage(root->ValueAt(i));
            B_PLUS_TREE_LEAF_PAGE_TYPE *childNode = reinterpret_cast<B_PLUS_TREE_LEAF_PAGE_TYPE *>(child->GetData());
            std::cout << childNode->ToString(verbose) << "<==>";
        }
        return "";
    } else { return "Empty"; }
}

/*
 * This method is used for test only
 * Read data from file and insert one by one
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::InsertFromFile(const std::string &file_name,
                                    Transaction *transaction) {
  int64_t key;
  std::ifstream input(file_name);
  while (input) {
    input >> key;

    KeyType index_key;
    index_key.SetFromInteger(key);
    RID rid(key);
    Insert(index_key, rid, transaction);
  }
}
/*
 * This method is used for test only
 * Read data from file and remove one by one
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::RemoveFromFile(const std::string &file_name,
                                    Transaction *transaction) {
  int64_t key;
  std::ifstream input(file_name);
  while (input) {
    input >> key;
    KeyType index_key;
    index_key.SetFromInteger(key);
    Remove(index_key, transaction);
  }
}

template class BPlusTree<GenericKey<4>, RID, GenericComparator<4>>;
template class BPlusTree<GenericKey<8>, RID, GenericComparator<8>>;
template class BPlusTree<GenericKey<16>, RID, GenericComparator<16>>;
template class BPlusTree<GenericKey<32>, RID, GenericComparator<32>>;
template class BPlusTree<GenericKey<64>, RID, GenericComparator<64>>;

} // namespace cmudb
