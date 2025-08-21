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
  // int size = page->GetSize();
  
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
  std::memmove(leaf_page->rid_array_ + insertIndex + 1, leaf_page->rid_array_ + insertIndex, sizeof(KeyType) * (size - insertIndex));
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
  int mid = n / 2;
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
    left->page_id_array_[pos + 1] = new_child_page_id;
    left->SetSize(nleft + 1);
  }
  else {
    int pos = KeySlotLowerBound(right, sep_key);
    int nright = right->GetSize();

    std::memmove(right->key_array_ + pos + 1, right->key_array_ + pos, sizeof(KeyType) * (nright - pos));
    std::memmove(right->page_id_array_ + pos + 1, right->page_id_array_ + pos, sizeof(page_id_t) * (nright - pos));
    right->SetKeyAt(pos, sep_key);
    right->page_id_array_[pos + 1] = new_child_page_id;
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

    ctx.header_page_->AsMut<BPlusTreeHeaderPage>()->root_page_id_ = root_page_id; // 这一步是必要的，看似我们修改了ctx这个临时变量似乎对全局的b+Tree没有影响
                                                                                  // 但是实际上我们是在修改ctx当前writeGuard指向的frame中的data，也就是存储在内存空间--全局的data
                                                                                  // 我们修改了全局的数据，ctx作为指向这个全局数据的临时载体，会在出scope之后被销毁，但是全局数据的rootpageid已经被修改了

    return true;
  }

  // 2) Non-empty tree, traverse from root to leaf using read-latch 
  ctx.read_set_.push_back(bpm_->ReadPage(ctx.root_page_id_));
  auto page = ctx.read_set_.back().As<BPlusTreePage>();
  // path.push_back(ctx.root_page_id_);

  if (page->IsLeafPage()) {
    ctx.read_set_.pop_back();
    ctx.write_set_.push_back(bpm_->WritePage(ctx.root_page_id_));
    auto leaf = ctx.write_set_.back().AsMut<LeafPage>();

    int pos = LeafIndexToInsert(leaf, key);
    if (pos < leaf->GetSize() && comparator_(leaf->KeyAt(pos), key) == 0) { return false;}

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
    // if (idx != -1) {return false;}  // 不应在 internal 命中就视为重复
    page_id_t cid = parent->ValueAt(idx);

    // child_idx.push_back(idx);
    ctx.read_set_.push_back(bpm_->ReadPage(cid));
    page = ctx.read_set_.back().As<BPlusTreePage>();
    path.push_back(cid);

    // lab crabbing：子页“安全”（这里只讨论无回撤的读路径，是否安全在叶子阶段再说），可以释放更高的祖先读锁
    if (parent->GetSize() <= parent->GetMaxSize() && ctx.read_set_.size() > 2) {
      ctx.read_set_.pop_front();
    }

  }

  //2.4 到达leaf:升级leaf write-latch, 先pop readlatch，然后将readlatch升级成writelatch，此时parent仍然持有latch，安全
  page_id_t leaf_id = path.back();
  ctx.read_set_.pop_back(); 
  ctx.write_set_.push_back(bpm_->WritePage(leaf_id));
  auto leaf = ctx.write_set_.back().AsMut<LeafPage>();

  int pos = LeafIndexToInsert(leaf, key);
  if (pos < leaf->GetSize() && comparator_(leaf->KeyAt(pos), key) == 0) {return false;}
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
  page_id_t new_child_id = new_leaf_guard.GetPageId(); // 插入到parent中的新的right child
  
  // from bottom to top: 将(sep, new_child_id) 插入parent
  while (true) {
    //是否有parent
    if (path.size() == 1) {  //root被分裂
      // root 是internal
      // ctx header write-latch需要更新ctx.root_page_id_
      if (!ctx.header_page_.has_value()) { //按理来说这里不可能发生
        ctx.header_page_.emplace(bpm_->WritePage(header_page_id_));
      }

      auto root_guard = bpm_->WritePage(ctx.root_page_id_);
      auto root = root_guard.AsMut<InternalPage>();
      // int ins = FindKeyBiSearch(root, sep) + 1; // ins是insert的index + 1的位置
      if (root->GetSize() <= root->GetMaxSize()) { // 这里的 "<=" 和下面
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
      if (!ctx.header_page_.has_value()) {
        ctx.header_page_.emplace(bpm_->WritePage(header_page_id_));
      }
      // ctx.read_set_.pop_back();
    }
    ctx.read_set_.pop_back();
    ctx.write_set_.push_back(bpm_->WritePage(parent_id));
    auto parent = ctx.write_set_.back().AsMut<InternalPage>();

    if (parent->GetSize() <= parent->GetMaxSize()) {  // parent未满，leaf insert多出的node是可以安全放入parent中的，无需循环往上，后续直接return
                                                      // 这里是 "<=" 而不是 "<"的原因是 GetSize()返回的是keySize + 1，而GetMaxSize()返回的是max keySize, 
                                                      // 所以只要当 keySize + 1 <= maxKeySize, 也就是 keySize < maxKeySize就可以直接insert
      InsertIntoInternal(parent, sep, new_child_id);
      return true;
    }

    // parent 满了，也需分裂，得到上推键up和new right brother
    KeyType up{};
    auto new_internal_guard = bpm_->WritePage(bpm_->NewPage());
    auto new_internal = new_internal_guard.AsMut<InternalPage>();
    new_internal->Init(internal_max_size_);
    SplitInternal(parent, sep, new_internal, new_child_id, up); 

    // 将(up, new_internal_id)向上一层插入
    sep = up;
    new_child_id = new_internal_guard.GetPageId();
    path.pop_back();
    ctx.write_set_.pop_back();
  }

  return true;  
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
