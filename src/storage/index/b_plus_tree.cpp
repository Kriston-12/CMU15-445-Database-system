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
      // key >= arr[mid] or key < arr[mid+1]
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

// 返回 key 槽位 j ∈ [1..C]，lower_bound(x) on key[1..C-1]
// j == C 表示“插到最后一个 key 的后面”
INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::KeySlotLowerBound(const InternalPage* p, const KeyType &x) -> int {
  const int c = p->GetSize();
  if (c <= 1) return 1; //没有key,默认从1开始插入
  int l = 1, r = c - 1, ans = c;
  while (l <= r) {
    int m = (l + r) >> 1;
    if (comparator_(p->KeyAt(m), x) >= 0) { // key[m] >= x
      ans = m, r = m - 1;
    }
    else {
      l = m + 1;
    }
  }
  return ans;
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
  // for (int i = size; i < insertIndex; i--) {
  //   leaf_page->SetKey(i, leaf_page->KeyAt(i - 1));
  //   leaf_page->SetValueAt(i, leaf_page->ValueAt(i - 1));
  // }
  std::memmove(leaf_page->key_array_ + insertIndex + 1, leaf_page->key_array_ + insertIndex, sizeof(KeyType) * (size - insertIndex));
  std::memmove(leaf_page->rid_array_ + insertIndex + 1, leaf_page->rid_array_ + insertIndex, sizeof(ValueType) * (size - insertIndex));
}

INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::ShiftLeftByOne(LeafPage* leaf_page, int removeIndex) -> void {
  int size = leaf_page->GetSize();
  std::memmove(leaf_page->key_array_ + removeIndex, leaf_page->key_array_ + removeIndex + 1, sizeof(KeyType) * (size - removeIndex - 1));
  std::memmove(leaf_page->rid_array_ + removeIndex, leaf_page->rid_array_ + removeIndex + 1, sizeof(ValueType) * (size - removeIndex - 1));
}


// 左边只用修改size无需修改Key/ValueArray
INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::SplitLeaf(LeafPage* left, const KeyType& key, const ValueType& value, LeafPage* right, page_id_t right_id, KeyType& sep_key) -> void {
  const int n = left->GetSize();
  const int left_sz = (n + 2) / 2; // total = n + 1, left_size = (total + 1) / 2 = (n + 2) / 2
  const int right_sz = n + 1 - left_sz;

  // 1)找插入位置
  int pos = LeafIndexToInsert(left, key);
  
  if (pos < left_sz) { // 新key留在左页
    // -- 新建留在左页
    // 右页应得到: 原left 的 [left_sz - 1, n - 1] 共 right_sz - 1 个 + 我们从插入后左页有 left_sz 个
    // 先把左页的右半 (不含将要插入的位置左侧元素) 整体帮到右页
    // 注意: 插入后左页大小是  left_sz, 因此从原数组抽取区间 [left_sz - 1, n - 1] 共 (n - (left_sz - 1)) = right_sz 个元素
    // 但由于新建在左页，我们搬到右页的其实是left page中右半的 right_sz 个 “old element"
    // right page的目标区间 [0, right_sz - 1]
    std::memmove(right->key_array_, left->key_array_ + (left_sz - 1), sizeof(KeyType) * right_sz);
    std::memmove(right->rid_array_, left->rid_array_ + (left_sz - 1), sizeof(ValueType) * right_sz);

    right->SetSize(right_sz);

    // 在左页内，为pos挪出空位，把 [pos, left_sz - 2]向右移动一格
    std::memmove(left->key_array_ + pos + 1, left->key_array_ + pos, sizeof(KeyType) * (left_sz - pos - 1));
    std::memmove(left->rid_array_ + pos + 1, left->rid_array_ + pos, sizeof(ValueType) * (left_sz - pos - 1));

    // 写入新建到左页
    left->SetKeyAt(pos, key);
    left->SetValueAt(pos, value);
    left->SetSize(left_sz);
  }
  else { // 新key留在右页，左页只需SetSize
    const int move_cnt = n - left_sz;
    std::memmove(right->key_array_, left->key_array_ + left_sz, sizeof(KeyType) * move_cnt);
    std::memmove(right->rid_array_, left->rid_array_ + left_sz, sizeof(ValueType) * move_cnt);

    left->SetSize(left_sz);

    // 新key在right leaf中的位置
    const int rpos = pos - left_sz;

    if (rpos < move_cnt) {
      std::memmove(right->key_array_ + rpos + 1, right->key_array_ + rpos, sizeof(KeyType) * (move_cnt - rpos));
      std::memmove(right->rid_array_ + rpos + 1, right->rid_array_ + rpos, sizeof(ValueType) * (move_cnt - rpos));
    }

    // 写入NewKey到右边
    right->SetKeyAt(rpos, key);
    right->SetValueAt(rpos, value);
    right->SetSize(right_sz);
  }

  sep_key = right->KeyAt(0);
  // 维护链表指针：right->SetNextPageId(left->GetNextPageId()); left->SetNextPageId(right_id);
  // std::cout << left->GetNextPageId() << std::endl;
  // std::cout << right->GetNextPageId() << std::endl;
  right->SetNextPageId(left->GetNextPageId()); left->SetNextPageId(right_id);
  // std::cout << left->GetNextPageId() << std::endl;
  // std::cout << right->GetNextPageId() << std::endl;
}

INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::SplitInternal(InternalPage* left, KeyType& sep_key, InternalPage* right, page_id_t new_child_page_id, KeyType& up) -> void {
  const int n = left->GetSize();
  int mid = (n + 1) / 2;
  up = left->KeyAt(mid);

  // int pos = FindKeyBiSearch(left, sep_key); 

  // 右页接收 mid+1..n-1 的 keys，共 right_keys = n - mid - 1
  int right_keys = n - mid - 1;

  if (right_keys > 0) {
    // internal_page的key_array的第一个element是invalid
    std::memmove(right->key_array_ + 1, left->key_array_ + (mid + 1), sizeof(KeyType) * right_keys);
  }

  // Unlike that for key_array, the first parameter below does not have "+1", and we are moving "right_keys + 1" elements
  std::memmove(right->page_id_array_, left->page_id_array_ + mid, sizeof(page_id_t) * (right_keys + 1));
  right->SetSize(right_keys + 1);
  left->SetSize(mid); // up key由于在 index = mid位置，会被自动忽略掉，因为left现在的size变成了mid，最后一个index是mid - 1

  //决定将 sep_key, new_child插入到左边或者右边 
  // sep < up, 左边，else 右边
  if (comparator_(sep_key, up) < 0) {
    // insert to the left
    int pos = KeySlotLowerBound(left, sep_key);

    int nleft = mid;
    std::memmove(left->key_array_ + (pos + 1), left->key_array_ + pos, sizeof(KeyType) * (nleft - pos));
    std::memmove(left->page_id_array_ + (pos + 1), left->page_id_array_ + pos, sizeof(page_id_t) * (nleft - pos));
    left->SetKeyAt(pos, sep_key);
    left->page_id_array_[pos] = new_child_page_id;
    left->SetSize(nleft + 1);
  }
  else {
    int pos = KeySlotLowerBound(right, sep_key);
    int nright = right->GetSize();

    std::memmove(right->key_array_ + pos + 1, right->key_array_ + pos, sizeof(KeyType) * (nright - pos));
    std::memmove(right->page_id_array_ + pos + 1, right->page_id_array_ + pos, sizeof(page_id_t) * (nright - pos));
    right->SetKeyAt(pos, sep_key);
    right->page_id_array_[pos] = new_child_page_id;
    right->SetSize(nright + 1);
  }

  // （可选）把左页后半的残留键/指针清零，便于调试
}


INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::InsertIntoInternal(InternalPage* parent, KeyType& sep_key, page_id_t new_child_page_id) -> void {
  int pos = KeySlotLowerBound(parent, sep_key);
  // std::cout << "InsertIntoInternal " << pos << std::endl;
  int n = parent->GetSize();

  // keys: [pos..n-1] → [pos+1..n]
  std::memmove(parent->key_array_ + pos + 1, parent->key_array_ + pos, sizeof(KeyType) * (n - pos));
  std::memmove(parent->page_id_array_ + pos + 1, parent->page_id_array_ + pos, sizeof(page_id_t) * (n - pos));
  parent->SetKeyAt(pos, sep_key);
  parent->page_id_array_[pos] = new_child_page_id;
  parent->SetSize(n + 1);
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
  // return InsertUpDown(key, value);
  return InsertPreemptive(key, value);
}


INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::InsertUpDown(const KeyType &key, const ValueType &value) -> bool {
  // Declaration of context instance. Using the Context is not necessary but advised.
  Context ctx;

  // 1) acquire header write-lock, read root_page_id （只在空树 or root=leaf 的情况下长期持有header-write-lock)
  ctx.header_page_.emplace(bpm_->WritePage(header_page_id_));
  ctx.root_page_id_ = ctx.header_page_->As<BPlusTreeHeaderPage>()->root_page_id_;

  // Case 1: Tree is empty
  if (ctx.root_page_id_ == INVALID_PAGE_ID) {
    page_id_t root_page_id = bpm_->NewPage();
    WritePageGuard root_guard = bpm_->WritePage(root_page_id);
    auto root_page = root_guard.AsMut<LeafPage>();

    root_page->Init(leaf_max_size_);
    root_page->SetKeyAt(0, key);
    root_page->SetValueAt(0, value);
    root_page->SetSize(1);

    ctx.header_page_->AsMut<BPlusTreeHeaderPage>()->root_page_id_ = root_page_id;  // update global header page
    return true;
  }

  // 2) Non-empty tree, traverse from root to leaf using read-latch
  ctx.read_set_.push_back(bpm_->ReadPage(ctx.root_page_id_));
  auto page = ctx.read_set_.back().As<BPlusTreePage>();

  if (page->IsLeafPage()) {
    ctx.read_set_.pop_back();
    ctx.write_set_.push_back(bpm_->WritePage(ctx.root_page_id_));
    auto leaf = ctx.write_set_.back().AsMut<LeafPage>();

    int pos = LeafIndexToInsert(leaf, key);
    if (pos < leaf->GetSize() && comparator_(leaf->KeyAt(pos), key) == 0) {
      return false;
    }

    if (leaf->GetSize() < leaf->GetMaxSize()) {
      ShiftRightByOne(leaf, pos);
      leaf->SetKeyAt(pos, key);
      leaf->SetValueAt(pos, value);
      leaf->SetSize(leaf->GetSize() + 1);
      return true;
    }

    // split root
    KeyType sep{};
    auto right_leaf_id = bpm_->NewPage();
    auto right_leaf_Guard = bpm_->WritePage(right_leaf_id);
    auto right_leaf = right_leaf_Guard.AsMut<LeafPage>();

    right_leaf->Init(leaf_max_size_);
    SplitLeaf(leaf, key, value, right_leaf, right_leaf_id, sep);

    page_id_t new_root_id = bpm_->NewPage();
    auto new_root_guard = bpm_->WritePage(new_root_id);
    auto new_root = new_root_guard.AsMut<InternalPage>();

    new_root->Init(internal_max_size_);
    new_root->SetSize(2);
    new_root->SetKeyAt(1, sep);
    new_root->SetValueAt(0, ctx.write_set_.back().GetPageId());
    new_root->SetValueAt(1, right_leaf_id);

    ctx.header_page_->AsMut<BPlusTreeHeaderPage>()->root_page_id_ = new_root_id;
    return true;
  }

  // 2.2 root is internal; 后续不改 root, 先释放header 降低争用
  ctx.header_page_ = std::nullopt;

  // 2.3 自顶向下定位叶子 (readlatch) ; "释放规则": 只要 parent 不满可以释放祖先
  std::vector<page_id_t> path;
  path.push_back(ctx.root_page_id_);

  while (!page->IsLeafPage()) {
    auto parent = static_cast<const InternalPage *>(page);
    int idx = FindKeyBiSearch(page, key);
    page_id_t cid = parent->ValueAt(idx);

    ctx.read_set_.push_back(bpm_->ReadPage(cid));
    page = ctx.read_set_.back().As<BPlusTreePage>();
    path.push_back(cid);

    // lab crabbing：子页“安全”可以释放更高的祖先读锁
    if (parent->GetSize() < parent->GetMaxSize() && ctx.read_set_.size() > 2) {
      ctx.read_set_.pop_front();
    }
  }

  // 2.4 到达leaf:升级leaf write-latch
  page_id_t leaf_id = path.back();
  ctx.read_set_.pop_back();
  ctx.write_set_.push_back(bpm_->WritePage(leaf_id));
  auto leaf = ctx.write_set_.back().AsMut<LeafPage>();

  int pos = LeafIndexToInsert(leaf, key);
  if (pos < leaf->GetSize() && comparator_(leaf->KeyAt(pos), key) == 0) {
    return false;
  }
  if (leaf->GetSize() < leaf->GetMaxSize()) {
    ShiftRightByOne(leaf, pos);
    leaf->SetKeyAt(pos, key);
    leaf->SetValueAt(pos, value);
    leaf->SetSize(leaf->GetSize() + 1);
    return true;
  }

  // 2.5 叶子已经满了: 叶子分裂并准备向上插入分隔键
  KeyType sep{};
  auto right_page_id = bpm_->NewPage();
  auto new_leaf_guard = bpm_->WritePage(right_page_id);
  auto new_leaf = new_leaf_guard.AsMut<LeafPage>();
  new_leaf->Init(leaf_max_size_);
  SplitLeaf(leaf, key, value, new_leaf, right_page_id, sep);
  page_id_t new_child_id = new_leaf_guard.GetPageId();

  // from bottom to top: 将(sep, new_child_id) 插入parent
  while (true) {
    if (path.size() == 1) {
      ctx.read_set_.clear();
      ctx.write_set_.clear();
      // ctx释放了所有的读写锁并且还没有获取到下面的header锁的这个过程中，别的thread可能会对root进行修改
      // 如果其他requests将b+ tree删除到只剩下root为leaf, 那么下面的InsertIntoInternal会出错，因为root不是internal page了
      if (!ctx.header_page_.has_value()) { // 这里可能发生。 如果header有value，那么是之前循环中parent为root并且sep需要被insert到parent中，所以获取了锁
        ctx.header_page_.emplace(bpm_->WritePage(header_page_id_));
      }

      ctx.root_page_id_ = ctx.header_page_->As<BPlusTreeHeaderPage>()->root_page_id_;

      auto root_guard = bpm_->WritePage(ctx.root_page_id_);
      auto root = root_guard.AsMut<InternalPage>();
      // int ins = FindKeyBiSearch(root, sep) + 1; // ins是insert的index + 1的位置
      if (root->GetSize() < root->GetMaxSize()) {
        InsertIntoInternal(root, sep, new_child_id);
        return true;
      }

      // root 满 -> 拆 root (先插再分)，再造新 root
      KeyType up{};
      auto right_guard = bpm_->WritePage(bpm_->NewPage());
      auto right_page = right_guard.AsMut<InternalPage>();
      right_page->Init(internal_max_size_);
      SplitInternal(root, sep, right_page, new_child_id, up);

      page_id_t new_root_id = bpm_->NewPage();
      auto new_root_guard = bpm_->WritePage(new_root_id);
      auto new_root = new_root_guard.AsMut<InternalPage>();
      new_root->Init(internal_max_size_);
      new_root->SetSize(2);
      new_root->SetKeyAt(1, up);
      new_root->SetValueAt(0, ctx.root_page_id_);
      new_root->SetValueAt(1, right_guard.GetPageId());
      ctx.header_page_->AsMut<BPlusTreeHeaderPage>()->root_page_id_ = new_root_id;

      return true;
    }

    // root非leaf，自下而上分裂
    page_id_t parent_id = path[path.size() - 2];  // path[path.size() - 1] 是child，已经被升级wlatch
    if (ctx.read_set_.back().GetPageId() == ctx.root_page_id_) {
      // 先放掉在身上的页锁, 不然此时获取到了header锁的thread在等待root的释放，没有这两行那么这个thread就会持续等待root，然后下面代码持续等待header，就会卡死
      ctx.read_set_.clear();
      ctx.write_set_.clear();
      if (!ctx.header_page_.has_value()) {
        ctx.header_page_.emplace(bpm_->WritePage(header_page_id_));
      }
      ctx.root_page_id_ = ctx.header_page_->As<BPlusTreeHeaderPage>()->root_page_id_;
    }
    
    if (!ctx.read_set_.empty()) {ctx.read_set_.pop_back();}; 
    ctx.write_set_.push_back(bpm_->WritePage(parent_id));
    auto parent = ctx.write_set_.back().AsMut<InternalPage>();

    if (parent->GetSize() <= parent->GetMaxSize()) {
      InsertIntoInternal(parent, sep, new_child_id);
      return true;
    }

    // parent 满了，也需分裂，得到上推键up和new right brother
    KeyType up{};
    auto new_internal_guard = bpm_->WritePage(bpm_->NewPage());
    auto new_internal = new_internal_guard.AsMut<InternalPage>();
    new_internal->Init(internal_max_size_);
    SplitInternal(parent, sep, new_internal, new_child_id, up); 

    if (parent_id == ctx.root_page_id_) {
      ctx.read_set_.clear();
      ctx.write_set_.clear();
      if (!ctx.header_page_.has_value()) {
        ctx.header_page_.emplace(bpm_->WritePage(header_page_id_));
      }
      ctx.root_page_id_ = ctx.header_page_->As<BPlusTreeHeaderPage>()->root_page_id_;
      auto new_root_id = bpm_->NewPage();
      auto new_root_guard = bpm_->WritePage(new_root_id);
      auto new_root_page = new_root_guard.AsMut<InternalPage>();
      new_root_page->Init(internal_max_size_);
      new_root_page->SetSize(2);
      new_root_page->SetKeyAt(1, up);
      new_root_page->SetValueAt(0, parent_id);
      new_root_page->SetValueAt(1, new_internal_guard.GetPageId());

      ctx.header_page_->AsMut<BPlusTreeHeaderPage>()->root_page_id_ = new_root_id;
      return true;
    }

    // 将(up, new_internal_id)向上一层插入
    sep = up;
    new_child_id = new_internal_guard.GetPageId();
    path.pop_back();
    ctx.write_set_.pop_back();
  }

  return true;
}

