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
 * @brief A helper function that find the place to insert according to given  key
 *
 * @param key the key to insert
 * @return: return the index to insert 
 */
INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::FindKeyBiSearch(const LeafPage *page, const KeyType &key) -> int {
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
      // key >= arr[mid]，key < arr[mid+1]
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
    auto internal_page = static_cast<const InternalPage*>(page);  // Not sure if the static_cast is necessary 
    page_id_t page_id = internal_page->ValueAt(index);
    ctx.read_set_.push_back(bpm_->ReadPage(page_id));  
    page = ctx.read_set_.back().As<BPlusTreePage>();
    ctx.read_set_.pop_front(); // Release the lock of the parent of the current page 
  }

  // Now the page is leaf page
  int index = FindKeyBiSearch(page, key);
  if (index == -1) {return false;}

  auto leaf_page = static_cast<const LeafPage*>(page);
  result->push_back(leaf_page->ValueAt(index));

  return true;
}

/*****************************************************************************
 * INSERTION
 *****************************************************************************/


INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::LeafIndexToInsert(const LeafPage* page, const KeyType &key) -> int {
  int left = 0;
  int right = page->GetSize() - 1;
  int size = page->GetSize();
  
  if (comparator_(key, page->KeyAt(left)) <= 0) { // If the key is smaller than the leftmost key in the page, we could just insert it at index 0
    return 0;
  }

  while (left <= right) {
    int mid = (left + right) >> 1;
    if (comparator_(page->KeyAt(mid), key) < 0) { // if arr[mid] < key, left = mid + 1
      left = mid + 1;
    }
    else {
      right = mid - 1;
    }
  }
  return left; 
}

INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::ShiftRightByOne(LeafPage* leaf_page, int insertIndex) -> void{
  int size = leaf_page->GetSize();
  for (int i = size; i < insertIndex; i--) {
    leaf_page->SetKey(i, leaf_page->KeyAt(i - 1));
    leaf_page->SetValueAt(i, leaf_page->ValueAt(i - 1));
  }
}

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
  // Declaration of context instance. Using the Context is not necessary but advised.
  Context ctx;

  // WritePageGuard head_guard = bpm_->WritePage(header_page_id_);
  // ctx.header_page_ = std::make_optional(std::move(head_guard));
  ctx.header_page_.emplace(bpm_->WritePage(header_page_id_));
  ctx.root_page_id_ = ctx.header_page_->As<BPlusTreeHeaderPage>()->root_page_id_;

  // Case 1: Tree is empty
  if (ctx.root_page_id_ == INVALID_PAGE_ID) {
    page_id_t root_page_id = bpm_->NewPage();
    WritePageGuard root_guard = bpm_->WritePage(root_page_id);
    auto root_page = root_guard.AsMut<LeafPage>();

    root_page.Init();
    root_page->SetKeyAt(0, key);
    root_page->SetValueAt(0, value);
    root_page->SetSize(1);

    return true;
  }

  // 2) Read root
  // auto header_page = ctx.header_page_;
  // page_id_t root_id = ctx.root_page_id_;
  ctx.read_set_.push_back(bpm_->ReadPage(ctx.root_page_id_));
  auto page = ctx.read_set_.back().As<BPlusTreePage>();

  // 2.1) root is a leaf node.
  if (page->IsLeafPage()) {
    ctx.read_set_.pop_back();
    ctx.write_set_.push_back(bpm_->WritePage(root_id));  
    auto leaf = ctx.write_set_.back().AsMut<BPlusTreePage>();
    ctx.header_page_ = std::nullopt; // release header guard lock
    // Insert and split to be implemented 

    // If leaf is full, not handle the case here yet
    if (leaf->GetSize() >= leaf->GetMaxSize()) {return false;}

    const int pos = LeafIndexToInsert(leaf, key);
    if (pos < leaf->GetSize() && comparator_(leaf->KeyAt(pos), key) == 0) return false; // Remove duplicates
    ShiftRightByOne(leaf, pos);
    leaf->SetKeyAt(pos, key);
    leaf->SetValueAt(pos, value);
    leaf->SetSize(leaf->GetSize() + 1);

    return true;
  }

  // 2.2) root is internal
  // page_id_t page_id = ctx.root_page_id_;
  // ctx.read_set_.push_back(bpm_->ReadPage(page_id));
  // auto page = ctx.read_set_.back().As<BPlusTreePage>();

  while (!page->IsLeafPage()) {
    auto parent = static_cast<const InternalPage*>(page);
    int index = FindKeyBiSearch(page, key);
    if (index == -1) {return false;}
    page_id_t child_id = parent->ValueAt(index);

    ctx.read_set_.push_back(bpm_->ReadPage(child_id));
    auto child = ctx.read_set_.back().As<BPlusTree>();

    // If the next-level page is leaf page, we need to upgrade the child read-lock to write-lock 
    if (child->IsLeafPage()) {
      ctx.read_set_.pop_back(); 
      ctx.write_set_.push_back(bpm_->WritePage(child_id)); // lock child 
      auto leaf = ctx.write_set_.back().AsMut<LeafPage>();

      // 父读锁可以释放了（无 split 场景下后续不再用父）
      ctx.read_set_.pop_back(); // 释放上一层 parent 的读锁

      if (leaf->GetSize() >= leaf->GetMaxSize()) return false; // 交给 split 路径

      const int pos = LeafIndexToInsert(leaf, key);
      if (pos < leaf->GetSize() && comparator_(leaf->KeyAt(pos), key) == 0) return false; // Remove duplicates
      ShiftRightByOne(leaf, pos);
      leaf->SetKeyAt(pos, key);
      leaf->SetValueAt(pos, value);
      leaf->SetSize(leaf->GetSize() + 1);
      return true;
    }

    
  }

  // // 2.1 When the size is smaller than the capacity--split is not required
  // auto write_page = page;
  // if (write_page->GetSize() < write_page->GetMaxSize()) {  
  //   auto leaf_page = static_cast<LeafPage*>(write_page);
  //   int insert_index = LeafIndexToInsert(leaf_page, key);

  //   // if (insert_index == -1) {return false;}

  //   // If we already have the key in our tree--no need to insert, return immediately
  //   if (comparator_(leaf_page->KeyAt(insert_index), key) == 0) {return false;}

  //   int size = leaf_page->GetSize();
  //   for (int i = size; i > insert_index; i--) { // Shift all elements in the leaf node to the right by 1
  //     leaf_page->SetKeyAt(i, leaf_page->KeyAt(i - 1));
  //     leaf_page->SetValueAt(i, leaf_page->KeyAt(i - 1));
  //   }
  //   leaf_page->SetKeyAt(i, key);
  //   leaf_page->SetValueAt(i, value);

  //   leaf_page->SetSize(size + 1);
  //   return true;
  // }

  // // 
  // WritePageGuard write_root_guard = bpm_->WritePage(ctx.root_page_id_);
  // // auto page = write_page_guard.AsMut<BPlusTreePage>();
  // ctx.write_set_.push_back(std::move(write_page_guard));  // .emplace_back(write_page_guard) might be better 
  // if (page->GetSize() < page->GetMaxSize()) { // Parent is safe, , release its write lock/guard
  //   ctx.header_page_ = std::nullopt;
  // }

  // while (!page->IsLeafPage()) {
  //   int index = FindKeyBiSearch(page, key);
  //   if (index == -1) {return false;}
  //   auto internal_page = static_cast<InternalPage*>(page);
  //   page_id_t page_id = InternalPage->ValueAt(index);
  //   ctx.write_set_.push_back(bpm_->WritePage(page_id));
  //   page = ctx.write_set_.back().AsMut<BPlusTreePage>();

  //   if (page->GetSize() < page->GetMaxSize()) {  // Split is not required, release w-latch of its parent
  //     if (ctx.header_page_.has_value()) {
  //       ctx.header_page_ = std::nullopt;
  //     }
  //     while (ctx.write_set_.size() > 1) {
  //       ctx.write_set_.pop_front();
  //     }
  //   }
  // }

  // auto leaf_page = static_cast<LeafPage*>(page);
  // int insert_index = LeafIndexToInsert(leaf_page, key);
  // // If we already have the key--no need to insert
  // if (comparator_(leaf_page->KeyAt(insert_index), key) == 0) {return false;}

  // if (leaf_page->GetSize() < leaf_page->GetMaxSize()) {
  //   int size = leaf_page->GetSize();
  //   for (int i = size; i < insert_index; i--) {
  //     leaf_page->SetKeyAt(i, leaf_page->KeyAt(i - 1));
  //     leaf_page->SetValueAt(i, leaf_page->ValueAt(i - 1));
  //   }
  //   leaf_page->SetKeyAt(insert_index, key);
  //   leaf_page->SetValueAt(insert_index, value);
  //   leaf_page->SetSize(size + 1);
  //   ctx.write_set_.pop_front();
  //   return true;
  // }
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
