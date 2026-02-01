//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// index_iterator.cpp
//
// Identification: src/storage/index/index_iterator.cpp
//
// Copyright (c) 2015-2025, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

/**
 * index_iterator.cpp
 */
#include <cassert>

#include "storage/index/index_iterator.h"

namespace bustub {

/**
 * @note you can change the destructor/constructor method here
 * set your own input parameters
 */
INDEX_TEMPLATE_ARGUMENTS
INDEXITERATOR_TYPE::IndexIterator(BufferPoolManager *bpm, page_id_t page_id, int index)
    : bpm_(bpm), current_page_id_(page_id), cur_index_(index) {
  if (page_id == INVALID_PAGE_ID) {
    // end iterator
    return;
  }
  auto page_guard = bpm_->ReadPage(current_page_id_);
  auto leaf_page = page_guard.As<B_PLUS_TREE_LEAF_PAGE_TYPE>();
  target_.first = leaf_page->KeyAt(cur_index_);
  target_.second = leaf_page->ValueAt(cur_index_);
};

INDEX_TEMPLATE_ARGUMENTS
INDEXITERATOR_TYPE::~IndexIterator() = default;  // NOLINT

INDEX_TEMPLATE_ARGUMENTS
auto INDEXITERATOR_TYPE::IsEnd() -> bool { return cur_index_ == -1; }

INDEX_TEMPLATE_ARGUMENTS
auto INDEXITERATOR_TYPE::operator*() -> std::pair<const KeyType &, const ValueType &> {
  return target_;
}

INDEX_TEMPLATE_ARGUMENTS
auto INDEXITERATOR_TYPE::operator++() -> INDEXITERATOR_TYPE & {
  auto page_guard = bpm_->ReadPage(current_page_id_);
  auto leaf_page = page_guard.As<B_PLUS_TREE_LEAF_PAGE_TYPE>();
  cur_index_++;
  if (cur_index_ >= leaf_page->GetSize()) {
    // need to go to next page
    page_id_t next_page_id = leaf_page->GetNextPageId();
    if (next_page_id == INVALID_PAGE_ID) {
      // end iterator
      current_page_id_ = INVALID_PAGE_ID;
      cur_index_ = -1;
      return *this;
    }
    // move to next page
    current_page_id_ = next_page_id;
    page_guard = bpm_->ReadPage(current_page_id_);  
    leaf_page = page_guard.As<B_PLUS_TREE_LEAF_PAGE_TYPE>();
    cur_index_ = 0;
  }
  target_.first = leaf_page->KeyAt(cur_index_);
  target_.second = leaf_page->ValueAt(cur_index_);
  return *this;
}

template class IndexIterator<GenericKey<4>, RID, GenericComparator<4>>;

template class IndexIterator<GenericKey<8>, RID, GenericComparator<8>>;

template class IndexIterator<GenericKey<16>, RID, GenericComparator<16>>;

template class IndexIterator<GenericKey<32>, RID, GenericComparator<32>>;

template class IndexIterator<GenericKey<64>, RID, GenericComparator<64>>;

}  // namespace bustub