/*****************************************************************************
 * Below is for the striclty correct insert--preemptive split--in descending process,
 * we split internal nodes on the way down if they are full--keySize == maxSize == 
 * This ensures that when we reach the leaf, the leaf's parent is guaranteed to have space
 * 
 * INSERTION - helper
 *****************************************************************************/
INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::insert_into_empty_tree(Context &ctx, const KeyType &key, const ValueType &value) -> bool {
  page_id_t root_page_id = bpm_->NewPage();
  WritePageGuard root_guard = bpm_->WritePage(root_page_id);
  auto root_page = root_guard.AsMut<LeafPage>();

  root_page->Init(leaf_max_size_);
  root_page->SetKeyAt(0, key);
  root_page->SetValueAt(0, value);
  root_page->SetSize(1);

  ctx.header_page_->AsMut<BPlusTreeHeaderPage>()->root_page_id_ = root_page_id;

  return true;
}

INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::split_child_preemptive(WritePageGuard &parent_guard, 
                                        WritePageGuard &child_guard, const KeyType &key,
                                        const std::optional<ValueType> &value_if_leaf) -> page_id_t {
  auto *parent = parent_guard.AsMut<InternalPage>();
  auto *child_page = child_guard.AsMut<BPlusTreePage>();

  BUSTUB_ASSERT(parent->GetSize() < parent->GetMaxSize(), "Parent must be safe before splitting child");

  // --- Case 1: child is leaf: split + insert (key,value) ---
  if (child_page->IsLeafPage()) {
    auto *left = child_guard.AsMut<LeafPage>();
    page_id_t right_id = bpm_->NewPage();
    WritePageGuard right_guard = bpm_->WritePage(right_id);
    auto *right = right_guard.AsMut<LeafPage>();
    right->Init(leaf_max_size_);

    KeyType sep{};
    SplitLeaf(left, key, value_if_leaf.value(), right, right_id, sep);

    InsertIntoInternal(parent, sep, right_id);
    if (comparator_(key, sep) >= 0) {
      return right_id;
    }
    return child_guard.GetPageId();
  }

  // --- Case 2: child is internal: split only ---
  auto *left = child_guard.AsMut<InternalPage>();
  page_id_t right_id = bpm_->NewPage();
  WritePageGuard right_guard = bpm_->WritePage(right_id);
  auto *right = right_guard.AsMut<InternalPage>();
  right->Init(internal_max_size_);

  KeyType up{};
  {
    const int n = left->GetSize();               // n = child pointer count
    const int mid = (n + 1) / 2;                
    up = left->KeyAt(mid);

    const int right_keys = n - mid - 1;          // keys(mid+1..n-1)
    if (right_keys > 0) {
      std::memmove(right->key_array_ + 1, left->key_array_ + (mid + 1), sizeof(KeyType) * right_keys);
    }
    std::memmove(right->page_id_array_, left->page_id_array_ + mid, sizeof(page_id_t) * (right_keys + 1));

    right->SetSize(right_keys + 1);
    left->SetSize(mid);
  }

  InsertIntoInternal(parent, up, right_id);

  if (comparator_(key, up) >= 0) {
    return right_id;
  }
  return child_guard.GetPageId();
}

