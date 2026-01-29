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
  else {
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

// INDEX_TEMPLATE_ARGUMENTS
// auto BPLUSTREE_TYPE::SplitInternal(InternalPage* left, KeyType& sep_key, InternalPage* right, page_id_t new_child_page_id, KeyType& up) -> void {
//   const int n = left->GetSize();
//   int mid = (n + 1) / 2;

//   int pos = KeySlotLowerBound(left, sep_key); // 找到sep_key在left中的插入位置

//   // 举一个极端例子，keysize = 2, value size = 3, mid = (3 + 1) / 2 = 2
//   // left page: k0 (invalid), k1, k2; v0, v1, v2, minsize = (3 + 1) / 2 = 2 = mid
//   if (pos < mid) { // 插入在left页
//     // 右页应得到: 原left 的 [mid, n - 1] 共 n - mid 个 + 我们从插入后左页有 mid 个
//     // 先把左页的右半 (不含将要插入的位置左侧元素) 整体帮到右页
//     // 注意: 插入后左页大小是  mid, 因此从原数组抽取区间 [mid, n - 1] 共 (n - mid) 个元素
//     // 但由于新建在左页，我们搬到右页的其实是left page中右半的 n - mid 个 “old element"
//     // right page的目标区间 [0, n - mid - 1]
//     std::memmove(right->key_array_ + 1, left->key_array_ + mid, sizeof(KeyType) * (n - mid));
//     std::memmove(right->page_id_array_, left->page_id_array_ + mid - 1, sizeof(page_id_t) * (n - mid + 1));

//     right->SetSize(n - mid + 1);

//     // 在左页内，为pos挪出空位，把 [pos, mid - 1]向右移动一格，由于right夺取了left mid个key但是mid + 1个value，
//     // 所以下面left的key和value移动的大小是相同的，都是 mid - pos - 1
//     std::memmove(left->key_array_ + pos + 1, left->key_array_ + pos, sizeof(KeyType) * (mid - pos - 1));
//     std::memmove(left->page_id_array_ + pos + 1, left->page_id_array_ + pos, sizeof(page_id_t) * (mid - pos - 1));

//     // 写入新建到左页
//     left->SetKeyAt(pos, sep_key);
//     left->page_id_array_[pos] = new_child_page_id;
//     left->SetSize(mid);
//     up = left->KeyAt(mid); // up 是 left 页最后一个key
//   }
//   else { // 插入在right页，那么右页夺取mid + 1之后，比插入在left的情况小一个
//     std::memmove(right->key_array_ + 1, left->key_array_ + mid + 1, sizeof(KeyType) * (n - mid - 1));
//     std::memmove(right->page_id_array_, left->page_id_array_ + mid, sizeof(page_id_t) * (n - mid));
    
//     right->SetSize(n - mid);
    
//     InsertIntoInternal(right, sep_key, new_child_page_id);

//     left->SetSize(mid);
//     up = left->KeyAt(mid); // up 是 left 页最后一个key
//   }

// }

INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::SplitInternal(InternalPage *left,
                                  KeyType &sep_key,
                                  InternalPage *right,
                                  page_id_t new_child_page_id,
                                  KeyType &up) -> void {
  const int n = left->GetSize();        // old value count
  const int n2 = n + 1;                // new value count after insertion
  const int split = n2 / 2;            // left keeps 'split' children (value count)

  // pos is key index in [1..n] (because after insertion key count = n)
  int pos = KeySlotLowerBound(left, sep_key);

  const int right_size = n2 - split;   // value count in right
  right->SetSize(right_size);
  // helper lambda: 拷贝一段 old values 到 right values
  auto copy_old_values_to_right = [&](int right_dst, int old_src, int cnt) {
    std::memmove(right->page_id_array_ + right_dst,
                 left->page_id_array_ + old_src,
                 sizeof(page_id_t) * cnt);
  };
  auto copy_old_keys_to_right = [&](int right_dst_keyidx, int old_src_keyidx, int cnt) {
    // right keys start at 1
    std::memmove(right->key_array_ + right_dst_keyidx,
                 left->key_array_ + old_src_keyidx,
                 sizeof(KeyType) * cnt);
  };

  // 我们要让 right 拿 children indices [split .. n2-1] (共 right_size = n2 - split)
  // 对应的 keys 是 inserted_keys[split+1 .. n]，但写到 right 的 key_array_[1..]

  // 构造 right children
  // 情况分三类：split 区间整体在 pos 左 / 右 / 跨过 pos
  if (split < pos) {
    // inserted_children[split .. pos-1] 来自 old values [split .. pos-1]
    // inserted_children[pos] 是 new_child
    // inserted_children[pos+1 ..] 来自 old values [pos ..]
    int first_cnt = pos - split; // 来自 old [split..pos-1]
    copy_old_values_to_right(0, split, first_cnt);
    right->page_id_array_[first_cnt] = new_child_page_id;
    int remain = right_size - (first_cnt + 1);
    if (remain > 0) {
      copy_old_values_to_right(first_cnt + 1, pos, remain);
    }
  } else if (split == pos) {
    // 第一个 child 就是 new_child
    right->page_id_array_[0] = new_child_page_id;
    // 后面来自 old values [pos ..]
    copy_old_values_to_right(1, pos, right_size - 1);
  } else { // split > pos
    // right 全部来自 old values [split-1 ..] 因为插入点在 split 之前
    copy_old_values_to_right(0, split - 1, right_size);
  }

  // 构造 right keys（注意：right->key_array_[1] 对应 inserted_key[split+1]）
  // inserted_key[k] 的来源同理
  if (split + 1 < pos) {
    // inserted_keys[split+1 .. pos-1] <- old keys [split+1 .. pos-1]
    int first_cnt = pos - (split + 1);
    copy_old_keys_to_right(1, split + 1, first_cnt);
    // inserted_key[pos] = sep_key 写到 right 的某个位置：
    // right key idx = 1 + (pos - (split+1)) = pos - split
    right->SetKeyAt(pos - split, sep_key);
    int remain = (right_size - 1) - (first_cnt + 1);
    if (remain > 0) {
      // inserted_keys[pos+1 ..] <- old keys [pos ..]
      copy_old_keys_to_right((pos - split) + 1, pos, remain);
    }
  } else if (split + 1 == pos) {
    // right 最小 key 就是 sep_key
    right->SetKeyAt(1, sep_key);
    // 之后 inserted_keys[pos+1..] <- old keys [pos..]
    if (right_size - 2 > 0) {
      copy_old_keys_to_right(2, pos, right_size - 2);
    }
  } else { // split+1 > pos
    // right keys 全来自 old keys [split ..] (因为 inserted_keys 向右偏了一位)
    // inserted_key[split+1] <- old key[split] (当 pos <= split)
    if (right_size - 1 > 0) {
      copy_old_keys_to_right(1, split, right_size - 1);
    }
  }

  // ---------- Step 2: 计算 up key，并收缩 left ----------
  // up 是 inserted_key[split]（注意 split>=1）
  // 我们现在要从 left 的内容中拿到它：
  if (split < pos) {
    // inserted_key[split] 来自 old key[split]
    up = left->KeyAt(split);
  } else if (split == pos) {
    up = sep_key;
  } else { // split > pos
    // inserted_key[split] 来自 old key[split-1]
    up = left->KeyAt(split - 1);
  }

  // left 保留 children[0..split-1] => value count = split
  left->SetSize(split);

  // 同时要保证 left 的 keys 是 inserted_keys[1..split-1]
  // 如果 pos < split，需要在 left 内插入 sep_key 并右移；但我们最终不需要保存 inserted_key[split]（up）
  // 简单做法：在 left 内“完成插入”，再把 split 位置（up）丢掉。
  //
  // 完成插入 (只影响 left 的前 split 部分)
  if (pos < split) {
    // 需要在 left 的 key[pos] 插入 sep_key，value[pos] 插入 new_child
    // keys shift: [pos..split-1] -> [pos+1..split]，但 split 是 up，将会被丢掉，所以只需移动到 split-1 即可
    std::memmove(left->key_array_ + pos + 1,
                 left->key_array_ + pos,
                 sizeof(KeyType) * (split - 1 - pos));   // 移动 keys 到 split-1

    std::memmove(left->page_id_array_ + pos + 1,
                 left->page_id_array_ + pos,
                 sizeof(page_id_t) * (split - pos));      // children 多 1，所以移动到 split-1

    left->SetKeyAt(pos, sep_key);
    left->SetValueAt(pos, new_child_page_id);
  } else if (pos == split) {
    // sep_key/new_child 落在 up 位置（会被提升），所以 left 不需要插入 key
    // 但 left 的 children 在 split 位置会被 up 分割掉：left 保留到 split-1，正好不包含 new_child
    // 所以不动 left
  } else {
    // pos > split：插入点在 right，left 不需要动
  }

  // 注意：left->SetSize(split) 已经把 split 之后的视为无效，无需清空残留内存
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
  // parent->page_id_array_[pos] = new_child_page_id;
  parent->SetValueAt(pos, new_child_page_id); 
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
  // return InsertPreemptive(key, value);
  return InsertUpDownRetry(key, value);
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

INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::InsertUpDownRetry(const KeyType &key, const ValueType &value) -> bool {
  Context ctx;
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

  // Case 2: Non-empty and safe tree (leaf is not full), latch crabbing down
  ctx.read_set_.push_back(bpm_->ReadPage(ctx.root_page_id_));
  ctx.header_page_ = std::nullopt;
  auto page = ctx.read_set_.back().As<BPlusTreePage>();
  while (!page->IsLeafPage()) {
    auto parent = static_cast<const InternalPage *>(page);
    int idx = FindKeyBiSearch(page, key);
    page_id_t cid = parent->ValueAt(idx);

    ctx.read_set_.push_back(bpm_->ReadPage(cid));
    page = ctx.read_set_.back().As<BPlusTreePage>();
    // lab crabbing：子页“安全”可以释放更高的祖先读锁
    if (parent->GetSize() < parent->GetMaxSize() && ctx.read_set_.size() > 2) {
      ctx.read_set_.pop_front();
    }
  }

  // Leaf is safe, no need to retry 
  if (page->GetSize() < page->GetMaxSize()) { 
    page_id_t leaf_id = ctx.read_set_.back().GetPageId();
    ctx.read_set_.pop_back();
    ctx.write_set_.push_back(bpm_->WritePage(leaf_id)); 
    if (ctx.read_set_.size() > 0) {ctx.read_set_.pop_back();}  // already acquired child latch, and child is safe, so parent can be released

    auto leaf = ctx.write_set_.back().AsMut<LeafPage>();

    int pos = LeafIndexToInsert(leaf, key);
    if (pos < leaf->GetSize() && comparator_(leaf->KeyAt(pos), key) == 0) {
      return false;
    }

    ShiftRightByOne(leaf, pos);
    leaf->SetKeyAt(pos, key);
    leaf->SetValueAt(pos, value);
    leaf->SetSize(leaf->GetSize() + 1);
    return true;
  }

  // Case 3: leaf isn't safe, release all latches and retry InsertUpDown
  ctx.read_set_.clear();
  ctx.header_page_.emplace(bpm_->WritePage(header_page_id_));
  ctx.root_page_id_ = ctx.header_page_->As<BPlusTreeHeaderPage>()->root_page_id_;

  ctx.write_set_.emplace_back(bpm_->WritePage(ctx.root_page_id_)); // upgrade root latch to write latch);
  auto page1 = ctx.write_set_.back().AsMut<BPlusTreePage>();
  while (!page1->IsLeafPage()) {
    auto parent = static_cast<InternalPage *>(page1);
    int idx = FindKeyBiSearch(page1, key);
    page_id_t cid = parent->ValueAt(idx);

    ctx.write_set_.push_back(bpm_->WritePage(cid));
    page1 = ctx.write_set_.back().AsMut<BPlusTreePage>();
    if (page1->GetSize() < page1->GetMaxSize()) {
      while (ctx.write_set_.size() > 1) {
        ctx.header_page_ = std::nullopt;
        ctx.write_set_.pop_front(); // child has slot to insert, release parent
      }
    }
  }
  auto leaf = ctx.write_set_.back().AsMut<LeafPage>();  
  // leaf is full, split the leaf 
  KeyType sep{};
  auto right_page_id = bpm_->NewPage();
  auto new_leaf_guard = bpm_->WritePage(right_page_id);
  auto new_leaf = new_leaf_guard.AsMut<LeafPage>();
  new_leaf->Init(leaf_max_size_);
  SplitLeaf(leaf, key, value, new_leaf, right_page_id, sep);
  page_id_t new_child_id = new_leaf_guard.GetPageId();
  ctx.write_set_.pop_back(); // pop leaf

  if (ctx.write_set_.empty()) { // leaf was root and needed split
    ctx.root_page_id_ = ctx.header_page_->AsMut<BPlusTreeHeaderPage>()->root_page_id_;
    auto new_root_id = bpm_->NewPage();
    auto new_root_guard = bpm_->WritePage(new_root_id);
    auto new_root_page = new_root_guard.AsMut<InternalPage>();
    new_root_page->Init(internal_max_size_);
    new_root_page->SetSize(2);
    new_root_page->SetKeyAt(1, sep);
    new_root_page->SetValueAt(0, ctx.root_page_id_);
    new_root_page->SetValueAt(1, new_child_id);

    ctx.header_page_->AsMut<BPlusTreeHeaderPage>()->root_page_id_ = new_root_id;
    return true;

  }

  // from bottom to top: 将(sep, new_child_id) 插入parent
  while (true) {
    auto parent = ctx.write_set_.back().AsMut<InternalPage>();
    if (parent->GetSize() < parent->GetMaxSize()) {
      InsertIntoInternal(parent, sep, new_child_id);
      return true;
    }

    // parent 满了，也需分裂，得到上推键up和new right brother
    KeyType up{};
    auto new_internal_guard = bpm_->WritePage(bpm_->NewPage());
    auto new_internal = new_internal_guard.AsMut<InternalPage>();
    new_internal->Init(internal_max_size_);
    SplitInternal(parent, sep, new_internal, new_child_id, up); 

    auto parent_id = ctx.write_set_.back().GetPageId();
    if (parent_id == ctx.root_page_id_) {
      ctx.root_page_id_ = ctx.header_page_->AsMut<BPlusTreeHeaderPage>()->root_page_id_;
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
    ctx.write_set_.pop_back();
  }
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
  // (void)value;

  while (!page->IsLeafPage()) {
    auto parent = static_cast<const InternalPage *>(page);
    this->path.push_back(true);
    if (parent->GetSize() >= parent->GetMaxSize()) {
      // Parent is not safe to receive a split.
      return true;
    }

    int idx = FindKeyBiSearch(page, key);
    page_id_t child_page_id = parent->ValueAt(idx);
    ctx.read_set_.push_back(bpm_->ReadPage(child_page_id));
    page = ctx.read_set_.back().As<BPlusTreePage>();

    if (this->path.size() > 2) {ctx.read_set_.pop_front(); this->path.pop_back();}  // Release grandparent lock
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
    auto header_guard = bpm_->WritePage(header_page_id_);
    ctx.header_page_ = std::make_optional(std::move(header_guard));
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
        ctx.read_set_.pop_back();

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

/*****************************************************************************
 * REMOVE
 *****************************************************************************/

// Borrow the rightmost element from the left_page, and update the key that points to the right page as the changed leftmost key in the right page
INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::BorrowFromLeftLeafPage(LeafPage *page, LeafPage* left_page, InternalPage* parent_page, int index) -> void {
  // parent必然是internal
  
  int size = page->GetSize();
  int left_size = left_page->GetSize();

  // 如果page是leaf，那么getsize返回的是key的大小，否则返回keySize + 1
  ShiftRightByOne(page, 0);
  page->SetKeyAt(0, left_page->KeyAt(left_size - 1));
  page->SetValueAt(0, left_page->ValueAt(left_size - 1));
  page->SetSize(size + 1);
  left_page->SetSize(left_size - 1);
  parent_page->SetKeyAt(index, page->KeyAt(0));
}

INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::BorrowFromRightLeafPage(LeafPage *page, LeafPage* right_page, InternalPage* parent_page, int index) -> void {
  int size = page->GetSize();
  int right_size = right_page->GetSize();

  page->SetKeyAt(size, right_page->KeyAt(0));
  page->SetValueAt(size, right_page->ValueAt(0));
  page->SetSize(size + 1);

  ShiftLeftByOne(right_page, 0);
  right_page->SetSize(right_size - 1);
  parent_page->SetKeyAt(index + 1, right_page->KeyAt(0));
}

// Right page pulls the key from its parent and then use it as its leftmost key. Borrow the rightmost key of left page as its leftmost key.
// The parent will change the corresponding key to the newer rightmost key of the leftpage
INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::BorrowFromLeftInternalPage(InternalPage *page, InternalPage* left_page, InternalPage* parent_page, int index) -> void {
  int size = page->GetSize();
  int left_size = left_page->GetSize();

  std::memmove(page->key_array_ + 1, page->key_array_, sizeof(KeyType) * size);
  std::memmove(page->page_id_array_ + 1, page->page_id_array_, sizeof(page_id_t) * size);
  page->SetKeyAt(1, parent_page->KeyAt(index));
  page->SetValueAt(0, left_page->ValueAt(left_size - 1));
  page->SetSize(size + 1);
  
  // parent changes the key to the rightmost key of the leftpage,
  // namely the first key of page 
  parent_page->SetKeyAt(index, left_page->KeyAt(left_size - 1));
  left_page->SetSize(left_size - 1);
}

INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::BorrowFromRightInternalPage(InternalPage *page, InternalPage* right_page, InternalPage* parent_page, int index) -> void {
  int size = page->GetSize();
  int right_size = right_page->GetSize(); 

  page->SetKeyAt(size, parent_page->KeyAt(index + 1));
  page->SetValueAt(size, right_page->ValueAt(0));
  page->SetSize(size + 1);
  parent_page->SetKeyAt(index + 1, right_page->KeyAt(1));
  std::memmove(right_page->key_array_ + 1, right_page->key_array_ + 2, sizeof(KeyType) * (right_size - 2));
  std::memmove(right_page->page_id_array_, right_page->page_id_array_ + 1, sizeof(page_id_t) * (right_size - 1));
  right_page->SetSize(right_size - 1);
  
}

INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::MergeWithLeft(BPlusTreePage *page, BPlusTreePage *left_page, InternalPage *parent_page,
                                                                                  int index) -> void 
{
  int left_size = left_page->GetSize();
  int size = page->GetSize();
  int parent_size = parent_page->GetSize();
  // auto parent_internal = static_cast<InternalPage *>(parent_page);
  
  if (page->IsLeafPage()) {
    auto leaf_page = static_cast<LeafPage *>(page);
    auto left_leaf_page = static_cast<LeafPage *>(left_page);

    std::memmove(left_leaf_page->key_array_ + left_size, leaf_page->key_array_, sizeof(KeyType) * size);
    std::memmove(left_leaf_page->rid_array_ + left_size, leaf_page->rid_array_, sizeof(ValueType) * size);
    left_leaf_page->SetSize(left_size + size);
    left_leaf_page->SetNextPageId(leaf_page->GetNextPageId());
  }
  else {
    auto internal_page = static_cast<InternalPage *>(page);
    auto left_internal_page = static_cast<InternalPage *>(left_page);

    // pull down the separating key from parent, and set the corresponding value
    left_internal_page->SetKeyAt(left_size, parent_page->KeyAt(index));
    left_internal_page->SetValueAt(left_size, internal_page->ValueAt(0));

    std::memmove(left_internal_page->key_array_ + left_size + 1, internal_page->key_array_ + 1, sizeof(KeyType) * (size - 1));
    std::memmove(left_internal_page->page_id_array_ + left_size + 1, internal_page->page_id_array_, sizeof(page_id_t) * (size - 1));
    
    left_internal_page->SetSize(left_size + size);
  }
  // say parent has invalid,0,1,2,3,4 keys and 0,1,2,3,4,5 pointers, 
  // remove index=3; key = 2, value = 3
  // we need to move keys 3,4 to 2,3 and pointers 4,5 to 3,4
  std::memmove(parent_page->key_array_ + index, parent_page->key_array_ + index + 1, sizeof(KeyType) * (parent_size - index - 1));
  std::memmove(parent_page->page_id_array_ + index, parent_page->page_id_array_ + index + 1, sizeof(page_id_t) * (parent_size - index - 1));
  parent_page->SetSize(parent_size - 1);
}

INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::MergeWithRight(BPlusTreePage *page, BPlusTreePage *right_page, InternalPage *parent_page,
                                                                                  int index) -> void 
{
  int right_size = right_page->GetSize();
  int size = page->GetSize();
  int parent_size = parent_page->GetSize();
  // auto parent_internal = static_cast<InternalPage *>(parent_page);
  
  if (page->IsLeafPage()) {
    auto leaf_page = static_cast<LeafPage *>(page);
    auto right_leaf_page = static_cast<LeafPage *>(right_page);

    std::memmove(leaf_page->key_array_ + size, right_leaf_page->key_array_, sizeof(KeyType) * right_size);
    std::memmove(leaf_page->rid_array_ + size, right_leaf_page->rid_array_, sizeof(ValueType) * right_size);
    leaf_page->SetSize(right_size + size);
    leaf_page->SetNextPageId(right_leaf_page->GetNextPageId());
  }
  else {
    auto internal_page = static_cast<InternalPage *>(page);
    auto right_internal_page = static_cast<InternalPage *>(right_page);

    // pull down the separating key from parent, and set the corresponding value
    internal_page->SetKeyAt(size, parent_page->KeyAt(index + 1));
    internal_page->SetValueAt(size, right_internal_page->ValueAt(0));

    std::memmove(internal_page->key_array_ + size + 1, right_internal_page->key_array_ + 1, sizeof(KeyType) * (right_size - 1));
    std::memmove(internal_page->page_id_array_ + size + 1, right_internal_page->page_id_array_ + 1, sizeof(page_id_t) * (right_size - 1));

    internal_page->SetSize(size + right_size);
  }
  // here we are removing index + 1 (right_page), so dest param below is index + 2
  std::memmove(parent_page->key_array_ + index + 1, parent_page->key_array_ + index + 2, sizeof(KeyType) * (parent_size - index - 2));
  std::memmove(parent_page->page_id_array_ + index + 1, parent_page->page_id_array_ + index + 2, sizeof(page_id_t) * (parent_size - index - 2));
  parent_page->SetSize(parent_size - 1);
}


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

  auto head_guard = bpm_->WritePage(header_page_id_);
  ctx.root_page_id_ = head_guard.As<BPlusTreeHeaderPage>()->root_page_id_;

  if (ctx.root_page_id_ == INVALID_PAGE_ID) {return;}

  // 1) 乐观阶段：只要叶子“就地删除”后不下溢，且不需要改父分隔键，就直接完成
  // std::vector<int> idx_path; // child在parent中的index--给 borrow/merge的parent使用，这个也应该放在class中作为attribute
  ctx.read_set_.push_back(bpm_->ReadPage(ctx.root_page_id_));
  auto page = ctx.read_set_.back().As<BPlusTreePage>();
  
  // root是leaf，直接升级为write, 并且直接从leaf/root中删除此key
  if (page->IsLeafPage()) {
    ctx.read_set_.pop_back();
    ctx.write_set_.push_back(bpm_->WritePage(ctx.root_page_id_));
    auto leaf = ctx.write_set_.back().AsMut<LeafPage>();

    int n = leaf->GetSize();
    int pos = LeafIndexToInsert(leaf, key);
    if ((comparator_(leaf->KeyAt(pos), key) != 0)) {return;}  // pos最大等于 n - 1, 最小等于0, 这里的意思是如果没有该key，直接返回
    ShiftLeftByOne(leaf, pos);
    leaf->SetSize(n - 1);

    // root 被删空 → root = INVALID
    if(leaf->GetSize() == 0) {
      head_guard.AsMut<BPlusTreeHeaderPage>()->root_page_id_ = INVALID_PAGE_ID; 
    }
    return;
  }
  
  // 确认root并非leaf，可以先释放header_guard, 我们仍持有root的锁，所以安全
  const InternalPage* parent = nullptr;
  head_guard.Drop();
  while (!page->IsLeafPage()) {
    parent = static_cast<const InternalPage *>(page);
    int idx = FindKeyBiSearch(page, key);
    
    page_id_t page_id = parent->ValueAt(idx);

    ctx.read_set_.push_back(bpm_->ReadPage(page_id));
    page = ctx.read_set_.back().As<BPlusTreePage>();
    this->idx_path.push_back(idx);

    // 这里选择在while中写这个判断而不是在while block之后的原因是:
    // 我们可以在最后一个parent.read_set_.pop_back()释放父锁之前拿到子的写锁
    // 如果放在while之后再升级，就会在释放父锁之后才拿到子锁，中间有个时间窗口，别的thread可能会修改子节点
    if (page->IsLeafPage()) { 
      ctx.read_set_.pop_back();
      ctx.write_set_.push_back(bpm_->WritePage(page_id));

      // 我觉的这一行似乎没必要--之前read_set_.pop_back()调用了read_guard的析构函数，释放了read latch
      // 但是这个page对应的真正frame仍然在bpm中，并没有被evict掉，write_set_.push_back会重新获取这个page的guard,
      // 也就是走了checked_write_page中page_table_的lookup逻辑，发现frame已经在bpm中了，所以直接返回这个frame的write_guard
      // 所以这一行代码似乎没必要
      page = ctx.write_set_.back().As<BPlusTreePage>(); 
    }
    ctx.read_set_.pop_back();
  }

  // 如果key不存在，直接返回
  auto leafPage = ctx.write_set_.back().AsMut<LeafPage>();
  int pos = LeafIndexToInsert(leafPage, key);
  if (comparator_(leafPage->KeyAt(pos), key) != 0) {
    return;
  }

  int n = leafPage->GetSize();
  const bool will_underflow = n <= leafPage->GetMinSize();
  // const bool delete_is_first = (pos == 0); // if this is true, we gotta change the key in the parent_page
  // uncessery to check if delete_is_first, for example, if the parent is [10, 20, 30], 
  // child between 20 and 30 is [20, 22, 25],  if we delete 20 here, we dont necessarily need to 
  // change the parent key 20 to 22--the range still holds.

  // const bool has_parent = (path.size() > 1); //因为我们之前处理过root is leaf的情况，这里page应该是100% has parent

  // 获得被delete的key在parent中的位置
  // int idx = this->idx_path.back();

  // has parent百分百触发，因为root = leaf的情况已经讨论过了
  if (!will_underflow) {
    // 不会underflow，直接删除并返回
    std::memmove(leafPage->key_array_ + pos, leafPage->key_array_ + pos + 1, sizeof(KeyType) * (n - pos - 1));
    std::memmove(leafPage->rid_array_ + pos, leafPage->rid_array_ + pos + 1, sizeof(ValueType) * (n - pos - 1));
    leafPage->SetSize(n - 1);
    return;
  }
  ctx.write_set_.clear(); // seems pop_back() is fine here, but just to be safe
  this->idx_path.clear();

  // 然后是必须处理underflow的情况。不能采用从下至上的方式--因为会和上面的逻辑从上之下获取锁冲突，导致死锁
  // 所以需要restart probe获取写锁
  head_guard = bpm_->WritePage(header_page_id_);
  ctx.header_page_ = std::make_optional(std::move(head_guard));
  ctx.root_page_id_ = ctx.header_page_->As<BPlusTreeHeaderPage>()->root_page_id_;

  auto root_guard = bpm_->WritePage(ctx.root_page_id_);
  ctx.write_set_.push_back(std::move(root_guard));
  page = ctx.write_set_.back().AsMut<BPlusTreePage>();
  if (page->GetSize() > 2) {
    // 这里即使root删除了一个node还有至少一个node在
    // 这意味着后面我们不会修改root_page_id，所以可以释放header_page的写锁
    // 当然我们可以直接无脑释放header_page的写锁，但是后面需要多写一个关于root_page_id的判断
    // 这里偏好问题，我觉得无脑释放可能concurrency小高一点，区别不大
    ctx.header_page_ = std::nullopt;
  }

  while (!page->IsLeafPage()) {
    parent = static_cast<const InternalPage *>(page);
    int idx = FindKeyBiSearch(page, key);
    this->idx_path.push_back(idx);
    
    page_id_t page_id = parent->ValueAt(idx);

    ctx.write_set_.push_back(bpm_->WritePage(page_id));
    page = ctx.write_set_.back().AsMut<BPlusTreePage>();
    if (page->GetSize() > page->GetMinSize()) {
      // page wont underflow after deletion. we can release all header and parent locks
      ctx.header_page_ = std::nullopt;
      while (ctx.write_set_.size() > 1) {
        ctx.write_set_.pop_front();
      }
    }
  }

  leafPage = ctx.write_set_.back().AsMut<LeafPage>();
  int delete_pos = LeafIndexToInsert(leafPage, key);
  if (comparator_(leafPage->KeyAt(delete_pos), key) != 0) {
    return;  // key not found
  }

  n = leafPage->GetSize();
  std::memmove(leafPage->key_array_ + delete_pos, leafPage->key_array_ + delete_pos + 1, sizeof(KeyType) * (n - delete_pos - 1));
  std::memmove(leafPage->rid_array_ + delete_pos, leafPage->rid_array_ + delete_pos + 1, sizeof(ValueType) * (n - delete_pos - 1));
  leafPage->SetSize(n - 1);
  auto op_page = ctx.write_set_.back().AsMut<BPlusTreePage>();
  page_id_t current_page_id = INVALID_PAGE_ID;
  while (true) {
    if (ctx.write_set_.size() == 1) { // root_page
      auto root_page = ctx.write_set_.back().AsMut<BPlusTreePage>();
      if (root_page->IsLeafPage()) { 
        if (root_page->GetSize() == 0) {
          ctx.header_page_->AsMut<BPlusTreeHeaderPage>()->root_page_id_ = INVALID_PAGE_ID;
        }
        return;
      }
      // root is internal page, size <= 1 means root is invalid now
      if (root_page->GetSize() <= 1) {
        ctx.write_set_.pop_back();
        bpm_->DeletePage(ctx.root_page_id_);
        ctx.header_page_->AsMut<BPlusTreeHeaderPage>()->root_page_id_ = current_page_id;
      }
      return; 
    }
    // exit if no underflow
    if (op_page->GetSize() >= op_page->GetMinSize()) {
      return;
    }

    // reverse iterator to get parent
    auto it = ctx.write_set_.rbegin();
    auto parent_page = (++it)->AsMut<InternalPage>();
    int idx = this->idx_path.back();

    // try borrow from left sibling
    if (idx > 0) {
      WritePageGuard left_guard = bpm_->WritePage(parent_page->ValueAt(idx - 1));
      auto left_page = left_guard.AsMut<BPlusTreePage>();
      ctx.write_set_.emplace_back(std::move(left_guard)); // should call move constructor by default
      if (left_page->GetSize() > left_page->GetMinSize()) {
        // std::cout << left_page->GetSize() << " " << left_page->GetMinSize() << std::endl;
        if (op_page->IsLeafPage()) {
          // leafPage = ctx.write_set_.back().AsMut<LeafPage>();
          auto cur_page = static_cast<LeafPage *>(op_page);
          BorrowFromLeftLeafPage(cur_page, static_cast<LeafPage *>(left_page), parent_page, idx);
          // std::cout << this->DrawBPlusTree() << std::endl;
        }
        else {
          auto cur_page = static_cast<InternalPage *>(op_page);
          BorrowFromLeftInternalPage(cur_page, static_cast<InternalPage *>(left_page), parent_page, idx - 1);
        }
        return;
      }
      ctx.write_set_.pop_back(); // cannot borrow from left, remove left_guard
    }

    // idx != Size - 1, because we wouldn't not have right sibling if idx == Size - 1
    if (idx < parent_page->GetSize() - 1) {
      WritePageGuard right_guard = bpm_->WritePage(parent_page->ValueAt(idx + 1));
      auto right_page = right_guard.AsMut<BPlusTreePage>();
      ctx.write_set_.emplace_back(std::move(right_guard));
      if (right_page->GetSize() > right_page->GetMinSize()) {
        // can borrow from right sibling
        if (op_page->IsLeafPage()) {
          // leafPage = ctx.write_set_.back().AsMut<LeafPage>();
          auto cur_page = static_cast<LeafPage *>(op_page);
          BorrowFromRightLeafPage(cur_page, static_cast<LeafPage *>(right_page), parent_page, idx);
        }
        else {
          auto cur_page = static_cast<InternalPage *>(op_page);
          BorrowFromRightInternalPage(cur_page, static_cast<InternalPage *>(right_page), parent_page, idx);
        }
        return;
      }
      ctx.write_set_.pop_back(); // cannot borrow from right, remove right_guard
    }

    // cannot borrow from either side, try merge
    if (idx > 0) {
      WritePageGuard left_guard = bpm_->WritePage(parent_page->ValueAt(idx - 1));
      auto left_page = left_guard.AsMut<BPlusTreePage>();
      
      ctx.write_set_.emplace_back(std::move(left_guard));
      
      MergeWithLeft(op_page, left_page, parent_page, idx);
      current_page_id = ctx.write_set_.back().GetPageId(); // after merge, the op_page is removed, so current_page_id is left_page's id
      ctx.write_set_.pop_back(); // remove left_guard
      page_id_t merged_page_id = ctx.write_set_.back().GetPageId();
      bpm_->DeletePage(merged_page_id);
      // std::cout << this->DrawBPlusTree() << std::endl;  
      // ctx.write_set_.pop_back(); // go back to parent_page
    }
    else { // right merge
      WritePageGuard right_guard = bpm_->WritePage(parent_page->ValueAt(idx + 1));
      auto right_page = right_guard.AsMut<BPlusTreePage>(); 
      
      ctx.write_set_.emplace_back(std::move(right_guard));
      MergeWithRight(op_page, right_page, parent_page, idx);
      page_id_t merged_page_id = ctx.write_set_.back().GetPageId(); // after merge, the right_page is removed, mark it for deletion
      ctx.write_set_.pop_back(); // remove right_guard
      if (1) {
        auto test_page = ctx.write_set_.back().AsMut<BPlusTreePage>();
        if (test_page->GetSize() == 0) {
          // should not happen
        }
      }
     
      current_page_id = ctx.write_set_.back().GetPageId(); // after merge, the right_page is removed, so current_page_id is op_page's id
      bpm_->DeletePage(merged_page_id);
    }
    // current_page_id = ctx.write_set_.back().GetPageId();
    ctx.write_set_.pop_back(); // remove op_page
    this->idx_path.pop_back();
    op_page = ctx.write_set_.back().AsMut<BPlusTreePage>(); // parent_page?
  }

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
