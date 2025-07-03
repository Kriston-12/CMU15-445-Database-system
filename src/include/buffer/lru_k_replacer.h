//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// lru_k_replacer.h
//
// Identification: src/include/buffer/lru_k_replacer.h
//
// Copyright (c) 2015-2025, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#pragma once

#include <limits>
#include <list>
#include <mutex>  // NOLINT
#include <optional>
#include <unordered_map>
#include <vector>
#include <queue>
#include "common/config.h"
#include "common/macros.h"
#include <deque>
// #include <limits>
// #include <map>

namespace bustub {

enum class AccessType { Unknown = 0, Lookup, Scan, Index };

class LRUKNode {
  public:
  explicit LRUKNode(size_t k) : k_(k) {}

  void frameRecordAccess(size_t timestamp) {
    if (history.size() == k_) {
      history.pop_front(); // if we already have kth recent access, pop the front access
    }
    history.push_back(timestamp);
    isRemoved = false;
  }

  bool alreadyHasKAccess() const {return history.size() >= k_;}

  size_t getEarliestTime() {return history.front();}

  // size_t getKDis() const {
  //   if (alreadyHasKAccess()) {
  //     return history.front();
  //   }
  //   return std::numeric_limits<size_t>::max();
  // }

  void reset() {
    isEvictable = false;
    isRemoved = true;
    std::deque<size_t>().swap(history);
  }

  bool isEvictable{false};
  bool isRemoved{false};

  private:
  /** History of last seen K timestamps of this page. Least recent timestamp stored in front. */
  // Remove maybe_unused if you start using them. Feel free to change the member variables as you want.

  // std::list<size_t> history; // memory of std::list is not continuous, not cache friendly 
  std::deque<size_t> history;
  size_t k_;
  frame_id_t fid_;
  
};

/**
 * LRUKReplacer implements the LRU-k replacement policy.
 *
 * The LRU-k algorithm evicts a frame whose backward k-distance is maximum
 * of all frames. Backward k-distance is computed as the difference in time between
 * current timestamp and the timestamp of kth previous access.
 *
 * A frame with less than k historical references is given
 * +inf as its backward k-distance. When multiple frames have +inf backward k-distance,
 * classical LRU algorithm is used to choose victim.
 */
class LRUKReplacer {
 public:
  explicit LRUKReplacer(size_t num_frames, size_t k);
  

  DISALLOW_COPY_AND_MOVE(LRUKReplacer);

  /**
   * TODO(P1): Add implementation
   *
   * @brief Destroys the LRUReplacer.
   */
  ~LRUKReplacer() = default;

  auto Evict() -> std::optional<frame_id_t>;

  void RecordAccess(frame_id_t frame_id, AccessType access_type = AccessType::Unknown);

  void SetEvictable(frame_id_t frame_id, bool set_evictable);

  void Remove(frame_id_t frame_id);

  auto Size() -> size_t;

 private:
  // TODO(student): implement me! You can replace these member variables as you like.
  // Remove maybe_unused if you start using them.

  size_t current_timestamp_{0};
  size_t curr_size_{0};
  size_t replacer_size_;
  size_t k_;
  std::mutex latch_;

  std::unordered_map<frame_id_t, LRUKNode> node_store_;
  std::vector<LRUKNode> frames;
  
};

}  // namespace bustub