// struct split_root_result {
//   page_id_t new_root_id;
//   bool end;
// }

INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::split_root_preemptive(WritePageGuard &old_root_guard, 
                                          const KeyType& key, 
                                          const ValueType& value) -> page_id_t {
  auto *old_root_page = old_root_guard.AsMut<BPlusTreePage>();
  BUSTUB_ASSERT(!old_root_page->IsLeafPage(), "split_root_preemptive expects an internal root");

  auto *left = old_root_guard.AsMut<InternalPage>();
  page_id_t right_id = bpm_->NewPage();
  WritePageGuard right_guard = bpm_->WritePage(right_id);
  auto *right = right_guard.AsMut<InternalPage>();
  right->Init(internal_max_size_);

  KeyType up{};
  {
    const int n = left->GetSize();
    const int mid = (n + 1) / 2;
    up = left->KeyAt(mid);

    const int right_keys = n - mid - 1;
    if (right_keys > 0) {
      std::memmove(right->key_array_ + 1, left->key_array_ + (mid + 1), sizeof(KeyType) * right_keys);
    }
    std::memmove(right->page_id_array_, left->page_id_array_ + mid, sizeof(page_id_t) * (right_keys + 1));
    right->SetSize(right_keys + 1);
    left->SetSize(mid);
  }

  page_id_t new_root_id = bpm_->NewPage();
  WritePageGuard new_root_guard = bpm_->WritePage(new_root_id);
  auto *new_root = new_root_guard.AsMut<InternalPage>();
  new_root->Init(internal_max_size_);
  new_root->SetSize(2);
  new_root->SetKeyAt(1, up);
  new_root->SetValueAt(0, old_root_guard.GetPageId());
  new_root->SetValueAt(1, right_id);
  return new_root_id;
}

// use read-latch lock to probe to leaf, return whether need retry. true means need retry
INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::probe_to_leaf_read_only(Context &ctx, const KeyType &key, 
                                            const ValueType &value,
                                            const BPlusTreePage* page) -> bool {
  (void)value;

  while (!page->IsLeafPage()) {
    auto parent = static_cast<const InternalPage *>(page);
    if (parent->GetSize() >= parent->GetMaxSize()) {
      // Parent is not safe to receive a split.
      return true;
    }

    int idx = FindKeyBiSearch(page, key);
    page_id_t child_page_id = parent->ValueAt(idx);
    ctx.read_set_.push_back(bpm_->ReadPage(child_page_id));
    page = ctx.read_set_.back().As<BPlusTreePage>();

    // Conservative: if we ever see a full internal page on the path, we will need write-mode.
    if (!page->IsLeafPage() && page->GetSize() == page->GetMaxSize()) {
      return true;
    }
  }

  // Leaf reached; if leaf is full we need to restart with write-mode to split.
  if (page->GetSize() == page->GetMaxSize()) {
    return true;
  }
  return false;
}
  

