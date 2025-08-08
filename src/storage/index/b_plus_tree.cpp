//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// b_plus_tree.cpp
//
// Identification: src/storage/index/b_plus_tree.cpp
//
// Copyright (c) 2015-2025, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "storage/index/b_plus_tree.h"
#include "storage/index/b_plus_tree_debug.h"


namespace bustub {

INDEX_TEMPLATE_ARGUMENTS
BPLUSTREE_TYPE::BPlusTree(std::string name, page_id_t header_page_id, BufferPoolManager *buffer_pool_manager,
                          const KeyComparator &comparator, int leaf_max_size, int internal_max_size)
    : index_name_(std::move(name)),
      bpm_(buffer_pool_manager),
      comparator_(std::move(comparator)),
      leaf_max_size_(leaf_max_size),
      internal_max_size_(internal_max_size),
      header_page_id_(header_page_id) {
  WritePageGuard guard = bpm_->WritePage(header_page_id_);
  auto root_page = guard.AsMut<BPlusTreeHeaderPage>();
  root_page->root_page_id_ = INVALID_PAGE_ID;
}

/**
 * @brief Helper function to decide whether current b+tree is empty
 * @return Returns true if this B+ tree has no keys and values.
 */
INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::IsEmpty() const -> bool { 
  ReadPageGuard guard = bpm_->ReadPage(header_page_id_);
  return guard.As<BPlusTreeHeaderPage>()->root_page_id_ == INVALID_PAGE_ID;
}

/*****************************************************************************
 * SEARCH
 *****************************************************************************/
/**
 * @brief Return the only value that associated with input key
 *
 * This method is used for point query
 *
 * @param key input key
 * @param[out] result vector that stores the only value that associated with input key, if the value exists
 * @return : true means key exists
 */
INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::GetValue(const KeyType &key, std::vector<ValueType> *result) -> bool {
  
  // Declaration of context instance. Using the Context is not necessary but advised.
  Context ctx;

  ReadPageGuard guard = bpm_->ReadPage(header_page_id_);
  auto head_page = guard.As<BPlusTreeHeaderPage>();
  ctx.root_page_id_ = head_page->root_page_id_;
  guard.Drop();

  if (ctx.root_page_id_ == INVALID_PAGE_ID) {
    return false;
  }

  //Internal node
  ctx.read_set_.push_back(bpm_->ReadPage(ctx.root_page_id_));
  auto page = ctx.read_set_.back().As<BPlusTreePage>();
  while (!page->IsLeafPage()) {
    int index = FindKeyBiSearch(page, key);
    if (index == -1) {return false;}
    auto internal_page = static_cast<const InternalPage*>(page);
    page_id_t page_id = internal_page->ValueAt(index);
    ctx.read_set_.push_back(bpm_->ReadPage(page_id));  
    page = ctx.read_set_.back().As<BPlusTreePage>();
    ctx.read_set_.pop_front(); 
  }

  // Now the page is 
  int index = FindKeyBiSearch(page, key);
  if (index == -1) {return false;}

  auto leaf_page = static_cast<const LeafPage*>(page);
  result->push_back(leaf_page->ValueAt(index));

  return true;
}

/*****************************************************************************
 * INSERTION
 *****************************************************************************/
/**
 * @brief A helper function that find the place to insert according to given  key
 *
 * @param key the key to insert
 * @return: return the index to insert 
 */
INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::FindKeyBiSearch(const BPlusTreePage *page, const KeyType &key) -> int {
  int left, right;

  if (page->IsLeafPage()) {
    auto leaf_page = static_cast<const LeafPage*>(page);
    left = 0, right = leaf_page->GetSize() - 1; // [left, right]
    while (left <= right) {
      int mid = (left + right) >> 1;
      if (comparator_(leaf_page->KeyAt(mid), key) == 0) { // nums[mid] == target
        return mid;
      }
      if (comparator_(leaf_page->KeyAt(mid), key) < 0) { // nums[mid] < target
        left = mid + 1;
      }
      else {
        right = mid - 1;
      }
    }
  }

  auto internal_page = static_cast<const InternalPage *>(page);
  
  if (comparator_(internal_page->KeyAt(1), key) > 0) {  //nums[1] > target, which means go to next level
    return 0;
  }

  int size = internal_page->GetSize();
  left = 1, right = size - 1; // left = 1 is bc first key is invalid of a internal_page
  while (left <= right) {
    int mid = (left + right) >> 1;
    if (comparator_(internal_page->KeyAt(mid), key) <= 0) {
      // key >= key[mid]，key < key[mid+1]
      if (mid + 1 == size || comparator_(key, internal_page->KeyAt(mid + 1)) < 0) {
        return mid;
      }
      left = mid + 1;
    }
    else {
      right = mid - 1;
    }
  }
  return -1;
}

// INDEX_TEMPLATE_ARGUMENTS
// auto BPLUSTREE_TYPE::LeafIndexBiSearch(const BPlusTreePage* page, const KeyType &key) -> int {
//   int left, right = 0, bpm_->GetSize() - 1;
// }

/**
 * @brief Insert constant key & value pair into b+ tree
 *
 * if current tree is empty, start new tree, update root page id and insert
 * entry, otherwise insert into leaf page.
 *
 * @param key the key to insert
 * @param value the value associated with key
 * @return: since we only support unique key, if user try to insert duplicate
 * keys return false, otherwise return true.
 */
INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::Insert(const KeyType &key, const ValueType &value) -> bool {
  UNIMPLEMENTED("TODO(P2): Add implementation.");
  // Declaration of context instance. Using the Context is not necessary but advised.
  Context ctx;
}

/*****************************************************************************
 * REMOVE
 *****************************************************************************/
/**
 * @brief Delete key & value pair associated with input key
 * If current tree is empty, return immediately.
 * If not, User needs to first find the right leaf page as deletion target, then
 * delete entry from leaf page. Remember to deal with redistribute or merge if
 * necessary.
 *
 * @param key input key
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::Remove(const KeyType &key) {
  // Declaration of context instance.
  Context ctx;
  UNIMPLEMENTED("TODO(P2): Add implementation.");
}

/*****************************************************************************
 * INDEX ITERATOR
 *****************************************************************************/
/**
 * @brief Input parameter is void, find the leftmost leaf page first, then construct
 * index iterator
 *
 * You may want to implement this while implementing Task #3.
 *
 * @return : index iterator
 */
INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::Begin() -> INDEXITERATOR_TYPE { UNIMPLEMENTED("TODO(P2): Add implementation."); }

/**
 * @brief Input parameter is low key, find the leaf page that contains the input key
 * first, then construct index iterator
 * @return : index iterator
 */
INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::Begin(const KeyType &key) -> INDEXITERATOR_TYPE { UNIMPLEMENTED("TODO(P2): Add implementation."); }

/**
 * @brief Input parameter is void, construct an index iterator representing the end
 * of the key/value pair in the leaf node
 * @return : index iterator
 */
INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::End() -> INDEXITERATOR_TYPE { UNIMPLEMENTED("TODO(P2): Add implementation."); }

/**
 * @return Page id of the root of this tree
 *
 * You may want to implement this while implementing Task #3.
 */
INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::GetRootPageId() -> page_id_t {
  auto guard = bpm_->ReadPage(header_page_id_);
  auto header_page = guard.As<BPlusTreeHeaderPage>();
  return header_page->root_page_id_;
}

template class BPlusTree<GenericKey<4>, RID, GenericComparator<4>>;

template class BPlusTree<GenericKey<8>, RID, GenericComparator<8>>;

template class BPlusTree<GenericKey<16>, RID, GenericComparator<16>>;

template class BPlusTree<GenericKey<32>, RID, GenericComparator<32>>;

template class BPlusTree<GenericKey<64>, RID, GenericComparator<64>>;

}  // namespace bustub