// Striclty correct insert function, including retry logic
INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::InsertPreemptive(const KeyType &key, const ValueType &value) -> bool {
  bool force_write_mode = false;

  auto try_insert_into_leaf = [&](LeafPage *leaf) -> std::optional<bool> {
    int pos = LeafIndexToInsert(leaf, key);
    if (pos < leaf->GetSize() && comparator_(leaf->KeyAt(pos), key) == 0) {
      return false;
    }
    if (leaf->GetSize() >= leaf->GetMaxSize()) {
      return std::nullopt;
    }
    ShiftRightByOne(leaf, pos);
    leaf->SetKeyAt(pos, key);
    leaf->SetValueAt(pos, value);
    leaf->SetSize(leaf->GetSize() + 1);
    return true;
  };

  while (true) {
    Context ctx;

    // Phase 0: read root page id under header latch.
    ctx.header_page_.emplace(bpm_->WritePage(header_page_id_));
    page_id_t root_page_id = ctx.header_page_->As<BPlusTreeHeaderPage>()->root_page_id_;
    if (root_page_id == INVALID_PAGE_ID) {
      return insert_into_empty_tree(ctx, key, value);
    }

    // Phase 1: probe with read latches (optional).
    if (!force_write_mode) {
      ctx.read_set_.push_back(bpm_->ReadPage(root_page_id));
      ctx.header_page_ = std::nullopt;
      const auto *root_page = ctx.read_set_.back().As<BPlusTreePage>();

      if (root_page->GetSize() == root_page->GetMaxSize()) {
        force_write_mode = true;
        ctx.read_set_.clear();
        continue;
      }

      bool need_write = probe_to_leaf_read_only(ctx, key, value, root_page);
      if (!need_write) {
        page_id_t leaf_id = ctx.read_set_.back().GetPageId();
        ctx.read_set_.clear();

        WritePageGuard leaf_guard = bpm_->WritePage(leaf_id);
        auto *leaf = leaf_guard.AsMut<LeafPage>();
        auto res = try_insert_into_leaf(leaf);
        if (res.has_value()) {
          return res.value();
        }
      }

      force_write_mode = true;
      ctx.read_set_.clear();
      continue;
    }

    // Phase 2: write-mode top-down insert with preemptive splits.
    WritePageGuard parent_guard = bpm_->WritePage(root_page_id);
    auto *root_page = parent_guard.AsMut<BPlusTreePage>();

    // Root is leaf: either insert or split root and finish.
    if (root_page->IsLeafPage()) {
      auto *leaf = parent_guard.AsMut<LeafPage>();
      auto res = try_insert_into_leaf(leaf);
      if (res.has_value()) {
        return res.value();
      }

      KeyType sep{};
      page_id_t right_leaf_id = bpm_->NewPage();
      WritePageGuard right_leaf_guard = bpm_->WritePage(right_leaf_id);
      auto *right_leaf = right_leaf_guard.AsMut<LeafPage>();
      right_leaf->Init(leaf_max_size_);
      SplitLeaf(leaf, key, value, right_leaf, right_leaf_id, sep);

      page_id_t new_root_id = bpm_->NewPage();
      WritePageGuard new_root_guard = bpm_->WritePage(new_root_id);
      auto *new_root = new_root_guard.AsMut<InternalPage>();
      new_root->Init(internal_max_size_);
      new_root->SetSize(2);
      new_root->SetKeyAt(1, sep);
      new_root->SetValueAt(0, parent_guard.GetPageId());
      new_root->SetValueAt(1, right_leaf_id);

      ctx.header_page_->AsMut<BPlusTreeHeaderPage>()->root_page_id_ = new_root_id;
      return true;
    }

    // Root is internal: split root if full.
    if (root_page->GetSize() == root_page->GetMaxSize()) {
      page_id_t new_root_id = split_root_preemptive(parent_guard, key, value);
      ctx.header_page_->AsMut<BPlusTreeHeaderPage>()->root_page_id_ = new_root_id;
      root_page_id = new_root_id;
      parent_guard = bpm_->WritePage(root_page_id);
    }

    // Root is now safe; release header early.
    ctx.header_page_ = std::nullopt;

    while (true) {
      auto *parent = parent_guard.AsMut<InternalPage>();
      int idx = FindKeyBiSearch(parent_guard.AsMut<BPlusTreePage>(), key);
      page_id_t child_id = parent->ValueAt(idx);
      WritePageGuard child_guard = bpm_->WritePage(child_id);
      auto *child_page = child_guard.AsMut<BPlusTreePage>();

      if (child_page->IsLeafPage()) {
        auto *leaf = child_guard.AsMut<LeafPage>();
        auto res = try_insert_into_leaf(leaf);
        if (res.has_value()) {
          return res.value();
        }

        KeyType sep{};
        page_id_t right_leaf_id = bpm_->NewPage();
        WritePageGuard right_leaf_guard = bpm_->WritePage(right_leaf_id);
        auto *right_leaf = right_leaf_guard.AsMut<LeafPage>();
        right_leaf->Init(leaf_max_size_);
        SplitLeaf(leaf, key, value, right_leaf, right_leaf_id, sep);
        InsertIntoInternal(parent, sep, right_leaf_id);
        return true;
      }

      // Internal child: split if full before descending.
      if (child_page->GetSize() == child_page->GetMaxSize()) {
        page_id_t next_id = split_child_preemptive(parent_guard, child_guard, key, std::nullopt);
        if (next_id != child_guard.GetPageId()) {
          child_guard.Drop();
          child_guard = bpm_->WritePage(next_id);
        }
      }

      parent_guard = std::move(child_guard);
    }
  }
}

// /*****************************************************************************
//  * REMOVE
//  *****************************************************************************/

// // Borrow the rightmost element from the left_page, and update the key that points to the right page as the changed leftmost key in the right page
// INDEX_TEMPLATE_ARGUMENTS
// auto BPLUSTREE_TYPE::BorrowFromLeftLeafPage(LeafPage *page, LeafPage* left_page, InternalPage* parent_page, int index) -> void {
//   // parent必然是internal
  
//   int size = page->GetSize();
//   int left_size = left_page->GetSize();

//   // 如果page是leaf，那么getsize返回的是key的大小，否则返回keySize + 1
//   ShiftRightByOne(page, 0);
//   page->SetKeyAt(0, left_page->KeyAt(left_size - 1));
//   page->SetValueAt(0, left_page->ValueAt(left_size - 1));
//   page->SetSize(size + 1);
//   left_page->SetSize(left_size - 1);
//   parent_page->SetKeyAt(index, page->KeyAt(0));
// }

// // Right page pulls the key from its parent and then use it as its leftmost key. Borrow the rightmost key of left page as its leftmost key.
// // The parent will change the corresponding key to the newer rightmost key of the leftpage
// INDEX_TEMPLATE_ARGUMENTS
// auto BPLUSTREE_TYPE::BorrowFromLeftInternalPage(InternalPage *page, InternalPage* left_page, InternalPage* parent_page, int index) -> void {
//   int size = page->GetSize();
//   int left_size = left_page->GetSize();
//   ShiftRightByOne(page, 0);
//   page->SetKeyAt(1, parent_page->KeyAt(index));
//   page->SetValueAt(0, left_page->KeyAt(left_size - 1));
//   page->SetSize(size + 1);
  
//   // parent changes the key to the rightmost key of the leftpage
//   parent_page-SetKey(index, left_page->KeyAt(left_size - 1));
//   left_page->SetSize(left_page - 1);
// }


// /**
//  * @brief Delete key & value pair associated with input key
//  * If current tree is empty, return immediately.
//  * If not, User needs to first find the right leaf page as deletion target, then
//  * delete entry from leaf page. Remember to deal with redistribute or merge if
//  * necessary.
//  *
//  * @param key input key
//  */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::Remove(const KeyType &key) {
//   // Declaration of context instance.
//   Context ctx;

//   WritePageGuard head_guard = bpm_->WritePage(header_page_id_);
//   ctx.root_page_id_ = read_head_guard.As<BPlusTreeHeaderPage>()->root_page_id_;

//   if (ctx.root_page_id_ == INVALID_PAGE_ID) {return;}

//   // 1) 乐观阶段：只要叶子“就地删除”后不下溢，且不需要改父分隔键，就直接完成
//   std::vector<page_id_t> path; // 这个path，同insert中的path，可以设置为b+tree的一个attribute，不然每次insert调用都需要vector allocation
//   std::vector<int> idx_path; // child在parent中的index--给 borrow/merge的parent使用，这个也应该放在class中作为attribute
//   ctx.read_set_.push_back(bpm_->ReadPage(ctx.root_page_id_));
//   auto page = ctx.read_set_.back().As<BPlusTreePage>();
  
//   // root是leaf，直接升级为write, 并且直接从leaf/root中删除此key
//   if (page->IsLeafPage()) {
//     ctx.read_set_.pop_back();
//     ctx.write_set_.push_back(bpm_->WritePage(ctx.root_page_id_));
//     page = ctx.write_set_.back().AsMut<BPlusTreePage>();

//     int n = page->GetSize();
//     int pos = LeafIndexToInsert(page, key);
//     if ((comparator_(page->KeyAt(pos), key) == 0)) {return};  // pos最大等于 n - 1, 最小等于0, 这里的意思是如果没有该key，直接返回
//     ShiftLeftByOne(page, pos);
//     page->SetSize(n - 1);

//     // root 被删空 → root = INVALID
//     if(page->GetSize() == 0) {
//       head_guard.AsMut<BPlusTreeHeaderPage>()->root_page_id_ = INVALID_PAGE_ID; 
//     }
//     return;
//   }
  
//   // 确认root并非leaf，可以先释放header_guard, 我们仍持有root的锁，所以安全
//   auto parent = nullptr;
//   head_guard.Drop();
//   while (!page->IsLeafPage()) {
//     parent = static_cast<InternalPage *>(page);
//     int idx = FindKeyBiSearch(page, key);
    
//     page_id_t cid = parent->ValueAt(idx);

//     ctx.read_set_.push_back(bpm_->ReadPage(cid));
//     page = ctx.read_set_.back().As<BPlusTreePage>();
//     path.push_back(cid);
//     idx_path.push_back(idx);

//     if (parent->GetSize() - 1 > parent->GetMaxSize() && ctx.read_set_.size() > 2) { // the first condition means 即使child 有merge，parent 删除node之后也不会影响grand parent
//       ctx.read_set_.pop_front();
//     }
//   }

//   // 升级cid为wlatch
//   page_id_t leaf_id = path.back();
//   ctx.read_set_.pop_back();
//   ctx.write_set_.push_back(bpm_->WritePage(leaf_id));
//   page = ctx.write_set_.back().As<BPlusTreePage>();

//   // 尝试就地删除
//   int n = page->GetSize();
//   int pos = LeafIndexToInsert(page, key);
//   if (comparator_(leaf->KeyAt(pos), key) == 0) {
//     return;
//   }

//   const bool will_underflow = (n - 1) < page->GetMinSize();
//   const bool delete_is_first = (pos == 0); // if this is true, we gotta change the key in the parent_page
//   const bool has_parent = (path.size() > 1); //因为我们之前处理过page is leaf的情况，这里page应该是100% has parent

//   // 获得被delete的key在parent中的位置
//   int idx = idx_path.back();

//   // has parent百分百触发，因为root = leaf的情况已经讨论过了
//   if (!will_underflow && has_parent) {
//     if (!delete_is_first) {
//       // 叶子直接正常删除
//       ShiftLeftByOne(page, pos);
//       page->SetSize(n - 1);
//       return;
//     }
//     else {
//       ShiftLeftByOne(page, pos);
//       page->SetSize(n - 1);
//       auto parent_page_id = ctx.read_set_.back().GetPageId();
//       if (parent_page_id == ctx.root_page_id_) { // parent是root，那么首先需要获取header的写锁
//         ctx.header_page_.emplace(bpm_->WritePage(header_page_id_));
//       }
//       // 将parent中指向child的位置的key设置成child中最左边的key
//       parent.SetKeyAt(idx, page->KeyAt(0));
//       return;
//     }
//   }

//   //接下来是Underflow = true需要merge/borrow的复杂情况, 先delete. 然后处理leaf，最后进入进入while由下至上 判断borrow，如果不行再merge
//   if (!delete_is_first) { // 无需改动parent
//     ShiftLeftByOne(page, pos);
//     page->SetSize(n - 1);
//   }
//   else {
//     ShiftLeftByOne(page, pos);
//     page->SetSize(n - 1);
//     auto parent_page_id = ctx.read_set_.back().GetPageId();
//     if (parent_page_id == ctx.root_page_id_) { // parent是root，那么首先需要获取header的写锁
//       ctx.header_page_.emplace(bpm_->WritePage(header_page_id_));
//     }
//     parent.SetKeyAt(idx, page.KeyAt(0));
//   }

//   // 处理leaf中borrow和merge的情况，后续的代码处理internal node中的merge 和borrow
//   // left=leaf的情况：
//   // leafsize > minsize: 1. left_page.getsize + page.getsize < leaf.maxsize, 直接pull parent中一个node，然后merge，parent少一个node，判断parent
//   //                     2. left_page.getsize + page.getsize >= leaf.maxsize, 只能从left中pull一个node到right_page, 然后改parent直接返回
//   // rightsize > minsize 1. 类似于left
//   //                     2. 也类似

//   WritePageGuard left_guard = bpm_->WritePage(parent_page->ValueAt(idx - 1));
//   auto left_page = left_guard.AsMut<BPlusTreeLeafPage>();  // 因为当前page是leaf，那么它的left必然也是leaf
//   ctx.write_set_.push_back(left_guard);
//   if ( left_page->GetSize() > left_page->GetMinSize() ) {
//     if (left_page->GetSize() + page->GetSize() >= page->GetMaxSize()) {  // NO merge should happen 
//       BorrowFromLeft(page, static_cast<LeafPage*>(left_page), parent, idx);
//     }
//     else { // pull one key off the parent, merge with this key, left_page. (left_page + key + page). Parentsize - 1
//       // 如果parent是root，需要拿到header_page_guard, 但是获取header_guard这个过程中可能有其他其他call获取到了header然后再等待root锁，那么这里会死锁
//     }
    

//   }
  
  
//   // 进入while，root必然不为leaf，无需考虑root的情况，因为leaf size < half的时候，自动上提，root直接成为leaf。那么下一次delete的request会自动被上面的if block处理
//   while (true) {
//     // 先判断是否可以左借，r
    
//       WritePageGuard left_guard = bpm_->WritePage(parent_page->ValueAt(idx - 1));
//       auto left_page = left_guard.AsMut<BPlusTreePage>();

//       // 后续是一下几种情况，
      
//       // left 是 internal 

//       // 如果满足left可以被borrow的要求, write_set_需持有left_guard, 从而修改
//       if ( left_page->GetSize() > left_page->GetMinSize() ) {
//         if (left_page->IsLeafPage()) { 
//           ctx.write_set_.push_back(left_guard);
//           BorrowFromLeft(page, static_cast<LeafPage*>(left_page), parent, idx);
//           continue; //直接跳到下一回合无需看right_page
//         }
//         else { // left_page is an internal page, borrow or merge
//           if (left->GetSize() + page->GetSize)

//         }
//       }
      
    

//   }
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
